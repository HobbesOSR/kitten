/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/kernel.h>
#include <lwk/sched.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/string.h>

#include "call.h"
#include "ffa.h"
#include "transport.h"
#include "types.h"
#include "ctrl.h"
#include "hf.h"




#define HAFNIUM_CMD_PATH "/dev/hafnium"
static char * options = NULL;



static void           * hf_send_page;
static void           * hf_recv_page;


#if 0
static atomic64_t       hf_next_port = ATOMIC64_INIT(0);
static int              hf_irq;
//static enum cpuhp_state hf_cpuhp_state;
static ffa_vm_id_t      current_vm_id;

static DEFINE_SPINLOCK(hf_send_lock);
//static DEFINE_HASHTABLE(hf_local_port_hash, 7);
static DEFINE_SPINLOCK(hf_local_port_hash_lock);
#endif

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







static int
__init_hypervisor()
{
	struct ffa_value ffa_ret;
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
    printk("Launching Admin VM\n");
    if (hf_launch_vm(PRIMARY2_VM_ID) != 0) {
        pr_err("Could not launch Primary2 VM\n");
        ret = -EFAULT;
        goto fail_with_cleanup;
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



static long
__hafnium_ioctl(struct file   * filp, 
                unsigned int    ioctl,
                unsigned long   arg)
{
    void __user * argp = (void __user *)arg;
    int ret = 0;

    printk("Hafnium IOCTL: %d\n", ioctl);

    switch (ioctl) {
        case HAFNIUM_IOCTL_HYP_INIT:
            ret = __init_hypervisor();
            break;
        case HAFNIUM_IOCTL_LAUNCH_VM: /* Falls through. */
        default:
            printk(KERN_ERR "\tUnhandled global ctrl cmd: %d\n", ioctl);
            
            return -EINVAL;
    }



    return ret;

}



static struct kfs_fops hafnium_ctrl_fops = {
    .unlocked_ioctl = __hafnium_ioctl,
};


static int
hafnium_init(void)
{
    printk(KERN_INFO "---- Initializing Hafnium hypervisor support\n");

    kfs_create(HAFNIUM_CMD_PATH, 
               NULL, 
               &hafnium_ctrl_fops, 
               0777, NULL, 0);


    return 0;
}

DRIVER_INIT( "module", hafnium_init );
DRIVER_PARAM( options, charp);