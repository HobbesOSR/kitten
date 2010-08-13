/**
 * \file
 * The LWK doesn't have jiffies, so this does impedance matching.
 */

#ifndef _LINUX_JIFFIES_H
#define _LINUX_JIFFIES_H
#include <lwk/time.h>
/**
 * LWK doesn't have jiffies, so assume it == nanoseconds.
 * Change all jiffies references to get_time().
 */
#define jiffies			get_time()
#define HZ			1000000000ul
#define CURRENT_TIME		ns_to_timespec(get_time())

/**
 * These macros are for handling wrap-around.
 * Since we are using 64-bit time values, assume this never happens.
 * @{
 */
#define time_before(a,b)	((a) < (b))
#define time_after(a,b)		((a) > (b))
#define time_after_eq(a,b)	((a) >= (b))
//@}
#define time_after64(a,b)       \
	(typecheck(__u64, a) && \
	 typecheck(__u64, b) && \
	 ((__s64)(b) - (__s64)(a) < 0))
#define time_before64(a,b)      time_after64(b,a)


/**
 * Converts milliseconds to jiffies (aka nanoseconds).
 */
static inline unsigned long
msecs_to_jiffies(const unsigned int m)
{
	return m * 1000L * 1000L;
}

static inline unsigned long usecs_to_jiffies(const unsigned int u)
{
	return u;
}

static inline unsigned int jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
# if BITS_PER_LONG == 32
	return (HZ_TO_MSEC_MUL32 * j) >> HZ_TO_MSEC_SHR32;
# else
	return (j * HZ_TO_MSEC_NUM) / HZ_TO_MSEC_DEN;
# endif
#endif
}

static inline u64 get_jiffies_64(void)
{
	return get_time();	
}
#endif
