#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/extable.h>
#include <arch/desc.h>
#include <arch/page.h>
#include <arch/signal.h>
#include <arch/siginfo.h>
#include <arch/ptrace.h>
#include <arch/show.h>
#include <arch/proto.h>

/**
 * These handlers are defined in arch/x86_64/entry.S. They do some voodoo
 * setup and then call the C do_[name]() handler. For example, divide_error()
 * calls the C function do_divide_error().
 */
extern void divide_error(void);
extern void nmi(void);
extern void int3(void);
extern void overflow(void);
extern void bounds(void);
extern void invalid_op(void);
extern void device_not_available(void);
extern void double_fault(void);
extern void coprocessor_segment_overrun(void);
extern void invalid_TSS(void);
extern void segment_not_present(void);
extern void stack_segment(void);
extern void general_protection(void);
extern void page_fault(void);
extern void spurious_interrupt_bug(void);
extern void coprocessor_error(void);
extern void alignment_check(void);
extern void machine_check(void);
extern void simd_coprocessor_error(void);

/**
 * Initializes standard x86_64 architecture exceptions.
 * The architecture reserves interrupt vectors [0-31] for specific purposes.
 * Interrupt vectors [32-255] can be used for devices or other purposes
 * (system call traps, interprocessor interrupts, etc.).
 */
void __init
trap_init(void)
{
	set_intr_gate    (  0, &divide_error                                 );
	set_intr_gate_ist(  2, &nmi,                        NMI_STACK        );
	set_intr_gate_ist(  3, &int3,                       DEBUG_STACK      );
	set_system_gate  (  4, &overflow                                     );
	set_intr_gate    (  5, &bounds                                       );
	set_intr_gate    (  6, &invalid_op                                   );
	set_intr_gate    (  7, &device_not_available                         );
	set_intr_gate_ist(  8, &double_fault,               DOUBLEFAULT_STACK);
	set_intr_gate    (  9, &coprocessor_segment_overrun                  );
	set_intr_gate    ( 10, &invalid_TSS                                  );
	set_intr_gate    ( 11, &segment_not_present                          );
	set_intr_gate_ist( 12, &stack_segment,              STACKFAULT_STACK );
	set_intr_gate    ( 13, &general_protection                           );
	set_intr_gate    ( 14, &page_fault                                   );
	set_intr_gate    ( 15, &spurious_interrupt_bug                       );
	set_intr_gate    ( 16, &coprocessor_error                            );
	set_intr_gate    ( 17, &alignment_check                              );
	set_intr_gate_ist( 18, &machine_check,              MCE_STACK        );
	set_intr_gate    ( 19, &simd_coprocessor_error                       );
}


#if 0
/**
 * Common exception handler code.
 */
static void
do_trap(
	int              trapnr,        /* Trap number */
	int              signr,         /* Signal number to generate */
	char *           str,           /* Short descriptive string */
	struct pt_regs * regs,          /* Registers at time of trap */
	long             error_code,    /* Error code */
	siginfo_t *      info           /* Signal information */
)
{
#if 0
	struct task *tsk = current;

	if (user_mode(regs)) {
		/*
		 * We want error_code and trap_no set for userspace
		 * faults and kernelspace faults which result in
		 * die(), but not kernelspace faults which are fixed
		 * up.  die() gives the process no chance to handle
		 * the signal and notice the kernel fault information,
		 * so that won't result in polluting the information
		 * about previously queued, but not yet delivered,
		 * faults.  See also do_general_protection below.
		 */
		tsk->arch.thread.error_code = error_code;
		tsk->arch.thread.trap_no = trapnr;

		if (unhandled_signal(tsk, signr))
			printk(KERN_INFO
			       "%s[%d] trap %s rip:%lx rsp:%lx error:%lx\n",
			       tsk->name, tsk->pid, str,
			       regs->rip, regs->rsp, error_code); 

#if 0
		if (info)
			force_sig_info(signr, info, tsk);
		else
			force_sig(signr, tsk);
#endif
		return;
	}


	/* kernel trap */ 
	{	     
		const struct exception_table_entry *fixup;
		fixup = search_exception_table(regs->rip);
		if (fixup)
			regs->rip = fixup->fixup;
		else {
			tsk->arch.thread.error_code = error_code;
			tsk->arch.thread.trap_no = trapnr;
			//die(str, regs, error_code);
		}

		return;
	}
#endif
}
#endif

void
do_divide_error(struct pt_regs *regs, long error_code)
{
	printk("Divide Error Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_nmi(struct pt_regs *regs, long error_code)
{
	printk("NMI Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_int3(struct pt_regs *regs, long error_code)
{
	printk("INT3 Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_overflow(struct pt_regs *regs, long error_code)
{
	printk("Overflow Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_bounds(struct pt_regs *regs, long error_code)
{
	printk("Bounds Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_invalid_op(struct pt_regs *regs, long error_code)
{
	printk("Invalid Op Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_device_not_available(struct pt_regs *regs, long error_code)
{
	printk("Device Not Available Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_double_fault(struct pt_regs *regs, long error_code)
{
	printk("Double Fault Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_coprocessor_segment_overrun(struct pt_regs *regs, long error_code)
{
	printk("Coprocessor Segment Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_invalid_TSS(struct pt_regs *regs, long error_code)
{
	printk("Invalid TSS Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_segment_not_present(struct pt_regs *regs, long error_code)
{
	printk("Segment Not Present Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_stack_segment(struct pt_regs *regs, long error_code)
{
	printk("Stack Segment Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_general_protection(struct pt_regs *regs, long error_code)
{
	printk("General Protection Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_page_fault(struct pt_regs *regs, long error_code)
{
	printk("Page Fault Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_spurious_interrupt_bug(struct pt_regs *regs, long error_code)
{
	printk("Spurious Interrupt Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_coprocessor_error(struct pt_regs *regs, long error_code)
{
	printk("Coprocessor Error Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_alignment_check(struct pt_regs *regs, long error_code)
{
	printk("Alignment Check Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_machine_check(struct pt_regs *regs, long error_code)
{
	printk("Machine Check Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

void
do_simd_coprocessor_error(struct pt_regs *regs, long error_code)
{
	printk("SIMD Coprocessor Error Exception (error code=%ld)\n", error_code);
	show_registers(regs);
	while (1) {}
}

