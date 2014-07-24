/*
 * XPMEM extensions for multiple domain support
 *
 * Common functionality for name and forwarding partitions
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 */

#ifndef _XPMEM_PARTITION_H
#define _XPMEM_PARTITION_H

#include <lwk/spinlock_types.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_extended.h>


#define XPMEM_MAX_LINK_ID 16

/* The well-known name server's domid */
#define XPMEM_NS_DOMID    1


struct xpmem_partition_state {
    int           initialized;   /* partition initialization */
    spinlock_t    lock; 	 /* partition lock */
    xpmem_link_t  local_link;    /* link to our own domain */
    xpmem_domid_t domid;         /* domid for this partition */

    atomic_t      uniq_link;     /* unique link id generation */

    int           is_nameserver; /* are we running the nameserver? */

    /* map of XPMEM domids to local link ids */
    struct xpmem_hashtable * domid_map;

    /* map of link ids to connection structs */
    struct xpmem_hashtable * link_map;

    /* this partition's internal state */
    union {
	struct xpmem_ns_state  * ns_state;
	struct xpmem_fwd_state * fwd_state;
    };

    /* private data */
    void * domain_priv;
};

struct xpmem_partition {
    struct xpmem_partition_state part_state;
};


u32
xpmem_hash_fn(uintptr_t key);

int
xpmem_eq_fn(uintptr_t key1,
            uintptr_t key2);


char *
cmd_to_string(xpmem_op_t op);


xpmem_link_t
alloc_xpmem_link(struct xpmem_partition_state * state);


int
xpmem_add_domid(struct xpmem_partition_state * state,
                xpmem_domid_t                  domid,
		xpmem_link_t                   link);

xpmem_link_t
xpmem_search_domid(struct xpmem_partition_state * state,
                   xpmem_domid_t                  domid);

xpmem_link_t
xpmem_remove_domid(struct xpmem_partition_state * state,
                   xpmem_domid_t                  domid);


int xpmem_add_link(struct xpmem_partition_state * state,
                   xpmem_link_t                   link,
		   struct xpmem_link_connection * conn);

struct xpmem_link_connection *
xpmem_search_link(struct xpmem_partition_state * state,
                  xpmem_link_t                   link);

struct xpmem_link_connection *
xpmem_remove_link(struct xpmem_partition_state * state,
                  xpmem_link_t                   link);


int
xpmem_send_cmd_link(struct xpmem_partition_state * state,
                    xpmem_link_t                   link,
		    struct xpmem_cmd_ex          * cmd);


int
xpmem_ns_deliver_cmd(struct xpmem_partition_state * state,
                     xpmem_link_t                   link,
		     struct xpmem_cmd_ex          * cmd);

int
xpmem_fwd_deliver_cmd(struct xpmem_partition_state * state,
                      xpmem_link_t                   link,
		      struct xpmem_cmd_ex          * cmd);


int
xpmem_partition_init(struct xpmem_partition_state * state,
                     int                            is_nameserver);

int
xpmem_partition_deinit(struct xpmem_partition_state * state);


xpmem_apid_t xpmem_get_local_apid(xpmem_apid_t remote_apid);



#endif /* _XPMEM_PARTITION_H */
