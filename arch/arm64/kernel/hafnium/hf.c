#include <lwk/spinlock.h>
#include <lwk/timer.h>
#include <lwk/task.h>
#include <lwk/interrupt.h>
#include <lwk/sched.h>
#include <lwk/driver.h>
#include <arch/irqchip.h>

#include "call.h"
#include "ffa.h"
#include "transport.h"

#define CONFIG_HAFNIUM_MAX_VMS   (16)
#define CONFIG_HAFNIUM_MAX_VCPUS (32)

#define HF_VM_ID_BASE            (0)
#define PRIMARY_VM_ID            (HF_VM_ID_OFFSET + 0)
#define PRIMARY2_VM_ID   		 (HF_VM_ID_OFFSET + 1)
#define FIRST_SECONDARY_VM_ID    (HF_VM_ID_OFFSET + 2)


struct hf_vcpu {
	struct hf_vm       * vm;
	ffa_vcpu_index_t     vcpu_index;
	struct task_struct * task;
	atomic_t             abort_sleep;
	atomic_t             waiting_for_message;
	struct timer         timer;
};

struct hf_vm {
	ffa_vm_id_t        id;
	ffa_vcpu_count_t   vcpu_count;
	struct hf_vcpu   * vcpu;
};

static struct hf_vm   * hf_vms;
static ffa_vm_count_t   hf_vm_count;
static void           * hf_send_page;
static void           * hf_recv_page;
static atomic64_t       hf_next_port = ATOMIC64_INIT(0);
static int              hf_irq;
//static enum cpuhp_state hf_cpuhp_state;
static ffa_vm_id_t      current_vm_id;

static DEFINE_SPINLOCK(hf_send_lock);
//static DEFINE_HASHTABLE(hf_local_port_hash, 7);
static DEFINE_SPINLOCK(hf_local_port_hash_lock);



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

static void 
print_ffa_error(struct ffa_value ffa_ret)
{
	if (ffa_ret.func == FFA_ERROR_32) {
		pr_err("FF-A error code %d\n", ffa_ret.arg2);
	} else {
		pr_err("Unexpected FF-A function %#x\n", ffa_ret.func);
	}
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

static void 
hf_free_resources(void)
{
#if 0
	/* uint16_t i; */
	/* ffa_vcpu_index_t j; */

	/* /\* */
	/*  * First stop all worker threads. We need to do this before freeing */
	/*  * resources because workers may reference each other, so it is only */
	/*  * safe to free resources after they have all stopped. */
	/*  *\/ */
	/* for (i = 0; i < hf_vm_count; i++) { */
	/* 	struct hf_vm *vm = &hf_vms[i]; */

	/* 	for (j = 0; j < vm->vcpu_count; j++) */
	/* 		kthread_stop(vm->vcpu[j].task); */
	/* } */

	/* /\* Free resources. *\/ */
	/* for (i = 0; i < hf_vm_count; i++) { */
	/* 	struct hf_vm *vm = &hf_vms[i]; */

	/* 	for (j = 0; j < vm->vcpu_count; j++) */
	/* 		put_task_struct(vm->vcpu[j].task); */
	/* 	kfree(vm->vcpu); */
	/* } */

	/* kfree(hf_vms); */

	/* ffa_rx_release(); */
	/* if (hf_send_page) { */
	/* 	__free_page(hf_send_page); */
	/* 	hf_send_page = NULL; */
	/* } */
	/* if (hf_recv_page) { */
	/* 	__free_page(hf_recv_page); */
	/* 	hf_recv_page = NULL; */
	/* } */
#endif
}

static char * options;

static int
hafnium_init(void)
{
	struct ffa_value ffa_ret;
	ffa_vm_count_t   secondary_vm_count;
	uint32_t         total_vcpu_count;

	ffa_vcpu_index_t j;

	int64_t          ret;

	printk("Setting up Hafnium environment\n");

	/* Allocate a page for send and receive buffers. */
	hf_send_page = kmem_get_pages(0);

	if (hf_send_page == NULL) {
		pr_err("Unable to allocate send buffer\n");
		return -ENOMEM;
	}

	hf_recv_page = kmem_get_pages(0);

	if (hf_recv_page == NULL) {
		kmem_free_pages(hf_send_page, 0);
		hf_send_page = NULL;

		pr_err("Unable to allocate receive buffer\n");
		return -ENOMEM;
	}

	/*
	 * Configure both addresses. Once configured, we cannot free these pages
	 * because the hypervisor will use them, even if the module is
	 * unloaded.
	 */
	printk("Mapping pages, send %p recv %p\n", hf_send_page, hf_recv_page);

	ffa_ret = ffa_rxtx_map(__pa(hf_send_page), __pa(hf_recv_page));

	if (ffa_ret.func != FFA_SUCCESS_32) {
		pr_err("Unable to configure VM mailbox.\n");

		print_ffa_error(ffa_ret);

		ret = -EIO;
		goto fail_with_cleanup;
	}


	/* Only track the secondary VMs. */
	hf_vms = kmem_alloc(CONFIG_HAFNIUM_MAX_VMS * sizeof(struct hf_vm));

	if (hf_vms == NULL) {
		ret = -ENOMEM;
		goto fail_with_cleanup;
	}


    if (hf_vm_get_count() < 2) {
        pr_err("Primary2 VM missing from manifest\n");
        ret = -EINVAL;
        goto fail_with_cleanup;
    }

	/* Start the Admin VM */
	{
		struct hf_vm      * vm = &hf_vms[PRIMARY2_VM_ID];
		ffa_vcpu_count_t    vcpu_count;

		/* Adjust the index as only the secondaries are tracked. */
		vm->id         = PRIMARY2_VM_ID;
		vm->vcpu_count = hf_vcpu_get_count(vm->id);
		vm->vcpu       = kmem_alloc(vm->vcpu_count * sizeof(struct hf_vcpu));

		if (vm->vcpu == NULL) {
			ret = -ENOMEM;
			goto fail_with_cleanup;
		}


		total_vcpu_count += vm->vcpu_count;

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
				goto fail_with_cleanup;
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

	}

	return 0;


fail_with_cleanup:
	hf_free_resources();
	return ret;

#if 0

	/* Confirm the maximum number of VMs looks sane. */
	BUG_ON(CONFIG_HAFNIUM_MAX_VMS < 1);
	//    BUG_ON(CONFIG_HAFNIUM_MAX_VMS > U16_MAX);

	/* Validate the number of VMs. There must at least be the primary. */
	if (secondary_vm_count > CONFIG_HAFNIUM_MAX_VMS - 1) {
		pr_err("Number of VMs is out of range: %d\n",
			secondary_vm_count);
		ret = -EDQUOT;
		goto fail_with_cleanup;
	}



	/* Cache the VM id for later usage. */
	current_vm_id = hf_vm_get_id();

	/* Initialize each VM. */
	total_vcpu_count = 0;
	
	for (i = 0; i < secondary_vm_count; i++) {
		struct hf_vm      * vm = &hf_vms[i];
		ffa_vcpu_count_t    vcpu_count;

		/* Adjust the index as only the secondaries are tracked. */
		vm->id     = FIRST_SECONDARY_VM_ID + i;
		vcpu_count = hf_vcpu_get_count(vm->id);

		/* Avoid overflowing the vcpu count. */
		if (vcpu_count > (UINT_MAX - total_vcpu_count)) {
			pr_err("Too many vcpus: %u\n", total_vcpu_count);

			ret = -EDQUOT;
			goto fail_with_cleanup;
		}

		/* Confirm the maximum number of VCPUs looks sane. */
		BUG_ON(CONFIG_HAFNIUM_MAX_VCPUS < 1);
		//      BUG_ON(CONFIG_HAFNIUM_MAX_VCPUS > U16_MAX);

		/* Enforce the limit on vcpus. */
		total_vcpu_count += vcpu_count;

		if (total_vcpu_count > CONFIG_HAFNIUM_MAX_VCPUS) {
			pr_err("Too many vcpus: %u\n", total_vcpu_count);

			ret = -EDQUOT;
			goto fail_with_cleanup;
		}

		vm->vcpu_count = vcpu_count;
		vm->vcpu       = kmem_alloc(vm->vcpu_count * sizeof(struct hf_vcpu));

		if (vm->vcpu == NULL) {
			ret = -ENOMEM;
			goto fail_with_cleanup;
		}

		/* Update the number of initialized VMs. */
		hf_vm_count = i + 1;

		/* Create a kernel thread for each vcpu. */
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
				goto fail_with_cleanup;
			}

			/* NMG: Kitten doesn't refcount task structs so I don't think this is needed. */
			//get_task_struct(vcpu->task);
			vcpu->vm         = vm;
			vcpu->vcpu_index = j;

			atomic_set(&vcpu->abort_sleep,         0);
			atomic_set(&vcpu->waiting_for_message, 0);
		}
	}

	/* NMG: Disable inter-VM communication for now. We'll get there eventually */
	/* /\* Register protocol and socket family. *\/ */
	/* ret = proto_register(&hf_sock_proto, 0); */
	/* if (ret) { */
	/*     pr_err("Unable to register protocol: %lld\n", ret); */
	/*     goto fail_with_cleanup; */
	/* } */

	/* ret = sock_register(&proto_family); */
	/* if (ret) { */
	/*     pr_err("Unable to register Hafnium's socket family: %lld\n", */
	/*            ret); */
	/*     goto fail_unregister_proto; */
	/* } */

	/*
	 * Start running threads now that all is initialized.
	 *
	 * Any failures from this point on must also unregister the driver with
	 * platform_driver_unregister().
	 */
	
	for (i = 0; i < hf_vm_count; i++) {
		struct hf_vm * vm = &hf_vms[i];

		for (j = 0; j < vm->vcpu_count; j++) {
			sched_wakeup_task(vm->vcpu[j].task, TASK_STOPPED);
		}
	}

	/* Dump vm/vcpu count info. */
	pr_info("Hafnium successfully loaded with %u VMs:\n", hf_vm_count);
	for (i = 0; i < hf_vm_count; i++) {
		struct hf_vm * vm = &hf_vms[i];

		pr_info("\tVM %u: %u vCPUS\n", vm->id, vm->vcpu_count);
	}

	return 0;

fail_unregister_socket:
	//sock_unregister(PF_HF);
fail_unregister_proto:
	//proto_unregister(&hf_sock_proto);


#endif
}

DRIVER_INIT( "module", hafnium_init );
DRIVER_PARAM( options, charp);