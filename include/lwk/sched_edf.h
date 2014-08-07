/*
 * Copyright (c) 2013, Oscar Mondragon <omondrag@cs.unm.edu>
 * Copyright (c) 2013, Patrick G. Bridges <bridges@cs.unm.edu>
 * Copyright (c) 2013, Kitten Lightweight Kernel <https://software.sandia.gov/trac/kitten>
 * All rights reserved.
 *
 * Author: Oscar Mondragon <omondrag@cs.unm.edu>
 *         Patrick G. Bridges <bridges@cs.unm.edu>
 */

#ifndef _LWK_SCHED_EDF_H
#define _LWK_SCHED_EDF_H


#include <lwk/task.h>
#include <lwk/rbtree.h>
#include <lwk/spinlock.h>
#include <lwk/time.h>

/* Overview
 *
 * EDF Scheduling
 *
 * The EDF scheduler uses a dynamic calculated priority as scheduling criteria to 
 * choose * what thread (e.g. Kitten task or Palacios vcore) will be scheduled. That
 * priority is calculated according with the relative deadline of the threads that 
 * are ready to run in the runqueue. This runqueue is a per-CPU data structure used 
 * to keep the runnable threads allocated to that CPU. The threads with less time 
 * before its deadline will receive better priorities. The runqueue is sorted each 
 * time that a thread becomes runnable or when its period is over. At that time the 
 * thread is enqueue. A new scheduling decision is taken each time that an 
 * interrupt is received. At that time if the earliest deadline thread is different
 * that the current thread, the current thread goes to sleep and the earliest 
 * deadline thread is woken up. Each time a thread is scheduled, the parameter 
 * "used_time" is set to zero and the current deadline is calculated using its 
 * period. One thread uses at least slice seconds of CPU. Some extra time may be 
 * allocated to that virtual core after all the other virtual cores consumes their 
 * slices.
 */

// Default configuration values for the EDF Scheduler
// time parameters in microseconds

#define MAX_PERIOD 1000000000
#define MIN_PERIOD 500000
#define MAX_SLICE 1000000000
#define MIN_SLICE 200000
#define MIN_SPEED_KHZ  500000
#define CPU_PERCENT 150 /* It should be 100. Temporarly set to 150 because running the hypervisor test with EDF may fail because there is a moment in which the init_task is running at the same time with the Vcores and they will not be admited if the utilization is greater than 100, need to be fixed*/

#define DEADLINE_INTERVAL 10000000 // Period in which the missed deadline ratio is checked

typedef uint64_t time_us;

/*
 * Scheduler configuration
 */

struct edf_sched_config {
    time_us min_slice;       // Minimum allowed slice
    time_us max_slice;       // Maximum allowed slice
    time_us min_period;      // Minimum allowed period
    time_us max_period;      // Maximum allowed period
    int cpu_percent;         // Percentange of CPU utilization for the scheduler in each physical CPU (100 or less)

};

/*
 * Run queue structure. Per-CPU data structure used to keep the runnable threads allocated to that CPU
 * Contains a pointer to the red black tree, the data structure of configuration options, and other info
 */

struct edf_rq {
    int cpu_u;	                                // CPU utilization (<= cpu_percent)
    struct rb_root tasks_tree;	                // Red-Black Tree
    struct edf_sched_config edf_config;	        // Scheduling config structure
    struct task_struct *curr_task;	        // Current running CPU
    time_us start_time;                         // Time at which first core started running in this runqueue
};

/*
 * Basic functions for scheduling
 */

int __init edf_sched_init_runqueue(struct edf_rq *, int cpu_id);
int edf_sched_deinit(void);
int edf_sched_add_task(struct edf_rq *, struct task_struct *info);
int edf_sched_del_task(struct edf_rq *, struct task_struct *info);
int edf_adjust_schedule(struct edf_rq *, struct task_struct *info);
struct task_struct * edf_schedule(struct edf_rq *, struct list_head *, ktime_t *t);
extern void edf_sched_cpu_remove(struct edf_rq *, void *);
ktime_t edf_schedule_timeout(ktime_t nsec);
int set_wakeup_task(struct edf_rq *, struct task_struct *task);
int edf_sched_admit(struct task_struct *info);
int edf_sched_remap(struct task_struct *info, int cpu_id);
int edf_sched_dvfs(struct task_struct *info, int speed);

#endif
