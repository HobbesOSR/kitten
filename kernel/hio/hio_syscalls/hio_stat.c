#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long
sys_stat(const char __user *filename, struct __old_kernel_stat __user *statbuf);

static int
hio_is_local_pathname(char * pathname)
{
	if (strncmp(pathname, "/dev/xpmem", sizeof("/dev/xpmem")) == 0)
		return true;

	if (strncmp(pathname, "/dev/pisces", sizeof("/dev/pisces")) == 0)
		return true;

	if (strncmp(pathname, "/dev/v3vee", sizeof("/dev/v3vee")) == 0)
		return true;

	return false;
}

long
hio_stat(const char __user *filename, struct __old_kernel_stat __user *statbuf)
{
	char pathname[MAX_PATHLEN];

	if (strncpy_from_user(pathname, (void *)filename, sizeof(pathname)) < 0)
		return -EFAULT;

	if ( (!syscall_isset(__NR_stat, current->aspace->hio_syscall_mask)) ||
	     (hio_is_local_pathname(pathname))
	   )
		return sys_stat(filename, statbuf);

	return hio_format_and_exec_syscall(__NR_stat, 2, filename, statbuf); 
}
