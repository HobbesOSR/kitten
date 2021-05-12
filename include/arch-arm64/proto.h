/** \file
 * Miscellaneous architecture dependent things that don't fit anywhere else.
 *
 * \todo Should these be distributed to more appropriate headers?
 */
#ifndef _X86_64_PROTO_H
#define _X86_64_PROTO_H

#include <lwk/init.h>
#include <arch/ptrace.h>


/* misc architecture specific prototypes */

extern char boot_exception_stacks[];

extern unsigned long table_start, table_end;

void init_kernel_pgtables(unsigned long start, unsigned long end);

extern unsigned long end_pfn_map;

extern void init_resources(void);

extern void asm_syscall(void);
extern void asm_syscall_ignore(void);

extern unsigned long __phys_addr(unsigned long virt_addr);

void __init interrupts_init(void);

extern paddr_t fdt_start, fdt_end;
extern paddr_t initrd_start, initrd_end;

/** Low-level interrupt handler type */
typedef void (*idtvec_handler_t)(
	struct pt_regs *	regs,
	unsigned int		vector
);

extern void
set_idtvec_handler(
	unsigned int		vector,	//!< Interrupt vector number
	idtvec_handler_t	handler //!< Callback function
);



#endif
