#include <lwk/driver.h>
#include <lwk/pci/pci.h>


// This gets called to initialize a gemini driver context.
// It is called once for each Gemini device discovered.
int gemini_probe(pci_dev_t *pci_dev, const pci_dev_id_t *id)
{
	printk("Hello from gemini_probe(), PCI header dump:\n");
	pcicfg_hdr_print(&pci_dev->cfg);
	return 0;
}


// Table of PCI device IDs supported by this driver.
// Currently only one rev of the Gemini is supported.
// The list must be NULL terminated.
static const pci_dev_id_t gemini_id_table[] = {
	{ 0x17db, 0x201, 0, 0, 0, 0 },
	{ 0, }
};


// PCI driver structure representing us.
// This gets registered with the LWK PCI subsystem.
pci_driver_t gemini_driver = {
	.name     = "gemini",
	.id_table = gemini_id_table,
	.probe    = gemini_probe,
};


// This gets called once at boot to register the Gemini driver with the LWK.
// It does not actually initialize the Gemini (that is done by gemini_probe()).
int gemini_init(void)
{
	if (pci_register_driver(&gemini_driver) != 0) {
		printk(KERN_WARNING "Failed to initialize Cray Gemini driver.\n");
		return -1;
	}

	return 0;
}


DRIVER_INIT("net", gemini_init);
