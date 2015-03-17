/** \file
 * Scheduler control API.
 */
#ifndef _LWK_SCHED_CONTROL_H
#define _LWK_SCHED_CONTROL_H

#include <lwk/task.h>

/*Cooperative scheduling functions*/

extern void sched_yield_task_to(int pid, int tid);
extern int sched_setparams_task(int pid, int tid, int64_t slice, int64_t period);

/*System call wrappers for cooperative scheduling functions*/

extern void sys_sched_yield_task_to(int pid, int tid);
extern void sys_sched_setparams_task(int pid, int tid, int64_t slice, int64_t period);

#endif
