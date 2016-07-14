#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>

int
sys_get_rank(
	id_t			       aspace_id,
	id_t __user *                  rank_id
)
{
	int  status;
	id_t _rank_id;


	if (aspace_id == MY_ID)
		aspace_get_myid(&aspace_id);	

	status = aspace_get_rank(aspace_id, &_rank_id);
	if (status)
		return status;

	if (copy_to_user(rank_id, &_rank_id, sizeof(_rank_id)))
		return -EFAULT;

	return 0;
}
