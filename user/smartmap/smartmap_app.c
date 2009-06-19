#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>


#define DATA_COUNT 10
int data[DATA_COUNT];


/**
 * Magic function for converting local address to (intra-node) remote address.
 */
static inline void *
remote_address(
        unsigned	rank,
        void *		vaddr
)
{
	uintptr_t addr = (uintptr_t) vaddr;
	addr |= ((uintptr_t) (rank+1)) << 39;

	return (void*) addr;
}


/**
 * Returns the local task's rank.
 * This assumes the low 8-bits of the task ID are the rank.
 */
int
get_my_rank(void)
{
	pid_t pid = getpid();
	return (pid & 0xFF);
}


/**
 * Returns the number of ranks on the node, numbered [0,num_ranks).
 */
int
get_num_ranks(void)
{
	int i, num_ranks=0;
	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpuset), &cpuset);

	for (i = 0; i < sizeof(cpuset) * 8; i++) {
		if (CPU_ISSET(i, &cpuset))
			++num_ranks;
	}

	return num_ranks;
}


/**
 * A very simple SMARTMAP test program.
 */
int main( int argc, char **argv )
{
	int rank, i;
	int *datap;

	/* Initialize each rank's data array to contain the rank's ID */
	for (i=0; i<DATA_COUNT; i++)
		data[i] = get_my_rank();

	printf("Simulating barrier by sleeping 3 secs\n");
	sleep(3);

	/* Rank 0 prints out each other rank's data array */
	if (get_my_rank() == 0) {
		for (rank = 1; rank < get_num_ranks(); rank++) {
			datap = (int *)remote_address(rank, &data);

			printf("Data for rank %d:\n", rank);
			for (i = 0; i < DATA_COUNT; i++) {
				printf("data[%d] = %d\n", i, datap[i]);
			}
		}
	}

	/* Sleep "forever" */
	while (1)
		sleep(100000);
}
