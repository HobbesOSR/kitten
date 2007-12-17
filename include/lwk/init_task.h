#ifndef _LWK_INIT_TASK_H
#define _LWK_INIT_TASK_H

/**
 * Initializes architecture-independent fields in the initial address space.
 */
#define INIT_ASPACE(name)

/**
 * Initializes architecture-independent fields in the initial task structure.
 */
#define INIT_TASK(name) \
	.task_id	=	0,					\
	.task_name	=	"idle task",				\
	.cpu		=	0,					\
	.aspace		=	&init_aspace,				\

#define init_task  init_task_union.task_info
#define init_stack init_task_union.stack

#endif
