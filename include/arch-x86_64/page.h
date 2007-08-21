#ifndef _X86_64_PAGE_H
#define _X86_64_PAGE_H

#define PAGE_SHIFT		12

/**
 * CAREFUL... Each of the following defines has two versions, one for
 *            assembly and another for C.  Be sure to update both!
 */
#ifndef __ASSEMBLY__
#define PAGE_SIZE		(1UL << PAGE_SHIFT)
#define __PHYSICAL_START        ((unsigned long)CONFIG_PHYSICAL_START)
#define __START_KERNEL          (__START_KERNEL_map + __PHYSICAL_START)
#define __START_KERNEL_map      0xffffffff80000000UL
#define __PAGE_OFFSET           0xffff810000000000UL
#else
#define PAGE_SIZE		(0x1 << PAGE_SHIFT)
#define __PHYSICAL_START        CONFIG_PHYSICAL_START
#define __START_KERNEL          (__START_KERNEL_map + __PHYSICAL_START)
#define __START_KERNEL_map      0xffffffff80000000
#define __PAGE_OFFSET           0xffff810000000000
#endif

#define TASK_ORDER 1 
#define TASK_SIZE  (PAGE_SIZE << TASK_ORDER)
#define CURRENT_MASK (~(TASK_SIZE-1))

#endif
