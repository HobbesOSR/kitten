#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/mman.h>
#include <lwk/kfs.h>

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
	struct file *file;
	struct vm_area_struct vma;
	unsigned long mmap_brk;
	int rv;

	if (len != round_up(len, PAGE_SIZE))
		return -EINVAL;

	/* we only support anonymous private mapping; file-backed
	   private mapping has copy-on-write semantics, which we don't
	   want due to complete lack of any pagefaulting resolution */

	if((flags & MAP_PRIVATE) && !(flags & MAP_ANONYMOUS))
		return -EINVAL;

	/* anonymous mappings (not backed by a file) are handled specially */
	if(flags & MAP_ANONYMOUS) {
		/* anonymous mmap()ed memory is put at the top of the
		   heap region, and grows from high to low addresses,
		   i.e. down towards the current heap end. */
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

	/* file-backed mappings */

	/* TODO: add a million checks here that we'll simply ignore now */

	file = get_current_file(fd);
	if(NULL == file)
		return -EBADF;

	if(NULL == file->f_op ||
	   NULL == file->f_op->mmap)
		return -ENODEV;

	spin_lock(&as->lock);
	if ((rv = __aspace_find_hole(as, addr, len, PAGE_SIZE, &addr))) {
		spin_unlock(&as->lock);
		return -ENOMEM;
	}

	if ((rv = __aspace_add_region(as, addr, len,
				      VM_READ|VM_WRITE|VM_USER,
				      PAGE_SIZE, "mmap"))) {
		/* assuming there is no race between find_hole and
		   add_region, as we're holding the as->lock, this
		   failure can't be due to someone adding our region
		   in between */
		spin_unlock(&as->lock);
		return -ENOMEM;
	}
	spin_unlock(&as->lock);

	/* fill the vm_area_struct to keep compatible with linux layer */
	vma.vm_start = addr;
	vma.vm_end = addr + len;
	vma.vm_page_prot = __pgprot(VM_READ|VM_WRITE);
	vma.vm_pgoff = 0;

	rv = file->f_op->mmap(file, &vma);
	if(rv) {
		spin_lock(&as->lock);
		__aspace_del_region(as, addr, len);
		spin_unlock(&as->lock);
		return rv;
	}
	return vma.vm_start;
}
