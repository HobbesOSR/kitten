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

#endif
