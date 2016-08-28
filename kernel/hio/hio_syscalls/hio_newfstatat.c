#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

int
hio_newfstatat(int     dfd,
               uaddr_t filename,
               uaddr_t statbuf,
               int     flag)
{
	return hio_format_and_exec_syscall(__NR_newfstatat, 4, dfd, filename, statbuf, flag); 
}
