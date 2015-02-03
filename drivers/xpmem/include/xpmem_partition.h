/*
 * XPMEM extensions for multiple domain support
 *
 * Common functionality for name and forwarding partitions
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 */

#ifndef _XPMEM_PARTITION_H
#define _XPMEM_PARTITION_H

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_iface.h>
#include <lwk/xpmem/xpmem_extended.h>

#include <lwk/spinlock_types.h>

#define XPMEM_MAX_LINK	128
#define XPMEM_MIN_DOMID 32
#define XPMEM_MAX_DOMID 128

/* The well-known name server's domid */
#define XPMEM_NS_DOMID	  1

struct xpmem_partition_state {
    /* spinlock for state */
    spinlock_t	  lock;

    /* link to our own domain */
    xpmem_link_t  local_link;

    /* domid for this partition */
    xpmem_domid_t domid;

    /* table mapping link ids to connection structs */
    struct xpmem_link_connection * conn_map[XPMEM_MAX_LINK];

    /* table mapping domids to link ids */
    xpmem_link_t		   link_map[XPMEM_MAX_DOMID];

    xpmem_link_t  uniq_link;

    /* are we running the nameserver */
    int is_nameserver;

    /* this partition's internal state */
    union {
	struct xpmem_ns_state  * ns_state;
	struct xpmem_fwd_state * fwd_state;
    };
};


int
xpmem_partition_init(struct xpmem_partition_state * state,
		     int			    is_nameserver);

int
xpmem_partition_deinit(struct xpmem_partition_state * state);

/* Functions used internally by fwd/ns */
char *
cmd_to_string(xpmem_op_t op);

int
xpmem_send_cmd_link(struct xpmem_partition_state * state,
		    xpmem_link_t		   link,
		    struct xpmem_cmd_ex		 * cmd);

void
xpmem_add_domid_link(struct xpmem_partition_state * state,
		     xpmem_domid_t		    domid,
		     xpmem_link_t		    link);

xpmem_link_t
xpmem_get_domid_link(struct xpmem_partition_state * state,
		     xpmem_domid_t		    domid);

void
xpmem_remove_domid_link(struct xpmem_partition_state * state,
			xpmem_domid_t		       domid);


#endif /* _XPMEM_PARTITION_H */
