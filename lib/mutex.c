/*
 * kernel/mutex.c
 *
 * Mutexes: blocking mutual exclusion locks
 *
 * Started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 * Many thanks to Arjan van de Ven, Thomas Gleixner, Steven Rostedt and
 * David Howells for suggestions and improvements.
 *
 * Also see Documentation/mutex-design.txt.
 *
 * Imported to Kitten from Linux 2.6.25
 */
#include <lwk/mutex.h>
#include <lwk/sched.h>
#include <lwk/spinlock.h>
#include <lwk/interrupt.h>
#include <arch/mutex.h>

#define spin_lock_mutex(lock, flags) \
                do { spin_lock(lock); (void)(flags); } while (0)

#define spin_unlock_mutex(lock, flags) \
                do { spin_unlock(lock); (void)(flags); } while (0)

#define mutex_remove_waiter(lock, waiter, ti) \
                __list_del((waiter)->list.prev, (waiter)->list.next)




/***
 * mutex_init - initialize the mutex
 * @lock: the mutex to be initialized
 *
 * Initialize the mutex to unlocked state.
 *
 * It is not allowed to initialize an already locked mutex.
 */
void
__mutex_init(struct mutex *lock, const char *name)
{
	atomic_set(&lock->count, 1);
	spin_lock_init(&lock->wait_lock);
	INIT_LIST_HEAD(&lock->wait_list);
}


/*
 * We split the mutex lock/unlock logic into separate fastpath and
 * slowpath functions, to reduce the register pressure on the fastpath.
 * We also put the fastpath first in the kernel image, to make sure the
 * branch is predicted by the CPU as default-untaken.
 */
static void noinline
__mutex_lock_slowpath(atomic_t *lock_count);

/***
 * mutex_lock - acquire the mutex
 * @lock: the mutex to be acquired
 *
 * Lock the mutex exclusively for this task. If the mutex is not
 * available right now, it will sleep until it can get it.
 *
 * The mutex must later on be released by the same task that
 * acquired it. Recursive locking is not allowed. The task
 * may not exit without first unlocking the mutex. Also, kernel
 * memory where the mutex resides mutex must not be freed with
 * the mutex still locked. The mutex must first be initialized
 * (or statically defined) before it can be locked. memset()-ing
 * the mutex to 0 is not allowed.
 *
 * ( The CONFIG_DEBUG_MUTEXES .config option turns on debugging
 *   checks that will enforce the restrictions and will also do
 *   deadlock debugging. )
 *
 * This function is similar to (but not equivalent to) down().
 */
void inline mutex_lock(struct mutex *lock)
{
	/*
	 * The locking fastpath is the 1->0 transition from
	 * 'unlocked' into 'locked' state.
	 */
	__mutex_fastpath_lock(&lock->count, __mutex_lock_slowpath);
}


static noinline void __mutex_unlock_slowpath(atomic_t *lock_count);

/***
 * mutex_unlock - release the mutex
 * @lock: the mutex to be released
 *
 * Unlock a mutex that has been locked by this task previously.
 *
 * This function must not be used in interrupt context. Unlocking
 * of a not locked mutex is not allowed.
 *
 * This function is similar to (but not equivalent to) up().
 */
void mutex_unlock(struct mutex *lock)
{
	/*
	 * The unlocking fastpath is the 0->1 transition from 'locked'
	 * into 'unlocked' state:
	 */
	__mutex_fastpath_unlock(&lock->count, __mutex_unlock_slowpath);
}


/*
 * Lock a mutex (possibly interruptible), slowpath:
 */
static inline int
__mutex_lock_common(struct mutex *lock, long state)
{
	struct task_struct *task = current;
	struct mutex_waiter waiter;
	unsigned int old_val;
	unsigned long flags;

	spin_lock_mutex(&lock->wait_lock, flags);

	/* add waiting tasks to the end of the waitqueue (FIFO): */
	list_add_tail(&waiter.list, &lock->wait_list);
	waiter.task = task;

	old_val = atomic_xchg(&lock->count, -1);
	if (old_val == 1)
		goto done;

	for (;;) {
		/*
		 * Lets try to take the lock again - this is needed even if
		 * we get here for the first time (shortly after failing to
		 * acquire the lock), to make sure that we get a wakeup once
		 * it's unlocked. Later on, if we sleep, this is the
		 * operation that gives us the lock. We xchg it to -1, so
		 * that when we release the lock, we properly wake up the
		 * other waiters:
		 */
		old_val = atomic_xchg(&lock->count, -1);
		if (old_val == 1)
			break;

		/*
		 * got a signal? (This code gets eliminated in the
		 * TASK_UNINTERRUPTIBLE case.)
		 */
#if 0
		if (unlikely((state == TASKSTATE_INTERRUPTIBLE &&
					signal_pending(task)) ||
			      (state == TASKSTATE_KILLABLE &&
					fatal_signal_pending(task)))) {
			mutex_remove_waiter(lock, &waiter,
					    task_thread_info(task));
			spin_unlock_mutex(&lock->wait_lock, flags);

			return -EINTR;
		}
#endif
		set_mb( task->state, state );

		/* didnt get the lock, go to sleep: */
		spin_unlock_mutex(&lock->wait_lock, flags);
		schedule();
		spin_lock_mutex(&lock->wait_lock, flags);
	}

done:
	/* got the lock - rejoice! */
	mutex_remove_waiter(lock, &waiter, task_thread_info(task));

	/* set it to 0 if there are no waiters left: */
	if (likely(list_empty(&lock->wait_list)))
		atomic_set(&lock->count, 0);

	spin_unlock_mutex(&lock->wait_lock, flags);


	return 0;
}


/*
 * Release the lock, slowpath:
 */
static inline void
__mutex_unlock_common_slowpath(atomic_t *lock_count, int nested)
{
	struct mutex *lock = container_of(lock_count, struct mutex, count);
	unsigned long flags;

	spin_lock_mutex(&lock->wait_lock, flags);

	/*
	 * some architectures leave the lock unlocked in the fastpath failure
	 * case, others need to leave it locked. In the later case we have to
	 * unlock it here
	 */
	if (__mutex_slowpath_needs_to_unlock())
		atomic_set(&lock->count, 1);

	int call_schedule = 0;

	if (!list_empty(&lock->wait_list)) {
		/* get the first entry from the wait-list: */
		struct mutex_waiter *waiter =
				list_entry(lock->wait_list.next,
					   struct mutex_waiter, list);

		sched_wakeup_task( waiter->task, TASKSTATE_NORMAL );
		call_schedule = 1;
	}

	spin_unlock_mutex(&lock->wait_lock, flags);

	if( call_schedule )
		schedule();
}

/*
 * Release the lock, slowpath:
 */
static noinline void
__mutex_unlock_slowpath(atomic_t *lock_count)
{
	__mutex_unlock_common_slowpath(lock_count, 1);
}

/*
 * Here come the less common (and hence less performance-critical) APIs:
 * mutex_lock_interruptible() and mutex_trylock().
 */
static noinline int
__mutex_lock_interruptible_slowpath(atomic_t *lock_count);

/***
 * mutex_lock_interruptible - acquire the mutex, interruptable
 * @lock: the mutex to be acquired
 *
 * Lock the mutex like mutex_lock(), and return 0 if the mutex has
 * been acquired or sleep until the mutex becomes available. If a
 * signal arrives while waiting for the lock then this function
 * returns -EINTR.
 *
 * This function is similar to (but not equivalent to) down_interruptible().
 */
int mutex_lock_interruptible(struct mutex *lock)
{
	return __mutex_fastpath_lock_retval
			(&lock->count, __mutex_lock_interruptible_slowpath);
}

static noinline void
__mutex_lock_slowpath(atomic_t *lock_count)
{
	struct mutex *lock = container_of(lock_count, struct mutex, count);

	__mutex_lock_common(lock, TASKSTATE_UNINTERRUPTIBLE);
}

static noinline int
__mutex_lock_interruptible_slowpath(atomic_t *lock_count)
{
	struct mutex *lock = container_of(lock_count, struct mutex, count);

	return __mutex_lock_common(lock, TASKSTATE_INTERRUPTIBLE);
}

/*
 * Spinlock based trylock, we take the spinlock and check whether we
 * can get the lock:
 */
static inline int __mutex_trylock_slowpath(atomic_t *lock_count)
{
	struct mutex *lock = container_of(lock_count, struct mutex, count);
	unsigned long flags;
	int prev;

	spin_lock_mutex(&lock->wait_lock, flags);

	prev = atomic_xchg(&lock->count, -1);
	/* Set it back to 0 if there are no waiters: */
	if (likely(list_empty(&lock->wait_list)))
		atomic_set(&lock->count, 0);

	spin_unlock_mutex(&lock->wait_lock, flags);

	return prev == 1;
}

/***
 * mutex_trylock - try acquire the mutex, without waiting
 * @lock: the mutex to be acquired
 *
 * Try to acquire the mutex atomically. Returns 1 if the mutex
 * has been acquired successfully, and 0 on contention.
 *
 * NOTE: this function follows the spin_trylock() convention, so
 * it is negated to the down_trylock() return values! Be careful
 * about this when converting semaphore users to mutexes.
 *
 * This function must not be used in interrupt context. The
 * mutex must be released by the same task that acquired it.
 */
int mutex_trylock(struct mutex *lock)
{
	return __mutex_fastpath_trylock(&lock->count,
					__mutex_trylock_slowpath);
}



#include <lwk/driver.h>
#include <lwk/types.h>

DEFINE_MUTEX( test_mutex );

static int
mutex_thread( void * id_v )
{
	int iters = 10;
	const unsigned long id = (unsigned long) id_v;

	while( iters-- )
	{
		printk( "%s: %ld Getting mutex\n", __func__, id );
		mutex_lock( &test_mutex );
		printk( "%s: %ld Got it %d\n", __func__, id, iters );

		volatile uint64_t i;
		for( i=0 ; i < (1<<20) ; i++ )
			schedule();
		printk( "%s: %ld unlocking %d\n", __func__, id, iters );
		mutex_unlock( &test_mutex );
	}

	return 0;
}


void
mutex_test( void )
{
	printk( "%s: Creating mutex threads\n", __func__ );
	mutex_init( &test_mutex );
	unsigned long i;
	for( i=0 ; i<4 ; i++ )
		kthread_create( mutex_thread, (void*) i, "mutex%ld\n", i );
}

