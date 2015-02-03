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
typedef int16_t xpmem_link_t;
typedef int64_t xpmem_sigid_t;

typedef enum {
    XPMEM_CONN_NONE = 0,
    XPMEM_CONN_LOCAL,
    XPMEM_CONN_REMOTE,
} xpmem_connection_t;


struct xpmem_cmd_ex;

xpmem_link_t
xpmem_add_connection(xpmem_connection_t type,
		     void	      * priv_data,
		     int  (*in_cmd_fn)(struct xpmem_cmd_ex * cmd, void * priv_data),
		     int  (*in_irq_fn)(int		     irq, void * priv_data),
		     void (*kill)     (void * priv_data));

void
xpmem_remove_connection(xpmem_link_t link);

void *
xpmem_get_link_data(xpmem_link_t link);

void
xpmem_put_link_data(xpmem_link_t link);


int
xpmem_cmd_deliver(xpmem_link_t		link,
		  struct xpmem_cmd_ex * cmd);

int
xpmem_request_irq_link(xpmem_link_t link);

int
xpmem_release_irq_link(xpmem_link_t link,
		       int	    irq);

int
xpmem_irq_deliver(xpmem_link_t  link,
		  xpmem_domid_t	domid,
		  xpmem_sigid_t	sigid);

#endif /* __KERNEL__ */

#endif /* _XPMEM_IFACE_H */
