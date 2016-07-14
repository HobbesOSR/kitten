/*
 * XPMEM extensions for multiple domain support.
 *
 * xpmem_fwd.c: The XPMEM forwarding service
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 *
 */


#include <lwk/list.h>
#include <lwk/waitq.h>


#include <xpmem_private.h>

struct xpmem_fwd_state {
    /* Lock for fwd state */
    spinlock_t			   lock;
    
    /* Have we requested a domid */
    int				   domid_requested;
    volatile int                   domid_received;

    /* Our partition's domid */
    xpmem_domid_t                  domid;

    /* "Upstream" link to the nameserver */
    xpmem_link_t		   ns_link; 

    /* list of outstanding domid requests for this domain. Requests that cannot
     * be immediately serviced are put on this list
     */
    struct list_head		   domid_req_list;

    /* waitq for outstanding domid requests */
    waitq_t			   domid_waitq;
};


/* TODO: add field indicating whether we've actually sent a request or just
 * put this on a list 
 */
struct xpmem_domid_req_iter {
    xpmem_link_t     link;
    struct list_head node;
};


/* Ping all of the connections with a domid request, skipping id 'skip'. Return 0
 * if we sent at least one request
 */
static int
xpmem_request_domid(struct xpmem_partition_state * part,
                    xpmem_link_t                   request_link) 
{
    struct xpmem_fwd_state * state = part->fwd_state;
    xpmem_link_t             link  = 0;
    int                      ret   = -ENOENT;
    struct xpmem_cmd_ex      domid_cmd;
    unsigned long            flags;

    memset(&(domid_cmd), 0, sizeof(struct xpmem_cmd_ex));

    domid_cmd.type    = XPMEM_DOMID_REQUEST;
    domid_cmd.req_dom = -1;
    domid_cmd.src_dom = -1;
    domid_cmd.dst_dom = XPMEM_NS_DOMID;

    spin_lock_irqsave(&(state->lock), flags);
    {
	link = state->ns_link;
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    /* If we know how to get to the name server, send it there. Else, ping 
     * each of our connections
     */
    if (link > 0) {
	ret = xpmem_send_cmd_link(part, link, &domid_cmd);
    } else {
	for (link = 0; link < XPMEM_MAX_LINK; link++) {
	    /* Don't PING the local domain */
	    if (link == part->local_link) {
		continue;
	    }

	    /* Dont PING the requesting link */
	    if (link == request_link) {
		continue;
	    }

	    if (xpmem_send_cmd_link(part, link, &domid_cmd) == 0) {
		ret = 0;
	    }
	}
    }

    return ret;
}

static xpmem_domid_t
xpmem_wait_domid(struct xpmem_partition_state * part)
{
    struct xpmem_fwd_state * state = part->fwd_state;
    xpmem_domid_t            domid = 0;
    unsigned long            flags;

    wait_event_interruptible(state->domid_waitq,
	(state->domid_received == 1));
    mb();

    spin_lock_irqsave(&(state->lock), flags);
    {
	/* Return nonzero if we don't have a domid */
	domid = state->domid;
	state->domid_requested = 0;
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    return domid;
}

static void
xpmem_wakeup_domid(struct xpmem_partition_state * part)
{
    struct xpmem_fwd_state * state = part->fwd_state;

    state->domid_received = 1;
    mb();

    waitq_wakeup(&(state->domid_waitq));
}


/* Process an XPMEM_DOMID_REQUEST/RESPONSE/RELEASE command */
static int
xpmem_fwd_process_domid_cmd(struct xpmem_partition_state * part,
                            xpmem_link_t                   link,
                            struct xpmem_cmd_ex          * cmd)
{
    struct xpmem_fwd_state * state    = part->fwd_state;

    /* There's no reason not to reuse the input command struct for responses */
    struct xpmem_cmd_ex    * out_cmd  = cmd;
    xpmem_link_t	     out_link = link;
    int                      ret      = 0;
    unsigned long            flags;

    switch (cmd->type) {
	case XPMEM_DOMID_REQUEST: {
	    /* A domid is requested by someone downstream from us on link
	     * 'link'. Remember the request and forward it
	     */
	    struct xpmem_domid_req_iter * iter = NULL;

	    iter = kmem_alloc(sizeof(struct xpmem_domid_req_iter));
	    if (!iter) {
		return -ENOMEM;
	    }

	    iter->link = link;

	    spin_lock_irqsave(&(state->lock), flags);
	    {
		list_add_tail(&(iter->node), &(state->domid_req_list));
	    }
	    spin_unlock_irqrestore(&(state->lock), flags);

	    /* If we do not have a domid, we send two requests here: one for the
	     * requesting domain and one for us.
	     */

	    /* Request a domid for ourselves */
	    ret = xpmem_fwd_get_domid(part, link);
	    if (ret > 0) {
		/* Request a domid for the remote domain */
		ret = xpmem_request_domid(part, link);
	    }

	    /* No command to forward */
	    return ret;
	}

	case XPMEM_DOMID_RESPONSE: {
	    /* We've been allocated a domid.
	     *
	     * If our domain has no domid, take it for ourselves it.
	     * Otherwise, assign it to a link that has requested a domid from us
	     */
	    int forward = 1;
	    spin_lock_irqsave(&(state->lock), flags);
	    {
		if (state->domid == -1) {
		    /* Take the domid */
		    state->domid = cmd->domid_req.domid;

                    /* Remember that this link is to the name server */
		    state->ns_link = link;

		    /* Update the domid map to remember the ns link */
		    xpmem_add_domid_link(part, XPMEM_NS_DOMID, state->ns_link);

		    /* Update the domid map to remember our own domid */
		    xpmem_add_domid_link(part, state->domid, part->local_link);

		    /* Wake up the wait queue */
		    xpmem_wakeup_domid(part);

		    forward = 0;
		}
	    }
	    spin_unlock_irqrestore(&(state->lock), flags);

	    if (forward == 0) {
		/* We took the domid - no command to forward */
		return 0;
	    } else {
		struct xpmem_domid_req_iter * iter = NULL;

		spin_lock_irqsave(&(state->lock), flags);
		{
		    if (list_empty(&(state->domid_req_list))) {
			XPMEM_ERR("We currently do not support the buffering of XPMEM domids");
			ret = -1;
		    } else {
			iter = list_first_entry(&(state->domid_req_list),
				    struct xpmem_domid_req_iter,
				    node);
			list_del(&(iter->node));
		    }
		}
		spin_unlock_irqrestore(&(state->lock), flags);

		if (ret == 0) {
		    /* Forward the domid to this link */
		    out_link = iter->link;
		    kmem_free(iter);

		    /* Update the domid map to remember who has this */
		    xpmem_add_domid_link(part, cmd->domid_req.domid, out_link);
		}
	    }

	    break;
	}

	case XPMEM_DOMID_RELEASE:
	    /* Someone downstream is releasing their domid: simply forward to the
	     * namserver */
	    out_link = xpmem_get_domid_link(part, out_cmd->dst_dom);

	    if (out_link == 0) {
		XPMEM_ERR("Cannot find domid %lli in hashtable", out_cmd->dst_dom);
		return -EFAULT;
	    }

	    break;

	default: {
	    XPMEM_ERR("Unknown DOMID operation: %s", xpmem_cmd_to_string(cmd->type));
	    return -EINVAL;
	}
    }

    /* Send the response */
    ret = xpmem_send_cmd_link(part, out_link, out_cmd);
    if (ret != 0) {
	XPMEM_ERR("Cannot send command on link %d", out_link);
	return -EFAULT;
    }

    return ret;
}

static void
xpmem_set_failure(struct xpmem_cmd_ex * cmd)
{
    switch (cmd->type) {
	case XPMEM_MAKE:
	    cmd->make.segid = -1;
	    break;

	case XPMEM_REMOVE:
	    break;

	case XPMEM_GET:
	    cmd->get.apid = -1;
	    break;

	case XPMEM_RELEASE:
	    break;

	case XPMEM_ATTACH:
	    cmd->attach.num_pfns = 0;
	    break;

	case XPMEM_DETACH:
	    break;

	default:
	    break;
    }
}

static void
xpmem_set_complete(struct xpmem_cmd_ex * cmd)
{
    switch (cmd->type) {
	case XPMEM_MAKE:
	    cmd->type = XPMEM_MAKE_COMPLETE;
	    break;

	case XPMEM_REMOVE:
	    cmd->type = XPMEM_REMOVE_COMPLETE;
	    break;

	case XPMEM_GET:
	    cmd->type = XPMEM_GET_COMPLETE;
	    break;

	case XPMEM_RELEASE:
	    cmd->type = XPMEM_RELEASE_COMPLETE;
	    break;

	case XPMEM_ATTACH:
	    cmd->type = XPMEM_ATTACH_COMPLETE;
	    break;

	case XPMEM_DETACH:
	    cmd->type = XPMEM_DETACH_COMPLETE;
	    break;

	default:
	    break;
    }
}


/* Process a regular XPMEM command. If we get here we are connected to the name
 * server already and have a domid
 */
static int
xpmem_fwd_process_xpmem_cmd(struct xpmem_partition_state * part,
			   xpmem_link_t			   link,
			   struct xpmem_cmd_ex		 * cmd)
{
    /* There's no reason not to reuse the input command struct for responses */
    struct xpmem_cmd_ex * out_cmd  = cmd;
    xpmem_link_t          out_link = link;
    int                   ret      = 0;
    unsigned long         flags;

    spin_lock_irqsave(&(part->fwd_state->lock), flags);
    {
        if (part->fwd_state->domid == -1)
            ret = -ENOENT;
    }
    spin_unlock_irqrestore(&(part->fwd_state->lock), flags);

    if (ret != 0) {
        XPMEM_ERR("This domain has no XPMEM domid. Are you running the name server anywhere?");

        xpmem_set_failure(out_cmd);
        xpmem_set_complete(out_cmd);

        if (xpmem_send_cmd_link(part, out_link, out_cmd))
            XPMEM_ERR("Cannot send command on link %d", out_link);

        return ret;
    }

    switch (cmd->type) {
	case XPMEM_MAKE:
	case XPMEM_REMOVE:
	case XPMEM_GET:
	case XPMEM_RELEASE:
	case XPMEM_ATTACH:
	case XPMEM_DETACH:
	case XPMEM_MAKE_COMPLETE:
	case XPMEM_REMOVE_COMPLETE:
	case XPMEM_GET_COMPLETE:
	case XPMEM_RELEASE_COMPLETE:
	case XPMEM_ATTACH_COMPLETE:
	case XPMEM_DETACH_COMPLETE: {

	    out_link = xpmem_get_domid_link(part, out_cmd->dst_dom);

	    if (out_link == 0) {
		XPMEM_ERR("Cannot find domid %lli in hashtable", out_cmd->dst_dom);
		return -EINVAL;
	    }

	    break;
	}

	default: {
	    XPMEM_ERR("Unknown operation: %s", xpmem_cmd_to_string(cmd->type));
	    return -EINVAL;
	}
    }

    /* Write the response */
    if (xpmem_send_cmd_link(part, out_link, out_cmd)) {
	XPMEM_ERR("Cannot send command on link %d", out_link);
	return -EFAULT;
    }

    return 0;
}


static void
prepare_domids(struct xpmem_partition_state * part,
	       xpmem_link_t		      link,
	       struct xpmem_cmd_ex	    * cmd)
{
    struct xpmem_fwd_state * state   = part->fwd_state;

    if (part->local_link == link) {
	if (cmd->req_dom == 0) {
	    /* The request is being generated here: set the req domid */
	    cmd->req_dom = state->domid;
	}

	/* TODO: if the dst_dom is already set (i.e., we decide to push
	 * domid knowledge to user space) this will have to change
	 */

	cmd->src_dom = state->domid;
	cmd->dst_dom = XPMEM_NS_DOMID;
    }
}

int
xpmem_fwd_deliver_cmd(struct xpmem_partition_state * part,
		      xpmem_link_t		     link,
		      struct xpmem_cmd_ex	   * cmd)
{
    /* Prepare the domids for routing, if necessary */
    prepare_domids(part, link, cmd);

    switch (cmd->type) {
	case XPMEM_DOMID_REQUEST:
	case XPMEM_DOMID_RESPONSE:
	case XPMEM_DOMID_RELEASE:
	    return xpmem_fwd_process_domid_cmd(part, link, cmd);

	default:
	    return xpmem_fwd_process_xpmem_cmd(part, link, cmd);
    }
}


int
xpmem_fwd_init(struct xpmem_partition_state * part)
{
    struct xpmem_fwd_state * state = kmem_alloc(sizeof(struct xpmem_fwd_state));
    if (state == NULL) {
	return -ENOMEM;
    }

    spin_lock_init(&(state->lock));
    INIT_LIST_HEAD(&(state->domid_req_list));
    waitq_init(&(state->domid_waitq));

    state->ns_link         = -1;
    state->domid_requested = 0;
    state->domid_received  = 0;
    state->domid           = -1;

    /* Save state pointer */
    part->fwd_state = state;

    printk("XPMEM forwarding service initialized\n");

    return 0;
}

int
xpmem_fwd_deinit(struct xpmem_partition_state * part)
{
    struct xpmem_fwd_state * state   = part->fwd_state;
    xpmem_domid_t            domid   = -1;
    int                      release = 0;
    unsigned long            flags;

    spin_lock_irqsave(&(state->lock), flags);
    {
	if (state->domid > 0) {
	    release = 1;
	    domid   = state->domid;
	}

	/* Clear the domid */
	state->domid = -1;

	/* Wake up all wait queue waiters */
	xpmem_wakeup_domid(part);
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    /* Send a domid release, if we have one */
    if (release) {
	struct xpmem_cmd_ex dom_cmd;

	memset(&(dom_cmd), 0, sizeof(struct xpmem_cmd_ex));

	dom_cmd.type		= XPMEM_DOMID_RELEASE;
	dom_cmd.req_dom		= domid;
	dom_cmd.src_dom		= domid;
	dom_cmd.dst_dom		= XPMEM_NS_DOMID;
	dom_cmd.domid_req.domid = domid;

	if (xpmem_send_cmd_link(part, state->ns_link, &dom_cmd)) {
	    XPMEM_ERR("Cannot send DOMID_RELEASE on link %d", state->ns_link);
	}
    }

    /* Delete domid cmd list */
    spin_lock_irqsave(&(state->lock), flags);
    {
	struct xpmem_domid_req_iter * iter = NULL;
	struct xpmem_domid_req_iter * next = NULL;

	list_for_each_entry_safe(iter, next, &(state->domid_req_list), node) {
	    list_del(&(iter->node));
	    kmem_free(iter);
	}
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    kmem_free(state);
    part->fwd_state = NULL;

    printk("XPMEM forwarding service deinitialized\n");

    return 0;
}

xpmem_domid_t
xpmem_fwd_get_domid(struct xpmem_partition_state * part,
                    xpmem_link_t                   request_link)
{
    struct xpmem_fwd_state * state = part->fwd_state;
    xpmem_domid_t            domid = 0;
    unsigned long            flags;

    int request = 0;
    int wait    = 0;
    int ret     = 0;

    spin_lock_irqsave(&(state->lock), flags);
    {
	domid = state->domid;

	if (domid == -1) {
	    wait = 1;

	    if (state->domid_requested == 0) {
		request = 1;
		state->domid_received  = 0;
		state->domid_requested = 1;
	    }
	}
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    if (request)
	ret = xpmem_request_domid(part, request_link);

    if ((ret == 0) && wait)
	domid = xpmem_wait_domid(part);

    return domid;
}

int
xpmem_fwd_ensure_valid_domid(struct xpmem_partition_state * part)
{
    struct xpmem_fwd_state * state = part->fwd_state;
    xpmem_domid_t            domid = 0;
    unsigned long            flags;

    spin_lock_irqsave(&(state->lock), flags);
    {
	domid = state->domid;
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    return (domid > 0);
}
