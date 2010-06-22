#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/capability.h>
#include <asm/uaccess.h>

/**
 * kmemdup - duplicate region of memory
 *
 * @src: memory region to duplicate
 * @len: memory region length
 * @gfp: GFP mask to use
 */
void *kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *p;

	p = kmalloc_track_caller(len, gfp);
	if (p)
		memcpy(p, src, len);
	return p;
}
EXPORT_SYMBOL(kmemdup);

void *__krealloc(const void *p, size_t new_size, gfp_t flags)
{
	void *ret;
	size_t ks = 0;

	if (unlikely(!new_size))
		return ZERO_SIZE_PTR;

	if (p)
		ks = 0;//ksize(p);

	if (ks >= new_size)
		return (void *)p;

	ret = kmalloc_track_caller(new_size, flags);
	if (ret && p)
		memcpy(ret, p, ks);

	return ret;
}


void *krealloc(const void *p, size_t new_size, gfp_t flags)
{
	void *ret;

	if (unlikely(!new_size)) {
		kfree(p);
		return ZERO_SIZE_PTR;
	}

	ret = __krealloc(p, new_size, flags);
	if (ret && p != ret)
		kfree(p);

	return ret;
}


int can_do_mlock(void)
{
	return 1;
}

struct mm_struct *get_task_mm(struct task_struct *task)
{
	struct mm_struct *mm;

	//task_lock(task);
	mm = task->mm;
	if (mm) {
	/* needs kitten implementation
	if (task->flags & PF_KTHREAD)
			mm = NULL;
		else
			atomic_inc(&mm->mm_users);
	}
	*/
	}
	//task_unlock(task);
	return mm;
}
void mmput(struct mm_struct *mm)
{
 /* Drop mmap !?*/
}

/* page-writeback.c */
int set_page_dirty(struct page *page)
{
	/* Has kitten dirty pages method ?*/
	return 0;
}

int set_page_dirty_lock(struct page *page)
{
	int ret;

	//lock_page_nosync(page);
	ret = set_page_dirty(page);
	//unlock_page(page);
	return ret;
}
void __iomem *ioremap_wc(resource_size_t phys_addr, unsigned long size)
{
	printk("ioremap_wc :  not implemented\n");
	return NULL; /* ioremap_nocache(phys_addr, size); */
}

