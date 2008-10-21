/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

/*
 * Copyright (C) 2004 Amit S. Kale <amitkale@linsyssoft.com>
 * Copyright (C) 2000-2001 VERITAS Software Corporation.
 * Copyright (C) 2002 Andi Kleen, SuSE Labs
 * Copyright (C) 2004 LinSysSoft Technologies Pvt. Ltd.
 * Copyright (C) 2007 MontaVista Software, Inc.
 * Copyright (C) 2007-2008 Jason Wessel, Wind River Systems, Inc.
 * Copyright (C) 2008 Patrick Bridges, University of New Mexico
 */
/****************************************************************************
 *  Contributor:     Lake Stevens Instrument Division$
 *  Written by:      Glenn Engel $
 *  Updated by:	     Amit Kale<akale@veritas.com>
 *  Updated by:	     Tom Rini <trini@kernel.crashing.org>
 *  Updated by:	     Jason Wessel <jason.wessel@windriver.com>
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *  Origianl kgdb, compatibility with 2.1.xx kernel by
 *  David Grothe <dave@gcom.com>
 *  Integrated into 2.2.5 kernel by Tigran Aivazian <tigran@sco.com>
 *  X86_64 changes from Andi Kleen's patch merged by Jim Houston
 *  Ported to Kitten by Patrick Bridges
 */
#include <lwk/spinlock.h>
#include <lwk/string.h>
#include <lwk/kernel.h>
#include <lwk/ptrace.h>
#include <lwk/delay.h>
#include <lwk/kgdb.h>
#include <lwk/init.h>
#include <lwk/smp.h>

#include <arch/apicdef.h>
#include <arch/system.h>

/* #include <asm/mach_apic.h> */

/*
 * Put the error code here just in case the user cares:
 */
static int gdb_x86errcode;

/*
 * Likewise, the vector number here (since GDB only gets the signal
 * number through the usual means, and that's not very specific):
 */
static int gdb_x86vector = -1;

/**
 *	pt_regs_to_gdb_regs - Convert ptrace regs to GDB regs
 *	@gdb_regs: A pointer to hold the registers in the order GDB wants.
 *	@regs: The &struct pt_regs of the current process.
 *
 *	Convert the pt_regs in @regs into the format for registers that
 *	GDB expects, stored in @gdb_regs.
 */
void pt_regs_to_gdb_regs(unsigned long *gdb_regs, struct pt_regs *regs)
{
	gdb_regs[GDB_AX]	= regs->rax;
	gdb_regs[GDB_BX]	= regs->rbx;
	gdb_regs[GDB_CX]	= regs->rcx;
	gdb_regs[GDB_DX]	= regs->rdx;
	gdb_regs[GDB_SI]	= regs->rsi;
	gdb_regs[GDB_DI]	= regs->rdi;
	gdb_regs[GDB_BP]	= regs->rbp;
	gdb_regs[GDB_PS]	= regs->eflags;
	gdb_regs[GDB_PC]	= regs->rip;
	gdb_regs[GDB_R8]	= regs->r8;
	gdb_regs[GDB_R9]	= regs->r9;
	gdb_regs[GDB_R10]	= regs->r10;
	gdb_regs[GDB_R11]	= regs->r11;
	gdb_regs[GDB_R12]	= regs->r12;
	gdb_regs[GDB_R13]	= regs->r13;
	gdb_regs[GDB_R14]	= regs->r14;
	gdb_regs[GDB_R15]	= regs->r15;
	gdb_regs[GDB_SP]	= regs->rsp;
}

/**
 *	sleeping_thread_to_gdb_regs - Convert ptrace regs to GDB regs
 *	@gdb_regs: A pointer to hold the registers in the order GDB wants.
 *	@p: The &struct task_struct of the desired process.
 *
 *	Convert the register values of the sleeping process in @p to
 *	the format that GDB expects.
 *	This function is called when kgdb does not have access to the
 *	&struct pt_regs and therefore it should fill the gdb registers
 *	@gdb_regs with what has	been saved in &struct thread_struct
 *	thread field during switch_to.
 */
void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p)
{
	gdb_regs[GDB_AX]	= 0;
	gdb_regs[GDB_BX]	= 0;
	gdb_regs[GDB_CX]	= 0;
	gdb_regs[GDB_DX]	= 0;
	gdb_regs[GDB_SI]	= 0;
	gdb_regs[GDB_DI]	= 0;
	gdb_regs[GDB_BP]	= *(unsigned long *)p->arch.thread.rsp;
	gdb_regs[GDB_PS]	= *(unsigned long *)(p->arch.thread.rsp + 8);
	gdb_regs[GDB_PC]	= 0;
	gdb_regs[GDB_R8]	= 0;
	gdb_regs[GDB_R9]	= 0;
	gdb_regs[GDB_R10]	= 0;
	gdb_regs[GDB_R11]	= 0;
	gdb_regs[GDB_R12]	= 0;
	gdb_regs[GDB_R13]	= 0;
	gdb_regs[GDB_R14]	= 0;
	gdb_regs[GDB_R15]	= 0;
	gdb_regs[GDB_SP]	= p->arch.thread.rsp;
}

/**
 *	gdb_regs_to_pt_regs - Convert GDB regs to ptrace regs.
 *	@gdb_regs: A pointer to hold the registers we've received from GDB.
 *	@regs: A pointer to a &struct pt_regs to hold these values in.
 *
 *	Convert the GDB regs in @gdb_regs into the pt_regs, and store them
 *	in @regs.
 */
void gdb_regs_to_pt_regs(unsigned long *gdb_regs, struct pt_regs *regs)
{
	regs->rax		= gdb_regs[GDB_AX];
	regs->rbx		= gdb_regs[GDB_BX];
	regs->rcx		= gdb_regs[GDB_CX];
	regs->rdx		= gdb_regs[GDB_DX];
	regs->rsi		= gdb_regs[GDB_SI];
	regs->rdi		= gdb_regs[GDB_DI];
	regs->rbp		= gdb_regs[GDB_BP];
	regs->eflags		= gdb_regs[GDB_PS];
	regs->rip		= gdb_regs[GDB_PC];
	regs->r8		= gdb_regs[GDB_R8];
	regs->r9		= gdb_regs[GDB_R9];
	regs->r10		= gdb_regs[GDB_R10];
	regs->r11		= gdb_regs[GDB_R11];
	regs->r12		= gdb_regs[GDB_R12];
	regs->r13		= gdb_regs[GDB_R13];
	regs->r14		= gdb_regs[GDB_R14];
	regs->r15		= gdb_regs[GDB_R15];
}

/**
 *	kgdb_post_primary_code - Save error vector/code numbers.
 *	@regs: Original pt_regs.
 *	@e_vector: Original error vector.
 *	@err_code: Original error code.
 *
 *	This is needed on architectures which support SMP and KGDB.
 *	This function is called after all the slave cpus have been put
 *	to a know spin state and the primary CPU has control over KGDB.
 */
void kgdb_post_primary_code(struct pt_regs *regs, int e_vector, int err_code)
{
	/* primary processor is completely in the debugger */
	gdb_x86vector = e_vector;
	gdb_x86errcode = err_code;
}

/**
 *	kgdb_arch_handle_exception - Handle architecture specific GDB packets.
 *	@vector: The error vector of the exception that happened.
 *	@signo: The signal number of the exception that happened.
 *	@err_code: The error code of the exception that happened.
 *	@remcom_in_buffer: The buffer of the packet we have read.
 *	@remcom_out_buffer: The buffer of %BUFMAX bytes to write a packet into.
 *	@regs: The &struct pt_regs of the current process.
 *
 *	This function MUST handle the 'c' and 's' command packets,
 *	as well packets to set / remove a hardware breakpoint, if used.
 *	If there are additional packets which the hardware needs to handle,
 *	they are handled here.  The code should return -1 if it wants to
 *	process more packets, and a %0 or %1 if it wants to exit from the
 *	kgdb callback.
 */
int kgdb_arch_handle_exception(int e_vector, int signo, int err_code,
			       char *remcomInBuffer, char *remcomOutBuffer,
			       struct pt_regs *linux_regs)
{
	unsigned long addr;
	char *ptr;
	int newPC;

	switch (remcomInBuffer[0]) {
	case 'c':
	case 's':
		/* try to read optional parameter, pc unchanged if no parm */
		ptr = &remcomInBuffer[1];
		if (kgdb_hex2long(&ptr, &addr))
			linux_regs->rip = addr;
		newPC = linux_regs->rip;

		/* clear the trace bit */
		linux_regs->eflags &= ~TF_MASK;
		atomic_set(&kgdb_cpu_doing_single_step, -1);

		/* set the trace bit if we're stepping */
		if (remcomInBuffer[0] == 's') {
			linux_regs->eflags |= TF_MASK;
			kgdb_single_step = 1;
			if (kgdb_contthread) {
				atomic_set(&kgdb_cpu_doing_single_step,
					   raw_smp_processor_id());
			}
		}

		return 0;
	}

	/* this means that we do not want to exit from the handler: */
	return -1;
}

static inline int
single_step_cont(int trapnr, int signr, int err, struct pt_regs *regs)
{
	/*
	 * Single step exception from kernel space to user space so
	 * eat the exception and continue the process:
	 */
	printk(KERN_ERR "KGDB: trap/step from kernel to user space, "
			"resuming...\n");
	kgdb_arch_handle_exception(trapnr, signr, err, "c", "", regs);

	return 0;
}

/* Return 1 to crash the processor, 0 to continue the processor */
static int 
__kgdb_exception_enter(int trapnr, int signr, int err, struct pt_regs *regs)
{
	if (atomic_read(&kgdb_cpu_doing_single_step) ==
			raw_smp_processor_id() &&
			user_mode(regs))
			return single_step_cont(trapnr, signr, err, regs);
		/* fall through */
	if (user_mode(regs))
		return 1;
	
	if (kgdb_handle_exception(trapnr, signr, err, regs))
		return 1;
	return 0;
}

int
kgdb_exception_enter(int trapnr, int err, struct pt_regs *regs)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	/* TODO: map trapnr to signr just to make things more sane XXX */
	ret = __kgdb_exception_enter(trapnr, 0, err, regs);
	local_irq_restore(flags);

	return ret;
}

/**
 *
 *	kgdb_skipexception - Bail of of KGDB when we've been triggered.
 *	@exception: Exception vector number
 *	@regs: Current &struct pt_regs.
 *
 *	On some architectures we need to skip a breakpoint exception when
 *	it occurs after a breakpoint has been removed.
 *
 * Skip an int3 exception when it occurs after a breakpoint has been
 * removed. Backtrack eip by 1 since the int3 would have caused it to
 * increment by 1.
 */
int kgdb_skipexception(int exception, struct pt_regs *regs)
{
	if (exception == 3 && kgdb_isremovedbreak(regs->rip - 1)) {
		regs->rip -= 1;
		return 1;
	}
	return 0;
}

unsigned long kgdb_arch_pc(int exception, struct pt_regs *regs)
{
	if (exception == 3)
		return instruction_pointer(regs) - 1;
	return instruction_pointer(regs);
}

struct kgdb_arch arch_kgdb_ops = {
	/* Breakpoint instruction: */
	.gdb_bpt_instr		= { 0xcc },
};
