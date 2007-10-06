#ifndef _LWK_INIT_TASK_H
#define _LWK_INIT_TASK_H

/**
 * Initializes architecture-independent fields in the initial task structure.
 */
#define INIT_MM(name)

/**
 * Initializes architecture-independent fields in the initial task structure.
 */
#define INIT_TASK(name) \
	.pid		=	0,					\
	.tid		=	0,					\
	.uid		=	0,					\
	.cpu		=	0,					\
	.mm		=	&init_mm,				\

#endif
