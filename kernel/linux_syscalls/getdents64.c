#include <lwk/kfs.h>
#include <lwk/stat.h>

int
sys_getdents64(unsigned int fd, uaddr_t dirp, unsigned int count)
{
	int ret = -EBADF;
	struct file * const file = get_current_file( fd );

	if(NULL == file)
		goto out;

	if(!S_ISDIR(file->inode->mode)) {
		ret = -ENOTDIR;
		goto out;
	}

	ret = file->f_op->readdir(file, dirp, count, kfs_fill_dirent64);
out:

	return ret;
}


