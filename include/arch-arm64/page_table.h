#ifndef _ARCH_ARM64_PAGE_TABLE_H
#define _ARCH_ARM64_PAGE_TABLE_H

typedef struct {
	uint64_t            /* x86-64 descriptions */
		valid      :1,  /* Is there a physical page? */
		type       :1,  /* Is the page writable? */
		attrIndx0  :1,  /* Is the page accessible to user-space? */
		attrIndx1  :1,  /* Is the page write-through cached? */
		attrIndx2  :1,  /* Is the page uncached? */
		NS         :1,  /* Has the page been read? */
		AP1        :1,  /* Has the page been written to? */
		AP2        :1,  /* 0 == 4KB, 1 == (2 MB || 1 GB) */
		SH0        :1,  /* Is the page mapped in all address spaces? */
		SH1        :1,
		AF         :1,  /* Available for us! */
		nG         :1,
		base_paddr :40, /* Bits [51,12] of base address. */
		contiguous :1,
		PXN        :1,
		XN         :1,
		os_bits_4  :4, /* Available for us! */
		ignored    :5;  /* Is the page executable? */
} xpte_t;

typedef struct {
	uint64_t
		valid      :1,  /* Is there a physical page? */
		type       :1,  /* Is the page writable? */
		attrIndx0  :1,  /* Is the page accessible to user-space? */
		attrIndx1  :1,  /* Is the page write-through cached? */
		attrIndx2  :1,  /* Is the page uncached? */
		NS         :1,  /* Has the page been read? */
		AP1        :1,  /* Has the page been written to? */
		AP2        :1,  /* Page attribute table bit. */
		SH0        :1,  /* Is the page mapped in all address spaces? */
		SH1        :1,  /* Available for us! */
		AF         :1,
		nG         :1,
		base_paddr :40, /* Bits [51,12] of page's base physical addr. */
		contiguous :1,
		PXN        :1,
		XN         :1,
		os_bits_4  :4, /* Available for us! */
		ignored    :5;  /* Is the page executable? */
} xpte_4KB_t;

typedef struct {
	uint64_t
		valid      :1,  /* 0 Is there a physical page? */
		type       :1,  /* 1 Is the page writable? */
		attrIndx0  :1,  /* 2 Is the page accessible to user-space? */
		attrIndx1  :1,  /* 3 Is the page write-through cached? */
		attrIndx2  :1,  /* 4 Is the page uncached? */
		NS         :1,  /* 5 Has the page been read? */
		AP1        :1,  /* 6 Has the page been written to? */
		AP2        :1,  /* 7 Must be 1 to indicate a 2 MB page. */
		SH0        :1,  /* 8 Is the page mapped in all address spaces? */
		SH1        :1,  /* 9 Available for us! */
		AF         :1,  /* 10 */
		nG         :1,  /* 11 */
		must_be_0  :9,  /* 12 - 20 Reserved, must be zero. */
		base_paddr :31, /* 21 - 51 Bits [51,21] of page's base physical addr. */
		contiguous :1,  /* 52 */
		PXN        :1,  /* 53 */
		XN         :1,  /* 54 */
		os_bits_4  :4,  /* 55 - 58 Available for us! */
		ignored    :5;  /* 59 - 63 Is the page executable? */
} xpte_2MB_t;

typedef struct {
	uint64_t
		valid      :1,  /* Is there a physical page? */
		type       :1,  /* Is the page writable? */
		attrIndx0  :1,  /* Is the page accessible to user-space? */
		attrIndx1  :1,  /* Is the page write-through cached? */
		attrIndx2  :1,  /* Is the page uncached? */
		NS         :1,  /* Has the page been read? */
		dirty      :1,  /* Has the page been written to? */
		must_be_1  :1,  /* Must be 1 to indicate a 1GB page. */
		global     :1,  /* Is the page mapped in all address spaces? */
		os_bits_1  :3,  /* Available for us! */
		pat        :1,  /* Page attribute table bit. */
		must_be_0  :17, /* Reserved, must be zero. */
		base_paddr :22, /* Bits [51,30] of page's base physical addr. */
		contiguous :1,
		PXN        :1,
		XN         :1,
		os_bits_4  :4, /* Available for us! */
		ignored    :5;  /* Is the page executable? */
} xpte_1GB_t;

#endif
