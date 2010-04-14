/* Nothing */
#include <asm/page.h>
/* memmap is virtually contiguous.  */
#define __pfn_to_page(pfn)      (vmemmap + (pfn))
#define __page_to_pfn(page)     (unsigned long)((page) - vmemmap)

#define page_to_pfn __page_to_pfn
#define pfn_to_page __pfn_to_page

