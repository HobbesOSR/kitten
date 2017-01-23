#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_newfstatat(int dfd, const char __user *filename, struct stat __user *statbuf, int flag)
{
	return hio_format_and_exec_syscall(__NR_newfstatat, 4, dfd, filename, statbuf, flag); 
}
