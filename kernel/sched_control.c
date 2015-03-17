#include <lwk/sched_control.h>
#include <lwk/sched.h>
#include <lwk/task.h>
#include <lwk/aspace.h>

/*
 * get_task taken from gdb.c
 */
struct task_struct *get_task(id_t pid, id_t tid){
	struct task_struct *tsk, *tskret = NULL;
	struct aspace *aspace = aspace_acquire(pid);
	if(aspace){
		list_for_each_entry(tsk, &aspace->task_list, aspace_link) {
			if (tsk->id == tid)
				tskret = tsk;
		}
	}
	aspace_release(aspace);
	return tskret;
}

extern void sched_yield_task_to(int pid, int tid){

	if( pid < 0 || tid < 0 ){
		sched_yield();
	}
	else{
		struct task_struct * task = get_task(pid,tid);
		if(task){
			sched_yield_to(task);
		}
	}
}

extern int sched_setparams_task(int pid, int tid, int64_t slice, int64_t period){

	if(!slice || !period){
		printk(" Sched parameters non set\n");
		return -1;
	}

	struct task_struct * task = get_task(pid,tid);

	if(!task){
		printk("Error: Not task found!\n");
		return -1;
	}

	sched_set_params(task, slice, period);

	return 0;
}
