/*
 * LWK specific implementations of ioremap and friends.
 * This depends on the BIOS setting up the MTRRs correctly.
 */

#include <arch/page.h>
#include <arch/io.h>

void __iomem *
ioremap_nocache(unsigned long offset, unsigned long size)
{
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
