/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * Cross Partition Memory (XPMEM) structures and macros.
 */

#ifndef _XPMEM_EXT_H
#define _XPMEM_EXT_H

#include <lwk/xpmem/xpmem.h>

int xpmem_version(void);
xpmem_segid_t xpmem_make(void *, size_t, int, void *);
int xpmem_remove(xpmem_segid_t);
xpmem_apid_t xpmem_get(xpmem_segid_t, int, int, void *);
int xpmem_release(xpmem_apid_t);
void *xpmem_attach(struct xpmem_addr, size_t, void *);
int xpmem_detach(void *);

#endif /* _XPMEM_EXT_H */
