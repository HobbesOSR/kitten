#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <lwk/spinlock.h>

ssize_t
sys_write(int     fd,
	  uaddr_t buf,
	  size_t  len)
{
	unsigned long flags;
	//int orig_fd = fd;
	ssize_t ret;
	char buffer[PIPE_BUFFER_MAX+1];
	struct file * const file = get_current_file(fd);

	//if (file != NULL && file->pipe_end_type == PIPE_END_WRITE) {
	//	fd = file->pipe_other_fd;
	//	file = get_current_file(fd);
	//}

	if (!file) {
		ret = -EBADF;
	} else if (file->pipe != NULL) {
		int result = min((size_t)PIPE_BUFFER_MAX, len);
		if (strncpy_from_user(buffer, (void*) buf, result) < 0)
			return -EFAULT;
		buffer[result] = 0; // make sure it's null-terminated

		int n = 0;

		//printk("sys_write(%d): writing (%d) '%s' via pipe to %d\n", orig_fd, result, buffer, fd);
		//printk("sys_write: LOCKING\n");
		spin_lock_irqsave(&file->pipe->buffer_lock, flags);

		while (n < result) {
			// If the buffer is full, wait until it empties a bit
			while (pipe_buffer_full(file->pipe)) {
				//printk("sys_write: UNLOCKING and waiting\n");
				spin_unlock_irqrestore(&file->pipe->buffer_lock, flags);
				if (wait_event_interruptible(file->pipe->buffer_wait,
				     (!pipe_buffer_full(file->pipe))))
					return -ERESTARTSYS;
				//printk("sys_write: LOCKING\n");
				spin_lock_irqsave(&file->pipe->buffer_lock, flags);
			}

			// write into the pipe
			int num_to_write = min(result-n, pipe_amount_free(file->pipe));
			strncat(file->pipe->buffer, buffer+n, num_to_write);
			file->pipe->buffer[num_to_write] = 0; // make sure it's null-terminated
			n += num_to_write;
			// notify everybody about our addition to the buffer
			file->pipe->amount += num_to_write;
			waitq_wakeup(&file->pipe->buffer_wait);
			//break;
		}
		result = n;

		//printk("sys_write(%d): pipe buffer: '%s'\n", orig_fd, file->pipe->buffer);
		//printk("sys_write: UNLOCKING\n");
		spin_unlock_irqrestore(&file->pipe->buffer_lock, flags);
		ret = result;
	} else if (file->f_op->write) {
		ret = file->f_op->write( file,
					 (const char __user *)buf,
					 len , NULL );
	} else {
		printk( KERN_WARNING "%s: fd %d (%s) has no write operation\n",
		__func__, fd, file->inode->name);
		ret = -EBADF;
	}

	//printk("sys_write(%d): returning %d\n", fd, ret);
	return ret;
}
