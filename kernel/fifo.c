
#include <lwk/kernel.h>
#include <lwk/poll.h>
#include <arch/uaccess.h>
#include <lwk/kfs.h>
#include <lwk/sched.h>
#include <arch/unistd.h>

#define FIFO_SIZE               PAGE_SIZE

struct fifo_buffer {
        unsigned char *buf;
        unsigned int rIndex, wIndex, avail, length;
};

struct fifo_file {
	wait_queue_head_t	poll_wait;
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
	int num = size > ( pbuf->length - pbuf->avail ) ? 
			( pbuf->length - pbuf->avail ) : size; 
	int i = 0;

	while ( i++ < num ) {
       		if ( copy_from_user( pbuf->buf + pbuf->wIndex++, ubuf++, 1 ) ) {
               		return -EFAULT;
		}
		if ( pbuf->wIndex == pbuf->length ) {
			pbuf->wIndex = 0;
		} 
	}
	pbuf->avail += num;
	return num;
}

static int buf_read( struct fifo_buffer* pbuf, char __user *ubuf, size_t size )
{
	int num = size > pbuf->avail ? pbuf->avail : size; 
	int i = 0;

	while ( i++ < num ) {
       		if ( copy_to_user( ubuf++, pbuf->buf + pbuf->rIndex++, 1 ) ) {
               		return -EFAULT;
		}
		if ( pbuf->rIndex == pbuf->length ) {
			pbuf->rIndex = 0;
		} 
	}
	pbuf->avail -= num;
	return num;
}

static struct fifo_buffer* alloc_fifo_buffer( int length ) 
{
	struct fifo_buffer* pbuf = kmem_alloc(sizeof(struct fifo_buffer));

	if ( ! pbuf ) return NULL;

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

#if 1 
	_KDBG("size=%ld\n",size);
#endif

	while(1) {
		int ret = buf_read( pbuf, ubuf, size );

		if ( ret < 0 ) return ret;

		num_read += ret;
		size -= ret;

		// we just freed up buffer space, wake the writer
		wake_up_interruptible( &file->other->poll_wait );

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

#if 1 
	_KDBG("size=%ld\n",size);
#endif

	while(1) {
		int ret = buf_write( pbuf, ubuf, size );

		if ( ret < 0 ) return ret;

		num_wrote += ret;
		size -= ret;

		// we just wrote to buffer space, wake the reader 
		wake_up_interruptible( &file->other->poll_wait );

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


#if 0
	_KDBG("wait table=%p %d %d want events %#x\n",  table,
			pfile->buffer->avail, pfile->buffer->length, key );
#endif

        poll_wait(filep, &pfile->poll_wait, table);

	if ( pfile->buffer->avail )  {
		if ( key & POLLIN  ) mask |= POLLIN; 
		if ( key & POLLRDNORM  ) mask |= POLLRDNORM; 
	}

	if ( pfile->buffer->length - pfile->buffer->avail  )  {
		if ( key & POLLOUT ) mask |= POLLOUT; 
		if ( key & POLLWRNORM ) mask |= POLLWRNORM; 
	}

	//_KDBG("mask=%#x\n",mask);
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

	init_waitqueue_head(&priv->write->poll_wait);
	init_waitqueue_head(&priv->read->poll_wait);

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
	_KDBG("\n");
		 return -ENOMEM;
	}
	
	return 0;
}

int do_pipe( int fds[] )
{
	struct inode* inode;
	struct fifo_inode_priv* priv = create_fifo_priv();

        inode = kfs_create_inode();
        if ( ! inode ) {
                 return -ENOMEM;
        }
        inode->fops = &fifo_fops;

        fds[0] = get_unused_fd();
        if ( fds[0]  < 0 ) {
        }

        current->files[ fds[0] ] = kfs_open( inode, 0, 0 );
        current->files[ fds[0] ]->private_data = priv->read;

        fds[1] = get_unused_fd();
        if ( fds[1]  < 0 ) {
        }
        current->files[ fds[1] ] = kfs_open( inode, 0 , 0 );
        current->files[ fds[1] ]->private_data = priv->write;

        return 0;
}

long sys_pipe(int __user *fildes)
{
        int fd[2];
        int error;

        //_KDBG("\n");

        if (! ( error = do_pipe(fd) ) ) {
                if ( copy_to_user( fildes, fd, 2 * sizeof(int) ) ) {
                        error = -EFAULT;
                }
        }

        return error;
}
