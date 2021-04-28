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





void
do_mem_abort(unsigned long    addr, 
	     unsigned int     esr,
	     struct pt_regs * regs)
{
	printk("do_mem_abort: addr=%p, esr=%x\n", (void *)addr, esr);



	struct siginfo s;
	memset(&s, 0, sizeof(struct siginfo));
	s.si_signo = SIGSEGV;
	s.si_errno = 0;
	s.si_addr  = (void *)NULL; // JRL: TODO FIXME
	s.si_code  = SEGV_ACCERR; // SEGV_MAPERR;

	int i = sigsend(current->aspace->id, ANY_ID, SIGSEGV, &s);
	printk(">> PAGE FAULT! Sent signal %d: CR2 is %p\n", i, s.si_addr);
	show_registers(regs);
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


