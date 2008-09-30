#ifndef _LWK_SCHED_H
#define _LWK_SCHED_H

#include <lwk/task.h>

int sched_init(void);
void sched_add_task(struct task_struct *task);
void sched_del_task(struct task_struct *task);
void schedule(void); 

extern struct task_struct *arch_context_switch(struct task_struct *prev,
                                               struct task_struct *next);
extern void arch_idle_task_loop_body(void);

#endif
