#ifndef _LWK_TIMER_H
#define _LWK_TIMER_H

#include <lwk/idspace.h>
#include <lwk/list.h>

/**
 * This structure defines a timer, including when the timer should expire
 * and the callback function to call when it expires. The callback function
 * runs in interrupt context with interrupts disabled.
 */
struct timer {
	struct list_head link;
	id_t             cpu;            /* CPU this timer is installed on */
	uint64_t         expires;        /* Time when this timer expires */
	uintptr_t        data;           /* arg to pass to function */
	void (*function)(uintptr_t);     /* executed when timer expires */
};

/**
 * Core timer API.
 */
void timer_add(struct timer *timer);
void timer_del(struct timer *timer);
uint64_t timer_sleep_until(uint64_t when);

/**
 * Internal timer-subsystem functions.
 * Normal kernel code and drivers should not call these.
 */
int timer_subsys_init(void);
void expire_timers(void);
void schedule_next_wakeup(void);

/**
 * Architecture-dependent timer functions.
 */
void arch_schedule_next_wakeup(uint64_t when);

#endif
