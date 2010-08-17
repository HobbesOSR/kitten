
#include <lwk/kernel.h>
#include <lwk/poll.h>
#include <arch/uaccess.h>
#include <lwk/kfs.h>
#include <lwk/sched.h>
#include <arch/unistd.h>
#include <lwk/spinlock.h>

#define FIFO_SIZE               PAGE_SIZE

#undef _KDBG
#define _KDBG(fmt,args...)

struct fifo_buffer {
	unsigned char *buf;
	unsigned int rIndex, wIndex, avail, length;
	spinlock_t	lock;
};

struct fifo_file {
	waitq_t	poll_wait;
	struct fifo_file*	other;
	struct fifo_buffer*	buffer;
};

struct fifo_inode_priv {
	struct fifo_buffer*	buffer;
	struct fifo_file*	read;
	struct fifo_file*	write;
};

static int buf_write( struct fifo_buffer* pbuf,
				const char __user *ubuf, size_t size )
{
	spin_lock( &pbuf->lock );
	int num = size > ( pbuf->length - pbuf->avail ) ? 
			( pbuf->length - pbuf->avail ) : size; 
	int i = 0;

	while ( i++ < num ) {
       		if ( copy_from_user( pbuf->buf + pbuf->wIndex++, ubuf++, 1 ) ) {
			spin_unlock( &pbuf->lock );
               		return -EFAULT;
		}
		if ( pbuf->wIndex == pbuf->length ) {
			pbuf->wIndex = 0;
		} 
	}
	pbuf->avail += num;
	spin_unlock( &pbuf->lock );
	return num;
}

static int buf_read( struct fifo_buffer* pbuf, char __user *ubuf, size_t size )
{
	spin_lock( &pbuf->lock );
	int num = size > pbuf->avail ? pbuf->avail : size; 
	int i = 0;

	while ( i++ < num ) {
       		if ( copy_to_user( ubuf++, pbuf->buf + pbuf->rIndex++, 1 ) ) {
			spin_unlock( &pbuf->lock );
               		return -EFAULT;
		}
		if ( pbuf->rIndex == pbuf->length ) {
			pbuf->rIndex = 0;
		} 
	}
	pbuf->avail -= num;
	spin_unlock( &pbuf->lock );
	return num;
}

static struct fifo_buffer* alloc_fifo_buffer( int length ) 
{
	struct fifo_buffer* pbuf = kmem_alloc(sizeof(struct fifo_buffer));

	if ( ! pbuf ) return NULL;

	spin_lock_init(&pbuf->lock);
	pbuf->rIndex = 0;
	pbuf->wIndex = 0;
	pbuf->avail = 0;
	pbuf->length = length;

	if ( ( pbuf->buf = kmem_alloc( length ) ) == NULL ) {
		kmem_free(pbuf);       
		return NULL;
	}
	return pbuf;
}

static ssize_t
read(struct file *filep, char __user *ubuf, size_t size, loff_t* off )
{
	struct fifo_file *file = filep->private_data;
	struct fifo_buffer* pbuf = file->buffer;   
	int num_read = 0;

	_KDBG("id=%d size=%ld\n",current->id,size);

	while(1) {
		int ret = buf_read( pbuf, ubuf, size );

		if ( ret < 0 ) return ret;

		num_read += ret;
		size -= ret;

		// we just freed up buffer space, wake the writer
		waitq_wake_nr( &file->other->poll_wait, 1 );

		if ( size == 0 ) return num_read;
		
		if ( wait_event_interruptible( file->poll_wait,
							pbuf->avail ) ) {
			if ( num_read ) return num_read;
			return -EINTR;
		} 
	}

}

static ssize_t
write(struct file *filep, const char __user *ubuf, size_t size, loff_t* off )
{
	struct fifo_file *file = filep->private_data;
	struct fifo_buffer* pbuf = file->buffer;   
	int num_wrote = 0;

	_KDBG("id=%d size=%ld\n",current->id, size);

	while(1) {
		int ret = buf_write( pbuf, ubuf, size );

		if ( ret < 0 ) return ret;

		num_wrote += ret;
		size -= ret;

		// we just wrote to buffer space, wake the reader 
		waitq_wake_nr( &file->other->poll_wait, 1 );

		if ( size == 0 ) return num_wrote;
		
		if ( wait_event_interruptible( file->poll_wait,
					pbuf->avail != pbuf->length ) ) {
			if ( num_wrote ) return num_wrote;
			return -EINTR;
		} 
	}
}

static unsigned int poll(struct file *filep, struct poll_table_struct *table)
{
	struct fifo_file *pfile = filep->private_data;
	unsigned int mask = 0;
	unsigned int key = 0;
	if ( table ) key = table->key;

	_KDBG("wait table=%p %d %d want events %#x\n",  table,
			pfile->buffer->avail, pfile->buffer->length, key );
	
	poll_wait(filep, &pfile->poll_wait, table);

	if ( pfile->buffer->avail )  {
		mask |= POLLIN; 
		mask |= POLLRDNORM; 
	}

	if ( pfile->buffer->length - pfile->buffer->avail  )  {
		mask |= POLLOUT; 
		mask |= POLLWRNORM; 
	}

	_KDBG("mask=%#x\n",mask);
	return mask;
}

#define O_RDONLY             00
#define O_WRONLY             01

static int open(struct inode * inodep, struct file * filep)
{
	_KDBG("flags %#x\n",filep->f_flags);
	struct fifo_inode_priv* priv = inodep->priv;
	if ( filep->f_flags == O_RDONLY  ) {
		filep->private_data = priv->read;
	} else if ( filep->f_flags == O_WRONLY  ) {
		filep->private_data = priv->write;
	} else {
		return -EINVAL;
	}
	return 0;
}

static int close(struct file *filep)
{
	_KDBG("\n");
	return 0;
}

static struct kfs_fops fifo_fops = {
	.open = open,
	.write = write,
	.read = read,
	.poll = poll,
	.close = close,
};

static struct fifo_inode_priv* create_fifo_priv( void )
{
	struct fifo_inode_priv* priv = kmem_alloc( sizeof( *priv ) ); 
	if ( ! priv ) { 
		return NULL;
	}

	_KDBG("\n");
	priv->read = kmem_alloc(sizeof(struct fifo_file));
	if ( ! priv->read  ) return NULL;

	priv->write = kmem_alloc(sizeof(struct fifo_file));
	if ( ! priv->write  ) return NULL;

	priv->write->buffer = alloc_fifo_buffer( FIFO_SIZE );
	if ( ! priv->write->buffer ) return NULL;

	priv->read->buffer = priv->write->buffer; 

	priv->read->other = priv->write;
	priv->write->other = priv->read;

	waitq_init(&priv->write->poll_wait);
	waitq_init(&priv->read->poll_wait);

	return priv;
}

int mkfifo( const char* name, mode_t mode )
{

	struct inode* inode;
	struct fifo_inode_priv* priv = create_fifo_priv();
	if ( ! priv ) return -ENOMEM;
	_KDBG("\n");
	inode = kfs_create( name, &fifo_fops, mode, priv, sizeof(*priv) );
	if ( ! inode ) {
		 return -ENOMEM;
	}
	
	return 0;
}
