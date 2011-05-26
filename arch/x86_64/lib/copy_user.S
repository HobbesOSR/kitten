/* Copyright 2002 Andi Kleen, SuSE Labs.
 * Subject to the GNU Public License v2.
 * 
 * Functions to copy from and to user space.		
 */		 

#include <lwk/linkage.h>
#include <arch/dwarf2.h>

#include <arch/current.h>
#include <arch/asm-offsets.h>

/* Standard copy_to_user with segment limit checking */		
ENTRY(copy_to_user)
	CFI_STARTPROC
	GET_CURRENT(%rax)
	movq %rdi,%rcx
	addq %rdx,%rcx
	jc  bad_to_user
	cmpq tsk_arch_addr_limit(%rax),%rcx
	jae bad_to_user
	xorl %eax,%eax	/* clear zero flag */
	jmp copy_user_generic_string
	CFI_ENDPROC

ENTRY(copy_user_generic)
	CFI_STARTPROC
	movl $1,%ecx	/* set zero flag */
	jmp copy_user_generic_string
	CFI_ENDPROC

ENTRY(__copy_from_user_inatomic)
	CFI_STARTPROC
	xorl %ecx,%ecx	/* clear zero flag */
	jmp copy_user_generic_string
	CFI_ENDPROC

/* Standard copy_from_user with segment limit checking */	
ENTRY(copy_from_user)
	CFI_STARTPROC
	GET_CURRENT(%rax)
	movq %rsi,%rcx
	addq %rdx,%rcx
	jc  bad_from_user
	cmpq tsk_arch_addr_limit(%rax),%rcx
	jae  bad_from_user
	movl $1,%ecx	/* set zero flag */
	jmp copy_user_generic_string
	CFI_ENDPROC
ENDPROC(copy_from_user)
	
	.section .fixup,"ax"
	/* must zero dest */
bad_from_user:
	CFI_STARTPROC
	movl %edx,%ecx
	xorl %eax,%eax
	rep
	stosb
bad_to_user:
	movl	%edx,%eax
	ret
	CFI_ENDPROC
END(bad_from_user)
	.previous
	
		
 /* rdi	destination
  * rsi source
  * rdx count
  * ecx zero flag
  *
  * Output:
  * eax uncopied bytes or 0 if successfull.
  *
  * Only 4GB of copy is supported. This shouldn't be a problem
  * because the kernel normally only writes from/to page sized chunks
  * even if user space passed a longer buffer.
  * And more would be dangerous because both Intel and AMD have
  * errata with rep movsq > 4GB. If someone feels the need to fix
  * this please consider this.
  */
ENTRY(copy_user_generic_string)
	CFI_STARTPROC
	movl %ecx,%r8d		/* save zero flag */
	movl %edx,%ecx
	shrl $3,%ecx
	andl $7,%edx	
	jz   10f
1:	rep 
	movsq 
	movl %edx,%ecx
2:	rep
	movsb
9:	movl %ecx,%eax
	ret

	/* multiple of 8 byte */
10:	rep
	movsq
	xor %eax,%eax
	ret

	/* exception handling */
3:      lea (%rdx,%rcx,8),%rax	/* exception on quad loop */
	jmp 6f
5:	movl %ecx,%eax		/* exception on byte loop */
	/* eax: left over bytes */
6:	testl %r8d,%r8d		/* zero flag set? */
	jz 7f
	movl %eax,%ecx		/* initialize x86 loop counter */
	push %rax
	xorl %eax,%eax
8:	rep
	stosb 			/* zero the rest */
11:	pop %rax
7:	ret
	CFI_ENDPROC
END(copy_user_generic_string)

	.section __ex_table,"a"
	.quad 1b,3b
	.quad 2b,5b
	.quad 8b,11b
	.quad 10b,3b
	.previous
