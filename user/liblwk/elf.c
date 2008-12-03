/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/liblwk.h>
#include <lwk/ctype.h>

/**
 * Verifies that an ELF header is sane.
 * Returns 0 if header is sane and random non-zero values if header is insane.
 */
int
elf_check_hdr(const struct elfhdr *hdr)
{
	if (memcmp(hdr->e_ident, ELFMAG, SELFMAG) != 0) {
		print(TYPE_ERR "bad e_ident %#x\n",
		               *((unsigned int *)hdr->e_ident));
		return -1;
	}
	if (hdr->e_ident[EI_CLASS] != ELF_CLASS) {
		print(TYPE_ERR "bad e_ident[EI_CLASS] %#x\n",
		               (unsigned int)hdr->e_ident[EI_CLASS]);
		return -1;
	}
	if (hdr->e_ident[EI_DATA] != ELF_DATA) {
		print(TYPE_ERR "bad e_ident[EI_DATA] %#x\n",
		               (unsigned int)hdr->e_ident[EI_DATA]);
		return -1;
	}
	if (hdr->e_ident[EI_VERSION] != EV_CURRENT) {
		print(TYPE_ERR "bad e_ident[EI_VERSION] %#x\n",
		               (unsigned int)hdr->e_ident[EI_VERSION]);
		return -1;
	}
	if (hdr->e_ident[EI_OSABI] != ELF_OSABI) {
		print(TYPE_ERR "bad e_dent[EI_OSABI] %#x\n",
		               (unsigned int)hdr->e_ident[EI_OSABI]);
		return -1;
	}
	if (hdr->e_type != ET_EXEC) {
		print(TYPE_ERR "bad e_type %#x\n",
		               (unsigned int)hdr->e_type);
		return -1;
	}
	if (hdr->e_machine != ELF_ARCH) {
		print(TYPE_ERR "bad e_machine %#x\n",
		               (unsigned int)hdr->e_machine);
		return -1;
	}
	if (hdr->e_version != EV_CURRENT) {
		print(TYPE_ERR "bad e_version %#x\n",
		               (unsigned int)hdr->e_version);
		return -1;
	}
	if (hdr->e_flags != 0) {
		print(TYPE_ERR "bad e_flags %#x\n",
		               (unsigned int)hdr->e_flags);
		return -1;
	}

	return 0;
}

/**
 * Prints the contents of an ELF file header to the console.
 */
void
elf_print_elfhdr(const struct elfhdr *hdr)
{
	print(TYPE_NORM "ELF File Header:\n");
	print(TYPE_NORM "  type      %0#10x\n",  (unsigned int)  hdr->e_type      );
	print(TYPE_NORM "  machine   %0#10x\n",  (unsigned int)  hdr->e_machine   );
	print(TYPE_NORM "  version   %0#10x\n",  (unsigned int)  hdr->e_version   );
	print(TYPE_NORM "  entry     %0#18lx\n", (unsigned long) hdr->e_entry     );
	print(TYPE_NORM "  phoff     %0#18lx\n", (unsigned long) hdr->e_phoff     );
	print(TYPE_NORM "  shoff     %0#18lx\n", (unsigned long) hdr->e_shoff     );
	print(TYPE_NORM "  flags     %0#10x\n",  (unsigned int)  hdr->e_flags     );
	print(TYPE_NORM "  ehsize    %0#10x\n",  (unsigned int)  hdr->e_ehsize    );
	print(TYPE_NORM "  phentsize %0#10x\n",  (unsigned int)  hdr->e_phentsize );
	print(TYPE_NORM "  phnum     %0#10x\n",  (unsigned int)  hdr->e_phnum     );
	print(TYPE_NORM "  shentsize %0#10x\n",  (unsigned int)  hdr->e_shentsize );
	print(TYPE_NORM "  shnum     %0#10x\n",  (unsigned int)  hdr->e_shnum     );
	print(TYPE_NORM "  shstrndx  %0#10x\n",  (unsigned int)  hdr->e_shstrndx  );
}

/**
 * Prints the contents of an ELF program header to the console.
 */
void
elf_print_phdr(const struct elf_phdr *hdr)
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

	print(TYPE_NORM "ELF Program Segment Header:\n");
	print(TYPE_NORM "  type   %s\n", name);
	print(TYPE_NORM "  flags  %0#10x\n",  (unsigned int)  hdr->p_flags  );
	print(TYPE_NORM "  offset %0#18lx\n", (unsigned long) hdr->p_offset );
	print(TYPE_NORM "  vaddr  %0#18lx\n", (unsigned long) hdr->p_vaddr  );
	print(TYPE_NORM "  paddr  %0#18lx\n", (unsigned long) hdr->p_paddr  );
	print(TYPE_NORM "  filesz %0#18lx\n", (unsigned long) hdr->p_filesz );
	print(TYPE_NORM "  memsz  %0#18lx\n", (unsigned long) hdr->p_memsz  );
	print(TYPE_NORM "  align  %0#18lx\n", (unsigned long) hdr->p_align  );
}

/**
 * Converts ELF flags to the corresponding kernel memory subsystem flags.
 */
vmflags_t
elf_pflags_to_vmflags(unsigned int elf_pflags)
{
	vmflags_t vmflags = VM_USER;
	if ( elf_pflags & PF_R ) vmflags |= VM_READ;
	if ( elf_pflags & PF_W ) vmflags |= VM_WRITE;
	if ( elf_pflags & PF_X ) vmflags |= VM_EXEC;
	return vmflags;
}

/**
 * Determines an ELF executable's entry point... where to start executing.
 * Note: The address returned is in the context of the executing ELF
 *       image, not an address within the passed in elf_image.
 */
vaddr_t
elf_entry_point(const void *elf_image)
{
	const struct elfhdr *ehdr = elf_image;
	return ehdr->e_entry;
}

/**
 * Determines the address of an ELF executable's program header table.
 * Note: The address returned is in the context of the executing ELF
 *       image, not an address within the passed in elf_image.
 */
vaddr_t
elf_phdr_table_addr(const void *elf_image)
{
	const struct elfhdr *ehdr = elf_image;
	struct elf_phdr *phdr_array, *phdr;
	unsigned int i;

	phdr_array = (struct elf_phdr *)(elf_image + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = &phdr_array[i];
		if (phdr->p_type == PT_LOAD)
			return phdr->p_vaddr - phdr->p_offset + ehdr->e_phoff;
	}
	return 0;
}

/**
 * Returns the number of entries in an ELF executable's program header table.
 */
unsigned int
elf_num_phdrs(const void *elf_image)
{
	const struct elfhdr *ehdr = elf_image;
	return ehdr->e_phnum;
}

/**
 * Determines where the UNIX heap should start for a given ELF executable.
 * Note: The address returned is in the context of the executing ELF
 *       image, not an address relative to the passed in elf_image.
 */
vaddr_t
elf_heap_start(const void *elf_image)
{
	const struct elfhdr *ehdr;
	const struct elf_phdr *phdr_array;
	const struct elf_phdr *phdr;
	vaddr_t end, heap_start=0;
	size_t i;

	/* Locate the program header array (in this context) */
	ehdr       = elf_image;
	phdr_array = (struct elf_phdr *)(elf_image + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = &phdr_array[i];
		if (phdr->p_type != PT_LOAD)
			continue;

		/* Calculate the end of the LOAD segment in memory */
		end = phdr->p_vaddr + phdr->p_memsz;

		if (end > heap_start)
			heap_start = end;
	}

	return heap_start;
}

/**
 * Given an argument string like "arg1=foo arg2=bar", parses it into
 * an argv[] or envp[] style array of string pointers. Useful for
 * constructing the argv and envp arguments to elf_init_stack().
 */
int
elf_init_str_array(
	size_t  size,
	char *  ptrs[],
	char *  str
)
{ 
	size_t pos = 0;
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
 * Determines the value of the current stack pointer in the target
 * address space. The sp argument is the stack pointer in this context
 * (i.e., the context this code is executing in), stack_mapping is the
 * address of the stack in this context, stack_start is the address
 * of the stack in the target address space, and extent is the size
 * of the stack.
 */
static vaddr_t
sp_in_aspace(void *sp,
             void *stack_mapping, vaddr_t stack_start, size_t extent)
{
	vaddr_t stack_end    = (vaddr_t)stack_mapping + extent;
	vaddr_t stack_vend   = stack_start + extent;
	size_t  stack_offset = stack_end - (vaddr_t)sp;

	return stack_vend - stack_offset;
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
 *       [IN]  elf_image     The ELF executable, needed to setup aux info.
 *       [IN]  stack_mapping Where the stack is mapped in this context.
 *       [IN]  stack_start   Where the stack is located in the target aspace.
 *       [IN]  stack_extent  Size of the stack.
 *       [IN]  argv[]        Array of pointers to argument strings.
 *       [IN]  envp[]        Array of pointers to environment strings.
 *       [IN]  uid           User ID of the task
 *       [IN]  gid           Group ID of the task
 *       [IN]  hwcap         Hardware capability bitfield
 *                           (used for AT_HWCAP entry in aux info table)
 *       [OUT] stack_ptr     The initial stack pointer value for the new task.
 *                           (note this is an address in the target aspace)
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, stack may have been partially initialized
 */
int
elf_init_stack(
	void *    elf_image,
	void *    stack_mapping,
	vaddr_t   stack_start,
	size_t    stack_extent,
	char *    argv[],
	char *    envp[],
	uid_t     uid,
	gid_t     gid,
	uint32_t  hwcap,
	vaddr_t * stack_ptr
)
{
	size_t i, len;
	uintptr_t sp;
	const char *platform_str = ELF_PLATFORM;
	struct aux_ent auxv[AT_ENTRIES];
	size_t argc=0, envc=0, auxc=0;
	size_t arg_len=0, env_len=0, auxv_len=0;
	size_t platform_str_len=0;

	char *strings_sp;
	char *platform_str_sp;
	unsigned long *auxv_sp;
	unsigned long *envp_sp;
	unsigned long *argv_sp;
	unsigned long *argc_sp;

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
	sp = (uintptr_t)((vaddr_t)stack_mapping + stack_extent);
	sp -= (arg_len + env_len + platform_str_len);
	strings_sp = (void *) sp;

	/* Build the auxilliary information table */
	write_aux(auxv, auxc++, AT_HWCAP, hwcap);
	write_aux(auxv, auxc++, AT_PAGESZ, ELF_EXEC_PAGESIZE);
	write_aux(auxv, auxc++, AT_CLKTCK, 1000000l);
	write_aux(auxv, auxc++, AT_PHDR, elf_phdr_table_addr(elf_image));
	write_aux(auxv, auxc++, AT_PHENT, sizeof(struct elf_phdr));
	write_aux(auxv, auxc++, AT_PHNUM, elf_num_phdrs(elf_image));
	write_aux(auxv, auxc++, AT_BASE, 0);
	write_aux(auxv, auxc++, AT_FLAGS, 0);
	write_aux(auxv, auxc++, AT_ENTRY, elf_entry_point(elf_image));
	write_aux(auxv, auxc++, AT_UID, uid);
	write_aux(auxv, auxc++, AT_EUID, uid);
	write_aux(auxv, auxc++, AT_GID, gid);
	write_aux(auxv, auxc++, AT_EGID, gid);
	write_aux(auxv, auxc++, AT_SECURE, 0);
	if (platform_str) {
		platform_str_sp = strings_sp;
		write_aux(
			auxv, auxc++, AT_PLATFORM,
			sp_in_aspace(platform_str_sp,
			             stack_mapping, stack_start,
			             stack_extent)
		);
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
	argc_sp = (unsigned long *) sp;
	argv_sp = argc_sp  + 1;
	envp_sp = argv_sp  + argc + 1;
	auxv_sp = envp_sp  + envc + 1;

	/* Store arch's platform string, if there is one */
	if (platform_str) {
		memcpy(strings_sp, platform_str, platform_str_len);
		strings_sp += platform_str_len;
	}

	/* Store the auxiliary information array */
	memcpy(auxv_sp, auxv, auxv_len);

	/* Store argv[] */
	for (i = 0; i < argc; i++) {
		len = strlen(argv[i]) + 1;
		memcpy(strings_sp, argv[i], len);
		argv_sp[i] = sp_in_aspace(strings_sp,
		                          stack_mapping, stack_start,
		                          stack_extent),
		strings_sp += len;
	}
	argv_sp[i] = 0;  /* NULL terminate argv[] */

	/* Store envp[] */
	for (i = 0; i < envc; i++) {
		len = strlen(envp[i]) + 1;
		memcpy(strings_sp, envp[i], len);
		envp_sp[i] = sp_in_aspace(strings_sp,
		                          stack_mapping, stack_start,
		                          stack_extent),
		strings_sp += len;
	}
	envp_sp[i] = 0;  /* NULL terminate argv[] */

	/* Store argc */
	*argc_sp = argc;

	if (stack_ptr) {
		*stack_ptr = sp_in_aspace((void *)sp,
		                          stack_mapping, stack_start,
		                          stack_extent);
	}
	return 0;
}

/**
 * A "default" alloc_pmem() function for use with elf_load_executable().
 * A user may wish to define a custom replacement alloc_pmem() function
 * to, for example, keep track of the physical memory that is allocated.
 */
paddr_t
elf_dflt_alloc_pmem(size_t size, size_t alignment, uintptr_t arg)
{
	struct pmem_region result;

	if (pmem_alloc_umem(size, alignment, &result))
		return 0;

	/* Mark the memory as being used by the init task */
	result.type = PMEM_TYPE_INIT_TASK;
	pmem_update(&result);
	
	return result.start;
}

static int
load_writable_segment(
	void *            elf_image,
	struct elf_phdr * phdr,
	id_t              aspace_id,
	vaddr_t           start,
	size_t            extent,
	vmpagesize_t      pagesz,
	uintptr_t         alloc_pmem_arg,
	paddr_t (*alloc_pmem)(size_t size, size_t alignment, uintptr_t arg)
)
{
	int status;
	paddr_t pmem;
	vaddr_t local_start;
	vaddr_t src, dst;
	id_t my_aspace_id;

	/* Figure out my address space ID */
	if ((status = aspace_get_myid(&my_aspace_id)))
		return status;

	/* Allocate physical memory for the segment */
	if (!(pmem = alloc_pmem(extent, pagesz, alloc_pmem_arg)))
		return -ENOMEM;

	/* Map the segment into the target address space */
	status =
	aspace_map_region(
		aspace_id,
		start,
		extent,
		elf_pflags_to_vmflags(phdr->p_flags),
		pagesz,
		"ELF",
		pmem
	);
	if (status)
		return status;

	/* Map the segment into this address space */
	status =
	aspace_map_region_anywhere(
		my_aspace_id,
		&local_start,
		extent,
		(VM_USER|VM_READ|VM_WRITE),
		pagesz,
		"temporary",
		pmem
	);
	if (status)
		return status;

	/* Copy segment data from ELF image into the target address space
 	 * (via its temporary mapping in our address space) */
	dst = local_start + (phdr->p_vaddr - start);
	src = (vaddr_t)elf_image + phdr->p_offset;
	memcpy((void *)dst, (void *)src, phdr->p_filesz);

	/* Unmap the segment from this address space */
	status = aspace_del_region(my_aspace_id, local_start, extent);
	if (status)
		return status;

	return 0;
}

static int
load_readonly_segment(
	paddr_t           elf_image_paddr,
	struct elf_phdr * phdr,
	id_t              aspace_id,
	vaddr_t           start,
	size_t            extent,
	vmpagesize_t      pagesz
)
{
	return aspace_map_region(
		aspace_id,
		start,
		extent,
		elf_pflags_to_vmflags(phdr->p_flags),
		pagesz,
		"ELF (mapped)",
		elf_image_paddr +
			round_down(phdr->p_offset, pagesz)
	);
}

/**
 * Loads an ELF executable image into the specified address space. 
 *
 * Arguments:
 *       [IN]  elf_image:        Location of ELF image in this address space.
 *       [IN]  elf_image_paddr:  Location of ELF image in physical memory.
 *       [IN]  aspace_id:        Address space to load ELF image into.
 *       [IN]  pagesz:           Page size to use when mapping ELF image.
 *       [IN]  alloc_pmem_arg:   Argument to pass to alloc_pmem().
 *       [IN]  alloc_pmem:       Function pointer to use to allocate physical
 *                               memory for the region.  alloc_mem() returns 
 *                               the physical address of the memory allocated.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, the target address space is left in an
 *                undefined state and should be destroyed.
 */
int
elf_load_executable(
	void *       elf_image,
	paddr_t      elf_image_paddr,
	id_t         aspace_id,
	vmpagesize_t pagesz,
	uintptr_t    alloc_pmem_arg,
	paddr_t (*alloc_pmem)(size_t size, size_t alignment, uintptr_t arg)
)
{
	struct elfhdr *   ehdr;
	struct elf_phdr * phdr_array;
	struct elf_phdr * phdr;
	size_t            i;
	vaddr_t           start, end;
	size_t            extent;
	size_t            num_load_segments=0;
	int               status;

	/* Locate the program header array (in this context) */
	ehdr       = elf_image;
	phdr_array = (struct elf_phdr *)(elf_image + ehdr->e_phoff);

	/* Set up a region for each program segment */
	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = &phdr_array[i];
		if (phdr->p_type != PT_LOAD)
			continue;

		/* Calculate the segment's bounds */
		start  = round_down(phdr->p_vaddr, pagesz);
		end    = round_up(phdr->p_vaddr + phdr->p_memsz, pagesz);
		extent = end - start;

		if (phdr->p_flags & PF_W) {
			/* Writable segments must be copied into the
			 * target address space */
			status =
			load_writable_segment(
				elf_image,
				phdr,
				aspace_id,
				start,
				extent,
				pagesz,
				alloc_pmem_arg,
				alloc_pmem
			);
			if (status)
				return status;
		} else {
			/* Read-only segments are mapped directly
			 * from the ELF image */
			status =
			load_readonly_segment(
				elf_image_paddr,
				phdr,
				aspace_id,
				start,
				extent,
				pagesz
			);
			if (status)
				return status;
		}

		++num_load_segments;
	}

	return (num_load_segments) ? 0 : -ENOENT;
}

static int
make_region(
	id_t         aspace_id,
	vaddr_t      start,
	size_t       extent,
	vmflags_t    flags,
	vmpagesize_t pagesz,
	const char * name,
	uintptr_t    alloc_pmem_arg,
	paddr_t (*alloc_pmem)(size_t size, size_t alignment, uintptr_t arg),
	paddr_t *    pmem
)
{
	int status;

	*pmem = alloc_pmem(extent, pagesz, alloc_pmem_arg);
	if (*pmem == 0) {
		print("Failed to allocate physical memory for %s.", name);
		return -ENOMEM;
	}

	status = aspace_map_region(aspace_id, start, extent,
	                           flags, pagesz,
	                           name, *pmem);
	if (status) {
		print("Failed to map physical memory for %s (status=%d).",
		       name, status);
		return status;
	}

	return 0;
}

/**
 * Maximum number of arguments and environment variables that may
 * be passed to the new task created by elf_load().
 */
#define MAX_ARGC 32
#define MAX_ENVC 32

/**
 * Kitchen-sink ELF image load function.
 * If something more custom is desired, this function can be used as a guide
 * for what needs to be done to load an ELF executable and setup the
 * accompanying address space.
 */
int
elf_load(
	void *          elf_image,
	paddr_t         elf_image_paddr,
	const char *    name,
	id_t            desired_aspace_id,
	vmpagesize_t    pagesz,
	size_t          heap_size,
	size_t          stack_size,
	char *          argv_str,
	char *          envp_str,
	start_state_t * start_state,
	uintptr_t       alloc_pmem_arg,
	paddr_t (*alloc_pmem)(size_t size, size_t alignment, uintptr_t arg)
)
{
	int status;
	char *argv[MAX_ARGC] = { (char *)name };
	char *envp[MAX_ENVC];
	id_t my_aspace_id, aspace_id;
	vaddr_t heap_start, stack_start, stack_end, stack_ptr;
	vaddr_t local_stack_start;
	size_t heap_extent, stack_extent;
	paddr_t heap_pmem, stack_pmem;
	uint32_t hwcap;

	if (!elf_image || !start_state || !alloc_pmem)
		return -EINVAL;

	if (elf_init_str_array(MAX_ARGC-1, argv+1, argv_str)) {
		print("Too many ARGV strings.");
		return -EINVAL;
	}

	if (elf_init_str_array(MAX_ENVC, envp, envp_str)) {
		print("Too many ENVP strings.");
		return -EINVAL;
	}

	if ((status = aspace_create(desired_aspace_id, "init_task", &aspace_id))) {
		print("Failed to create aspace (status=%d).", status);
		return status;
	}

	/* Load the ELF executable's LOAD segments */
	status =
	elf_load_executable(
		elf_image,       /* where I can access the ELF image */
		elf_image_paddr, /* where it is in physical memory */
		aspace_id,       /* the address space to map it into */
		pagesz,          /* page size to map it with */
		0,               /* arg to pass to alloc_pmem */
		alloc_pmem       /* func to use to allocate phys mem */
	);
	if (status) {
		print("Failed to load ELF image (status=%d).", status);
		return status;
	}

	/* Create the UNIX heap */
	heap_start  = round_up(elf_heap_start(elf_image), pagesz);
	heap_extent = round_up(heap_size, pagesz);
	status =
	make_region(
		aspace_id,
		heap_start,
		heap_extent,
		(VM_USER|VM_READ|VM_WRITE|VM_EXEC|VM_HEAP),
		pagesz,
		"heap",
		alloc_pmem_arg,
		alloc_pmem,
		&heap_pmem
	);
	if (status) {
		print("Failed to create heap (status=%d).", status);
		return status;
	}

	/* Create the stack region */
	stack_end    = SMARTMAP_ALIGN;
	stack_start  = round_down(stack_end - stack_size, pagesz);
	stack_extent = stack_end - stack_start;
	status = 
	make_region(
		aspace_id,
		stack_start,
		stack_extent,
		(VM_USER|VM_READ|VM_WRITE|VM_EXEC),
		pagesz,
		"stack",
		alloc_pmem_arg,
		alloc_pmem,
		&stack_pmem
	);
	if (status) {
		print("Failed to create stack (status=%d).", status);
		return status;
	}

	/* Map the stack region into this address space */
	if ((status = aspace_get_myid(&my_aspace_id)))
		return status;
	status =
	aspace_map_region_anywhere(
		my_aspace_id,
		&local_stack_start,
		stack_extent,
		(VM_USER|VM_READ|VM_WRITE),
		pagesz,
		"temporary",
		stack_pmem
	);
	if (status) {
		print("Failed to map stack locally (status=%d).", status);
		return status;
	}

	/* Initialize the stack */
	status = elf_hwcap(start_state->cpu_id, &hwcap);
	if (status) {
		print("Failed to get hw capabilities (status=%d).", status);
		return status;
	}
	status =
	elf_init_stack(
		elf_image,
		(void *)local_stack_start,  /* Where I can access it */
		stack_start,                /* Where it is in target aspace */
		stack_extent,
		argv, envp,
		start_state->uid, start_state->gid,
		hwcap,
		&stack_ptr
	);
	if (status) {
		print("Failed to initialize stack (status=%d).", status);
		return status;
	}

	start_state->aspace_id   = aspace_id;
	start_state->entry_point = elf_entry_point(elf_image);
	start_state->stack_ptr   = stack_ptr;

	return 0;
}
