#include <arch/uaccess.h>
#include <arch/mman.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long
sys_mmap(unsigned long addr, unsigned long len, unsigned long prot,
	 unsigned long flags, unsigned long fd, unsigned long pgoff);

long
hio_mmap(unsigned long addr, unsigned long len, unsigned long prot,
         unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	if ( (flags & MAP_ANONYMOUS) ||
	     (!syscall_isset(__NR_mmap, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_mmap(addr, len, prot, flags, fd, pgoff);

	return hio_format_and_exec_syscall(__NR_mmap, 6, 
		addr, len, prot, flags, fd, pgoff); 
}
