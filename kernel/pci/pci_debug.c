/* Copyright (c) 2013, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/pci/pcicfg.h>


/** Prints a pci_bar_t structure to the console. */
void
pcicfg_bar_print(pci_bar_t *bar)
{
	if (bar->mem == PCIM_BAR_MEM_SPACE) {
		if (bar->type == 0) {
			printk("BAR[%d]: MEM addr=0x%08x (32-bit, %s)\n",
			       bar->index, (uint32_t)bar->address,
			       (bar->prefetch) ? "prefetchable" : "non-prefetchable");
		} else if (bar->type == 1) {
			printk("BAR[%d]: MEM addr=0x%08x (< 1MB, %s)\n",
			       bar->index, (uint32_t)bar->address,
			       (bar->prefetch) ? "prefetchable" : "non-prefetchable");
		} else if (bar->type == 2) {
			printk("BAR[%d]: MEM addr=0x%016lx (64-bit, %s)\n",
			       bar->index, (unsigned long)bar->address,
			       (bar->prefetch) ? "prefetchable" : "non-prefetchable");
		}
	} else {
		printk("BAR[%d]: IO  addr=0x%04x\n",
		       bar->index, (uint16_t)bar->address);
	}
}


/** Prints a PCI config header to the console */
void
pcicfg_hdr_print(pcicfg_hdr_t *hdr)
{
	printk("======================================================================\n");

	printk("Device %d:%d.%d\n", hdr->bus, hdr->slot, hdr->func);
	printk("--------------------------------------\n");

	/* PCI Device Independent Region */
	printk("  Header Type:         0x%x\n", hdr->hdr_type);
	printk("    Is Multi-Func Dev: %d\n",   hdr->is_multi_func);
	printk("  Vendor ID:           0x%x\n", hdr->vendor_id);
	printk("  Device ID:           0x%x\n", hdr->device_id);
	printk("  Command:             0x%x\n", hdr->command_reg);
	printk("  Status:              0x%x\n", hdr->status_reg);
	printk("  Revision ID:         0x%x\n", hdr->rev_id);
	printk("  Class Code:\n");
	printk("    Programming Iface: 0x%x\n", hdr->prog_iface);
	printk("    Sub-Class Code:    0x%x\n", hdr->sub_class);
	printk("    Base-Class Code:   0x%x\n", hdr->base_class);
	printk("  Cache Line Size:     %d bytes\n", hdr->cache_line_sz * 4);
	printk("  Latency Timer:       0x%x\n", hdr->latency_timer);

	printk("\n");

	/* PCI Device Header Type Region */
	printk("  Subsys Vendor Id:    0x%x\n", hdr->sub_vendor);
	printk("  Subsys Device Id:    0x%x\n", hdr->sub_device);
	printk("  Interrupt Line:      0x%x (set by sw)\n", hdr->interrupt_line);
	printk("  Interrupt Pin:       0x%x\n", hdr->interrupt_pin);
	printk("  Min Grant:           0x%x\n", hdr->min_grant);
	printk("  Max Latency:         0x%x\n", hdr->max_latency);
	printk("  Num Maps:            0x%x\n", hdr->num_maps);
	printk("    BAR[0]:            0x%08x (%s)\n", hdr->bar[0], (hdr->bar[0] & 0x1) ? "IO" : "MEM");
	printk("    BAR[1]:            0x%08x (%s)\n", hdr->bar[1], (hdr->bar[1] & 0x1) ? "IO" : "MEM");
	printk("    BAR[2]:            0x%08x (%s)\n", hdr->bar[2], (hdr->bar[2] & 0x1) ? "IO" : "MEM");
	printk("    BAR[3]:            0x%08x (%s)\n", hdr->bar[3], (hdr->bar[3] & 0x1) ? "IO" : "MEM");
	printk("    BAR[4]:            0x%08x (%s)\n", hdr->bar[4], (hdr->bar[4] & 0x1) ? "IO" : "MEM");
	printk("    BAR[5]:            0x%08x (%s)\n", hdr->bar[5], (hdr->bar[5] & 0x1) ? "IO" : "MEM");

	if (hdr->pp.valid) {
		printk("\n");
		printk("  EXTCAP PCI Power Management:\n");
		printk("    Capabilities:      0x%04x\n", hdr->pp.pmc);
		printk("    Control/Status:    0x%04x\n", hdr->pp.pmcsr);
	}

	if (hdr->pcix.valid) {
		printk("\n");
		printk("  EXTCAP PCIX:\n");
		printk("    Command:           0x%04x\n", hdr->pcix.command);
		printk("    Status:            0x%08x\n", hdr->pcix.status);
	}

	printk("======================================================================\n");
}
