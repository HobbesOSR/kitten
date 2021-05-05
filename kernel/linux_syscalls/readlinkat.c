#include <lwk/kfs.h>
#include <lwk/stat.h>
#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

ssize_t
sys_readlinkat(int dirfd, const char __user *path, char __user *buf, size_t bufsiz)
{
	char p[2048];
	size_t len;
	char pathname[ MAX_PATHLEN ];
	struct inode * inode      = NULL;
	struct inode * root_inode = NULL;
	
	if( strncpy_from_user( pathname, (void*) path, sizeof(pathname) ) < 0 ) {
		printk("bad copy from user\n");

		return -EFAULT;
	}

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

	
	inode = kfs_lookup(root_inode, pathname, 0);
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
