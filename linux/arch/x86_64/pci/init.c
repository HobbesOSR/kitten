#include <linux/pci.h>
#include <linux/init.h>
#include "pci.h"

/* arch_initcall has too random ordering, so call the initializers
   in the right sequence from here. */
static __init int pci_arch_init(void)
{

	pci_mmcfg_early_init();

#ifdef CONFIG_PCI_OLPC
	if (!pci_olpc_init())
		return 0;	/* skip additional checks if it's an XO */
#endif
#ifdef CONFIG_PCI_BIOS
	pci_pcbios_init();
#endif

	dmi_check_pciprobe();

	dmi_check_skip_isa_align();

	return 0;
}
arch_initcall(pci_arch_init);
