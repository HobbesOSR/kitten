#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>
int
sys_set_rank(
         id_t aspace_id,
         id_t rank_id
         )
{
	int status;

  // look up aspace from addr
  // then get rank id

	return aspace_set_rank(aspace_id, rank_id);
}
