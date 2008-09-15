#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/mman.h>

long
sys_mmap(
	unsigned long addr,
	unsigned long len,
	unsigned long prot,
	unsigned long flags,
	unsigned long fd,
	unsigned long off
)
{
	unsigned long mmap_brk;

	/* For now we only support private/anonymous mappings */
	if (!(flags & MAP_PRIVATE) || !(flags & MAP_ANONYMOUS))
		return -EINVAL;

	if (len != round_up(len, PAGE_SIZE))
		return -EINVAL;

	mmap_brk = current->aspace->mmap_brk;
	mmap_brk = round_down(mmap_brk - len, PAGE_SIZE);

	/* Protect against extending into the UNIX data segment */
	if (mmap_brk <= current->aspace->brk)
		return -ENOMEM;

	current->aspace->mmap_brk = mmap_brk;
	return mmap_brk;
}

