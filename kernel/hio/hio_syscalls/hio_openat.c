#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

#define AT_FDCWD (-100)

extern int
sys_openat(int, uaddr_t, int, mode_t);

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

int
hio_openat(int     dirfd,
	   uaddr_t u_pathname,
	   int	   flags,
	   mode_t  mode)
{
	char pathname[MAX_PATHLEN];

	if (strncpy_from_user(pathname, (void *)u_pathname, sizeof(pathname)) < 0)
		return -EFAULT;

	if ( (flags & O_CREAT) ||
	     (!syscall_isset(__NR_openat, current->aspace->hio_syscall_mask)) ||
	     ((dirfd != AT_FDCWD) && fdTableFile(current->fdTable, dirfd)) ||
	     (hio_is_local_pathname(pathname))
	   )
		return sys_openat(dirfd, u_pathname, flags, mode);

	return hio_format_and_exec_syscall(__NR_openat, 4, dirfd, u_pathname, flags, mode); 
}
