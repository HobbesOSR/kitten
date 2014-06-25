#ifndef _LWK_BOOTSTRAP_H
#define _LWK_BOOTSTRAP_H

/**
 * Initializes architecture-independent fields in the bootstrap address space.
 */
#define BOOTSTRAP_ASPACE(aspace) \
	.id		=	0,					\
	.name		=	"bootstrap",				\
	.lock		=	SPIN_LOCK_UNLOCKED,			\
	.child_list	=	LIST_HEAD_INIT(aspace.child_list),	\

/**
 * Initializes architecture-independent fields in the bootstrap task structure.
 */
#ifdef CONFIG_SCHED_EDF
#define BOOTSTRAP_TASK(task_info) \
	.id		=	0,					\
	.name		=	"bootstrap",				\
	.state		=	TASK_RUNNING,				\
	.cpu_id		=	0,					\
	.aspace		=	&bootstrap_aspace,			\
	.edf.period	= 	0,					\
	.migrate_link	 =	LIST_HEAD_INIT(task_info.migrate_link),	\
	.rr.sched_link	=	LIST_HEAD_INIT(task_info.rr.sched_link),\

#else
#define BOOTSTRAP_TASK(task_info) \
	.id		=	0,					\
	.name		=	"bootstrap",				\
	.state		=	TASK_RUNNING,				\
	.cpu_id		=	0,					\
	.aspace		=	&bootstrap_aspace,			\
	.migrate_link	=	LIST_HEAD_INIT(task_info.migrate_link),	\
	.rr.sched_link	=	LIST_HEAD_INIT(task_info.rr.sched_link),\

#endif

#define bootstrap_task  bootstrap_task_union.task_info
#define bootstrap_stack bootstrap_task_union.stack

#endif
