#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int
sys_stat(const char *, uaddr_t);

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
hio_stat(const char * u_pathname,
	 uaddr_t      buf)
{
	char pathname[MAX_PATHLEN];

	if (strncpy_from_user(pathname, (void *)u_pathname, sizeof(pathname)) < 0)
		return -EFAULT;

	if ( (!syscall_isset(__NR_stat, current->aspace->hio_syscall_mask)) ||
	     (hio_is_local_pathname(pathname))
	   )
		return sys_stat(u_pathname, buf);

	return hio_format_and_exec_syscall(__NR_stat, 2, u_pathname, buf); 
}
