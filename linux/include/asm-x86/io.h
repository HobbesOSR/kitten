#ifndef _ASM_X86_IO_H
#define _ASM_X86_IO_H

#include <arch/io.h>

extern int ioremap_change_attr(unsigned long vaddr, unsigned long size,
				unsigned long prot_val);

#endif
