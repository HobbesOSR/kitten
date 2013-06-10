#include <lwk/kfs.h>

off_t
sys_lseek( int fd, off_t offset, int whence )
{
	off_t ret = -EBADF;
	struct file * const file = get_current_file( fd );
	if( !file )
		goto out;

	if( file->f_op->lseek )
		ret = file->f_op->lseek( file, offset, whence );

out:
	return ret;
}


