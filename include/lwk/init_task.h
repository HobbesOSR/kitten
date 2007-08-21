#ifndef _LWK_INIT_TASK_H
#define _LWK_INIT_TASK_H

/*
 * Macro that initializes platform-independent fields in the
 * initial task structure.  All architectures should be able to use
 * this macro.
 */
#define INIT_TASK(tsk) \
	.pid		=	0,					\
	.tid		=	0,					\
	.uid		=	0,					\
	.cpu		=	0,					\

#endif
