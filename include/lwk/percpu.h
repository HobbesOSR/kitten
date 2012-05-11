#ifndef _LWK_PERCPU_H
#define _LWK_PERCPU_H
#include <lwk/cpu.h>
#include <lwk/string.h>
#include <lwk/cpumask.h>
#include <arch/percpu.h>

/* Enough to cover all DEFINE_PER_CPU()'s in kernel. */
#ifndef PERCPU_ENOUGH_ROOM
#define PERCPU_ENOUGH_ROOM 32768
#endif

/* Must be an lvalue. */
#define get_cpu_var(var) __get_cpu_var(var)
#define put_cpu_var(var) 

struct percpu_data {
	void *ptrs[NR_CPUS];
};

#define __percpu_disguise(pdata) (struct percpu_data *)~(unsigned long)(pdata)

/* 
 * Use this to get to a cpu's version of the per-cpu object allocated using
 * alloc_percpu.  Non-atomic access to the current CPU's version should
 * probably be combined with get_cpu()/put_cpu().
 */ 
#define percpu_ptr(ptr, cpu)                   \
({                                              \
        struct percpu_data *__p = (struct percpu_data *)~(unsigned long)(ptr); \
        (__typeof__(ptr))__p->ptrs[(cpu)];	\
})

extern void percpu_free( void *);
extern void *__percpu_alloc_mask(size_t size, cpumask_t *mask);

#define percpu_alloc_mask(size, mask)  __percpu_alloc_mask((size),  &(mask))
#define percpu_alloc(size)    percpu_alloc_mask((size), cpu_present_map)


#endif /* _LWK_PERCPU_H */
