#ifndef _X86_64_PROTO_H
#define _X86_64_PROTO_H

/* misc architecture specific prototypes */

extern void early_idt_handler(void);

extern char boot_exception_stacks[];

extern unsigned long table_start, table_end;

void init_kernel_pgtables(unsigned long start, unsigned long end);

extern unsigned long end_pfn_map;

extern void init_resources(void);

void __init zap_low_mappings(int cpu);

extern unsigned long ebda_addr, ebda_size;

extern int unhandled_signal(struct task_struct *tsk, int sig);

extern void system_call(void);
extern void ignore_sysret(void);

#endif
