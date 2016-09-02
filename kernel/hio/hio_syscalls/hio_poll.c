#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int sys_poll(void *fds, unsigned int nfds, int timeout);

int
hio_poll( void *fds, unsigned int nfds, int timeout )
{
	if ( !syscall_isset(__NR_poll, current->aspace->hio_syscall_mask) ) {
		return sys_poll(fds, nfds, timeout);
	}

	return hio_format_and_exec_syscall(__NR_poll, 3, fds, nfds, timeout);
}
