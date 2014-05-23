#ifndef __XPMEM_EXT_H__
#define __XPMEM_EXT_T__

#include <arch/pisces/pisces_xpmem.h>

int xpmem_pisces_xpmem_cmd(struct xpmem_dom * src_dom);
int xpmem_pisces_add_pisces_dom(void);
int xpmem_pisces_add_local_dom(void);
int xpmem_pisces_add_dom(int fd, int dom_id);
struct xpmem_dom * xpmem_pisces_get_dom_list(int * size);

#endif /* __XPMEM_EXT_H__ */
