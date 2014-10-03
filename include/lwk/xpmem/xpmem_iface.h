/*
 * XPMEM extensions for multiple domain support
 *
 * xpmem_iface.h: The interface to the XPMEM name and forwarding services
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 *
 */

#ifndef _XPMEM_IFACE_H
#define _XPMEM_IFACE_H


#ifdef __KERNEL__

typedef int64_t xpmem_domid_t;
typedef int64_t xpmem_link_t;

typedef enum {
    XPMEM_CONN_LOCAL,
    XPMEM_CONN_REMOTE,
} xpmem_connection_t;


struct xpmem_partition_state;
struct xpmem_cmd_ex;


struct xpmem_partition_state *
xpmem_get_partition(void);


xpmem_link_t
xpmem_add_connection(struct xpmem_partition_state * part,
                     xpmem_connection_t             conn,
                     int (*in_cmd_fn)(struct xpmem_cmd_ex * cmd, void * priv_data),
                     void                         * priv_data);

int
xpmem_remove_connection(struct xpmem_partition_state * part,
                        xpmem_link_t                   link);

int
xpmem_cmd_deliver(struct xpmem_partition_state * part,
                  xpmem_link_t                   link,
                  struct xpmem_cmd_ex          * cmd);

#endif

#endif /* _XPMEM_IFACE_H */
