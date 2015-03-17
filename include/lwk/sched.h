/** \file
 * Process scheduler API.
 *
 * Kitten maintains a linked list of tasks per CPU that are in a
 * ready to run state.  Since the number of tasks is expected to
 * be low, the overhead is very small.
 */
#ifndef _LWK_SCHED_H
#define _LWK_SCHED_H

#include <lwk/init.h>
#include <lwk/task.h>

#include <lwk/sched_rr.h>
#ifdef CONFIG_SCHED_EDF
#include <lwk/sched_edf.h>
#endif

extern unsigned int sched_hz;

extern int __init sched_init_runqueue(int cpu_id);
extern void sched_add_task(struct task_struct *task);
extern void sched_del_task(struct task_struct *task);
extern int sched_wakeup_task(struct task_struct *task,
                             taskstate_t valid_states);
extern void sched_cpu_remove(void *);
extern void schedule(void);

extern struct task_struct *
context_switch(struct task_struct *prev, struct task_struct *next);

#define MAX_SCHEDULE_TIMEOUT TIME_T_MAX
extern ktime_t schedule_timeout(ktime_t timeout);

extern void sched_yield(void);
extern void sched_yield_to(struct task_struct * task);
extern void sched_set_params(struct task_struct * task, ktime_t slice, ktime_t period);

extern void
fire_sched_out_preempt_notifiers(struct task_struct * curr,
				 struct task_struct * next);

extern void
fire_sched_in_preempt_notifiers(struct task_struct * curr);

/** Each architecture must provide its own context-switch code
 * \ingroup arch
 */
extern struct task_struct *
arch_context_switch(
	struct task_struct *prev,
       	struct task_struct *next
);

/** Each architecture must provide its own idle task body
 * \ingroup arch
 */
extern void arch_idle_task_loop_body(int irqenable);

/**
 * set_current_state() includes a barrier so that the write of current->state
 * is correctly serialized wrt the caller's subsequent test of whether to
 * actually sleep:
 *
 *	set_current_state(TASK_UNINTERRUPTIBLE);
 *	if (do_i_need_to_sleep())
 *		schedule();
 *
 * If the caller does not need such serialization then use __set_current_state()
 */
#define __set_current_state(state_value) \
	__set_task_state( current, state_value )
#define set_current_state(state_value) \
	set_task_state( current, state_value )

/**
 * These are similar to set_current_state() and __set_current_state(), except
 * they allow any task's state to be set instead of just current.
 */
#define __set_task_state(tsk, state_value) \
	do { (tsk)->state = (state_value); } while (0)
#define set_task_state(tsk, state_value) \
	set_mb((tsk)->state, (state_value))

#endif
