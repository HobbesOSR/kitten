#include <lwk/aspace.h>
#include <lwk/hio.h>

#include <arch/uaccess.h>

int
sys_aspace_update_user_hio_syscall_mask(
	id_t			     id,
	user_syscall_mask_t __user * user_syscall_mask
)
{
	user_syscall_mask_t _user_syscall_mask;
	syscall_mask_t syscall_mask;

	if (id == MY_ID)
		aspace_get_myid(&id);

	if (copy_from_user(&_user_syscall_mask, user_syscall_mask, sizeof(_user_syscall_mask)))
		return -EFAULT;

	syscall_mask = syscall_mask_user2kernel(&_user_syscall_mask);

	return aspace_update_hio_syscall_mask(id, &syscall_mask);
}
