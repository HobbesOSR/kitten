#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int
sys_getdents(unsigned int, uaddr_t, unsigned int);

int
hio_getdents(unsigned int  fd,
	     uaddr_t       dirp,
	     unsigned int  count)
{
	if ( (!syscall_isset(__NR_getdents, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_getdents(fd, dirp, count);

	return hio_format_and_exec_syscall(__NR_getdents, 3, fd, dirp, count);
}
