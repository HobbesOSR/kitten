/*
 * Low-level CPU initialisation
 * Based on arch/arm/kernel/head.S
 *
 * Copyright (C) 1994-2002 Russell King
 * Copyright (C) 2003-2012 ARM Ltd.
 * Authors:	Catalin Marinas <catalin.marinas@arm.com>
 *		Will Deacon <will.deacon@arm.com>
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
#include <lwk/linkage.h>
#include <lwk/init.h>
#include <lwk/const.h>


#include <arch/assembler.h>
#include <arch/ptrace.h>
#include <arch/memory.h>
#include <arch/pgtable.h>
#include <arch/page.h>
#include <arch/cputype.h>


// TODO Brian Needs to have a better location
#define UART_PHYS_ADDR _AC(CONFIG_SERIAL_PHYS,u)
//#define UART_PHYS_ADDR _AC(0x1C021000,u)

//---------------------
//	1:
//	mov x5, #0
//	cmp x5, #0
//	beq 1b
//	// Serial code to indicate failed boot
//	1:
//	// Read status register
//	adr x5, UTLSR
//	ldr w5, [x5] // x5 = addr of UTRSTAT
//
//	adr x4, UTTHR
//	ldr w4, [x4] // x4 = addr of UTXH
//
//	// Beginning of string
//	adr x6, HELLO
//	adr x3, HELLO_END
//
//	2:
//		// if ((readb((u8 *)base + UART_LSR) & 0x20) == 0x20)
//       	ldr w0, [x5] // read status
//
//			// Mask transmit ready bit and check is ready
//			and w0, w0, #0x20
//			cmp w0, #0
//		beq 2b // read status
//
//		ldrb w0, [x6], #1
//		strb w0,[x4]
//		cmp x6, x3
//	bne 2b
//
//	b .
//-------------------

//
// 0x30 -> '0'
// 0x41 -> 'A'
//
// 	b0011 b0000
// 	b0100 b0001
//

/* JRL: Note this is an infinite loop that doesn't do anything */
.macro serial_write, tmp64, tmp32, num
	// wait
	1:
		ldr \tmp64, =UART_PHYS_ADDR
		// read LSR
		ldrb \tmp32, [\tmp64, #0x14]
		and \tmp32, \tmp32, #0x20
		cmp \tmp32, #0
	beq 1b

	lsr \tmp64, \num, #4
	// num / 16 (2^4) right shift by 4
.endm

.macro _serial_write_bytes port, val, more:vararg
    ldr x14, =\port

    ldr x15, =\val
    str x15, [x14]

    .ifnb \more
    _serial_write_bytes \port, \more
    .endif


.endm


.macro serial_write_bytes, byte, args:vararg
    _serial_write_bytes UART_PHYS_ADDR, \byte, \args
.endm

.macro vserial_write_bytes, byte, args:vararg
    _serial_write_bytes EARLYCON_IOBASE, \byte, \args
.endm


.macro _serial_write_str, port:req, byte:req, more:vararg
    _serial_write_bytes \port, \byte, \more

    ldr x14, =\port

    ldr x15, =0x0d
    str x15, [x14]

    ldr x15, =0x0a
    str x15, [x14]


.endm

.macro serial_write_str, byte, args:vararg  
    _serial_write_str UART_PHYS_ADDR, \byte, \args
.endm

.macro vserial_write_str, byte, args:vararg  
    _serial_write_str EARLYCON_IOBASE, \byte, \args
.endm

.macro _serial_write_reg, port, reg
    ldr x14, =\port
    ldr x11, =64
    mov x15, \reg

1000:
    sub x11, x11, 4

    mov x15, \reg
    lsr x15, x15, x11
    and x15, x15, 0xf


    cmp x15, #9
    bgt 2000f

    add x15, x15, #48
    b 3000f
2000:
    add x15, x15,  #55
3000:

    str x15, [x14]

    cmp x11, #0
    bne 1000b

    ldr x15, =0x0d
    str x15, [x14]

    ldr x15, =0x0a
    str x15, [x14]

.endm

.macro serial_write_reg reg
    _serial_write_reg UART_PHYS_ADDR, \reg
.endm

.macro vserial_write_reg reg
    _serial_write_reg EARLYCON_IOBASE, \reg
.endm

.macro HALT
999:
    ldr x14, =0xdeadbeef
    b 999b
.endm

/*
 * swapper_pg_dir is the virtual address of the initial page table. We place
 * the page tables 3 * PAGE_SIZE below KERNEL_RAM_VADDR. The idmap_pg_dir has
 * 2 pages and is placed below swapper_pg_dir.
 */
#define KERNEL_RAM_VADDR	(PAGE_OFFSET + TEXT_OFFSET)

#if (KERNEL_RAM_VADDR & 0xfffff) != 0x80000
#error KERNEL_RAM_VADDR must start at 0xXXX80000
#endif

#define SWAPPER_DIR_SIZE	(3 * PAGE_SIZE)
#define IDMAP_DIR_SIZE		(2 * PAGE_SIZE)

	.globl	swapper_pg_dir
	.equ	swapper_pg_dir, KERNEL_RAM_VADDR - SWAPPER_DIR_SIZE

	.globl	idmap_pg_dir
	.equ	idmap_pg_dir, swapper_pg_dir - IDMAP_DIR_SIZE

	.macro	pgtbl, ttb0, ttb1, phys
	add	\ttb1, \phys, #TEXT_OFFSET - SWAPPER_DIR_SIZE
	sub	\ttb0, \ttb1, #IDMAP_DIR_SIZE
	.endm

#ifdef CONFIG_ARM64_64K_PAGES
#define BLOCK_SHIFT	PAGE_SHIFT
#define BLOCK_SIZE	PAGE_SIZE
#else
#define BLOCK_SHIFT	SECTION_SHIFT
#define BLOCK_SIZE	SECTION_SIZE
#endif

#define KERNEL_START	KERNEL_RAM_VADDR
#define KERNEL_END	_end

/*
 * Initial memory map attributes.
 */
#ifndef CONFIG_SMP
#define PTE_FLAGS	PTE_TYPE_PAGE | PTE_AF
#define PMD_FLAGS	PMD_TYPE_SECT | PMD_SECT_AF
#else
#define PTE_FLAGS	PTE_TYPE_PAGE | PTE_AF | PTE_SHARED
#define PMD_FLAGS	PMD_TYPE_SECT | PMD_SECT_AF | PMD_SECT_S
#endif

#ifdef CONFIG_ARM64_64K_PAGES
#define MM_MMUFLAGS	PTE_ATTRINDX(MT_NORMAL) | PTE_FLAGS
#else
#define MM_MMUFLAGS	PMD_ATTRINDX(MT_NORMAL) | PMD_FLAGS
#endif

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * The requirements are:
 *   MMU = off, D-cache = off, I-cache = on or off,
 *   x0 = physical address to the FDT blob.
 *
 * This code is mostly position independent so you call this at
 * __pa(PAGE_OFFSET + TEXT_OFFSET).
 *
 * Note that the callee-saved registers are used for storing variables
 * that are useful before the MMU is enabled. The allocations are described
 * in the entry routines.
 */

#	__HEAD
	.text
	.section .bootstrap.text

	/*
	 * DO NOT MODIFY. Image header expected by Linux boot-loaders.
	 */
	b	stext				// branch to kernel start, magic
	.long	0				// reserved
	.quad	TEXT_OFFSET			// Image load offset from start of RAM
	.quad	0				// reserved
	.quad	0				// reserved

ENTRY(stext)
	serial_write_byte_fuck_the_stack 'a'

    // JRL: START debugging loop
30:
        ldr x0, =4
    b 30b
    // JRL: END debuggng 

	mov	x21, x0				// x21=FDT
	bl	el2_setup			// Drop to EL1

	/* The following code looks up the cpu_info structure related to the
	   compiled system. This is used to locate the setup function. I have
	   bypassed this to directly locate this function. */
	//mrs	x22, midr_el1			// x22=cpuid
	//mov	x0, x22
	//bl	lookup_processor_type
	//mov	x23, x0				// x23=current cpu_table
	//cbz	x23, __error_p			// invalid processor (x23=0)?

	bl	__calc_phys_offset		// x24=PHYS_OFFSET, x28=PHYS_OFFSET-PAGE_OFFSET
	bl	__vet_fdt
	bl	__create_page_tables		// x25=TTBR0, x26=TTBR1
	/*
	 * The following calls CPU specific code in a position independent
	 * manner. See arch/arm64/mm/proc.S for details. x23 = base of
	 * cpu_info structure selected by lookup_processor_type above.
	 * On return, the CPU will be ready for the MMU to be turned on and
	 * the TCR will have been set.
	 */
	ldr	x27, __switch_data		// address to jump to after
									// MMU has been enabled
	/* 	x0 (SCTLR enabling MMU) and
		x27 (address to jump to after MMU enabled) must be set !!! */
	mrs x0, sctlr_el1
	orr x0, x0, #1
	adr	lr, __enable_mmu		// return (PIC) address (lr = x30)
	adr x12, __cpu_setup
	//ldr	x12, [x23, #CPU_INFO_SETUP]
	br	x12				// initialise processor
ENDPROC(stext)

/*
 * If we're fortunate enough to boot at EL2, ensure that the world is
 * sane before dropping to EL1.
 */
ENTRY(el2_setup)
	mrs	x0, CurrentEL
	cmp	x0, #PSR_MODE_EL2t
	ccmp	x0, #PSR_MODE_EL2h, #0x4, ne
	b.eq	1f
	ret

	/* Hyp configuration. */
1:	mov	x0, #(1 << 31)			// 64-bit EL1
	msr	hcr_el2, x0

	/* Generic timers. */
	mrs	x0, cnthctl_el2
	orr	x0, x0, #3			// Enable EL1 physical timers
	msr	cnthctl_el2, x0

	/* Populate ID registers. */
	mrs	x0, midr_el1
	mrs	x1, mpidr_el1
	msr	vpidr_el2, x0
	msr	vmpidr_el2, x1

	/* sctlr_el1 */
	mov	x0, #0x0800			// Set/clear RES{1,0} bits
	movk	x0, #0x30d0, lsl #16
	msr	sctlr_el1, x0

	/* Coprocessor traps. */
	mov	x0, #0x33ff
	msr	cptr_el2, x0			// Disable copro. traps to EL2

#ifdef CONFIG_COMPAT
	msr	hstr_el2, xzr			// Disable CP15 traps to EL2
#endif

	/* spsr */
	mov	x0, #(PSR_F_BIT | PSR_I_BIT | PSR_A_BIT | PSR_D_BIT |\
		      PSR_MODE_EL1h)
	msr	spsr_el2, x0
	msr	elr_el2, lr /* lr = x30 */
	eret
ENDPROC(el2_setup)

	.align	3
2:	.quad	.
	.quad	PAGE_OFFSET

#ifdef CONFIG_SMP
	.pushsection    .smp.pen.text, "ax"
	.align	3
1:	.quad	.
	.quad	0//secondary_holding_pen_release



#define BOOT_CPU_MODE_EL1	(0xe11)
#define BOOT_CPU_MODE_EL2	(0xe12)

/*
 * Sets the __boot_cpu_mode flag depending on the CPU boot mode passed
 * in w0. See arch/arm64/include/asm/virt.h for more info.
 */
set_cpu_boot_mode_flag:
	adr_l	x1, __boot_cpu_mode
	cmp	w0, #BOOT_CPU_MODE_EL2
	b.ne	1f
	add	x1, x1, #4
1:	str	w0, [x1]			// This CPU has booted in EL1
	dmb	sy
	dc	ivac, x1			// Invalidate potentially stale cache line
	ret
ENDPROC(set_cpu_boot_mode_flag)


	/*
/*
 * These values are written with the MMU off, but read with the MMU on.
 * Writers will invalidate the corresponding address, discarding up to a
 * 'Cache Writeback Granule' (CWG) worth of data. The linker script ensures
 * sufficient alignment that the CWG doesn't overlap another section.
 */
	.pushsection ".mmuoff.data.write", "aw"
/*
 * We need to find out the CPU boot mode long after boot, so we need to
 * store it in a writable variable.
 *
 * This is not in .bss, because we set it sufficiently early that the boot-time
 * zeroing of .bss would clobber it.
 */
ENTRY(__boot_cpu_mode)
	.long	BOOT_CPU_MODE_EL2
	.long	BOOT_CPU_MODE_EL1
/*
 * The booting CPU updates the failed status @__early_cpu_boot_status,
 * with MMU turned off.
 */
ENTRY(__early_cpu_boot_status)
	.long 	0

	.popsection

	/*
	 * This provides a "holding pen" for platforms to hold all secondary
	 * cores are held until we're ready for them to initialise.
	 */
ENTRY(secondary_holding_pen)
	bl	el2_setup			// Drop to EL1, w0=cpu_boot_mode
	bl	set_cpu_boot_mode_flag
	mrs	x0, mpidr_el1
	mov_q	x1, MPIDR_HWID_BITMASK
	and	x0, x0, x1
	adr_l	x3, secondary_holding_pen_release
pen:	ldr	x4, [x3]
	cmp	x4, x0
	b.eq	secondary_startup
	wfe
	b	pen
ENDPROC(secondary_holding_pen)

	/*
	 * Secondary entry point that jumps straight into the kernel. Only to
	 * be used where CPUs are brought online dynamically by the kernel.
	 */
ENTRY(secondary_entry)
	bl	el2_setup			// Drop to EL1
	bl	set_cpu_boot_mode_flag
	b	secondary_startup
ENDPROC(secondary_entry)


ENTRY(secondary_startup)
	/*
	 * Common entry point for secondary CPUs.
	 */
	mrs	x22, midr_el1			// x22=cpuid

#if 0
	mov	x0, x22
	bl	lookup_processor_type
	mov	x23, x0				// x23=current cpu_table
	cbz	x23, __error_p			// invalid processor (x23=0)?
#endif


	bl	__calc_phys_offset		// x24=phys offset

	pgtbl	x25, x26, x24			// x25=TTBR0, x26=TTBR1
	//ldr	x12, [x23, #CPU_INFO_SETUP]
	//add	x12, x12, x28			// __virt_to_phys
	//blr	x12				// initialise processor

	adr 	lr, __enable_mmu		// return (PIC) address (lr = x30)
	adr 	x12, __cpu_setup

	ldr	x21, =secondary_data
	ldr	x27, =__secondary_switched	// address to jump to after enabling the MMU

	br	x12				// initialise processor

	b	__enable_mmu
ENDPROC(secondary_startup)

ENTRY(__secondary_switched)
	ldr	x0, [x21]			// get secondary_data.stack
	mov	sp, x0
	mov	x29, #0

	b	secondary_start_kernel
ENDPROC(__secondary_switched)
#endif	/* CONFIG_SMP */

/*
 * Setup common bits before finally enabling the MMU. Essentially this is just
 * loading the page table pointer and vector base registers.
 *
 * On entry to this code, x0 must contain the SCTLR_EL1 value for turning on
 * the MMU.
 */
__enable_mmu:
	ldr	x5, =vectors
	msr	vbar_el1, x5
	msr	ttbr0_el1, x25			// load TTBR0
	msr	ttbr1_el1, x26			// load TTBR1
	isb
	b	__turn_mmu_on
ENDPROC(__enable_mmu)

/*
 * Enable the MMU. This completely changes the structure of the visible memory
 * space. You will not be able to trace execution through this.
 *
 *  x0  = system control register
 *  x27 = *virtual* address to jump to upon completion
 *
 * other registers depend on the function called upon completion
 */
	.align	6
__turn_mmu_on:
	msr	sctlr_el1, x0
	isb
	br	x27
ENDPROC(__turn_mmu_on)

/*
 * Calculate the start of physical memory.
 */
__calc_phys_offset:
	adr	x0, 1f
	ldp	x1, x2, [x0]
	sub	x28, x0, x1			// x28 = PHYS_OFFSET - PAGE_OFFSET
	add	x24, x2, x28			// x24 = PHYS_OFFSET
	ret
ENDPROC(__calc_phys_offset)

	.align 3
1:	.quad	.
	.quad	PAGE_OFFSET

/*
 * Macro to populate the PGD for the corresponding block entry in the next
 * level (tbl) for the given virtual address.
 *
 * Preserves:	pgd, tbl, virt
 * Corrupts:	tmp1, tmp2
 */
 // Gigabyte pages not blocks
	.macro	create_pgd_entry, pgd, tbl, virt, tmp1, tmp2
	lsr	\tmp1, \virt, #PGDIR_SHIFT
	and	\tmp1, \tmp1, #PTRS_PER_PGD - 1	// PGD index
	orr	\tmp2, \tbl, #3			// PGD entry table type
	str	\tmp2, [\pgd, \tmp1, lsl #3]
	.endm

/*
 * Macro to populate block entries in the page table for the start..end
 * virtual range (inclusive).
 *
 * Preserves:	tbl, flags
 * Corrupts:	phys, start, end, pstate
 */
	.macro	create_block_map, tbl, flags, phys, start, end, idmap=0
	lsr	\phys, \phys, #BLOCK_SHIFT
	.if	\idmap
	and	\start, \phys, #PTRS_PER_PTE - 1	// table index
	.else
	lsr	\start, \start, #BLOCK_SHIFT
	and	\start, \start, #PTRS_PER_PTE - 1	// table index
	.endif
	orr	\phys, \flags, \phys, lsl #BLOCK_SHIFT	// table entry
	.ifnc	\start,\end
	lsr	\end, \end, #BLOCK_SHIFT
	and	\end, \end, #PTRS_PER_PTE - 1		// table end index
	.endif
9999:	str	\phys, [\tbl, \start, lsl #3]		// store the entry
	.ifnc	\start,\end
	add	\start, \start, #1			// next entry
	add	\phys, \phys, #BLOCK_SIZE		// next block
	cmp	\start, \end
	b.ls	9999b
	.endif
	.endm

/*
 * Setup the initial page tables. We only setup the barest amount which is
 * required to get the kernel running. The following sections are required:
 *   - identity mapping to enable the MMU (low address, TTBR0)
 *   - first few MB of the kernel linear mapping to jump to once the MMU has
 *     been enabled, including the FDT blob (TTBR1)
 *   - UART mapping if CONFIG_EARLY_PRINTK is enabled (TTBR1)
 */
__create_page_tables:

	pgtbl	x25, x26, x24			// idmap_pg_dir and swapper_pg_dir addresses

	/*
	 * Clear the idmap and swapper page tables.
	 */
	mov	x0, x25
	add	x6, x26, #SWAPPER_DIR_SIZE
1:	stp	xzr, xzr, [x0], #16
	stp	xzr, xzr, [x0], #16
	stp	xzr, xzr, [x0], #16
	stp	xzr, xzr, [x0], #16
	cmp	x0, x6
	b.lo	1b

	ldr	x7, =MM_MMUFLAGS

	/*
	 * Create the identity mapping.
	 */
	add	x0, x25, #PAGE_SIZE		// section table address
	adr	x3, __turn_mmu_on		// virtual/physical address
	create_pgd_entry x25, x0, x3, x5, x6
	create_block_map x0, x7, x3, x5, x5, idmap=1

	/*
	 * Map the kernel image (starting with PHYS_OFFSET).
	 */
	add	x0, x26, #PAGE_SIZE		// section table address
	mov	x5, #PAGE_OFFSET
	create_pgd_entry x26, x0, x5, x3, x6
	//ldr x6, =(PAGE_OFFSET + PGDIR_SIZE - 1)
	ldr	x6, =KERNEL_END - 1
	mov	x3, x24				// phys offset
	create_block_map x0, x7, x3, x5, x6

	/*
	 * Map the FDT blob (maximum 2MB; must be within 512MB of
	 * PHYS_OFFSET).
	 */
	mov	x3, x21				// FDT phys address
	and	x3, x3, #~((1 << 21) - 1)	// 2MB aligned
	mov	x6, #PAGE_OFFSET
	sub	x5, x3, x24			// subtract PHYS_OFFSET
	tst	x5, #~((1 << 29) - 1)		// within 512MB?
	csel	x21, xzr, x21, ne		// zero the FDT pointer
	b.ne	1f
	add	x5, x5, x6			// __va(FDT blob)
	add	x6, x5, #1 << 21		// 2MB for the FDT blob
	sub	x6, x6, #1			// inclusive range
	create_block_map x0, x7, x3, x5, x6
1:
#define CONFIG_EARLY_PRINTK
#ifdef CONFIG_EARLY_PRINTK
	/*
	 * Create the pgd entry for the UART mapping. The full mapping is done
	 * later based earlyprintk kernel parameter.
	 */
	ldr	x5, =EARLYCON_IOBASE		// UART virtual address
	add	x0, x26, #2 * PAGE_SIZE		// section table address
	create_pgd_entry x26, x0, x5, x6, x7

	// Map in UART physical
	ldr x3, =UART_PHYS_ADDR
	//and x3, x3, #(-1<<21)
	add x6, x5, #(1<<21)
	ldr	x7, =(PMD_ATTRINDX(MT_DEVICE_nGnRnE) | PMD_FLAGS)//=MM_MMUFLAGS

	create_block_map x0, x7, x3, x5, x6
#endif
#if 0
	.macro	create_pgd_entry, pgd, tbl, virt, tmp1, tmp2
	lsr	\tmp1, \virt, #PGDIR_SHIFT
	and	\tmp1, \tmp1, #PTRS_PER_PGD - 1	// PGD index
	orr	\tmp2, \tbl, #3			// PGD entry table type
	str	\tmp2, [\pgd, \tmp1, lsl #3]
	.endm

#endif

/*  JRL: WTF is this???

	ldr x6, =0x10007d000
	ldr x5, =0x10007e003
	str x5, [x6]
*/


	ret
ENDPROC(__create_page_tables)
	.ltorg

	.align	3
	.type	__switch_data, %object
__switch_data:
	.quad	__mmap_switched
	.quad	__data_loc			// x4
	.quad	_data				// x5
	.quad	__bss_start			// x6
	.quad	_end				// x7
	.quad	0 //processor_id			// x4
	.quad	fdt_start			// x5
	.quad	memstart_addr			// x6
	//.quad	_init_stack-16
	.quad	bootstrap_task_union + TASK_SIZE - 16 //init_thread_union + THREAD_START_SP // sp

/*
 * The following fragment of code is executed with the MMU on in MMU mode, and
 * uses absolute addresses; this is not position independent.
 */
__mmap_switched:
	adr	x3, __switch_data + 8

	ldp	x4, x5, [x3], #16
	ldp	x6, x7, [x3], #16
	cmp	x4, x5				// Copy data segment if needed
1:	ccmp	x5, x6, #4, ne
	b.eq	2f
	ldr	x16, [x4], #8
	str	x16, [x5], #8
	b	1b
2:
1:	cmp	x6, x7
	b.hs	2f
	str	xzr, [x6], #8			// Clear BSS
	b	1b
2:
	ldp	x4, x5, [x3], #16
	ldr	x6, [x3], #8
	ldr	x16, [x3]

	msr SPSel, #1
	mov	sp, x16
	//str	x22, [x4]			// Save processor ID
	str	x21, [x5]			// Save FDT pointer
	str	x24, [x6]			// Save PHYS_OFFSET
	mov	x29, #0

	b	arm64_start_kernel
ENDPROC(__mmap_switched)

//	.globl init_rsp
//init_rsp:
//	.quad  bootstrap_task_union+TASK_SIZE-8

/*
	.align	8
	.globl	initial_code
initial_code:
	.quad	arm64_start_kernel

*/
/*
 * Exception handling. Something went wrong and we can't proceed. We ought to
 * tell the user, but since we don't have any guarantee that we're even
 * running on the right architecture, we do virtually nothing.
 */
__error_p:
ENDPROC(__error_p)

__error:
1:	nop
	b	1b
ENDPROC(__error)

/*
 * This function gets the processor ID in w0 and searches the cpu_table[] for
 * a match. It returns a pointer to the struct cpu_info it found. The
 * cpu_table[] must end with an empty (all zeros) structure.
 *
 * This routine can be called via C code and it needs to work with the MMU
 * both disabled and enabled (the offset is calculated automatically).
 */
ENTRY(lookup_processor_type)
	adr	x1, __lookup_processor_type_data
	ldp	x2, x3, [x1]
	sub	x1, x1, x2			// get offset between VA and PA
	add	x3, x3, x1			// convert VA to PA
1:
	ldp	w5, w6, [x3]			// load cpu_id_val and cpu_id_mask
	cbz	w5, 2f				// end of list?
	and	w6, w6, w0
	cmp	w5, w6
	b.eq	3f
	//add	x3, x3, #CPU_INFO_SZ
	b	1b
2:
	mov	x3, #0				// unknown processor
3:
	mov	x0, x3
	ret
ENDPROC(lookup_processor_type)

	.align	3
	.type	__lookup_processor_type_data, %object
__lookup_processor_type_data:
	.quad	.
	.quad	//cpu_table
	.size	__lookup_processor_type_data, . - __lookup_processor_type_data

/*
 * Determine validity of the x21 FDT pointer.
 * The dtb must be 8-byte aligned and live in the first 512M of memory.
 */
__vet_fdt:
	tst	x21, #0x7
	b.ne	1f
	cmp	x21, x24
	b.lt	1f
	mov	x0, #(1 << 29)
	add	x0, x0, x24
	cmp	x21, x0
	b.ge	1f
	ret
1:
	mov	x21, #0
	ret
ENDPROC(__vet_fdt)

/* The following code was pulled from mm/proc.S Linux 3.7 kernel */

#ifndef CONFIG_SMP
/* PTWs cacheable, inner/outer WBWA not shareable */
#define TCR_FLAGS	TCR_IRGN_WBWA | TCR_ORGN_WBWA
#else
/* PTWs cacheable, inner/outer WBWA shareable */
#define TCR_FLAGS	TCR_IRGN_WBWA | TCR_ORGN_WBWA | TCR_SHARED
#endif


#define MAIR(attr, mt)	((attr) << ((mt) * 8))
/*
 *	__cpu_setup
 *
 *	Initialise the processor for turning the MMU on.  Return in x0 the
 *	value of the SCTLR_EL1 register.
 */
ENTRY(__cpu_setup)
	/*
	 * Preserve the link register across the function call.
	 */
	mov	x28, lr
	bl	__flush_dcache_all
	mov	lr, x28
	ic	iallu				// I+BTB cache invalidate
	dsb	sy

	mov	x0, #1
	msr	pmcr_el0, x0			// Enable performance counters
	mov	x0, #1 << 31
	msr 	pmcntenset_el0, x0		// Enable Performance monitor cycle counter

	mov	x0, #3 << 20
	msr	cpacr_el1, x0			// Enable FP/ASIMD
	mov	x0, #1
	msr	oslar_el1, x0			// Set the debug OS lock
	tlbi	vmalle1is			// invalidate I + D TLBs
	/*
	 * Memory region attributes for LPAE:
	 *
	 *   n = AttrIndx[2:0]
	 *			n	MAIR
	 *   DEVICE_nGnRnE	000	00000000
	 *   DEVICE_nGnRE	001	00000100
	 *   DEVICE_GRE		010	00001100
	 *   NORMAL_NC		011	01000100
	 *   NORMAL		100	11111111
	 */
	ldr	x5, =MAIR(0x00, MT_DEVICE_nGnRnE) | \
		     MAIR(0x04, MT_DEVICE_nGnRE) | \
		     MAIR(0x0c, MT_DEVICE_GRE) | \
		     MAIR(0x44, MT_NORMAL_NC) | \
		     MAIR(0xff, MT_NORMAL)
	msr	mair_el1, x5
	/*
	 * Prepare SCTLR
	 */
	adr	x5, crval
	ldp	w5, w6, [x5]
	mrs	x0, sctlr_el1
	bic	x0, x0, x5			// clear bits
	orr	x0, x0, x6			// set bits
	/*
	 * Set/prepare TCR and TTBR. We use 512GB (39-bit) address range for
	 * both user and kernel.
	 */
	ldr	x10, =TCR_TxSZ(VA_BITS) | TCR_FLAGS | TCR_IPS_40BIT | \
		      TCR_ASID16 | (1 << 31)
#ifdef CONFIG_ARM64_64K_PAGES
	orr	x10, x10, TCR_TG0_64K
	orr	x10, x10, TCR_TG1_64K
#endif
	msr	tcr_el1, x10
	ret					// return to head.S
ENDPROC(__cpu_setup)

	/*
	 *                 n n            T
	 *       U E      WT T UD     US IHBS
	 *       CE0      XWHW CZ     ME TEEA S
	 * .... .IEE .... NEAI TE.I ..AD DEN0 ACAM
	 * 0011 0... 1101 ..0. ..0. 10.. .... .... < hardware reserved
	 * .... .100 .... 01.1 11.1 ..01 0001 1101 < software settings
	 */
	.type	crval, #object
crval:
	.word	0x030802e2			// clear
	.word	0x0405d11d			// set

/*
.balign 4096
.space 4096
.space 4096
.space 4096
_init_stack:
*/

	.section .bss.page_aligned, "aw", @nobits
	.balign PAGE_SIZE
ENTRY(empty_zero_page)
	.skip PAGE_SIZE
