#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/string.h>
#include <lwk/log2.h>
#include <lwk/aspace.h>
#include <lwk/elf.h>


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
 * Copies the argument array and environment array to the top of the user stack
 * in the format that the C library expects them. Eventually the arguments get
 * passed to the application's main(argc, argv, envp) function.
 *
 * This routine is careful to only write to the kernel mapping of the stack
 * (all writes are via kstack_top). All pointers written to the stack are
 * converted to user pointers relative to ustack_top.
 *
 * This returns the new value of the user stack pointer after the arguments and
 * environment have been pushed onto it.
 */
static unsigned long
copy_args_to_stack(
	unsigned long	kstack_top,  /* kernel virtual addr of stack top */
	unsigned long	ustack_top,  /* user virtual addr of stack top */
	char *		argv[], 
	char *		envp[]
)
{
	int argc = 0;
	int envc = 0;
	int space = 0;
	int len;
	int i;
	char *arg_sptr, *env_sptr;
	char **arg_ptr, **env_ptr;
	unsigned long stack = kstack_top;

	/* put a NULL pointer here for case were there are 0 args and envs */ 
	stack -= sizeof(char *);
	*((char *)stack) = 0;

	/* Calculate how much stack memory to reserve for argument strings */
	while ((len = strlen(argv[argc])) != 0) {
		space += len + 1;
		++argc;
	}
	arg_sptr = (char *)(stack -= round_up(space, 16));

	/* Calculate how much stack memory to reserve for environment strings */
	space = 0;
	while ((len = strlen(envp[envc])) != 0) {
		space += len + 1;
		++envc;
	}
	env_sptr = (char *)(stack -= round_up(space, 16));

	/* Push environment variables onto user stack */
	env_ptr  = (char **)(stack -= sizeof(void *) * (envc+1));
	for (i = 0; i < envc; i++) {
		/*
 		 * Write a pointer to the environment string to the 
 		 * stack... need to be careful to convert the kernel
 		 * pointer to a user pointer.
 		 */
		env_ptr[i] = (char *)(
		    ustack_top - (kstack_top - (unsigned long)env_sptr)
		);
		strcpy(env_sptr, envp[i]);
		env_sptr += strlen(envp[i]) + 1;
	} 
	env_ptr[i] = 0;

	/* Push arguments onto user stack */
	arg_ptr = (char **)(stack -= sizeof(void *) * (argc+1));
	for (i = 0; i < argc; i++) {
		/*
 		 * Write a pointer to the argument string to the stack...
 		 * need to be careful to convert the kernel pointer to a
 		 * user pointer.
 		 */
		arg_ptr[i] = (char *)(
		    ustack_top - (kstack_top - (unsigned long)arg_sptr)
		);
		strcpy(arg_sptr, argv[i]);
		arg_sptr += strlen(argv[i]) + 1;
	} 
	arg_ptr[i] = 0;

	/* Push the number of arguments onto user stack */
	*((int *)(stack -= sizeof(void *))) = argc;

	return ustack_top - (kstack_top - stack);
}


/**
 * Loads an ELF executable image into the specified address space. The address
 * space passed in (aspace) must have previously been created with
 * aspace_create(). 
 *
 * If successful, the initial entry point and stack pointer are returned to the
 * caller. The caller should use these when it launches the executable (e.g.,
 * for the x86-64 architecture set %rip=entry_point and %rsp=stack_ptr).
 *
 * Arguments:
 *       [IN]    elf_image:   Pointer to an ELF image
 *       [IN]    heap_size:   Number of bytes to allocate for heap
 *       [IN]    stack_size:  Number of bytes to allocate for stack
 *       [IN]    argv[]:      Array of pointers to argument strings
 *       [IN]    envp[]:      Array of pointers to environment strings
 *       [INOUT] aspace:      Address space to load ELF image into
 *       [OUT]   entry_point: Initial instruction pointer value
 *       [OUT]   stack_ptr:   Initial stack pointer value
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, the address space passed in should be destroyed.
 */
int
elf_load_executable(
	void *		elf_image,
	unsigned long	heap_size,
	unsigned long	stack_size,
	char *		argv[],
	char *		envp[],
	struct aspace *	aspace,
	unsigned long *	entry_point,
	unsigned long *	stack_ptr
)
{
	struct elfhdr *   ehdr;
	struct elf_phdr * phdr_array;
	struct elf_phdr * phdr;
	unsigned int      i;
	int               status;
	unsigned long     start, end, extent;
	unsigned int      num_load_segments = 0;
	unsigned long     heap_start = 0;
	unsigned long     stack_top;
	void *            kmem;
	unsigned long     src, dst;

	BUG_ON(!elf_image||!argv||!envp||!aspace||!entry_point||!stack_ptr);
	BUG_ON((heap_size == 0) || (stack_size == 0));

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
	if (status)
		return status;

	/* Copy arguments and environment to the top of the stack */
	stack_top = copy_args_to_stack(
			(unsigned long)kmem + extent,
			end,
			argv, envp
	);

	/* Return initial entry point and stack pointer */
	*entry_point = ehdr->e_entry;
	*stack_ptr   = stack_top;

	return 0;
}


