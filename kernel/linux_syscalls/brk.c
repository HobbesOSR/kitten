#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>

unsigned long
sys_brk(unsigned long brk)
{
	struct aspace *as = current->aspace;

	spin_lock(&as->lock);
	if ((brk >= as->heap_start) && (brk < as->mmap_brk))
		as->brk = brk;
	spin_unlock(&as->lock);

	return as->brk;
}

