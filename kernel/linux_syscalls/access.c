#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

/** Access system call
 *
 * \todo Implement real uid/gid 
 * \todo Implement group and other permission at all
 */
int
sys_access(const char *pathname, mode_t mode)
{
	if (mode & ~00007)	/* where's F_OK, X_OK, W_OK, R_OK? */
		return -EINVAL;

	struct inode * inode = kfs_lookup(kfs_root, pathname, 0);
	if (!inode)
		return -ENOENT;

	//FIXME: This doesn't look at the effective uid/gid ..
	//FIXME: This only looks at the uid
	if ( ((inode->mode >> 6) & mode) == mode) return 0;
	else return -1;
}
