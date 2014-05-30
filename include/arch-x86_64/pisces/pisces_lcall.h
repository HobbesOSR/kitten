#ifndef __PISCES_LCALL_H__
#define __PISCES_LCALL_H__

#include <lwk/types.h>

#define KERN_LCALL_START 10000
#define USER_LCALL_START 20000


/* KERNEL LCALLS (10000 - 19999) */
#define PISCES_LCALL_VFS_READ        (KERN_LCALL_START + 0)
#define PISCES_LCALL_VFS_WRITE       (KERN_LCALL_START + 1)
#define PISCES_LCALL_VFS_OPEN        (KERN_LCALL_START + 2)
#define PISCES_LCALL_VFS_CLOSE       (KERN_LCALL_START + 3)
#define PISCES_LCALL_VFS_SIZE        (KERN_LCALL_START + 4)
#define PISCES_LCALL_VFS_MKDIR       (KERN_LCALL_START + 5)
#define PISCES_LCALL_VFS_READDIR     (KERN_LCALL_START + 6)

#define PISCES_LCALL_PCI_ACK_IRQ     (KERN_LCALL_START + 100)
#define PISCES_LCALL_PCI_CMD         (KERN_LCALL_START + 101)
#define PISCES_LCALL_PCI_ATTACH      (KERN_LCALL_START + 102)
#define PISCES_LCALL_PCI_DETACH      (KERN_LCALL_START + 103)
#define PISCES_LCALL_IOMMU_MAP       (KERN_LCALL_START + 104)
#define PISCES_LCALL_IOMMU_UNMAP     (KERN_LCALL_START + 105)

#define PISCES_LCALL_XPMEM_VERSION   (KERN_LCALL_START + 200)
#define PISCES_LCALL_XPMEM_MAKE      (KERN_LCALL_START + 201)
#define PISCES_LCALL_XPMEM_REMOVE    (KERN_LCALL_START + 202)
#define PISCES_LCALL_XPMEM_GET       (KERN_LCALL_START + 203)
#define PISCES_LCALL_XPMEM_RELEASE   (KERN_LCALL_START + 204)
#define PISCES_LCALL_XPMEM_ATTACH    (KERN_LCALL_START + 205)
#define PISCES_LCALL_XPMEM_DETACH    (KERN_LCALL_START + 206)

/* USER LCALLS (20000 - 29999) */
#define PISCES_LCALL_PPE_MSG         (USER_LCALL_START + 0)
#define PISCES_LCALL_XPMEM_CMD_EX    (USER_LCALL_START + 200)





struct pisces_lcall {
	u64 lcall;
	u32 data_len;
	u8  data[0];
} __attribute__((packed));



struct pisces_lcall_resp {
	u64 status;
	u32 data_len;
	u8  data[0];
} __attribute__((packed));



int pisces_lcall_exec(struct pisces_lcall       * lcall, 
		      struct pisces_lcall_resp ** resp);

int pisces_lcall_init( void );


#endif
