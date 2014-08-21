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

#define task_container_of(rb_node, member) container_of(container_of(rb_node, struct task_edf, member), struct task_struct, edf)
#define READY_QUEUE	0
#define RESCHED_QUEUE	1
#define MIN_EDF_QUANTUM 100000 // 100 usec

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
	uint64_t curr_time_us = get_time()/1000ul; // Time is returned by get_time() in ns and converted to us
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
 * insert_task_edf: Finds a place in the tree, adds the node
 * and rebalaces the tree
 */

static bool
insert_task_edf(struct task_struct *task, struct edf_rq *runqueue, int queue){

	struct rb_node *task_node = NULL;
	struct rb_root * tree = NULL;

	struct rb_node *parent = NULL;
	struct task_struct *curr_task;

	if (queue == READY_QUEUE){
		task_node = &task->edf.node;
		tree = &runqueue->tasks_tree;
	}
	else{	// RESCHED_QUEUE
		task_node = &task->edf.resched_node;
		tree = &runqueue->resched_tree;
	}

	struct rb_node **new_rb = &tree->rb_node;

	// Find out place in the tree for the new core
	while (*new_rb) {

		if (queue == READY_QUEUE)
			curr_task = task_container_of(*new_rb, node);
		else
			curr_task = task_container_of(*new_rb, resched_node);

		parent = *new_rb;

		if (task->edf.curr_deadline < curr_task->edf.curr_deadline)
			new_rb = &((*new_rb)->rb_left);
		else if (task->edf.curr_deadline > curr_task->edf.curr_deadline)
			new_rb = &((*new_rb)->rb_right);
		else // It is Possible to have same current deadlines in both tasks!
			return false;
	}
	// Add new node and rebalance tree.
	rb_link_node(task_node, parent, new_rb);
	rb_insert_color(task_node, tree);

	return true;
}

/*
 * next_start_period: Given the current host time and the period of a given task,
 * calculates the time in which its next period starts.
 *
 */

static uint64_t
next_start_period(uint64_t curr_time_us, struct task_struct *task){

	uint64_t next_start_us = 0;

	if(!task->edf.curr_deadline)
		next_start_us = curr_time_us + task->edf.period;
	else
		next_start_us = task->edf.curr_deadline + task->edf.period;
	return next_start_us;
}


static void cpu_remove_tasks(struct rb_root *tree, int queue){

	struct task_struct * task = NULL;
	struct rb_node *nd;

	nd = rb_first(tree);

	while(nd){

		if ( queue == READY_QUEUE)
			task = task_container_of(nd, node);
		else
			task = task_container_of(nd, resched_node);

		kthread_bind(task, 0);
		cpu_clear(this_cpu, task->aspace->cpu_mask);
		nd = rb_next(nd);
	}
}

/*
 * edf_sched_cpu_remove: Adapted rr_sched_cpu_remove (rr_sched.c)
 */

/* set the offlining flag and migrate all tasks away
 * The idle task will complete the offline operation when it runs
 */
void edf_sched_cpu_remove(struct edf_rq *runqueue, void * arg)
{
	cpu_remove_tasks(&runqueue->tasks_tree, READY_QUEUE);
	cpu_remove_tasks(&runqueue->resched_tree, RESCHED_QUEUE);
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

		bool ins = insert_task_edf(task, runqueue, READY_QUEUE);
		/*
		 * If not inserted is possible that there is other task
		 * with the same deadline.
		 * Then, the deadline is modified and try again
		 */
		while(!ins){
			task->edf.curr_deadline ++;
			ins = insert_task_edf(task, runqueue, READY_QUEUE);
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
		printk(KERN_INFO "EDF_SCHED. activate_task. could not activate task %s. Not admissible (U = %d)\n",
		       task->name,
		       runqueue->cpu_u);

		return 0;

}


/*
 * edf_sched_task_init: Initializes per task data structure (task_edf_sched) and
 * calls activate function.
 */


int
edf_sched_add_task(struct edf_rq *runqueue, struct task_struct * task){

	//printk("EDF_SCHED. Adding Task: id %d name %s cpu_id %d\n", task->id,task->name, task->cpu_id);

	activate_task(task, runqueue);
	return 0;
}


/*
 * search_task_edf: Searches a task in the red-black tree by using its curr_deadline
 */
static struct task_struct *
search_task_edf(time_us curr_deadline, struct edf_rq *runqueue, int queue){

	struct rb_root * tree = NULL;

	if (queue == READY_QUEUE){
		tree = &runqueue->tasks_tree;
	}
	else{	// RESCHED_QUEUE
		tree = &runqueue->resched_tree;
	}

	struct rb_node *node = tree->rb_node;

	while (node) {

		struct task_struct *task = NULL;

		if ( queue == READY_QUEUE)
			task = task_container_of(node, node);
		else
			task = task_container_of(node, resched_node);

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
delete_task_edf( struct task_struct *task, struct edf_rq *runqueue, int queue){

	struct rb_node *task_node = NULL;
	struct rb_root * tree = NULL;

	if (queue == READY_QUEUE){
		task_node = &task->edf.node;
		tree = &runqueue->tasks_tree;
	}
	else{	// RESCHED_QUEUE
		task_node = &task->edf.resched_node;
		tree = &runqueue->resched_tree;
	}

	struct task_struct *tsk = search_task_edf(task->edf.curr_deadline,
						   runqueue, queue);
	if (tsk){
		rb_erase(task_node, tree);
		return true;
	}
	else{
		//printk(KERN_INFO "EDF_SCHED. delete_task_edf.Attempted to erase unexisting task\n");
		return false;
	}
}

#if 0
static void
check_statistics(struct task_struct * task, struct  edf_rq *runqueue){

	time_us host_time = get_curr_host_time();
	time_us time_prints=0;
	int deadlines_percentage=0;


	/*Task Statistics*/

	if(task->edf.print_miss_deadlines == 0){
		task->edf.print_miss_deadlines = host_time;
		task->edf.deadlines = 0;
		task->edf.miss_deadlines = 0;
	}

	time_prints = host_time - task->edf.print_miss_deadlines;
	if(time_prints >= DEADLINE_INTERVAL && task->state == TASK_RUNNING){
		if (task->edf.deadlines)
			deadlines_percentage = 100 * task->edf.miss_deadlines / task->edf.deadlines;
		printk(KERN_INFO
		"EDF_SCHED. rq_U %d, Tsk: id %d, name %s, state %d, used_t %llu, sl %llu, T %llu, host_t %llu,  dl %llu, nr_dl %llu, miss_dl %llu miss_dl_per %d\n",
			runqueue->cpu_u,
			task->id,
			task->name,
			task->state,
			task->edf.used_time,
			task->edf.slice,
			task->edf.period,
			host_time,
			task->edf.curr_deadline,
			task->edf.deadlines,
			task->edf.miss_deadlines,
			deadlines_percentage);

		task->edf.deadlines=0;
		task->edf.miss_deadlines=0;
		deadlines_percentage = 0;
		task->edf.print_miss_deadlines=host_time;

	}
	if (task->state != TASK_RUNNING){
		task->edf.print_miss_deadlines=0;
	}
}
#endif

/*
 * deactivate_task - Removes a task from the red-black tree.
 */

static void
deactivate_task(struct task_struct * task, struct edf_rq *runqueue){

#if 0
	check_statistics(task,runqueue);
#endif
	delete_task_edf(task, runqueue, READY_QUEUE);
	runqueue->cpu_u -= task->edf.cpu_reservation;

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

	/* Tasks which deadline is over are inserted in the resched rbtree
	 * resched rbtree includes the tasks which have consumed their slices and
	 * tasks which deadline is over*/
	while(node){
		next_task = task_container_of(node, node);
		if(next_task->edf.curr_deadline <= host_time ){
			/* This task's deadline is over, send to resched tree */
			insert_task_edf(next_task, runqueue, RESCHED_QUEUE);
		} else {
			/* If current task's deadline is still valid we
			 * do not check more tasks*/
			break;
		}
		node = rb_next(node);
	}

	/* Check tasks in the resched rbtree to find the ones which deadline is over
	 * and deactivate them*/
	node = rb_first(&runqueue->resched_tree);

	while(node){

		next_task = task_container_of(node, resched_node);
		/*If Task's deadline is over, remove task from the tasks runqueue*/
		if(next_task->edf.curr_deadline <= host_time ){
			next_task->edf.deadlines++;
			deactivate_task(next_task,runqueue);
			list_add_tail(&next_task->edf.sched_link, &resched_list);
			/*This task missed its deadline*/
			if(next_task->edf.used_time < next_task->edf.slice
				&& next_task->state == TASK_RUNNING){
				next_task->edf.miss_deadlines++;
			}
		} else{
			break;
		}
		node = rb_next(node);
	}

	/* Reinsert tasks which deadline is over into the
	 * tasks_tree for its new deadline and remove from resched tree*/

	struct task_struct *task, *tmp;
	list_for_each_entry_safe(task, tmp, &resched_list,
				 edf.sched_link) {
#ifdef CONFIG_SCHED_EDF_RR
		if (!list_empty(&task->rr.sched_link)) {
			list_del(&task->rr.sched_link);
			list_head_init(&task->rr.sched_link);
		}
#endif
		if ((task->cpu_id == task->cpu_target_id)
		    && (task->state != TASK_EXITED)) {
			delete_task_edf(task, runqueue, RESCHED_QUEUE);
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

	/*Pick first earliest deadline task*/

	struct rb_node *node=NULL;
	struct task_struct * found = NULL;

	check_deadlines(runqueue);

	node = rb_first(&runqueue->tasks_tree);

	/* Pick the next task that has not used its whole slice */
	while(node){

		next_task = task_container_of(node, node);
		if ((next_task->state == TASK_RUNNING)
			&& (next_task->cpu_id == next_task->cpu_target_id)) {
			found = next_task;
			break;
		}
		node = rb_next(node);
	}

	return found;

}

static ktime_t
get_quantum_period(struct edf_rq * runqueue){

	/* set quantum timer for closest start period of tasks that have consumed their
	 * slices (tasks in resched_tree rb_tree) and tasks which still have not consumed
	 * their slices (tasks in tasks_tree rb_tree)
	 * */

	ktime_t inttime = 0;
	struct rb_node *node_ready = rb_first(&runqueue->tasks_tree);
	struct rb_node *node_resched = rb_first(&runqueue->resched_tree);
	struct task_struct * task_ready = NULL;
	struct task_struct * task_resched = NULL;

	if(node_ready && node_resched){

		task_ready = task_container_of(node_ready, node);
		task_resched = task_container_of(node_resched, resched_node);

		if(task_ready->edf.curr_deadline < task_resched->edf.curr_deadline)
			inttime = task_ready->edf.curr_deadline;
		else
			inttime = task_resched->edf.curr_deadline;
	}
	else if (node_ready){

		task_ready = task_container_of(node_ready, node);
		inttime = task_ready->edf.curr_deadline;
	}
	else if (node_resched){

		task_resched = task_container_of(node_resched, resched_node);
		inttime = task_resched->edf.curr_deadline;
	}

	const ktime_t now_us = get_time()/1000ul;

	/* Deadline already passed fire the timer soon*/
	if( inttime && (inttime < now_us)){
		inttime = now_us + MIN_EDF_QUANTUM;
	}

	return (inttime * 1000ul);
}

/*
 * edf_schedule: Pick next task to be scheduled and wake it up
 */

struct task_struct *
edf_schedule(struct edf_rq *runq, struct list_head *migrate_list, ktime_t *inttime)
{

	struct task_struct *task;

	task = pick_next_task(runq); /* Pick next task to schedule */

        const uint64_t now = get_time();

	/* Set the timer to finish this quantum; note that the EDF timer
         * could be set and expire before this if there's an upcoming
         * EDF period, but no current EDF task with slice available. */

	if(inttime){
		if (task) {
			uint64_t remaining = task->edf.slice - task->edf.used_time;
			*inttime = (now + (remaining * 1000ul));
		}

		/* Calculate the quantum to fire the timer at the closest start of period*/
		ktime_t inttime_period = get_quantum_period(runq);

		if (inttime_period){ /*There is at least a task in one of the queues*/
			if (!task){
#ifdef CONFIG_SCHED_EDF_RR
				/*If not task ready, tasks are RR scheduled, so we need small quantums*/
				*inttime = (now + MIN_EDF_QUANTUM);
#endif
			}

			if (*inttime){
				if (inttime_period < *inttime)
					*inttime = inttime_period;
			}else
				*inttime = inttime_period;
		}
	}
	return task;
}

/*
 * adjust_slice - Adjust task parameters values
 */
static int adjust_slice(struct edf_rq * runq,struct task_struct *task){

	uint64_t host_time = get_curr_host_time();
	uint64_t used_time =  host_time - task->edf.last_wakeup;
	int edf_scheduled = 1;
	uint64_t old_used_t = task->edf.used_time;
	task->edf.used_time += used_time;

	/* If slice is consumed add the task to the rr_list */
	if(task->edf.used_time >= task->edf.slice){
#ifdef CONFIG_SCHED_EDF_RR
		edf_scheduled=0;
#endif

#ifdef CONFIG_SCHED_EDF_WC
	deactivate_task(task,runq);
	activate_task(task,runq);
#else
	if (old_used_t < task->edf.slice){ // First time the slice is exceed
		delete_task_edf(task, runq, READY_QUEUE);
		insert_task_edf(task, runq, RESCHED_QUEUE);
	}
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
	runq->resched_tree = RB_ROOT;
	runq->cpu_u=0;
	runq->curr_task=NULL;
	runq->start_time = 0;
	init_edf_config(&runq->edf_config);

	return 0;
}
