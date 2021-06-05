#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <lwk/kernel.h>
#include <arch/barrier.h>

#ifdef __KERNEL__

#define __STR(x) #x
#define STR(x) __STR(x)





#endif	/* __KERNEL__ */




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
({								\
	unsigned long flags;		\
	local_save_flags(flags);	\
	!!(flags & (0x1 << 7));		\
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

#include <lwk/delay.h>
/* used in the idle loop;  */
 #define safe_halt() 							\
do {									\
	__asm__ __volatile__("msr daifclr, #2; wfi": : :"memory"); 	\
} while (0);



/* used when interrupts are already enabled or to shutdown the processor */
#define halt() 							\
	do {							\
		printk("Halting\n");				\
		__asm__ __volatile__ ("wfi" : : : "memory");	\
	} while (0)


void cpu_idle_wait(void);

extern unsigned long arch_align_stack(unsigned long sp);
extern void free_init_pages(char *what, unsigned long begin, unsigned long end);

#include <lwk/reboot.h>
extern void (*arm_pm_restart)(enum reboot_mode reboot_mode, const char *cmd);

#endif
