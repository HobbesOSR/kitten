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
	struct aspace *as = current->aspace;
	unsigned long mmap_brk;

	/* For now we only support private/anonymous mappings */
	if (!(flags & MAP_PRIVATE) || !(flags & MAP_ANONYMOUS))
		return -EINVAL;

	if (len != round_up(len, PAGE_SIZE))
		return -EINVAL;

	spin_lock(&as->lock);
	mmap_brk = round_down(as->mmap_brk - len, PAGE_SIZE);

	/* Protect against extending into the UNIX data segment */
	if (mmap_brk <= as->brk) {
		spin_unlock(&as->lock);
		return -ENOMEM;
	}

	as->mmap_brk = mmap_brk;
	spin_unlock(&as->lock);

	return mmap_brk;
}

