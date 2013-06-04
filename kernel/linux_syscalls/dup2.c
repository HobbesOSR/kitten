#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/fdTable.h>

int
sys_dup2(
	unsigned int 		oldfd,
	unsigned int 		newfd
)
{
	int ret = -EBADF;
	struct file *file = fdTableFile( current->fdTable, oldfd );

	newfd = fdTableGetUnused( current->fdTable );
	fdTableInstallFd( current->fdTable, newfd, file );

	ret = newfd;
	return ret;
}
