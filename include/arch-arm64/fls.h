#ifndef _ASM_GENERIC_BITOPS_FLS_H_
#define _ASM_GENERIC_BITOPS_FLS_H_

/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
#define __fls(x) \
	fls(x)

static inline unsigned long fls(unsigned long x)
{
	int r = 63;

	if (!x)
		return 0;
	if (!(x & 0xffffffff00000000ul)) {
		x <<= 32;
		r -= 32;
	}
	if (!(x & 0xffff000000000000ul)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff00000000000000ul)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf000000000000000ul)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc000000000000000ul)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x8000000000000000ul)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

#endif /* _ASM_GENERIC_BITOPS_FLS_H_ */
