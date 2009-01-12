#ifndef _ASM_X86_ASM_H
#define _ASM_X86_ASM_H

#ifdef __ASSEMBLY__
# define __ASM_FORM(x)	x
# define __ASM_EX_SEC	.section __ex_table
#else
# define __ASM_FORM(x)	" " #x " "
# define __ASM_EX_SEC	" .section __ex_table,\"a\"\n"
#endif

# define __ASM_SEL(a,b) __ASM_FORM(b)

#define __ASM_SIZE(inst)	__ASM_SEL(inst##l, inst##q)
#define __ASM_REG(reg)		__ASM_SEL(e##reg, r##reg)

#define _ASM_PTR	__ASM_SEL(.long, .quad)
#define _ASM_ALIGN	__ASM_SEL(.balign 4, .balign 8)
#define _ASM_MOV_UL	__ASM_SIZE(mov)

#define _ASM_INC	__ASM_SIZE(inc)
#define _ASM_DEC	__ASM_SIZE(dec)
#define _ASM_ADD	__ASM_SIZE(add)
#define _ASM_SUB	__ASM_SIZE(sub)
#define _ASM_XADD	__ASM_SIZE(xadd)
#define _ASM_AX		__ASM_REG(ax)
#define _ASM_BX		__ASM_REG(bx)
#define _ASM_CX		__ASM_REG(cx)
#define _ASM_DX		__ASM_REG(dx)

/* Exception table entry */
# define _ASM_EXTABLE(from,to) \
	__ASM_EX_SEC	\
	_ASM_ALIGN "\n" \
	_ASM_PTR #from "," #to "\n" \
	" .previous\n"

#endif /* _ASM_X86_ASM_H */
