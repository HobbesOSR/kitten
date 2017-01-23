#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_readv(unsigned long fd, const struct iovec __user *vec, unsigned long vlen);

long
hio_readv(unsigned long fd, const struct iovec __user *vec, unsigned long vlen)
{
	if ( (!syscall_isset(__NR_readv, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_readv(fd, vec, vlen);

	return hio_format_and_exec_syscall(__NR_readv, 3, fd, vec, vlen);
}
