
#include <lwk/kernel.h>
#include <lwk/poll.h>
#include <arch/uaccess.h>
#include <lwk/kfs.h>
#include <lwk/sched.h>

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

static int open(struct inode * inodep, struct file * filep)
{
	_KDBG("\n");
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

int mkfifo( const char* name, mode_t mode )
{
	struct inode* inode;
	struct fifo_file *rfile, *wfile;

	_KDBG("\n");
	rfile = kmem_alloc(sizeof(struct fifo_file));
	if ( ! rfile  ) return -ENOMEM;

	wfile = kmem_alloc(sizeof(struct fifo_file));
	if ( ! wfile  ) return -ENOMEM;

	wfile->buffer = alloc_fifo_buffer( FIFO_SIZE );
	if ( ! wfile->buffer ) return -ENOMEM;

	rfile->buffer = wfile->buffer; 

	rfile->other = wfile;
	wfile->other = rfile;

	init_waitqueue_head(&wfile->poll_wait);
	init_waitqueue_head(&rfile->poll_wait);

	_KDBG("\n");
	inode = kfs_create( name, &fifo_fops, mode, 0, 0);	
	if ( ! inode ) {
	_KDBG("\n");
		 return -ENOMEM;
	}
	
	return 0;
}
