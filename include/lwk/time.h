#ifndef _LWK_TIME_H
#define _LWK_TIME_H

#include <lwk/types.h>
#include <lwk/init.h>
#include <arch/time.h>

#define NSEC_PER_SEC  1000000000L
#define NSEC_PER_USEC 1000L
#define USEC_PER_NSEC 1000L

struct timeval {
	time_t		tv_sec;		/* seconds */
	suseconds_t	tv_usec;	/* microseconds */
};

struct timezone {
	int		tz_minuteswest;	/* minutes west of Greenwich */
	int		tz_dsttime;	/* type of dst correction */
};

struct timespec {
	time_t		tv_sec;		/* seconds */
	long		tv_nsec;	/* nanoseconds */
};

void __init time_init(void);
void init_cycles2ns(uint32_t khz);
uint64_t cycles2ns(uint64_t cycles);
uint64_t get_time(void);
void set_time(uint64_t ns);

#define timespec_is_valid(ts) \
(((ts)->tv_sec >= 0) && (((unsigned long)(ts)->tv_nsec) < NSEC_PER_SEC))

#endif
