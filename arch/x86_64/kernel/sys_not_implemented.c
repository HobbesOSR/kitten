#include <lwk/kernel.h>
#include <lwk/task.h>
#include <arch/vsyscall.h>


/**
 * Silent handler for unimplemented system calls.
 */
long
syscall_not_implemented_silent(void)
{
	return -ENOSYS;
}


/**
 * Noisy handler for unimplemented system calls.
 */
long
syscall_not_implemented(void)
{
	unsigned long syscall_number;
	vaddr_t rsp, frame;
	vaddr_t r11, r10, r9, r8, rax, rcx, rdx, rsi, rdi, orig_rax, rip;

	/* On entry to function, syscall # is in %rax register */
	asm volatile("mov %%rax, %0" : "=r"(syscall_number)::"%rax");

	/************************************************************************/
	/* BEGIN THIS IS SUPER FRAGILE */
	asm volatile("mov %%rsp, %0" : "=r"(rsp)::"%rsp");

	/* Calculate the base of the register frame that was saved by
	 * asm_syscall().  This is the super duper fragile part, since
	 * the compiler might change the offset. */
	frame    = rsp + 8 + 8 + 8 + 8;

	rip      = frame + 10*8;
	orig_rax = frame +  9*8;
	rdi      = frame +  8*8;
	rsi      = frame +  7*8;
	rdx      = frame +  6*8;
	rcx      = frame +  5*8;
	rax      = frame +  4*8;
	r8       = frame +  3*8;
	r9       = frame +  2*8;
	r10      = frame +  1*8;
	r11      = frame +  0*8;
	/* END THIS IS SUPER FRAGILE */
	/************************************************************************/

	printk(KERN_DEBUG "[%s] System call not implemented! "
	                  "(syscall_number=%lu)\n",
	                  current->name, syscall_number);

	/************************************************************************/
	/* BEGIN THIS IS SUPER FRAGILE */
	printk("RIP  %lx\n", *((vaddr_t *)rip));
	printk("ORAX %lx\n", *((vaddr_t *)orig_rax));
	printk("RDI  %lx\n", *((vaddr_t *)rdi));
	printk("RSI  %lx\n", *((vaddr_t *)rsi));
	printk("RDX  %lx\n", *((vaddr_t *)rdx));
	printk("XXX  %lx\n", *((vaddr_t *)rcx));
	printk("RAX  %lx\n", *((vaddr_t *)rax));
	printk("R8   %lx\n", *((vaddr_t *)r8));
	printk("R9   %lx\n", *((vaddr_t *)r9));
	printk("R10  %lx\n", *((vaddr_t *)r10));
	printk("R11  %lx\n", *((vaddr_t *)r11));
	/* END THIS IS SUPER FRAGILE */
	/************************************************************************/

	/* Only warn once about each system call */
	syscall_register(syscall_number, syscall_not_implemented_silent);
	return -ENOSYS;
}
