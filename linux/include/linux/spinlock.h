#ifndef __LINUX_SPINLOCK_H
#define __LINUX_SPINLOCK_H

#include <lwk/spinlock.h>

#define spin_lock_nested(lock, subclass)	_spin_lock(lock)
#define spin_lock_nest_lock(lock, nest_lock)	_spin_lock(lock)

#endif
