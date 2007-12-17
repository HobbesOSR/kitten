#include <lwk/task.h>
#include <lwk/init_task.h>
#include <lwk/percpu.h>
#include <lwk/aspace.h>
#include <arch/processor.h>

struct aspace init_aspace = {
	INIT_ASPACE(init_aspace)
	.arch = {
		.pgd = (xpte_t *) init_level4_pgt
	}
};

union task_union init_task_union
	__attribute__((__section__(".data.init_task"))) =
		{
			/* Initialize task_union.task_info */
			{
				/* arch independent portion */
				INIT_TASK(init_task_union.task_info)

				/* x86_64 specific portion */
				.arch = {
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
DEFINE_PER_CPU(struct tss_struct, init_tss)
____cacheline_internodealigned_in_smp = INIT_TSS;

