/* SATA Interface control structures
 * (c) 2014, Jack Lange <jacklange@cs.pitt.edu>
 */

/**
 * Generic Host Control Registers 
 */

/* Generic Host Control Register Layout */
typedef struct __attribute__((packed)) {
    volatile u32 cap;           /* Host Capabilities */
    volatile u32 ghc;           /* Global Host Control */
    volatile u32 is;            /* Interrupt Status */
    volatile u32 pi;            /* Ports Implemented */
    volatile u32 vs;            /* Version */
    volatile u32 ccc_ctl;       /* Cmd Completion coalescing ctrl */
    volatile u32 ccc_ports;     /* Cmd Completion coalescing ports */
    volatile u32 em_loc;        /* Enclosure mgmt location */
    volatile u32 em_ctl;        /* Enclosure mgmt ctrl */
    volatile u32 cap2;          /* Host Capabilities extended */
    volatile u32 bohc;          /* BIOS/OS handoff ctrl and status */
} hba_host_ctrl_t;

#define HBA_HOST_REG_CAP        0x0000
#define HBA_HOST_REG_GHC        0x0004
#define HBA_HOST_REG_IS         0x0008
#define HBA_HOST_REG_PI         0x000C
#define HBA_HOST_REG_VS         0x0010
#define HBA_HOST_REG_CCC_CTL    0x0014
#define HBA_HOST_REG_CCC_PORTS  0x0018
#define HBA_HOST_REG_EM_LOC     0x001c
#define HBA_HOST_REG_EM_CTL     0x0020
#define HBA_HOST_REG_CAP2       0x0024
#define HBA_HOST_REG_BOHC       0x0028



/* Host Control Register Fields */
typedef struct __attribute__((packed)) {
    u32 np         : 5;  /* (RO) No. of Ports */
    u32 sxs        : 1;  /* (RO) Supports ext. SATA */
    u32 ems        : 1;  /* (RO) Enclosure Mgmt. Supported */
    u32 cccs       : 1;  /* (RO) Cmd. Completion Coalescing Support */
    u32 ncs        : 5;  /* (RO) No. of Cmd slots */
    u32 psc        : 1;  /* (RO) Partial State Capable */
    u32 ssc        : 1;  /* (RO) Slumber State Capable */
    u32 pmd        : 1;  /* (RO) PIO Mult. DRQ Block */
    u32 fbss       : 1;  /* (RO) FIS-based Switching Supported */
    u32 spm        : 1;  /* (RO) Supports Port Multiplier */
    u32 sam        : 1;  /* (RO) Supports AHCI Mode Only */
    u32 rsvd       : 1;  /* (RO) Reserved */
    u32 iss        : 4;  /* (RO) Interface Speed Support */
    u32 sclo       : 1;  /* (RO) Supports Cmd List Override */
    u32 sal        : 1;  /* (RO) Supports Activity LED */
    u32 salp       : 1;  /* (RO) Supports Agressive Link Pwr Mgmt */
    u32 sss        : 1;  /* (RO) Supports Staggered Spin-up */
    u32 smps       : 1;  /* (RO) Supports Mech. Presence Switch */
    u32 ssntf      : 1;  /* (RO) Supports SNotification Reg */
    u32 sncq       : 1;  /* (RO) Supports native cmd queuing */
    u32 s64a       : 1;  /* (RO) Supports 64-bit Addressing */
} hba_host_reg_cap_t;

/* Global HBA Control */
typedef struct __attribute__((packed)) {
    u32 hr         : 1;  /* (RW1) HBA Reset */
    u32 ie         : 1;  /* (RW) Interrupt Enable */
    u32 mrsm       : 1;  /* (RO) MSI Revert to Single Msg. */
    u32 rsvd       : 28; /* (RO) Reserved */
    u32 ae         : 1;  /* (RW/RO) AHCI Enable */
} hba_host_reg_ghc_t;


/* Interrupt Status Register */
typedef struct __attribute__((packed)) {
    u32 ips;             /* (RWC) Interrupt Pending Status */
} hba_host_reg_is_t; 


/* Ports Implemented */
typedef struct __attribute__((packed)) {
    u32 pi;              /* (RO) Ports implemented */
} hba_host_reg_pi_t; 

/* AHCI Version */
typedef struct __attribute__((packed)) {
    u32 mnr        :16;  /* (RO) Major version */
    u32 mjr        :16;  /* (RO) Minor version */
} hba_host_reg_vs_t;


/* Command Completion Coalescing Control */
typedef struct __attribute__((packed)) {
    u32 en         : 1;  /* (RW) Enable */
    u32 rsvd       : 2;  /* (RO) Reserved */
    u32 intr       : 5;  /* (RO) Interrupt */
    u32 cc         : 8;  /* (RW) Command Completions */
    u32 tv         : 16; /* (RW) Timeout Value */
} hba_host_reg_ccc_ctl_t;

/* Command Completion Coalescing Ports */
typedef struct __attribute__((packed)) {
    u32 prt;             /* (RW) Ports */
} hba_host_reg_ccc_ports_t;

/* Enclosure Management Location */
typedef struct __attribute__((packed)) {
    u32 sz         : 16; /* (RO) Buffer Size */
    u32 ofst       : 16; /* (RO) Offset */
} hba_host_reg_em_loc_t; 

/* Enclosure Management Control */
typedef struct __attribute__((packed)) {
    u32 sts_mr     : 1;  /* (RWC) Message Received */
    u32 rsvd1      : 7;  /* (RO) Reserved */
    u32 ctl_tm     : 1;  /* (RW1) Transmit Message */
    u32 ctl_rst    : 1;  /* (RW1) Reset */
    u32 rsvd2      : 6;  /* (RO) Reserved */
    u32 supp_led   : 1;  /* (RO) LED Message Types */
    u32 supp_safte : 1;  /* (RO) SAF-TE Enclosure Mgmt Msgs */
    u32 supp_ses2  : 1;  /* (RO) SES-2 Enclosure Mgmt Msgs */
    u32 supp_sgpio : 1;  /* (RO) SGPIO Enclosure Mgmt Msgs */
    u32 rsvd3      : 4;  /* (RO) Reserved */
    u32 attr_smb   : 1;  /* (RO) Single Message Buffer */
    u32 attr_xmt   : 1;  /* (RO) Transmit Only */
    u32 attr_alhd  : 1;  /* (RO) Activity LED Hardware driven */
    u32 attr_pm    : 1;  /* (RO) Port Multiplier Support */
    u32 rsvd4      : 4;  /* (RO) Reserved */
} hba_host_reg_em_ctl_t; 

/* HBA Capabilities Extended */
typedef struct __attribute__((packed)) {
    u32 boh        : 1;  /* (RO) BIOS/OS Handoff */
    u32 rsvd       : 31; /* (RO) Reserved */
} hba_host_reg_cap2_t;

/* BIOS/OS Handoff Control and Status */
typedef struct __attribute__((packed)) {
    u32 bos        : 1;  /* (RW) BIOS Owned Semaphore */
    u32 oos        : 1;  /* (RW) OS Owned Semaphore */
    u32 sooe       : 1;  /* (RW) SMI on OS Ownership Change Enable */
    u32 ooc        : 1;  /* (RWC) OS Ownership Change */
    u32 bb         : 1;  /* (RW) BIOS Busy */
    u32 rsvd       : 27; /* (RO) Reserved */
} hba_host_reg_bohc_t;

/** 
 * Port Registers 
 */

/* Port Register Layout */
typedef struct __attribute__((packed)) {
    volatile u32 pxclb;         /* Port x Cmd List Base Addr */
    volatile u32 pxclbu;        /* Port x Cmd List Base Addr Hi */
    volatile u32 pxfb;          /* Port x FIS Base Addr */
    volatile u32 pxfbu;         /* Port x FIS Base Addr Hi */
    volatile u32 pxis;          /* Port x Interrupt Status */
    volatile u32 pxie;          /* Port x Interrupt Enable */
    volatile u32 pxcmd;         /* Port x Cmd and Status */
    volatile u32 rsvd1;         /* Reserved */
    volatile u32 pxtfd;         /* Port x Task File Data */
    volatile u32 pxsig;         /* Port x Signature */
    volatile u32 pxssts;        /* Port x SATA Status (SCR0: SStatus) */
    volatile u32 pxsctl;        /* Port x SATA Ctrl   (SCR2: SControl) */
    volatile u32 pxserr;        /* Port x SATA Error  (SCR1: SError) */
    volatile u32 pxsact;        /* Port x SATA Active (SCR3: SActive) */
    volatile u32 pxci;          /* Port x Command Issue */
    volatile u32 pxsntf;        /* Port x SATA Notification (SCR4: SNotification) */
    volatile u32 pxfbs;         /* Port x FIS-based Switching Control */
    volatile u32 rsvd2;         /* Reserved */
    volatile u32 pxvs;          /* Port x Vendor Specific */
} hba_port_ctrl_t;

#define HBA_PORT_OFFSET(port)       (0x100 + (0x80 * port))
#define HBA_PORT_REG_PXCLB(port)    (HBA_PORT_OFFSET(port) + 0x0000)
#define HBA_PORT_REG_PXCLBU(port)   (HBA_PORT_OFFSET(port) + 0x0004)
#define HBA_PORT_REG_PXFB(port)     (HBA_PORT_OFFSET(port) + 0x0008)
#define HBA_PORT_REG_PXFBU(port)    (HBA_PORT_OFFSET(port) + 0x000c)
#define HBA_PORT_REG_PXIS(port)     (HBA_PORT_OFFSET(port) + 0x0010)
#define HBA_PORT_REG_PXIE(port)     (HBA_PORT_OFFSET(port) + 0x0014)
#define HBA_PORT_REG_PXCMD(port)    (HBA_PORT_OFFSET(port) + 0x0018)
#define HBA_PORT_REG_PXTFD(port)    (HBA_PORT_OFFSET(port) + 0x0020)
#define HBA_PORT_REG_PXSIG(port)    (HBA_PORT_OFFSET(port) + 0x0024)
#define HBA_PORT_REG_PXSSTS(port)   (HBA_PORT_OFFSET(port) + 0x0028)
#define HBA_PORT_REG_PXSCTL(port)   (HBA_PORT_OFFSET(port) + 0x002c)
#define HBA_PORT_REG_PXSERR(port)   (HBA_PORT_OFFSET(port) + 0x0030)
#define HBA_PORT_REG_PXSACT(port)   (HBA_PORT_OFFSET(port) + 0x0034)
#define HBA_PORT_REG_PXCI(port)     (HBA_PORT_OFFSET(port) + 0x0038)
#define HBA_PORT_REG_PXSNTF(port)   (HBA_PORT_OFFSET(port) + 0x003c)
#define HBA_PORT_REG_PXFBS(port)    (HBA_PORT_OFFSET(port) + 0x0040)
#define HBA_PORT_REG_PXVS(port)     (HBA_PORT_OFFSET(port) + 0x0070)



/* Port x Interrupt Status */
typedef struct __attribute__((packed)) {
    u32 dhrs       : 1;  /* (RWC) Device to Host Register FIS Interrupt */
    u32 pss        : 1;  /* (RWC) PIO Setup FIS Interrupt */
    u32 dss        : 1;  /* (RWC) DMA Setup FIS Interrupt */
    u32 sdbs       : 1;  /* (RWC) Set Device Bits Interrupt */
    u32 ufs        : 1;  /* (RO) Unknown FIS Interrupt */
    u32 dps        : 1;  /* (RWC) Descriptor Processed */
    u32 pcs        : 1;  /* (RO) Port Connect Change Status */
    u32 dmps       : 1;  /* (RWC) Dev. Mech. Presence Status */
    u32 rsvd1      : 14; /* (RO) Rsserved */
    u32 prcs       : 1;  /* (RO) PhyRdy Change Status */
    u32 ipms       : 1;  /* (RWC) Incorrect Port Multiplier Status */
    u32 ofs        : 1;  /* (RWC) Overflow Status */
    u32 rsvd2      : 1;  /* (RO) Reserved */
    u32 infs       : 1;  /* (RWC) Interface Non-fatal Error Status */
    u32 ifs        : 1;  /* (RWC) Interface Fatal Error Status */
    u32 hbds       : 1;  /* (RWC) Host Bus Data Error Status */
    u32 hbfs       : 1;  /* (RWC) Host Bus Fatal Error Status */
    u32 tfes       : 1;  /* (RWC) Task File Error Status */
    u32 cpds       : 1;  /* (RWC) Cold Port Detect Status */
} hba_port_pxis_t;


/* Port x Interrupt Enable */
typedef struct __attribute__((packed)) {
    u32 dhre       : 1;  /* (RW) Device to Host Register FIS Intr Enable */
    u32 pse        : 1;  /* (RW) PIO Setup FIS Intr Enable */
    u32 dse        : 1;  /* (RW) DMA Setup FIS Intr Enable */
    u32 sdbe       : 1;  /* (RW) Set Device Bits FIS Intr Enable */
    u32 ufe        : 1;  /* (RW) Unknown FIS Intr Enable */
    u32 dpe        : 1;  /* (RW) Descriptor Processed Intr Enable */
    u32 pce        : 1;  /* (RW) Port Change Intr Enable */
    u32 dmpe       : 1;  /* (RW/RO) Device Mech. Presence Enable */
    u32 rsvd1      : 14; /* (RO) Reserved */
    u32 prce       : 1;  /* (RW) PhyRdy Change Intr Enable */
    u32 ipme       : 1;  /* (RW) Incorrect Port Multiplier Enable */
    u32 ofe        : 1;  /* (RW) Overflow Enable */
    u32 rsvd2      : 1;  /* (RO) Reserved */
    u32 infe       : 1;  /* (RW) Interface Non-fatal Error Enable */
    u32 ife        : 1;  /* (RW) Interface Fatal Error Enable */
    u32 hbde       : 1;  /* (RW) Host Bus Data Error Enable */
    u32 hbfe       : 1;  /* (RW) Host Bus Fatal Error Enable */
    u32 tfee       : 1;  /* (RW) Task File Error Enable */
    u32 cpde       : 1;  /* (RW/RO) Cold Presence Detect Enable */
} hba_port_pxie_t;


/* Port x Command and Status */
typedef struct __attribute__((packed)) {
    u32 st         : 1;  /* (RW) Start */
    u32 sud        : 1;  /* (RW/RO) Spin-Up Device */
    u32 pod        : 1;  /* (RW/RO) Power On Device */
    u32 clo        : 1;  /* (RW1) Command List Override */
    u32 fre        : 1;  /* (RW) FIS Receive Enable */
    u32 rsvd1      : 3;  /* (RO) Reserved */
    u32 ccs        : 1;  /* (RO) Current Command Slot */
    u32 mpss       : 1;  /* (RO) Mech. Presence Switch State */
    u32 fr         : 1;  /* (RO) FIS Receive Running */
    u32 cr         : 1;  /* (RO) Command List Running */
    u32 cps        : 1;  /* (RO) Cold Presence State */
    u32 pma        : 1;  /* (RW/RO) Port Multiplier Attached */
    u32 hpcp       : 1;  /* (RO) Hot Plug Capable Port */
    u32 mpsp       : 1;  /* (RO) Mech. Presence Switch Attached to Port */
    u32 cpd        : 1;  /* (RO) Cold Presence Detection */
    u32 esp        : 1;  /* (RO) External SATA Port */
    u32 fbscp      : 1;  /* (RO) FIS-based Switching Capable Port */
    u32 rsvd2      : 1;  /* (RO) Reserved */
    u32 atapi      : 1;  /* (RW) Device is ATAPI */
    u32 dlae       : 1;  /* (RW) Drive LED on ATAPI Enable */
    u32 alpe       : 1;  /* (RW/RO) Aggressive Link Power Mgmt Enable */
    u32 asp        : 1;  /* (RW/RO) Aggressive Slumber / Partial */
    u32 icc        : 1;  /* (RW) Interface Comm. Control */
} hba_port_pxcmd_t;


/* Port x Task File Data */
typedef struct __attribute__((packed)) {
    u32 sts        : 8;  /* (RO) Status */
    u32 err        : 8;  /* (RO) Error */
    u32 rsvd       : 16; /* (RO) Reserved */
} hba_port_pxtfd_t;


/* Port x Signature */
#define SATA_SIG_ATA    0x00000101  /* ATA Drive */
#define SATA_SIG_ATAPI  0xeb140101  /* ATAPI Drive */
#define SATA_SIG_SEMB   0xc33c0101  /* Enclosure Mgmt Bridge */
#define SATA_SIG_PM     0x96690101  /* Port Multiplier */

typedef struct __attribute__((packed)) {
    u32 sec_cnt    : 8;  /* (RO) Sector Count */
    u32 lba_low    : 8;  /* (RO) LBA Low Register */
    u32 lba_mid    : 8;  /* (RO) LBA Mid Register */
    u32 lba_high   : 8;  /* (RO) LBA High Register */
} hba_port_pxsig_t; 

/* Port x SATA Status */
typedef struct __attribute__((packed)) {
    u32 det        : 4;  /* (RO) Device Detection */
    u32 spd        : 4;  /* (RO) Current Interface Speed */
    u32 ipm        : 4;  /* (RO) Interface Power Management */
    u32 rsvd       : 4;  /* (RO) Reserved */
} hba_port_pxssts_t;

/* Port x SATA Control */
typedef struct __attribute__((packed)) {
    u32 det        : 4;  /* (RW) Device Detection Initialization */
    u32 spd        : 4;  /* (RW) Speed Allowed */
    u32 ipm        : 4;  /* (RW) Interface Power Mgmt Transitions Allowed */
    u32 spm        : 4;  /* (RO) Select Power Management <Not used by AHCI> */
    u32 pmp        : 4;  /* (RO) Port Multiplier Port  <Not used by AHCI> */
    u32 rsvd       : 12; /* (RO) Reserved */
} hba_port_pxsctl_t;

/* Port x SATA Error */
typedef struct __attribute__((packed)) {
    union __attribute__((packed)) {
	u32 err    : 16; /* (RWC) Error */
	struct __attribute__((packed)) {
	    u32 err_i      : 1;     /* Recovered Data Integrity Error */
	    u32 err_m      : 1;     /* Recovered Comm. Error */
	    u32 err_rsvd1  : 6;     /* Reserved */
	    u32 err_t      : 1;     /* Transient Data Integrity Error */
	    u32 err_c      : 1;     /* Persistent Comm. or Data Integrity Error */
	    u32 err_p      : 1;     /* Protocol Error */
	    u32 err_e      : 1;     /* Internal Error */
	    u32 err_rsvd2  : 4;     /* Reserved */
	};
    };

    union __attribute__((packed)) {
	u32 diag   : 16; /* (RWC) Diagnostics */
	struct __attribute__((packed)) {
	    u32 diag_n     : 1;     /* PhyRdy Change */
	    u32 diag_i     : 1;     /* Phy Internal Error */
	    u32 diag_w     : 1;     /* Comm Wake */
	    u32 diag_b     : 1;     /* 10B to 8B Error */
	    u32 diag_d     : 1;     /* Disparity Error */
	    u32 diag_c     : 1;     /* CRC Error */
	    u32 diag_h     : 1;     /* Handshake Error */
	    u32 diag_s     : 1;     /* Link Sequence Error */
	    u32 diag_t     : 1;     /* Transport State Transition Error */
	    u32 diag_f     : 1;     /* Unknown FIS Type */
	    u32 diag_x     : 1;     /* Exchanged */
	    u32 diag_rsvd  : 5;     /* Reserved */
	};
	
    };
} hba_port_pxserr_t;


/* Port x SATA Notification */
typedef struct __attribute__((packed)) {
    u32 pmn        : 16; /* (RWC) PM Notify */
    u32 rsvd       : 16; /* (RO) Reserved */
} hba_port_pxsntf_t;


/* Port x FIS-based Switching Control */
typedef struct __attribute__((packed)) {
    u32 en         : 1;  /* (RW) Enable */
    u32 dec        : 1;  /* (RW1) Device Error Clear */
    u32 sde        : 1;  /* (RO) Single Device Error */
    u32 rsvd1      : 5;  /* (RO) Reserved */
    u32 dev        : 4;  /* (RW) Device To Issue */
    u32 ado        : 4;  /* (RO) Active Device Optimization */
    u32 dwe        : 4;  /* (RO) Device With Error */
    u32 rsvd2      : 12; /* Reserved */
} hba_port_pxfbs_t;



/** 
 * HBA Memory Register Layout 
 */

typedef struct __attribute__((packed)) {
    volatile hba_host_ctrl_t   host_ctrl_regs;
    volatile u8                rsvd[116]; 
    volatile u8                vendor_regs[96];
    volatile hba_port_ctrl_t   ports[32];
} hba_regs_t;


/**
 * SATA Command Header 
 *   This is a 32 byte structure defining a SATA operation
 *   Each disk has an array of 32 of these headers, linked from the control area
 */
typedef struct __attribute__((packed)) {
    /* DW-0 */
    u32 cfl        : 5;  /* CMD FIS Length */
    u32 atapi      : 1;  /* ATAPI */
    u32 wr         : 1;  /* Write */
    u32 pre        : 1;  /* Prefetchable */
    u32 rst        : 1;  /* Reset */
    u32 bist       : 1;  /* BIST */
    u32 clr        : 1;  /* CLear Busy on R_OK */
    u32 rsvd1      : 1;  /* Reserved */
    u32 pmp        : 4;  /* Port Multiplier Port */
    u32 prdtl      : 16; /* Phys. Region Desc. Table Length */

    /* DW-1 */
    u32 prdbc;           /* Phys. Region Desc. Byte Cound */

    /* DW-2+3 */
    u64 ctba;            /* CMD Table Desc. Base Address */

	
    /* DW-4 ... DW-7 */
    u32 dw4;
    u32 dw5;
    u32 dw6;
    u32 dw7;

} sata_cmd_hdr_t;


/** 
 * PRD Descriptor Table 
 *   This is a DMA descriptor that is included in the command table
 *   Pointed to by the Command Header
 *   There are 0 to 65535 prd entries in the cmd table 
 */
typedef struct __attribute__((packed)) {
    /* DW 0+1 */
    u64 dba;             /* Data Base Address */

    /* DW-2 */
    u32 rsvd1;           /* Reserved */

    /* DW-3 */
    u32 dbc        : 22; /* Data Byte Count */
    u32 rsvd2      : 9;  /* Reserved */
    u32 intr       : 1;  /* Interrupt on Completion */
} sata_prd_tbl_t;


/** 
 * Command Table
 *   Contains the FIS header and PRD tables used for the command
 */
typedef struct __attribute__((packed)) {
    u32 cfis[16];              /* Command FIS <This is the ATA cmd> */
    u32 acmd[4];               /* ATAPI Command */
    u32 rsvd[12];              /* Reserved */
    sata_prd_tbl_t prdts[0];   /* Array of up to 65535 PRD tables */
} sata_cmd_tbl_t;




/** 
 * FIS Structures
 */

#define FIS_TYPE_REG_H2D    0x27
#define FIS_TYPE_REG_D2H    0x34
#define FIS_TYPE_DMA_ACT    0x39
#define FIS_TYPE_DMA_SETUP  0x41
#define FIS_TYPE_DATA       0x46
#define FIS_TYPE_BIST       0x58
#define FIS_TYPE_PIO_SETUP  0x5f
#define FIS_TYPE_DEV_BITS   0xa1


/* Host-to-Device FIS */
typedef struct __attribute__((packed)) {
    /* DWORD 0 */
    u32 type       : 8;  /* FIS_TYPE_REG_H2D */
    u32 pmp        : 4;  /* Port Multiplier */
    u32 rsvd1      : 3;  /* Reserved */
    u32 c          : 1;  /* 1=Cmd, 0=Ctrl */
    u32 cmd        : 8;  /* Command */
    u32 feature_lo : 8;  /* Low 8 bites of Feature Register */
    

    /* DWORD 1 */
    u32 lba        : 24; /* Low 24 bits of LBA */
    u32 dev        : 8;  /* Device Register */
    
    /* DWORD 2 */
    u32 lba_hi     : 24; /* High 24 bits of LBA */
    u32 feature_hi : 8;  /* High 8 bits of Feature Register */

    /* DWORD 3 */
    u32 cnt        : 16; /* Sector Count */
    u32 icc        : 8;  /* Isochronous CMD Completion */
    u32 ctrl       : 8;  /* Control Register */
    
    /* DWORD 4 */
    u32 rsvd2;           /* Reserved */
} fis_reg_h2d_t;


/* Device-to-Host FIS */
typedef struct __attribute__((packed)) {
    /* DWORD 0 */
    u32 type       : 8;  /* FIS_TYPE_REG_D2H */
    u32 pmp        : 4;  /* Port Multiplier */
    u32 rsvd1      : 2;  /* Reserved */
    u32 i          : 1;  /* Interrupt Bit */
    u32 rsvd2      : 1;  /* Reserved */

    u32 status     : 8;  /* Status Register  */
    u32 error      : 8;  /* Error Register  */
    

    /* DWORD 1 */
    u32 lba        : 24; /* Low 24 bits of LBA */
    u32 dev        : 8;  /* Device Register */
    
    /* DWORD 2 */
    u32 lba_hi     : 24; /* High 24 bits of LBA */
    u32 rsvd3      : 8;  /* Reserved */

    /* DWORD 3 */
    u32 cnt        : 16; /* Count */
    u32 rsvd4      : 16; /* Reserved */
   
    /* DWORD 4 */
    u32 rsvd5;           /* Reserved */
} fis_reg_d2h_t;


/* Data FIS - Bidirectional */
typedef struct __attribute__((packed)) {
    /* DWORD 0 */
    u32 type       : 8;  /* FIS_TYPE_DATA */
    u32 pmp        : 4;  /* Port Multiplier */
    u32 rsvd0      : 20; /* Reserved */

    /* DWORD 1-N */
    u32 data[0];         /* Payload */
} fis_data_t;


/* PIO Setup FIS - Device-to-Host */
typedef struct __attribute__((packed)) {
    /* DWORD 0 */
    u32 type       : 8;  /* FIS_TYPE_PIO_SETUP */
    u32 pmp        : 4;  /* Port Multiplier */
    u32 rsvd1      : 1;  /* Reserved */
    u32 d          : 1;  /* Data Direction (1 = d2h) */
    u32 i          : 1;  /* Interrupt Bit */
    u32 rsvd2      : 1;  /* Reserved */

    u32 status     : 8;  /* Status Register  */
    u32 error      : 8;  /* Error Register  */
    

    /* DWORD 1 */
    u32 lba        : 24; /* Low 24 bits of LBA */
    u32 dev        : 8;  /* Device Register */
    
    /* DWORD 2 */
    u32 lba_hi     : 24; /* High 24 bits of LBA */
    u32 rsvd3      : 8;  /* Reserved */

    /* DWORD 3 */
    u32 cnt        : 16; /* Count */
    u32 rsvd4      : 8;  /* Reserved */
    u32 e_sts      : 8;  /* New value of status register */

    /* DWORD 4 */
    u32 tc         : 16; /* Transfer Count */
    u32 rsvd5      : 16; /* Reserved */
} fis_pio_setup_t;


/* DMA Setup FIS - Device-to-Host */
typedef struct __attribute__((packed)) {
    /* DWORD 0 */
    u32 type       : 8;  /* FIS_TYPE_DMA_SETUP */
    u32 pmp        : 4;  /* Port Multiplier */
    u32 rsvd1      : 1;  /* Reserved */
    u32 d          : 1;  /* Data Direction (1 = d2h) */
    u32 i          : 1;  /* Interrupt Bit */
    u32 a          : 1;  /* Auto-activate: (If FIS_TYPE_DMA_ACT is needed) */
    u32 rsvd2      : 16; /* Reserved */
    
    /* DWORD 1+2 */
    u64 dma_buf_id;      /* DMA Buffer ID (??) */
  
    /* DWORD 3 */
    u32 rsvd3;           /* Reserved */

    /* DWORD 4 */
    u32 dma_buf_off;     /* Byte Offset into Buffer (lo 2 bits MBZ) */
    
    /* DWORD 5 */
    u32 xfer_cnt;        /* Bytes to transfer (bit 0 MBZ) */
    
    /* DWORD 6 */
    u32 rsvd4;           /* Reserved */
} fis_dma_setup_t;







/** 
 * Received FIS Structure 
 */
typedef struct __attribute__((packed)) {
    fis_dma_setup_t    dsfis;      /* DMA Setup FIS */
    u32                rsvd1;      /* Reserved */
    fis_pio_setup_t    psfis;      /* PIO Setup FIS */
    u32                rsvd2[3];   /* Reserved */
    fis_reg_d2h_t      rfis;       /* D2H Register FIS */
    u32                rsvd3;      /* Reserved */
    u64                sdbfis;     /* Set Device Bits FIS */
    u32                ufis[16];   /* Unknown FIS */
    u32                rsvd4[24];  /* Reserved */ 
} hba_rx_fis_t;





