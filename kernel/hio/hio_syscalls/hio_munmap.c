#include <arch/uaccess.h>
#include <arch/mman.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long
sys_munmap(unsigned long addr, size_t len);

long
hio_munmap(unsigned long addr, size_t len)
{
	long ret;

	/* This is a bit of a hack, but the clearest way to tell what
	 * to do with this is just look at the addr.
	 * If it is less than SMARTMAP_ALIGN, it was locally created.
	 * Else, it must have been created remotely
	 */
	if ( ((addr + len) <= SMARTMAP_ALIGN) ||
	     (!syscall_isset(__NR_munmap, current->aspace->hio_syscall_mask))
	   )
		return sys_munmap(addr, len);

	ret = hio_format_and_exec_syscall(__NR_munmap, 2, addr, len); 

	/* Remove the locally-created mapping for this region as well */
	if (ret == 0)
		sys_munmap(addr, len);

	return ret;
}
