/*
 * Copyright (c) 2014, Oscar Mondragon <omondrag@cs.unm.edu>
 * Copyright (c) 2014, Patrick G. Bridges <bridges@cs.unm.edu>
 * Copyright (c) 2014, Kitten Lightweight Kernel <https://software.sandia.gov/trac/kitten>
 * All rights reserved.
 *
 * Author: Oscar Mondragon <omondrag@cs.unm.edu>
 *         Patrick G. Bridges <bridges@cs.unm.edu>
 */

#include <lwk/sched_edf.h>
#include <lwk/sched.h>
#include <lwk/task.h>
#include <lwk/percpu.h>
#include <lwk/smp.h>                 //this_cpu
#include <lwk/cpuinfo.h>
#include <lwk/timer.h>
#include <lwk/params.h>
#include <lwk/preempt_notifier.h>
#include <lwk/kthread.h>
#include <lwk/list.h>
#include <lwk/sched_rr.h>

#define task_container_of(rb_node) container_of(container_of(rb_node, struct task_edf, node), struct task_struct, edf)
/*
 * Interaction with the round-robin scheduler:
 * - EDF tasks have task->period > 0
 * - Tasks without this set are pure round-robin tasks that we do not
 *   schedule and are run by the round-robin scheduler if there are no
 *   EDF deadlines outstanding.
 * - EDF tasks can be scheduled by the EDF scheduler - we use rr_add_task to
 *   put such tasks on to the round robin scheduler and keep a list of tasks
 *   owned by the EDF scheduler we have given temporarily to the round robin
 *   scheduler
 */

/*
 * init_edf_config: Initialize scheduler configuration
 */

static void
init_edf_config(struct edf_sched_config *edf_config){

	edf_config->min_slice = MIN_SLICE;
	edf_config->max_slice = MAX_SLICE;
	edf_config->min_period = MIN_PERIOD;
	edf_config->max_period = MAX_PERIOD;
	edf_config->cpu_percent = CPU_PERCENT;
}

static uint64_t
get_curr_host_time(void)
{
        uint64_t curr_time_us = get_time()/1000; // Time is returned by get_time() in ns and converted to us
        return curr_time_us;
}


/*
 * is_admissible_task: Decides if a task is admited to the red black tree according with
 * the admisibility formula.
 */

static bool
is_admissible_task(struct task_struct * new_sched_task, struct edf_rq *runqueue){

	int curr_utilization = runqueue->cpu_u;
	int new_utilization = curr_utilization + (100 * new_sched_task->edf.slice / new_sched_task->edf.period);
	int cpu_percent = runqueue->edf_config.cpu_percent;

	if (new_utilization <= cpu_percent)
		return true;
	else
		return false;

	return true;
}


/*
 * insert_task_edf: Finds a place in the tree for a newly activated task, adds the node
 * and rebalaces the tree
 */

static bool
insert_task_edf(struct task_struct *task, struct edf_rq *runqueue){

	struct rb_node **new_rb = &(runqueue->tasks_tree.rb_node);
	struct rb_node *parent = NULL;
	struct task_struct *curr_task;

	// Find out place in the tree for the new core
	while (*new_rb) {

		curr_task = task_container_of(*new_rb);
		parent = *new_rb;

		if (task->edf.curr_deadline < curr_task->edf.curr_deadline)
			new_rb = &((*new_rb)->rb_left);
		else if (task->edf.curr_deadline > curr_task->edf.curr_deadline)
			new_rb = &((*new_rb)->rb_right);
		else // It is Possible to have same current deadlines in both tasks!
			return false;
	}
	// Add new node and rebalance tree.
	rb_link_node(&task->edf.node, parent, new_rb);
	rb_insert_color(&task->edf.node, &runqueue->tasks_tree);

	return true;
}

/*
 * next_start_period: Given the current host time and the period of a given task,
 * calculates the time in which its next period starts.
 *
 */

static uint64_t
next_start_period(uint64_t curr_time_us, struct task_struct *task){

	uint64_t period_us = task->edf.period;
	uint64_t next_start_us = 0;

#ifdef CONFIG_SCHED_EDF_WC
	next_start_us = task->edf.curr_deadline + period_us;
#else
	uint64_t time_period_us = curr_time_us % period_us;
	uint64_t remaining_time_us = period_us - time_period_us;
	next_start_us = curr_time_us + remaining_time_us;
#endif
	return next_start_us;
}

/*
 * edf_sched_cpu_remove: Adapted rr_sched_cpu_remove (rr_sched.c)
 */

/* set the offlining flag and migrate all tasks away
 * The idle task will complete the offline operation when it runs
 */
void edf_sched_cpu_remove(struct edf_rq *runqueue, void * arg)
{
	struct task_struct * task = NULL;
	struct rb_node *node;

	node = rb_first(&runqueue->tasks_tree);

	while(node){
		task = task_container_of(node);
		kthread_bind(task, 0);
		cpu_clear(this_cpu, task->aspace->cpu_mask);
		node = rb_next(node);
	}
}

/*
 * set_wakeup_task: Properly set EDF params when a task is woken up
 */

int
set_wakeup_task(struct edf_rq *runqueue, struct task_struct *task){

	time_us host_time = get_curr_host_time();
	task->edf.last_wakeup = host_time;
	runqueue->curr_task = task;

	return 0;
}



/*
 * activate_task - Moves a task to the red-black tree.
 * used time is set to zero and current deadline is calculated
 */

static int
activate_task(struct task_struct * task, struct edf_rq *runqueue)
{
	if (is_admissible_task(task, runqueue)){

		uint64_t curr_time_us = get_curr_host_time();
		uint64_t curr_deadline = next_start_period(curr_time_us, task);

		task->edf.curr_deadline = curr_deadline;
		task->edf.used_time=0;

		bool ins = insert_task_edf(task, runqueue);
		/*
		 * If not inserted is possible that there is other task
		 * with the same deadline.
		 * Then, the deadline is modified and try again
		 */
		while(!ins){
			task->edf.curr_deadline ++;
			ins = insert_task_edf(task, runqueue);
		}

		if (task->state == TASK_RUNNING) {
			task->edf.cpu_reservation = 100 * task->edf.slice / task->edf.period;
			runqueue->cpu_u += task->edf.cpu_reservation;
		} else {
			task->edf.cpu_reservation = 0;
		}

		if(!runqueue->curr_task){
			runqueue->start_time =  get_curr_host_time();
		}

	}
	else
		printk(KERN_INFO "EDF_SCHED. activate_task. CPU cannot activate the core. It is not admissible\n");
	return 0;

}


/*
 * edf_sched_task_init: Initializes per task data structure (task_edf_sched) and
 * calls activate function.
 */


int
edf_sched_add_task(struct edf_rq *runqueue, struct task_struct * task){

	//printk("EDF_SCHED. Adding Task: id %d name %s to CPU %d\n",task->id,task->name,this_cpu);
	activate_task(task, runqueue);
	return 0;
}


/*
 * search_task_edf: Searches a task in the red-black tree by using its curr_deadline
 */
static struct task_struct *
search_task_edf(time_us curr_deadline, struct edf_rq *runqueue){

	struct rb_node *node = runqueue->tasks_tree.rb_node;

	while (node) {
		struct task_struct *task = task_container_of(node);

		if (curr_deadline < task->edf.curr_deadline)
			node = node->rb_left;
		else if (curr_deadline > task->edf.curr_deadline)
			node = node->rb_right;
		else
			return task;
	}
	return NULL;
}


/*
 * delete_task_edf: Deletes a task from the red black tree, generally, when
 * its period is over
 */

static bool
delete_task_edf( struct task_struct *task_edf, struct edf_rq *runqueue){

	struct task_struct *task = search_task_edf(task_edf->edf.curr_deadline,
						   runqueue);
	if (task){
		rb_erase(&task->edf.node, &runqueue->tasks_tree);
		return true;
	}
	else{
		//printk(KERN_INFO "EDF_SCHED. delete_task_edf.Attempted to erase unexisting task\n");
		return false;
	}
}


/*
 * deactivate_task - Removes a task from the red-black tree.
 */

static void
deactivate_task(struct task_struct * task, struct edf_rq *runqueue){

	/*
	 time_us host_time = get_curr_host_time();
	 time_us time_prints=0;
	 int deadlines=0;
	 int deadlines_percentage=0;
	*/

	/*Task Statistics*/
	/*if(task->edf.print_miss_deadlines == 0)
		task->edf.print_miss_deadlines = host_time;
	time_prints = host_time - task->edf.print_miss_deadlines;
	if(time_prints >= DEADLINE_INTERVAL && task->state == TASK_RUNNING){
	 deadlines = time_prints / task->edf.period;
	 deadlines_percentage = 100 * task->edf.miss_deadlines / deadlines;
	 printk(KERN_INFO
	 "EDF_SCHED. rq_U %d, Tsk: id %d, name %s, used_t %llu, sl %llu, T %llu,  miss_dl_per %d\n",
	 runqueue->cpu_u,
	 task->id,
	 task->name,
	 task->edf.used_time,
	 task->edf.slice,
	 task->edf.period,
	 deadlines_percentage);
	 task->edf.miss_deadlines=0;
	 deadlines_percentage = 0;
	 task->edf.print_miss_deadlines=host_time;
	 }*/

	if(delete_task_edf(task, runqueue)){
		runqueue->cpu_u -= task->edf.cpu_reservation;
	}
}

/*
 * Check_deadlines - Check tasks and re-insert in the runqueue the ones which deadline is over
 */


static void check_deadlines(struct edf_rq *runqueue)
{
	struct list_head resched_list;

	struct rb_node *node = rb_first(&runqueue->tasks_tree);
	struct task_struct *next_task;
	time_us host_time = get_curr_host_time();

	list_head_init(&resched_list);
	/* Tasks which deadline is over are re-inserted in the rbtree*/
	while(node){
		next_task = task_container_of(node);
		if(next_task->edf.curr_deadline < host_time ){
			/* This task's deadline is over - deactivate it */
			list_add(&next_task->edf.sched_link, &resched_list);
			/*This task missed its deadline*/
			if(next_task->edf.used_time < next_task->edf.slice){
				next_task->edf.miss_deadlines++;
			}
		} else {
			/* If current task's deadline is still valid we
			 * do not check more tasks*/
			break;
		}
		node = rb_next(node);
	}

	/* Reinsert tasks which deadline is over into the
	 * tree for its new deadline */
	struct task_struct *task, *tmp;
	list_for_each_entry_safe(task, tmp, &resched_list,
				 edf.sched_link) {

		deactivate_task(task,runqueue);
#ifdef CONFIG_SCHED_EDF_RR
		if (!list_empty(&task->rr.sched_link)) {
			list_del(&task->rr.sched_link);
			list_head_init(&task->rr.sched_link);
		}
#endif
		if ((task->cpu_id == task->cpu_target_id)
		    && (task->state != TASK_EXITED)) {
			activate_task(task,runqueue);
		}
	}
}

/*
 * pick_next_task: Returns the next task to be scheduled from the red black tree
 * if all tasks have consumed their slices, it returns NULL
 */
static struct task_struct *
pick_next_task(struct edf_rq *runqueue){

	struct task_struct *next_task;

	//Pick first earliest deadline task

	struct rb_node *node=NULL;
	struct task_struct * found = NULL;

	check_deadlines(runqueue);

	node = rb_first(&runqueue->tasks_tree);

	// Pick the next task that has not used its whole slice
	while(node){

		next_task = task_container_of(node);
		if ((next_task->edf.used_time < next_task->edf.slice)
		    && (next_task->state == TASK_RUNNING)
		    && (next_task->cpu_id == next_task->cpu_target_id)) {
			found = next_task;
			break;
		}
		node = rb_next(node);
	}
	return found;
}


/*
 * edf_schedule: Pick next task to be scheduled and wake it up
 */

struct task_struct *
edf_schedule(struct edf_rq *runq, struct list_head *migrate_list)
{

	struct task_struct *task;

	task = pick_next_task(runq); /* Pick next task to schedule */

	if (!task)
		return NULL;

	return task;
}

/*
 * adjust_slice - Adjust task parameters values
 */
static int adjust_slice(struct edf_rq * runq,struct task_struct *task){

	uint64_t host_time = get_curr_host_time();
	uint64_t used_time =  host_time - task->edf.last_wakeup;
	int edf_scheduled = 1;
	task->edf.used_time += used_time;

	/* If slice is consumed add the task to the rr_list */
	if(task->edf.used_time >= task->edf.slice){
#ifdef CONFIG_SCHED_EDF_RR
		edf_scheduled=0;
#endif

#ifdef CONFIG_SCHED_EDF_WC
	deactivate_task(task,runq);
	activate_task(task,runq);
#endif
	}
	return edf_scheduled;
}

int
edf_adjust_schedule(struct edf_rq * runq, struct task_struct * task){
	return adjust_slice(runq, task);
}

int
edf_sched_del_task(struct edf_rq *runq, struct task_struct *task){
	deactivate_task(task,runq);
	return 0;
}

/*
 * edf_sched_init: Scheduler initialization function
 */
int
edf_sched_init_runqueue(struct edf_rq *runq, int cpuid)
{
	printk(KERN_DEBUG "EDF_SCHED. edf_sched_init. Initializing CPU %d\n",
	       cpuid);

	runq->tasks_tree = RB_ROOT;
	runq->cpu_u=0;
	runq->curr_task=NULL;
	runq->start_time = 0;
	init_edf_config(&runq->edf_config);

	return 0;
}
