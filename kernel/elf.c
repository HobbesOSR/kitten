#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/string.h>
#include <lwk/log2.h>
#include <lwk/aspace.h>
#include <lwk/elf.h>
#include <lwk/auxvec.h>
#include <lwk/cpuinfo.h>
#include <arch/uaccess.h>
#include <arch/param.h>

int
aspace_alloc_region(
	struct aspace *	aspace,
	vaddr_t		start,
	size_t		extent,
	vmflags_t	flags,
	vmpagesize_t	pagesz,
	const char *	name,
	void * (*alloc_mem)(size_t size, size_t alignment),
	void **		mem
)
{
	int status;
	void *_mem;

	/* Add the region to the address space */
	status = aspace_add_region(
			aspace->id,
			start,
			extent,
			flags,
			pagesz,
			name
	);
	if (status)
		return status;

	/* Allocate memory for the region */
	_mem = (*alloc_mem)(extent, pagesz);
	if (_mem == NULL)
		return -ENOMEM;

	/* Map the memory to the region */
	status = aspace_map_pmem(
			aspace->id,
			__pa(_mem),
			start,
			extent
	);
	if (status)
		return status;

	/* Return pointer to the kernel mapping of the memory allocated */
	if (mem)
		*mem = _mem;

	return 0;
}


/**
 * Kernel command line option for enabling more verbose ELF debugging.
 */
int elf_debug = 0;
param(elf_debug, int);



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
 *       [INOUT] task:        Task to load ELF image into.
 *       [IN]    elf_image:   Pointer to an ELF image.
 *       [IN]    heap_size:   Number of bytes to allocate for heap.
 *       [IN]    alloc_mem:   Function pointer to use to allocate memory for
 *                            the region.  alloc_mem() returns kernel virtual.
 *                            address of the memory allocated.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, task->aspace should be destroyed by caller.
 */
int
elf_load_executable(
	struct task_struct * task,
	void *		     elf_image,
	unsigned long	     heap_size,
	void * (*alloc_mem)(size_t size, size_t alignment)
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
	void *            mem;
	unsigned long     src, dst;
	unsigned long     load_addr = 0;
	int               load_addr_set = 0;

	BUG_ON(!task||!elf_image||!heap_size);

	aspace = task->aspace;
	BUG_ON(!aspace);

	/* Make sure ELF header is sane */
	ehdr = elf_image;
	if (elf_check_hdr(ehdr)) {
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
		status = aspace_alloc_region(
				aspace,
				start,
				extent,
				elf_pflags_to_vmflags(phdr->p_flags),
				PAGE_SIZE,
				"ELF PT_LOAD",
				alloc_mem,
				&mem
		);
		if (status)
			return status;

		/* Copy segment data from ELF image into the address space */
		dst = (unsigned long)mem + 
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
	status = aspace_alloc_region(
			aspace,
			start,
			extent,
			VM_READ | VM_WRITE | VM_USER,
			PAGE_SIZE,
			"Heap",
			alloc_mem,
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

	/* Remember the entry point */
	task->entry_point = elf_entry_point(elf_image);

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
 * Sets up the initial stack for a new task.  This includes storing the
 * argv[] argument array, envp[] environment array, and auxiliary info table
 * to the top of the user stack in the format that the C library expects them.
 * Eventually the arguments get passed to the application's
 * main(argc, argv, envp) function.
 * 
 * If successful, the initial stack pointer value that should be used when
 * starting the new task is returned in >stack_ptr.  
 *
 * This function sets up the initial stack as follows (stack grows down):
 *
 *                 Environment Strings
 *                 Argument Strings
 *                 Platform String
 *                 Auxiliary Info Table
 *                 envp[]
 *                 argv[]
 *                 argc
 *
 * Arguments:
 *       [INOUT] task:        Task to initialize.
 *       [IN]    stack_size:  Number of bytes to allocate for stack.
 *       [IN]    alloc_mem:   Function pointer to use to allocate memory for
 *                            the region.  alloc_mem() returns kernel virtual.
 *                            address of the memory allocated.
 *       [IN]    argv[]:      Array of pointers to argument strings.
 *       [IN]    envp[]:      Array of pointers to environment strings.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, task->aspace should be destroyed by caller.
 */
int
setup_initial_stack(
	struct task_struct * task,
	void *		     elf_image,
	unsigned long        stack_size,
	void * (*alloc_mem)(size_t size, size_t alignment),
	char *               argv[],
	char *               envp[]
)
{
	int status;
	unsigned long i, len;
	struct aspace * aspace;
	unsigned long start, end, extent;
	unsigned long sp;

	const char *platform_str = ELF_PLATFORM;
	struct aux_ent auxv[AT_ENTRIES];

	unsigned long argc = 0;
	unsigned long envc = 0;
	unsigned long auxc = 0;
	unsigned long arg_len = 0;
	unsigned long env_len = 0;
	unsigned long auxv_len = 0;
	unsigned long platform_str_len = 0;

	char __user *strings_uptr;
	char __user *platform_str_uptr;
	unsigned long __user *auxv_uptr;
	unsigned long __user *envp_uptr;
	unsigned long __user *argv_uptr;
	unsigned long __user *argc_uptr;
	
	BUG_ON(!task||!stack_size||!argv||!envp);

	aspace = task->aspace;
	BUG_ON(!aspace);

	/* Set up an address space region for the stack */
	end    = SMARTMAP_ALIGN - PAGE_SIZE; /* one guard page */
	start  = round_down(end - stack_size, PAGE_SIZE);
	extent = end - start;
	status = aspace_alloc_region(
			aspace,
			start,
			extent,
			VM_READ | VM_WRITE | VM_USER,
			PAGE_SIZE,
			"Stack",
			alloc_mem,
			NULL
	);
	if (status) {
		printk(KERN_WARNING
		       "Failed to allocate stack, size=0x%lx\n", stack_size);
		return status;
	}

	/* Set the initial stack pointer */
	sp = end;

	/* Count # of arguments and their total string length */
	while ((len = strlen(argv[argc])) != 0) {
		arg_len += (len + 1);
		++argc;
	}

	/* Count # of environment variables and their total string length */
	while ((len = strlen(envp[envc])) != 0) {
		env_len += (len + 1);
		++envc;
	}

	/* Calculate length of the arch's platform string, if there is one */
	if (platform_str)
		platform_str_len = strlen(platform_str) + 1;

	/* Make room on stack for arg, env, and platform strings */
	sp -= (arg_len + env_len + platform_str_len);
	strings_uptr = (void *) sp;

	/* Build the auxilliary information table */
	write_aux(auxv, auxc++, AT_HWCAP, ELF_HWCAP);
	write_aux(auxv, auxc++, AT_PAGESZ, ELF_EXEC_PAGESIZE);
	write_aux(auxv, auxc++, AT_CLKTCK, CLOCKS_PER_SEC);
	write_aux(auxv, auxc++, AT_PHDR, elf_phdr_table_addr(elf_image));
	write_aux(auxv, auxc++, AT_PHENT, sizeof(struct elf_phdr));
	write_aux(auxv, auxc++, AT_PHNUM, elf_num_phdrs(elf_image));
	write_aux(auxv, auxc++, AT_BASE, 0);
	write_aux(auxv, auxc++, AT_FLAGS, 0);
	write_aux(auxv, auxc++, AT_ENTRY, elf_entry_point(elf_image));
	write_aux(auxv, auxc++, AT_UID, task->uid);
	write_aux(auxv, auxc++, AT_EUID, task->uid);
	write_aux(auxv, auxc++, AT_GID, task->gid);
	write_aux(auxv, auxc++, AT_EGID, task->gid);
	write_aux(auxv, auxc++, AT_SECURE, 0);
	if (platform_str) {
		platform_str_uptr = strings_uptr;
		write_aux(auxv, auxc++, AT_PLATFORM,
		                        (unsigned long)platform_str_uptr);
	}
	write_aux(auxv, auxc++, AT_NULL, 0);

	/* Make room on stack for aux info table */
	auxv_len = auxc * sizeof(struct aux_ent);
	sp -= auxv_len;

	/* Make room on stack for argc, argv[], envp[] */
	sp -= ((1 + (argc + 1) + (envc + 1)) * sizeof(unsigned long));

	/* Align stack to 16-byte boundary */
	sp = round_down(sp, 16);

	/* Calculate stack address to store argc, argv[], envp[], and auxv[] */
	argc_uptr = (void *) sp;
	argv_uptr = argc_uptr + 1;
	envp_uptr = argv_uptr + argc + 1;
	auxv_uptr = envp_uptr + envc + 1;

	/* Store arch's platform string, if there is one */
	if (platform_str) {
		if (copy_to_user(strings_uptr, platform_str, platform_str_len))
			goto error;
		strings_uptr += platform_str_len;
	}

	/* Store the auxiliary information array */
	if (copy_to_user(auxv_uptr, auxv, auxv_len))
		goto error;

	/* Store argv[] */
	for (i = 0; i < argc; i++) {
		len = strlen(argv[i]) + 1;
		if (copy_to_user(strings_uptr, argv[i], len))
			goto error;
		if (put_user(strings_uptr, argv_uptr++))
			goto error;
		strings_uptr += len;
	}
	if (put_user(0, argv_uptr))  /* NULL terminate argv[] */
		goto error;

	/* Store envp[] */
	for (i = 0; i < envc; i++) {
		len = strlen(envp[i]) + 1;
		if (copy_to_user(strings_uptr, envp[i], len))
			goto error;
		if (put_user(strings_uptr, envp_uptr++))
			goto error;
		strings_uptr += len;
	}
	if (put_user(0, envp_uptr))  /* NULL terminate envp[] */
		goto error;

	/* Store argc */
	if (put_user(argc, argc_uptr))
		goto error;

	/* Remember the initial stack pointer */
	task->stack_ptr = sp;

	return 0;

error:
	status = aspace_del_region(aspace->id, start, extent);
	if (status)
		panic("aspace_del_region() failed, status=%d", status);
	return -EFAULT;
}

