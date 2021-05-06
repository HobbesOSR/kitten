#include <lwk/kernel.h>
#include <lwk/task.h>
#include <arch/prctl.h>
#include <arch/uaccess.h>


long
do_arch_prctl(struct task_struct *task, int code, unsigned long addr)
{
	int ret = 0; 
	int doit = task == current;

	switch (code) { 

	case ARCH_SET_TLS:

		printk("Setting TLS To %p\n", addr);
		
		if (addr >= task->arch.addr_limit)
			return -EPERM; 

		task->arch.thread.tp_value = addr;

		break;
	case ARCH_GET_TLS: { 
		unsigned long base; 

		base = task->arch.thread.tp_value;
		ret = put_user(base, (unsigned long __user *)addr); 
		break; 
	}

	default:
		ret = -EINVAL;
		break;
	} 

	return ret;	

}


long
sys_arch_prctl(int code, unsigned long addr)
{
	return do_arch_prctl(current, code, addr);
}

