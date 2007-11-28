#include <lwk/kernel.h>
#include <lwk/show.h>

/**
 * Prints the contents of memory in hex to the console.
 * The region printed starts at vaddr and extends n unsigned longs.
 */
void
show_memory(unsigned long vaddr, size_t n)
{
	int i;

	for (i = 0; i < n; i++) {
		printk(KERN_DEBUG "0x%016lx: 0x%08x_%08x\n",
			vaddr,
			*((unsigned int *)(vaddr+4)),
			*((unsigned int *)(vaddr))
		);
		vaddr += sizeof(unsigned long);
	}
}

