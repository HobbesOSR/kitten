#ifndef _ASM_GDB_H_
#define _ASM_GDB_H_

/*
 * Copyright (C) 2001-2004 Amit S. Kale
 * Copyright (C) 2008 Wind River Systems, Inc.
 */

enum gdb_regnum{
    GDB_RAX_REGNUM,     /* %rax */
    GDB_RBX_REGNUM,     /* %rbx */
    GDB_RCX_REGNUM,     /* %rcx */
    GDB_RDX_REGNUM,     /* %rdx */
    GDB_RSI_REGNUM,     /* %rsi */
    GDB_RDI_REGNUM,     /* %rdi */
    GDB_RBP_REGNUM,     /* %rbp */
    GDB_RSP_REGNUM,     /* %rsp */
    
    GDB_R8_REGNUM,      /* %r8 */
    GDB_R9_REGNUM,      /* %r9 */
    GDB_R10_REGNUM,     /* %r10 */
    GDB_R11_REGNUM,     /* %r11 */
    GDB_R12_REGNUM,     /* %r12 */
    GDB_R13_REGNUM,     /* %r13 */
    GDB_R14_REGNUM,     /* %r14 */
    GDB_R15_REGNUM,     /* %r15 */
    
    GDB_RIP_REGNUM,     /* %rip */
    
    //32bits
    GDB_EFLAGS_REGNUM,      /* %eflags */
    GDB_CS_REGNUM,      /* %cs */
    GDB_SS_REGNUM,      /* %ss */
    GDB_DS_REGNUM,      /* %ds */
    GDB_ES_REGNUM,      /* %es */
    GDB_FS_REGNUM,      /* %fs */
    GDB_GS_REGNUM,      /* %gs */
    
    //TODO Register ST0~ST7 occupy 10bytes in total
    GDB_ST0_REGNUM     =   24,
    /*GDB_ST1_REGNUM,
    GDB_ST2_REGNUM,
    GDB_ST3_REGNUM,
    GDB_ST4_REGNUM,
    GDB_ST5_REGNUM,
    GDB_ST6_REGNUM,
    GDB_ST7_REGNUM,
    */

    //32bit float point registers
    GDB_FCTRL_REGNUM   =   GDB_ST0_REGNUM + 10,
    GDB_FSTAT_REGNUM,
    GDB_FTAG_REGNUM,
    GDB_FISEG_ERGNUM,
    GDB_FIOFF_ERGNUM,
    GDB_FOSEG_ERGNUM,
    GDB_FOOFF_ERGNUM,
    GDB_FOP_ERGNUM 
};

#define GDB_NUM_GPREGS         (GDB_GS_REGNUM + 1)
#define GDB_NUM_STREGS          8
#define GDB_NUM_FPREGS          8 
#define GDB_NUM_GPREGBYTES		(GDB_NUM_GPREGS * 8)
#define GDB_NUM_STREGBYTES      (GDB_NUM_STREGS * 10)
#define GDB_NUM_FPREGBYTES      (GDB_NUM_FPREGS * 8)
#define GDB_NUM_REGBYTES        (GDB_NUM_GPREGBYTES + GDB_NUM_STREGBYTES + GDB_NUM_FPREGBYTES)

static inline void arch_gdb_breakpoint(void)
{
	asm("   int $3");
}
int gdb_exception_enter(int trapnr, int err, struct pt_regs *regs);
//void gdb_nmi_cpus(void);

#define BREAK_INSTR_SIZE	1
#define CACHE_FLUSH_IS_SAFE	1

#endif
