#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_poll(struct pollfd __user *ufds, unsigned int nfds, int timeout);

long
hio_poll(struct pollfd __user *ufds, unsigned int nfds, int timeout)
{
	if (!syscall_isset(__NR_poll, current->aspace->hio_syscall_mask))
		return sys_poll(ufds, nfds, timeout);

	return hio_format_and_exec_syscall(__NR_poll, 3, ufds, nfds, timeout);
}
