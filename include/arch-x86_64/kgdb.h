#ifndef _ASM_KGDB_H_
#define _ASM_KGDB_H_

/*
 * Copyright (C) 2001-2004 Amit S. Kale
 * Copyright (C) 2008 Wind River Systems, Inc.
 */

/*
 * BUFMAX defines the maximum number of characters in inbound/outbound
 * buffers at least NUMREGBYTES*2 are needed for register packets
 * Longer buffer is needed to list all threads
 */
#define BUFMAX			1024

/*
 *  Note that this register image is in a different order than
 *  the register image that Linux produces at interrupt time.
 *
 *  Linux's register image is defined by struct pt_regs in ptrace.h.
 *  Just why GDB uses a different order is a historical mystery.
 */
enum regnames {
	GDB_AX,			/* 0 */
	GDB_DX,			/* 1 */
	GDB_CX,			/* 2 */
	GDB_BX,			/* 3 */
	GDB_SI,			/* 4 */
	GDB_DI,			/* 5 */
	GDB_BP,			/* 6 */
	GDB_SP,			/* 7 */
	GDB_R8,			/* 8 */
	GDB_R9,			/* 9 */
	GDB_R10,		/* 10 */
	GDB_R11,		/* 11 */
	GDB_R12,		/* 12 */
	GDB_R13,		/* 13 */
	GDB_R14,		/* 14 */
	GDB_R15,		/* 15 */
	GDB_PC,			/* 16 */
	GDB_PS,			/* 17 */
};

# define NUMREGBYTES		((GDB_PS+1)*8)

static inline void arch_kgdb_breakpoint(void)
{
	asm("   int $3");
}
int kgdb_exception_enter(int trapnr, int err, struct pt_regs *regs);
int kgdb_register_idle_task(struct task_struct *);
int kgdb_register_running_task(struct task_struct *);
void kgdb_nmi_cpus(void);

#define BREAK_INSTR_SIZE	1
#define CACHE_FLUSH_IS_SAFE	1

#endif				/* _ASM_KGDB_H_ */
