#ifndef _PISCES_PCI_H_
#define _PISCES_PCI_H_

#include<lwk/pci/pci.h>

int pisces_pci_msi_enable(pci_dev_t *dev, u8 vector);
int pisces_pci_msi_disable(pci_dev_t *dev);
int pisces_pci_msix_enable(pci_dev_t *dev, struct msix_entry * entries, u32 num_entries);
int pisces_pci_msix_disable(pci_dev_t *dev);

#endif
