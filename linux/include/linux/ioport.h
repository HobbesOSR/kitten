#ifndef __LINUX_IOPORT_H
#define __LINUX_IOPORT_H

#include <lwk/resource.h>

struct resource_list {
	struct resource_list *next;
	struct resource *res;
	struct pci_dev *dev;
};

#endif
