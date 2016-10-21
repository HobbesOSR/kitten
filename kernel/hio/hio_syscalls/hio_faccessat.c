#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

//extern int sys_faccessat(int dirfd, const char *pathname, int mode, int flags);

int
hio_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
	if (!syscall_isset(__NR_faccessat, current->aspace->hio_syscall_mask)) {
		//return sys_faccessat(dirfd, pathname, mode, flags);
		printk("faccessat system call not supported.\n");
		return -ENOSYS;
	}

	return hio_format_and_exec_syscall(__NR_faccessat, 4, dirfd, pathname, mode, flags);
}
