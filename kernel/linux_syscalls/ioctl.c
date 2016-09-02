#include <lwk/kfs.h>

int
sys_ioctl(unsigned int  fd,
	  unsigned int  request,
	  unsigned long arg)
{
	int ret = -EBADF;
	struct file * const file = get_current_file( fd );
	if( !file )
		goto out;

	if( file->f_op->unlocked_ioctl )
		ret = file->f_op->unlocked_ioctl( file, request, arg );

	else if( file->f_op->ioctl )
		ret = file->f_op->ioctl( file, request, arg );

	else {
		printk("sys_ioctl %s : no ioctl!\n",file->inode->name);
		ret = -ENOTTY;
	}
out:
	return ret;
}


