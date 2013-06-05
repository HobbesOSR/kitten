#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/fdTable.h>

int
sys_dup(
	unsigned int 		filedes
)
{
	int ret = -EBADF;
	unsigned int newfd;
	struct file *file = fdTableFile( current->fdTable, filedes );

	newfd = fdTableGetUnused( current->fdTable );
	fdTableInstallFd( current->fdTable, newfd, file );

	ret = newfd;
	return ret;
}
