/*
 * LWK specific implementations of ioremap and friends.
 * This depends on the BIOS setting up the MTRRs correctly.
 */

#include <lwk/aspace.h>
#include <arch/pgtable.h>
#include <arch/page.h>
#include <arch/io.h>

void __iomem *
ioremap_nocache(unsigned long offset, unsigned long size)
{
	paddr_t paddr;
	int ret=0;

	for (paddr = offset; paddr < offset + size; paddr += VM_PAGE_4KB) {
		if (arch_aspace_virt_to_phys(&bootstrap_aspace, (vaddr_t)__va(paddr), NULL) == -ENOENT) {
			ret = arch_aspace_map_page(
			          &bootstrap_aspace,
			          (vaddr_t)__va(paddr),
			          paddr,
			          _KERNPG_TABLE | _PAGE_PCD,
			          VM_PAGE_4KB
			);

			if (ret) {
				printk(KERN_ERR "Error: could not map kernel memory for MMIO paddr=0x%016lx.\n", paddr);
				break;
			}
		}
	}

	if (ret) {
		printk(KERN_ERR "Error: ioremap_nocache failed at %lx, size %lu\n", offset, size);
		return (void *) NULL;
	}

	return phys_to_virt(offset);
}

void __iomem *
ioremap(unsigned long offset, unsigned long size)
{
	return ioremap_nocache(offset, size);
}

void
iounmap(volatile void __iomem *addr)
{
	/* Nothing to do */
}

int
ioremap_change_attr(unsigned long vaddr, unsigned long size, unsigned long prot_val)
{
	/* TODO: actually do something */
	return 0;
}
