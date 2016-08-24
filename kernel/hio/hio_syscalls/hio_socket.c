#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int
sys_socket(int, int, int);

int
hio_socket(int domain, 
	   int type, 
	   int protocol)
{
	if ( (!syscall_isset(__NR_socket, current->aspace->hio_syscall_mask))
	   )
		return sys_socket(domain, type, protocol);

	return hio_format_and_exec_syscall(__NR_socket, 3,
		domain, type, protocol);
}
