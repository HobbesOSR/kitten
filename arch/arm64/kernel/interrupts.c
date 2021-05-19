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
#include <lwk/interrupt.h>
//#include <arch/desc.h>
#include <arch/extable.h>
#include <arch/irq_vectors.h>
#include <arch/xcall.h>
//#include <arch/i387.h>
#include <arch/io.h>
#include <arch/proto.h>


idtvec_handler_t irqvec_table[NUM_IRQ_ENTRIES] __aligned(PAGE_SIZE_4KB);
static DEFINE_SPINLOCK(irqvec_table_lock);


/** \name Interrupt handlers
 * @{
 */
void
do_unhandled_exception(struct pt_regs * regs, unsigned int vector)
{
	printk(KERN_EMERG
		"Unhandled Exception! (vector=%u)\n", vector);
}


/** \name Interrupt handlers
 * @{
 */
void
do_unhandled_irq(struct pt_regs *regs, unsigned int vector)
{

	printk(KERN_EMERG
		"Unhandled Interrupt! (vector=%u)\n", vector);
	
}


static inline u64 get_mair_el1()
{
	u64 mair = 0xdeadbeefULL;;
	__asm__ __volatile__("mrs %0, mair_el1\n":"=r"(mair));
	return mair;
}

static inline u64 get_spsr_el1()
{
	u64 spsr = 0xdeadbeefULL;
	__asm__ __volatile__("mrs %0, spsr_el1\n":"=r"(spsr));
	return spsr;
}

void
do_mem_abort(unsigned long    addr, 
	     unsigned int     esr,
	     struct pt_regs * regs)
{
	u64 mair_el1 = get_mair_el1();
	u64 spsr_el1 = get_spsr_el1();
	u64 tpidr_el0 = __mrs(tpidr_el0);

	printk("do_mem_abort: addr=%p, esr=%x\n", (void *)addr, esr);
	printk("MAIR_EL1: %p\n", (void *)mair_el1);
	printk("SPSR_EL1: %p\n", (void *)spsr_el1);
	printk("TPIDR_EL0: %p\n", (void *)tpidr_el0);


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
handle_irq(struct pt_regs * regs,
	   unsigned int	    vector)
{

	irqreturn_t ret = IRQ_NONE;
	uint64_t eoi_val = vector;

	//	printk(">> Hardware IRQ!!!!\n");

	
	irqvec_table[vector](regs, vector);


	msr(ICC_EOIR1_EL1, vector);


}



void
set_irq_handler(
	unsigned int		vector,	//!< Interrupt vector number
	irq_handler_t   	handler //!< Callback function
)
{
	char namebuf[KSYM_NAME_LEN+1];
	unsigned long symsize, offset;
	unsigned long irqstate;

	ASSERT(vector < NUM_IRQ_ENTRIES);

	if (handler != &do_unhandled_irq) {
		printk(KERN_DEBUG "IDT Vector %3u -> %s()\n",
			vector, kallsyms_lookup( (unsigned long)handler,
			                          &symsize, &offset, namebuf )
		);
	}

	spin_lock_irqsave(&irqvec_table_lock, irqstate);
	irqvec_table[vector] = handler;
	spin_unlock_irqrestore(&irqvec_table_lock, irqstate);

}



/**
 * Initialize high level exception handling 
 */
void __init
interrupts_init(void)
{
	int vector;

	for (vector = 0; vector < NUM_IRQ_ENTRIES; vector++) {
		set_irq_handler(vector, &do_unhandled_irq);
	}




}


