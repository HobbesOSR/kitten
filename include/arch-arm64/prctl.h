#ifndef ARM64_PRCTL_H
#define ARM64_PRCTL_H 1

#define ARCH_SET_TLS 0x1001
#define ARCH_GET_TLS 0x1003

extern long
do_arch_prctl(
	struct task_struct *	task,
	int			code,
	unsigned long		addr
);

#endif
