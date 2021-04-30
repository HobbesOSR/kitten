/*
 * include/asm-x86_64/processor.h
 *
 * Copyright (C) 1994 Linus Torvalds
 */

#ifndef _ARM_64_PROCESSOR_H
#define _ARM_64_PROCESSOR_H

#include <arch/segment.h>
#include <arch/page.h>
#include <arch/types.h>
#include <arch/sigcontext.h>
#include <arch/cpufeature.h>
/* #include <linux/threads.h> */
#include <arch/msr.h>
#include <arch/current.h>
#include <arch/system.h>
/* #include <arch/mmsegment.h> */
#include <arch/percpu.h>
/* #include <lwk/personality.h> */
#include <lwk/cpumask.h>
#include <lwk/cache.h>


typedef struct {
	uint64_t
		t0sz    :6,
		res0_0	:1,
		epd0	:1,
		irgn0	:2,
		orgn0	:2,
		sh0		:2,
		tg0		:2,
		t1sz	:6,
		a1		:1,
		epd1	:1,
		irgn1	:2,
		orgn1	:2,
		sh1		:2,
		tg1		:2,
		ips		:3,
		res0_1	:1,
		as		:1,
		tbi0	:1,
		tbi1	:1,
		resrved0:25;
} tcr_el1;

static inline tcr_el1 get_tcr_el1()
{
	tcr_el1 tcr;
	__asm__ __volatile__("mrs %0, tcr_el1\n":"=r"(tcr));
	return tcr;
}

static inline unsigned long get_ttbr1_el1()
{
	unsigned long tmp;
	__asm__ __volatile__("mrs %0, ttbr1_el1\n":"=r"(tmp));
	return tmp;
}

static inline unsigned long get_ttbr0_el1()
{
	unsigned long tmp;
	__asm__ __volatile__("mrs %0, ttbr0_el1\n":"=r"(tmp));
	return tmp;
}

struct cpu_context {
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;
    unsigned long sp;
    unsigned long pc;
    unsigned long sp0;
    unsigned long usersp;
};

struct thread_struct {
    struct cpu_context  cpu_context;    /* cpu context */
    unsigned long       tp_value;
   // struct fpsimd_state fpsimd_state;
    unsigned long       fault_address;  /* fault info */
    //struct debug_info   debug;      /* debugging */
};

#define TF_MASK		0x00000100
#define IF_MASK		0x00000200
#define IOPL_MASK	0x00003000
#define NT_MASK		0x00004000
#define VM_MASK		0x00020000
#define AC_MASK		0x00040000
#define VIF_MASK	0x00080000	/* virtual interrupt flag */
#define VIP_MASK	0x00100000	/* virtual interrupt pending */
#define ID_MASK		0x00200000

#define desc_empty(desc) \
               (!((desc)->a | (desc)->b))

#define desc_equal(desc1, desc2) \
               (((desc1)->a == (desc2)->a) && ((desc1)->b == (desc2)->b))

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr()

//({ void *pc; asm volatile("leaq 1f(%%rip),%0\n1:":"=r"(pc)); pc; })

#define X86_VENDOR_INTEL 0
#define X86_VENDOR_CYRIX 1
#define X86_VENDOR_AMD 2
#define X86_VENDOR_UMC 3
#define X86_VENDOR_NEXGEN 4
#define X86_VENDOR_CENTAUR 5
#define X86_VENDOR_RISE 6
#define X86_VENDOR_TRANSMETA 7
#define X86_VENDOR_NUM 8
#define X86_VENDOR_UNKNOWN 0xff

extern char ignore_irq13;

extern void identify_cpu(void);

/*
 * EFLAGS bits
 */
#define X86_EFLAGS_CF	0x00000001 /* Carry Flag */
#define X86_EFLAGS_PF	0x00000004 /* Parity Flag */
#define X86_EFLAGS_AF	0x00000010 /* Auxillary carry Flag */
#define X86_EFLAGS_ZF	0x00000040 /* Zero Flag */
#define X86_EFLAGS_SF	0x00000080 /* Sign Flag */
#define X86_EFLAGS_TF	0x00000100 /* Trap Flag */
#define X86_EFLAGS_IF	0x00000200 /* Interrupt Flag */
#define X86_EFLAGS_DF	0x00000400 /* Direction Flag */
#define X86_EFLAGS_OF	0x00000800 /* Overflow Flag */
#define X86_EFLAGS_IOPL	0x00003000 /* IOPL mask */
#define X86_EFLAGS_NT	0x00004000 /* Nested Task */
#define X86_EFLAGS_RF	0x00010000 /* Resume Flag */
#define X86_EFLAGS_VM	0x00020000 /* Virtual Mode */
#define X86_EFLAGS_AC	0x00040000 /* Alignment Check */
#define X86_EFLAGS_VIF	0x00080000 /* Virtual Interrupt Flag */
#define X86_EFLAGS_VIP	0x00100000 /* Virtual Interrupt Pending */
#define X86_EFLAGS_ID	0x00200000 /* CPUID detection flag */




#define BOOTSTRAP_THREAD  { \
	.rsp0 = (unsigned long)&bootstrap_stack + sizeof(bootstrap_stack) \
}

#define BOOTSTRAP_TSS  { \
	.rsp0 = (unsigned long)&bootstrap_stack + sizeof(bootstrap_stack) \
}

#define INIT_MMAP \
{ &init_mm, 0, 0, NULL, PAGE_SHARED, VM_READ | VM_WRITE | VM_EXEC, 1, NULL, NULL }

#define get_debugreg(var, register)

#if 0
\
		__asm__("movq %%db" #register ", %0"		\
			:"=r" (var))
#endif

#define set_debugreg(value, register)

#if 0
			 \
		__asm__("movq %0,%%db" #register		\
			: /* no output */			\
			:"r" (value))
#endif
struct mm_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);

/* Prepare to copy thread state - unlazy all lazy status */
extern void prepare_to_copy(struct task_struct *tsk);

/*
 * create a kernel thread without removing it from tasklists
 */
extern long kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/*
 * Return saved PC of a blocked thread.
 * What is this good for? it will be always the scheduler or ret_from_fork.
 */
#define thread_saved_pc(t) (*(unsigned long *)((t)->thread.rsp - 8))

extern unsigned long get_wchan(struct task_struct *p);
#define task_pt_regs(tsk) ((struct pt_regs *)(tsk)->thread.rsp0 - 1)
#define KSTK_EIP(tsk) (task_pt_regs(tsk)->rip)
#define KSTK_ESP(tsk) -1 /* sorry. doesn't work for syscall. */


struct microcode_header {
	unsigned int hdrver;
	unsigned int rev;
	unsigned int date;
	unsigned int sig;
	unsigned int cksum;
	unsigned int ldrver;
	unsigned int pf;
	unsigned int datasize;
	unsigned int totalsize;
	unsigned int reserved[3];
};

struct microcode {
	struct microcode_header hdr;
	unsigned int bits[0];
};

typedef struct microcode microcode_t;
typedef struct microcode_header microcode_header_t;

/* microcode format is extended from prescott processors */
struct extended_signature {
	unsigned int sig;
	unsigned int pf;
	unsigned int cksum;
};

struct extended_sigtable {
	unsigned int count;
	unsigned int cksum;
	unsigned int reserved[3];
	struct extended_signature sigs[0];
};


#define ASM_NOP_MAX 8

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static inline void rep_nop(void)
{
	//__asm__ __volatile__("rep;nop": : :"memory");
}

/* Stop speculative execution */
static inline void sync_core(void)
{ 
	int tmp;
	//asm volatile("cpuid" : "=a" (tmp) : "0" (1) : "ebx","ecx","edx","memory");
} 

#define cpu_has_fpu 1

#define ARCH_HAS_PREFETCH
static inline void prefetch(void *x) 
{ 
	//asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x));
} 

#define ARCH_HAS_PREFETCHW 1
static inline void prefetchw(void *x) 
{ 
	//asm volatile("prefetchtw %0" :: "m" (*(unsigned long *)x));
} 

#define ARCH_HAS_SPINLOCK_PREFETCH 1

#define spin_lock_prefetch(x)  prefetchw(x)

#define cpu_relax()   rep_nop()

static inline void serialize_cpu(void)
{
	//__asm__ __volatile__ ("cpuid" : : : "ax", "bx", "cx", "dx");
}


#define stack_current() \
({								\
	struct thread_info *ti;					\
	/*asm("andq %%rsp,%0; ":"=r" (ti) : "0" (CURRENT_MASK));*/	\
	ti->task;					\
})

#define cache_line_size() (boot_cpu_data.arch.x86_cache_alignment)

extern unsigned long boot_option_idle_override;
/* Boot loader type from the setup header */
extern int bootloader_type;

#define HAVE_ARCH_PICK_MMAP_LAYOUT 1

#endif /* _ARM_64_PROCESSOR_H */
