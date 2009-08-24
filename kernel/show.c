#include <lwk/kernel.h>
#include <lwk/show.h>


/**
 * Prints the contents of memory in hex to the console.
 * The region printed starts at vaddr and extends n unsigned longs.
 */
void
show_memory(vaddr_t vaddr, size_t n)
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


/**
 * Prints the contents of the passed in register context to the console.
 */
void
show_registers(struct pt_regs *regs)
{
	arch_show_registers(regs);
}


/**
 * Prints the current kernel stack trace to the console.
 */
void
show_kstack(void)
{
	printk(KERN_DEBUG "Kernel Stack Trace:\n");
	arch_show_kstack();
}
