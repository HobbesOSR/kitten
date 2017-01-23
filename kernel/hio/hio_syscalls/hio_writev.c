#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_writev(unsigned long fd, const struct iovec __user *vec, unsigned long vlen);

long
hio_writev(unsigned long fd, const struct iovec __user *vec, unsigned long vlen)
{
	if ( (!syscall_isset(__NR_writev, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_writev(fd, vec, vlen);

	return hio_format_and_exec_syscall(__NR_writev, 3, fd, vec, vlen);
}
