/** \file
 * RTL8139 network device constants
 */
#ifndef _rtl8139_h_
#define _rtl8139_h_

#define RTL8139_IRQ	 11
#define RTL8139_IDTVEC	 59
#define RTL8139_MTU      1500

#define RTL8139_BASE_ADDR 	0xc100		/* Starting address of the card (a guess right now)*/

#define RTL8139_IDR0	RTL8139_BASE_ADDR		/* ID Registers */
#define RTL8139_IDR1	(RTL8139_BASE_ADDR + 0x01)
#define RTL8139_IDR2	(RTL8139_BASE_ADDR + 0x02)
#define RTL8139_IDR3	(RTL8139_BASE_ADDR + 0x03)
#define RTL8139_IDR4	(RTL8139_BASE_ADDR + 0x04)
#define RTL8139_IDR5	(RTL8139_BASE_ADDR + 0x05)

#define RTL8139_MAR0	(RTL8139_BASE_ADDR + 0x08)	/* Mulicast Registers*/
#define RTL8139_MAR1	(RTL8139_BASE_ADDR + 0x09)
#define RTL8139_MAR2	(RTL8139_BASE_ADDR + 0x0a)
#define RTL8139_MAR3	(RTL8139_BASE_ADDR + 0x0b)
#define RTL8139_MAR4	(RTL8139_BASE_ADDR + 0x0c)
#define RTL8139_MAR5	(RTL8139_BASE_ADDR + 0x0d)
#define RTL8139_MAR6	(RTL8139_BASE_ADDR + 0x0e)
#define RTL8139_MAR7	(RTL8139_BASE_ADDR + 0x0f)

#define RTL8139_TSD0	(RTL8139_BASE_ADDR + 0x10)	/* Tx Status of Descriptors */
#define RTL8139_TSD1	(RTL8139_BASE_ADDR + 0x14)
#define RTL8139_TSD2	(RTL8139_BASE_ADDR + 0x18)
#define RTL8139_TSD3	(RTL8139_BASE_ADDR + 0x1c)

#define RTL8139_TSAD0	(RTL8139_BASE_ADDR + 0x20)	/* Tx Start Address of Descriptors */
#define RTL8139_TSAD1	(RTL8139_BASE_ADDR + 0x24)
#define RTL8139_TSAD2	(RTL8139_BASE_ADDR + 0x28)
#define RTL8139_TSAD3	(RTL8139_BASE_ADDR + 0x2c)

#define RTL8139_RBSTART	(RTL8139_BASE_ADDR + 0x30)	/* Rx Buffer Start Address */
#define RTL8139_ERBCR	(RTL8139_BASE_ADDR + 0x34)	/* Early Rx Byte Count Register */
#define RTL8139_ERSR	(RTL8139_BASE_ADDR + 0x36)	/* Early Rx Status Register */
#define RTL8139_CR	(RTL8139_BASE_ADDR + 0x37)	/* Command Register */
#define RTL8139_CAPR	(RTL8139_BASE_ADDR + 0x38)	/* Current Address of Pkt Read */
#define RTL8139_CBR	(RTL8139_BASE_ADDR + 0x3a)	/* Current Buffer Address */
#define RTL8139_IMR	(RTL8139_BASE_ADDR + 0x3c)	/* Intrpt Mask Reg */
#define RTL8139_ISR	(RTL8139_BASE_ADDR + 0x3e)	/* Intrpt Status Reg */
#define RTL8139_TCR	(RTL8139_BASE_ADDR + 0x40)	/* Tx Config Reg */
#define RTL8139_RCR	(RTL8139_BASE_ADDR + 0x44)	/* Rx Config Reg */
#define RTL8139_TCTR	(RTL8139_BASE_ADDR + 0x48)	/* Timer Count Reg */
#define RTL8139_MPC	(RTL8139_BASE_ADDR + 0x4c)	/* Missed Pkt Counter */
#define RTL8139_9346CR	(RTL8139_BASE_ADDR + 0x50)	/* 9346 Command Reg */
#define RTL8139_CONFIG0	(RTL8139_BASE_ADDR + 0x51)	/* Config Reg */
#define RTL8139_CONFIG1	(RTL8139_BASE_ADDR + 0x52)
#define RTL8139_TimerInt	(RTL8139_BASE_ADDR + 0x54)	/* Timer Intrpt Reg */
#define RTL8139_MSR	(RTL8139_BASE_ADDR + 0x58)	/* Media Status Reg */
#define RTL8139_CONFIG3	(RTL8139_BASE_ADDR + 0x59)	
#define RTL8139_CONFIG4	(RTL8139_BASE_ADDR + 0x5a)
#define RTL8139_MULINT	(RTL8139_BASE_ADDR + 0x5c)	/* Multiple Intrpt Select */
#define RTL8139_RERID	(RTL8139_BASE_ADDR + 0x5e)	
#define RTL8139_TSAD	(RTL8139_BASE_ADDR + 0x60)	/* Tx Status of All Descriptors */
#define RTL8139_BMCR	(RTL8139_BASE_ADDR + 0x62)	/* Basic Mode Control Register */
#define RTL8139_BMSR	(RTL8139_BASE_ADDR + 0x64)	/* Basic Mode Status Register */
#define RTL8139_ANAR	(RTL8139_BASE_ADDR + 0x66)	/* Auto-Negotiation Advertisement Register */
#define RTL8139_ANLPAR	(RTL8139_BASE_ADDR + 0x68)	/* Auto-Negotiation Link Partner Register */
#define RTL8139_ANER	(RTL8139_BASE_ADDR + 0x6a)	/* Auto-Negotiation Expansion Register */
#define RTL8139_DIS	(RTL8139_BASE_ADDR + 0x6c)	/* Disconnect Counter */
#define RTL8139_FCSC	(RTL8139_BASE_ADDR + 0x6e)	/* False Carrier Sense Counter */
#define RTL8139_NWAYTR	(RTL8139_BASE_ADDR + 0x70)	/* N-way Test Register */
#define RTL8139_REC	(RTL8139_BASE_ADDR + 0x72)	/* RX ER Counter */
#define RTL8139_CSCR	(RTL8139_BASE_ADDR + 0x74)	/* CS Config Register */
#define RTL8139_PHY1_PARM	(RTL8139_BASE_ADDR + 0x78)	/* PHY parameter */
#define RTL8139_TW_PARM	(RTL8139_BASE_ADDR + 0x7c)	/* Twister parameter */
#define RTL8139_PHY2_PARM	(RTL8139_BASE_ADDR + 0x80)

#define RTL8139_CRC0	(RTL8139_BASE_ADDR + 0x84)	/* Power Management CRC Reg for wakeup frame */
#define RTL8139_CRC1	(RTL8139_BASE_ADDR + 0x85)
#define RTL8139_CRC2	(RTL8139_BASE_ADDR + 0x86)
#define RTL8139_CRC3	(RTL8139_BASE_ADDR + 0x87)
#define RTL8139_CRC4	(RTL8139_BASE_ADDR + 0x88)
#define RTL8139_CRC5	(RTL8139_BASE_ADDR + 0x89)
#define RTL8139_CRC6	(RTL8139_BASE_ADDR + 0x8a)
#define RTL8139_CRC7	(RTL8139_BASE_ADDR + 0x8b)

#define RTL8139_Wakeup0	(RTL8139_BASE_ADDR + 0x8c)	/* Power Management wakeup frame */
#define RTL8139_Wakeup1	(RTL8139_BASE_ADDR + 0x94)
#define RTL8139_Wakeup2	(RTL8139_BASE_ADDR + 0x9c)
#define RTL8139_Wakeup3	(RTL8139_BASE_ADDR + 0xa4)
#define RTL8139_Wakeup4	(RTL8139_BASE_ADDR + 0xac)
#define RTL8139_Wakeup5	(RTL8139_BASE_ADDR + 0xb4)
#define RTL8139_Wakeup6	(RTL8139_BASE_ADDR + 0xbc)
#define RTL8139_Wakeup7	(RTL8139_BASE_ADDR + 0xc4)

#define RTL8139_LSBCRO0	(RTL8139_BASE_ADDR + 0xcc)	/* LSB of the mask byte of wakeup frame */
#define RTL8139_LSBCRO1	(RTL8139_BASE_ADDR + 0xcd)
#define RTL8139_LSBCRO2	(RTL8139_BASE_ADDR + 0xce)
#define RTL8139_LSBCRO3	(RTL8139_BASE_ADDR + 0xcf)
#define RTL8139_LSBCRO4	(RTL8139_BASE_ADDR + 0xd0)
#define RTL8139_LSBCRO5	(RTL8139_BASE_ADDR + 0xd1)
#define RTL8139_LSBCRO6	(RTL8139_BASE_ADDR + 0xd2)
#define RTL8139_LSBCRO7	(RTL8139_BASE_ADDR + 0xd3)

#define RTL8139_Config5	(RTL8139_BASE_ADDR + 0xd8)

/* Interrupts */
#define PKT_RX		0x0001
#define RX_ERR 		0x0002
#define TX_OK 		0x0004
#define TX_ERR 		0x0008
#define RX_BUFF_OF 	0x0010
#define RX_UNDERRUN	0x0020
#define RX_FIFO_OF	0x0040
#define CABLE_LEN_CHNG	0x2000
#define TIME_OUT	0x4000
#define SERR		0x8000



/* The macros below were copied straight from the linux 8139too.c driver. */
enum ClearBitMasks {
	MultiIntrClear = 0xF000,
	ChipCmdClear = 0xE2,
	Config1Clear = (1<<7)|(1<<6)|(1<<3)|(1<<2)|(1<<1),
};

enum ChipCmdBits {
	CmdReset = 0x10,
	CmdRxEnb = 0x08,
	CmdTxEnb = 0x04,
	RxBufEmpty = 0x01,
};

/* Interrupt register bits */
enum IntrStatusBits {
	PCIErr = 0x8000,
	PCSTimeout = 0x4000,
	RxFIFOOver = 0x40,
	RxUnderrun = 0x20,
	RxOverflow = 0x10,
	TxErr = 0x08,
	TxOK = 0x04,
	RxErr = 0x02,
	RxOK = 0x01,

	RxAckBits = RxFIFOOver | RxOverflow | RxOK,
};

enum TxStatusBits {
	TxHostOwns = 0x2000,
	TxUnderrun = 0x4000,
	TxStatOK = 0x8000,
	TxOutOfWindow = 0x20000000,
	TxAborted = 0x40000000,
	TxCarrierLost = 0x80000000,
};
enum RxStatusBits {
	RxMulticast = 0x8000,
	RxPhysical = 0x4000,
	RxBroadcast = 0x2000,
	RxBadSymbol = 0x0020,
	RxRunt = 0x0010,
	RxTooLong = 0x0008,
	RxCRCErr = 0x0004,
	RxBadAlign = 0x0002,
	RxStatusOK = 0x0001,
};

/* Bits in RxConfig. */
enum rx_mode_bits {
	AcceptErr = 0x20,
	AcceptRunt = 0x10,
	AcceptBroadcast = 0x08,
	AcceptMulticast = 0x04,
	AcceptMyPhys = 0x02,
	AcceptAllPhys = 0x01,
};

/* Bits in TxConfig. */
enum tx_config_bits {

        /* Interframe Gap Time. Only TxIFG96 doesn't violate IEEE 802.3 */
        TxIFGShift = 24,
        TxIFG84 = (0 << TxIFGShift),    /* 8.4us / 840ns (10 / 100Mbps) */
        TxIFG88 = (1 << TxIFGShift),    /* 8.8us / 880ns (10 / 100Mbps) */
        TxIFG92 = (2 << TxIFGShift),    /* 9.2us / 920ns (10 / 100Mbps) */
        TxIFG96 = (3 << TxIFGShift),    /* 9.6us / 960ns (10 / 100Mbps) */

	TxLoopBack = (1 << 18) | (1 << 17), /* enable loopback test mode */
	TxCRC = (1 << 16),	/* DISABLE appending CRC to end of Tx packets */
	TxClearAbt = (1 << 0),	/* Clear abort (WO) */
	TxDMAShift = 8,		/* DMA burst value (0-7) is shifted this many bits */
	TxRetryShift = 4,	/* TXRR value (0-15) is shifted this many bits */

	TxVersionMask = 0x7C800000, /* mask out version bits 30-26, 23 */
};

/* Bits in Config1 */
enum Config1Bits {
	Cfg1_PM_Enable = 0x01,
	Cfg1_VPD_Enable = 0x02,
	Cfg1_PIO = 0x04,
	Cfg1_MMIO = 0x08,
	LWAKE = 0x10,		/* not on 8139, 8139A */
	Cfg1_Driver_Load = 0x20,
	Cfg1_LED0 = 0x40,
	Cfg1_LED1 = 0x80,
	SLEEP = (1 << 1),	/* only on 8139, 8139A */
	PWRDN = (1 << 0),	/* only on 8139, 8139A */
};

/* Bits in Config3 */
enum Config3Bits {
	Cfg3_FBtBEn    = (1 << 0), /* 1 = Fast Back to Back */
	Cfg3_FuncRegEn = (1 << 1), /* 1 = enable CardBus Function registers */
	Cfg3_CLKRUN_En = (1 << 2), /* 1 = enable CLKRUN */
	Cfg3_CardB_En  = (1 << 3), /* 1 = enable CardBus registers */
	Cfg3_LinkUp    = (1 << 4), /* 1 = wake up on link up */
	Cfg3_Magic     = (1 << 5), /* 1 = wake up on Magic Packet (tm) */
	Cfg3_PARM_En   = (1 << 6), /* 0 = software can set twister parameters */
	Cfg3_GNTSel    = (1 << 7), /* 1 = delay 1 clock from PCI GNT signal */
};

/* Bits in Config4 */
enum Config4Bits {
	LWPTN = (1 << 2),	/* not on 8139, 8139A */
};

/* Bits in Config5 */
enum Config5Bits {
	Cfg5_PME_STS     = (1 << 0), /* 1 = PCI reset resets PME_Status */
	Cfg5_LANWake     = (1 << 1), /* 1 = enable LANWake signal */
	Cfg5_LDPS        = (1 << 2), /* 0 = save power when link is down */
	Cfg5_FIFOAddrPtr = (1 << 3), /* Realtek internal SRAM testing */
	Cfg5_UWF         = (1 << 4), /* 1 = accept unicast wakeup frame */
	Cfg5_MWF         = (1 << 5), /* 1 = accept multicast wakeup frame */
	Cfg5_BWF         = (1 << 6), /* 1 = accept broadcast wakeup frame */
};

enum RxConfigBits {
	/* rx fifo threshold */
	RxCfgFIFOShift = 13,
	RxCfgFIFONone = (7 << RxCfgFIFOShift),

	/* Max DMA burst */
	RxCfgDMAShift = 8,
	RxCfgDMAUnlimited = (7 << RxCfgDMAShift),

	/* rx ring buffer length */
	RxCfgRcv8K = 0,
	RxCfgRcv16K = (1 << 11),
	RxCfgRcv32K = (1 << 12),
	RxCfgRcv64K = (1 << 11) | (1 << 12),

	/* Disable packet wrap at end of Rx buffer. (not possible with 64k) */
	RxNoWrap = (1 << 7),
};

/* Twister tuning parameters from RealTek.
   Completely undocumented, but required to tune bad links on some boards. */
enum CSCRBits {
	CSCR_LinkOKBit = 0x0400,
	CSCR_LinkChangeBit = 0x0800,
	CSCR_LinkStatusBits = 0x0f000,
	CSCR_LinkDownOffCmd = 0x003c0,
	CSCR_LinkDownCmd = 0x0f3c0,
};

enum Cfg9346Bits {
	Cfg9346_Lock = 0x00,
	Cfg9346_Unlock = 0xC0,
};



#endif // _rtl8139_h_
