#ifndef _LWK_KTHERAD_H
#define _LWK_KTHERAD_H

#include <lwk/task.h>

struct task_struct *
kthread_create(
	int		(*entry_point)(void *arg),
	void *		arg,
	const char *	fmt,
	...
);

struct task_struct *
kthread_create_on_cpu(
	id_t		cpu_id, 
	int		(*entry_point)(void *arg),
	void *		arg,
	const char *	fmt,
	...
);

/**
 * kthread_run - create and wake a thread.
 * @threadfn: the function to run until signal_pending(current).
 * @data: data ptr for @threadfn.
 * @namefmt: printf-style name for the thread.
 *
 * Description: Convenient wrapper for kthread_create() followed by
 * sched_wakeup_task().  Returns the kthread or ERR_PTR(-ENOMEM).
 */
#define kthread_run(threadfn, data, namefmt, ...)			\
	({								\
	        struct task_struct *__k					\
	                = kthread_create(threadfn, data, namefmt, ## __VA_ARGS__); \
	        if (__k != NULL)					\
			sched_wakeup_task(__k, TASK_STOPPED);		\
	        __k;							\
	})

int kthread_bind( struct task_struct* tsk, int cpu );
int kthread_should_stop(void);

#endif
