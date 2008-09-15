#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/ptrace.h>
#include <lwk/version.h>
#include <lwk/kallsyms.h>
#include <arch/msr.h>

/**
 * Prints a nicely formatted address to the console.
 * This attempts to look up the symbolic name of the address.
 */
void
printk_address(unsigned long address)
{
	unsigned long offset = 0, symsize;
	const char *symname;
	char namebuf[128];

	symname = kallsyms_lookup(address, &symsize, &offset, namebuf);
	if (!symname) {
		printk(" [<%016lx>]\n", address);
		return;
	}
	printk(" [<%016lx>] %s+0x%lx/0x%lx\n",
		address, symname, offset, symsize);
}


/**
 * Print a stack trace of the context.
 */
void
kstack_trace(
	void *			rbp_v
)
{
#ifndef CONFIG_FRAME_POINTER
	printk( "Unable to generate stack trace "
		"(recompile with CONFIG_FRAME_POINTER)\n" );
	return;
#endif

	uint64_t * rbp = rbp_v;
	if( rbp == 0 )
		asm( "mov %%rbp, %0" : "=r"(rbp) );

	int max_depth = 16;
	printk( "Stack trace from RBP %p\n", rbp );

	while( rbp && max_depth-- )
	{
		printk_address( rbp[1] );
		rbp = (uint64_t*) *rbp;
	}
}


/**
 * Prints x86_64 general purpose registers and friends to the console.
 * NOTE: This prints the CPU register values contained in the passed in
 *       'struct pt_regs *'.  It DOES NOT print the current values in
 *       the CPU's registers.
 */
void
show_registers(struct pt_regs * regs)
{
	unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs, shadowgs;
	unsigned int fsindex, gsindex;
	unsigned int ds, cs, es;
	bool user_fault = (regs->rip < PAGE_OFFSET);
	char namebuf[128];

	printk("Task ID: %d   Task Name: %s   UTS_RELEASE: %s\n",
		current->task_id, current->task_name, UTS_RELEASE);
	printk("RIP: %04lx:%016lx (%s)\n", regs->cs & 0xffff, regs->rip,
	       (user_fault) ? "user-context"
	                    : kallsyms_lookup(regs->rip, NULL, NULL, namebuf));
	printk("RSP: %04lx:%016lx EFLAGS: %08lx ERR: %08lx\n",
		regs->ss, regs->rsp, regs->eflags, regs->orig_rax);
	printk("RAX: %016lx RBX: %016lx RCX: %016lx\n",
	       regs->rax, regs->rbx, regs->rcx);
	printk("RDX: %016lx RSI: %016lx RDI: %016lx\n",
	       regs->rdx, regs->rsi, regs->rdi);
	printk("RBP: %016lx R08: %016lx R09: %016lx\n",
	       regs->rbp, regs->r8, regs->r9);
	printk("R10: %016lx R11: %016lx R12: %016lx\n",
	       regs->r10, regs->r11, regs->r12);
	printk("R13: %016lx R14: %016lx R15: %016lx\n",
	       regs->r13, regs->r14, regs->r15);

	asm("movl %%ds,%0" : "=r" (ds));
	asm("movl %%cs,%0" : "=r" (cs));
	asm("movl %%es,%0" : "=r" (es));
	asm("movl %%fs,%0" : "=r" (fsindex));
	asm("movl %%gs,%0" : "=r" (gsindex));

	rdmsrl(MSR_FS_BASE, fs);
	rdmsrl(MSR_GS_BASE, gs);
	rdmsrl(MSR_KERNEL_GS_BASE, shadowgs);

	asm("movq %%cr0, %0": "=r" (cr0));
	asm("movq %%cr2, %0": "=r" (cr2));
	asm("movq %%cr3, %0": "=r" (cr3));
	asm("movq %%cr4, %0": "=r" (cr4));

	printk("FS:  %016lx(%04x) GS:%016lx(%04x) knlGS:%016lx\n",
	       fs, fsindex, gs, gsindex, shadowgs);
	printk("CS:  %04x DS: %04x ES: %04x CR0: %016lx\n", cs, ds, es, cr0);
	printk("CR2: %016lx CR3: %016lx CR4: %016lx\n", cr2, cr3, cr4);

	if (!user_fault)
		kstack_trace( (void *) regs->rbp );
}

