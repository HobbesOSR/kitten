#ifndef __LINUX_INTERRUPT_H
#define __LINUX_INTERRUPT_H

#include <lwk/interrupt.h>
#include <lwk/spinlock.h>
#include <linux/workqueue.h>

#define request_irq	irq_request
#define free_irq	irq_free

/*
 * These flags used only by the kernel as part of the
 * irq handling routines.
 *
 * IRQF_DISABLED - keep irqs disabled when calling the action handler
 * IRQF_SAMPLE_RANDOM - irq is used to feed the random generator
 * IRQF_SHARED - allow sharing the irq among several devices
 * IRQF_PROBE_SHARED - set by callers when they expect sharing mismatches to occur
 * IRQF_TIMER - Flag to mark this interrupt as timer interrupt
 * IRQF_PERCPU - Interrupt is per cpu
 * IRQF_NOBALANCING - Flag to exclude this interrupt from irq balancing
 * IRQF_IRQPOLL - Interrupt is used for polling (only the interrupt that is
 *                registered first in an shared interrupt is considered for
 *                performance reasons)
 */
#define IRQF_DISABLED		0x00000020
#define IRQF_SAMPLE_RANDOM	0x00000040
#define IRQF_SHARED		0x00000080
#define IRQF_PROBE_SHARED	0x00000100
#define IRQF_TIMER		0x00000200
#define IRQF_PERCPU		0x00000400
#define IRQF_NOBALANCING	0x00000800
#define IRQF_IRQPOLL		0x00001000

struct tasklet_struct
{
	struct work_struct work;
	struct list_head list;
	unsigned long state;
	atomic_t count;
	void (*func)(unsigned long);
	unsigned long data;
	char *n;
};

extern struct workqueue_struct *ktaskletd_wq;

#define tasklet_hi_schedule tasklet_schedule
static inline void tasklet_schedule(struct tasklet_struct *t)
{
	WARN_ON_ONCE(!ktaskletd_wq);
	queue_work(ktaskletd_wq, &t->work);
}

extern void tasklet_kill(struct tasklet_struct *t);
extern void tasklet_init(struct tasklet_struct *t,
			 void (*func)(unsigned long), unsigned long data);

extern void local_bh_disable(void);
extern void _local_bh_enable(void);
extern void local_bh_enable(void);
extern void local_bh_enable_ip(unsigned long ip);


#endif
