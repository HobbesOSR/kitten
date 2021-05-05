#include <lwk/kfs.h>
#include <arch-generic/fcntl.h>

int
sys_pipe2(int fd[2], int flags)
{
	int ret = -EFAULT;
	int fd_read, fd_write;

	if (flags == 0) {
		return sys_pipe(fd);
	} else {
		printk("Unimplemented pipe2 flags (%x)\n", flags);
		return -EINVAL;
	}

}

