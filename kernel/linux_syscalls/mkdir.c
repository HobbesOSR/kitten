#include <lwk/kfs.h>
#include <arch/uaccess.h>

int
sys_mkdir(uaddr_t u_pathname,
	  mode_t  mode)
{
        char pathname[ MAX_PATHLEN ];
        if( strncpy_from_user( pathname, (void*) u_pathname,
                               sizeof(pathname) ) < 0 )
                return -EFAULT;

	dbg( "name=`%s' mode %x\n", pathname, mode);

__lock(&_lock);
        struct inode* i = kfs_mkdir( pathname, mode );
__unlock(&_lock);

        if ( ! i ) return ENOENT;
        return 0;
}
