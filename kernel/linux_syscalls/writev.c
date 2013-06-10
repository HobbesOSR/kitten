#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <lwk/uio.h>

ssize_t
sys_writev(int fd, uaddr_t uvec, int count)
{
	//int orig_fd = fd;
	struct iovec vector;
	struct file * const file = get_current_file( fd );
	ssize_t ret = 0, tret;
	//if (file != NULL && file->pipe_end_type == PIPE_END_WRITE) {
	//	fd = file->pipe_other_fd;
	//	file = get_current_file( fd );
	//}

	if(!file)
		return -EBADF;

	if(count < 0)
		return -EINVAL;

	if(!file->f_op->write) {
		printk( KERN_WARNING "%s: fd %d (%s) has no write operation\n",
			__func__, fd, file->inode->name);
		return -EINVAL;
	}

	while(count-- > 0) {
		if(copy_from_user(&vector, (char *)uvec,
				  sizeof(struct iovec)))
			return -EFAULT;
		uvec += sizeof(struct iovec);
		tret = file->f_op->write(file,
					 (char *)vector.iov_base,
					 vector.iov_len,
					 NULL);
		if(tret < 0)
			return -tret;
		else if(tret == 0)
			break;
		else
			ret += tret;
	}

	return ret;
}


