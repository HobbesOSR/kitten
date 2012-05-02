#ifndef _LWK_LINUX_COMPAT_H
#define _LWK_LINUX_COMPAT_H


#ifdef CONFIG_LINUX

#include <linux/module.h>
extern void linux_init(void);
extern void linux_wakeup(void);

#else

#include <lwk/types.h>
#include <arch/page.h>

#define EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym)

typedef paddr_t resource_size_t;


struct vm_area_struct {
    unsigned long vm_start;         /* Our start address within vm_mm. */
    unsigned long vm_end;           /* The first byte after our end address
                                           within vm_mm. */
    pgprot_t vm_page_prot;          /* Access permissions of this VMA. */

    unsigned long vm_pgoff;     /* Offset (within vm_file) in PAGE_SIZE
                       units, *not* PAGE_CACHE_SIZE */
};

static inline void linux_init(void) {}
#endif

#endif
