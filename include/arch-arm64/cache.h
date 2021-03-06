/*
 * include/arch-x86_64/cache.h
 */
#ifndef _ARCH_CACHE_H
#define _ARCH_CACHE_H

/* L1 cache line size */
#define L1_CACHE_SHIFT		(CONFIG_ARM64_L1_CACHE_SHIFT)
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

/* Inter-node cache line size for NUMA systems */
#define INTERNODE_CACHE_SHIFT	(CONFIG_ARM64_INTERNODE_CACHE_SHIFT)
#define INTERNODE_CACHE_BYTES	(1 << INTERNODE_CACHE_SHIFT)

#define __read_mostly __attribute__((__section__(".data.read_mostly")))

#endif
