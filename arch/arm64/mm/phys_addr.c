#include <arch/page.h>

extern unsigned long phys_base;

/**
 * This converts a kernel virtual address to a physical address.
 *
 * NOTE: This function only works for kernel virtual addresses in the kernel's
 *       identity mapping of all of physical memory. It will not work for the
 *       fixmap, vmalloc() areas, or any other type of virtual address.
 */
unsigned long
__phys_addr(unsigned long virt_addr)
{
	/* Handle kernel symbols */
	printk("memstart %llx\n",memstart_addr);
	if (virt_addr >= __START_KERNEL_map)
		return virt_addr - __START_KERNEL_map + PHYS_OFFSET;
	/* Handle kernel data */
	return virt_addr - PAGE_OFFSET;
}

