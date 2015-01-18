#include <lwk/kernel.h>
#include <lwk/task.h>
#include <arch/prctl.h>
#include <arch/uaccess.h>


long
do_arch_prctl(struct task_struct *task, int code, unsigned long addr)
{
	printk("do arch prctl\n");
#if 0
	int ret = 0; 
	int doit = task == current;

	switch (code) { 
	case ARCH_SET_GS:
		if (addr >= task->arch.addr_limit)
			return -EPERM; 

		task->arch.thread.gsindex = 0;
		task->arch.thread.gs = addr;
		if (doit) {
			/* The kernel's %gs is currently loaded, so this
			   call is needed to set the user version. */
			load_gs_index(0);
			ret = checking_wrmsrl(MSR_KERNEL_GS_BASE, addr);
		} 

		break;
	case ARCH_SET_FS:
		/* Not strictly needed for fs, but do it for symmetry
		   with gs */
		if (addr >= task->arch.addr_limit)
			return -EPERM; 

		task->arch.thread.fsindex = 0;
		task->arch.thread.fs = addr;
		if (doit) {
			/* The kernel doesn't use %fs so we can set it
			   directly.  set the selector to 0 to not confuse
			   __switch_to */
			//asm volatile("movl %0,%%fs" :: "r" (0));
			ret = checking_wrmsrl(MSR_FS_BASE, addr);
		}

		break;
	case ARCH_GET_FS: { 
		unsigned long base; 
		if (doit)
			rdmsrl(MSR_FS_BASE, base);
		else
			base = task->arch.thread.fs;
		ret = put_user(base, (unsigned long __user *)addr); 
		break; 
	}
	case ARCH_GET_GS: { 
		unsigned long base;
		unsigned gsindex;
		if (doit) {
 			//asm("movl %%gs,%0" : "=r" (gsindex));
			if (gsindex)
				rdmsrl(MSR_KERNEL_GS_BASE, base);
			else
				base = task->arch.thread.gs;
		}
		else
			base = task->arch.thread.gs;
		ret = put_user(base, (unsigned long __user *)addr); 
		break;
	}

	default:
		ret = -EINVAL;
		break;
	} 

	return ret;	
#endif
	return -EINVAL;
}


long
sys_arch_prctl(int code, unsigned long addr)
{
	return do_arch_prctl(current, code, addr);
}

