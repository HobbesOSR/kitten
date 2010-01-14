#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/mman.h>
#include <lwk/kfs.h>

long
sys_munmap(
	unsigned long addr,
	size_t len
)
{
	struct aspace *as = current->aspace;

	/* TODO: add a million checks here that we'll simply ignore now */

	/* TODO: this won't munmap() anonymous mapping - need to take care
	   of those, too, otherwise we leak memory. */

	spin_lock(&as->lock);
	__aspace_del_region(as, addr, len);
	spin_unlock(&as->lock);

	return 0;
}
