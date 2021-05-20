/*
 * Based on arch/arm/include/asm/assembler.h
 *
 * Copyright (C) 1996-2000 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASSEMBLY__
#error "Only include this from assembly code"
#endif

//#include <asm/ptrace.h>

/*
 * Stack pushing/popping (register pairs only). Equivalent to store decrement
 * before, load increment after.
 */
	.macro	push, xreg1, xreg2
	stp	\xreg1, \xreg2, [sp, #-16]!
	.endm

	.macro	pop, xreg1, xreg2
	ldp	\xreg1, \xreg2, [sp], #16
	.endm

/*
 * Enable and disable interrupts.
 */
	.macro	disable_irq
	msr	daifset, #2
	.endm

	.macro	enable_irq
	msr	daifclr, #2
	.endm

/*
 * Save/disable and restore interrupts.
 */
	.macro	save_and_disable_irqs, olddaif
	mrs	\olddaif, daif
	disable_irq
	.endm

	.macro	restore_irqs, olddaif
	msr	daif, \olddaif
	.endm

/*
 * Enable and disable debug exceptions.
 */
	.macro	disable_dbg
	msr	daifset, #8
	.endm

	.macro	enable_dbg
	msr	daifclr, #8
	.endm

	.macro	disable_step, tmp
	mrs	\tmp, mdscr_el1
	bic	\tmp, \tmp, #1
	msr	mdscr_el1, \tmp
	.endm

	.macro	enable_step, tmp
	mrs	\tmp, mdscr_el1
	orr	\tmp, \tmp, #1
	msr	mdscr_el1, \tmp
	.endm

	.macro	enable_dbg_if_not_stepping, tmp
	mrs	\tmp, mdscr_el1
	tbnz	\tmp, #1, 9990f
	enable_dbg
9990:
	.endm

/*
 * SMP data memory barrier
 */
	.macro	smp_dmb, opt
#ifdef CONFIG_SMP
	dmb	\opt
#endif
	.endm

#define USER(l, x...)				\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.quad	9999b,l;			\
	.previous

/*
 * Register aliases.
 */
lr	.req	x30		// link register



/*
 * Pseudo-ops for PC-relative adr/ldr/str <reg>, <symbol> where
 * <symbol> is within the range +/- 4 GB of the PC when running
 * in core kernel context. In module context, a movz/movk sequence
 * is used, since modules may be loaded far away from the kernel
 * when KASLR is in effect.
 */

	/*
	 * @dst: destination register (64 bit wide)
	 * @sym: name of the symbol
	 */
	.macro	adr_l, dst, sym
#ifndef MODULE
	adrp	\dst, \sym
	add	\dst, \dst, :lo12:\sym
#else
	movz	\dst, #:abs_g3:\sym
	movk	\dst, #:abs_g2_nc:\sym
	movk	\dst, #:abs_g1_nc:\sym
	movk	\dst, #:abs_g0_nc:\sym
#endif
	.endm

	/*
	 * @dst: destination register (32 or 64 bit wide)
	 * @sym: name of the symbol
	 * @tmp: optional 64-bit scratch register to be used if <dst> is a
	 *       32-bit wide register, in which case it cannot be used to hold
	 *       the address
	 */
	.macro	ldr_l, dst, sym, tmp=
#ifndef MODULE
	.ifb	\tmp
	adrp	\dst, \sym
	ldr	\dst, [\dst, :lo12:\sym]
	.else
	adrp	\tmp, \sym
	ldr	\dst, [\tmp, :lo12:\sym]
	.endif
#else
	.ifb	\tmp
	adr_l	\dst, \sym
	ldr	\dst, [\dst]
	.else
	adr_l	\tmp, \sym
	ldr	\dst, [\tmp]
	.endif
#endif
	.endm

	/*
	 * @src: source register (32 or 64 bit wide)
	 * @sym: name of the symbol
	 * @tmp: mandatory 64-bit scratch register to calculate the address
	 *       while <src> needs to be preserved.
	 */
	.macro	str_l, src, sym, tmp
#ifndef MODULE
	adrp	\tmp, \sym
	str	\src, [\tmp, :lo12:\sym]
#else
	adr_l	\tmp, \sym
	str	\src, [\tmp]
#endif
	.endm

/*
 * Annotate a function as position independent, i.e., safe to be called before
 * the kernel virtual mapping is activated.
 */
#define ENDPIPROC(x)			\
	.globl	__pi_##x;		\
	.type 	__pi_##x, %function;	\
	.set	__pi_##x, x;		\
	.size	__pi_##x, . - x;	\
	ENDPROC(x)


/*
 * mov_q - move an immediate constant into a 64-bit register using
 *         between 2 and 4 movz/movk instructions (depending on the
 *         magnitude and sign of the operand)
 */
	.macro	mov_q, reg, val
	.if (((\val) >> 31) == 0 || ((\val) >> 31) == 0x1ffffffff)
	movz	\reg, :abs_g1_s:\val
	.else
	.if (((\val) >> 47) == 0 || ((\val) >> 47) == 0x1ffff)
	movz	\reg, :abs_g2_s:\val
	.else
	movz	\reg, :abs_g3:\val
	movk	\reg, :abs_g2_nc:\val
	.endif
	movk	\reg, :abs_g1_nc:\val
	.endif
	movk	\reg, :abs_g0_nc:\val
	.endm
