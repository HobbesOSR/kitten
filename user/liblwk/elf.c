/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/liblwk.h>

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

vaddr_t
elf_entry_point(const void *elf_image)
{
	const struct elfhdr *ehdr = elf_image;
	return ehdr->e_entry;
}

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

unsigned int
elf_num_phdrs(const void *elf_image)
{
	const struct elfhdr *ehdr = elf_image;
	return ehdr->e_phnum;
}

