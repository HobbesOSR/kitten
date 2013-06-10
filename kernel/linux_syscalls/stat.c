#include <lwk/kfs.h>

int
sys_stat( const char *path, uaddr_t buf)
{
	int ret = -EBADF;
__lock(&_lock);
	struct inode * const inode = kfs_lookup(NULL, path, 0);
	if( !inode )
		goto out;

	ret = kfs_stat(inode, buf);
out:
__unlock(&_lock);
	return ret;
}
