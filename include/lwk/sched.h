/** \file
 * Process scheduler API.
 *
 * Kitten maintains a linked list of tasks per CPU that are in a
 * ready to run state.  Since the number of tasks is expected to
 * be low, the overhead is very smal.
 */
#ifndef _LWK_SCHED_H
#define _LWK_SCHED_H

#include <lwk/task.h>
#include <lwk/init.h>

extern int __init sched_subsys_init(void);
extern void sched_add_task(struct task_struct *task);
extern void sched_del_task(struct task_struct *task);
extern int sched_wakeup_task(struct task_struct *task,
                             taskstate_t valid_states);
extern void schedule(void); 

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
extern void arch_idle_task_loop_body(void);

#endif
