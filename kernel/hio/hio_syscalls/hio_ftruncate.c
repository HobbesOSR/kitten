#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

//extern long ftruncate( unsigned int fd, unsigned long length );

long
hio_ftruncate( unsigned int fd, unsigned long length)
{
	if ( (!syscall_isset(__NR_ftruncate, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   ) {
		return -ENOSYS;
		//return sys_ftruncate(fd, offset, whence);
	}

	return hio_format_and_exec_syscall(__NR_ftruncate, 2, fd, length);
}
