/*
 * Copyright (2004) Linus Torvalds
 *
 * Author: Zwane Mwaikambo <zwane@fsmlabs.com>
 *
 * Copyright (2004, 2005) Ingo Molnar
 *
 * This file contains the spinlock/rwlock implementations for the
 * SMP and the DEBUG_SPINLOCK cases. 
 */

#include <lwk/linkage.h>
#include <lwk/spinlock.h>
//#include <lwk/interrupt.h>
#include <lwk/linux_compat.h>

/*
 * Generic declaration of the raw read_trylock() function,
 * architectures are supposed to optimize this:
 */
int __lockfunc generic__raw_read_trylock(raw_rwlock_t *lock)
{
	__raw_read_lock(lock);
	return 1;
}
EXPORT_SYMBOL(generic__raw_read_trylock);

int __lockfunc _spin_trylock(spinlock_t *lock)
{
	if (_raw_spin_trylock(lock))
		return 1;
	return 0;
}
EXPORT_SYMBOL(_spin_trylock);

int __lockfunc _read_trylock(rwlock_t *lock)
{
	if (_raw_read_trylock(lock))
		return 1;
	return 0;
}
EXPORT_SYMBOL(_read_trylock);

int __lockfunc _write_trylock(rwlock_t *lock)
{
	if (_raw_write_trylock(lock))
		return 1;
	return 0;
}
EXPORT_SYMBOL(_write_trylock);

void __lockfunc _read_lock(rwlock_t *lock)
{
	_raw_read_lock(lock);
}
EXPORT_SYMBOL(_read_lock);

unsigned long __lockfunc _spin_lock_irqsave(spinlock_t *lock)
{
	unsigned long flags;

	local_irq_save(flags);
	_raw_spin_lock_flags(lock, &flags);
	return flags;
}
EXPORT_SYMBOL(_spin_lock_irqsave);

void __lockfunc _spin_lock_irq(spinlock_t *lock)
{
	local_irq_disable();
	_raw_spin_lock(lock);
}
EXPORT_SYMBOL(_spin_lock_irq);

#if 0
void __lockfunc _spin_lock_bh(spinlock_t *lock)
{
	local_bh_disable();
	_raw_spin_lock(lock);
}
EXPORT_SYMBOL(_spin_lock_bh);
#endif

unsigned long __lockfunc _read_lock_irqsave(rwlock_t *lock)
{
	unsigned long flags;

	local_irq_save(flags);
	_raw_read_lock(lock);
	return flags;
}
EXPORT_SYMBOL(_read_lock_irqsave);

void __lockfunc _read_lock_irq(rwlock_t *lock)
{
	local_irq_disable();
	_raw_read_lock(lock);
}
EXPORT_SYMBOL(_read_lock_irq);

#if 0
void __lockfunc _read_lock_bh(rwlock_t *lock)
{
	local_bh_disable();
	_raw_read_lock(lock);
}
EXPORT_SYMBOL(_read_lock_bh);
#endif

unsigned long __lockfunc _write_lock_irqsave(rwlock_t *lock)
{
	unsigned long flags;

	local_irq_save(flags);
	_raw_write_lock(lock);
	return flags;
}
EXPORT_SYMBOL(_write_lock_irqsave);

void __lockfunc _write_lock_irq(rwlock_t *lock)
{
	local_irq_disable();
	_raw_write_lock(lock);
}
EXPORT_SYMBOL(_write_lock_irq);

#if 0
void __lockfunc _write_lock_bh(rwlock_t *lock)
{
	local_bh_disable();
	_raw_write_lock(lock);
}
EXPORT_SYMBOL(_write_lock_bh);
#endif

void __lockfunc _spin_lock(spinlock_t *lock)
{
	_raw_spin_lock(lock);
}

EXPORT_SYMBOL(_spin_lock);

void __lockfunc _write_lock(rwlock_t *lock)
{
	_raw_write_lock(lock);
}

EXPORT_SYMBOL(_write_lock);

void __lockfunc _spin_unlock(spinlock_t *lock)
{
	_raw_spin_unlock(lock);
}
EXPORT_SYMBOL(_spin_unlock);

void __lockfunc _write_unlock(rwlock_t *lock)
{
	_raw_write_unlock(lock);
}
EXPORT_SYMBOL(_write_unlock);

void __lockfunc _read_unlock(rwlock_t *lock)
{
	_raw_read_unlock(lock);
}
EXPORT_SYMBOL(_read_unlock);

void __lockfunc _spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	_raw_spin_unlock(lock);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(_spin_unlock_irqrestore);

void __lockfunc _spin_unlock_irq(spinlock_t *lock)
{
	_raw_spin_unlock(lock);
	local_irq_enable();
}
EXPORT_SYMBOL(_spin_unlock_irq);

#if 0
void __lockfunc _spin_unlock_bh(spinlock_t *lock)
{
	_raw_spin_unlock(lock);
	local_bh_enable();
}
EXPORT_SYMBOL(_spin_unlock_bh);
#endif

void __lockfunc _read_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
{
	_raw_read_unlock(lock);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(_read_unlock_irqrestore);

void __lockfunc _read_unlock_irq(rwlock_t *lock)
{
	_raw_read_unlock(lock);
	local_irq_enable();
}
EXPORT_SYMBOL(_read_unlock_irq);

#if 0
void __lockfunc _read_unlock_bh(rwlock_t *lock)
{
	_raw_read_unlock(lock);
	local_bh_enable();
}
EXPORT_SYMBOL(_read_unlock_bh);
#endif

void __lockfunc _write_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
{
	_raw_write_unlock(lock);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(_write_unlock_irqrestore);

void __lockfunc _write_unlock_irq(rwlock_t *lock)
{
	_raw_write_unlock(lock);
	local_irq_enable();
}
EXPORT_SYMBOL(_write_unlock_irq);

#if 0
void __lockfunc _write_unlock_bh(rwlock_t *lock)
{
	_raw_write_unlock(lock);
	local_bh_enable();
}
EXPORT_SYMBOL(_write_unlock_bh);
#endif

#if 0
int __lockfunc _spin_trylock_bh(spinlock_t *lock)
{
	local_bh_disable();
	if (_raw_spin_trylock(lock))
		return 1;
	local_bh_enable();
	return 0;
}
EXPORT_SYMBOL(_spin_trylock_bh);
#endif

int in_lock_functions(unsigned long addr)
{
	/* Linker adds these: start and end of __lockfunc functions */
	extern char __lock_text_start[], __lock_text_end[];

	return addr >= (unsigned long)__lock_text_start
	&& addr < (unsigned long)__lock_text_end;
}
EXPORT_SYMBOL(in_lock_functions);
