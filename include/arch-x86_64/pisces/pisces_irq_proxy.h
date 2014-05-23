#ifndef _PISCES_IRQ_PROXY_H
#define _PISCES_IRQ_PROXY_H

#include <lwk/pci/pci.h>

int 
pisces_setup_irq_proxy(pci_dev_t     * dev, 
		       irq_handler_t   handler,
		       int             vector,
		       void          * priv);

#endif
