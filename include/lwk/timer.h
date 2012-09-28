/** \file
 * One-shot timers.
 *
 * Kitten implements one shot timers via an ordered queue of
 * struct timer entities.
 */
#ifndef _LWK_TIMER_H
#define _LWK_TIMER_H

#include <lwk/idspace.h>
#include <lwk/list.h>

#define TIMER_INITIALIZER(_name, _function, _expires, _data) {	\
		.link = LIST_HEAD_INIT((_name).link),		\
		.function = (_function),			\
		.expires = (_expires),				\
		.data = (_data),				\
	}

/**
 * This structure defines a timer, including when the timer should expire
 * and the callback function to call when it expires. The callback function
 * runs in interrupt context with interrupts disabled.  The list will be
 * unlocked during the function so that it is safe to call timer_add()
 * to replace the timer.
 *
 * \note The timer_add() function will initialize the link and cpu fields.
 */
struct timer {
	struct list_head link;           /**< Ordered list of callbacks */
	id_t             cpu;            /**< CPU this timer is installed on */
	uint64_t         expires;        /**< Time when this timer expires */
	uintptr_t        data;           /**< arg to pass to function */
	void (*function)(uintptr_t);     /**< executed when timer expires */
};

/** \name Core timer API.
 * @{
 */

/** Add a timer to the ordered list of timer events.
 *
 * \note timer should not be stack allocated unless the timer
 * structure will remain in scope until expiration of the timer.
 */
extern void
timer_add(struct timer *timer);

/** 
 * Same as timer_add but on a CPU
 */
extern void
timer_add_on( struct timer *timer, int cpu );

/** Cancel a pending timer.
 *
 * \returns 1 if the timer was still pending, 0 if it fired.
 *
 * \note If the timer has already been run, this is a nop.
 */
extern int
timer_del(struct timer *timer);

/** Sleep the current thread for a while.
 *
 * The caller must set current->state to TASK_INTERRUPTIBLE or
 * TASK_UNINTERUPTIBLE before calling timer_sleep_until(),
 * otherwise it will return immediately.
 *
 * \returns Time remaining unslept if there is any.
 */
extern uint64_t
timer_sleep_until(
	uint64_t		when
);

static inline int timer_pending(const struct timer * timer)
{
    return !list_empty(&timer->link);
}

// @}

/** \name Internal timer-subsystem functions.
 * Normal kernel code and drivers should not call these.
 * @{
 */
int timer_subsys_init(void);
void expire_timers(void);

/** \note unused? */
void schedule_next_wakeup(void);
// @}

/**
 * Architecture-dependent timer functions.
 *
 * \note The architecture port must implement this function.
 */
void arch_schedule_next_wakeup(uint64_t when);

#endif
