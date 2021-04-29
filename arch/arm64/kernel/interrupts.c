/** \file
 * x86-64 interrupt handlers.
 *
 * 
 */
#include <lwk/aspace.h>
#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/kallsyms.h>
#include <lwk/kgdb.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <lwk/timer.h>
#include <lwk/show.h>
//#include <arch/desc.h>
#include <arch/extable.h>
#include <arch/idt_vectors.h>
#include <arch/xcall.h>
//#include <arch/i387.h>
#include <arch/io.h>
#include <arch/proto.h>




/** \name Interrupt handlers
 * @{
 */
void
do_unhandled_exception(struct pt_regs * regs, unsigned int vector)
{
	printk(KERN_EMERG
		"Unhandled Exception! (vector=%u)\n", vector);
}



static inline u64 get_mair_el1()
{
	u64 mair;
	__asm__ __volatile__("mrs %0, mair_el1\n":"=r"(mair));
	return mair;
}

void
do_mem_abort(unsigned long    addr, 
	     unsigned int     esr,
	     struct pt_regs * regs)
{
	u64 mair_el1 = get_mair_el1();

	printk("do_mem_abort: addr=%p, esr=%x\n", (void *)addr, esr);
	printk("MAIR_EL1: %p\n", (void *)mair_el1);

	print_pgtable_arm64((void *)addr);

	struct siginfo s;
	memset(&s, 0, sizeof(struct siginfo));
	s.si_signo = SIGSEGV;
	s.si_errno = 0;
	s.si_addr  = (void *)addr;
	s.si_code  = SEGV_ACCERR; // SEGV_MAPERR;

	int i = sigsend(current->aspace->id, ANY_ID, SIGSEGV, &s);
	printk(">> PAGE FAULT! Sent signal %d: Addr is %p\n", i, s.si_addr);
	show_registers(regs);

	panic("Page faults are a no-no\n");
}


void
set_idtvec_handler(
	unsigned int		vector,	//!< Interrupt vector number
	idtvec_handler_t	handler //!< Callback function
)
{
	panic("IDT is not an ARM construct\n");
}


/**
 * Initialize high level exception handling 
 */
void __init
interrupts_init(void)
{

}


