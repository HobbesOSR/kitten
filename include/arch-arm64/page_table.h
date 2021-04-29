#ifndef _ARCH_ARM64_PAGE_TABLE_H
#define _ARCH_ARM64_PAGE_TABLE_H

/*
	In the VMSAv8-64 translation table format, the AttrIndx[2:0] field in a block or page translation table descriptor
	for a stage 1 translation indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding
	memory region. 
*/




#ifndef CONFIG_ARM64_64K_PAGES


typedef struct {
	uint64_t            /* ARM64 descriptions */
		valid      :1,  /* Is the descriptor valid? */
		type       :1,  /* 0 = block entry, 1 = table entry */
		ignored1   :10,
		base_paddr :36, /* Bits [47,12] of base address. */
		res0       :4,
		ignored2   :7,
		PXNTable   :1,  /* Max PXN value of next pgtable level */
		XNTable    :1,  /* Max XN value of next pgtable level */
		APTable1   :1,  /* Max AP1 of next pgtable level */
		APTable2   :1,  /* Max AP2 of next pgtable level */
		NSTable    :1;  /* next pgtable level is Not Secure */
} xpte_t;


typedef struct {
	uint64_t
		valid      :1, /* Is the descriptor valid? */
		type       :1, /* 0 = block entry, 1 = table entry */
		attrIndx   :3, /* indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding	memory region. */
		NS         :1, /* 0 = Secure, 1 = Not Secure?? */
		AP1        :1, /* 0 = system, 1 = user */
		AP2        :1, /* 0 = writable, 1 = read only  */
		SH0        :1, /* 0 = outer shareable, 1 = inner shareable */
		SH1        :1, /* Shareable  */
		AF         :1, /* Accessed Flag */
		nG         :1, /* not Global */
		rsvd       :38, /* Page address will go somewhere in here */
		GP         :1, /* Guarded Page  (If ARMv8.5-BTI) */
		DBM        :1, /* Dirty Bit Modifier */
		contiguous :1, /* Part of a contigous run of pages */
		PXN        :1, /* Privilege No Execute */
		XN         :1, /* No Execute */
		os_bits_4  :4, /* Available for us! */
		pbha       :4, /* Page based HW attributes */
		ignored    :1;  
} xpte_leaf_t;

typedef struct {
	uint64_t
		valid      :1, /* Is the descriptor valid? */
		type       :1, /* 0 = block entry, 1 = table entry */
		attrIndx   :3, /* indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding	memory region. */
		NS         :1, /* 0 = Secure, 1 = Not Secure?? */
		AP1        :1, /* 0 = system, 1 = user */
		AP2        :1, /* 0 = writable, 1 = read only  */
		SH0        :1, /* 0 = outer shareable, 1 = inner shareable */
		SH1        :1, /* Shareable  */
		AF         :1, /* Accessed Flag */
		nG         :1, /* not Global */
		base_paddr :36, /* Bits [47,12] of page's base physical addr. */
		res0       :2,
		GP         :1, /* Guarded Page  (If ARMv8.5-BTI) */
		DBM        :1, /* Dirty Bit Modifier */
		contiguous :1, /* Part of a contigous run of pages */
		PXN        :1, /* Privilege No Execute */
		XN         :1, /* No Execute */
		os_bits_4  :4, /* Available for us! */
		pbha       :4, /* Page based HW attributes */
		ignored    :1;  
} xpte_4KB_t;



typedef struct {
	uint64_t
		valid      :1,  /* Is the descriptor valid? */
		type       :1,  /* 0 = block entry, 1 = table entry */
		attrIndx   :3, /* indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding	memory region. */
		NS         :1,  /* 0 = Secure, 1 = Not Secure?? */
		AP1        :1,  /* 0 = system, 1 = user */
		AP2        :1,  /* 0 = writable, 1 = read only  */
		SH0        :1,  /* 0 = outer shareable, 1 = inner shareable */
		SH1        :1,  /* Shareable  */
		AF         :1,  /* Accessed Flag */
		nG         :1,  /* not Global */
		OA         :4,  /* bits[51:48] of the output  address. (if ARMv8.2-LPA) */
		nT         :1,  /* Block Translation Entry  - This looks sketchy*/
		must_be_0  :4,  /* 17 - 20 Reserved, must be zero. */
		base_paddr :27, /* 21 - 47 Bits [47,21] of page's base physical addr. */
		res0       :2,
		GP         :1,  /* Guarded Page  (If ARMv8.5-BTI) */
		DBM        :1,  /* Dirty Bit Modifier */
		contiguous :1,  /* Part of a contigous run of pages */
		PXN        :1,  /* Privilege No Execute */
		XN         :1,  /* No Execute */
		os_bits_4  :4,  /* Available for us! */
		pbha       :4,  /* Page based HW attributes */
		ignored    :1;  
} xpte_2MB_t;

typedef struct {
	uint64_t
		valid      :1,  /* Is the descriptor valid? */
		type       :1,  /* 0 = block entry, 1 = table entry */
		attrIndx   :3, /* indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding	memory region. */		NS         :1,  /* 0 = Secure, 1 = Not Secure?? */
		AP1        :1,  /* 0 = system, 1 = user */
		AP2        :1,  /* 0 = writable, 1 = read only  */
		SH0        :1,  /* 0 = outer shareable, 1 = inner shareable */
		SH1        :1,  /* Shareable  */
		AF         :1,  /* Accessed Flag */
		nG         :1,  /* not Global */
		OA         :4,  /* bits[51:48] of the output  address. (if ARMv8.2-LPA) */
		nT         :1,  /* Block Translation Entry  - This looks sketchy*/
 		must_be_0  :13, /* 17 - 29 Reserved, must be zero. */
		base_paddr :18, /* Bits [47,30] of page's base physical addr. */
		res0       :2,
		GP         :1,  /* Guarded Page  (If ARMv8.5-BTI) */
		DBM        :1,  /* Dirty Bit Modifier */
		contiguous :1,  /* Part of a contigous run of pages */
		PXN        :1,  /* Privilege No Execute */
		XN         :1,  /* No Execute */
		os_bits_4  :4,  /* Available for us! */
		pbha       :4,  /* Page based HW attributes */
		ignored    :1;  
} xpte_1GB_t;



#else



typedef struct {
	uint64_t            /* ARM64 descriptions */
		valid      :1,  /* Is the descriptor valid? */
		type       :1,  /* 0 = block entry, 1 = table entry */
		ignored1   :10,
		TA         :4,  /* bits[51:48] of the next-level table address. (if ARMv8.2-LPA) */
		base_paddr :32, /* Bits [47,16] of base address. */
		res0       :4,
		ignored2   :6,
		PXNTable   :1, /* Max PXN value of next pgtable level */
		XNTable    :1, /* Max XN value of next pgtable level */
		APTable1   :1, /* Max AP1 of next pgtable level */
		APTable2   :1, /* Max AP2 of next pgtable level */
		NSTable    :1; /* next pgtable level is Not Secure */
} xpte_t;



typedef struct {
	uint64_t
		valid      :1, /* Is the descriptor valid? */
		type       :1, /* 0 = block entry, 1 = table entry */
		attrIndx   :3, /* indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding	memory region. */
		NS         :1, /* 0 = Secure, 1 = Not Secure?? */
		AP1        :1, /* 0 = system, 1 = user */
		AP2        :1, /* 0 = writable, 1 = read only  */
		SH0        :1, /* 0 = outer shareable, 1 = inner shareable */
		SH1        :1, /* Shareable  */
		AF         :1, /* Accessed Flag */
		nG         :1, /* not Global */
		rsvd       :38, /* Page address will go somewhere in here */
		GP         :1, /* Guarded Page  (If ARMv8.5-BTI) */
		DBM        :1, /* Dirty Bit Modifier */
		contiguous :1, /* Part of a contigous run of pages */
		PXN        :1, /* Privilege No Execute */
		XN         :1, /* No Execute */
		os_bits_4  :4, /* Available for us! */
		pbha       :4, /* Page based HW attributes */
		ignored    :1;  
} xpte_leaf_t;



typedef struct {
	uint64_t
		valid      :1, /* Is the descriptor valid? */
		type       :1, /* 0 = block entry, 1 = table entry */
		attrIndx   :3, /* indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding	memory region. */		NS         :1, /* 0 = Secure, 1 = Not Secure?? */
		AP1        :1, /* 0 = system, 1 = user */
		AP2        :1, /* 0 = writable, 1 = read only  */
		SH0        :1, /* 0 = outer shareable, 1 = inner shareable */
		SH1        :1, /* Shareable  */
		AF         :1, /* Accessed Flag */
		nG         :1, /* not Global */
		TA         :4,  /* bits[51:48] of the page address. (if ARMv8.2-LPA) */
		base_paddr :32, /* Bits [47,16] of page's base physical addr. */
		res0_2     :2,
		GP         :1, /* Guarded Page */
		DBM        :1, /* Dirty Bit Modifier */
		contiguous :1, /* Part of a contigous run of pages */
		PXN        :1, /* Privilege No Execute */
		XN         :1, /* No Execute */
		os_bits_4  :4, /* Available for us! */
		pbha       :4, /* Page based HW attributes */
		ignored    :1;  
} xpte_64KB_t;

typedef struct {
	uint64_t
		valid      :1, /* Is the descriptor valid? */
		type       :1, /* 0 = block entry, 1 = table entry */
		attrIndx   :3, /* indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding	memory region. */		NS         :1, /* 0 = Secure, 1 = Not Secure?? */
		AP1        :1, /* 0 = system, 1 = user */
		AP2        :1, /* 0 = writable, 1 = read only  */
		SH0        :1, /* 0 = outer shareable, 1 = inner shareable */
		SH1        :1, /* Shareable  */
		AF         :1, /* Accessed Flag */
		nG         :1, /* not Global */
		OA         :4, /* bits[51:48] of the output  address. (if ARMv8.2-LPA) */
		nT         :1, /* Block Translation Entry  - This looks sketchy*/
		res0_2     :12
		base_paddr :19, /* Bits [47,29] of page's base physical addr. */
		res0_3     :2,
		GP         :1, /* Guarded Page */
		DBM        :1, /* Dirty Bit Modifier */
		contiguous :1, /* Part of a contigous run of pages */
		PXN        :1, /* Privilege No Execute */
		XN         :1, /* No Execute */
		os_bits_4  :4, /* Available for us! */
		pbha       :4, /* Page based HW attributes */
		ignored    :1;  
} xpte_512MB_t;




#endif


static inline paddr_t xpte_paddr(xpte_t  *pte)       { return ((paddr_t)(pte->base_paddr)) << PAGE_SHIFT; }

#ifndef CONFIG_ARM64_64K_PAGES

static inline paddr_t xpte_4KB_paddr (xpte_4KB_t  *pte)       { return ((paddr_t)(pte->base_paddr)) << PAGE_SHIFT_4KB; }
static inline paddr_t xpte_2MB_paddr (xpte_2MB_t  *pte)       { return ((paddr_t)(pte->base_paddr)) << PAGE_SHIFT_2MB; }
static inline paddr_t xpte_1GB_paddr (xpte_1GB_t  *pte)       { return ((paddr_t)(pte->base_paddr)) << PAGE_SHIFT_1GB; }

#else

static inline paddr_t xpte_64KB_paddr (xpte_64KB_t  *pte)       { return ((paddr_t)(pte->base_paddr)) << PAGE_SHIFT_64KB; }
static inline paddr_t xpte_512MB_paddr(xpte_512MB_t *pte)       { return ((paddr_t)(pte->base_paddr)) << PAGE_SHIFT_512MB; }

#endif


#endif
