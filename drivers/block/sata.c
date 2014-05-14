/* SATA Block Driver
 * (c) 2014, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/driver.h>
#include <lwk/pci/pci.h>
#include <lwk/interrupt.h>
#include <arch/fixmap.h>
#include <arch/apicdef.h>
#include <arch/io_apic.h>
#include <arch/io.h>
#include <lwk/blkdev.h>

#include <lwk/delay.h>
#include <lwk/spinlock.h>

#ifdef CONFIG_PISCES
#include <arch/pisces/pisces_irq_proxy.h>
#endif

#include "ata.h"
#include "sata-regs.h"

// Macros that read/write e1000 memory mapped registers
#define mmio_read32(offset)         *((volatile uint32_t *)(ahci_dev.bar_vaddr + (offset)))
#define mmio_write32(offset, value) *((volatile uint32_t *)(ahci_dev.bar_vaddr + (offset))) = (value)


static int ports_param_arr[32] = {[0 ... 31] = 0};
static int num_ports           = 0;

typedef struct {

	struct __attribute__((packed)) {
		u32 avail     : 1;  /* Set to 1 when this req slot is reserved */
		u32 active    : 1;  /* Set to 1 when this req is submitted to device */
		u32 async     : 1;  /* Set to 1 when this req is aynchronous */
	 	u32 rsvd      : 29;
	};

	int error;                    /* Set to error code of HBA, if the request failed */

	u32 num_prdts;                /* Number of PRD entries for DMA */

	blk_req_t * blk_req;          /* Pointer to block request */
} sata_req_t;




typedef struct sata_port_s {
	u32  port_num;
	u8   port_in_use;               /* Set when a port has been initialized */

	void * ctrl_page;               /* Page containing the RX-FIS/CMD-List */
	
	hba_rx_fis_t   * rx_fis;        /* RX FIS Structure (256 byte aligned) */
	sata_cmd_hdr_t * cmd_list;      /* Array of 32 Command Headers  (1KB aligned) */

	sata_req_t     * sata_reqs;     /* Internal state for requests from the block layer */
	u32              pending_cmds;  /* Bit mask that tracks which cmd slots we are waiting on */

	u32 sector_size;
	u64 sector_cnt; 

	spinlock_t port_lock;
} sata_port_t;


typedef struct ahci_dev_s {
	pci_dev_t * pci_dev;

	/* The first 5 bars are legacy IDE interfaces
	 * The SATA interface has its own dedicated mem bar 
	 * The capabilities space, might have more info on this
	 */
	pci_bar_t mem_bar;
	paddr_t   bar_paddr;
	vaddr_t   bar_vaddr;

	int num_cmd_slots;
	int num_ports;
	sata_port_t sata_ports[32];
} ahci_dev_t;


static ahci_dev_t ahci_dev;



static int 
is_disk_present(int port) {
	u32 pi = mmio_read32(HBA_HOST_REG_PI);

	if (pi & (0x1 << port)) {
		u32 ssts = mmio_read32(HBA_PORT_REG_PXSSTS(port));
		
		if (((hba_port_pxssts_t *)&ssts)->det == 0x3) {
			return 1;
		}
	}
	
	return 0;
}

static int 
get_free_cmd_slot(sata_port_t * sata_port) {
	int cmd_slot       = -1;
	unsigned int flags = 0;
	int i              = 0;

	spin_lock_irqsave(&(sata_port->port_lock), flags);
	{

		u32 ci_reg   = mmio_read32(HBA_PORT_REG_PXCI(sata_port->port_num));

		for (i = 0; i < ahci_dev.num_cmd_slots; i++) {
			if (sata_port->sata_reqs[i].avail == 0) {
				continue;
			}
			
		
			if ((ci_reg & (0x1 << i)) == 0) {
	
				sata_port->sata_reqs[i].avail = 0;
				cmd_slot = i;
				
				break;
			}
		}
	}
	spin_unlock_irqrestore(&(sata_port->port_lock), flags);

	//	printk("Got Cmd Slot=%d\n", cmd_slot);
	
	return cmd_slot;
}

static void
release_cmd_slot(sata_port_t * sata_port, int cmd_slot)
{
	sata_req_t * sata_req = &(sata_port->sata_reqs[cmd_slot]);
	unsigned int flags;

	//	printk("Releasing CMD Slot %d\n", cmd_slot);
	spin_lock_irqsave(&(sata_port->port_lock), flags);
	{
		sata_req->active    = 0;
		sata_req->async     = 0;
		sata_req->error     = 0;
		sata_req->blk_req   = NULL;
		sata_req->num_prdts = 0;
		sata_req->avail     = 1;
	}
	spin_unlock_irqrestore(&(sata_port->port_lock), flags);

	return;
}

static int last_issued_slot = -1;

static int 
issue_cmd(sata_port_t * sata_port, int cmd_slot) {
	int          port_num  = sata_port->port_num;
	sata_req_t * sata_req  = &(sata_port->sata_reqs[cmd_slot]);
	unsigned int flags;

	spin_lock_irqsave(&(sata_port->port_lock), flags);
	{
		sata_req->active = 1;
		sata_port->pending_cmds |= (0x1 << cmd_slot);
		
		mmio_write32(HBA_PORT_REG_PXCI(port_num), 0x1 << cmd_slot);
	}
	spin_unlock_irqrestore(&(sata_port->port_lock), flags);


	return 0;
}

static sata_cmd_tbl_t *  
alloc_cmd_table(sata_req_t * sata_req) {
	u32 table_len    = 0x80 + (sata_req->num_prdts * sizeof(sata_prd_tbl_t));
	u32 num_pages    = (table_len / PAGE_SIZE) + ((table_len % PAGE_SIZE) != 0);
	void * cmd_table = NULL;

	cmd_table = kmem_get_pages(get_order(num_pages));
	memset(cmd_table, 0, PAGE_SIZE * num_pages);

	return cmd_table;
}


static void 
free_cmd_table(sata_req_t * sata_req, sata_cmd_tbl_t * cmd_table) {
	u32 table_len  = 0x80 + (sata_req->num_prdts * sizeof(sata_prd_tbl_t));
	u32 num_pages  = (table_len / PAGE_SIZE) + ((table_len % PAGE_SIZE) != 0);

	kmem_free_pages(cmd_table, get_order(num_pages));
}


static int 
sata_dump_state(void * priv_data) 
{
	sata_port_t * sata_port = (sata_port_t *)priv_data;
	unsigned int  flags     = 0;
	int i  = 0;

	spin_lock_irqsave(&(sata_port->port_lock), flags);
	{

		printk("AHCI: DUMPING STATE (Port %d)\n", sata_port->port_num);

		printk("AHCI: ====>>>> HBA Registers <<<<====\n");
		printk("AHCI:\tHBA Capabilities           (CAP)       : 0x%x\n", mmio_read32(HBA_HOST_REG_CAP));
		printk("AHCI:\tGlobal HBA Control         (GHC)       : 0x%x\n", mmio_read32(HBA_HOST_REG_GHC));
		printk("AHCI:\tGlobal Intr Status         (IS)        : 0x%x\n", mmio_read32(HBA_HOST_REG_IS));
		printk("AHCI:\tPorts Implemented          (PI)        : 0x%x\n", mmio_read32(HBA_HOST_REG_PI));
		printk("AHCI:\tAHCI Version               (VS)        : 0x%x\n", mmio_read32(HBA_HOST_REG_VS));
		printk("AHCI:\tCCC Control                (CCC_CTL)   : 0x%x\n", mmio_read32(HBA_HOST_REG_CCC_CTL));
		printk("AHCI:\tCCC Ports                  (CCC_PORTS) : 0x%x\n", mmio_read32(HBA_HOST_REG_CCC_PORTS));
		printk("AHCI:\tEnc. Mgmt Location         (EM_LOC)    : 0x%x\n", mmio_read32(HBA_HOST_REG_EM_LOC));
		printk("AHCI:\tEnc. Mgmt Control          (EM_CTL)    : 0x%x\n", mmio_read32(HBA_HOST_REG_EM_CTL));
		printk("AHCI:\tHBA Ext Capabilities       (CAP2)      : 0x%x\n", mmio_read32(HBA_HOST_REG_CAP2));
		printk("AHCI:\tBIOS Handoff CTL/STS       (BOHC)      : 0x%x\n", mmio_read32(HBA_HOST_REG_BOHC));
	
		printk("AHCI: ====>>>> Port Registers <<<<====\n");
		printk("AHCI:\tCmd List Base Addr         (PXCLB)     : 0x%x\n", mmio_read32(HBA_PORT_REG_PXCLB(sata_port->port_num)));
		printk("AHCI:\tCmd List Base Addr (Hi)    (PXCLBU)    : 0x%x\n", mmio_read32(HBA_PORT_REG_PXCLBU(sata_port->port_num)));
		printk("AHCI:\tFIS Base Addr              (PXFB)      : 0x%x\n", mmio_read32(HBA_PORT_REG_PXFB(sata_port->port_num)));
		printk("AHCI:\tFIS Base Addr (Hi)         (PXFBU)     : 0x%x\n", mmio_read32(HBA_PORT_REG_PXFBU(sata_port->port_num)));
		printk("AHCI:\tIntr Status                (PXIS)      : 0x%x\n", mmio_read32(HBA_PORT_REG_PXIS(sata_port->port_num)));
		printk("AHCI:\tIntr Enable                (PXIE)      : 0x%x\n", mmio_read32(HBA_PORT_REG_PXIE(sata_port->port_num)));
		printk("AHCI:\tCommand and Status         (PXCMD)     : 0x%x\n", mmio_read32(HBA_PORT_REG_PXCMD(sata_port->port_num)));
		printk("AHCI:\tTask File Data             (PXTFD)     : 0x%x\n", mmio_read32(HBA_PORT_REG_PXTFD(sata_port->port_num)));
		printk("AHCI:\tSignature                  (PXSIG)     : 0x%x\n", mmio_read32(HBA_PORT_REG_PXSIG(sata_port->port_num)));
		printk("AHCI:\tSStatus                    (PXSSTS)    : 0x%x\n", mmio_read32(HBA_PORT_REG_PXSSTS(sata_port->port_num)));
		printk("AHCI:\tSControl                   (PXSCTL)    : 0x%x\n", mmio_read32(HBA_PORT_REG_PXSCTL(sata_port->port_num)));
		printk("AHCI:\tSError                     (PXSERR)    : 0x%x\n", mmio_read32(HBA_PORT_REG_PXSERR(sata_port->port_num)));
		printk("AHCI:\tSActive                    (PXSACT)    : 0x%x\n", mmio_read32(HBA_PORT_REG_PXSACT(sata_port->port_num)));
		printk("AHCI:\tCommand Issue              (PXCI)      : 0x%x\n", mmio_read32(HBA_PORT_REG_PXCI(sata_port->port_num)));
		printk("AHCI:\tSNotification              (PXSNTF)    : 0x%x\n", mmio_read32(HBA_PORT_REG_PXSNTF(sata_port->port_num)));
		printk("AHCI:\tFIS-based Switch Ctrl      (PXFBS)     : 0x%x\n", mmio_read32(HBA_PORT_REG_PXFBS(sata_port->port_num)));


		printk("AHCI: ====>>>> Internal Port State <<<<====\n");
		printk("AHCI:\tport_in_use       : %d\n",   sata_port->port_in_use);
		printk("AHCI:\tpending_cmds      : 0x%x\n", sata_port->pending_cmds);
		printk("AHCI:\tctrl_page addr    : %p\n",   sata_port->ctrl_page);
		printk("AHCI:\tSector Size       : %d\n",   sata_port->sector_size);
		printk("AHCI:\tSector Count      : %llu\n", sata_port->sector_cnt);

		printk("AHCI: ====>>>> Requests <<<<====\n");
		for (i = 0; i < ahci_dev.num_cmd_slots; i++) {
			sata_req_t * req = &(sata_port->sata_reqs[i]);

		
			if (req->avail   == 0) {
				printk("AHCI:\tReq %.2d -- avail=%d, active=%d, async=%d, error=%d, num_prdts=%d\n", 
				       i, req->avail, req->active, req->async, req->error, req->num_prdts);
				if (req->blk_req != NULL) {
					printk("AHCI:\t       -- BLKREQ: tot_len=%llu, off=%llu, desc_cnt=%u, write=%d, complete=%d\n",
					       req->blk_req->total_len, 
					       req->blk_req->offset, 
					       req->blk_req->desc_cnt,
					       req->blk_req->write, 
					       req->blk_req->complete);
				}
			}
		}
		

		printk("AHCI: ==============\n");
		printk("AHCI: ==============\n");

	}
	spin_unlock_irqrestore(&(sata_port->port_lock), flags);

	return 0;
}


static int 
identify_disk(sata_port_t * sata_port) {
	int              cmd_slot  = get_free_cmd_slot(sata_port);
	sata_req_t     * sata_req  = &(sata_port->sata_reqs[cmd_slot]);
	sata_cmd_tbl_t * cmd_table = NULL;
	fis_reg_h2d_t  * id_fis    = NULL;
	sata_cmd_hdr_t * cmd_hdr   = NULL;
	u8             * id_data   = NULL;
	int ret = 0;
	
	if (cmd_slot == -1) {
		printk(KERN_ERR "AHCI: Could not find free SATA CMD Slot on port %d\n", 
		       sata_port->port_num);
		return -1;
	}
	printk("Identifying device with cmd_slot=%d\n", cmd_slot);

	sata_req->async     = 0;
	sata_req->num_prdts = 1;

	cmd_hdr = &(sata_port->cmd_list[cmd_slot]);

	memset(cmd_hdr, 0, sizeof(sata_cmd_hdr_t));

	cmd_hdr->cfl   = 5;
	cmd_hdr->prdtl = 1;


	cmd_table     = alloc_cmd_table(sata_req);
	cmd_hdr->ctba = __pa(cmd_table);

	id_fis = (fis_reg_h2d_t *)cmd_table->cfis;

	id_fis->type = FIS_TYPE_REG_H2D;
	id_fis->cmd  = ATA_CMD_IDENTIFY;
	id_fis->dev  = 0;
	id_fis->c    = 1;

	id_data = kmem_alloc(512);
	memset(id_data, 0, 512);

	cmd_table->prdts[0].dba  = (uintptr_t)__pa(id_data);
	cmd_table->prdts[0].dbc  = 511;
	cmd_table->prdts[0].intr = 0;
	
	issue_cmd(sata_port, cmd_slot);

	while (sata_req->active != 0) {
	  //	        sata_dump_state(sata_port);
		mdelay(1000);
		__asm__ __volatile__ ("":::"memory");
	}

	ret = sata_req->error;
	release_cmd_slot(sata_port, cmd_slot);

	if (ret == 0) {
	    char model_str[40 + 1];
	    char firmware_str[8 + 1];
	    char serialno_str[20 + 1];

	    printk("AHCI: Port %d Disk information:\n", sata_port->port_num);
	    printk("\tModel: %s\n",             ata_get_model_str(id_data, model_str));	    
	    printk("\tSerial Num: %s\n",        ata_get_serialno_str(id_data, serialno_str));
	    printk("\tFirmware Revision: %s\n", ata_get_firmware_str(id_data, firmware_str));

	    /* We just have to hardcode it for now, because it doesn't seem to be available */
	    sata_port->sector_size = 512; 
	    sata_port->sector_cnt  = *(u64 *)(id_data + 200); 
	    printk("\tCapacity=%lluGB\n", (sata_port->sector_size * sata_port->sector_cnt) / (1024 * 1024 * 1024));
	    
	} else {
	    printk("AHCI: Error Retrieving Disk Information. (Maybe device is ATAPI?)\n");
	}
	
	kmem_free(id_data);

	return ret;
}



static int 
ahci_port_reset(int port_num) 
{
	int i = 0;
    u32 offset = 0;
	printk("Resetting AHCI Port %d\n", port_num);

	/* Stop Port */
	mmio_write32(HBA_PORT_REG_PXCMD(port_num), 
		     mmio_read32(HBA_PORT_REG_PXCMD(port_num)) & ~0x00000001);



	/* Disable all interrupts... */
	mmio_write32(HBA_PORT_REG_PXIE(port_num), 0x0);

	/* Clear PxIS (Intr Status) */
	mmio_write32(HBA_PORT_REG_PXIS(port_num), 0xffffffff);


	/* Wait for CR to clear */
	for (i = 0; i < 5; i++) {
		if ((mmio_read32(HBA_PORT_REG_PXCMD(port_num)) & (0x00000001 << 15)) == 0) {
			break;
		}

		mdelay(100);
		__asm__ __volatile__ ("":::"memory");
	}

	/* Initiate hard port reset */
	mmio_write32(HBA_PORT_REG_PXSCTL(port_num), 0x1);
	mdelay(2);
	mmio_write32(HBA_PORT_REG_PXSCTL(port_num),
		     mmio_read32(HBA_PORT_REG_PXSCTL(port_num)) & 0xfffffff0);
 
		     

	/* Wait for PxSSTS.det = 1 */
    offset = 0;
	while ((mmio_read32(HBA_PORT_REG_PXSSTS(port_num)) & 0x1) == 0) {
	    mdelay(100);
        if (!(offset % 10)) {
            printk("Wait for SSTS reset...\n");
        }
        offset++;
        __asm__ __volatile__ ("":::"memory");
	}

	mmio_write32(HBA_PORT_REG_PXSERR(port_num), 0xffffffff);

	/* Wait for PXSERR.diag.N (physRDY change) to = 1 */
    /*
	while ((mmio_read32(HBA_PORT_REG_PXSERR(port_num)) & (0x1 << 16)) == 0) {
		mdelay(100);
    }
	*/

    offset = 0;
    while (mmio_read32(HBA_PORT_REG_PXTFD(port_num)) & (0x1 << 7)) {
		mdelay(100);
        if (!(offset % 10)) {
            printk("Wait for TFD reset...\n");
        }
        offset++;
        __asm__ __volatile__ ("":::"memory");
    }
		     
	return 0;
}


static int 
ahci_port_init(sata_port_t * sata_port) {
	int i = 0;

	spin_lock_init(&(sata_port->port_lock));


	sata_port->port_in_use = 1;
	sata_port->ctrl_page   = kmem_get_pages(0);
	
	if (!sata_port->ctrl_page) {
		printk(KERN_ERR "AHCI: Could not allocate control page for SATA Port %d\n", 
		       sata_port->port_num);
		return -1;
	}

	memset(sata_port->ctrl_page, 0, PAGE_SIZE);

	/* 
	 * The command list will sit in the first 1KB 
	 * The FIS will sit in the first 256 bytes of the 2nd KB 
	 * The sata request array will sit at the start of the 3rd KB
	 */
	sata_port->cmd_list  = sata_port->ctrl_page;
	sata_port->rx_fis    = (sata_port->ctrl_page + 1024);
	sata_port->sata_reqs = (sata_port->ctrl_page + 2048);


	for (i = 0; i < ahci_dev.num_cmd_slots; i++) {
		// Mark all sata request slots available
		sata_port->sata_reqs[i].avail = 1;

		// Clear the CTBA, allocated on demand for each request
		sata_port->cmd_list[i].ctba = (u64)NULL;
	}

	mmio_write32(HBA_PORT_REG_PXCLB(sata_port->port_num),  (u32)__pa(sata_port->cmd_list));
	mmio_write32(HBA_PORT_REG_PXCLBU(sata_port->port_num), (u32)(__pa(sata_port->cmd_list) >> 32));
	mmio_write32(HBA_PORT_REG_PXFB(sata_port->port_num),   (u32)__pa(sata_port->rx_fis));
	mmio_write32(HBA_PORT_REG_PXFBU(sata_port->port_num),  (u32)(__pa(sata_port->rx_fis)   >> 32));

	
	/* Enable FRE */
	mmio_write32(HBA_PORT_REG_PXCMD(sata_port->port_num), 
		     mmio_read32(HBA_PORT_REG_PXCMD(sata_port->port_num)) | 0x10);

	/* Clear PxSERR */
	mmio_write32(HBA_PORT_REG_PXSERR(sata_port->port_num), 0xffffffff);

	/* Clear PxIS (Intr Status) */
	mmio_write32(HBA_PORT_REG_PXIS(sata_port->port_num),   0xffffffff);

	/* Enable All Interrupts but PDE */
	mmio_write32(HBA_PORT_REG_PXIE(sata_port->port_num),   0xffffffff & ~(0x1 << 4));

	/* Start Port */
	mmio_write32(HBA_PORT_REG_PXCMD(sata_port->port_num), 
		     mmio_read32(HBA_PORT_REG_PXCMD(sata_port->port_num)) | 0x1);

	return 0;
}

static int sata_handle_blkreq_slot(blk_req_t * blk_req, void * priv_data, int slot);

static int
ahci_port_handle_fatal_error(sata_port_t * sata_port) {
    unsigned long flags  = 0;
    int i = 0;
	int port_num = sata_port->port_num;

    printk("AHCI Error: Dumping state...\n");
    sata_dump_state(sata_port);

    printk("AHCI: Hard resetting port %d, reissuing outstanding requests...\n", port_num);
    printk("  Last issued cmd slot: %d\n", last_issued_slot); 

    spin_lock_irqsave(&(sata_port->port_lock), flags);

    ahci_port_reset(port_num);

    /* Clear PxSERR */
    mmio_write32(HBA_PORT_REG_PXSERR(sata_port->port_num), 0xffffffff);

    /* Clear PxIS (Intr Status) */
    mmio_write32(HBA_PORT_REG_PXIS(sata_port->port_num),   0xffffffff);

    /* Enable All Interrupts */
	mmio_write32(HBA_PORT_REG_PXIE(sata_port->port_num),   0xffffffff & ~(0x1 << 4));

    /* Start Port */
    mmio_write32(HBA_PORT_REG_PXCMD(sata_port->port_num), 
             mmio_read32(HBA_PORT_REG_PXCMD(sata_port->port_num)) | 0x1);

    spin_unlock_irqrestore(&(sata_port->port_lock), flags);

    printk("AHCI: Pending cmds: %x\n", sata_port->pending_cmds);

    {
        for (i = 0; i < ahci_dev.num_cmd_slots; i++) {
            if (sata_port->pending_cmds & (0x1 << i)) {
                printk("Reissuing blkdev request %d\n", i);
                sata_handle_blkreq_slot(sata_port->sata_reqs[i].blk_req, sata_port, i);
            }
        }
    }

    printk("AHCI: Port reset, dumping state:\n");
    sata_dump_state(sata_port);

    return 0;
}

static int 
ahci_port_handle_irq(sata_port_t * sata_port) {
	int port_num     = sata_port->port_num;
	u32 port_irq_sts = mmio_read32(HBA_PORT_REG_PXIS(port_num));
	u32 error        = 0;
	u32 status       = 0;
	int hba_error    = 0;

	//  printk("AHCI: SATA PORT IRQ STATUS=%x\n", port_irq_sts);
	//	printk("AHCI: SATA PORT CI=%x\n",         mmio_read32(HBA_PORT_REG_PXCI(port_num)));
    //
    
    if (port_irq_sts == 0x20) {
        int i;
        printk("AHCI: PRD FIS received\n");
        for (i = 0; i < 10; i++) {
            u32 sts = mmio_read32(HBA_PORT_REG_PXIS(port_num));
            if (sts != port_irq_sts) {
                port_irq_sts = sts;
                break;
            }
            mdelay(10);
        }
    }

	if (port_irq_sts & 0x01) {
		// Dev to host FIS received OK
        error  = sata_port->rx_fis->rfis.error;
		status = sata_port->rx_fis->rfis.status;
	} else if (port_irq_sts & 0x02) {
		// PIO Set FIS received OK
		error  = sata_port->rx_fis->psfis.error;
		status = sata_port->rx_fis->psfis.status;
	} else if (port_irq_sts & 0x04) {
		// DMA Setup FIS received OK
		printk("AHCI: DMA Setup FIS Received... \n");
	} else if (port_irq_sts & 0x08) {
		// Set Device BITS FIS received OK
		printk("AHCI: Set Device BITS FIS Received... \n");
	} else {

		if (port_irq_sts & 0x40000000) {
			// Task File ERROR 

			printk("AHCI: FIS ERROR\n");
			printk("AHCI: rfis_setup->status=%x\n", 
			       sata_port->rx_fis->rfis.status);
			printk("AHCI: rfis_setup->error=%x\n", 
			       sata_port->rx_fis->rfis.error);
		} else if (port_irq_sts & 0x08000000) {
			/* Fatal Error detected on interface */
			printk("Interface Fatal Error \n");
            ahci_port_handle_fatal_error(sata_port);
		} else {
			// Unknown ERROR
	        printk("AHCI Unknown error: SATA PORT IRQ STATUS=%x\n", port_irq_sts);
            ahci_port_handle_fatal_error(sata_port);
		}

		hba_error = 1;
	}

	if (port_irq_sts & 0x00400000) {
		/* PhyRdy Change Status (PRCS) */
		mmio_write32(HBA_PORT_REG_PXSERR(port_num), 0x10000); 
	}

	if (port_irq_sts & 0x00000040) {
		/* Port Connect Change Status (PCS) */
		mmio_write32(HBA_PORT_REG_PXSERR(port_num), 0x4000000); 
	}



	mmio_write32(HBA_PORT_REG_PXIS(port_num), port_irq_sts);


	if ( (hba_error == 0) && 
	     ((status & (ATA_STS_BUSY | ATA_STS_DF | ATA_STS_ERR)) == 0) && 
	     ((status & ATA_STS_RDY) != 0)) {
		u32 cmds_complete    = 0;
		unsigned long flags  = 0;
		int i = 0;
		// Success

		spin_lock_irqsave(&(sata_port->port_lock), flags);
		{
			cmds_complete = sata_port->pending_cmds ^ mmio_read32(HBA_PORT_REG_PXCI(port_num));
			sata_port->pending_cmds = mmio_read32(HBA_PORT_REG_PXCI(port_num));
		}
		spin_unlock_irqrestore(&(sata_port->port_lock), flags);

		// Check which cmd completed
		for (i = 0; i < ahci_dev.num_cmd_slots; i++) {
			if (cmds_complete & (0x1 << i)) {
				sata_req_t * sata_req = &(sata_port->sata_reqs[i]);
				// Finish Cmd i
				
				sata_req->active = 0;

				// Free Command table
				{
					free_cmd_table(sata_req, __va(sata_port->cmd_list[i].ctba));
					sata_port->cmd_list[i].ctba = (u64)NULL;
				}
		
				if (sata_req->async) {
					blk_req_complete(sata_req->blk_req, 0);
					release_cmd_slot(sata_port, i);
				} 
			}
		}

	} else {
        /* SIGNAL ERRORS to ALL outstanding REQUESTS */
	} 

	return 0;
}

static irqreturn_t
sata_irq_handler(int vector, void * priv) {
	u32 hba_intr_sts  = mmio_read32(HBA_HOST_REG_IS);
	u32 intrs_handled = 0;
	int i             = 0;

	//	printk("AHCI: SATA IRQ received\n");
	//	printk("AHCI: HBA IRQ=%x\n", hba_intr_sts);

	for (i = 0; i < ahci_dev.num_ports; i++) {
		sata_port_t * sata_port = &ahci_dev.sata_ports[i];

		if ((sata_port->port_in_use      == 1) && 
		    ((hba_intr_sts & (0x1 << i)) != 0)) {
			int ret = 0;

			ret = ahci_port_handle_irq(sata_port);

			if (ret == -1) {
				// Bad error
				// reset port 
			}

			intrs_handled |= (1 << i);
		}
	}

	mmio_write32(HBA_HOST_REG_IS, intrs_handled);

	if (intrs_handled != 0) {
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}



static int
sata_handle_blkreq_slot(blk_req_t * blk_req, void * priv_data, int slot) {
	sata_port_t    * sata_port = (sata_port_t *)priv_data;
	//int              cmd_slot  = get_free_cmd_slot(sata_port);
	int              cmd_slot  = slot;
	sata_req_t     * sata_req  = &(sata_port->sata_reqs[cmd_slot]);
	sata_cmd_tbl_t * cmd_table = NULL;
	fis_reg_h2d_t  * data_fis  = NULL;
	sata_cmd_hdr_t * cmd_hdr   = NULL;
	u64              sect_off  = 0;
	u64              sect_len  = 0;
	int i = 0;
	
	if (cmd_slot == -1) {
		printk(KERN_ERR "AHCI: Could not find free SATA CMD Slot on port %d\n", 
		       sata_port->port_num);
		return -EAGAIN;
	}

	if (blk_req->offset % sata_port->sector_size) {
		printk(KERN_ERR "AHCI: Block Request offset is misaligned.\n");
		printk(KERN_ERR "AHCI: \tByte Offset=%llu, sector_size=%u\n", 
		       blk_req->offset, sata_port->sector_size);
		return -EINVAL;
	}

	if (blk_req->total_len % sata_port->sector_size) {
		printk(KERN_ERR "AHCI: Block Request Length is misaligned\n");
		printk(KERN_ERR "AHCI: \tByte Length=%llu, sector_size=%u\n", 
		       blk_req->total_len, sata_port->sector_size);
		return -EINVAL;
	}

	/* Convert Byte Offset to Sector Offset */
	sect_off = blk_req->offset / sata_port->sector_size;

	/* Convert Byte Length to Sector Length */
	sect_len = blk_req->total_len / sata_port->sector_size;

	cmd_hdr = &(sata_port->cmd_list[cmd_slot]);
	memset(cmd_hdr, 0, sizeof(sata_cmd_hdr_t));

	cmd_hdr->cfl   = 5;
	cmd_hdr->prdtl = blk_req->desc_cnt;

	sata_req->num_prdts = blk_req->desc_cnt;
	sata_req->async     = 1;
	sata_req->blk_req   = blk_req;

	//	printk("Requesting %llu bytes from offset %llu\n",
	//	       blk_req->total_len, sect_off);

	/* Allocate Command Table */
	cmd_table     = alloc_cmd_table(sata_req);
	cmd_hdr->ctba = __pa(cmd_table);

	data_fis = (fis_reg_h2d_t *)cmd_table->cfis;

	data_fis->type = FIS_TYPE_REG_H2D;

	if (blk_req->write) {
		data_fis->cmd = ATA_CMD_WRITE_DMA_EXT;
	} else {
		data_fis->cmd = ATA_CMD_READ_DMA_EXT;
	}

	data_fis->c          = 1;
	data_fis->feature_lo = 1; /* ?? Should this be 0? */

	data_fis->lba    =  sect_off        & 0x00ffffff;
	data_fis->lba_hi = (sect_off >> 24) & 0x00ffffff;
	data_fis->cnt    =  sect_len;
	data_fis->dev    = (1 << 6);

	for (i = 0; i < blk_req->desc_cnt; i++) {
		cmd_table->prdts[i].dba  = blk_req->dma_descs[i].buf_paddr;
		cmd_table->prdts[i].dbc  = blk_req->dma_descs[i].length - 1;
		cmd_table->prdts[i].intr = 0;
	}

	cmd_table->prdts[i - 1].intr = 1;

	if (blk_req->write) {
		cmd_hdr->wr = 1;
	}

    last_issued_slot = cmd_slot;

	return issue_cmd(sata_port, cmd_slot);
}

static int
sata_handle_blkreq(blk_req_t * blk_req, void * priv_data) 
{
	sata_port_t    * sata_port = (sata_port_t *)priv_data;
	int              cmd_slot  = get_free_cmd_slot(sata_port);
	sata_req_t     * sata_req  = &(sata_port->sata_reqs[cmd_slot]);
	sata_cmd_tbl_t * cmd_table = NULL;
	fis_reg_h2d_t  * data_fis  = NULL;
	sata_cmd_hdr_t * cmd_hdr   = NULL;
	u64              sect_off  = 0;
	u64              sect_len  = 0;
	int i = 0;
	
	if (cmd_slot == -1) {
		printk(KERN_ERR "AHCI: Could not find free SATA CMD Slot on port %d\n", 
		       sata_port->port_num);
		return -EAGAIN;
	}

	if (blk_req->offset % sata_port->sector_size) {
		printk(KERN_ERR "AHCI: Block Request offset is misaligned.\n");
		printk(KERN_ERR "AHCI: \tByte Offset=%llu, sector_size=%u\n", 
		       blk_req->offset, sata_port->sector_size);
		return -EINVAL;
	}

	if (blk_req->total_len % sata_port->sector_size) {
		printk(KERN_ERR "AHCI: Block Request Length is misaligned\n");
		printk(KERN_ERR "AHCI: \tByte Length=%llu, sector_size=%u\n", 
		       blk_req->total_len, sata_port->sector_size);
		return -EINVAL;
	}

	/* Convert Byte Offset to Sector Offset */
	sect_off = blk_req->offset / sata_port->sector_size;

	/* Convert Byte Length to Sector Length */
	sect_len = blk_req->total_len / sata_port->sector_size;

	cmd_hdr = &(sata_port->cmd_list[cmd_slot]);
	memset(cmd_hdr, 0, sizeof(sata_cmd_hdr_t));

	cmd_hdr->cfl   = 5;
	cmd_hdr->prdtl = blk_req->desc_cnt;

	sata_req->num_prdts = blk_req->desc_cnt;
	sata_req->async     = 1;
	sata_req->blk_req   = blk_req;

	//	printk("Requesting %llu bytes from offset %llu\n",
	//	       blk_req->total_len, sect_off);

	/* Allocate Command Table */
	cmd_table     = alloc_cmd_table(sata_req);
	cmd_hdr->ctba = __pa(cmd_table);

	data_fis = (fis_reg_h2d_t *)cmd_table->cfis;

	data_fis->type = FIS_TYPE_REG_H2D;

	if (blk_req->write) {
		data_fis->cmd = ATA_CMD_WRITE_DMA_EXT;
	} else {
		data_fis->cmd = ATA_CMD_READ_DMA_EXT;
	}

	data_fis->c          = 1;
	data_fis->feature_lo = 1; /* ?? Should this be 0? */

	data_fis->lba    =  sect_off        & 0x00ffffff;
	data_fis->lba_hi = (sect_off >> 24) & 0x00ffffff;
	data_fis->cnt    =  sect_len;
	data_fis->dev    = (1 << 6);

	for (i = 0; i < blk_req->desc_cnt; i++) {
		cmd_table->prdts[i].dba  = blk_req->dma_descs[i].buf_paddr;
		cmd_table->prdts[i].dbc  = blk_req->dma_descs[i].length - 1;
		cmd_table->prdts[i].intr = 0;
	}

	cmd_table->prdts[i - 1].intr = 1;

	if (blk_req->write) {
		cmd_hdr->wr = 1;
	}

    last_issued_slot = cmd_slot;

	return issue_cmd(sata_port, cmd_slot);
}



static blkdev_ops_t sata_blk_ops = {
	.handle_blkreq = sata_handle_blkreq,
	.dump_state    = sata_dump_state
};


static int 
ahci_port_probe(int port_num)
{
	sata_port_t * sata_port = NULL;

	if (port_num > ahci_dev.num_ports || port_num < 0) {
		printk(KERN_ERR "AHCI: Requested non-existent SATA port (%d) MAX=%d\n", 
		       port_num, ahci_dev.num_ports);
		return -1;
	}

	if (!is_disk_present(port_num)) {
		printk(KERN_ERR "AHCI: No Disk could be found on SATA port %d\n", port_num);
		return -1;
	}

	if (mmio_read32(HBA_PORT_REG_PXSIG(port_num)) != SATA_SIG_ATA) {
		printk(KERN_ERR "AHCI: SATA Port %d is not an ATA compatible Disk\n", port_num);
		return -1;
	}


	ahci_port_reset(port_num);

	sata_port = &(ahci_dev.sata_ports[port_num]);
	sata_port->port_num = port_num;

	ahci_port_init(sata_port);
	
	printk("Port Initialized, identifying\n");
	identify_disk(sata_port);

	{
		char name[32] = {[0 ... 31] = 0};
		snprintf(name, 32, "sata-%d", port_num);

		blkdev_register(name, &sata_blk_ops, 
				sata_port->sector_size, sata_port->sector_cnt, 
				0xffff, ahci_dev.num_cmd_slots, 
				sata_port);
	}

        return 0;
}

static int 
sata_init(void) 
{
	pci_dev_t * pci_dev = NULL;
	int irq_vec         = 0;
	int i               = 0;

	memset(&ahci_dev, 0, sizeof(ahci_dev_t));


	// Check if SATA is available
	// Dell Optiplex   -- 8086:1c02
	// Dell R420       -- 8086:1d02
	// VMWARE Fusion 6 -- 15ad:07e0
	if ( ((pci_dev = pci_lookup_device(0x15ad, 0x07e0)) == NULL) &&
	     ((pci_dev = pci_lookup_device(0x8086, 0x1d02)) == NULL) &&
	     ((pci_dev = pci_lookup_device(0x8086, 0x1c02)) == NULL) ) 
	{
		return -1;
	}
	    
	
	printk("AHCI: Found HBA PCI device\n");

	ahci_dev.pci_dev = pci_dev;

	// Find the bars
	pcicfg_bar_decode(&pci_dev->cfg, 5, &(ahci_dev.mem_bar));
	ahci_dev.bar_paddr = ahci_dev.mem_bar.address;
	ahci_dev.bar_vaddr = (vaddr_t)__va(ahci_dev.bar_paddr);


	{
		// Enable PCI bus mastering, so the card can initiate memory requests.
		// On VMware, E1000 bus mastering is disabled after power on, so the
		// card can't actually send or receive anything until this bit is set.
		uint16_t cmd = pci_read(pci_dev, PCIR_COMMAND, 2);
		cmd |= PCIM_CMD_BUSMASTEREN;
		pci_write(pci_dev, PCIR_COMMAND, 2, cmd);
	}

	{
		u32 cap = 0;
		//	u32 ctrl = 0;

		cap = mmio_read32(HBA_HOST_REG_CAP);
		//	ctrl = mmio_read32(HBA_HOST_REG_GHC);
		
		ahci_dev.num_cmd_slots = ((hba_host_reg_cap_t *)&cap)->ncs + 1;
		ahci_dev.num_ports     = ((hba_host_reg_cap_t *)&cap)->np + 1;

		printk("AHCI: HBA ports=%d, CMD Slots=%d\n", 
		       ahci_dev.num_ports, ahci_dev.num_cmd_slots);
		
	}


	// grab the IRQ
	irq_vec = ioapic_pcidev_vector(ahci_dev.pci_dev->cfg.bus, ahci_dev.pci_dev->cfg.slot, 0);

	if (irq_vec == -1) {
		printk(KERN_ERR "AHCI: Failed to find interrupt vector.\n");
		return -1;
	}

	irq_request(irq_vec, &sata_irq_handler, 0, "AHCI", NULL);

	// Clear pending HBA irqs
	mmio_write32(HBA_HOST_REG_IS, 0);

	/* Enable HBA Interrupts */
	mmio_write32(HBA_HOST_REG_GHC, mmio_read32(HBA_HOST_REG_GHC) | 0x2);

	// Allocate + configure ports
	printk("AHCI: Driver Registering %d port(s) on IRQ %d\n", num_ports, irq_vec);

	for (i = 0; i < num_ports; i++) {
		ahci_port_probe(ports_param_arr[i]);
	}

	return 0;
}


#ifdef CONFIG_PISCES

static int 
pisces_sata_init(void) 
{
	pci_dev_t * pci_dev = NULL;
	int irq_vec         = 0;
        int i;
 
	memset(&ahci_dev, 0, sizeof(ahci_dev_t));
 

	// Check if SATA is available
	// Dell Optiplex   -- 8086:1c02
	// Dell R420       -- 8086:1d02
	// VMWARE Fusion 6 -- 15ad:07e0
	if ( ((pci_dev = pci_lookup_device(0x15ad, 0x07e0)) == NULL) &&
	     ((pci_dev = pci_lookup_device(0x8086, 0x1d02)) == NULL) &&
	     ((pci_dev = pci_lookup_device(0x8086, 0x1c02)) == NULL) ) 
	{
                printk(KERN_ERR "AHCI Device Not Found!!\n");
		return -1;
	}
	    
	
	printk("AHCI: Found HBA PCI device\n");

	ahci_dev.pci_dev = pci_dev;

	// Find the bars
	pcicfg_bar_decode(&pci_dev->cfg, 5, &(ahci_dev.mem_bar));
	ahci_dev.bar_paddr = ahci_dev.mem_bar.address;
	ahci_dev.bar_vaddr = (vaddr_t) ioremap(ahci_dev.bar_paddr, ahci_dev.mem_bar.size);

	{
		u32 cap = 0;
		//	u32 ctrl = 0;

		cap = mmio_read32(HBA_HOST_REG_CAP);
		//	ctrl = mmio_read32(HBA_HOST_REG_GHC);
		
		ahci_dev.num_cmd_slots = ((hba_host_reg_cap_t *)&cap)->ncs + 1;
		ahci_dev.num_ports     = ((hba_host_reg_cap_t *)&cap)->np + 1;

		printk("AHCI: HBA ports=%d, CMD Slots=%d\n", 
		       ahci_dev.num_ports, ahci_dev.num_cmd_slots);
		
	}


	// grab the IRQ
        irq_vec = 67;

	printk("AHCI: Setup forwarding on IRQ %d\n", irq_vec);
        if (pisces_setup_irq_proxy(ahci_dev.pci_dev, &sata_irq_handler, irq_vec, &ahci_dev)
                < 0) {
            printk(KERN_ERR "AHCI: Failed to Setup IRQ Proxy!!\n");
        }

	// Clear pending HBA irqs
	// mmio_write32(HBA_HOST_REG_IS, 0);

	/* Enable HBA Interrupts */
	// mmio_write32(HBA_HOST_REG_GHC, mmio_read32(HBA_HOST_REG_GHC) | 0x2);

	// Allocate + configure ports
	printk("AHCI: Driver Registering %d port(s) on IRQ %d\n", num_ports, irq_vec);

	for (i = 0; i < num_ports; i++) {
                printk("AHCI: %d: probe port %d\n", i, ports_param_arr[i]);
                ahci_port_probe(ports_param_arr[i]);
	}

	return 0;
}

#endif


#ifdef CONFIG_PISCES
DRIVER_INIT("block", pisces_sata_init);
#else
DRIVER_INIT("block", sata_init);
#endif

DRIVER_PARAM_ARRAY_NAMED(ports, ports_param_arr, int, &num_ports);
