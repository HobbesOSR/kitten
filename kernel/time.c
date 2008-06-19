#include <lwk/time.h>
#include <arch/div64.h>

static uint64_t shift;
static uint64_t mult;
static uint64_t offset;

/**
 * Converts the input khz cycle counter frequency to a time source multiplier.
 * The multiplier is used to convert cycle counts to nanoseconds.
 */
void
init_cycles2ns(uint32_t khz)
{
	/*
	 * Shift is used to obtain greater precision.
	 * Linux uses 22 for the x86 time stamp counter.
	 * For now we assume this will work for most cases.
	 */
	shift = 22;

	/*  
 	 *  khz = cyc/(Million ns)
	 *  mult/2^shift  = ns/cyc
	 *  mult = ns/cyc * 2^shift
	 *  mult = 1Million/khz * 2^shift
	 *  mult = 1000000 * 2^shift / khz
	 *  mult = (1000000<<shift) / khz
	 */
	mult = ((u64)1000000) << shift;
	mult += khz/2; /* round for do_div */
	do_div(mult, khz);
}

/**
 * Converts cycles to nanoseconds.
 * init_cycles2ns() must be called before this will work properly.
 */
uint64_t
cycles2ns(uint64_t cycles)
{
	return (cycles * mult) >> shift;
}

/**
 * Returns the current time in nanoseconds.
 */
uint64_t
get_time(void)
{
	return cycles2ns(get_cycles()) + offset;
}

/**
 * Sets the current time in nanoseconds.
 */
void
set_time(uint64_t ns)
{
	offset = ns - cycles2ns(get_cycles());
}

