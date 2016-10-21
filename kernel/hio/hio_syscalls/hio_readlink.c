#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern ssize_t sys_readlink(const char *path, char *buf, size_t bufsiz);

ssize_t
hio_readlink(const char *path, char *buf, size_t bufsiz)
{
	if (!syscall_isset(__NR_readlink, current->aspace->hio_syscall_mask))
		return sys_readlink(path, buf, bufsiz);

	return hio_format_and_exec_syscall(__NR_readlink, 3, path, buf, bufsiz);
}
