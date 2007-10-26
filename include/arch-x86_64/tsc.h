/*
 * linux/include/asm-i386/tsc.h
 *
 * i386 TSC related functions
 */
#ifndef _ARCH_x86_64_TSC_H
#define _ARCH_x86_64_TSC_H

#include <lwk/types.h>
#include <arch/processor.h>

typedef uint64_t cycles_t;

/**
 * Returns the current value of the CPU's cycle counter.
 *
 * NOTE: This is not serializing. It doesn't necessarily wait for previous
 *       instructions to complete before reading the cycle counter. Also,
 *       subsequent instructions could potentially begin execution before
 *       the cycle counter is read.
 */
static __always_inline cycles_t
get_cycles(void)
{
	cycles_t ret = 0;
	rdtscll(ret);
	return ret;
}

/**
 * This is a synchronizing version of get_cycles(). It ensures that all
 * previous instructions have completed before reading the cycle counter.
 */
static __always_inline cycles_t
get_cycles_sync(void)
{
	sync_core();
	return get_cycles();
}

#endif
