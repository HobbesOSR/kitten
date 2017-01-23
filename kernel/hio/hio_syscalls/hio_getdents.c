#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long
sys_getdents(unsigned int fd, struct linux_dirent __user *dirent, unsigned int count);

long
hio_getdents(unsigned int fd, struct linux_dirent __user *dirent, unsigned int count)
{
	if ( (!syscall_isset(__NR_getdents, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_getdents(fd, dirent, count);

	return hio_format_and_exec_syscall(__NR_getdents, 3, fd, dirent, count);
}
