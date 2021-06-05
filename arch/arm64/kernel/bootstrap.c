#include <lwk/task.h>
#include <lwk/bootstrap.h>
#include <lwk/percpu.h>
#include <lwk/aspace.h>
#include <arch/processor.h>

struct aspace bootstrap_aspace = {
	BOOTSTRAP_ASPACE(bootstrap_aspace)
	.arch = {
		.pgd       = (xpte_t *)swapper_pg_dir,
	}
};

union task_union bootstrap_task_union
	__attribute__((__section__(".data.bootstrap_task"))) =
		{
			/* Initialize task_union.task_info */
			{
				/* arch independent portion */
				BOOTSTRAP_TASK(bootstrap_task_union.task_info)

				/* x86_64 specific portion */
				.arch = {
					.addr_limit = PAGE_OFFSET
				}
			}
		};


