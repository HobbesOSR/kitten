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
 * Amount of memory to reserve for the init_task's heap.
 */
unsigned long init_heap_size = (1024 * 1024 * 4);  /* 4 MB */
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

static int
init_str_array(
	int    size,
	char * ptrs[],
	char * str
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
			if (pos == size - 1)
				return -1;
			ptrs[pos++] = tmp; 
		}
	
	}
	ptrs[pos] = "";
	return 0;
}

static int
alloc_pmem(size_t size, size_t alignment, paddr_t *pmem)
{
	struct pmem_region result;

	if (pmem_alloc_umem(size, alignment, &result))
		return -ENOMEM;

	/* Mark the memory as being used by the init task */
	result.type = PMEM_TYPE_INIT_TASK;
	BUG_ON(pmem_update(&result));

	if (pmem)
		*pmem = result.start;
	return 0;
}

static int
make_region(
	id_t         aspace_id,
	vaddr_t      start,
	size_t       extent,
	vmflags_t    flags,
	vmpagesize_t pagesz,
	const char * name,
	paddr_t *    pmem
)
{
	int status;

	status = alloc_pmem(extent, PAGE_SIZE, pmem);
	if (status) {
		printk("Failed to allocate init_task %s (status=%d).",
		       name, status);
		return status;
	}

	status = aspace_map_region(aspace_id, start, extent,
	                           flags, pagesz,
	                           name, *pmem);
	if (status) {
		printk("Failed to map init_task %s (status=%d).",
		       name, status);
		return status;
	}

	return 0;
}

int
arch_create_init_task(void)
{
	int status;
	void *init_elf_image = __va(initrd_start);
	char *argv[INIT_MAX_ARGC] = { "init_task" };
	char *envp[INIT_MAX_ENVC];
	id_t aspace_id, task_id;
	vaddr_t heap_start, stack_start, stack_end, stack_ptr;
	size_t heap_extent, stack_extent;
	paddr_t heap_pmem, stack_pmem;
	start_state_t start_state;
	uid_t uid = 0; gid_t gid = 0;

	if (init_str_array(INIT_MAX_ARGC-1, argv+1, init_argv_str)) {
		printk("Too many ARGV strings for init_task.");
		return -EINVAL;
	}

	if (init_str_array(INIT_MAX_ENVC, envp, init_envp_str)) {
		printk("Too many ENVP strings for init_task.");
		return -EINVAL;
	}

	if (init_elf_image == NULL) {
		printk("Could not locate init ELF image.");
		return -EINVAL;
	}

	if ((status = aspace_create(INIT_ASPACE_ID, "init_task", &aspace_id))) {
		printk("Failed to create init aspace (status=%d).", status);
		return status;
	}

	/* TODO: remove */
	arch_aspace_activate(aspace_acquire(aspace_id));

	status =
	elf_load_executable(
		init_elf_image,       /* where I can access the ELF image */
		__pa(init_elf_image), /* where it is in physical memory */
		aspace_id,            /* the address space to map it into */
		(void *)0,            /* where I can access the aspace */
		PAGE_SIZE,            /* page size to map it with */
		&alloc_pmem           /* func to use to allocate phys mem */
	);
	if (status) {
		printk("Failed to map init ELF image (status=%d).", status);
		return status;
	}

	/* Setup the UNIX heap */
	heap_start  = round_up(elf_heap_start(init_elf_image), PAGE_SIZE);
	heap_extent = round_up(init_heap_size, PAGE_SIZE);
	status =
	make_region(
		aspace_id,
		heap_start,
		heap_extent,
		(VM_USER|VM_READ|VM_WRITE|VM_EXEC|VM_HEAP),
		PAGE_SIZE,
		"heap",
		&heap_pmem
	);
	if (status) {
		printk("Failed to create init heap (status=%d).", status);
		return status;
	}

	/* Setup the stack */
	stack_end    = SMARTMAP_ALIGN - PAGE_SIZE;  /* add one guard page */
	stack_start  = round_down(stack_end - init_stack_size, PAGE_SIZE);
	stack_extent = stack_end - stack_start;
	status = 
	make_region(
		aspace_id,
		stack_start,
		stack_extent,
		(VM_USER|VM_READ|VM_WRITE|VM_EXEC),
		PAGE_SIZE,
		"stack",
		&stack_pmem
	);
	if (status) {
		printk("Failed to create init stack (status=%d).", status);
		return status;
	}
	status =
	elf_init_stack(
		init_elf_image,
		__va(stack_pmem),  /* Where I can access it */
		stack_start,       /* Where it is in aspace */
		stack_extent,
		argv, envp,
		uid, gid,
		&stack_ptr
	);
	if (status) {
		printk("Failed to initialize init stack (status=%d).", status);
		return status;
	}

	/* Create the new task and add it to the target CPU's run queue */
	start_state.uid         = uid;
	start_state.gid         = gid;
	start_state.aspace_id   = aspace_id;
	start_state.entry_point = elf_entry_point(init_elf_image);
	start_state.stack_ptr   = stack_ptr;
	start_state.cpu_id      = this_cpu;
	start_state.cpumask     = NULL;

	if ((status = task_create(INIT_TASK_ID,
	                          "init_task", &start_state, &task_id))) {
		printk("Failed to create init task (status=%d).", status);
		return status;
	}

	return 0;
}
