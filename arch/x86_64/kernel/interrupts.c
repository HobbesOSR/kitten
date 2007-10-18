#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/kallsyms.h>
#include <arch/desc.h>
#include <arch/idt_vectors.h>
#include <arch/show.h>

typedef void (*idtvec_handler_t)(struct pt_regs *regs, unsigned int vector);

idtvec_handler_t idtvec_table[NUM_IDT_ENTRIES];

extern void asm_idtvec_table(void);

void
do_unhandled_idt_vector(struct pt_regs *regs, unsigned int vector)
{
	printk(KERN_EMERG "Unhandled IDT Vector! (vector=%u)\n", vector);
	show_registers(regs);
	while (1) {}
}

void
do_divide_error(struct pt_regs *regs, unsigned int vector)
{
	printk("Divide Error Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_nmi(struct pt_regs *regs, unsigned int vector)
{
	printk("NMI Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_int3(struct pt_regs *regs, unsigned int vector)
{
	printk("INT3 Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_overflow(struct pt_regs *regs, unsigned int vector)
{
	printk("Overflow Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_bounds(struct pt_regs *regs, unsigned int vector)
{
	printk("Bounds Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_invalid_op(struct pt_regs *regs, unsigned int vector)
{
	printk("Invalid Op Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_device_not_available(struct pt_regs *regs, unsigned int vector)
{
	printk("Device Not Available Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_double_fault(struct pt_regs *regs, unsigned int vector)
{
	printk("Double Fault Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_coproc_segment_overrun(struct pt_regs *regs, unsigned int vector)
{
	printk("Coprocessor Segment Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_invalid_tss(struct pt_regs *regs, unsigned int vector)
{
	printk("Invalid TSS Exception)\n");
	show_registers(regs);
	while (1) {}
}

void
do_segment_not_present(struct pt_regs *regs, unsigned int vector)
{
	printk("Segment Not Present Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_stack_segment(struct pt_regs *regs, unsigned int vector)
{
	printk("Stack Segment Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_general_protection(struct pt_regs *regs, unsigned int vector)
{
	printk("General Protection Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_page_fault(struct pt_regs *regs, unsigned int vector)
{
	printk("Page Fault Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_spurious_interrupt_bug(struct pt_regs *regs, unsigned int vector)
{
	printk("Spurious Interrupt Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_coprocessor_error(struct pt_regs *regs, unsigned int vector)
{
	printk("Coprocessor Error Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_alignment_check(struct pt_regs *regs, unsigned int vector)
{
	printk("Alignment Check Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_machine_check(struct pt_regs *regs, unsigned int vector)
{
	printk("Machine Check Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_simd_coprocessor_error(struct pt_regs *regs, unsigned int vector)
{
	printk("SIMD Coprocessor Error Exception\n");
	show_registers(regs);
	while (1) {}
}

void
do_apic_timer(struct pt_regs *regs, unsigned int vector)
{
	printk("Got APIC Timer Interrupt, vector=%u\n", vector);
	lapic_ack_interrupt();
}

void
do_apic_perf_counter(struct pt_regs *regs, unsigned int vector)
{
	printk("APIC Perf. Counter Interrupt, vector=%u\n", vector);
	show_registers(regs);
	while (1) {}
}

void
do_apic_thermal(struct pt_regs *regs, unsigned int vector)
{
	printk("APIC Thermal Interrupt, vector=%u\n", vector);
	show_registers(regs);
	while (1) {}
}

void
do_apic_error(struct pt_regs *regs, unsigned int vector)
{
	printk("APIC Error Interrupt, vector=%u\n", vector);
	show_registers(regs);
	while (1) {}
}

void
do_apic_spurious(struct pt_regs *regs, unsigned int vector)
{
	printk("APIC Spurious Interrupt, vector=%u\n", vector);
	show_registers(regs);
	while (1) {}
}

void __init
set_idtvec_handler(unsigned int vector, idtvec_handler_t handler)
{
	char namebuf[KSYM_NAME_LEN+1];
	unsigned long symsize, offset;

	ASSERT(vector < NUM_IDT_ENTRIES);

	if (handler != &do_unhandled_idt_vector) {
		printk(KERN_DEBUG "IDT Vector %3u -> %s()\n",
			vector, kallsyms_lookup( (unsigned long)handler,
			                          &symsize, &offset, namebuf )
		);
	}

	idtvec_table[vector] = handler;
}

void
do_interrupt(struct pt_regs *regs, unsigned int vector)
{
	idtvec_table[vector](regs, vector);
	if (vector >= FIRST_EXTERNAL_VECTOR)
		lapic_ack_interrupt();
}

void __init
interrupts_init(void)
{
	int vector;

	/*
	 * Initialize the Interrupt Descriptor Table (IDT).
	 */
	for (vector = 0; vector < NUM_IDT_ENTRIES; vector++) {
		void *asm_handler = (void *) (
		  (unsigned long)(&asm_idtvec_table) + (vector * 16)
		);
		set_intr_gate(vector, asm_handler);
		set_idtvec_handler(vector, &do_unhandled_idt_vector);
	}

	/*
	 * Register handlers for the standard x86_64 interrupts & exceptions.
	 */
	set_idtvec_handler( DIVIDE_ERROR_VECTOR,           &do_divide_error           );
	set_idtvec_handler( NMI_VECTOR,                    &do_nmi                    );
	set_idtvec_handler( INT3_VECTOR,                   &do_int3                   );
	set_idtvec_handler( OVERFLOW_VECTOR,               &do_overflow               );
	set_idtvec_handler( BOUNDS_VECTOR,                 &do_bounds                 );
	set_idtvec_handler( INVALID_OP_VECTOR,             &do_invalid_op             );
	set_idtvec_handler( DEVICE_NOT_AVAILABLE_VECTOR,   &do_device_not_available   );
	set_idtvec_handler( DOUBLE_FAULT_VECTOR,           &do_double_fault           );
	set_idtvec_handler( COPROC_SEGMENT_OVERRUN_VECTOR, &do_coproc_segment_overrun );
	set_idtvec_handler( INVALID_TSS_VECTOR,            &do_invalid_tss            );
	set_idtvec_handler( SEGMENT_NOT_PRESENT_VECTOR,    &do_segment_not_present    );
	set_idtvec_handler( STACK_SEGMENT_VECTOR,          &do_stack_segment          );
	set_idtvec_handler( GENERAL_PROTECTION_VECTOR,     &do_general_protection     );
	set_idtvec_handler( PAGE_FAULT_VECTOR,             &do_page_fault             );
	set_idtvec_handler( SPURIOUS_INTERRUPT_BUG_VECTOR, &do_spurious_interrupt_bug );
	set_idtvec_handler( COPROCESSOR_ERROR_VECTOR,      &do_coprocessor_error      );
	set_idtvec_handler( ALIGNMENT_CHECK_VECTOR,        &do_alignment_check        );
	set_idtvec_handler( MACHINE_CHECK_VECTOR,          &do_machine_check          );
	set_idtvec_handler( SIMD_COPROCESSOR_ERROR_VECTOR, &do_simd_coprocessor_error );

	/*
	 * Register handlers for all of the local APIC vectors.
	 */
	set_idtvec_handler( APIC_TIMER_VECTOR,        &do_apic_timer        );
	set_idtvec_handler( APIC_PERF_COUNTER_VECTOR, &do_apic_perf_counter );
	set_idtvec_handler( APIC_THERMAL_VECTOR,      &do_apic_thermal      );
	set_idtvec_handler( APIC_ERROR_VECTOR,        &do_apic_error        );
	set_idtvec_handler( APIC_SPURIOUS_VECTOR,     &do_apic_spurious     );
}


