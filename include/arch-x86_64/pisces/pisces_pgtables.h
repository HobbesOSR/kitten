#ifndef PISCES_PGTABLES_H
#define PISCES_PGTABLES_H



#define MAX_PDE32_ENTRIES          1024
#define MAX_PTE32_ENTRIES          1024

#define MAX_PDPE32PAE_ENTRIES      4
#define MAX_PDE32PAE_ENTRIES       512
#define MAX_PTE32PAE_ENTRIES       512

#define MAX_PML4E64_ENTRIES        512
#define MAX_PDPE64_ENTRIES         512
#define MAX_PDE64_ENTRIES          512
#define MAX_PTE64_ENTRIES          512


/* Converts an address into a page table index */
#define PDE32_INDEX(x)  ((((u32)x) >> 22) & 0x3ff)
#define PTE32_INDEX(x)  ((((u32)x) >> 12) & 0x3ff)


#define PDPE32PAE_INDEX(x) ((((u32)x) >> 30) & 0x3)
#define PDE32PAE_INDEX(x)  ((((u32)x) >> 21) & 0x1ff)
#define PTE32PAE_INDEX(x)  ((((u32)x) >> 12) & 0x1ff)

#define PML4E64_INDEX(x) ((((u64)x) >> 39) & 0x1ff)
#define PDPE64_INDEX(x)  ((((u64)x) >> 30) & 0x1ff)
#define PDE64_INDEX(x)   ((((u64)x) >> 21) & 0x1ff)
#define PTE64_INDEX(x)   ((((u64)x) >> 12) & 0x1ff)


/* Gets the base address needed for a Page Table entry */
#define PAGE_TO_BASE_ADDR(x)     ((x) >> 12)
#define PAGE_TO_BASE_ADDR_4KB(x) ((x) >> 12)
#define PAGE_TO_BASE_ADDR_2MB(x) ((x) >> 21)
#define PAGE_TO_BASE_ADDR_4MB(x) ((x) >> 22)
#define PAGE_TO_BASE_ADDR_1GB(x) ((x) >> 30)

#define BASE_TO_PAGE_ADDR(x)     (((uintptr_t)x) << 12)
#define BASE_TO_PAGE_ADDR_4KB(x) (((uintptr_t)x) << 12)
#define BASE_TO_PAGE_ADDR_2MB(x) (((uintptr_t)x) << 21)
#define BASE_TO_PAGE_ADDR_4MB(x) (((uintptr_t)x) << 22)
#define BASE_TO_PAGE_ADDR_1GB(x) (((uintptr_t)x) << 30)
/* *** */


//#define PAGE_OFFSET(x) ((x) & 0xfff)
#define PAGE_OFFSET_4KB(x) ((x) & 0xfff)
#define PAGE_OFFSET_2MB(x) ((x) & 0x1fffff)
#define PAGE_OFFSET_4MB(x) ((x) & 0x3fffff)
#define PAGE_OFFSET_1GB(x) ((x) & 0x3fffffff)

#define PAGE_POWER     12
#define PAGE_POWER_4KB 12
#define PAGE_POWER_2MB 21
#define PAGE_POWER_4MB 22
#define PAGE_POWER_1GB 30

// We shift instead of mask because we don't know the address size
#define PAGE_ADDR(x)     (((x) >> PAGE_POWER)     << PAGE_POWER)
#define PAGE_ADDR_4KB(x) (((x) >> PAGE_POWER_4KB) << PAGE_POWER_4KB)
#define PAGE_ADDR_2MB(x) (((x) >> PAGE_POWER_2MB) << PAGE_POWER_2MB)
#define PAGE_ADDR_4MB(x) (((x) >> PAGE_POWER_4MB) << PAGE_POWER_4MB)
#define PAGE_ADDR_1GB(x) (((x) >> PAGE_POWER_1GB) << PAGE_POWER_1GB)

//#define PAGE_SIZE 4096
#define PAGE_SIZE_4KB  4096
#define PAGE_SIZE_2MB (4096 * 512)
#define PAGE_SIZE_4MB (4096 * 1024)
#define PAGE_SIZE_1GB 0x40000000

#define PAGE_ALIGN_4KB(x) ALIGN(x, PAGE_SIZE_4KB)
#define PAGE_ALIGN_2MB(x) ALIGN(x, PAGE_SIZE_2MB)
#define PAGE_ALIGN_1GB(x) ALIGN(x, PAGE_SIZE_1GB)

#define PAGE_ALIGN_DOWN(x, s) (x & ~(s - 1))
/* *** */





#define CR3_TO_PDE32_PA(cr3)     ((uintptr_t)(((u32)cr3) & 0xfffff000))
#define CR3_TO_PDPE32PAE_PA(cr3) ((uintptr_t)(((u32)cr3) & 0xffffffe0))
#define CR3_TO_PML4E64_PA(cr3)   ((uintptr_t)(((u64)cr3) & 0x000ffffffffff000LL))

#define CR3_TO_PDE32_VA(cr3)     ((pde32_t     *)__va((void *)(uintptr_t)(((u32)cr3) & 0xfffff000)))
#define CR3_TO_PDPE32PAE_VA(cr3) ((pdpe32pae_t *)__va((void *)(uintptr_t)(((u32)cr3) & 0xffffffe0)))
#define CR3_TO_PML4E64_VA(cr3)   ((pml4e64_t   *)__va((void *)(uintptr_t)(((u64)cr3) & 0x000ffffffffff000LL)))



typedef struct gen_pt {
    u32 present        : 1;
    u32 writable       : 1;
    u32 user_page      : 1;
} __attribute__((packed)) gen_pt_t;

typedef struct pde32 {
    u32 present         : 1;
    u32 writable        : 1;
    u32 user_page       : 1;
    u32 write_through   : 1;
    u32 cache_disable   : 1;
    u32 accessed        : 1;
    u32 reserved        : 1;
    u32 large_page      : 1;
    u32 global_page     : 1;
    u32 vmm_info        : 3;
    u32 pt_base_addr    : 20;
} __attribute__((packed))  pde32_t;

typedef struct pde32_4MB {
    u32 present         : 1;
    u32 writable        : 1;
    u32 user_page       : 1;
    u32 write_through   : 1;
    u32 cache_disable   : 1;
    u32 accessed        : 1;
    u32 dirty           : 1;
    u32 large_page      : 1;
    u32 global_page     : 1;
    u32 vmm_info        : 3;
    u32 pat             : 1;
    u32 rsvd            : 9;
    u32 page_base_addr  : 10;

} __attribute__((packed))  pde32_4MB_t;

typedef struct pte32 {
    u32 present         : 1;
    u32 writable        : 1;
    u32 user_page       : 1;
    u32 write_through   : 1;
    u32 cache_disable   : 1;
    u32 accessed        : 1;
    u32 dirty           : 1;
    u32 pte_attr        : 1;
    u32 global_page     : 1;
    u32 vmm_info        : 3;
    u32 page_base_addr  : 20;
}  __attribute__((packed)) pte32_t;
/* ***** */

/* 32 bit PAE PAGE STRUCTURES */
typedef struct pdpe32pae {
    u64 present       : 1;
    u64 rsvd          : 2; // MBZ
    u64 write_through : 1;
    u64 cache_disable : 1;
    u64 accessed      : 1; 
    u64 avail         : 1;
    u64 rsvd2         : 2;  // MBZ
    u64 vmm_info      : 3;
    u64 pd_base_addr  : 24;
    u64 rsvd3         : 28; // MBZ
} __attribute__((packed)) pdpe32pae_t;



typedef struct pde32pae {
    u64 present         : 1;
    u64 writable        : 1;
    u64 user_page       : 1;
    u64 write_through   : 1;
    u64 cache_disable   : 1;
    u64 accessed        : 1;
    u64 avail           : 1;
    u64 large_page      : 1;
    u64 global_page     : 1;
    u64 vmm_info        : 3;
    u64 pt_base_addr    : 24;
    u64 rsvd            : 28;
} __attribute__((packed)) pde32pae_t;

typedef struct pde32pae_2MB {
    u64 present         : 1;
    u64 writable        : 1;
    u64 user_page       : 1;
    u64 write_through   : 1;
    u64 cache_disable   : 1;
    u64 accessed        : 1;
    u64 dirty           : 1;
    u64 one             : 1;
    u64 global_page     : 1;
    u64 vmm_info        : 3;
    u64 pat             : 1;
    u64 rsvd            : 8;
    u64 page_base_addr  : 15;
    u64 rsvd2           : 28;

} __attribute__((packed)) pde32pae_2MB_t;

typedef struct pte32pae {
    u64 present         : 1;
    u64 writable        : 1;
    u64 user_page       : 1;
    u64 write_through   : 1;
    u64 cache_disable   : 1;
    u64 accessed        : 1;
    u64 dirty           : 1;
    u64 pte_attr        : 1;
    u64 global_page     : 1;
    u64 vmm_info        : 3;
    u64 page_base_addr  : 24;
    u64 rsvd            : 28;
} __attribute__((packed)) pte32pae_t;





/* ********** */


/* LONG MODE 64 bit PAGE STRUCTURES */
typedef struct pml4e64 {
    u64 present        : 1;
    u64 writable       : 1;
    u64 user_page      : 1;
    u64 write_through  : 1;
    u64 cache_disable  : 1;
    u64 accessed       : 1;
    u64 reserved       : 1;
    u64 zero           : 2;
    u64 vmm_info       : 3;
    u64 pdp_base_addr  : 40;
    u64 available      : 11;
    u64 no_execute     : 1;
} __attribute__((packed)) pml4e64_t;


typedef struct pdpe64 {
    u64 present        : 1;
    u64 writable       : 1;
    u64 user_page      : 1;
    u64 write_through  : 1;
    u64 cache_disable  : 1;
    u64 accessed       : 1;
    u64 avail          : 1;
    u64 large_page     : 1;
    u64 zero           : 1;
    u64 vmm_info       : 3;
    u64 pd_base_addr   : 40;
    u64 available      : 11;
    u64 no_execute     : 1;
} __attribute__((packed)) pdpe64_t;


// We Don't support this
typedef struct pdpe64_1GB {
    u64 present        : 1;
    u64 writable       : 1;
    u64 user_page      : 1;
    u64 write_through  : 1;
    u64 cache_disable  : 1;
    u64 accessed       : 1;
    u64 dirty          : 1;
    u64 large_page     : 1;
    u64 global_page    : 1;
    u64 vmm_info       : 3;
    u64 pat            : 1;
    u64 rsvd           : 17;
    u64 page_base_addr : 22;
    u64 available      : 11;
    u64 no_execute     : 1;
} __attribute__((packed)) pdpe64_1GB_t;



typedef struct pde64 {
    u64 present         : 1;
    u64 writable        : 1;
    u64 user_page       : 1;
    u64 write_through   : 1;
    u64 cache_disable   : 1;
    u64 accessed        : 1;
    u64 avail           : 1;
    u64 large_page      : 1;
    u64 global_page     : 1;
    u64 vmm_info        : 3;
    u64 pt_base_addr    : 40;
    u64 available       : 11;
    u64 no_execute      : 1;
} __attribute__((packed)) pde64_t;

typedef struct pde64_2MB {
    u64 present         : 1;
    u64 writable        : 1;
    u64 user_page       : 1;
    u64 write_through   : 1;
    u64 cache_disable   : 1;
    u64 accessed        : 1;
    u64 dirty           : 1;
    u64 large_page      : 1;
    u64 global_page     : 1;
    u64 vmm_info        : 3;
    u64 pat             : 1;
    u64 rsvd            : 8;
    u64 page_base_addr  : 31;
    u64 available       : 11;
    u64 no_execute      : 1;
} __attribute__((packed)) pde64_2MB_t;


typedef struct pte64 {
    u64 present         : 1;
    u64 writable        : 1;
    u64 user_page       : 1;
    u64 write_through   : 1;
    u64 cache_disable   : 1;
    u64 accessed        : 1;
    u64 dirty           : 1;
    u64 pte_attr        : 1;
    u64 global_page     : 1;
    u64 vmm_info        : 3;
    u64 page_base_addr  : 40;
    u64 available       : 11;
    u64 no_execute      : 1;
} __attribute__((packed)) pte64_t;

/* *************** */

typedef struct pf_error_code {
    u32 present           : 1; /* if 0, fault due to page not present                     */
    u32 write             : 1; /* if 1, faulting access was a write                       */
    u32 user              : 1; /* if 1, faulting access was in user mode                  */
    u32 rsvd_access       : 1; /* if 1, fault from reading a 1 from a reserved field (?)  */
    u32 ifetch            : 1; /* if 1, faulting access was an instr fetch (only with NX) */
    u32 rsvd              : 27;
} __attribute__((packed)) pf_error_t;


#endif /*_PGTABLES_H */
