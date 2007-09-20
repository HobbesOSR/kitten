#ifndef _X86_64_PROTO_H
#define _X86_64_PROTO_H

/* misc architecture specific prototypes */

extern void early_idt_handler(void);

extern char boot_cpu_stack[];
extern char boot_exception_stacks[];

extern unsigned long table_start, table_end;

void discover_ebda(void);

void init_kernel_pgtables(unsigned long start, unsigned long end);

extern unsigned long end_pfn_map;

extern void init_resources(void);

void __init zap_low_mappings(int cpu);

#endif
