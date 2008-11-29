#ifndef _LWK_XCALL_H
#define _LWK_XCALL_H

#include <lwk/cpumask.h>
#include <lwk/idspace.h>
#include <arch/xcall.h>

int
xcall_function(
	cpumask_t	cpu_mask,
	void		(*func)(void *info),
	void *		info,
	bool		wait
);

int
arch_xcall_function(
	cpumask_t	cpu_mask,
	void		(*func)(void *info),
	void *		info,
	bool		wait
);

void
xcall_reschedule(
	lwk_id_t		cpu
);

void
arch_xcall_reschedule(
	lwk_id_t		cpu
);

#endif
