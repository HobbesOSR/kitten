#ifndef _ARCH_X86_64_IDT_VECTORS_H
#define _ARCH_X86_64_IDT_VECTORS_H

/**
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

/**
 * Symbolic names for interrupt vectors.
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
#define COPROCESSOR_SEGMENT_OVERRUN_VECTOR	9
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
/* [20,31] are reserved by x86_64 architecture for future use */
/* [32,238] are free */
#define LOCAL_TIMER_VECTOR			239
#define INVALIDATE_TLB_0_VECTOR			240
#define INVALIDATE_TLB_1_VECTOR			241
#define INVALIDATE_TLB_2_VECTOR			242
#define INVALIDATE_TLB_3_VECTOR			243
#define INVALIDATE_TLB_4_VECTOR			244
#define INVALIDATE_TLB_5_VECTOR			245
#define INVALIDATE_TLB_6_VECTOR			246
#define INVALIDATE_TLB_7_VECTOR			247
/* 248 is free */
#define THRESHOLD_APIC_VECTOR			249
#define THERMAL_APIC_VECTOR			250
/* 251 is free */
#define CALL_FUNCTION_VECTOR			252
#define RESCHEDULE_VECTOR			253
#define ERROR_APIC_VECTOR			254
#define SPURIOUS_APIC_VECTOR			255

/**
 * Meta-defines describing the interrupt vector space defined above.
 */
#define FIRST_EXTERNAL_VECTOR			32
#define FIRST_SYSTEM_VECTOR			239
#define INVALIDATE_TLB_VECTOR_START		240
#define INVALIDATE_TLB_VECTOR_END		247
#define NUM_INVALIDATE_TLB_VECTORS		8

#endif
