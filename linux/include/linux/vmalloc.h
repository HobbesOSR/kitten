#define VM_MAP          VM_SMARTMAP /* org linux == 0x00000004 */     /* vmap()ed pages */
#include <linux/mm.h>
extern void *vmalloc(unsigned long size);
extern void *vmalloc_user(unsigned long size);
extern void vfree(const void *addr);

extern void *vmap(struct page **pages, unsigned int count,
		  unsigned long flags, pgprot_t prot);
extern void vunmap(const void *addr);
extern int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
			unsigned long pgoff);
