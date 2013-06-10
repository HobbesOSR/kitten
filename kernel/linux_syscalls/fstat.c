#include <lwk/kfs.h>

int
sys_fstat(int fd, uaddr_t buf)
{
	int ret = -EBADF;
	struct file * const file = get_current_file( fd );
	if( !file )
		goto out;

__lock(&_lock);
	if(file->inode)
		ret = kfs_stat(file->inode, buf);
	else
		printk(KERN_WARNING
		"Attempting fstat() on fd %d with no backing inode.\n", fd);
out:
__unlock(&_lock);
	return ret;
}
