#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/ptrace.h>
#include <lwk/version.h>
#include <lwk/kallsyms.h>
#include <arch/msr.h>


/**
 * Prints a kernel stack trace to the console.
 * If the base pointer passed in is zero the base pointer of the current
 * task used.
 */
static void
__arch_show_kstack(kaddr_t rbp)
{
#ifndef CONFIG_FRAME_POINTER
	printk( "Unable to generate stack trace "
		"(recompile with CONFIG_FRAME_POINTER)\n" );
	return;
#endif

	if (rbp == 0)
		asm("mov %0, x30\n" : "=r" (rbp));

	int max_depth = 16;
	printk(KERN_DEBUG "  Stack trace from RBP %016lx\n", rbp);

	while ((rbp >= PAGE_OFFSET) && max_depth--) {
		kaddr_t sym_addr = ((kaddr_t *)rbp)[1];
		char sym_name[KSYM_SYMBOL_LEN];

		kallsyms_sprint_symbol(sym_name, sym_addr);
		printk(KERN_DEBUG "    [<%016lx>] %s\n", sym_addr, sym_name);

		rbp = *((kaddr_t *)rbp);
	}
}


/**
 * Prints a kernel stack trace of the current task to the console.
 */
void
arch_show_kstack(void)
{
	__arch_show_kstack(0);
}


/**
 * Prints x86_64 general purpose registers and friends to the console.
 * NOTE: This prints the CPU register values contained in the passed in
 *       'struct pt_regs *'.  It DOES NOT print the current values in
 *       the CPU's registers.
 */
void
arch_show_registers(struct pt_regs * regs)
{

	bool user_fault = (regs->pc < PAGE_OFFSET);
	char namebuf[128];

	printk("Task ID: %d   Task Name: %s   UTS_RELEASE: %s\n",
		current ? current->id : -1,
		current ? current->name : "BAD CURRENT",
		UTS_RELEASE
	);


	printk("PC:     %016lx (%s)\n", regs->pc, 
		       (user_fault) ? "user-context"
             			  : kallsyms_lookup(regs->pc, NULL, NULL,  namebuf));
	printk("SP:     %016lx\n", regs->sp);
	printk("PSTATE: %016lx\n", regs->pstate);



	printk("X00: %016lx X01: %016lx X02: %016lx X03: %016lx\n", 
		regs->regs[0], regs->regs[1], regs->regs[2], regs->regs[3]);
	printk("X04: %016lx X05: %016lx X06: %016lx X07: %016lx\n", 
		regs->regs[4], regs->regs[5], regs->regs[6], regs->regs[7]);
	printk("X08: %016lx X09: %016lx X10: %016lx X11: %016lx\n", 
		regs->regs[8], regs->regs[9], regs->regs[10], regs->regs[11]);
	printk("X12: %016lx X13: %016lx X14: %016lx X15: %016lx\n", 
		regs->regs[12], regs->regs[13], regs->regs[14], regs->regs[15]);
	printk("X16: %016lx X17: %016lx X18: %016lx X19: %016lx\n", 
		regs->regs[16], regs->regs[17], regs->regs[18], regs->regs[19]);
	printk("X20: %016lx X21: %016lx X22: %016lx X23: %016lx\n", 
		regs->regs[20], regs->regs[21], regs->regs[22], regs->regs[23]);
	printk("X24: %016lx X25: %016lx X26: %016lx X27: %016lx\n", 
		regs->regs[24], regs->regs[25], regs->regs[26], regs->regs[27]);
	printk("X28: %016lx X29: %016lx X30: %016lx X31: %016lx\n", 
		regs->regs[28], regs->regs[29], regs->regs[30], regs->regs[31]);

	printk("orig_x0: %016lx, syscallno: %016lx\n", regs->orig_x0, regs->syscallno);


	if (!user_fault)
		__arch_show_kstack(regs->sp);

#if 0
	bool user_fault = (regs->rip < PAGE_OFFSET);
	char namebuf[128];

	printk("Task ID: %d   Task Name: %s   UTS_RELEASE: %s\n",
		current ? current->id : -1,
		current ? current->name : "BAD CURRENT",
		UTS_RELEASE
	);
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
#if 0
	asm("movl %%ds,%0" : "=r" (ds));
	asm("movl %%cs,%0" : "=r" (cs));
	asm("movl %%es,%0" : "=r" (es));
	asm("movl %%fs,%0" : "=r" (fsindex));
	asm("movl %%gs,%0" : "=r" (gsindex));
#endif
	rdmsrl(MSR_FS_BASE, fs);
	rdmsrl(MSR_GS_BASE, gs);
	rdmsrl(MSR_KERNEL_GS_BASE, shadowgs);
#if 0
	asm("movq %%cr0, %0": "=r" (cr0));
	asm("movq %%cr2, %0": "=r" (cr2));
	asm("movq %%cr3, %0": "=r" (cr3));
	asm("movq %%cr4, %0": "=r" (cr4));
#endif
	printk("FS:  %016lx(%04x) GS:%016lx(%04x) knlGS:%016lx\n",
	       fs, fsindex, gs, gsindex, shadowgs);
	printk("CS:  %04x DS: %04x ES: %04x CR0: %016lx\n", cs, ds, es, cr0);
	printk("CR2: %016lx CR3: %016lx CR4: %016lx\n", cr2, cr3, cr4);

	if (!user_fault)
		__arch_show_kstack(regs->rbp);
#endif
}
