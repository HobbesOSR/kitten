#ifndef _ARM64_PERCPU_H
#define _ARM64_PERCPU_H
#include <lwk/compiler.h>

/* Same as asm-generic/percpu.h, except that we store the per cpu offset
   in the PDA. Longer term the PDA and every per cpu variable
   should be just put into a single section and referenced directly*/

#include <arch/pda.h>
/*
 * Declaration/definition used for per-CPU variables that must be read mostly.
 */
#define DECLARE_PER_CPU_READ_MOSTLY(type, name)			\
	DECLARE_PER_CPU(type, name)
//	DECLARE_PER_CPU_SECTION(type, name, "..read_mostly")

#define DEFINE_PER_CPU_READ_MOSTLY(type, name)				\
	DEFINE_PER_CPU(type, name)
//	DEFINE_PER_CPU_SECTION(type, name, "..read_mostly")

#define __per_cpu_offset(cpu) (cpu_pda(cpu)->data_offset)
#define __my_cpu_offset() read_pda(data_offset)

/* Separate out the type, so (int[3], foo) works. */
#define DEFINE_PER_CPU(type, name) \
    __attribute__((__section__(".data.percpu"))) __typeof__(type) per_cpu__##name

/* var is in discarded region: offset to particular copy we want */
#define per_cpu(var, cpu) (*RELOC_HIDE(&per_cpu__##var, __per_cpu_offset(cpu)))
#define __get_cpu_var(var) (*RELOC_HIDE(&per_cpu__##var, __my_cpu_offset()))
#define raw_cpu_read(var) __get_cpu_var(var)
#define raw_cpu_write(var, val) (__get_cpu_var(var) = val)

#define DECLARE_PER_CPU(type, name) extern __typeof__(type) per_cpu__##name

#endif /* _X86_64_PERCPU_H */
