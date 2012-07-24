#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/params.h>
#include <lwk/init.h>
#include <lwk/elf.h>
#include <lwk/kfs.h>
#include <lwk/sched.h>

/**
 * Maximum number of arguments and environment variables that may
 * be passed to the init_task.
 */
#define INIT_MAX_ARGC 32
#define INIT_MAX_ENVC 32

/**
 * Maximum length of the init_argv= and init_envp= strings on the
 * kernel boot command line.
 */
#define INIT_ARGV_LEN 1024
#define INIT_ENVP_LEN 1024

/**
 * The init_task ELF executable.
 */
paddr_t init_elf_image;

/**
 * Amount of memory to reserve for the init_task's heap.
 */
unsigned long init_heap_size = (1024 * 1024 * 32);  /* 32 MB */
param(init_heap_size, ulong);

/**
 * Amount of memory to reserve for the init_task's stack.
 */
unsigned long init_stack_size = (1024 * 256);  /* 256 KB */
param(init_stack_size, ulong);

/**
 * Arguments to pass to the init_task.
 */
static char init_argv_str[INIT_ARGV_LEN] = { 0 };
param_string(init_argv, init_argv_str, sizeof(init_argv_str));

/**
 * Environment to pass to the init_task.
 */
static char init_envp_str[INIT_ENVP_LEN] = { 0 };
param_string(init_envp, init_envp_str, sizeof(init_envp_str));

/**
 * Creates the init_task.
 */
int
create_init_task(void)
{
	int status;
	start_state_t start_state = {
		.task_id	= ANY_ID,
		.task_name	= "init_task",
		.user_id	= 0,
		.group_id	= 0,
		.cpu_id		= ANY_ID,
		.use_args	= false,
	};

	if (!init_elf_image) {
		printk("No init_elf_image found.\n");
		return -EINVAL;
	}
	
	/* This initializes start_state aspace_id, entry_point, and stack_ptr */
	status =
	elf_load(
		__va(init_elf_image),
		start_state.task_name,
		INIT_ASPACE_ID,
		PAGE_SIZE,
		init_heap_size,
		init_stack_size,
		init_argv_str,
		init_envp_str,
		&start_state,
		0,
		&elf_dflt_alloc_pmem
	);
	if (status) {
		printk("Failed to load init_task (status=%d).\n", status);
		return status;
	}

	/* This prevents the address space from being deleted by
	 * user-space, since the kernel never releases this reference */
	if (!aspace_acquire(INIT_ASPACE_ID)) {
		printk("Failed to acquire INIT_ASPACE_ID.\n");
		return status;
	}


	struct task_struct *new_task = __task_create(&start_state, NULL);

	/* Assign stdout and stderr */

	kfs_init_stdio(new_task);

	sched_add_task(new_task);

	return 0;
}
