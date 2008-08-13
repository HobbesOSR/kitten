#ifndef _LWK_SPINLOCK_TYPES_H
#define _LWK_SPINLOCK_TYPES_H

/*
 * include/lwk/spinlock_types.h - generic spinlock type definitions
 *                                and initializers
 *
 * portions Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Released under the General Public License (GPL).
 */

#include <arch/spinlock_types.h>

typedef struct {
	raw_spinlock_t raw_lock;
#ifdef CONFIG_DEBUG_SPINLOCK
	unsigned int magic, owner_cpu;
	void *owner;
#endif
} spinlock_t;

#define SPINLOCK_MAGIC		0xdead4ead

typedef struct {
	raw_rwlock_t raw_lock;
#ifdef CONFIG_DEBUG_SPINLOCK
	unsigned int magic, owner_cpu;
	void *owner;
#endif
} rwlock_t;

#define RWLOCK_MAGIC		0xdeaf1eed

#define SPINLOCK_OWNER_INIT	((void *)-1L)

#ifdef CONFIG_DEBUG_SPINLOCK
# define SPIN_LOCK_UNLOCKED						\
			{	.raw_lock = __RAW_SPIN_LOCK_UNLOCKED,	\
				.magic = SPINLOCK_MAGIC,		\
				.owner = SPINLOCK_OWNER_INIT,		\
				.owner_cpu = -1 }
#define RW_LOCK_UNLOCKED						\
			{	.raw_lock = __RAW_RW_LOCK_UNLOCKED,	\
				.magic = RWLOCK_MAGIC,			\
				.owner = SPINLOCK_OWNER_INIT,		\
				.owner_cpu = -1 }
#else
# define SPIN_LOCK_UNLOCKED \
			{	.raw_lock = __RAW_SPIN_LOCK_UNLOCKED }
#define RW_LOCK_UNLOCKED \
			{	.raw_lock = __RAW_RW_LOCK_UNLOCKED }
#endif

#define DEFINE_SPINLOCK(x)	spinlock_t x = SPIN_LOCK_UNLOCKED
#define DEFINE_RWLOCK(x)	rwlock_t x = RW_LOCK_UNLOCKED

#endif /* _LWK_SPINLOCK_TYPES_H */
