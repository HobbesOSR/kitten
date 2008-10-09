#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/params.h>
#include <lwk/task.h>
#include <lwk/elf.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <lwk/ctype.h>
#include <lwk/ptrace.h>
#include <lwk/pmem.h>
#include <arch/proto.h>


/**
 * Amount of memory to reserve for the PCT's heap.
 */
unsigned long pct_heap_size = (1024 * 1024 * 4);  /* 4 MB */
param(pct_heap_size, ulong);


/**
 * Amount of memory to reserve for the PCT's stack.
 */
unsigned long pct_stack_size = (1024 * 256);  /* 256 KB */
param(pct_stack_size, ulong);


/**
 * Arguments to pass to the PCT.
 */
#define BOOT_ARG_LEN 128
static char pct_argv_str[BOOT_ARG_LEN] = { 0 };
param_string(pct_argv, pct_argv_str, sizeof(pct_argv_str));


/**
 * Environment to pass to the PCT.
 */
static char pct_envp_str[BOOT_ARG_LEN] = { 0 };
param_string(pct_envp, pct_envp_str, sizeof(pct_envp_str));


static int
init_str_array(
	int	size,
	char *	ptrs[],
	char *	str
)
{ 
	int pos = 0;
	char *tmp;

	while (strlen(str)) {
		/* move past white space */
		while (*str && isspace(*str))
			++str;

		tmp = str;

		/* find the end of the string */
		while (*str && !isspace(*str))
			++str;

		*str++ = 0;

		if (strlen(tmp)) {
			if (pos == size - 1) {
				return -1;
			}
			ptrs[pos++] = tmp; 
		}
	
	}
	ptrs[pos] = "";
	return 0;
}

static void *
alloc_mem_for_init_task(size_t size, size_t alignment)
{
	struct pmem_region result;

	if (pmem_alloc_umem(size, alignment, &result))
		return NULL;

	/* Mark the memory as being used by the init task */
	result.type = PMEM_TYPE_INIT_TASK;
	BUG_ON(pmem_update(&result));

	return (void *) __va(result.start);
}

#define MAX_NUM_STRS 32

int
arch_create_init_task(void)
{
	int status;
	char *argv[MAX_NUM_STRS] = { "init_task" };
	char *envp[MAX_NUM_STRS];
	void *init_task_elf_image = __va(initrd_start);
	id_t aspace_id;
	start_state_t start_state;
	vaddr_t entry_point, stack_ptr;
	struct task_struct *init_task;

	if (init_str_array(MAX_NUM_STRS-1, argv+1, pct_argv_str))
		panic("Too many ARGV strings for init_task.");

	if (init_str_array(MAX_NUM_STRS, envp, pct_envp_str))
		panic("Too many ENVP strings for init_task.");

	if (init_task_elf_image == NULL)
		panic("Could not locate initial task ELF image.");

	if (aspace_create(INIT_ASPACE_ID, "init_task", &aspace_id))
		panic("Failed to create aspace for init_task.");

{
	struct aspace *aspace = aspace_acquire(aspace_id);

	arch_aspace_activate(aspace);

	status = elf_load_executable(
		aspace,
		init_task_elf_image,
		pct_heap_size, 
		&alloc_mem_for_init_task,
		&entry_point
	);
	status = setup_initial_stack(
		aspace,
		init_task_elf_image,
		pct_stack_size,
		&alloc_mem_for_init_task,
		argv,
		envp,
		0, /* uid */
		0, /* gid */
		&stack_ptr
	);
	aspace_release(aspace);
}

	start_state.uid         = 0;
	start_state.gid         = 0;
	start_state.aspace_id   = aspace_id;
	start_state.entry_point = entry_point;
	start_state.stack_ptr   = stack_ptr;
	start_state.cpu_id      = this_cpu;
	start_state.cpumask     = NULL;

	if (__task_create(INIT_TASK_ID, "init_task", &start_state, &init_task))
		panic("Failed to create init_task.");

	sched_add_task(init_task);
	return 0;
}
