/*
 * Copyright (c) 2004 Cray Inc.
 *
 * The contents of this file is proprietary information of Cray Inc. 
 * and may not be disclosed without prior written consent.
 *
 */
/*
 * This code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */


#ifndef	__RCA_L0_LINUX_H__
#define	__RCA_L0_LINUX_H__

#include <lwk/version.h>

/* 
 * LINUX:
 * This works as long as the physical address is below 4GB and a static 
 * page table mapping has been setup for this address. This macro is 
 * intended to be used before ioremap() is available for e.g in the case
 * of early_printk.
 */
#define rca_l0_comm_va(addr) \
	(void*)(((unsigned long)0xFFFFFFFF << 32) | (unsigned long)(addr))

extern int l0rca_os_init(void);

#endif	/* __RCA_L0_LINUX_H__ */
