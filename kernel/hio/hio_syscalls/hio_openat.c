#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

#define AT_FDCWD (-100)

extern long
sys_openat(int dfd, const char __user *filename, int flags, int mode);

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
hio_openat(int dfd, const char __user *filename, int flags, int mode)
{
	char pathname[MAX_PATHLEN];

	if (strncpy_from_user(pathname, (void *)filename, sizeof(pathname)) < 0)
		return -EFAULT;

	if ( (flags & O_CREAT) ||
	     (!syscall_isset(__NR_openat, current->aspace->hio_syscall_mask)) ||
	     ((dfd != AT_FDCWD) && fdTableFile(current->fdTable, dfd)) ||
	     (hio_is_local_pathname(pathname))
	   )
		return sys_openat(dfd, filename, flags, mode);

	return hio_format_and_exec_syscall(__NR_openat, 4, dfd, filename, flags, mode); 
}
