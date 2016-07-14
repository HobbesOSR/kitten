#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>
int
sys_set_rank(
         id_t aspace_id,
         id_t rank_id
)
{
	if (aspace_id == MY_ID)
		aspace_get_myid(&aspace_id);

	return aspace_set_rank(aspace_id, rank_id);
}
