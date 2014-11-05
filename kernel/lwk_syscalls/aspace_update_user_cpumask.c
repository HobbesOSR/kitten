#include <lwk/aspace.h>
#include <arch/uaccess.h>
	
int
sys_aspace_update_user_cpumask(
		  id_t                      id,
		  user_cpumask_t  __user  * user_cpumask
		  )
{
    user_cpumask_t _user_cpumask;
    cpumask_t      cpumask;
    int ret = 0;


    if (current->uid != 0) {
	return -EPERM;
    }
	
    ret = copy_from_user(&_user_cpumask, user_cpumask, sizeof(_user_cpumask));

    if (ret) {
	return -EFAULT;
    }
 
    cpumask = cpumask_user2kernel(&_user_cpumask);

    return aspace_update_cpumask(id, &cpumask);
}

