#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_gettid(void);

long
hio_gettid(void)
{
	if (!syscall_isset(__NR_gettid, current->aspace->hio_syscall_mask))
		return sys_gettid();

	return hio_format_and_exec_syscall(__NR_gettid, 0);
}
