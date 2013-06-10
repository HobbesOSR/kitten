#include <lwk/kfs.h>

int
sys_dup2(int oldfd,
	 int newfd)
{
	int ret = -EBADF;
	struct file * const oldfile = get_current_file( oldfd );

//	dbg("oldfd=%d newfd=%d\n",oldfd,newfd);
	if( !oldfile )
		goto out;

	if( newfd < 0 || newfd > MAX_FILES )
		goto out;

//	sys_close( newfd );

//	atomic_inc( &oldfile->f_count );

	fdTableInstallFd( current->fdTable, newfd, oldfile );
out:
	return ret;
}


