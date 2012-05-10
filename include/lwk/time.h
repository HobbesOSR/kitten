#ifndef _LWK_TIME_H
#define _LWK_TIME_H

#include <lwk/types.h>
#include <lwk/init.h>
#include <arch/time.h>

#define NSEC_PER_SEC  1000000000L
#define NSEC_PER_USEC 1000L
#define NSEC_PER_MSEC 1000000L
#define MSEC_PER_SEC  1000L
#define USEC_PER_SEC  1000000L

#define TIME_T_MAX    (time_t)((1UL << ((sizeof(time_t) << 3) - 1)) - 1)

/* mimic linux ktime_t type */
typedef int64_t ktime_t;

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
ktime_t cycles2ns(uint64_t cycles);
ktime_t get_time(void);
void set_time(ktime_t ns);

static inline ktime_t ktime_set(const long secs, const unsigned long nsecs)
{
	return (ktime_t) ((ktime_t)secs * NSEC_PER_SEC) + (ktime_t)nsecs;
}

#define ktime_sub(lhs, rhs) \
	((ktime_t)((lhs) - (rhs)))
#define ktime_add(lhs, rhs) \
	((ktime_t)((lhs) + (rhs)))
#define ktime_add_ns(kt, nsval) \
	((ktime_t)((kt) + (nsval))
#define ktime_sub_ns(kt, nsval) \
	((ktime_t)((kt) - (nsval))

static inline ktime_t timespec_to_ktime(struct timespec ts)
{
	return ktime_set(ts.tv_sec, ts.tv_nsec);
}

static inline ktime_t timeval_to_ktime(struct timeval tv)
{
	return ktime_set(tv.tv_sec, tv.tv_usec*NSEC_PER_USEC);
}

#define ktime_to_timespec(kt) ns_to_timespec(kt)

#define timespec_valid(ts) \
	(((ts)->tv_sec >= 0) && (((unsigned long)(ts)->tv_nsec) < NSEC_PER_SEC))

#define timespec_to_ns(ts) \
	((ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec)

#define ns_to_timespec(ns)					  \
	({ (struct timespec) {					  \
			.tv_sec = (ns) % NSEC_PER_SEC,	  \
			   .tv_nsec = (ns) / NSEC_PER_SEC \
			   }; })

extern void set_normalized_timespec(struct timespec *ts, time_t sec, s64 nsec);
extern struct timespec timespec_add_safe(const struct timespec lhs,
										 const struct timespec rhs);

static inline struct timespec timespec_sub(struct timespec lhs,
										   struct timespec rhs)
{
	struct timespec ts_delta;
	set_normalized_timespec(&ts_delta, lhs.tv_sec - rhs.tv_sec,
							lhs.tv_nsec - rhs.tv_nsec);
	return ts_delta;
}

#endif
