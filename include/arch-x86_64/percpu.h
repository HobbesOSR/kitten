#ifndef _X86_64_PERCPU_H
#define _X86_64_PERCPU_H
#include <lwk/compiler.h>

/* Same as asm-generic/percpu.h, except that we store the per cpu offset
   in the PDA. Longer term the PDA and every per cpu variable
   should be just put into a single section and referenced directly
   from %gs */

#include <arch/pda.h>

#define __per_cpu_offset(cpu) (cpu_pda(cpu)->data_offset)
#define __my_cpu_offset() read_pda(data_offset)

/* Separate out the type, so (int[3], foo) works. */
#define DEFINE_PER_CPU(type, name) \
    __attribute__((__section__(".data.percpu"))) __typeof__(type) per_cpu__##name

/* var is in discarded region: offset to particular copy we want */
#define per_cpu(var, cpu) (*RELOC_HIDE(&per_cpu__##var, __per_cpu_offset(cpu)))
#define __get_cpu_var(var) (*RELOC_HIDE(&per_cpu__##var, __my_cpu_offset()))

extern void setup_per_cpu_areas(void);

#define DECLARE_PER_CPU(type, name) extern __typeof__(type) per_cpu__##name

#endif /* _X86_64_PERCPU_H */
