#include <arch/page.h>
#include <asm-generic/page.h>
#define VMEMMAP_START    _AC(0xffffea0000000000, UL)
#define vmemmap ((struct page *)VMEMMAP_START)

