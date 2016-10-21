#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int sys_fstat(int, uaddr_t);

int
hio_fstat(int     fd,
          uaddr_t buf)
{
	int ret;
	printk("%s: calling hio_fstat on fd %d %p\n", __func__, fd, fdTableFile(current->fdTable, fd) );
	if ((!syscall_isset(__NR_fstat, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   ) {
		return sys_fstat(fd, buf);
	}

	printk("%s: formatting and execing fstat fd %d buf %p\n", __func__, fd, buf);
	ret = hio_format_and_exec_syscall(__NR_fstat, 2, fd, buf);
	return ret;
}
