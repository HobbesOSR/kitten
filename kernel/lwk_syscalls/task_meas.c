#include <lwk/task.h>
#include <lwk/aspace.h>
#include <lwk/list.h>
#include <arch/uaccess.h>

int
sys_task_meas(id_t aspace_id, id_t task_id, uint64_t __user *time,
	uint64_t __user *energy, uint64_t __user *unit_energy)
{
	struct task_struct *tsk;
	struct aspace *aspace = aspace_acquire(aspace_id);

	list_for_each_entry(tsk, &aspace->task_list, aspace_link) {
		if(tsk->id == task_id) {
#ifdef CONFIG_TASK_MEAS_DEBUG
			printk("Found task %u - (%llu, %llu, %llu, %llu)\n",
				tsk->id, tsk->meas.time, tsk->meas.energy, tsk->meas.power, tsk->meas.units.energy);
#endif
			if( copy_to_user(time, &tsk->meas.time, sizeof(uint64_t)) )
				return -EFAULT;

			if( copy_to_user(energy, &tsk->meas.energy, sizeof(uint64_t)) )
				return -EFAULT;

			if( copy_to_user(unit_energy, &tsk->meas.units.energy, sizeof(uint64_t)) )
				return -EFAULT;
		}
#ifdef CONFIG_TASK_MEAS_DEBUG
		else
			printk("Skipping task %u\n", tsk->id);
#endif
	}

	return 0;
}
