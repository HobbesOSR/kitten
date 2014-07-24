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

#include <xpmem_partition.h>
#include <xpmem_hashtable.h>

struct xpmem_domain_state {
    int                            initialized;  /* domain state initialization */
    xpmem_link_t                   link;         /* XPMEM connection link */
    struct xpmem_partition_state * part;         /* pointer to XPMEM partition */

    int                            cmd_complete; /* command completion signal */
    waitq_t                        dom_waitq;    /* wait for results from fwd/name service */
    struct mutex                   mutex;        /* serialize access to fwd/name service */
    struct xpmem_cmd_ex          * cmd;          /* shared command struct */
};


struct xpmem_hashtable * attach_htable = NULL;

struct xpmem_remote_attach_struct {
    vaddr_t          vaddr;
    paddr_t          paddr;
    unsigned long    size;
    struct list_head node;
};


static u32 
domain_hash_fn(uintptr_t key)
{
    return hash_long(key, 32);
}

static int
domain_eq_fn(uintptr_t key1, 
             uintptr_t key2)            
{
    return (key1 == key2);
}



static int
xpmem_get_domain(struct xpmem_cmd_get_ex * get_ex)
{
    get_ex->apid = get_ex->segid;
    return 0;
}

static int
xpmem_release_domain(struct xpmem_cmd_release_ex * release_ex)
{
    return 0;
}

static int
xpmem_attach_domain(struct xpmem_cmd_attach_ex * attach_ex)
{
    xpmem_apid_t apid   = attach_ex->apid;
    off_t        offset = attach_ex->off;
    size_t       size   = attach_ex->size;

    if (apid <= 0) {
	return -1;
    }

    if (offset & (PAGE_SIZE -1)) {
	return -1;
    }

    if (size & (PAGE_SIZE -1)) {
	size += (PAGE_SIZE - (size & (PAGE_SIZE - 1)));
    }


    {
	xpmem_apid_t local_apid = 0;
	vaddr_t      seg_vaddr  = 0;
	u64          num_pfns   = 0;
	u64        * pfns       = NULL;

	/* Convert remote apid to local apid */
	local_apid = xpmem_get_local_apid(apid);
	if (local_apid <= 0) {
	    printk(KERN_ERR "XPMEM: no mapping for remote apid %lli\n", apid);
	}

	seg_vaddr = (vaddr_t)(local_apid + offset);
	/* TODO: this is a hack that has to be fixed */
	seg_vaddr -= 0x8000000000;

        num_pfns = size / PAGE_SIZE;

	pfns = kmem_alloc(sizeof(u64) * num_pfns);
	if (!pfns) {
	    printk(KERN_ERR "XPMEM: out of memory\n");
	    return -1;
	}

	{
	    uint64_t i = 0;

	    for (i = 0; i < num_pfns; i++) {
		paddr_t paddr = 0;

		if (aspace_virt_to_phys(current->aspace->id, seg_vaddr, &paddr)) {
		    printk(KERN_ERR "XPMEM: cannot get PFN for vaddr %p\n", (void *)seg_vaddr);
		    kmem_free(pfns);
		    return -1;
		}

		pfns[i] = (paddr >> PAGE_SHIFT);
	    }
	}

	/* Save in attachment struct */
	attach_ex->num_pfns = num_pfns;
	attach_ex->pfns     = pfns;
    }

    return 0;
}


static int
xpmem_detach_domain(struct xpmem_cmd_detach_ex * detach_ex)
{
    return 0;
}


unsigned long 
xpmem_map_pfn_range(u64 * pfns, 
                    u64 num_pfns)
{
    unsigned long size        = 0;
    vaddr_t       attach_addr = 0;

    size = num_pfns * PAGE_SIZE;

    /* Search for free aspace */
    if (aspace_find_hole(current->aspace->id, 0, size, PAGE_SIZE, &attach_addr)) {
	printk(KERN_ERR "XPMEM: aspace_find_hole failed\n");
	return -1;
    }

    /* Add region to aspace */
    if (aspace_add_region(current->aspace->id, attach_addr, size, VM_READ | VM_WRITE | VM_USER,
		PAGE_SIZE, "xpmem")) {
	printk(KERN_ERR "XPMEM: aspace_add_region failed\n");
	return -1;
    }

    /* Map PFNs into region */
    {
	u64 i = 0;

	for (i = 0; i < num_pfns; i++) {
	    vaddr_t addr =  attach_addr + (i * PAGE_SIZE);

	    if (aspace_map_pmem(current->aspace->id, (pfns[i] << PAGE_SHIFT), addr, PAGE_SIZE)) {
		printk(KERN_ERR "XPMEM: aspace_map_pmem failed\n");
		aspace_del_region(current->aspace->id, attach_addr, size);
		return -1;
	    }
	}
    }

    /* Add to htable */
    {
	struct xpmem_remote_attach_struct * remote = NULL;
	struct list_head                  * head   = NULL;

	if (!attach_htable) {
	    attach_htable = create_htable(0, domain_hash_fn, domain_eq_fn);
	    if (!attach_htable) {
		printk(KERN_ERR "XPMEM: cannot create attachment htable\n");
		aspace_del_region(current->aspace->id, attach_addr, size);
		return -1;
	    }
	}

	remote = kmem_alloc(sizeof(struct xpmem_remote_attach_struct));
	if (!remote) {
	    printk(KERN_ERR "XPMEM: out of memory\n");
	    aspace_del_region(current->aspace->id, attach_addr, size);
	    return -1;
	}

        /* Setup attach struct */
	remote->vaddr = attach_addr;
	remote->paddr = (pfns[0] << PAGE_SHIFT);
	remote->size  = size;


        head = (struct list_head *)htable_search(attach_htable, (uintptr_t)current->aspace->id);
	if (!head) {
	    head = kmem_alloc(sizeof(struct list_head));
	    if (!head) {
		printk(KERN_ERR "XPMEM: out of memory\n");
		aspace_del_region(current->aspace->id, attach_addr, size);
		return -1;
	    }

	    INIT_LIST_HEAD(head);
	    if (!htable_insert(attach_htable, (uintptr_t)current->aspace->id, (uintptr_t)head)) {
		printk(KERN_ERR "XPMEM: cannot add list head to htable\n");
		aspace_del_region(current->aspace->id, attach_addr, size);
		return -1;
	    }
	}

	list_add_tail(&(remote->node), head);
    }

    return attach_addr;
}


static void 
xpmem_detach_vaddr(struct xpmem_partition_state * part_state,
                   u64                            vaddr)
{
    struct list_head * head = NULL;

    if (!attach_htable) {
        return;
    }

    head = (struct list_head *)htable_search(attach_htable, (uintptr_t)current->aspace->id);
    if (!head) {
        printk(KERN_ERR "XPMEM_EXTENDED: LEAKING VIRTUAL ADDRESS SPACE\n");
    } else {
        struct xpmem_remote_attach_struct * remote = NULL, * next = NULL;
        list_for_each_entry_safe(remote, next, head, node) {
            if (remote->vaddr == vaddr) {
		aspace_del_region(current->aspace->id, remote->vaddr, remote->size);
                list_del(&(remote->node));
                kmem_free(remote);
                break;
            }
        }

        if (list_empty(head)) {
            htable_remove(attach_htable, (uintptr_t)current->aspace->id, 0);
            kmem_free(head);
        }
    }
}


/* Callback for command being issued by the XPMEM name/forwarding service */
static int
xpmem_cmd_fn(struct xpmem_cmd_ex * cmd,
             void                * priv_data)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)priv_data;
    int                         ret   = 0;

    if (!state->initialized) {
        return -1;
    }

    /* Process commands destined for this domain */
    switch (cmd->type) {
        case XPMEM_GET:

            ret = xpmem_get_domain(&(cmd->get));

            if (ret != 0) {
                cmd->get.apid = -1;
            }

            cmd->type    = XPMEM_GET_COMPLETE;
            cmd->dst_dom = XPMEM_NS_DOMID;

            xpmem_cmd_deliver(state->part, state->link, cmd);

            break;

        case XPMEM_RELEASE:
            ret = xpmem_release_domain(&(cmd->release));

            cmd->type    = XPMEM_RELEASE_COMPLETE;
            cmd->dst_dom = XPMEM_NS_DOMID;

            xpmem_cmd_deliver(state->part, state->link, cmd);

            break;

        case XPMEM_ATTACH:
            ret = xpmem_attach_domain(&(cmd->attach));

            if (ret != 0) {
                cmd->attach.pfns = NULL;
                cmd->attach.num_pfns = 0;
            }

            cmd->type    = XPMEM_ATTACH_COMPLETE;
            cmd->dst_dom = XPMEM_NS_DOMID;

            xpmem_cmd_deliver(state->part, state->link, cmd);

            break;

        case XPMEM_DETACH:
            ret = xpmem_detach_domain(&(cmd->detach));

            cmd->type    = XPMEM_DETACH_COMPLETE;
            cmd->dst_dom = XPMEM_NS_DOMID;

            xpmem_cmd_deliver(state->part, state->link, cmd);

            break;


            break;

        case XPMEM_MAKE_COMPLETE:
        case XPMEM_REMOVE_COMPLETE:
        case XPMEM_GET_COMPLETE:
        case XPMEM_RELEASE_COMPLETE:
        case XPMEM_ATTACH_COMPLETE: 
        case XPMEM_DETACH_COMPLETE: {
            state->cmd = kmem_alloc(sizeof(struct xpmem_cmd_ex));
            if (!state->cmd) {
                printk(KERN_ERR "XPMEM: out of memory\n");
                break;
            }

            *state->cmd = *cmd;
            
            if (cmd->type == XPMEM_ATTACH_COMPLETE) {
                state->cmd->attach.pfns = kmem_alloc(sizeof(u64) * cmd->attach.num_pfns);
                if (!state->cmd->attach.pfns) {
                    printk(KERN_ERR "XPMEM: out of memory\n");
		    kmem_free(state->cmd);
                    break;
                }

                memcpy(state->cmd->attach.pfns, cmd->attach.pfns, cmd->attach.num_pfns * sizeof(u64));
            }

            state->cmd_complete = 1;

            mb();
            waitq_wakeup(&(state->dom_waitq));

            break;
        }

        default:
            printk(KERN_ERR "XPMEM: domain given unknown XPMEM command %d\n", cmd->type);
            return -1;

    }

    return 0;
}


int
xpmem_domain_init(struct xpmem_partition_state * part)
{
    struct xpmem_domain_state * state = kmem_alloc(sizeof(struct xpmem_domain_state));
    if (!state) {
        return -1;
    }

    memset(state, 0, sizeof(struct xpmem_domain_state));
    
    mutex_init(&(state->mutex));
    waitq_init(&(state->dom_waitq));

    state->link = xpmem_add_connection(
            part,
            XPMEM_CONN_LOCAL,
            xpmem_cmd_fn,
            (void *)state);

    if (state->link <= 0) {
        printk(KERN_ERR "XPMEM: failed to register local domain with"
            " name/forwarding service\n");
        kmem_free(state);
        return -1;
    }

    state->cmd_complete = 0;
    state->initialized  = 1;
    state->part         = part;
    part->domain_priv   = state;

    printk("XPMEM local domain initialized\n");

    return 0;
}

int
xpmem_domain_deinit(struct xpmem_partition_state * part)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;

    if (!state) {
        return 0;
    }
    
    /* Remove domain connection */
    xpmem_remove_connection(state->part, state->link);

    kmem_free(state);
    part->domain_priv = NULL;

    printk("XPMEM local domain deinitialized\n");

    return 0;
}



/* Package XPMEM command into xpmem_cmd_ex structure and pass to forwarding/name
 * service layer. Wait for a response before proceeding
 */
int
xpmem_make_remote(struct xpmem_partition_state * part,
                  xpmem_segid_t                * segid)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)(struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type       = XPMEM_MAKE;
    cmd.src_dom    = state->part->domid;
    cmd.dst_dom    = XPMEM_NS_DOMID;

    cmd.make.segid = *segid;

    while (mutex_lock_interruptible(&(state->mutex)));

    {
        state->cmd_complete = 0;

        /* Deliver command */
        xpmem_cmd_deliver(state->part, state->link, &cmd);

        /* Wait for completion */
        mb();
        wait_event_interruptible(state->dom_waitq, state->cmd_complete == 1);
        
        /* Check command completion  */
        if (state->cmd_complete == 0) {
            return -1;
        }

        /* Grab allocated segid */
        *segid = state->cmd->make.segid;
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}

int
xpmem_remove_remote(struct xpmem_partition_state * part,
                    xpmem_segid_t                  segid)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type         = XPMEM_REMOVE;
    cmd.src_dom      = state->part->domid;
    cmd.dst_dom      = XPMEM_NS_DOMID;

    cmd.remove.segid = segid;

    while (mutex_lock_interruptible(&(state->mutex)));

    {
        state->cmd_complete = 0;

        /* Deliver command */
        xpmem_cmd_deliver(state->part, state->link, &cmd);

        /* Wait for completion */
        mb();
        wait_event_interruptible(state->dom_waitq, state->cmd_complete == 1);

        /* Check command completion  */
        if (state->cmd_complete == 0) {
            return -1;
        }
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}

int
xpmem_get_remote(struct xpmem_partition_state * part,
                 xpmem_segid_t                  segid,
                 int                            flags,
                 int                            permit_type,
                 u64                            permit_value,
                 xpmem_apid_t                 * apid)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type             = XPMEM_GET;
    cmd.src_dom          = state->part->domid;
    cmd.dst_dom          = XPMEM_NS_DOMID;

    cmd.get.segid        = segid;
    cmd.get.flags        = flags;
    cmd.get.permit_type  = permit_type;
    cmd.get.permit_value = permit_value;

    while (mutex_lock_interruptible(&(state->mutex)));

    {
        state->cmd_complete = 0;

        /* Deliver command */
        xpmem_cmd_deliver(state->part, state->link, &cmd);

        /* Wait for completion */
        mb();
        wait_event_interruptible(state->dom_waitq, state->cmd_complete == 1);

        /* Check command completion  */
        if (state->cmd_complete == 0) {
            return -1;
        }

        /* Grab allocated apid */
        *apid = state->cmd->get.apid;
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}

int
xpmem_release_remote(struct xpmem_partition_state * part,
                     xpmem_apid_t                   apid)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type         = XPMEM_RELEASE;
    cmd.src_dom      = state->part->domid;
    cmd.dst_dom      = XPMEM_NS_DOMID;

    cmd.release.apid = apid;

    while (mutex_lock_interruptible(&(state->mutex)));

    {
        state->cmd_complete = 0;

        /* Deliver command */
        xpmem_cmd_deliver(state->part, state->link, &cmd);

        /* Wait for completion */
        mb();
        wait_event_interruptible(state->dom_waitq, state->cmd_complete == 1);

        /* Check command completion  */
        if (state->cmd_complete == 0) {
            return -1;
        }
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}

int
xpmem_attach_remote(struct xpmem_partition_state * part,
                    xpmem_apid_t                   apid,
                    off_t                          offset,
                    size_t                         size,
                    u64                          * vaddr)
{
    struct xpmem_domain_state * state   = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type        = XPMEM_ATTACH;
    cmd.src_dom     = state->part->domid;
    cmd.dst_dom     = XPMEM_NS_DOMID;

    cmd.attach.apid = apid;
    cmd.attach.off  = offset;
    cmd.attach.size = size;

    while (mutex_lock_interruptible(&(state->mutex)));

    {
        state->cmd_complete = 0;

        /* Deliver command */
        xpmem_cmd_deliver(state->part, state->link, &cmd);

        /* Wait for completion */
        mb();
        wait_event_interruptible(state->dom_waitq, state->cmd_complete == 1);

        /* Check command completion  */
        if (state->cmd_complete == 0) {
            return -1;
        }

        /* Map pfn list */
        if (state->cmd->attach.num_pfns > 0) {
            *vaddr = (u64)xpmem_map_pfn_range(
                        state->cmd->attach.pfns,
                        state->cmd->attach.num_pfns
                    );
        } else {
            *vaddr = 0;
        }
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}



int
xpmem_detach_remote(struct xpmem_partition_state * part,
                    u64                            vaddr)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type         = XPMEM_DETACH;
    cmd.src_dom      = state->part->domid;
    cmd.dst_dom      = XPMEM_NS_DOMID;

    cmd.detach.vaddr = vaddr;

    while (mutex_lock_interruptible(&(state->mutex)));

    {
        state->cmd_complete = 0;

        /* Deliver command */
        xpmem_cmd_deliver(state->part, state->link, &cmd);

        /* Wait for completion */
        mb();
        wait_event_interruptible(state->dom_waitq, state->cmd_complete == 1);

        /* Check command completion  */
        if (state->cmd_complete == 0) {
            return -1;
        }
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    /* Free virtual address space */
    xpmem_detach_vaddr(part, vaddr);

    return 0;
}
