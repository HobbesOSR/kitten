#ifndef _ARCH_X86_64_IDT_VECTORS_H
#define _ARCH_X86_64_IDT_VECTORS_H

/*
 * Based on linux/include/asm-x86_64/hw_irq.h
 * Original file header:
 *     (C) 1992, 1993 Linus Torvalds, (C) 1997 Ingo Molnar
 *     moved some of the old arch/i386/kernel/irq.h to here. VY
 *     IRQ/IPI changes taken from work by Thomas Radke
 *     <tomsoft@informatik.tu-chemnitz.de>
 *     hacked by Andi Kleen for x86-64.
 */

/**
 * This file defines symbolic names for interrupt vectors installed in the
 * Interrupt Descriptor Table (IDT). The IDT contains entries for 256 interrupt
 * vectors.  Vectors [0,31] are used for specific purposes defined by the x86_64
 * architecture.  Vectors [32,255] are available for external interrupts. The
 * LWK uses a number of interrupt vectors for its own internal purposes, e.g.,
 * inter-processor interrupts for TLB invalidations.
 */

/*
 * [0,31] Standard x86_64 architecture vectors
 */
#define DIVIDE_ERROR_VECTOR			0
#define DEBUG_VECTOR				1
#define NMI_VECTOR				2
#define INT3_VECTOR				3
#define OVERFLOW_VECTOR				4
#define BOUNDS_VECTOR				5
#define INVALID_OP_VECTOR			6
#define DEVICE_NOT_AVAILABLE_VECTOR		7
#define DOUBLE_FAULT_VECTOR			8
#define COPROC_SEGMENT_OVERRUN_VECTOR		9
#define INVALID_TSS_VECTOR			10
#define SEGMENT_NOT_PRESENT_VECTOR		11
#define STACK_SEGMENT_VECTOR			12
#define GENERAL_PROTECTION_VECTOR		13
#define PAGE_FAULT_VECTOR			14
#define SPURIOUS_INTERRUPT_BUG_VECTOR		15
#define COPROCESSOR_ERROR_VECTOR		16
#define ALIGNMENT_CHECK_VECTOR			17
#define MACHINE_CHECK_VECTOR			18
#define SIMD_COPROCESSOR_ERROR_VECTOR		19
/*
 * [20,31] Reserved by x86_64 architecture for future use
 * [32,47] Free for use by devices
 * [48,63] Standard ISA IRQs
 */
#define IRQ0_VECTOR				48
#define IRQ1_VECTOR				49
#define IRQ2_VECTOR				50
#define IRQ3_VECTOR				51
#define IRQ4_VECTOR				52
#define IRQ5_VECTOR				53
#define IRQ6_VECTOR				54
#define IRQ7_VECTOR				55
#define IRQ8_VECTOR				56
#define IRQ9_VECTOR				57
#define IRQ10_VECTOR				58
#define IRQ11_VECTOR				59
#define IRQ12_VECTOR				60
#define IRQ13_VECTOR				61
#define IRQ14_VECTOR				62
#define IRQ15_VECTOR				63
/*
 * [64,238]  Free for use by devices
 * [239,255] Used by LWK for various internal purposes
 */
#define APIC_TIMER_VECTOR			239
#define INVALIDATE_TLB_0_VECTOR			240
#define INVALIDATE_TLB_1_VECTOR			241
#define INVALIDATE_TLB_2_VECTOR			242
#define INVALIDATE_TLB_3_VECTOR			243
#define INVALIDATE_TLB_4_VECTOR			244
#define INVALIDATE_TLB_5_VECTOR			245
#define INVALIDATE_TLB_6_VECTOR			246
#define INVALIDATE_TLB_7_VECTOR			247
#define LWK_XCALL_FUNCTION_VECTOR		248
#define APIC_PERF_COUNTER_VECTOR		249
#define APIC_THERMAL_VECTOR			250
#define LWK_XCALL_RESCHEDULE_VECTOR		251
#define LINUX_XCALL_FUNCTION_VECTOR		252
#define LINUX_XCALL_RESCHEDULE_VECTOR		253
#define APIC_ERROR_VECTOR			254
#define APIC_SPURIOUS_VECTOR			255

/**
 * Meta-defines describing the interrupt vector space defined above.
 */
#define NUM_IDT_ENTRIES				256
#define FIRST_EXTERNAL_VECTOR			32
#define FIRST_AVAIL_VECTOR                      64
#define FIRST_SYSTEM_VECTOR			239
#define INVALIDATE_TLB_VECTOR_START		240
#define INVALIDATE_TLB_VECTOR_END		247
#define NUM_INVALIDATE_TLB_VECTORS		8

#endif
