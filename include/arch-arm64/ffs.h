#ifndef _ASM_GENERIC_BITOPS_FFS_H_
#define _ASM_GENERIC_BITOPS_FFS_H_

#ifdef __KERNEL__
/**
 * ffs - find first bit set
 * @x: the word to search
 *
 * This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */
#define __ffs(x) \
	ffs(x)

static inline unsigned long ffs(unsigned long x)
{
	int r = 0;

	if (!x)
		return 0;
	if (!(x & 0xfffffffful)) {
		x >>= 32;
		r += 32;
	}
	if (!(x & 0xfffful)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xfful)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xful)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3ul)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1ul)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

#endif

#endif /* _ASM_GENERIC_BITOPS_FFS_H_ */
