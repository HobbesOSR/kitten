#include <lwk/kfs.h>
#include <arch/uaccess.h>

int
sys_mknod(uaddr_t u_pathname, mode_t  mode, dev_t dev)
{
	char pathname[ MAX_PATHLEN ];
	if( strncpy_from_user( pathname, (void*) u_pathname,
                               sizeof(pathname) ) < 0 )
		return -EFAULT;

	dbg( "name='%s' \n", pathname);

	extern int mkfifo( const char*, mode_t );

__lock(&_lock);
	int ret = mkfifo( pathname, 0777 );
__unlock(&_lock);

	return ret;
}


