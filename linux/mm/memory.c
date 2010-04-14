#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/pfn.h>

int
get_user_pages(struct task_struct *tsk, struct mm_struct *mm,
               unsigned long start, int len, int write, int force,
               struct page **pages, struct vm_area_struct **vmas)
{
	int i, status;

	if (tsk != current)
		panic("In get_user_pages(): tsk != current.");

	if (mm != current->mm)
		panic("In get_user_pages(): mm != current->mm.");

	/* LWK doesn't have page[] array, so allocate fake page structures */
	if (pages) {
		for (i = 0; i < len; i++)
			pages[i] = NULL;

		for (i = 0; i < len; i++) {
			pages[i] = kmem_alloc(sizeof(struct page));
			if (!pages[i]) {
				status = -ENOMEM;
				goto error;
			}
		}
	}

	if (vmas) {
		/* For now just zero the input vmas[] pointer array,
		 * hopefully causing an error somewhere down the road. */
		memset(vmas, 0, len * sizeof(struct vm_area_struct *));
	}

	/* Traverse the user buffer */
	for (i = 0; i < len; i++) {
		vaddr_t user_addr = start + (i * PAGE_SIZE);
		vaddr_t user_page = round_down(user_addr, PAGE_SIZE);
		paddr_t phys_page;

		if (__aspace_virt_to_phys(current->aspace, user_page, &phys_page)) {
			status = -EFAULT;
			goto error;
		}

		if (pages) {
			pages[i]->virtual = phys_to_virt(phys_page);
			pages[i]->user    = 1;
		}
	}

	return len;

error:
	for (i = 0; i < len; i++) {
		if (pages[i] != NULL)
			kmem_free(pages[i]);
	}
	return status;
}

void
put_page(struct page *page)
{
	kmem_free(page);
}

void *page_address(struct page *page)
{
	return (void *)page->virtual;
}

void *
lowmem_page_address(struct page *page)
{
	return (void *)page->virtual;
}

int
generic_access_phys(struct vm_area_struct *vma, unsigned long addr,
                    void *buf, int len, int write)
{
	panic("In generic_access_phys()... needs to be implemented.");
}

/**
 * remap_pfn_range - remap kernel memory to userspace
 * @vma: user vma to map to
 * @addr: target user address to start at
 * @pfn: physical address of kernel memory
 * @size: size of map area
 * @prot: page protection flags for this mapping
 *
 *  Note: this is only safe if the mm semaphore is held when called.
 *
 * TODO: LWK currently assumes vma corresponds to current, fix this.
 */
int
remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
                unsigned long pfn, unsigned long size, pgprot_t prot)
{
	paddr_t phys_addr = PFN_PHYS(pfn);
	vaddr_t virt_addr = addr;
	int status;

	status = __aspace_map_pmem(current->aspace, phys_addr, virt_addr, size);
	if (status)
		panic("In remap_pfn_range(): __aspace_map_pmem() failed (status=%d).", status);
	return status;
}

void *vmap(struct page **pages, unsigned int count,
                unsigned long flags, pgprot_t prot)
{
	printk("vmap :  %lx %d\n",(unsigned long)pages[0], count);
	return (page_to_virt(pages[0]));
}
void vunmap(const void *addr)
{
}

void *vmalloc(unsigned long size)
{
	printk("vmalloc: not implemented\n");
	return NULL;
}

void vfree(const void *addr)
{
	printk("vfree: not implemented\n");
}

struct page *vmalloc_to_page(const void *vmalloc_addr)
{
	printk("vmalloc_to_page: not implemented\n");
	return NULL;
}

void *vmalloc_user(unsigned long size)
{
	printk("vmalloc_user: not implemented\n");
	return NULL;
}

int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
			unsigned long pgoff)
{
	printk("remap_vmalloc_range: not implemented\n");
	return 0;
}


