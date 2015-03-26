#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <lwk/liblwk.h>

#define SLICE		200000000   //ns
#define PERIOD		440000000   //ns
#define TEST_TIME	100

#define THREAD_STACK_SIZE 4096


static int edf_sched_test(void);

/*
 * This test creates two EDF Tasks on each available CPU.
 * Each task is set with SLICE and PERIOD values. The test runs for
 * TEST_TIME seconds, unless EDF Tasks use 100% of the CPU. In that
 * case the test runs indefinitely. It is recommended to set
 * sched_hz (in sched.c) to 1000 or at least to 100.
 *
 * Note: statistics are printed by the check_statistics function in
 * sched_edf.c. Kitten Earliest Deadline First (EDF) must be selected
 * in the configuration menu.
 */

int
main(int argc, char *argv[], char *envp[])
{
	edf_sched_test();
	sleep(TEST_TIME);

	/* Since the init_task is a non-edf task, this printf is never reached
	 * when the CPU has a 100% of utilization (eg. for EDF-WC). In that case
	 * the CPU is never yielded to the init_task after EDF tasks are launched
	 */

	printf("End of EDF Test\n");

	return 0;
}

static void
simple_task(void){


	/* sleep for one second to allow the creation of other threads
	 * This is needed for EDF-WC since after entering in the while(1)
	 * the CPU is not yielded to the init task (Which is a non-EDF task)
	 */

	sleep(1);
	while(1){
	}
}

static int
start_thread(int id, id_t cpu)
{
        int status;
        id_t aspace_id;
        vaddr_t stack_ptr;
        user_cpumask_t cpumask;

        aspace_get_myid(&aspace_id);
        stack_ptr = (vaddr_t)malloc(THREAD_STACK_SIZE);
        cpus_clear(cpumask);
        cpu_set(cpu, cpumask);

        start_state_t start_state = {
                .task_id	= id,
		.user_id        = 1,
                .group_id       = 1,
                .aspace_id      = aspace_id,
                .entry_point    = (vaddr_t)&simple_task,
                .stack_ptr      = stack_ptr + THREAD_STACK_SIZE,
                .cpu_id         = cpu
        };

        sprintf(start_state.task_name, "cpu%u-task%d", cpu ,id);
	printf(" *** Creating Task %s  on cpu %d\n",
	       start_state.task_name,
	       cpu);

	status = task_create(&start_state, NULL);
	if (status) {
		printf("ERROR: failed to create thread (status=%d) on cpu %d.\n",
		       status,
		       cpu);
		return -1;
        }

	sched_setparams_task(aspace_id, id, SLICE, PERIOD);
	return 0;
}



static int
edf_sched_test(void)
{
	int status;
	unsigned i;
	id_t my_id;
	cpu_set_t cpu_mask;
	pthread_attr_t attr;

	printf("\n EDF TEST BEGIN (Running for %d seconds)\n",TEST_TIME);

	status = aspace_get_myid(&my_id);
	if (status) {
		printf("ERROR: task_get_myid() status=%d\n", status);
		return -1;
	}

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 32);

	printf("  My Linux process id (PID) is: %d\n",  getpid());

	status = pthread_getaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask);
	if (status) {
		printf("    ERROR: pthread_getaffinity_np() status=%d\n", status);
		return -1;
	}

	printf("  My task ID is %u\n", my_id);
	printf("  The following CPUs are in the cpumask:\n    ");
	for (i = CPU_MIN_ID; i <= CPU_MAX_ID; i++) {
		if (CPU_ISSET(i, &cpu_mask))
			printf("%d ", i);
	}
	printf("\n");

	printf("Creating two EDF threads on each CPU:\n");
	for (i = CPU_MIN_ID; i <= CPU_MAX_ID; i++) {
		if (CPU_ISSET(i, &cpu_mask)) {
			printf("CPU %d\n", i);
			start_thread(0x1000+i,i);
			start_thread(0x1100+i,i);
		}
	}

	printf("\n");
	return 0;
}
