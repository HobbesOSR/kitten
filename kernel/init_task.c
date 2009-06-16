#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/params.h>
#include <lwk/init.h>
#include <lwk/elf.h>
#include <lwk/kfs.h>

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
unsigned long init_heap_size = (1024 * 1024 * 16);  /* 16 MB */
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
	start_state_t start_state;

	if (!init_elf_image) {
		printk("No init_elf_image found.\n");
		return -EINVAL;
	}
	
	/* Initialize the start_state fields that we know up-front */
	start_state.uid     = 0;
	start_state.gid     = 0;
	start_state.cpu_id  = this_cpu;
	start_state.cpumask = NULL;

	/* This initializes start_state aspace_id, entry_point, and stack_ptr */
	status =
	elf_load(
		__va(init_elf_image),
		"init_task",
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

	id_t id;
	int rc = task_create(INIT_TASK_ID, "init_task", &start_state, &id);
	if( rc != 0 )
		return rc;

	/* Assign stdout and stderr */
	struct task_struct * new_task = task_lookup( id );
	if( !new_task )
		panic( "Unable to retrieve init task id %lu?", id  );

	struct kfs_file * console = kfs_lookup( kfs_root, "/dev/console", 0 );
	if( !console )
		panic( "Unable to open /dev/console?" );
	new_task->files[ 0 ] = console;
	new_task->files[ 1 ] = console;
	new_task->files[ 2 ] = console;

	return 0;
}
