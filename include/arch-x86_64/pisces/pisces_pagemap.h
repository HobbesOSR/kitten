#ifndef __PISCES_PAGEMAP_H__
#define __PISCES_PAGEMAP_H__

#include <lwk/types.h>

struct enclave_aspace {
    u64 cr3;
};

struct xpmem_pfn {
    u64 pfn;
};

unsigned long
pisces_map_xpmem_pfn_range(struct xpmem_pfn * pfns,
			   u64                num_pfns);


#endif /* __PISCES_PAGEMAP_H__ */
