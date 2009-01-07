#ifndef X86_64_PRCTL_H
#define X86_64_PRCTL_H 1

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

extern long
do_arch_prctl(
	struct task_struct *	task,
	int			code,
	unsigned long		addr
);

#endif
