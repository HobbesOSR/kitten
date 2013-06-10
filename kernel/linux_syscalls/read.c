#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <lwk/spinlock.h>

ssize_t
sys_read(int           fd,
	 char __user * buf,
	 size_t        len)
{
	char buffer[PIPE_BUFFER_MAX+1];
	char temp[PIPE_BUFFER_MAX+1];
	unsigned long flags;
	//int orig_fd = fd;
	ssize_t ret;
	struct file * file = get_current_file(fd);

	if (!file) {
		ret = -EBADF;
	} else if (file->pipe != NULL) {
		int result = min((size_t)PIPE_BUFFER_MAX, len);
		//printk("sys_read(%d): trying to read %d\n", orig_fd, result);
		buffer[0] = 0; // make sure it's null-terminated

		int n = 0;

		//printk("sys_read(%d): LOCKING reading via pipe\n", orig_fd);
		spin_lock_irqsave(&file->pipe->buffer_lock, flags);

		while (n < result) {
			// If the buffer is empty, wait until it fills a bit
			while (pipe_buffer_empty(file->pipe)) {
				//printk("sys_read: UNLOCKING and WAITING\n");
				spin_unlock_irqrestore(&file->pipe->buffer_lock, flags);
				if (wait_event_interruptible(file->pipe->buffer_wait,
				     (!pipe_buffer_empty(file->pipe))))
					return -ERESTARTSYS;
				//printk("sys_read: LOCKING\n");
				spin_lock_irqsave(&file->pipe->buffer_lock, flags);
			}

			int num_to_read = min(result-n, file->pipe->amount);
			//printk("sys_read(): read %d = '%s'\n", num_to_read, file->pipe->buffer);
			strncat(buffer+n, file->pipe->buffer, num_to_read);
			strncpy(temp, file->pipe->buffer+num_to_read, PIPE_BUFFER_MAX+1-num_to_read); // XXX
			strncpy(file->pipe->buffer, temp, PIPE_BUFFER_MAX+1);
			buffer[num_to_read] = 0; // make sure it's null-terminated
			n += num_to_read;
			// notify everybody about our deletion from the buffer
			file->pipe->amount -= num_to_read;
			waitq_wakeup(&file->pipe->buffer_wait);
			break;
		}
		result = n;

		int rc = copy_to_user(buf, buffer, result);
		//printk("sys_read(%d): pipe buffer: '%s'\n", orig_fd, file->pipe->buffer);
		//printk("sys_read: UNLOCKING\n");
		spin_unlock_irqrestore(&file->pipe->buffer_lock, flags);
		ret = rc ? -EFAULT : result;
	} else if (file->f_op->read) {
		ret = file->f_op->read( file, (char *)buf, len, NULL );
	} else {
		printk( KERN_WARNING "%s: fd %d (%s) has no read operation\n",
			__func__, fd, file->inode->name );
		ret = -EBADF;
        }

	//printk("sys_read(%d): returning %d\n", fd, ret);
	return ret;
}
