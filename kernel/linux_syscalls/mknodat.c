#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

int
sys_mknodat(int dirfd, uaddr_t u_pathname, mode_t  mode, dev_t dev)
{
	char pathname[ MAX_PATHLEN ];
	struct inode * root_inode = NULL;

	if( strncpy_from_user( pathname, (void*) u_pathname,
                               sizeof(pathname) ) < 0 )
		return -EFAULT;

	dbg( "name='%s' \n", pathname);


	if ((pathname[0] == '/') ||
	    (dirfd       == AT_FDCWD)) {
	    root_inode = kfs_root;
	} else {
	    struct file * root_file = get_current_file(dirfd);
	    
	    if (!root_file) {
		return -EBADF;
	    }

	    root_inode = root_file->inode;
	}

	extern int mkfifo_at( struct inode *, const char*, mode_t );

__lock(&_lock);
	int ret = mkfifo_at( root_inode, pathname, 0777 );
__unlock(&_lock);

	return ret;
}


