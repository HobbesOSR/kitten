#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

int
hio_statfs(const char * path,
           void *       buf)
{
	if ( !syscall_isset(__NR_statfs, current->aspace->hio_syscall_mask) )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_statfs, 2, path, buf);
}
