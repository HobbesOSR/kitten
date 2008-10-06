#ifndef _LWK_SCHED_H
#define _LWK_SCHED_H

#include <lwk/task.h>
#include <lwk/init.h>

extern int __init sched_subsys_init(void);
extern void sched_add_task(struct task_struct *task);
extern void sched_del_task(struct task_struct *task);
extern int sched_wakeup_task(struct task_struct *task,
                             taskstate_t valid_states, id_t *cpu_id);
extern void schedule(void); 
extern void reschedule_cpus(cpumask_t cpumask);

extern struct task_struct *arch_context_switch(struct task_struct *prev,
                                               struct task_struct *next);
extern void arch_idle_task_loop_body(void);

#endif
