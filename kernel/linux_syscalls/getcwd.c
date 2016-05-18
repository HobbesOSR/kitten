#include <lwk/kernel.h>
#include <arch/uaccess.h>

int
sys_getcwd(char __user *buf, unsigned long size)
{
	char *cwd = "/";
	size_t len = strlen(cwd);

	if (copy_to_user(buf, cwd, len))
		return -EFAULT;

	return len;
}
