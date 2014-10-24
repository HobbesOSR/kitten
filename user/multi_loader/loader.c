#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <lwk/liblwk.h>


// The linker sets this up to point to the embedded app1 and app2 ELF images
extern int _binary_loader_rawdata_start  __attribute__ ((weak)); // app1
extern int _binary_loader_rawdata2_start __attribute__ ((weak)); // app2


int
create_file(const char *filename, const void *contents, size_t nbytes)
{
	int fd;
	ssize_t rc;

	if ((fd = open(filename, (O_RDWR | O_CREAT))) == -1) {
		printf("create_file: open failed\n");
		return -1;
	}

	printf("nbytes=%d\n",(int) nbytes);
	if ((rc = write(fd, contents, nbytes)) == -1) {
		printf("create_file: write failed\n");
		return -1;
	}

	if (close(fd) != 0) {
		printf("create_file: close failed\n");
		return -1;
	}

	return 0;
}


int
main(int argc, char *argv[], char *envp[])
{
	volatile vaddr_t app1_elf_image = (vaddr_t) &_binary_loader_rawdata_start;
	volatile vaddr_t app2_elf_image = (vaddr_t) &_binary_loader_rawdata2_start;
	start_state_t *start_state;
	cpu_set_t cpuset;
	int num_ranks=0;
	int start_core=0;
	int status;
	int cpu, rank;
	int src, dst;

	// Create some files. The app(s) launched by
	// this loader will have access to these files.
	printf("Creating /tmp/file1 and /tmp/file2\n");
	const char *file1 = "Hi, I'm file 1!\n";
	create_file("/tmp/file1", file1, strlen(file1));

	const char *file2 = "Hi, I'm file 2!\n";
	create_file("/tmp/file2", file2, strlen(file2));

	// Figure out how many CPUs are available
	printf("Available cpus:\n    ");
	pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
	for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
		if (CPU_ISSET(cpu, &cpuset)) {
			printf("%d ", cpu);
			++num_ranks;
		}
	}
	printf("\n");

	// As long as there is more than one core, don't launch app on CPU 0
	if (num_ranks > 1) {
		--num_ranks;
		start_core=1;
	}

	// Allocate a start_state structure for each rank
	start_state = malloc(num_ranks * sizeof(start_state_t));
	if (!start_state) {
		printf("malloc of start_state[] failed\n");
		exit(-1);
	}

	// Create an address space for each rank, one per CPU, other than core 0
	printf("Creating address spaces...\n");
	for (cpu = start_core, rank = 0; cpu <= CPU_SETSIZE; cpu++, rank++) {
		if (!CPU_ISSET(cpu, &cpuset))
			continue;

		start_state[rank].task_id  = 0x1000 + rank;
		start_state[rank].cpu_id   = cpu;
		start_state[rank].user_id  = 1; // anything but 0(=root)
		start_state[rank].group_id = 1;

		sprintf(start_state[rank].task_name, "RANK%d", rank);

		// Run app1 on first half of cores, app 2 on the rest
		status =
		elf_load(
			(rank < num_ranks/2)
			  ? (void *)app1_elf_image
			  : (void *)app2_elf_image,
			"app1",
			0x1000 + rank,
			VM_PAGE_4KB,
			(1024 * 1024 * 16),  // heap_size  = 16 MB
			(1024 * 256),        // stack_size = 256 KB
			"",                  // argv_str
			"",                  // envp_str
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

	// Create SMARTMAP mappings for each rank
	printf("Creating SMARTMAP mappings...\n");
	for (dst = 0; dst < num_ranks; dst++) {
		for (src = 0; src < num_ranks; src++) {
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

	// Create a task for each rank
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

	// Go to sleep "forever"
	while (1)
		sleep(100000);
}
