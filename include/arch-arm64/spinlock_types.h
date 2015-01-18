#ifndef _ARM64_SPINLOCK_TYPES_H
#define _ARM64_SPINLOCK_TYPES_H

#ifndef _LWK_SPINLOCK_TYPES_H
# error "please don't include this file directly"
#endif

#ifndef ARM_RAW_SPINLOCK
#define ARM_RAW_SPINLOCK
typedef struct _raw_spinlock_t {
	volatile unsigned int slock;
} raw_spinlock_t;
#endif

#define __RAW_SPIN_LOCK_UNLOCKED	{ 0 }

typedef struct {
	volatile unsigned int lock;
} raw_rwlock_t;

#define __RAW_RW_LOCK_UNLOCKED		{ RW_LOCK_BIAS }

#endif
