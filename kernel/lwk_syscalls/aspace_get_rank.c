#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>

int
sys_get_rank(
	id_t __user *                  rank_id
)
{
	int status;
	id_t _rank_id, my_id;

  aspace_get_myid(&my_id);
  _rank_id = aspace_get_rank(my_id);
  // look up aspace from addr
  // then get rank id

	if (rank_id && copy_to_user(rank_id, &_rank_id, sizeof(_rank_id)))
		return -EFAULT;

	return 0;
}
