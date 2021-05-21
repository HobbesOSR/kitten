#ifndef _ARM64_PDA_H
#define _ARM64_PDA_H

#ifndef __ASSEMBLY__
#include <lwk/stddef.h>
#include <lwk/types.h>
#include <lwk/cache.h>
//#include <lwk/task.h>
#include <arch/page.h>

static inline void set_tpidr_el1(u64 val)
{
	__asm__ __volatile__("msr tpidr_el1, %0\n"::"r"(val));
	return ;
}

static inline u64 get_tpidr_el1()
{
	u64 tpidr;
	__asm__ __volatile__("mrs %0, tpidr_el1\n":"=r"(tpidr));
	return tpidr;
}


/* Per processor datastructure. Each core stores its own version in the tpidr_el1 register. */ 
struct ARM64_pda {
	struct task_struct * pcurrent;	/* Current process */
	unsigned long data_offset;	/* Per cpu data offset from linker address */
	unsigned long kernelstack;  /* top of kernel stack for current */ 
	unsigned long oldsp; 	    /* user rsp for system call */
#if DEBUG_STKSZ > EXCEPTION_STKSZ
	unsigned long debugstack;   /* #DB/#BP stack. */
#endif
        int irqcount;		    /* Irq nesting counter. Starts with -1 */  	
	int cpunumber;		    /* Logical CPU number */
	char *irqstackptr;	/* top of irqstack */
	int nodenumber;		    /* number of current node */
	unsigned int __softirq_pending;
	unsigned int __nmi_count;	/* number of NMI on this CPUs */
	int mmu_state;     
	struct aspace *active_aspace;
	unsigned int timer_reload_value;
	unsigned apic_timer_irqs;
}; // ____cacheline_aligned_in_smp;

extern struct ARM64_pda *_cpu_pda[];
extern struct ARM64_pda boot_cpu_pda[];

void pda_init(unsigned int cpu, struct task_struct *task);

#define cpu_pda(i) (_cpu_pda[i])



/* 
 * There is no fast way to get the base address of the PDA, all the accesses
 * have to mention %fs/%gs.  So it needs to be done this Torvaldian way.
 */ 
#define sizeof_field(type,field)  (sizeof(((type *)0)->field))
#define typeof_field(type,field)  typeof(((type *)0)->field)

extern void __bad_pda_field(void);

#define pda_offset(field) offsetof(struct ARM64_pda, field)


#define pda_to_op(op,field,val)															\
do {																		\
	typedef typeof_field(struct ARM64_pda, field) T__;											\
	u64 tpidr_el1 = get_tpidr_el1();													\
	switch (sizeof_field(struct ARM64_pda, field)) {											\
		case 1:																\
			asm volatile(op "b %w0, [%1, %2]" :: "ri" ((T__)val), "r"(tpidr_el1),  "i"(pda_offset(field)) : "memory"); break; 	\
		case 2:																\
			asm volatile(op "h %w0, [%1, %2]" :: "ri" ((T__)val), "r"(tpidr_el1),  "i"(pda_offset(field)) : "memory"); break; 	\
		case 4:																\
			asm volatile(op "  %w0, [%1, %2]" :: "ri" ((T__)val), "r"(tpidr_el1),  "i"(pda_offset(field)) : "memory"); break; 	\
		case 8:																\
			asm volatile(op "  %0,  [%1, %2]" :: "ri" ((T__)val), "r"(tpidr_el1),  "i"(pda_offset(field)) : "memory"); break; 	\
		default: __bad_pda_field();													\
       }																	\
} while (0)


#define pda_from_op(op,field) ({														\
        typeof_field(struct ARM64_pda, field) ret__; 												\
	u64 tpidr_el1 = get_tpidr_el1();      													\
        switch (sizeof_field(struct ARM64_pda, field)) { 											\
		case 1: 															\
			asm volatile(op "b %w0, [%1, %2]" : "=r" (ret__) : "r"(tpidr_el1), "i"(pda_offset(field)):"memory"); break;		\
		case 2: 															\
			asm volatile(op "h %w0, [%1, %2]" : "=r" (ret__) : "r"(tpidr_el1), "i"(pda_offset(field)):"memory"); break;		\
		case 4: 															\
			asm volatile(op "  %w0, [%1, %2]" : "=r" (ret__) : "r"(tpidr_el1), "i"(pda_offset(field)):"memory"); break;		\
		case 8: 															\
			asm volatile(op "  %0,  [%1, %2]" : "=r" (ret__) : "r"(tpidr_el1), "i"(pda_offset(field)):"memory"); break;		\
		default: __bad_pda_field(); 													\
       } 																	\
       ret__; })


#define read_pda(field)      pda_from_op("ldr",field)
#define write_pda(field,val) pda_to_op("str",field,val)
#define add_pda(field,val)   pda_to_op("add",field,val)
#define sub_pda(field,val)   pda_to_op("sub",field,val)
#define or_pda(field,val)    pda_to_op("or",field,val)



#define early_write_pda(cpu, field, val) do { 													\
	typedef typeof_field(struct ARM64_pda, field) T__;											\
	switch (sizeof_field(struct ARM64_pda, field)) {											\
		case 1:																\
			asm volatile("strb %w0, [%1, %2]" :: "ri" ((T__)val), "r"(cpu_pda(cpu)),  "i"(pda_offset(field)) : "memory"); break; 	\
		case 2:																\
			asm volatile("strh %w0, [%1, %2]" :: "ri" ((T__)val), "r"(cpu_pda(cpu)),  "i"(pda_offset(field)) : "memory"); break; 	\
		case 4:																\
			asm volatile("str  %w0, [%1, %2]" :: "ri" ((T__)val), "r"(cpu_pda(cpu)),  "i"(pda_offset(field)) : "memory"); break; 	\
		case 8:																\
			asm volatile("str  %0,  [%1, %2]" :: "ri" ((T__)val), "r"(cpu_pda(cpu)),  "i"(pda_offset(field)) : "memory"); break; 	\
		default: __bad_pda_field();													\
       }																	\
} while (0);


#define early_read_pda(cpu, field, val) do { 													\
	typedef typeof_field(struct ARM64_pda, field) T__;											\
	switch (sizeof_field(struct ARM64_pda, field)) {											\
		case 1: 															\
			asm volatile("ldrb %w0, [%1, %2]" : "=r" (ret__) : "r"(cpu_pda(cpu)), "i"(pda_offset(field)):"memory"); break;		\
		case 2: 															\
			asm volatile("ldrh %w0, [%1, %2]" : "=r" (ret__) : "r"(cpu_pda(cpu)), "i"(pda_offset(field)):"memory"); break;		\
		case 4: 															\
			asm volatile("ldr  %w0, [%1, %2]" : "=r" (ret__) : "r"(cpu_pda(cpu)), "i"(pda_offset(field)):"memory"); break;		\
		case 8: 															\
			asm volatile("ldr  %0,  [%1, %2]" : "=r" (ret__) : "r"(cpu_pda(cpu)), "i"(pda_offset(field)):"memory"); break;		\
		default: __bad_pda_field(); 													\
       }																	\
} while (0);


#endif

#define PDA_STACKOFFSET (5*8)

#endif
