#ifndef __LINUX_ASM_COMMON_PAGE_H
#define __LINUX_ASM_COMMON_PAGE_H

#include <lwk/compiler.h>
#include <lwk/string.h>
#include <arch/page.h>

#define clear_page(page)	memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from)	memcpy((void *)(to), (void *)(from), PAGE_SIZE)

#endif
