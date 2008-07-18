#include <lwk/kernel.h>
#include <lwk/stat.h>
#include <arch/uaccess.h>

long
sys_fstat(unsigned int fd, struct stat __user *statbuf)
{
	struct stat tmp;

	/* For now only allow stat()'ing stdio */
	if (fd != 1)
		return -EBADF;

	/* TODO: Fix this! */
	tmp.st_dev     = 11;
	tmp.st_ino     = 9;
	tmp.st_mode    = 0x2190;
	tmp.st_nlink   = 1;
	tmp.st_uid     = 0;
	tmp.st_gid     = 0;
	tmp.st_rdev    = 34823;
	tmp.st_size    = 0;
	tmp.st_blksize = 1024;
	tmp.st_blocks  = 0;
	tmp.st_atime   = 1204772189;
	tmp.st_mtime   = 1204772189;
	tmp.st_ctime   = 1204769465;
	
	return copy_to_user(statbuf, &tmp, sizeof(tmp)) ? -EFAULT : 0;
}

