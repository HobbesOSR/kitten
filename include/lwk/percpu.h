#ifndef _LWK_PERCPU_H
#define _LWK_PERCPU_H
#include <lwk/smp.h>
#include <lwk/string.h>
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

/* 
 * Use this to get to a cpu's version of the per-cpu object allocated using
 * alloc_percpu.  Non-atomic access to the current CPU's version should
 * probably be combined with get_cpu()/put_cpu().
 */ 
#define per_cpu_ptr(ptr, cpu)                   \
({                                              \
        struct percpu_data *__p = (struct percpu_data *)~(unsigned long)(ptr); \
        (__typeof__(ptr))__p->ptrs[(cpu)];	\
})

extern void *__alloc_percpu(size_t size);
extern void free_percpu(const void *);

/* Simple wrapper for the common case: zeros memory. */
#define alloc_percpu(type)	((type *)(__alloc_percpu(sizeof(type))))

extern void setup_per_cpu_areas(void);

#endif /* _LWK_PERCPU_H */
