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


static int
xpmem_get_domain(struct xpmem_cmd_get_ex * get_ex)
{
    get_ex->apid = get_ex->segid;
    get_ex->size = (1LL << SMARTMAP_SHIFT);
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
 
    return do_xpmem_attach_domain(apid, offset, size, &(attach_ex->pfns), &(attach_ex->num_pfns));
}


static int
xpmem_detach_domain(struct xpmem_cmd_detach_ex * detach_ex)
{
    return 0;
}


unsigned long 
xpmem_map_pfn_range(u64   at_vaddr,
                    u64 * pfns, 
                    u64   num_pfns)
{
    u64     i    = 0;
    vaddr_t addr = 0;

    for (i = 0; i < num_pfns; i++) {
	addr = at_vaddr + (i * PAGE_SIZE);

	if (aspace_map_pmem(current->aspace->id, (pfns[i] << PAGE_SHIFT), addr, PAGE_SIZE)) {
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

        case XPMEM_MAKE_COMPLETE:
        case XPMEM_REMOVE_COMPLETE:
        case XPMEM_GET_COMPLETE:
        case XPMEM_RELEASE_COMPLETE:
        case XPMEM_ATTACH_COMPLETE: 
        case XPMEM_DETACH_COMPLETE: {
            state->cmd = kmem_alloc(sizeof(struct xpmem_cmd_ex));
            if (!state->cmd) {
                break;
            }

            *state->cmd = *cmd;
            
            if (cmd->type == XPMEM_ATTACH_COMPLETE) {
                state->cmd->attach.pfns = kmem_alloc(sizeof(u64) * cmd->attach.num_pfns);
                if (!state->cmd->attach.pfns) {
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
	    XPMEM_ERR("Domain given unknown XPMEM command %d", cmd->type);
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
	XPMEM_ERR("Failed to register local domain with name/forwarding service");
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
	    mutex_unlock(&(state->mutex));
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
	    mutex_unlock(&(state->mutex));
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
                 xpmem_apid_t                 * apid,
		 u64                          * size)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type             = XPMEM_GET;
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
	    mutex_unlock(&(state->mutex));
            return -1;
        }

        /* Grab allocated apid */
        *apid = state->cmd->get.apid;
	*size = state->cmd->get.size;
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}

int
xpmem_release_remote(struct xpmem_partition_state * part,
                     xpmem_segid_t                  segid,
                     xpmem_apid_t                   apid)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type          = XPMEM_RELEASE;
    cmd.release.segid = segid;
    cmd.release.apid  = apid;

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
	    mutex_unlock(&(state->mutex));
            return -1;
        }
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}

int
xpmem_attach_remote(struct xpmem_partition_state * part,
                    xpmem_segid_t                  segid,
                    xpmem_apid_t                   apid,
                    off_t                          offset,
                    size_t                         size,
                    u64                            at_vaddr)
{
    struct xpmem_domain_state * state   = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;
    int                         ret     = 0;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type         = XPMEM_ATTACH;
    cmd.attach.segid = segid;
    cmd.attach.apid  = apid;
    cmd.attach.off   = offset;
    cmd.attach.size  = size;

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
	    mutex_unlock(&(state->mutex));
            return -1;
        }

        /* Map pfn list */
        if (state->cmd->attach.num_pfns > 0) {
            ret = (u64)xpmem_map_pfn_range(
			at_vaddr,
                        state->cmd->attach.pfns,
                        state->cmd->attach.num_pfns
                    );
        } else {
	    ret = -1;
        }
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return ret;
}



int
xpmem_detach_remote(struct xpmem_partition_state * part,
                    xpmem_segid_t                  segid,
                    xpmem_apid_t                   apid,
                    u64                            vaddr)
{
    struct xpmem_domain_state * state = (struct xpmem_domain_state *)part->domain_priv;
    struct xpmem_cmd_ex         cmd;

    if (!state->initialized) {
        return -1;
    }

    memset(&cmd, 0, sizeof(struct xpmem_cmd_ex));

    cmd.type         = XPMEM_DETACH;
    cmd.detach.segid = segid;
    cmd.detach.apid  = apid;
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
	    mutex_unlock(&(state->mutex));
            return -1;
        }
    }

    mutex_unlock(&(state->mutex));

    kmem_free(state->cmd);

    return 0;
}
