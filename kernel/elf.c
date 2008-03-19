#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/string.h>
#include <lwk/log2.h>
#include <lwk/aspace.h>
#include <lwk/elf.h>
#include <lwk/auxvec.h>
#include <arch/uaccess.h>
#include <arch/param.h>


/**
 * Kernel command line option for enabling more verbose ELF debugging.
 */
int elf_debug = 0;
param(elf_debug, int);


/**
 * Verifies that an ELF header is sane.
 * Returns 0 if header is sane and random non-zero values if header is insane.
 */
static int
elf_check(struct elfhdr *hdr)
{
	if (memcmp(hdr->e_ident, ELFMAG, SELFMAG) != 0) {
		printk(KERN_ERR "bad e_ident %#x\n",
		                *((unsigned int *)hdr->e_ident));
		return -1;
	}
	if (hdr->e_ident[EI_CLASS] != ELF_CLASS) {
		printk(KERN_ERR "bad e_ident[EI_CLASS] %#x\n",
		                (unsigned int)hdr->e_ident[EI_CLASS]);
		return -1;
	}
	if (hdr->e_ident[EI_DATA] != ELF_DATA) {
		printk(KERN_ERR "bad e_ident[EI_DATA] %#x\n",
		                (unsigned int)hdr->e_ident[EI_DATA]);
		return -1;
	}
	if (hdr->e_ident[EI_VERSION] != EV_CURRENT) {
		printk(KERN_ERR "bad e_ident[EI_VERSION] %#x\n",
		                (unsigned int)hdr->e_ident[EI_VERSION]);
		return -1;
	}
	if (hdr->e_ident[EI_OSABI] != ELF_OSABI) {
		printk(KERN_ERR "bad e_dent[EI_OSABI] %#x\n",
		                (unsigned int)hdr->e_ident[EI_OSABI]);
		return -1;
	}
	if (hdr->e_type != ET_EXEC) {
		printk(KERN_ERR "bad e_type %#x\n",
		                (unsigned int)hdr->e_type);
		return -1;
	}
	if (hdr->e_machine != ELF_ARCH) {
		printk(KERN_ERR "bad e_machine %#x\n",
		                (unsigned int)hdr->e_machine);
		return -1;
	}
	if (hdr->e_version != EV_CURRENT) {
		printk(KERN_ERR "bad e_version %#x\n",
		                (unsigned int)hdr->e_version);
		return -1;
	}
	if (hdr->e_flags != 0) {
		printk(KERN_ERR "bad e_flags %#x\n",
		                (unsigned int)hdr->e_flags);
		return -1;
	}

	return 0;
}


/**
 * Converts ELF flags to the corresponding kernel memory subsystem flags.
 */
static uint32_t
elf_to_vm_flags(unsigned int elf_flags)
{
	uint32_t vm_flags = VM_USER;

	if ( elf_flags & PF_R ) vm_flags |= VM_READ;
	if ( elf_flags & PF_W ) vm_flags |= VM_WRITE;
	if ( elf_flags & PF_X ) vm_flags |= VM_EXEC;

	return vm_flags;
}


/**
 * Prints the contents of an ELF program header to the console.
 */
static void
elf_print_phdr(struct elf_phdr *hdr)
{
	char *name;

	switch (hdr->p_type) {
		case PT_NULL:    name = "NULL";    break;
		case PT_LOAD:    name = "LOAD";    break;
		case PT_DYNAMIC: name = "DYNAMIC"; break;
		case PT_INTERP:  name = "INTERP";  break;
		case PT_NOTE:    name = "NOTE";    break;
		case PT_SHLIB:   name = "SHLIB";   break;
		case PT_PHDR:    name = "PHDR";    break;
		case PT_LOPROC:  name = "LOPROC";  break;
		case PT_HIPROC:  name = "HIPROC";  break;
		default:         name = "UNDEFINED TYPE";
	}

	printk(KERN_DEBUG "ELF Program Segment Header:\n");
	printk(KERN_DEBUG "  type   %s\n", name);
	printk(KERN_DEBUG "  flags  %0#10x\n",  (unsigned int)  hdr->p_flags  );
	printk(KERN_DEBUG "  offset %0#18lx\n", (unsigned long) hdr->p_offset );
	printk(KERN_DEBUG "  vaddr  %0#18lx\n", (unsigned long) hdr->p_vaddr  );
	printk(KERN_DEBUG "  paddr  %0#18lx\n", (unsigned long) hdr->p_paddr  );
	printk(KERN_DEBUG "  filesz %0#18lx\n", (unsigned long) hdr->p_filesz );
	printk(KERN_DEBUG "  memsz  %0#18lx\n", (unsigned long) hdr->p_memsz  );
	printk(KERN_DEBUG "  align  %0#18lx\n", (unsigned long) hdr->p_align  );
}


/**
 * Loads an ELF executable image into the specified task's address space. The
 * address space passed in (task->aspace) must have been previously created
 * with aspace_create(). 
 *
 * If successful, the initial entry point is returned in
 * task->aspace->entry_point.  The caller will need the entry point address
 * when it starts the task running (e.g., for the x86-64 architecture set
 * %rip=task->aspace->entry_point).
 *
 * Arguments:
 *       [INOUT] task:        Task to load ELF image into 
 *       [IN]    elf_image:   Pointer to an ELF image
 *       [IN]    heap_size:   Number of bytes to allocate for heap
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, task->aspace should be destroyed by caller.
 */
int
elf_load_executable(
	struct task_struct * task,
	void *		     elf_image,
	unsigned long	     heap_size
)
{
	struct aspace *   aspace;
	struct elfhdr *   ehdr;
	struct elf_phdr * phdr_array;
	struct elf_phdr * phdr;
	unsigned int      i;
	int               status;
	unsigned long     start, end, extent;
	unsigned int      num_load_segments = 0;
	unsigned long     heap_start = 0;
	void *            kmem;
	unsigned long     src, dst;
	unsigned long     load_addr = 0;
	int               load_addr_set = 0;

	BUG_ON(!task||!elf_image||!heap_size);

	aspace = task->aspace;
	BUG_ON(!aspace);

	/* Make sure ELF header is sane */
	ehdr = elf_image;
	if (elf_check(ehdr)) {
		printk(KERN_ERR "Bad ELF image.\n");
		return -EINVAL;
	}

	/* Locate the program header array */
	phdr_array = (struct elf_phdr *)(elf_image + ehdr->e_phoff);

	/* Set up a region for each program segment */
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr_array[i].p_type != PT_LOAD)
			continue;
		++num_load_segments;

		phdr = &phdr_array[i];
		if (elf_debug)
			elf_print_phdr(phdr);

		/* Calculate the segment's bounds */
		start  = round_down(phdr->p_vaddr, PAGE_SIZE);
		end    = round_up(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
		extent = end - start;

		/* Need to keep track of where heap should start */
		if (end > heap_start)
			heap_start = end;

		/* Remember the first load address */
		if (!load_addr_set) {
			load_addr_set = 1;
			load_addr = phdr->p_vaddr - phdr->p_offset;
		}

		/* Set up an address space region for the program segment */
		status = aspace_kmem_alloc_region(
				aspace,
				start,
				extent,
				elf_to_vm_flags(phdr->p_flags),
				PAGE_SIZE,
				"ELF PT_LOAD",
				&kmem
		);
		if (status)
			return status;

		/* Copy segment data from ELF image into the address space */
		dst = (unsigned long)kmem + 
			(phdr->p_vaddr - round_down(phdr->p_vaddr, PAGE_SIZE));
		src = (unsigned long)elf_image + phdr->p_offset;
		memcpy(
			(void *)dst,
			(void *)src,
			phdr->p_filesz
		);
	}

	/* There must be at least one load segment in a valid ELF executable */
	if (num_load_segments == 0) {
		printk(KERN_WARNING "No load segments found in ELF image.\n");
		return -EINVAL;
	}

	/* Set up an address space region for the heap */
	start  = heap_start;
	end    = round_up(heap_start + heap_size, PAGE_SIZE);
	extent = end - start;	
	status = aspace_kmem_alloc_region(
			aspace,
			start,
			extent,
			VM_READ | VM_WRITE | VM_USER,
			PAGE_SIZE,
			"Heap",
			NULL
	);
	if (status)
		return status;

	/* Remember the heap boundaries */
	aspace->heap_start = start;
	aspace->heap_end   = end;

	/* The UNIX data segment grows up from the bottom of the heap */
	aspace->brk = aspace->heap_start;

	/* Anonymous mmap() requests are satisfied from the top of the heap */
	aspace->mmap_brk = aspace->heap_end;

	/* Remember ELF load information */
	aspace->entry_point = (void __user *)ehdr->e_entry;
	aspace->e_phdr      = (void __user *)(load_addr + ehdr->e_phoff);
	aspace->e_phnum     = ehdr->e_phnum;

	return 0;
}


/**
 * Pushes the architecture's platform string to the user stack.
 * User-level uses this to determine what type of platform it is running on.
 */
static int
push_platform_string(
	void ** stack_ptr,
	void ** platform_ptr
)
{
	unsigned long sp = (unsigned long)(*stack_ptr);
	const char *platform_str = ELF_PLATFORM;
	unsigned long len;

	/* Nothing to do if the arch doesn't have a platform string */
	if (!platform_str)
		return 0;

	/* Push the platform string onto the user stack */
	len = strlen(platform_str) + 1;
	sp -= round_up(len, 16);
	if (copy_to_user((void __user *)sp, platform_str, len))
		return -ENOMEM;

	*stack_ptr = *platform_ptr = (void *)sp;
	return 0;
}


/**
 * Writes an auxiliary info table entry.
 */
static void
write_aux(
	struct aux_ent * table,
	int              index,
	unsigned long    id,
	unsigned long    val
)
{
	table[index].id  = id;
	table[index].val = val;
}


/**
 * Pushes auxiliary information onto the user stack.  Some of this information
 * is difficult for user-space to obtain by different means, so the kernel
 * passes it on the top of the user's stack.  
 */
static int
push_aux_info(
	void **              stack_ptr,
	struct task_struct * task,
	void *               platform_ptr
)
{
	unsigned long sp = (unsigned long)(*stack_ptr);
	struct aux_ent auxv[AT_ENTRIES];
	int auxc = 0;
	unsigned long len;

	/* Build the auxiliary info table */
	write_aux(auxv, auxc++, AT_HWCAP, ELF_HWCAP);
	write_aux(auxv, auxc++, AT_PAGESZ, ELF_EXEC_PAGESIZE);
	write_aux(auxv, auxc++, AT_CLKTCK, CLOCKS_PER_SEC);
	write_aux(auxv, auxc++, AT_PHDR,
	                        (unsigned long)task->aspace->e_phdr);
	write_aux(auxv, auxc++, AT_PHENT, sizeof(struct elf_phdr));
	write_aux(auxv, auxc++, AT_PHNUM, task->aspace->e_phnum);
	write_aux(auxv, auxc++, AT_BASE, 0);
	write_aux(auxv, auxc++, AT_FLAGS, 0);
	write_aux(auxv, auxc++, AT_ENTRY,
	                        (unsigned long)task->aspace->entry_point);
	write_aux(auxv, auxc++, AT_UID, task->uid);
	write_aux(auxv, auxc++, AT_EUID, task->uid);
	write_aux(auxv, auxc++, AT_GID, task->gid);
	write_aux(auxv, auxc++, AT_EGID, task->gid);
	write_aux(auxv, auxc++, AT_SECURE, 0);
	if (platform_ptr) {
		write_aux(auxv, auxc++, AT_PLATFORM,
		                        (unsigned long)platform_ptr);
	}
	write_aux(auxv, auxc++, AT_NULL, 0);

	/* Push the auxiliary info table onto the user stack */
	len = auxc * sizeof(struct aux_ent);
	sp -= round_up(len, 16);
	if (copy_to_user((void __user *)sp, auxv, len))
		return -ENOMEM;

	*stack_ptr = (void *)sp;
	return 0;
}


/**
 * Pushes arguments and environment variables onto the user stack.
 */
static int
push_args_and_env(
	void ** stack_ptr,
	char *  argv[],
	char *  envp[]
)
{
	unsigned long sp = (unsigned long)(*stack_ptr);
	int i, idx;
	unsigned long argc = 0;
	unsigned long envc = 0;
	unsigned long space, len;
	char __user *argstr_uptr;
	char __user *envstr_uptr;
	unsigned long __user *stack;

	/* Calculate how much stack memory is needed for argument strings */
	space = 0;
	while ((len = strlen(argv[argc])) != 0) {
		space += (len + 1);
		++argc;
	}
	argstr_uptr = (char *)(sp -= round_up(space, 16));

	/* Calculate how much stack memory is needed for environment strings */
	space = 0;
	while ((len = strlen(envp[envc])) != 0) {
		space += (len + 1);
		++envc;
	}
	envstr_uptr = (char *)(sp -= round_up(space, 16));

	/* Make room on the stack for argv[], envp[], and argc */
	space = ((argc + envc + 3) * sizeof(unsigned long));
	sp -= round_up(space, 16);

	/* Get ready to push values onto stack */
	stack = (unsigned long __user *)sp;
	idx = 0;

	/* Push argc onto the user stack */
	if (put_user(argc, &stack[idx++]))
		return -EFAULT;

	/* Push argv[] onto the user stack */
	for (i = 0; i < argc; i++) {
		len = strlen(argv[i]) + 1;
		/* Copy arg string to user stack */
		if (copy_to_user(argstr_uptr, argv[i], len))
			return -EFAULT;
		/* argv[] pointer to arg string */
		if (put_user(argstr_uptr, &stack[idx++]))
			return -EFAULT;
		argstr_uptr += len;
	}
	/* NULL terminate argv[] */
	if (put_user(0, &stack[idx++]))
		return -EFAULT;
	
	/* Push envp[] onto the user stack */
	for (i = 0; i < envc; i++) {
		len = strlen(envp[i]) + 1;
		/* Copy env string to user stack */
		if (copy_to_user(envstr_uptr, envp[i], len))
			return -EFAULT;
		/* envp[] pointer to env string */
		if (put_user(envstr_uptr, &stack[idx++]))
			return -EFAULT;
		envstr_uptr += len;
	}
	/* NULL terminate envp[] */
	if (put_user(0, &stack[idx++]))
		return -EFAULT;

	*stack_ptr = (void *)sp;
	return 0;
}


/**
 * Sets up the initial stack for a new task.  This includes storing the
 * argv[] argument array, envp[] environment array, and auxiliary info table
 * to the top of the user stack in the format that the C library expects them.
 * Eventually the arguments get passed to the application's
 * main(argc, argv, envp) function.
 * 
 * If successful, the initial stack pointer value that should be used when
 * starting the new task is returned in task->aspace->stack_ptr.  
 *
 * This function sets up the initial stack as follows (stack grows down):
 *
 *                 Platform String
 *                 Auxiliary Info Table
 *                 Environment Strings
 *                 Argument Strings
 *                 envp[]
 *                 argv[]
 *                 argc
 *
 * Arguments:
 *       [INOUT] task:        Task to initialize
 *       [IN]    stack_size:  Number of bytes to allocate for stack
 *       [IN]    argv[]:      Array of pointers to argument strings
 *       [IN]    envp[]:      Array of pointers to environment strings
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, task->aspace should be destroyed by caller.
 */
int
setup_initial_stack(
	struct task_struct * task,
	unsigned long        stack_size,
	char *               argv[],
	char *               envp[]
)
{
	int status, status2;
	struct aspace * aspace;
	unsigned long start, end, extent;
	void * kmem;
	void __user * stack_ptr;
	void __user * platform_ptr = NULL;

	BUG_ON(!task||!stack_size||!argv||!envp);

	aspace = task->aspace;
	BUG_ON(!aspace);

	/* Set up an address space region for the stack */
	end    = PAGE_OFFSET - PAGE_SIZE; /* one guard page */
	start  = round_down(end - stack_size, PAGE_SIZE);
	extent = end - start;
	status = aspace_kmem_alloc_region(
			aspace,
			start,
			extent,
			VM_READ | VM_WRITE | VM_USER,
			PAGE_SIZE,
			"Stack",
			&kmem
	);
	if (status) {
		printk(KERN_WARNING
		       "Failed to allocate stack, size=0x%lx\n", stack_size);
		return status;
	}

	/* Set the initial user stack pointer */
	stack_ptr = (void *)end;

	status = push_platform_string(&stack_ptr, &platform_ptr);
	if (status)
		goto error;

	status = push_aux_info(&stack_ptr, task, platform_ptr);
	if (status)
		goto error;

	status = push_args_and_env(&stack_ptr, argv, envp);
	if (status)
		goto error;

	/* Remember the initial stack pointer */
	aspace->stack_ptr = stack_ptr;

	return 0;

error:
	status2 = aspace_del_region(aspace, start, extent);
	if (status2)
		panic("aspace_del_region() failed, status=%d", status2);
	return status;
}

