#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <lwk/liblwk.h>


/* The linker sets this up to point to the embedded smartmap_app ELF image */
int _binary_loader_rawdata_start __attribute__ ((weak));


int
main(int argc, char *argv[], char *envp[])
{
	volatile vaddr_t elf_image = (vaddr_t) &_binary_loader_rawdata_start;
	start_state_t *start_state;
	user_cpumask_t cpumask;
	int num_ranks=0;
	int status;
	int cpu, rank;
	int src, dst;

	/* Figure out how many CPUs are available */
	printf("Available cpus:\n    ");
	task_get_cpumask(&cpumask);
	for (cpu = CPU_MIN_ID; cpu <= CPU_MAX_ID; cpu++) {
		if (cpu_isset(cpu, cpumask)) {
			printf("%d ", cpu);
			++num_ranks;
		}
	}
	printf("\n");

	/* Allocate a start_state structure for each rank */
	start_state = malloc(num_ranks * sizeof(start_state_t));
	if (!start_state) {
		printf("malloc of start_state[] failed\n");
		exit(-1);
	}

	/* Create an address space for each rank, one per CPU */
	printf("Creating address spaces...\n");
	for (cpu = CPU_MIN_ID, rank = 0; cpu <= CPU_MAX_ID; cpu++, rank++) {
		if (!cpu_isset(cpu, cpumask))
			continue;

		start_state[rank].task_id  = UTASK_MIN_ID + rank;
		start_state[rank].cpu_id   = cpu;
		start_state[rank].user_id  = 1; /* anything but 0(=root) */
		start_state[rank].group_id = 1;
		start_state[rank].cpumask  = cpumask;

		sprintf(start_state[rank].task_name, "RANK%d", rank);

		status =
		elf_load(
			(void *)elf_image,
			"smartmap_app",
			UASPACE_MIN_ID + rank,
			PAGE_SIZE,
			(1024 * 1024 * 16),  /* heap_size  = 16 MB */
			(1024 * 256),        /* stack_size = 256 KB */
			"",                  /* argv_str */
			"",                  /* envp_str */
			&start_state[rank],
			0,
			&elf_dflt_alloc_pmem
		);

		if (status) {
			printf("elf_load failed, status=%d\n", status);
			exit(-1);
		}
	}
	printf("    OK\n");

	/* Create SMARTMAP mappings for each rank */
	printf("Creating SMARTMAP mappings...\n");
	for (dst = 0; dst < num_ranks; dst++) {
		for (src = 0; src < num_ranks; src++) {
			if (src == dst)
				continue;

			status =
			aspace_smartmap(
				start_state[src].aspace_id,
				start_state[dst].aspace_id,
				SMARTMAP_ALIGN + (SMARTMAP_ALIGN * src),
				SMARTMAP_ALIGN
			);

			if (status) {
				printf("smartmap failed, status=%d\n", status);
				exit(-1);
			}
		}
	}
	printf("    OK\n");

	/* Create a task for each rank */
	printf("Creating tasks...\n");
	for (rank = 0; rank < num_ranks; rank++) {
		status = task_create(&start_state[rank], NULL);
		if (status) {
			printf("task_create failed, status=%d\n", status);
			exit(-1);
		}
	}
	printf("    OK\n");

	printf("LOADER DONE.\n");

	/* Go to sleep "forever" */
	while (1)
		sleep(100000);
}
