/** \file
 * Adapter layer for lwip to work with Kitten locks.
 */
#include <lwk/types.h>
#include <lwk/spinlock.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <lwk/sched.h>
#include <lwk/string.h>
#include <lwk/kthread.h>
#include <lwip/sys.h>


/** Semaphore and mailbox implementation for lwip */
spinlock_t sem_lock;
spinlock_t mbox_lock;

spinlock_t sys_lock;

/** Debuging output toggle.
 * 0 == none
 * 1 == creation / deletion
 * 2 == mbox operations
 * 3 == semaphore operations
 */
static const int sem_debug = 0;

/** Rough estiamte of CPU speed in KHz */
static const uint64_t cpu_hz = 2400000;

void
sys_init( void )
{
	spin_lock_init( &sem_lock );
	spin_lock_init( &mbox_lock );
	spin_lock_init( &sys_lock );
}


err_t
sys_sem_new(sys_sem_t *		sem,
	u8_t			count
)
{
	*sem = kmem_alloc( sizeof( **sem ) );
	**sem = count;

	if( sem_debug >= 1 )
	printk( "%s: sem %p value %d\n", __func__, *sem, count );

        return 0;
}


void
sys_sem_free(
	sys_sem_t *		sem
)
{
	kmem_free( (void*) (*sem) );
}


void
sys_sem_signal(
	sys_sem_t *		sem
)
{
	unsigned long irqstate;
	spin_lock_irqsave( &sem_lock, irqstate );
	(**sem)++;
	spin_unlock_irqrestore( &sem_lock, irqstate );

	if( sem_debug >= 3 )
	printk( "%s: sem %p value %d\n", __func__, *sem, **sem );
}


/** Try once to get the semaphore.
 *
 * \returns 0 if we did not get it, 1 if we did
 */
u32_t
sys_arch_sem_trywait(
	sys_sem_t *		sem
)
{
	unsigned long irqstate;
	spin_lock_irqsave( &sem_lock, irqstate );
	const int value = **sem;
	if( value )
		(**sem)--;
	spin_unlock_irqrestore( &sem_lock, irqstate );

	return value;
}



u32_t
sys_arch_sem_wait(
	sys_sem_t *		sem,
	u32_t			timeout
)
{
	const uint64_t start_time = rdtsc();
	const uint64_t timeout_ticks = timeout * cpu_hz;

	if( sem_debug >= 3 )
	printk( "%s: waiting for sem %p value %d\n", __func__, *sem, **sem );

	while( 1 )
	{
		const uint64_t delta_ticks = rdtsc() - start_time;
		if( timeout && delta_ticks > timeout_ticks )
			return SYS_ARCH_TIMEOUT;

		if( !sys_arch_sem_trywait( sem ) )
		{
			// Not yet.  Try again
			schedule();
			continue;
		}


		// We got it.
		if( sem_debug >= 3 )
		printk( "%s: sem %p\n", __func__, *sem );

		return delta_ticks / cpu_hz;
	}
}


int sys_sem_valid(sys_sem_t *sem)
{
    return 1;
}


void sys_sem_set_invalid(sys_sem_t *sem)
{
}


sys_prot_t 
sys_arch_protect( void )
{
    sys_prot_t irqstate;
    spin_lock_irqsave(&sys_lock, irqstate);

    return irqstate;
}


void 
sys_arch_unprotect(
	sys_prot_t		pval
)
{
    spin_unlock_irqrestore(&sys_lock, pval);
}


err_t
sys_mbox_new(
        sys_mbox_t *		mbox,
	int			size
)
{
	if( size <= 0 )
		size = 8;

	(*mbox) = kmem_alloc( sizeof(**mbox) + sizeof(void*) * size );
	(*mbox)->size = size;
	(*mbox)->read = (*mbox)->write = 0;

	if( sem_debug >= 1 )
	printk( "%s: %p @ %d\n", __func__, *mbox, size );

	return 0;
}


void
sys_mbox_free(
	sys_mbox_t *		mbox
)
{
	if( (*mbox)->read != (*mbox)->write )
		printk( "%s: mbox has remaining elements %d/%d\n",
			__func__,
			(*mbox)->read,
			(*mbox)->write
		);

	kmem_free( *mbox );
}



err_t
_sys_mbox_post(
	sys_mbox_t *		mbox,
	void *			msg,
	int			try_once
)
{
	unsigned long irqstate;
	while(1)
	{
		spin_lock_irqsave( &mbox_lock, irqstate );
		const int write = (*mbox)->write;
		const int next = (write + 1) % (*mbox)->size;
		if( next != (*mbox)->read )
		{
			(*mbox)->msgs[ write ] = msg;
			(*mbox)->write = next;
			spin_unlock_irqrestore( &mbox_lock, irqstate );

			if( sem_debug >= 2 )
			printk( "%s: mbox %p[%d] posting %p\n",
				__func__,
				*mbox,
				write,
				msg
			);

			return ERR_OK;
		}
		spin_unlock_irqrestore( &mbox_lock, irqstate );

		if( try_once )
			return ERR_MEM;

		// Give up the CPU until we can check again
		schedule();
	}
}


void
sys_mbox_post(
	sys_mbox_t *		mbox,
	void *			msg
)
{
	_sys_mbox_post( mbox, msg, 0 );
}


err_t
sys_mbox_trypost(
	sys_mbox_t *		mbox,
	void *			msg
)
{
	return _sys_mbox_post( mbox, msg, 1 );
}


u32_t
sys_arch_mbox_tryfetch(
	sys_mbox_t *		mbox,
	void **			msg
)
{
	unsigned long irqstate;
	spin_lock_irqsave( &mbox_lock, irqstate );
	const int read = (*mbox)->read;
	if( read == (*mbox)->write )
	{
		spin_unlock_irqrestore( &mbox_lock, irqstate );
		return SYS_MBOX_EMPTY;
	}

	*msg = (*mbox)->msgs[ read ];
	(*mbox)->read = ( read + 1 ) % (*mbox)->size;
	spin_unlock_irqrestore( &mbox_lock, irqstate );

	if( sem_debug >= 2 )
	printk( "%s: mbox %p[%d] read %p\n",
		__func__,
		*mbox,
		read,
		*msg
	);

	return 0;
}


u32_t
sys_arch_mbox_fetch(
	sys_mbox_t *		mbox,
	void **			msg,
	u32_t			timeout
)
{
	const uint64_t start_time = rdtsc();
	const uint64_t timeout_ticks = timeout * cpu_hz;

	while( 1 )
	{
		const uint64_t delta_ticks = rdtsc() - start_time;
		if( timeout && delta_ticks > timeout_ticks )
		{
			if( sem_debug >= 3 )
			printk( "%s: timed out %d ticks %llx -> %llx\n", 
				__func__,
				timeout,
				start_time,
				rdtsc()
			);
			return SYS_ARCH_TIMEOUT;
		}

		if( sys_arch_mbox_tryfetch( mbox, msg ) == 0 )
			return delta_ticks / cpu_hz;

		// Put us to sleep for a bit
		schedule();
	}
}


/* Threads */
sys_thread_t
sys_thread_new(
	const char *		name,
	void 			(*entry_point)( void * arg ),
	void *			arg,
	int			stacksize,
	int			prio
)
{
	struct task_struct *kthread =
		kthread_create((int (*)(void *))entry_point, arg, "ip:%s", name);
	return (kthread) ? kthread->id : ERROR_ID;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return *mbox != NULL;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
}
