#define MTRR_TYPE_WRCOMB     1
static inline int mtrr_add(unsigned long base, unsigned long size,
			   unsigned int type, bool increment)
{
	return -ENODEV;
}

static inline int mtrr_del(int reg, unsigned long base, unsigned long size)
{
	return -ENODEV;
}


