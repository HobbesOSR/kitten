/* Rewritten and vastly simplified by Rusty Russell for in-kernel
 * module loader:
 *   Copyright 2002 Rusty Russell <rusty@rustcorp.com.au> IBM Corporation
 */
#ifndef _LWK_KALLSYMS_H
#define _LWK_KALLSYMS_H

#define KSYM_NAME_LEN 127

unsigned long kallsyms_lookup_name(const char *name);

const char *kallsyms_lookup(unsigned long addr,
			    unsigned long *symbolsize,
			    unsigned long *offset,
			    char *namebuf);

#endif /* _LWK_KALLSYMS_H */
