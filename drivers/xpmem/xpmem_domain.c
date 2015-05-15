/*
 * XPMEM extensions for multiple domain support.
 *
 * This file implements XPMEM commands from remote processes 
 * destined for this domain, as well as wrappers for sending commands to remote
 * domains
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 *
 */


#include <lwk/aspace.h>
#include <lwk/waitq.h>
#include <lwk/list.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_extended.h>
#include <lwk/xpmem/xpmem_iface.h>

#include <xpmem_private.h>


#define MAX_UNIQ_REQ 64


struct xpmem_request_struct {
    /* Has it been allocated */
    int			  allocated;

    /* Has command been serviced? */
    int			  serviced;

    /* Completed command struct */
    struct xpmem_cmd_ex   cmd;

    /* Waitq for process */
    waitq_t		  waitq;
};

struct xpmem_domain_state {
    /* Lock for domain state */
    spinlock_t			   lock;

    /* Array of request structs indexed by reqid */
    struct xpmem_request_struct    requests[MAX_UNIQ_REQ];

    /* XPMEM connection link */
    xpmem_link_t		   link;

    /* Guarantee uniqueness of remote segid/apid pairs */
    atomic_t                       uniq_apid;
};


static int32_t
alloc_request_id(struct xpmem_domain_state * state)
{
    int32_t	  id	= -1;
    unsigned long flags = 0;

    spin_lock_irqsave(&(state->lock), flags);
    {
	int i = 0;
	for (i = 0; i < MAX_UNIQ_REQ; i++) {
	    if (state->requests[i].allocated == 0) {
		struct xpmem_request_struct * req = &(state->requests[i]);

		req->allocated = 1;
		req->serviced  = 0;

		id = i;
		break;
	    }
	}
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    return id;
}

static void
free_request_id(struct xpmem_domain_state * state,
		uint32_t		    reqid)
{
    unsigned long flags = 0;

    spin_lock_irqsave(&(state->lock), flags);
    {
	state->requests[reqid].allocated = 0;
    }
    spin_unlock_irqrestore(&(state->lock), flags);
}

static void
init_request_map(struct xpmem_domain_state * state)
{
    int i = 0;

    for (i = 0; i < MAX_UNIQ_REQ; i++) {
	struct xpmem_request_struct * req= &(state->requests[i]);

	req->allocated = 0;
	waitq_init(&(req->waitq));
    }
}

static int 
xpmem_get_domain(struct xpmem_cmd_get_ex * get_ex)
{
    xpmem_apid_t apid;
    struct xpmem_segment *seg;
    struct xpmem_thread_group *ap_tg, *seg_tg;
    int status;

    xpmem_segid_t segid = get_ex->segid;
    int flags = get_ex->flags;
    int permit_type = get_ex->permit_type;
    void * permit_value = (void *)get_ex->permit_value;

    if (segid <= 0)
        return -EINVAL;

    if ((flags & ~(XPMEM_RDONLY | XPMEM_RDWR)) ||
        (flags & (XPMEM_RDONLY | XPMEM_RDWR)) ==
        (XPMEM_RDONLY | XPMEM_RDWR))
        return -EINVAL;

    switch (permit_type) {
	case XPMEM_PERMIT_MODE:
	case XPMEM_GLOBAL_MODE:
	    if (permit_value != NULL)
		return -EINVAL;

	    break;
	default:
	    return -EINVAL;
    }

    seg_tg = xpmem_tg_ref_by_segid(segid);
    if (IS_ERR(seg_tg))
        return PTR_ERR(seg_tg);

    seg = xpmem_seg_ref_by_segid(seg_tg, segid);
    if (IS_ERR(seg)) {
        xpmem_tg_deref(seg_tg);
        return PTR_ERR(seg);
    }

    /* find accessor's thread group structure by using the seg group */
    ap_tg = seg_tg;
    if (IS_ERR(ap_tg)) {
        xpmem_seg_deref(seg);
        xpmem_tg_deref(seg_tg);
        return -XPMEM_ERRNO_NOPROC;
    }
    xpmem_tg_ref(ap_tg);

    apid = xpmem_make_apid(ap_tg);
    if (apid < 0) {
        xpmem_tg_deref(ap_tg);
        xpmem_seg_deref(seg);
        xpmem_tg_deref(seg_tg);
        return apid;
    }

    status = xpmem_get_segment(flags, permit_type, permit_value, apid, seg, seg_tg, ap_tg);

    if (status == 0) { 
        get_ex->apid  = apid;
        get_ex->size  = seg->size;
        get_ex->domid = seg->domid;

        if (seg->flags & XPMEM_FLAG_SIGNALLABLE)
            get_ex->sigid = *((xpmem_sigid_t *)&seg->sig);
        else
            get_ex->sigid = 0;
    } else {
        xpmem_seg_deref(seg);
        xpmem_tg_deref(seg_tg);
    }

    xpmem_tg_deref(ap_tg);

    /*
     * The following two derefs
     *
     *      xpmem_seg_deref(seg);
     *      xpmem_tg_deref(seg_tg);
     *
     * aren't being done at this time in order to prevent the seg
     * and seg_tg structures from being prematurely kmem_free'd as long as the
     * potential for them to be referenced via this ap structure exists.
     *
     * These two derefs will be done by xpmem_release_ap() at the time
     * this ap structure is destroyed.
     */

    return status;
}

static int 
xpmem_release_domain(struct xpmem_cmd_release_ex * release_ex)
{
    struct xpmem_thread_group *ap_tg, *seg_tg;
    struct xpmem_access_permit *ap;
    struct xpmem_segment *seg;

    xpmem_apid_t apid = release_ex->apid;

    if (apid <= 0)
        return -EINVAL;

    ap_tg = xpmem_tg_ref_by_apid(apid);
    if (IS_ERR(ap_tg))
        return PTR_ERR(ap_tg);

    ap = xpmem_ap_ref_by_apid(ap_tg, apid);
    if (IS_ERR(ap)) {
        xpmem_tg_deref(ap_tg);
        return PTR_ERR(ap);
    }   
    BUG_ON(ap->tg != ap_tg);

    seg = ap->seg;
    xpmem_seg_ref(seg);
    seg_tg = seg->tg;
    xpmem_tg_ref(seg_tg);

    xpmem_release_ap(ap_tg, ap);

    xpmem_ap_deref(ap);
    xpmem_tg_deref(ap_tg);
    xpmem_seg_deref(seg);
    xpmem_tg_deref(seg_tg);

    return 0;
}


static int
xpmem_get_pages(struct xpmem_attachment * att,
                u64                       num_pfns,
                u64                       pfn_pa)
{
    struct xpmem_segment       * seg    = NULL;
    struct xpmem_access_permit * ap     = NULL;
    struct xpmem_thread_group  * seg_tg = NULL;
    struct xpmem_thread_group  * ap_tg  = NULL;

    u64   seg_vaddr = 0;
    u64   i         = 0;
    u32   pfn       = 0;
    u32 * pfns      = 0;

    int ret = 0;

    xpmem_att_ref(att);
    ap = att->ap;
    xpmem_ap_ref(ap);
    ap_tg = ap->tg;
    xpmem_tg_ref(ap_tg);

    if ((ap->flags    & XPMEM_FLAG_DESTROYING) ||
        (ap_tg->flags & XPMEM_FLAG_DESTROYING))
    {
        xpmem_att_deref(att);
        xpmem_ap_deref(ap);
        xpmem_tg_deref(ap_tg);
        return -1;
    }

    seg = ap->seg;
    xpmem_seg_ref(seg);
    seg_tg = seg->tg;
    xpmem_tg_ref(seg_tg);

    /* Grab the segment vaddr */
    seg_vaddr = ((u64)att->vaddr & PAGE_MASK);

    /* The list is preallocated by the remote domain, the address of which is given
     * by 'pfn_pa'. We assume all memory is already mapped by Linux, so a simple __va
     * gives us the kernel mapping
     */
    pfns = (u32 *)__va(pfn_pa);

    for (i = 0; i < num_pfns; i++) {
	vaddr_t vaddr = 0;
	paddr_t paddr = 0;

	vaddr = seg_vaddr + (i * PAGE_SIZE);

	ret = aspace_virt_to_phys(seg_tg->aspace->id, vaddr, &paddr);
	if (ret != 0) {
	    XPMEM_ERR("aspace_virt_to_phys() failed (%d)", ret);
	    goto out;
	}

	pfn     = paddr >> PAGE_SHIFT;
        pfns[i] = pfn;
    }

out:
    xpmem_att_deref(att);
    xpmem_ap_deref(ap);
    xpmem_tg_deref(ap_tg);
    xpmem_seg_deref(seg);
    xpmem_tg_deref(seg_tg);

    return ret;
}

static int 
xpmem_attach_domain(struct xpmem_cmd_attach_ex * attach_ex)
{
    int ret;
    vaddr_t seg_vaddr;
    struct xpmem_thread_group *ap_tg, *seg_tg;
    struct xpmem_access_permit *ap;
    struct xpmem_segment *seg;
    struct xpmem_attachment *att;

    xpmem_apid_t apid = attach_ex->apid;
    off_t offset = attach_ex->off;
    size_t size = attach_ex->size;

    if (apid <= 0)
        return -EINVAL;

    /* The offset of the attachment must be page aligned */
    if (offset_in_page(offset) != 0)
        return -EINVAL;

    /* If the size is not page aligned, fix it */
    if (offset_in_page(size) != 0)  
        size += PAGE_SIZE - offset_in_page(size);

    ap_tg = xpmem_tg_ref_by_apid(apid);
    if (IS_ERR(ap_tg))
        return PTR_ERR(ap_tg);

    ap = xpmem_ap_ref_by_apid(ap_tg, apid);
    if (IS_ERR(ap)) {
        xpmem_tg_deref(ap_tg);
        return PTR_ERR(ap);
    }   

    seg = ap->seg;
    xpmem_seg_ref(seg);
    seg_tg = seg->tg;
    xpmem_tg_ref(seg_tg);

    xpmem_seg_down(seg);

    ret = xpmem_validate_access(ap_tg, ap, offset, size, XPMEM_RDWR, &seg_vaddr);
    if (ret != 0)
        goto out_1;

    /* size needs to reflect page offset to start of segment */
    size += offset_in_page(seg_vaddr);

    /* create new attach structure */
    att = kmem_alloc(sizeof(struct xpmem_attachment));
    if (att == NULL) {
        ret = -ENOMEM;
        goto out_1;
    }

    mutex_init(&att->mutex);
    att->vaddr    = seg_vaddr;
    att->at_vaddr = seg_vaddr;
    att->at_size  = size;
    att->ap       = ap;
    att->flags    = XPMEM_FLAG_REMOTE;
    INIT_LIST_HEAD(&att->att_node);

    xpmem_att_not_destroyable(att);
    xpmem_att_ref(att);

    /* get pages from the seg, copy to remote domain list */
    ret = xpmem_get_pages(att, attach_ex->num_pfns, attach_ex->pfn_pa);
    if (ret != 0)
        goto out_2;

    /* link attach structure to its access permit's remote att list */
    spin_lock(&ap->lock);
    if (ap->flags & XPMEM_FLAG_DESTROYING) {
        spin_unlock(&ap->lock);
        ret = -ENOENT;
        goto out_2;
    }
    list_add_tail(&att->att_node, &ap->att_list);
    spin_unlock(&ap->lock);

    /* NOTE: We don't add remote attachments to the tgap hash list, as there is no actual
     * teardown that requires knowledge of the virtual address on remote teardown
     */

    ret = 0;
out_2:
    if (ret != 0) {
        att->flags |= XPMEM_FLAG_DESTROYING;
        xpmem_att_destroyable(att);
    }
    xpmem_att_deref(att);
out_1:
    xpmem_seg_up(seg);
    xpmem_ap_deref(ap);
    xpmem_tg_deref(ap_tg);
    xpmem_seg_deref(seg);
    xpmem_tg_deref(seg_tg);
    return ret;
}

static int 
xpmem_detach_domain(struct xpmem_cmd_detach_ex * detach_ex)
{
    return 0;
}


static unsigned long 
xpmem_map_pfn_range(u64   at_vaddr,
		    u64   num_pfns,
		    u32 * pfns) 
{
    int i = 0;

    for (i = 0; i < num_pfns; i++) {
	vaddr_t addr  = at_vaddr + (i * PAGE_SIZE);
	paddr_t paddr = (addr_t)pfns[i] << PAGE_SHIFT;

	//printk("map pmem from va %p to pa %p\n", (void *)addr, (void *)paddr);

	if (aspace_map_pmem(current->aspace->id, paddr, addr, PAGE_SIZE)) {
	    XPMEM_ERR("aspace_map_pmem() failed");

	    while (--i >= 0) {
		addr = at_vaddr + (i * PAGE_SIZE);
		aspace_unmap_pmem(current->aspace->id, addr, PAGE_SIZE);
	    }

	    return -1;
	}
    }

    return 0;
}



static struct xpmem_cmd_ex *
xpmem_cmd_wait(struct xpmem_domain_state  * state,
	       uint32_t			    reqid)
{
    struct xpmem_request_struct * req = &(state->requests[reqid]);

    wait_event_interruptible(req->waitq, req->serviced > 0);
    mb();

    if (req->serviced == 0) {
	return NULL;
    }

    return &(req->cmd);
}

static void
xpmem_cmd_wakeup(struct xpmem_domain_state * state,
		 struct xpmem_cmd_ex	   * cmd)
{
    struct xpmem_request_struct * req = &(state->requests[cmd->reqid]);

    memcpy(&(req->cmd), cmd, sizeof(struct xpmem_cmd_ex));

    req->serviced = 1;
    mb();

    waitq_wakeup(&(req->waitq));
}


/* Callback for command being issued by the XPMEM name/forwarding service */
static int
xpmem_cmd_fn(struct xpmem_cmd_ex * cmd,
	     void		 * priv_data)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)priv_data;
    int				ret   = 0;

    /* Process commands destined for this domain */
    switch (cmd->type) {
	case XPMEM_GET:
	    ret = xpmem_get_domain(&(cmd->get));

	    if (ret != 0) {
		cmd->get.apid = -1;
	    }

	    cmd->type	 = XPMEM_GET_COMPLETE;
	    cmd->dst_dom = XPMEM_NS_DOMID;

	    xpmem_cmd_deliver(state->link, cmd);

	    break;

	case XPMEM_RELEASE:
	    ret = xpmem_release_domain(&(cmd->release));

	    cmd->type	 = XPMEM_RELEASE_COMPLETE;
	    cmd->dst_dom = XPMEM_NS_DOMID;

	    xpmem_cmd_deliver(state->link, cmd);

	    break;

	case XPMEM_ATTACH:
	    ret = xpmem_attach_domain(&(cmd->attach));

	    if (ret != 0) {
		cmd->attach.num_pfns = 0;
	    }

	    cmd->type	 = XPMEM_ATTACH_COMPLETE;
	    cmd->dst_dom = XPMEM_NS_DOMID;

	    xpmem_cmd_deliver(state->link, cmd);

	    break;

	case XPMEM_DETACH:
	    ret = xpmem_detach_domain(&(cmd->detach));

	    cmd->type	 = XPMEM_DETACH_COMPLETE;
	    cmd->dst_dom = XPMEM_NS_DOMID;

	    xpmem_cmd_deliver(state->link, cmd);

	    break;

	case XPMEM_MAKE_COMPLETE:
	case XPMEM_REMOVE_COMPLETE:
	case XPMEM_GET_COMPLETE:
	case XPMEM_RELEASE_COMPLETE:
	case XPMEM_ATTACH_COMPLETE: 
	case XPMEM_DETACH_COMPLETE:
	    xpmem_cmd_wakeup(state, cmd);

	    break;

	default:
	    XPMEM_ERR("Domain given unknown XPMEM command %d", cmd->type);
	    return -1;

    }

    return 0;
}

static void
xpmem_kill_fn(void * priv)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)priv;

    kmem_free(state);

    printk("XPMEM local domain deinitialized\n");
}


/* Package XPMEM command into xpmem_cmd_ex structure and pass to forwarding/name
 * service layer. Wait for a response before proceeding
 */
int
xpmem_make_remote(xpmem_link_t	  link,
		  xpmem_segid_t   request,
		  xpmem_segid_t	* segid)
{
    struct xpmem_domain_state * state  = NULL;
    struct xpmem_cmd_ex       * resp   = NULL;
    struct xpmem_cmd_ex		cmd;
    uint32_t			reqid  = 0;
    int				status = 0;

    /* Take a reference */
    state = xpmem_get_link_data(link);
    if (state == NULL) {
	return -1;
    }

    /* Allocate a request id */
    reqid = alloc_request_id(state);
    if (reqid < 0) {
	xpmem_put_link_data(link);
	return -EBUSY;
    }

    /* Setup command */
    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));
    cmd.type	     = XPMEM_MAKE;
    cmd.reqid	     = reqid;

    cmd.make.request = request;
    cmd.make.segid   = *segid;

    /* Deliver command */
    status = xpmem_cmd_deliver(state->link, &cmd);

    if (status != 0) {
	goto out;
    }

    /* Wait for completion */
    resp = xpmem_cmd_wait(state, reqid);

    /* Check command completion  */
    if (resp == NULL) {
	goto out;
    }

    /* Grab allocated segid */
    *segid = resp->make.segid;

out:
    free_request_id(state, reqid);
    xpmem_put_link_data(link);

    return status;
}


int
xpmem_remove_remote(xpmem_link_t  link,
		    xpmem_segid_t segid)
{
    struct xpmem_domain_state * state  = NULL;
    struct xpmem_cmd_ex       * resp   = NULL;
    struct xpmem_cmd_ex		cmd;
    uint32_t			reqid  = 0;
    int				status = 0;

    /* Take a reference */
    state = xpmem_get_link_data(link);
    if (state == NULL) {
	return -1;
    }
    
    /* Allocate a request id */
    reqid = alloc_request_id(state);
    if (reqid < 0) {
	xpmem_put_link_data(link);
	return -EBUSY;
    }

    /* Setup command */
    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));
    cmd.type	     = XPMEM_REMOVE;
    cmd.reqid	     = reqid;
    cmd.remove.segid = segid;

    /* Deliver command */
    status = xpmem_cmd_deliver(state->link, &cmd);

    if (status != 0) {
	goto out;
    }

    /* Wait for completion */
    resp = xpmem_cmd_wait(state, reqid);

    /* Check command completion  */
    if (resp == NULL) {
	goto out;
    }

out:
    free_request_id(state, reqid);
    xpmem_put_link_data(link);

    return status;
}

int
xpmem_get_remote(xpmem_link_t	 link,
		 xpmem_segid_t	 segid,
		 int		 flags,
		 int		 permit_type,
		 u64 	 	 permit_value,
		 xpmem_apid_t  * apid,
		 u64	       * size,
		 xpmem_domid_t * domid,
		 xpmem_sigid_t * sigid)
{
    struct xpmem_domain_state * state  = NULL;
    struct xpmem_cmd_ex       * resp   = NULL;
    struct xpmem_cmd_ex		cmd;
    uint32_t			reqid = 0;
    int				status = 0;

    /* Take a reference */
    state = xpmem_get_link_data(link);
    if (state == NULL) {
	return -1;
    }
    
    /* Allocate a request id */
    reqid = alloc_request_id(state);
    if (reqid < 0) {
	xpmem_put_link_data(link);
	return -EBUSY;
    }

    /* Setup command */
    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));
    cmd.type		 = XPMEM_GET;
    cmd.reqid		 = reqid;
    cmd.get.segid	 = segid;
    cmd.get.flags	 = flags;
    cmd.get.permit_type  = permit_type;
    cmd.get.permit_value = permit_value;

    /* Deliver command */
    status = xpmem_cmd_deliver(state->link, &cmd);

    if (status != 0) {
	goto out;
    }

    /* Wait for completion */
    resp = xpmem_cmd_wait(state, reqid);

    /* Check command completion  */
    if (resp == NULL) {
	goto out;
    }

    /* Grab segment parameters */
    *apid  = resp->get.apid;
    *size  = resp->get.size;
    *domid = resp->get.domid;
    *sigid = resp->get.sigid;

out:
    free_request_id(state, reqid);
    xpmem_put_link_data(link);

    return status;
}

int
xpmem_release_remote(xpmem_link_t  link,
		     xpmem_segid_t segid,
		     xpmem_apid_t  apid)
{
    struct xpmem_domain_state * state  = NULL;
    struct xpmem_cmd_ex       * resp   = NULL;
    struct xpmem_cmd_ex		cmd;
    uint32_t			reqid = 0;
    int				status = 0;

    /* Take a reference */
    state = xpmem_get_link_data(link);
    if (state == NULL) {
	return -1;
    }
    
    /* Allocate a request id */
    reqid = alloc_request_id(state);
    if (reqid < 0) {
	xpmem_put_link_data(link);
	return -EBUSY;
    }

    /* Setup command */
    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));
    cmd.type	      = XPMEM_RELEASE;
    cmd.reqid	      = reqid;
    cmd.release.segid = segid;
    cmd.release.apid  = apid;

    /* Deliver command */
    status = xpmem_cmd_deliver(state->link, &cmd);

    if (status != 0) {
	goto out;
    }

    /* Wait for completion */
    resp = xpmem_cmd_wait(state, reqid);

    /* Check command completion  */
    if (resp == NULL) {
	goto out;
    }

out:
    free_request_id(state, reqid);
    xpmem_put_link_data(link);

    return status;

}

int
xpmem_attach_remote(xpmem_link_t  link,
		    xpmem_segid_t segid,
		    xpmem_apid_t  apid,
		    off_t	  offset,
		    size_t	  size,
		    u64		  at_vaddr)
{
    struct xpmem_domain_state * state  = NULL;
    struct xpmem_cmd_ex       * resp   = NULL;
    struct xpmem_cmd_ex		cmd;
    uint32_t			reqid  = 0;
    int				status = 0;
    void                      * pfns   = NULL;

    /* Take a reference */
    state = xpmem_get_link_data(link);
    if (state == NULL) {
	return -1;
    }
    
    /* Allocate a request id */
    reqid = alloc_request_id(state);
    if (reqid < 0) {
	xpmem_put_link_data(link);
	return -EBUSY;
    }

    /* Setup command */
    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));
    cmd.type	        = XPMEM_ATTACH;
    cmd.reqid	        = reqid;
    cmd.attach.segid    = segid;
    cmd.attach.apid     = apid;
    cmd.attach.off      = offset;
    cmd.attach.size     = size;
    cmd.attach.num_pfns = size / PAGE_SIZE;

    /* Allocate buffer for pfn list */
    pfns = kmem_alloc(cmd.attach.num_pfns * sizeof(u32));
    if (pfns == NULL) {
	free_request_id(state, reqid);
	xpmem_put_link_data(link);
	return -ENOMEM;
    }

    /* Save paddr of pfn list */
    cmd.attach.pfn_pa = (u64)__pa(pfns);

    /* Deliver command */
    status = xpmem_cmd_deliver(state->link, &cmd);

    if (status != 0) {
	goto out;
    }

    /* Wait for completion */
    resp = xpmem_cmd_wait(state, reqid);

    /* Check command completion  */
    if (resp == NULL) {
	goto out;
    }

    /* Map pfn list */
    if (resp->attach.num_pfns > 0) {
	status = xpmem_map_pfn_range(
	    at_vaddr,
	    resp->attach.num_pfns,
	    pfns);
    } else {
	status = -1;
    }

    /* Free pfn list */
    kmem_free(pfns);

out:
    free_request_id(state, reqid);
    xpmem_put_link_data(link);

    return status;

}



int
xpmem_detach_remote(xpmem_link_t  link,
		    xpmem_segid_t segid,
		    xpmem_apid_t  apid,
		    u64		  vaddr)
{
    struct xpmem_domain_state * state  = NULL;
    struct xpmem_cmd_ex       * resp   = NULL;
    struct xpmem_cmd_ex		cmd;
    uint32_t			reqid = 0;
    int				status = 0;

    /* Take a reference */
    state = xpmem_get_link_data(link);
    if (state == NULL) {
	return -1;
    }
    
    /* Allocate a request id */
    reqid = alloc_request_id(state);
    if (reqid < 0) {
	xpmem_put_link_data(link);
	return -EBUSY;
    }

    /* Setup command */
    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));
    cmd.type	     = XPMEM_DETACH;
    cmd.reqid	     = reqid;
    cmd.detach.segid = segid;
    cmd.detach.apid  = apid;
    cmd.detach.vaddr = vaddr;

    /* Deliver command */
    status = xpmem_cmd_deliver(state->link, &cmd);

    if (status != 0) {
	goto out;
    }

    /* Wait for completion */
    resp = xpmem_cmd_wait(state, reqid);

    /* Check command completion  */
    if (resp == NULL) {
	goto out;
    }

out:
    free_request_id(state, reqid);
    xpmem_put_link_data(link);

    return status;

}

xpmem_link_t
xpmem_domain_init(void)
{
    struct xpmem_domain_state * state = kmem_alloc(sizeof(struct xpmem_domain_state));
    if (!state) {
	return -1;
    }

    /* Initialize stuff */
    spin_lock_init(&(state->lock));
    init_request_map(state);
    atomic_set(&(state->uniq_apid), 0);

    state->link = xpmem_add_local_connection(
	    (void *)state,
	    xpmem_cmd_fn,
	    NULL,
	    xpmem_kill_fn);

    if (state->link <= 0) {
	XPMEM_ERR("Failed to register local domain with name/forwarding service");
	kmem_free(state);
	return -1;
    }

    printk("XPMEM local domain initialized\n");

    return state->link;
}

int
xpmem_domain_deinit(xpmem_link_t link)
{
    /* Remove domain connection */
    xpmem_remove_connection(link);

    return 0;
}
