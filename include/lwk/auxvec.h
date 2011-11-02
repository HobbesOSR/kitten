#ifndef _LWK_AUXVEC_H
#define _LWK_AUXVEC_H

/**
 * Auxiliary info table entry.  A table of these entries gets placed at the
 * top of a new task's stack so user-space can figure out things that are
 * difficult or impossible to determine otherwise (e.g., its base load
 * address).
 */
struct aux_ent {
	unsigned long id;
	unsigned long val;
};

#include <arch/auxvec.h>

/* Symbolic values for the entries in the auxiliary table
   put on the initial stack */
#define AT_NULL   0	/* end of vector */
#define AT_IGNORE 1	/* entry should be ignored */
#define AT_EXECFD 2	/* file descriptor of program */
#define AT_PHDR   3	/* program headers for program */
#define AT_PHENT  4	/* size of program header entry */
#define AT_PHNUM  5	/* number of program headers */
#define AT_PAGESZ 6	/* system page size */
#define AT_BASE   7	/* base address of interpreter */
#define AT_FLAGS  8	/* flags */
#define AT_ENTRY  9	/* entry point of program */
#define AT_NOTELF 10	/* program is not ELF */
#define AT_UID    11	/* real uid */
#define AT_EUID   12	/* effective uid */
#define AT_GID    13	/* real gid */
#define AT_EGID   14	/* effective gid */
#define AT_PLATFORM 15  /* string identifying CPU for optimizations */
#define AT_HWCAP  16    /* arch dependent hints at CPU capabilities */
#define AT_CLKTCK 17	/* frequency at which times() increments */

#define AT_SECURE 23   /* secure mode boolean */

#define AT_RANDOM 25	/* address of 16 random bytes */

#define AT_ENTRIES  22 /* Number of entries in the auxiliary table */

#endif /* _LWK_AUXVEC_H */
