#include <lwk/kernel.h>
#include <lwk/task.h>
#include <arch/uaccess.h>

ssize_t
sys_write(unsigned int fd, const char __user * buf, size_t count)
{
	char    kbuf[512];
	size_t  kcount = count;

	/* Protect against overflowing the kernel buffer */
	if (kcount >= sizeof(kbuf))
		kcount = sizeof(kbuf) - 1;

	/* Copy the user-level string to a kernel buffer */
	if (copy_from_user(kbuf, buf, kcount))
		return -EFAULT;
	kbuf[kcount] = '\0';

	if (fd != 1)
	{
#ifdef CONFIG_NETWORK
		/* Assume any non-stdout fd is a socket */
		extern int lwip_write( int, const char *, size_t );
		return lwip_write( fd, kbuf, kcount );
#else
		return -1;
#endif
	}

	/* Write the string to the local console */
	printk(KERN_USERMSG
		"(%s) %s%s",
		current->name,
		kbuf,
		(kcount != count) ? " <TRUNCATED>" : ""
	);

	/* Return number of characters actually printed */
	return kcount;
}

