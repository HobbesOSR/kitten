#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <lwk/uio.h>

ssize_t
sys_readv(int fd, uaddr_t uvec, int count)
{
	struct iovec vector;
	struct file * const file = get_current_file( fd );
	ssize_t ret = 0, tret;

	if(!file)
		return -EBADF;
	if(count < 0)
		return -EINVAL;
	if(!file->f_op->read) {
		printk( KERN_WARNING "%s: fd %d (%s) has no read operation\n",
			__func__, fd, file->inode->name );
		return -EINVAL;
	}

	while(count-- > 0) {
		if(copy_from_user(&vector, (char *)uvec,
				  sizeof(struct iovec)))
			return -EFAULT;
		uvec += sizeof(struct iovec);
		tret = file->f_op->read(file,
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
