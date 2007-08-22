#ifndef _LWK_CACHE_H
#define _LWK_CACHE_H

#include <lwk/kernel.h>
#include <arch/cache.h>

#ifndef L1_CACHE_ALIGN
#define L1_CACHE_ALIGN(x) ALIGN(x, L1_CACHE_BYTES)
#endif

#ifndef SMP_CACHE_BYTES
#define SMP_CACHE_BYTES L1_CACHE_BYTES
#endif

#ifndef __read_mostly
#define __read_mostly
#endif

#ifndef ____cacheline_aligned
#define ____cacheline_aligned __attribute__((__aligned__(SMP_CACHE_BYTES)))
#endif

#ifndef ____cacheline_aligned_in_smp
#define ____cacheline_aligned_in_smp ____cacheline_aligned
#endif

#ifndef __cacheline_aligned
#define __cacheline_aligned					\
  __attribute__((__aligned__(SMP_CACHE_BYTES),			\
		 __section__(".data.cacheline_aligned")))
#endif

#ifndef __cacheline_aligned_in_smp
#define __cacheline_aligned_in_smp __cacheline_aligned
#endif

/*
 * The maximum alignment needed for some critical structures
 * These could be inter-node cacheline sizes/L3 cacheline
 * size etc.  Define this in asm/cache.h for your arch
 */
#ifndef INTERNODE_CACHE_SHIFT
#define INTERNODE_CACHE_SHIFT L1_CACHE_SHIFT
#endif

#if !defined(____cacheline_internodealigned_in_smp)
#define ____cacheline_internodealigned_in_smp \
	__attribute__((__aligned__(1 << (INTERNODE_CACHE_SHIFT))))
#endif

#endif /* _LWK_CACHE_H */
