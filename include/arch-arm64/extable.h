#ifndef _ASM_X86_64_EXTABLE_H
#define _ASM_X86_64_EXTABLE_H

#include <lwk/ptrace.h>

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * The nice thing about this mechanism is that the fixup code is completely
 * out of line with the main instruction path.  This means when everything
 * is well, we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */
struct exception_table_entry
{
        unsigned long insn;	/* Instruction addr that is allowed to fault */
	unsigned long fixup;	/* Fixup handler address */
};

extern int fixup_exception(struct pt_regs *regs);

#define ARCH_HAS_SEARCH_EXTABLE

#endif
