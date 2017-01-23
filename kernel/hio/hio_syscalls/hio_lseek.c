#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_lseek(unsigned int fd, off_t offset, unsigned int origin);

long
hio_lseek(unsigned int fd, off_t offset, unsigned int origin)
{
	if ( (!syscall_isset(__NR_lseek, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_lseek(fd, offset, origin);

	return hio_format_and_exec_syscall(__NR_lseek, 3, fd, offset, origin);
}
