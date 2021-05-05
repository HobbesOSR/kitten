#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

int
sys_mkdirat(
	int dirfd, 
	uaddr_t u_pathname,
	mode_t  mode
)
{
        char pathname[ MAX_PATHLEN ];
	struct inode * root_inode = NULL;

        if( strncpy_from_user( pathname, (void*) u_pathname,
                               sizeof(pathname) ) < 0 )
                return -EFAULT;

	dbg( "name=`%s' mode %x\n", pathname, mode);

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

__lock(&_lock);
        struct inode* i = kfs_mkdir_at(root_inode, pathname, mode );
__unlock(&_lock);

        if ( ! i ) return ENOENT;
        return 0;
}
