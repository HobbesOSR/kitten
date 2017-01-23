#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long
sys_getdents64(unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count);

long
hio_getdents64(unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count)
{
	if ( (!syscall_isset(__NR_getdents64, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_getdents64(fd, dirent, count);

	return hio_format_and_exec_syscall(__NR_getdents64, 3, fd, dirent, count);
}
