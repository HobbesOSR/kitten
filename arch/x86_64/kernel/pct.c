#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/task.h>
#include <lwk/elf.h>
#include <lwk/aspace.h>
#include <lwk/ctype.h>
#include <lwk/ptrace.h>
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


/**
 * Address of the PCT ELF executable image.
 */
void *pct_elf_image;


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


#define MAX_NUM_STRS 16

struct pt_regs user_contexts[4];

extern void ATTRIB_NORET asm_run_task(struct pt_regs *context);

int
arch_load_pct(void)
{
	int status;
	union task_union *new_task;
	struct task_struct *pct_task;
	char *argv[MAX_NUM_STRS] = { "pct" };
	char *envp[MAX_NUM_STRS];
	unsigned long entry_point, stack_ptr;
	struct pt_regs *regs;

	if (init_str_array(MAX_NUM_STRS-1, argv+1, pct_argv_str))
		panic("Too many PCT ARGV strings.");

	if (init_str_array(MAX_NUM_STRS, envp, pct_envp_str))
		panic("Too many PCT ENVP strings.");

	BUG_ON(pct_elf_image == NULL);

	new_task = kmem_get_pages(TASK_ORDER);
	if (!new_task)
		panic("Failed to allocate new task.");

	pct_task = &new_task->task_info;
	pct_task->arch.addr_limit = PAGE_OFFSET;

	pct_task->aspace = aspace_create();
	BUG_ON(!pct_task->aspace);

	printk("switching to new aspace's page table.\n");
	aspace_activate(pct_task->aspace);

	aspace_dump(pct_task->aspace);

	status = elf_load_executable(
			pct_elf_image,
			pct_heap_size,
			pct_stack_size,
			argv,
			envp,
			pct_task->aspace,
			&entry_point,
			&stack_ptr
	         );
	if (status)
		return status;

	aspace_dump(pct_task->aspace);

	regs = &user_contexts[0];

	regs->cs                   = __USER_CS;
	regs->ss                   = __USER_DS;

	regs->rip                  = entry_point;
	regs->rsp                  = stack_ptr;
	regs->eflags               = (1 << 9 );

	/* Set the PCT as the current process */
	write_pda(pcurrent, pct_task);

	/* Initialize the PCT's FPU state */
	memset(
		&pct_task->arch.thread.i387.fxsave,
		0,
		sizeof(struct i387_fxsave_struct)
	);
	pct_task->arch.thread.i387.fxsave.cwd = 0x37f;
	pct_task->arch.thread.i387.fxsave.mxcsr = 0x1f80;

	/*
	 * Clear the task switch flag... if not cleared we'll get
	 * a "Device Not Available" interrupt when the first FPU/SSE
	 * instruction is executed.
	 */
	clts();

	/* PCT runs as root */
	pct_task->uid = 0;
	pct_task->gid = 0;

	asm_run_task(regs);  /* this does not return */
}


