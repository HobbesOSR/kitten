#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_readlink(const char __user *path, char __user *buf, int bufsiz);

long
hio_readlink(const char __user *path, char __user *buf, int bufsiz)
{
	if (!syscall_isset(__NR_readlink, current->aspace->hio_syscall_mask))
		return sys_readlink(path, buf, bufsiz);

	return hio_format_and_exec_syscall(__NR_readlink, 3, path, buf, bufsiz);
}
