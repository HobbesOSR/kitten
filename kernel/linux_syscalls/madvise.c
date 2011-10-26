#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/mman.h>

long
sys_madvise(unsigned long start, size_t length, int advice)
{
	if (advice != MADV_DONTNEED) {
		printk(KERN_WARNING
		       "sys_madvise() advice=%d not supported (task=%u.%u, %s)\n",
		       advice, current->aspace->id, current->id, current->name);
	}

	return 0;
}
