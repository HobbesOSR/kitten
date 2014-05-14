/* Copyright (c) 2013, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/pci/pcicfg.h>


/** Prints a pci_bar_t structure to the console. */
void
pcicfg_bar_print(pci_bar_t *bar)
{
	if (bar->address == 0) {
		printk("    BAR[%d]: NONE\n", bar->index);
		return;
	}

	if (bar->mem == PCIM_BAR_MEM_SPACE) {
		if (bar->type == 0) {
			printk("    BAR[%d]: MEM addr=0x%08x, [size=%u%s] (32-bit, %s)\n",
			       bar->index, (uint32_t)bar->address,
			       (bar->size >= (1024 * 1024)) ? (uint32_t)(bar->size / (1024 * 1024)) : 
			       (bar->size >= 1024) ? (uint32_t)(bar->size / 1024) :
			       (uint32_t)bar->size,
			       (bar->size >= (1024 * 1024)) ? "M" : 
			       (bar->size >= 1024) ? "K" : "",
			       (bar->prefetch) ? "prefetchable" : "non-prefetchable");
		} else if (bar->type == 1) {
			printk("    BAR[%d]: MEM addr=0x%08x [size=%u%s] (< 1MB, %s)\n",
			       bar->index, (uint32_t)bar->address,
			       (bar->size >= (1024 * 1024)) ? (uint32_t)(bar->size / (1024 * 1024)) : 
			       (bar->size >= 1024) ? (uint32_t)(bar->size / 1024) :
			       (uint32_t)bar->size,
			       (bar->size >= (1024 * 1024)) ? "M" : 
			       (bar->size >= 1024) ? "K" : "",
			       (bar->prefetch) ? "prefetchable" : "non-prefetchable");
		} else if (bar->type == 2) {
			printk("    BAR[%d]: MEM addr=0x%016lx [size=%u%s] (64-bit, %s)\n",
			       bar->index, (unsigned long)bar->address,
			       (bar->size >= (1024 * 1024)) ? (uint32_t)(bar->size / (1024 * 1024)) : 
			       (bar->size >= 1024) ? (uint32_t)(bar->size / 1024) :
			       (uint32_t)bar->size,
			       (bar->size >= (1024 * 1024)) ? "M" : 
			       (bar->size >= 1024) ? "K" : "",
			       (bar->prefetch) ? "prefetchable" : "non-prefetchable");
		}
	} else {
		printk("    BAR[%d]: IO  addr=0x%04x [size=%u]\n",
		       bar->index, (uint16_t)bar->address,
		       (uint32_t)(bar->size));
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

	{
		pci_bar_t tmp_bar;
		int i;

		for (i = 0; i < 6; i++) {
			pcicfg_bar_decode(hdr, i, &tmp_bar);
			pcicfg_bar_print(&tmp_bar);
		}
	}
	
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
