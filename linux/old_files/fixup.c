/*
 * Exceptions for specific devices. Usually work-arounds for fatal design flaws.
 */

#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/pci.h>
#include <linux/init.h>
#include "pci.h"




static void __devinit pci_fixup_piix4_acpi(struct pci_dev *d)
{
	/*
	 * PIIX4 ACPI device: hardwired IRQ9
	 */
	d->irq = 9;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_3, pci_fixup_piix4_acpi);


/*
 * For some reasons Intel decided that certain parts of their
 * 815, 845 and some other chipsets must look like PCI-to-PCI bridges
 * while they are obviously not. The 82801 family (AA, AB, BAM/CAM,
 * BA/CA/DB and E) PCI bridges are actually HUB-to-PCI ones, according
 * to Intel terminology. These devices do forward all addresses from
 * system to PCI bus no matter what are their window settings, so they are
 * "transparent" (or subtractive decoding) from programmers point of view.
 */
static void __devinit pci_fixup_transparent_bridge(struct pci_dev *dev)
{
	if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI &&
	    (dev->device & 0xff00) == 0x2400)
		dev->transparent = 1;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pci_fixup_transparent_bridge);


/* Max PCI Express root ports */
#define MAX_PCIEROOT	6
static int quirk_aspm_offset[MAX_PCIEROOT << 3];

#define GET_INDEX(a, b) ((((a) - PCI_DEVICE_ID_INTEL_MCH_PA) << 3) + ((b) & 7))

static int quirk_pcie_aspm_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *value)
{
	return raw_pci_read(pci_domain_nr(bus), bus->number,
						devfn, where, size, value);
}

/*
 * Replace the original pci bus ops for write with a new one that will filter
 * the request to insure ASPM cannot be enabled.
 */
static int quirk_pcie_aspm_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 value)
{
	u8 offset;

	offset = quirk_aspm_offset[GET_INDEX(bus->self->device, devfn)];

	if ((offset) && (where == offset))
		value = value & 0xfffffffc;

	return raw_pci_write(pci_domain_nr(bus), bus->number,
						devfn, where, size, value);
}

static struct pci_ops quirk_pcie_aspm_ops = {
	.read = quirk_pcie_aspm_read,
	.write = quirk_pcie_aspm_write,
};

/*
 * Prevents PCI Express ASPM (Active State Power Management) being enabled.
 *
 * Save the register offset, where the ASPM control bits are located,
 * for each PCI Express device that is in the device list of
 * the root port in an array for fast indexing. Replace the bus ops
 * with the modified one.
 */
static void pcie_rootport_aspm_quirk(struct pci_dev *pdev)
{
	int cap_base, i;
	struct pci_bus  *pbus;
	struct pci_dev *dev;

	if ((pbus = pdev->subordinate) == NULL)
		return;

	/*
	 * Check if the DID of pdev matches one of the six root ports. This
	 * check is needed in the case this function is called directly by the
	 * hot-plug driver.
	 */
	if ((pdev->device < PCI_DEVICE_ID_INTEL_MCH_PA) ||
	    (pdev->device > PCI_DEVICE_ID_INTEL_MCH_PC1))
		return;

	if (list_empty(&pbus->devices)) {
		/*
		 * If no device is attached to the root port at power-up or
		 * after hot-remove, the pbus->devices is empty and this code
		 * will set the offsets to zero and the bus ops to parent's bus
		 * ops, which is unmodified.
		 */
		for (i = GET_INDEX(pdev->device, 0); i <= GET_INDEX(pdev->device, 7); ++i)
			quirk_aspm_offset[i] = 0;

		pbus->ops = pbus->parent->ops;
	} else {
		/*
		 * If devices are attached to the root port at power-up or
		 * after hot-add, the code loops through the device list of
		 * each root port to save the register offsets and replace the
		 * bus ops.
		 */
		list_for_each_entry(dev, &pbus->devices, bus_list) {
			/* There are 0 to 8 devices attached to this bus */
			cap_base = pci_find_capability(dev, PCI_CAP_ID_EXP);
			quirk_aspm_offset[GET_INDEX(pdev->device, dev->devfn)] = cap_base + 0x10;
		}
		pbus->ops = &quirk_pcie_aspm_ops;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PA,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PA1,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PB,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PB1,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PC,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PC1,	pcie_rootport_aspm_quirk);



/*
 * Regular PCI devices have 256 bytes, but AMD Family 10h/11h CPUs have
 * 4096 bytes configuration space for each function of their processor
 * configuration space.
 */
static void amd_cpu_pci_cfg_space_size(struct pci_dev *dev)
{
	dev->cfg_size = pci_cfg_space_size_ext(dev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1200, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1201, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1202, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1203, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1204, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1300, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1301, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1302, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1303, amd_cpu_pci_cfg_space_size);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, 0x1304, amd_cpu_pci_cfg_space_size);
