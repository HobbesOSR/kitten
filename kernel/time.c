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
ktime_t
cycles2ns(uint64_t cycles)
{
	return (cycles * mult) >> shift;
}

/**
 * Returns the current time in nanoseconds.
 */
ktime_t
get_time(void)
{
	return cycles2ns(get_cycles()) + offset;
}

/**
 * Sets the current time in nanoseconds.
 */
void
set_time(ktime_t ns)
{
	offset = ns - cycles2ns(get_cycles());
}

/**
 * set_normalized_timespec - set timespec sec and nsec parts and normalize
 *
 * @ts:         pointer to timespec variable to be set
 * @sec:        seconds to set
 * @nsec:       nanoseconds to set
 *
 * Set seconds and nanoseconds field of a timespec variable and
 * normalize to the timespec storage format
 *
 * Note: The tv_nsec part is always in the range of
 *      0 <= tv_nsec < NSEC_PER_SEC
 * For negative values only the tv_sec field is negative !
 */
void set_normalized_timespec(struct timespec *ts, time_t sec, s64 nsec)
{
	while (nsec >= NSEC_PER_SEC) {
		/*
		 * The following asm() prevents the compiler from
		 * optimising this loop into a modulo operation. See
		 * also __iter_div_u64_rem() in include/linux/time.h
		 */
		asm("" : "+rm"(nsec));
		nsec -= NSEC_PER_SEC;
		++sec;
	}
	while (nsec < 0) {
		asm("" : "+rm"(nsec));
		nsec += NSEC_PER_SEC;
		--sec;
	}
	ts->tv_sec = sec;
	ts->tv_nsec = nsec;
}

/*
 * Add two timespec values and do a safety check for overflow.
 * It's assumed that both values are valid (>= 0)
 */
struct timespec timespec_add_safe(const struct timespec lhs,
								  const struct timespec rhs)
{
	struct timespec res;

	set_normalized_timespec(&res, lhs.tv_sec + rhs.tv_sec,
							lhs.tv_nsec + rhs.tv_nsec);
	
	if (res.tv_sec < lhs.tv_sec || res.tv_sec < rhs.tv_sec)
		res.tv_sec = TIME_T_MAX;
	
	return res;
}
