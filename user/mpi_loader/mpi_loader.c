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


// The linker sets this up to point to the embedded app ELF images
extern int _binary_mpi_loader_rawdata_start  __attribute__ ((weak));


// Utility function to pre-populate a file in the ramdisk before starting app
int
create_file(const char *filename, const void *contents, size_t nbytes)
{
	int fd;
	ssize_t rc;

	if ((fd = open(filename, (O_RDWR | O_CREAT), (S_IRUSR|S_IWUSR))) == -1) {
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
	volatile vaddr_t app_elf_image = (vaddr_t) &_binary_mpi_loader_rawdata_start;
	start_state_t *start_state;
	cpu_set_t cpuset;
	int num_ranks=0;
	int start_core=0;
	int status;
	int cpu, rank;
	int src, dst;
	int i;
	char arg_str[1024], env_str[1024], *cur, *end;

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

	// Allocate a start_state structure for each rank
	start_state = malloc(num_ranks * sizeof(start_state_t));
	if (!start_state) {
		printf("malloc of start_state[] failed\n");
		exit(-1);
	}

	// Create an address space for each rank, one per CPU, other than core 0
	printf("Creating address spaces:\n");
	for (cpu = start_core, rank = 0; cpu <= CPU_SETSIZE; cpu++, rank++) {
		if (!CPU_ISSET(cpu, &cpuset))
			continue;

		start_state[rank].task_id  = 0x1000 + rank;
		start_state[rank].cpu_id   = cpu;
		start_state[rank].user_id  = 1; // anything but 0(=root)
		start_state[rank].group_id = 1;
		sprintf(start_state[rank].task_name, "RANK%d", rank);

		// The argv array passed to the loader's main() is passed
		// through to the app, with the exception of argv[0].
		cur = arg_str;
		end = arg_str + sizeof(arg_str);
		for (i = 1; i < argc; i++) {
			cur += snprintf(cur, end-cur, "%s ", argv[i]);
			if (cur >= end) {
				printf("snprintf(arg_str) failed, out of space.\n");
				exit(-1);
			}
		}

		// The envp array passed to the loader's main() is passed
		// through to the app, with PMI_RANK and PMI_SIZE added.
		cur = env_str;
		end = env_str + sizeof(env_str);
		for (i = 0; envp[i] != NULL; i++) {
			cur += snprintf(cur, end-cur, "%s ", envp[i]);
			if (cur >= end) {
				printf("snprintf(env_str) failed, out of space.\n");
				exit(-1);
			}
		}
		cur += snprintf(cur, end-cur, "PMI_RANK=%d PMI_SIZE=%d", rank, num_ranks);
		if (cur >= end) {
			printf("snprintf(env_str) failed, out of space.\n");
			exit(-1);
		}

		status =
		elf_load(
			(void *)app_elf_image,
			"app",
			0x1000 + rank,
			VM_PAGE_4KB,
			(1024 * 1024 * 1024),  // heap_size  = 1 GB
			(1024 * 1024),         // stack_size = 1 MB
			arg_str,               // argv_str
			env_str,               // envp_str
			&start_state[rank],
			0,
			&elf_dflt_alloc_pmem
		);

		if (status) {
			printf("elf_load failed, status=%d\n", status);
			exit(-1);
		}

		printf("    Created aspace for %s\n", start_state[rank].task_name);
	}
	printf("Done.\n");

	printf("Creating SMARTMAP mappings:\n");
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

		printf("    Created SMARTMAP mappings for %s\n", start_state[dst].task_name);
	}
	printf("Done.\n");

	printf("Creating tasks:\n");
	for (rank = 0; rank < num_ranks; rank++) {
		status = task_create(&start_state[rank], NULL);
		if (status) {
			printf("task_create failed, status=%d\n", status);
			exit(-1);
		}

		printf("    Created task for %s\n", start_state[rank].task_name);
	}
	printf("Done.\n");

	printf("LOADER DONE.\n");

	// Go to sleep "forever"
	while (1)
		sleep(100000);
}
