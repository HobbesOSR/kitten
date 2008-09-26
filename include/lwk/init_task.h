#ifndef _LWK_INIT_TASK_H
#define _LWK_INIT_TASK_H

/**
 * Initializes architecture-independent fields in the initial address space.
 */
#define BOOTSTRAP_ASPACE(name)

/**
 * Initializes architecture-independent fields in the initial task structure.
 */
#define BOOTSTRAP_TASK(task_info) \
	.id		=	0,					\
	.name		=	"bootstrap",				\
	.cpu_id		=	0,					\
	.aspace		=	&bootstrap_aspace,			\
	.sched_link	=	LIST_HEAD_INIT(task_info.sched_link),	\

#define bootstrap_task  bootstrap_task_union.task_info
#define bootstrap_stack bootstrap_task_union.stack

#endif
