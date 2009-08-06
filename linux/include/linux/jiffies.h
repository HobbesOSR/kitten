/**
 * \file
 * The LWK doesn't have jiffies, so this does impedance matching.
 */

#ifndef _LINUX_JIFFIES_H
#define _LINUX_JIFFIES_H

/**
 * LWK doesn't have jiffies, so assume it == nanoseconds.
 * Change all jiffies references to get_time().
 */
#define jiffies			get_time()
#define HZ			1000000000ul

/**
 * These macros are for handling wrap-around.
 * Since we are using 64-bit time values, assume this never happens.
 * @{
 */
#define time_before(a,b)	((a) < (b))
#define time_after(a,b)		((a) > (b))
#define time_after_eq(a,b)	((a) >= (b))
//@}

/**
 * Converts milliseconds to jiffies (aka nanoseconds).
 */
static inline unsigned long
msecs_to_jiffies(const unsigned int m)
{
	return m * 1000 * 1000;
}

#endif
