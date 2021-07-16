#include <lwk/spinlock.h>
#include <lwk/task.h>
#include <lwk/sched.h>


#include "call.h"
#include "ffa.h"
#include "transport.h"
#include "hf.h"




struct hf_vm   * hf_vms;
ffa_vm_count_t   hf_vm_count;



int64_t 
hf_call(uint64_t arg0, 
	uint64_t arg1, 
	uint64_t arg2, 
	uint64_t arg3)
{
	register uint64_t r0 __asm__("x0") = arg0;
	register uint64_t r1 __asm__("x1") = arg1;
	register uint64_t r2 __asm__("x2") = arg2;
	register uint64_t r3 __asm__("x3") = arg3;

	__asm__ volatile(
		"hvc #0"
		: /* Output registers, also used as inputs ('+' constraint). */
		"+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
		:
		: /* Clobber registers. */
		"x4", "x5", "x6", "x7");

	return r0;
}

struct ffa_value
ffa_call(struct ffa_value args)
{
	register uint64_t r0 __asm__("x0") = args.func;
	register uint64_t r1 __asm__("x1") = args.arg1;
	register uint64_t r2 __asm__("x2") = args.arg2;
	register uint64_t r3 __asm__("x3") = args.arg3;
	register uint64_t r4 __asm__("x4") = args.arg4;
	register uint64_t r5 __asm__("x5") = args.arg5;
	register uint64_t r6 __asm__("x6") = args.arg6;
	register uint64_t r7 __asm__("x7") = args.arg7;

	__asm__ volatile(
		"hvc #0"
		: /* Output registers, also used as inputs ('+' constraint). */
		"+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4), "+r"(r5),
		"+r"(r6), "+r"(r7));

	return (struct ffa_value) {
		          .func = r0,
				  .arg1 = r1,
				  .arg2 = r2,
				  .arg3 = r3,
				  .arg4 = r4,
				  .arg5 = r5,
				  .arg6 = r6,
				  .arg7 = r7};
}

static void 
hf_vcpu_sleep(struct hf_vcpu * vcpu)
{
	int abort;

	set_task_state(current, TASK_INTERRUPTIBLE);

	/* Check the sleep-abort flag after making thread interruptible. */
	abort = atomic_read(&vcpu->abort_sleep);
	if (!abort && !kthread_should_stop()) {
		schedule();
	}

	/* Set state back to running on the way out. */
	set_task_state(current, TASK_RUNNING);
}

static struct hf_vm *
hf_vm_from_id(ffa_vm_id_t vm_id)
{
	if ((vm_id <  FIRST_SECONDARY_VM_ID) ||
	    (vm_id >= FIRST_SECONDARY_VM_ID + hf_vm_count)) {
		return NULL;
	}

	return &hf_vms[vm_id - FIRST_SECONDARY_VM_ID];
}

static int 
hf_vcpu_wake_up(struct hf_vcpu * vcpu)
{
	/* Set a flag indicating that the thread should not go to sleep. */
	atomic_set(&vcpu->abort_sleep, 1);

	/* Set the thread to running state. */
	return sched_wakeup_task((vcpu->task), TASK_ALL);
}

static void 
hf_vcpu_timer_expired(struct hrtimer * timer)
{
	struct hf_vcpu * vcpu = container_of(timer, struct hf_vcpu, timer);

	/* TODO: Inject interrupt. */

	hf_vcpu_wake_up(vcpu);
}

static void 
hf_handle_wake_up_request(ffa_vm_id_t      vm_id,
		          ffa_vcpu_index_t vcpu)
{
	struct hf_vm * vm = hf_vm_from_id(vm_id);

	if (vm == NULL) {
		printk(KERN_WARNING "Request to wake up non-existent VM id: %u\n", vm_id);
		return;
	}

	if (vcpu >= vm->vcpu_count) {
		printk(KERN_WARNING "Request to wake up non-existent vCPU: %u.%u\n",
			vm_id, vcpu);
		return;
	}

	if (hf_vcpu_wake_up(&vm->vcpu[vcpu]) == 0) {
		/*
		 * The task was already running (presumably on a different
		 * physical CPU); interrupt it. This gives Hafnium a chance to
		 * inject any new interrupts.
		 */
		arch_xcall_reschedule(vm->vcpu[vcpu].task->cpu_id);
	}
}

static int 
hf_vcpu_thread(void * data)
{
	struct hf_vcpu * vcpu = data;
	struct ffa_value ret;

	vcpu->timer.function = &hf_vcpu_timer_expired;

	while (!kthread_should_stop()) {
		ffa_vcpu_index_t i;

		/*
		 * We're about to run the vcpu, so we can reset the abort-sleep
		 * flag.
		 */
		atomic_set(&vcpu->abort_sleep, 0);

		/* Call into Hafnium to run vcpu. */
		ret = ffa_run(vcpu->vm->id, vcpu->vcpu_index);

		switch (ret.func) {
		/* Preempted. */
		case FFA_INTERRUPT_32:
			
			if (test_bit(TF_NEED_RESCHED_BIT, &(current->arch.flags))) {
				schedule();
			}
			break;

		/* Yield. */
		case FFA_YIELD_32:
			if (!kthread_should_stop()) {
				schedule();
			}

			break;

		/* WFI. */
		case HF_FFA_RUN_WAIT_FOR_INTERRUPT:
			if (ret.arg2 != FFA_SLEEP_INDEFINITE) {
				vcpu->timer.expires = get_time() + ret.arg2;
				timer_add(&vcpu->timer);
			}
			
			hf_vcpu_sleep(vcpu);
			timer_del(&vcpu->timer);
			
			break;

		/* Waiting for a message. */
		case FFA_MSG_WAIT_32:
			atomic_set(&vcpu->waiting_for_message, 1);

			if (ret.arg2 != FFA_SLEEP_INDEFINITE) {
				vcpu->timer.expires = get_time() + ret.arg2;
				timer_add(&vcpu->timer);
			}
			
			hf_vcpu_sleep(vcpu);
			timer_del(&vcpu->timer);

			atomic_set(&vcpu->waiting_for_message, 0);
			break;

		/* Wake up another vcpu. */
		case HF_FFA_RUN_WAKE_UP:
			hf_handle_wake_up_request(ffa_vm_id(ret),
						  ffa_vcpu_index(ret));
			break;

		/* Response available. */
		/* case FFA_MSG_SEND_32: */
		/* 	if (ffa_msg_send_receiver(ret) == PRIMARY_VM_ID) { */
		/* 		hf_handle_message(vcpu->vm, */
		/* 				  ffa_msg_send_size(ret), */
		/* 				  page_address(hf_recv_page)); */
		/* 	} else { */
		/* 		hf_deliver_message(ffa_msg_send_receiver(ret)); */
		/* 	} */
		/* 	break; */

		/* /\* Notify all waiters. *\/ */
		/* case FFA_RX_RELEASE_32: */
		/* 	hf_notify_waiters(vcpu->vm->id); */
		/* 	break; */

		case FFA_ERROR_32:
			printk(KERN_WARNING "FF-A error %d running VM %d vCPU %d", 
				ret.arg2,
				vcpu->vm->id, 
				vcpu->vcpu_index);

			switch (ret.arg2) {
			/* Abort was triggered. */
			case FFA_ABORTED:
				for (i = 0; i < vcpu->vm->vcpu_count; i++) {
					if (i == vcpu->vcpu_index) {
						continue;
					}

					hf_handle_wake_up_request(vcpu->vm->id,
								  i);
				}
				hf_vcpu_sleep(vcpu);
				break;
			default:
				/* Treat as a yield and try again later. */
				if (!kthread_should_stop()) {
					schedule();
				}
				break;
			}
			break;
		}
	}

	return 0;
}


int 
hf_launch_vm(ffa_vm_id_t vm_id)
{
    struct hf_vm      * vm = &hf_vms[vm_id];

    ffa_vcpu_count_t    vcpu_count;
   	ffa_vcpu_index_t j;

    int ret;

    /* Adjust the index as only the secondaries are tracked. */
    vm->id         = vm_id;
    vm->vcpu_count = hf_vcpu_get_count(vm->id);
    vm->vcpu       = kmem_alloc(vm->vcpu_count * sizeof(struct hf_vcpu));

    if (vm->vcpu == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    for (j = 0; j < vm->vcpu_count; j++) {
        struct hf_vcpu * vcpu = &vm->vcpu[j];

        vcpu->task = kthread_create(hf_vcpu_thread, 
                                    vcpu,
                                    "vcpu_thread_%u_%u", 
                                    vm->id, 
                                    j);

        if (vcpu->task == NULL) {
            pr_err("Error creating task (vm = %u, vcpu = %u): %lu\n",
                vm->id, j, vcpu->task);

            vm->vcpu_count = j;
            ret            = vcpu->task;
            goto err;
        }

        vcpu->vm         = vm;
        vcpu->vcpu_index = j;

        atomic_set(&vcpu->abort_sleep,         0);
        atomic_set(&vcpu->waiting_for_message, 0);
    }

    for (j = 0; j < vm->vcpu_count; j++) {
        sched_wakeup_task(vm->vcpu[j].task, TASK_STOPPED);
    }

    printk("hafnium initialized\n");


    return 0;

err:
    return ret;
}


