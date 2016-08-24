#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int
sys_close(int fd);

int
hio_close(int fd)
{
	if ( (!syscall_isset(__NR_close, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_close(fd);

	return hio_format_and_exec_syscall(__NR_close, 1, fd);
}
