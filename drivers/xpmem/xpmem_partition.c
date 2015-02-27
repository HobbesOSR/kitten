/*
 * XPMEM extensions for multiple domain support
 *
 * xpmem_partition.c: Common functionality for the name and forwarding services
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 */

#include <lwk/interrupt.h>
#include <lwk/xpmem/xpmem_iface.h>

#include <xpmem_private.h>


/* The XPMEM link map is used to connect the forwarding service to all locally
 * attached domains in the system. The map has one entry for each VM and each
 * enclave connected to the domain, as well as one for local processes.
 *
 * Each domain in the system has a map of local connection links to a list of
 * XPMEM domids accessible "down the tree" via those links. The map also
 * includes the "upstream" link through which the name server may be reached.
 *
 * For example, consider the following topology:
 *
 *		 <XPMEM name server>
 *			 ^
 *			 |
 *			 |
 *			 v
 *		    <  Dom 2  >
 *		     ^	     ^
 *		     |	     |
 *		     |	     |
 *		     v	     v
 *		  <Dom 3>  <Dom 4>
 *		     ^
 *		     |
 *		     |
 *		     v 
 *		  <Dom 5>
 *
 * The maps look like:
 *
 * <Domain 1 (name server) map>
 *   [0: 1] (local processes are connected via link 1)
 *   [2: 2] (domid 2 is connected via link 2)
 *   [3: 2] (domid 3 is connected via link 2)
 *   [4: 2] (domid 4 is connected via link 2)
 *   [5: 2] (domid 5 is connected via link 2)
 *
 * <Domain 2 map>
 *   [0: 1] (local processes are connected via link 1)
 *   [1: 2] (domid 1 (name server) is connected via link 2)
 *   [3: 3] (domid 3 is connected via link 3)
 *   [4: 4] (domid 4 is connected via link 4)
 *   [5: 3] (domid 5 is connected via link 3)
 *
 * <Domain 3 map>
 *   [0: 1] (local processes are connected via link 1)
 *   [1: 2] (domid 1 (name server) is connected via link 2)
 *   [2: 2] (domid 2 is connected via link 2)
 *   [5: 3] (domid 5 is connected via link 3)
 *
 * <Domain 4 map>
 *   [0: 1] (local processes are connected via link 1)
 *   [1: 2] (domid 1 (name server) is connected via link 2)
 *   [2: 2] (domid 2 is connected via link 2)
 *
 * <Domain 5 map>
 *   [0: 1] (local processes are connected via link 1)
 *   [1: 2] (domid 1 (name server) is connected via link 2)
 *   [3: 2] (domid 3 is connected via link 2)
 *
 *
 *
 */


struct xpmem_link_connection {
    struct xpmem_partition_state * state;
    xpmem_link_t                   link;
    atomic_t                       refcnt;   
    void	                 * priv_data;
    int  (*in_cmd_fn)  (struct xpmem_cmd_ex *, void *);
    int  (*in_segid_fn)(xpmem_segid_t, xpmem_sigid_t, xpmem_domid_t, void *);
    void (*kill)       (void *);
};

static void
get_conn(struct xpmem_link_connection * conn)
{
    atomic_inc(&(conn->refcnt));
}

/* NOTE: the partition spinlock is already held */
static void
put_conn_last(struct xpmem_link_connection * conn)
{
    struct xpmem_partition_state * state = conn->state;
    xpmem_link_t                   idx   = 0;
    int i;

    /* Need to do four things:
     * (1) Set the conn to NULL in the conn map
     * (2) Remove all domains using this connection link in the map
     * (3) Invoke the private kill function for the conn
     * (4) Free the conn
     */

    idx = conn->link % XPMEM_MAX_LINK;
    state->conn_map[idx] = NULL;

    for (i = 0; i < XPMEM_MAX_DOMID; i++) {
	xpmem_link_t link = state->link_map[i];
	if (link == conn->link) {
	    if (state->is_nameserver) {
		/* Update name server map */
		spin_unlock(&(state->lock));
		xpmem_ns_kill_domain(state, i);
		spin_lock(&(state->lock));
	    }

	    state->link_map[i] = 0;
	}
    }

    if (conn->kill)
	conn->kill(conn->priv_data);

    kmem_free(conn);
}

static void 
put_conn(struct xpmem_link_connection * conn)
{
    struct xpmem_partition_state * state = conn->state;

    /* See lock reasoning below */
    spin_lock(&(state->lock));
    {
	if (atomic_dec_return(&(conn->refcnt)) == 0)
	    put_conn_last(conn);
    }
    spin_unlock(&(state->lock));
}

static struct xpmem_link_connection *
xpmem_get_link_conn(struct xpmem_partition_state * state,
		    xpmem_link_t                   link)
{
    struct xpmem_link_connection * conn = NULL;
    xpmem_link_t                   idx  = link % XPMEM_MAX_LINK;
    
    /* We need to synchronize with the last put of the conn, which otherwise could free
     * the conn in between us grabbing it from the conn_map and incrementing the refcnt */
    spin_lock(&(state->lock));
    {
	conn = state->conn_map[idx];

	if ((conn != NULL) &&
	    (conn->link == link))
	{
	    get_conn(conn);
	}
    }
    spin_unlock(&(state->lock));

    return conn;
}

char *
cmd_to_string(xpmem_op_t op)
{
    switch (op) {
	case XPMEM_MAKE:
	    return "XPMEM_MAKE";
	case XPMEM_REMOVE:
	    return "XPMEM_REMOVE";
	case XPMEM_GET:
	    return "XPMEM_GET";
	case XPMEM_RELEASE:
	    return "XPMEM_RELEASE";
	case XPMEM_ATTACH:
	    return "XPMEM_ATTACH";
	case XPMEM_DETACH:
	    return "XPMEM_DETACH";
	case XPMEM_MAKE_COMPLETE:
	    return "XPMEM_MAKE_COMPLETE";
	case XPMEM_REMOVE_COMPLETE:
	    return "XPMEM_REMOVE_COMPLETE";
	case XPMEM_GET_COMPLETE:
	    return "XPMEM_GET_COMPLETE";
	case XPMEM_RELEASE_COMPLETE:
	    return "XPMEM_RELEASE_COMPLETE";
	case XPMEM_ATTACH_COMPLETE:
	    return "XPMEM_ATTACH_COMPLETE";
	case XPMEM_DETACH_COMPLETE:
	    return "XPMEM_DETACH_COMPLETE";
	case XPMEM_PING_NS:
	    return "XPMEM_PING_NS";
	case XPMEM_PONG_NS:
	    return "XPMEM_PONG_NS";
	case XPMEM_DOMID_REQUEST:
	    return "XPMEM_DOMID_REQUEST";
	case XPMEM_DOMID_RESPONSE:
	    return "XPMEM_DOMID_RESPONSE";
	case XPMEM_DOMID_RELEASE:
	    return "XPMEM_DOMID_RELEASE";
	default:
	    return "UNKNOWN OPERATION";
    }
}

/* Send a command through a connection link */
int
xpmem_send_cmd_link(struct xpmem_partition_state * state,
		    xpmem_link_t		   link,
		    struct xpmem_cmd_ex		 * cmd)
{
    struct xpmem_link_connection * conn = NULL;
    int				   ret  = 0;

    conn = xpmem_get_link_conn(state, link);
    if (conn == NULL)
	return -1;

    ret = conn->in_cmd_fn(cmd, conn->priv_data);
    
    put_conn(conn);

    return ret;
}

void
xpmem_add_domid_link(struct xpmem_partition_state * state,
		     xpmem_domid_t		    domid,
		     xpmem_link_t		    link)
{
    spin_lock(&(state->lock));
    {
	state->link_map[domid] = link;
    }
    spin_unlock(&(state->lock));
}

xpmem_link_t
xpmem_get_domid_link(struct xpmem_partition_state * state,
		     xpmem_domid_t		    domid)
{
    xpmem_link_t link = 0;

    spin_lock(&(state->lock));
    {
	link = state->link_map[domid];
    }
    spin_unlock(&(state->lock));

    return link;
}

void
xpmem_remove_domid_link(struct xpmem_partition_state * state,
			xpmem_domid_t		       domid)
{
    spin_lock(&(state->lock));
    {
	state->link_map[domid] = 0;
    }
    spin_unlock(&(state->lock));
}

/* Link ids are always unique, but are hashed into a table with a simple modular 
 * function. Links may be skipped if their hash entry is already in use
 */
static xpmem_link_t
xpmem_get_free_link(struct xpmem_partition_state * state,
                    struct xpmem_link_connection * conn)
{
    xpmem_link_t idx  = 0;
    xpmem_link_t ret  = -1;
    xpmem_link_t link = 0;
    xpmem_link_t end  = 0;

    spin_lock(&(state->lock));
    {
	end = state->uniq_link;
	do {
	    link = ++(state->uniq_link);
	    idx  = link % XPMEM_MAX_LINK;
	    if (state->conn_map[idx] == NULL) {
		state->conn_map[idx] = conn;
		ret = link;
		break;
	    }
	} while (idx != end);
    }
    spin_unlock(&(state->lock));

    return ret;
}


static struct xpmem_partition_state *
xpmem_get_partition(void)
{
    extern struct xpmem_partition * xpmem_my_part;
    struct xpmem_partition_state  * part_state = &(xpmem_my_part->part_state);

    return part_state;
}

xpmem_link_t
xpmem_add_connection(void	      * priv_data,
		     int  (*in_cmd_fn)  (struct xpmem_cmd_ex *, void *),
		     int  (*in_segid_fn)(xpmem_segid_t, xpmem_sigid_t, xpmem_domid_t, void *),
		     void (*kill)       (void *))
{
    struct xpmem_partition_state * part = xpmem_get_partition();
    struct xpmem_link_connection * conn = NULL;

    /* Allocate new connection */
    conn = kmem_alloc(sizeof(struct xpmem_link_connection));
    if (conn == NULL) {
	return -ENOMEM;
    }

    /* Set conn fields */
    conn->state       = part;
    conn->in_cmd_fn   = in_cmd_fn;
    conn->in_segid_fn = in_segid_fn;
    conn->kill        = kill;
    conn->priv_data   = priv_data;
    atomic_set(&(conn->refcnt), 1);

    conn->link = xpmem_get_free_link(part, conn);
    if (conn->link < 0) {
	kmem_free(conn);
	return -EBUSY;
    }

    return conn->link;
}

xpmem_link_t
xpmem_add_local_connection(void		  * priv_data,
			   int  (*in_cmd_fn)  (struct xpmem_cmd_ex *, void *),
			   int  (*in_segid_fn)(xpmem_segid_t, xpmem_sigid_t, xpmem_domid_t, void *),
			   void (*kill)     (void *))
{
    struct xpmem_partition_state * part = xpmem_get_partition();
    xpmem_link_t link = xpmem_add_connection(priv_data, in_cmd_fn, in_segid_fn, kill);

    if (link >= 0) {
	/* Remember the local link */
	part->local_link = link;

	/* Associate the link as the NS link if we are the NS */
	if (part->is_nameserver)
	    xpmem_add_domid_link(part, XPMEM_NS_DOMID, link);
    }

    return link; 
}


void *
xpmem_get_link_data(xpmem_link_t link)
{
    struct xpmem_partition_state * part = xpmem_get_partition();
    struct xpmem_link_connection * conn = xpmem_get_link_conn(part, link);

    if (conn == NULL)
	return NULL;

    return conn->priv_data;
}

void
xpmem_put_link_data(xpmem_link_t link)
{
    xpmem_link_t                   idx  = link % XPMEM_MAX_LINK;
    struct xpmem_partition_state * part = xpmem_get_partition();
    struct xpmem_link_connection * conn = part->conn_map[idx];

    put_conn(conn);
}

void
xpmem_remove_connection(xpmem_link_t link)
{
    xpmem_put_link_data(link);
}


int
xpmem_cmd_deliver(xpmem_link_t		link,
		  struct xpmem_cmd_ex * cmd)
{
    struct xpmem_partition_state * part = xpmem_get_partition();
    int ret = 0;

    if (part->is_nameserver) {
	ret = xpmem_ns_deliver_cmd(part, link, cmd);
    } else {
	ret = xpmem_fwd_deliver_cmd(part, link, cmd);
    }

    return ret;
}


int
xpmem_irq_deliver(xpmem_segid_t segid,
                  xpmem_sigid_t sigid,
		  xpmem_domid_t domid)
{
    struct xpmem_partition_state * part = xpmem_get_partition();
    struct xpmem_link_connection * conn = NULL;
    struct xpmem_signal          * sig  = (struct xpmem_signal *)&sigid;
    xpmem_link_t                   link = xpmem_get_domid_link(part, domid);
    int                            ret  = 0;

    /* First, see if we have a link for the domid */
    if (link > 0) {
	/* We have a link --> invoke the irq callback directly */
	conn = xpmem_get_link_conn(part, link);
	if (conn == NULL)
	    return -1; 

	if (conn->in_segid_fn != NULL) {
	    ret = conn->in_segid_fn(segid, sigid, domid, conn->priv_data);
	    put_conn(conn);
	    return ret;
	}

	put_conn(conn);
    }   

    /* Send IPI */
    xpmem_send_ipi_to_apic(sig->apic_id, sig->vector);
    return 0;
}


int
xpmem_partition_init(int is_ns)
{
    struct xpmem_partition_state * part = xpmem_get_partition();
    int status = 0;

    memset(part, 0, sizeof(struct xpmem_partition_state));

    part->local_link	 = -1; 
    part->is_nameserver = is_ns;

    spin_lock_init(&(part->lock));

    /* Create ns/fwd state */
    if (is_ns) {
	status = xpmem_ns_init(part);
	if (status != 0) {
	    XPMEM_ERR("Could not initialize name service");
	    return status;
	}
    } else {
	status = xpmem_fwd_init(part);
	if (status != 0) {
	    XPMEM_ERR("Could not initialize forwarding service");
	    return status;
	}
    }

    return 0;
}


int
xpmem_partition_deinit(void)
{
    struct xpmem_partition_state * part = xpmem_get_partition();

    if (part->is_nameserver) {
	xpmem_ns_deinit(part);
    } else {
	xpmem_fwd_deinit(part);
    }

    return 0;
}

xpmem_domid_t
xpmem_get_domid(void)
{
    struct xpmem_partition_state * part = xpmem_get_partition();

    if (part->is_nameserver)
	return XPMEM_NS_DOMID;
    else
	return xpmem_fwd_get_domid(part, part->local_link);
}
