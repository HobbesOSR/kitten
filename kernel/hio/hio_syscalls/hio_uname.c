#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int
sys_uname(struct utsname __user *name);

int
hio_uname(struct utsname __user *name)
{
	if (!syscall_isset(__NR_uname, current->aspace->hio_syscall_mask))
		return sys_uname(name);

	return hio_format_and_exec_syscall(__NR_uname, 1, name);
}
