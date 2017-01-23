#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_set_tid_address(int __user *tidptr);

long
hio_set_tid_address(int __user *tidptr)
{
	if (!syscall_isset(__NR_set_tid_address, current->aspace->hio_syscall_mask))
		return sys_set_tid_address(tidptr);

	return hio_format_and_exec_syscall(__NR_set_tid_address, 1, tidptr);
}
