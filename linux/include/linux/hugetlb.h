#include <linux/rwsem.h>
#include <linux/capability.h>
static inline int is_vm_hugetlb_page(struct vm_area_struct *vma)
{
	/* FIXME No huge pages ?: return vma->vm_flags & VM_HUGETLB; */
	return false;
}
