#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <lwk/kernel.h>

#ifdef __KERNEL__

#define __STR(x) #x
#define STR(x) __STR(x)




/*
 * On SMP systems, when the scheduler does migration-cost autodetection,
 * it needs a way to flush as much of the CPU's caches as possible.
 */
static inline void sched_cacheflush(void)
{
	wbinvd();
}

#endif	/* __KERNEL__ */

#define nop()

//__asm__ __volatile__ ("nop")

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */

#define __HAVE_ARCH_CMPXCHG 1



#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#define smp_read_barrier_depends()	do {} while(0)
    



//asm volatile("sfence" ::: "memory")
#define read_barrier_depends()	do {} while(0)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)

#define warn_if_not_ulong(x) do { unsigned long foo; (void) (&(x) == &foo); } while (0)

/* interrupt control.. */


#define local_fiq_enable()	asm("msr	daifclr, #1" : : : "memory")
#define local_fiq_disable()	asm("msr	daifset, #1" : : : "memory")

#define local_async_enable()	asm("msr	daifclr, #4" : : : "memory")
#define local_async_disable()	asm("msr	daifset, #4" : : : "memory")

static inline unsigned long arch_local_save_flags(void)
{
	unsigned long flags;
	asm volatile(
		"mrs	%0, daif		// arch_local_save_flags"
		: "=r" (flags)
		:
		: "memory");
	return flags;
}

#define local_save_flags(flags)				\
	do {						\
		flags = arch_local_save_flags();	\
	} while (0)


static inline void local_irq_restore(unsigned long flags)
{
	asm volatile(
		"msr	daif, %0		// arch_local_irq_restore"
	:
	: "r" (flags)
	: "memory");
}

static inline void local_irq_disable(void)
{
	asm volatile(
		"msr	daifset, #2		// arch_local_irq_disable"
		:
		:
		: "memory");
}

static inline void local_irq_enable(void)
{
	asm volatile(
		"msr	daifclr, #2		// arch_local_irq_enable"
		:
		:
		: "memory");
}

#define irqs_disabled()			\
({					\
	unsigned long flags;		\
	local_save_flags(flags);	\
	!(flags & (1<<9));		\
})

#define irqs_enabled() !irqs_disabled()

/* For spinlocks etc */
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;
	asm volatile(
		"mrs	%0, daif		// arch_local_irq_save\n"
		"msr	daifset, #2"
		: "=r" (flags)
		:
		: "memory");
	return flags;
}

#define local_irq_save(flags)				\
	do {						\
		flags = arch_local_irq_save();		\
	} while (0)

/* used in the idle loop; sti takes one instruction cycle to complete */
 #define safe_halt()						\
__asm__ __volatile__("msr daifclr, #2; wfi": : :"memory")


#include <arch/intc.h>

/* used when interrupts are already enabled or to shutdown the processor */
#define halt() 							\
	do {							\
		printk("Halting\n");				\
		__asm__ __volatile__ ("wfi" : : : "memory");	\
	} while (0)


void cpu_idle_wait(void);

extern unsigned long arch_align_stack(unsigned long sp);
extern void free_init_pages(char *what, unsigned long begin, unsigned long end);

#endif
