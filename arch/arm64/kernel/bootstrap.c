#include <lwk/task.h>
#include <lwk/bootstrap.h>
#include <lwk/percpu.h>
#include <lwk/aspace.h>
#include <arch/processor.h>

struct aspace bootstrap_aspace = {
	BOOTSTRAP_ASPACE(bootstrap_aspace)
	.arch = {
		.pgd = (xpte_t *)swapper_pg_dir
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

/**
 * Each CPU gets its own Task State Segment (TSS) structure. Tasks are
 * completely 'soft' in the LWK, no more per-task TSS's and hardware task
 * switching... we switch tasks completely in software. The TSS size is kept
 * cacheline-aligned so they are allowed to end up in the
 * .data.cacheline_aligned section. Since TSS's are completely CPU-local, we
 * want them on exact cacheline boundaries, to eliminate cacheline ping-pong.
 */
#if 0
DEFINE_PER_CPU(struct tss_struct, tss)
____cacheline_internodealigned_in_smp = BOOTSTRAP_TSS;
#endif
