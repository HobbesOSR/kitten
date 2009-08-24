/* Rewritten and vastly simplified by Rusty Russell for in-kernel
 * module loader:
 *   Copyright 2002 Rusty Russell <rusty@rustcorp.com.au> IBM Corporation
 */
#ifndef _LWK_KALLSYMS_H
#define _LWK_KALLSYMS_H

#define KSYM_NAME_LEN 127

#define KSYM_SYMBOL_LEN (sizeof("%s+%#lx/%#lx [%s]") + (KSYM_NAME_LEN - 1) + \
				2*(BITS_PER_LONG*3/10) + 1)

extern unsigned long
kallsyms_lookup_name(
	const char *		name
);

extern const char *
kallsyms_lookup(
	kaddr_t			addr,
	unsigned long *		symbolsize,
	unsigned long *		offset,
	char *			namebuf
);

extern int
kallsyms_sprint_symbol(
	char *			buffer,
	kaddr_t			addr
);

#endif /* _LWK_KALLSYMS_H */
