#ifndef _X86_64_SPINLOCK_TYPES_H
#define _X86_64_SPINLOCK_TYPES_H

#ifndef _LWK_SPINLOCK_TYPES_H
# error "please don't include this file directly"
#endif

typedef struct {
	volatile unsigned int slock;
} raw_spinlock_t;

#define __RAW_SPIN_LOCK_UNLOCKED	{ 1 }

typedef struct {
	volatile unsigned int lock;
} raw_rwlock_t;

#define __RAW_RW_LOCK_UNLOCKED		{ RW_LOCK_BIAS }

#endif
