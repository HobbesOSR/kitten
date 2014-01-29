#ifndef _ARCH_X86_64_PAGE_TABLE_H
#define _ARCH_X86_64_PAGE_TABLE_H

typedef struct {
	uint64_t
		present    :1,  /* Is there a physical page? */
		write      :1,  /* Is the page writable? */
		user       :1,  /* Is the page accessible to user-space? */
		pwt        :1,  /* Is the page write-through cached? */
		pcd        :1,  /* Is the page uncached? */
		accessed   :1,  /* Has the page been read? */
		dirty      :1,  /* Has the page been written to? */
		pagesize   :1,  /* 0 == 4KB, 1 == (2 MB || 1 GB) */
		global     :1,  /* Is the page mapped in all address spaces? */
		os_bits_1  :3,  /* Available for us! */
		base_paddr :40, /* Bits [51,12] of base address. */
		os_bits_2  :11, /* Available for us! */
		no_exec    :1;  /* Is the page executable? */
} xpte_t;

typedef struct {
	uint64_t
		present    :1,  /* Is there a physical page? */
		write      :1,  /* Is the page writable? */
		user       :1,  /* Is the page accessible to user-space? */
		pwt        :1,  /* Is the page write-through cached? */
		pcd        :1,  /* Is the page uncached? */
		accessed   :1,  /* Has the page been read? */
		dirty      :1,  /* Has the page been written to? */
		pat        :1,  /* Page attribute table bit. */
		global     :1,  /* Is the page mapped in all address spaces? */
		os_bits_1  :3,  /* Available for us! */
		base_paddr :40, /* Bits [51,12] of page's base physical addr. */
		os_bits_2  :11, /* Available for us! */
		no_exec    :1;  /* Is the page executable? */
} xpte_4KB_t;

typedef struct {
	uint64_t
		present    :1,  /* Is there a physical page? */
		write      :1,  /* Is the page writable? */
		user       :1,  /* Is the page accessible to user-space? */
		pwt        :1,  /* Is the page write-through cached? */
		pcd        :1,  /* Is the page uncached? */
		accessed   :1,  /* Has the page been read? */
		dirty      :1,  /* Has the page been written to? */
		must_be_1  :1,  /* Must be 1 to indicate a 2 MB page. */
		global     :1,  /* Is the page mapped in all address spaces? */
		os_bits_1  :3,  /* Available for us! */
		pat        :1,  /* Page attribute table bit. */
		must_be_0  :8,  /* Reserved, must be zero. */
		base_paddr :31, /* Bits [51,21] of page's base physical addr. */
		os_bits_2  :11, /* Available for us! */
		no_exec    :1;  /* Is the page executable? */
} xpte_2MB_t;

typedef struct {
	uint64_t
		present    :1,  /* Is there a physical page? */
		write      :1,  /* Is the page writable? */
		user       :1,  /* Is the page accessible to user-space? */
		pwt        :1,  /* Is the page write-through cached? */
		pcd        :1,  /* Is the page uncached? */
		accessed   :1,  /* Has the page been read? */
		dirty      :1,  /* Has the page been written to? */
		must_be_1  :1,  /* Must be 1 to indicate a 1GB page. */
		global     :1,  /* Is the page mapped in all address spaces? */
		os_bits_1  :3,  /* Available for us! */
		pat        :1,  /* Page attribute table bit. */
		must_be_0  :17, /* Reserved, must be zero. */
		base_paddr :22, /* Bits [51,30] of page's base physical addr. */
		os_bits_2  :11, /* Available for us! */
		no_exec    :1;  /* Is the page executable? */
} xpte_1GB_t;

/*
 * These functions return the base_paddr field in the input pte as a paddr_t.
 * These functions should always be used rather than trying to use
 * pte->base_addr directly, since the default type of C bitfields is 
 * typically 32-bit integer, leading to hard to debug integer overflows
 * when working with 64-bit unsigned addresses.
 */
static inline paddr_t xpte_paddr(xpte_t *pte)         { return ((paddr_t)(pte->base_paddr)) << 12; }
static inline paddr_t xpte_4KB_paddr(xpte_4KB_t *pte) { return ((paddr_t)(pte->base_paddr)) << 12; }
static inline paddr_t xpte_2MB_paddr(xpte_2MB_t *pte) { return ((paddr_t)(pte->base_paddr)) << 21; }
static inline paddr_t xpte_1GB_paddr(xpte_1GB_t *pte) { return ((paddr_t)(pte->base_paddr)) << 30; }

#endif
