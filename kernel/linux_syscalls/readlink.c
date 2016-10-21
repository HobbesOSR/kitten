#include <lwk/kfs.h>
#include <lwk/stat.h>
#include <arch/uaccess.h>

ssize_t
sys_readlink(const char __user *path, char __user *buf, size_t bufsiz)
{
	char p[2048];
	size_t len;
	struct inode * const inode = kfs_lookup(NULL, path, 0);
	if(!inode)
		return -ENOENT;
	if(!S_ISLNK(inode->mode) || !inode->link_target)
		return -EINVAL;
	get_full_path(inode->link_target, p);
	len = strlen(p);
	if(len > bufsiz)
		return -ENAMETOOLONG;
	if(copy_to_user(buf, p, len))
		return -EFAULT;
	return len;
}
