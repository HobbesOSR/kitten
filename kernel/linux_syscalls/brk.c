#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>

unsigned long
sys_brk(unsigned long brk)
{
	struct aspace *as = current->aspace;

	if ((brk >= as->heap_start) && (brk < as->mmap_brk))
		as->brk = brk;

	return as->brk;
}

