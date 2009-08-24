#ifndef _LINUX_KALLSYMS_H
#define _LINUX_KALLSYMS_H

#include <lwk/kallsyms.h>

static inline int
sprint_symbol(char *buffer, unsigned long address)
{
	return kallsyms_sprint_symbol(buffer, address);
}

static inline void
print_symbol(const char *fmt, unsigned long address)
{
	char buffer[KSYM_SYMBOL_LEN];
	sprint_symbol(buffer, address);
	printk(fmt, buffer);
}

/*
 * Pretty-print a function pointer.
 *
 * ia64 and ppc64 function pointers are really function descriptors,
 * which contain a pointer the real address.
 */
static inline void
print_fn_descriptor_symbol(const char *fmt, void *address)
{
	#if defined(CONFIG_IA64) || defined(CONFIG_PPC64)
        address = *(void **)address;
	#endif
	print_symbol(fmt, (unsigned long)address);
}

#endif
