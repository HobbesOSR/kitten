#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_getpid(void);

long
hio_getpid(void)
{
	if (!syscall_isset(__NR_getpid, current->aspace->hio_syscall_mask))
		return sys_getpid();

	return hio_format_and_exec_syscall(__NR_getpid, 0);
}
