#define _GNU_SOURCE
#define CONFIG_SCHED_EDF 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <lwk/liblwk.h>
#include <lwk/sched_control.h>

#define SLICE 1000000
#define PERIOD 2200000
#define TEST_TIME 30

#define THREAD_STACK_SIZE 4096

static int cooperative_sched_test1(void);


int
main(int argc, char *argv[], char *envp[])
{
	cooperative_sched_test1();
	sleep(TEST_TIME);
	printf("End of COOPERATIVE SCHEDULING Test\n");

	return 0;
}

static void
simulation_task(void){

	sleep(1);
	while(1){
		sched_yield_task_to(1,0x1000);
		//sched_yield_task_to(-1,-1);
		//sleep(1);
	}
}

static void
analysis_task(void){

	sleep(1);
	while(1){
		sched_yield_task_to(1,0x1100);
		//sched_yield_task_to(-1,-1);
		//sleep(1);
	}
}

static int
start_thread(int id, id_t cpu, vaddr_t entry)
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
                .task_id        = id,
                .user_id        = 1,
                .group_id       = 1,
                .aspace_id      = aspace_id,
                .entry_point    = entry,
                .stack_ptr      = stack_ptr + THREAD_STACK_SIZE,
                .cpu_id         = cpu,
                .edf.slice      = SLICE,
                .edf.period     = PERIOD,
        };

        sprintf(start_state.task_name, "cpu%u-task%d", cpu ,id);

	printf("Procces id (pid) %d\n", getpid());

        printf(" *** Creating Task %s (S: %llu, T: %llu, aspaceid %d tid %d) on cpu %d\n",
               start_state.task_name,
               (unsigned long long)start_state.edf.slice,
               (unsigned long long)start_state.edf.period,
               aspace_id,
               id,
               cpu);

        status = task_create(&start_state, NULL);
        if (status) {
                printf("ERROR: failed to create thread (status=%d) on cpu %d.\n",
                       status,
                       cpu);
                return -1;
        }

        return 0;
}



static int
cooperative_sched_test1(void)
{
	start_thread(0x1100, 1, (vaddr_t)&simulation_task);
	start_thread(0x1000, 1, (vaddr_t)&analysis_task);
	return 0;
}
