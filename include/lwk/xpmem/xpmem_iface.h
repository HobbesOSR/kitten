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

#include <lwk/interrupt.h>
#include <lwk/xpmem/xpmem.h>

typedef int16_t xpmem_link_t;
typedef int64_t xpmem_sigid_t;

struct xpmem_cmd_ex;


/*
 * xpmem_sigid_t is of type __s64 and designed to be opaque to the user. It consists of 
 * the following underlying fields
 *
 * 'apic_id' is the local apic id where the destination process is running
 * 'vector' is the ipi vector allocated by the destination process
 * 'irq' is the irq allocated by the destination process, which is only used if the
 * destination process is running in a local domain
 */
struct xpmem_signal {
    uint16_t vector;
    uint16_t apic_id;
    int32_t irq;
};



xpmem_link_t
xpmem_add_connection(void	    * priv_data,
		     int  (*in_cmd_fn)(struct xpmem_cmd_ex *, void *),
		     int  (*in_segid_fn)(xpmem_segid_t, xpmem_sigid_t, xpmem_domid_t, void *),
		     void (*kill)     (void *));

void
xpmem_remove_connection(xpmem_link_t link);

void *
xpmem_get_link_data(xpmem_link_t link);

void
xpmem_put_link_data(xpmem_link_t link);


int
xpmem_cmd_deliver(xpmem_link_t		link,
		  struct xpmem_cmd_ex * cmd);

/* IRQ support */
int
xpmem_request_irq(irqreturn_t (*cb)(int, void *),
                  void      * priv_data);

void
xpmem_release_irq(int    irq,
                  void * priv_data);

int
xpmem_irq_to_vector(int irq);

int
xpmem_irq_deliver(xpmem_segid_t segid,
                  xpmem_sigid_t sigid,
		  xpmem_domid_t domid);

#endif /* __KERNEL__ */

#endif /* _XPMEM_IFACE_H */
