#include <linux/pci.h>
#include <linux/init.h>
#include "pci.h"

/* arch_initcall has too random ordering, so call the initializers
   in the right sequence from here. */
static __init int pci_arch_init(void)
{

	pci_mmcfg_early_init();

	return 0;
}
arch_initcall(pci_arch_init);
