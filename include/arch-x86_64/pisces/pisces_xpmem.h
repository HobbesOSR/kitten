#ifndef _PISCES_XPMEM_H
#define _PISCES_XPMEM_H

#include <lwk/xpmem/xpmem_iface.h>

xpmem_link_t
pisces_xpmem_init(void);

void
pisces_xpmem_deinit(xpmem_link_t);


#endif /* _PISCES_XPMEM_H */
