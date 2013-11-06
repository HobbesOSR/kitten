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
 *  Origianl gdb, compatibility with 2.1.xx kernel by
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
#include <lwk/gdb.h>
#include <lwk/init.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>

#include <arch/apic.h>
#include <arch/apicdef.h>
#include <arch/idt_vectors.h>
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
	gdb_regs[GDB_RAX_REGNUM]	= regs->rax;
	gdb_regs[GDB_RBX_REGNUM]	= regs->rbx;
	gdb_regs[GDB_RCX_REGNUM]	= regs->rcx;
	gdb_regs[GDB_RDX_REGNUM]	= regs->rdx;
	gdb_regs[GDB_RSI_REGNUM]	= regs->rsi;
	gdb_regs[GDB_RDI_REGNUM]	= regs->rdi;
	gdb_regs[GDB_RBP_REGNUM]	= regs->rbp;
	gdb_regs[GDB_RSP_REGNUM]	= regs->rsp;
	
	gdb_regs[GDB_R8_REGNUM]	    = regs->r8;
	gdb_regs[GDB_R9_REGNUM]	    = regs->r9;
	gdb_regs[GDB_R10_REGNUM]	= regs->r10;
	gdb_regs[GDB_R11_REGNUM]	= regs->r11;
	gdb_regs[GDB_R12_REGNUM]	= regs->r12;
	gdb_regs[GDB_R13_REGNUM]	= regs->r13;
	gdb_regs[GDB_R14_REGNUM]	= regs->r14;
	gdb_regs[GDB_R15_REGNUM]	= regs->r15;

	//gdb_regs[GDB_ORIG_RAX_REGNUM]   = regs->orig_rax;

	gdb_regs[GDB_RIP_REGNUM]	= regs->rip;
	gdb_regs[GDB_EFLAGS_REGNUM]	= regs->eflags;
	gdb_regs[GDB_CS_REGNUM]	    = regs->cs;
	gdb_regs[GDB_SS_REGNUM]	    = regs->ss;

	/*//TODO
	struct thread_struct tss = TASK_PTR(regs)->arch.thread;
	gdb_regs[GDB_DS_REGNUM]	    = tss.ds;
	gdb_regs[GDB_ES_REGNUM]	    = tss.es;
	gdb_regs[GDB_FS_REGNUM]	    = tss.fs;
	gdb_regs[GDB_GS_REGNUM]	    = tss.gs;
	*/    
	//TODO ST0~ST7, FCTRL, FSTAT, FTAG, XMM0, XMM1, MXCSR, YMM0H, YMM15H
}

/**
 *	sleeping_thread_to_gdb_regs - Convert ptrace regs to GDB regs
 *	@gdb_regs: A pointer to hold the registers in the order GDB wants.
 *	@p: The &struct task_struct of the desired process.
 *
 *	Convert the register values of the sleeping process in @p to
 *	the format that GDB expects.
 *	This function is called when gdb does not have access to the
 *	&struct pt_regs and therefore it should fill the gdb registers
 *	@gdb_regs with what has	been saved in &struct thread_struct
 *	thread field during switch_to.
 */
/*void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p)
{
	gdb_regs[GDB_RAX_REGNUM]	= 0;
	gdb_regs[GDB_RBX_REGNUM]	= 0;
	gdb_regs[GDB_RCX_REGNUM]	= 0;
	gdb_regs[GDB_RDX_REGNUM]	= 0;
	gdb_regs[GDB_RSI_REGNUM]	= 0;
	gdb_regs[GDB_RDI_REGNUM]	= 0;
	gdb_regs[GDB_RBP_REGNUM]	= *(unsigned long *)p->arch.thread.rsp;
	gdb_regs[GDB_RSP_REGNUM]	= p->arch.thread.rsp;

	gdb_regs[GDB_R8_REGNUM]	= 0;
	gdb_regs[GDB_R9_REGNUM]	= 0;
	gdb_regs[GDB_R10_REGNUM]	= 0;
	gdb_regs[GDB_R11_REGNUM]	= 0;
	gdb_regs[GDB_R12_REGNUM]	= 0;
	gdb_regs[GDB_R13_REGNUM]	= 0;
	gdb_regs[GDB_R14_REGNUM]	= 0;
	gdb_regs[GDB_R15_REGNUM]	= 0;

	gdb_regs[GDB_RIP_REGNUM]	= 0;
	gdb_regs[GDB_EFLAGS_REGNUM]	= *(unsigned long *)(p->arch.thread.rsp + 8);

}
*/
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
	regs->rax		= gdb_regs[GDB_RAX_REGNUM];
	regs->rbx		= gdb_regs[GDB_RBX_REGNUM];
	regs->rcx		= gdb_regs[GDB_RCX_REGNUM];
	regs->rdx		= gdb_regs[GDB_RDX_REGNUM];
	regs->rsi		= gdb_regs[GDB_RSI_REGNUM];
	regs->rdi		= gdb_regs[GDB_RDI_REGNUM];
	regs->rbp		= gdb_regs[GDB_RBP_REGNUM];
	regs->rsp       = gdb_regs[GDB_RSP_REGNUM];

	regs->r8		= gdb_regs[GDB_R8_REGNUM];
	regs->r9		= gdb_regs[GDB_R9_REGNUM];
	regs->r10		= gdb_regs[GDB_R10_REGNUM];
	regs->r11		= gdb_regs[GDB_R11_REGNUM];
	regs->r12		= gdb_regs[GDB_R12_REGNUM];
	regs->r13		= gdb_regs[GDB_R13_REGNUM];
	regs->r14		= gdb_regs[GDB_R14_REGNUM];
	regs->r15		= gdb_regs[GDB_R15_REGNUM];
    
	regs->rip		= gdb_regs[GDB_RIP_REGNUM];
	regs->cs        = gdb_regs[GDB_CS_REGNUM];
	regs->ss        = gdb_regs[GDB_SS_REGNUM];
	regs->eflags    = gdb_regs[GDB_EFLAGS_REGNUM];

	//regs->orig_rax  = gdb_regs[GDB_ORIG_RAX_REGNUM];
}

/**
 *	gdb_post_primary_code - Save error vector/code numbers.
 *	@regs: Original pt_regs.
 *	@e_vector: Original error vector.
 *	@err_code: Original error code.
 *
 *	This is needed on architectures which support SMP and GDB.
 *	This function is called after all the slave cpus have been put
 *	to a know spin state and the primary CPU has control over GDB.
 */
void gdb_post_primary_code(struct pt_regs *regs, int e_vector, int err_code)
{
	/* primary processor is completely in the debugger */
	gdb_x86vector = e_vector;
	gdb_x86errcode = err_code;
}

/**
 *	gdb_arch_handle_exception - Handle architecture specific GDB packets.
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
 *	gdb callback.
 */
int gdb_arch_handle_exception(int e_vector, int signo, int err_code,
			       struct pt_regs *linux_regs,
                   char *remcom_in_buffer, char *remcom_out_buffer,
                   atomic_t *gdb_cpu_doing_single_step)
{
	unsigned long addr;
	char *ptr;
    
	switch (remcom_in_buffer[0]) {
	case 'c':
	case 's':
		/* try to read optional parameter, pc unchanged if no parm */
		ptr = &remcom_in_buffer[1];
		if (gdb_hex2long(&ptr, &addr))
			linux_regs->rip = addr;

		/* clear the trace bit */
		linux_regs->eflags &= ~TF_MASK;
		atomic_set(gdb_cpu_doing_single_step, -1);
		
		if (remcom_in_buffer[0] == 's') {
			linux_regs->eflags |= TF_MASK;
//			gdb_single_step = 1;
			atomic_set(gdb_cpu_doing_single_step, raw_smp_processor_id());
		}
		return 0;
	}

	/* this means that we do not want to exit from the handler: */
	return -1;
}

//static inline int
//single_step_cont(int trapnr, int signr, int err, struct pt_regs *regs)
//{
//	/*
//	 * Single step exception from kernel space to user space so
//	 * eat the exception and continue the process:
//	 */
//	printk(KERN_ERR "GDB: trap/step from kernel to user space, "
//			"resuming...\n");
//	gdb_arch_handle_exception(trapnr, signr, err, "c", "", regs);
//
//	return 0;
//}

/* Return 1 to crash the processor, 0 to continue the processor */
static int 
__gdb_exception_enter(int trapnr, int signr, int err, struct pt_regs *regs)
{
	/*if (atomic_read(&gdb_cpu_doing_single_step) == raw_smp_processor_id())
			return single_step_cont(trapnr, signr, err, regs);
	*/
	if (gdb_handle_exception(trapnr, signr, err, regs))
		return 1;
	return 0;
}

int
gdb_exception_enter(int trapnr, int err, struct pt_regs *regs)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	/* TODO: map trapnr to signr just to make things more sane XXX */
	ret = __gdb_exception_enter(trapnr, 0x05, err, regs);
	local_irq_restore(flags);

	return ret;
}

void
gdb_nmi_cpus(void)
{
	int fromcpu = raw_smp_processor_id();
	int i;
	for (i = 0; i < num_cpus(); i++) 
		if (i != fromcpu)
			lapic_send_ipi(i, NMI_VECTOR);
	return;
}

/**
 *
 *	gdb_skipexception - Bail of of GDB when we've been triggered.
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
int gdb_skipexception(struct gdb_bkpt *gdb_breaks, int aspace_id, int exception, struct pt_regs *regs)
{
	if (exception == 3 && gdb_isremovedbreak(gdb_breaks, aspace_id, regs->rip - 1)) {
		regs->rip -= 1;
		return 1;
	}
	return 0;
}

unsigned long gdb_arch_pc(int exception, struct pt_regs *regs)
{
	if (exception == 3)
		return instruction_pointer(regs) - 1;
	return instruction_pointer(regs);
}

struct gdb_arch arch_gdb_ops = {
	/* Breakpoint instruction: */
	.gdb_bpt_instr		= { 0xcc },
};
