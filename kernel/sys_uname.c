#include <lwk/kernel.h>
#include <arch/uaccess.h>

long
sys_uname(struct utsname __user *name)
{
	int err = copy_to_user(name, &linux_utsname, sizeof(*name));
	return err ? -EFAULT : 0;
}

