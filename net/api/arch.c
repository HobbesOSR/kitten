/** \file
 * Adapter layer for lwip to work with Kitten locks.
 */
#include <lwk/types.h>
#include <lwk/spinlock.h>
#include <lwip/sys.h>


/** Semaphore and mailbox implementation for lwip */
spinlock_t sem_lock;
spinlock_t mbox_lock;

void
sys_init( void )
{
	spin_lock_init( &sem_lock );
	spin_lock_init( &mbox_lock );
}


sys_sem_t
sys_sem_new(
	u8_t			count
)
{
	int * lock = kmem_alloc( sizeof( *lock ) );
	*lock = count;
	return lock;
}


void
sys_sem_free(
	sys_sem_t		sem
)
{
	kmem_free( (void*) sem );
}


void
sys_sem_signal(
	sys_sem_t		sem
)
{
	unsigned long irqstate;
	spin_lock_irqsave( &sem_lock, irqstate );
	*sem++;
	spin_unlock_irqrestore( &sem_lock, irqstate );
}


u32_t
sys_arch_sem_wait(
	sys_sem_t		sem,
	u32_t			timeout
)
{
	unsigned long irqstate;

	u32_t delay = 0;
	for( ; delay < timeout ; delay++ )
	{
		spin_lock_irqsave( &sem_lock, irqstate );
		const int value = *sem;

		if( value )
		{
			*sem--;
			spin_unlock_irqrestore( &sem_lock, irqstate );
			return delay;
		}

		// Didn't get it.  Keep trying
		spin_unlock_irqrestore( &sem_lock, irqstate );
	}

	return SYS_ARCH_TIMEOUT;
}


sys_mbox_t
sys_mbox_new(
	int			size
)
{
	if( size <= 0 )
		size = 8;

	sys_mbox_t mbox = kmem_alloc( sizeof(*mbox) + sizeof(void*) * size );
	mbox->size = size;
	mbox->read = mbox->write = 0;

	return mbox;
}


void
sys_mbox_free(
	sys_mbox_t		mbox
)
{
	if( mbox->read != mbox->write )
		printk( "%s: mbox has remaining elements %d/%d\n",
			__func__,
			mbox->read,
			mbox->write
		);

	kmem_free( mbox );
}



err_t
_sys_mbox_post(
	sys_mbox_t		mbox,
	void *			msg,
	int			try_once
)
{
	unsigned long irqstate;

	while(1)
	{
		spin_lock_irqsave( &mbox_lock, irqstate );
		int next = (mbox->write + 1) % mbox->size;
		if( next != mbox->read )
		{
			mbox->msgs[ mbox->write = next ] = msg;
			spin_unlock_irqrestore( &mbox_lock, irqstate );
			return ERR_OK;
		}

		spin_unlock_irqrestore( &mbox_lock, irqstate );

		if( try_once )
			return ERR_MEM;
	}
}


void
sys_mbox_post(
	sys_mbox_t		mbox,
	void *			msg
)
{
	_sys_mbox_post( mbox, msg, 0 );
}


err_t
sys_mbox_trypost(
	sys_mbox_t		mbox,
	void *			msg
)
{
	return _sys_mbox_post( mbox, msg, 1 );
}


u32_t
sys_arch_mbox_tryfetch(
	sys_mbox_t		mbox,
	void **			msg
)
{
	unsigned long irqstate;
	spin_lock_irqsave( &mbox_lock, irqstate );
	const int read = mbox->read;
	if( read == mbox->write )
	{
		spin_unlock_irqrestore( &mbox_lock, irqstate );
		return SYS_MBOX_EMPTY;
	}

	*msg = mbox->msgs[ read ];
	mbox->read = ( read + 1 ) % mbox->size;
	spin_unlock_irqrestore( &mbox_lock, irqstate );
	return 0;
}


u32_t
sys_arch_mbox_fetch(
	sys_mbox_t		mbox,
	void **			msg,
	u32_t			timeout
)
{
	u32_t delay = 0;
	for( ; delay < timeout ; delay++ )
	{
		if( sys_arch_mbox_tryfetch( mbox, msg ) == 0 )
			return delay;
	}

	return SYS_ARCH_TIMEOUT;
}




/* Timeouts */
struct sys_timeouts timeouts[ 32 ];

struct sys_timeouts *
sys_arch_timeouts( void )
{
	return timeouts;
}


/* Threads */
sys_thread_t
sys_thread_new(
	char *			name,
	void 			(*thread)( void * arg ),
	void *			arg,
	int			stacksize,
	int			prio
)
{
	printk( "%s: Hah! %p\n", __func__, thread );
	return 0;
}
