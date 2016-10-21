#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int sys_unlink(const char *pathname);

int
hio_unlink(const char *pathname)
{
	if (!syscall_isset(__NR_unlink, current->aspace->hio_syscall_mask))
		return sys_unlink(pathname);

	return hio_format_and_exec_syscall(__NR_unlink, 1, pathname);
}
