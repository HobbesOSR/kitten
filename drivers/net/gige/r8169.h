/** \file
 * R8169 network device constants
 */
#ifndef _r8169_h_
#define _r8169_h_

#define R8169_IRQ	 11
#define R8169_IDTVEC	 64
#define R8169_MTU      1500

#define R8169_IDR0	0x00	/* ID Registers */
#define R8169_IDR1	0x01
#define R8169_IDR2	0x02
#define R8169_IDR3	0x03
#define R8169_IDR4	0x04
#define R8169_IDR5	0x05

#define R8169_MAR7	0x08	/* Mulicast Registers*/
#define R8169_MAR6	0x09
#define R8169_MAR5	0x0a
#define R8169_MAR4	0x0b
#define R8169_MAR3	0x0c
#define R8169_MAR2	0x0d
#define R8169_MAR1	0x0e
#define R8169_MAR0	0x0f

#define R8169_DTCCR	0x10	/* Dump Tally Counter Command Reg */

#define R8169_TNPDS_LO	0x20	/* Tx Normal Priority Descriptors: Start address LOW */
#define R8169_TNPDS_HI	0x24	/* Tx Normal Priority Descriptors: Start address HIGH */
#define R8169_THPDS_LO	0x28	/* Tx High Priority Descriptors: Start address LOW */
#define R8169_THPDS_HI	0x2c	/* Tx High Priority Descriptors: Start address HIGH */

#define R8169_CR	0x37	/* Command Regiseter */
#define R8169_TPPOLL	0x38	/* Transmit Priority Polling Reg */
#define R8169_IMR	0x3c	/* Intrpt Mask Reg */
#define R8169_ISR	0x3e	/* Intrpt Status Reg */
#define R8169_TCR	0x40	/* Tx Config Reg */
#define R8169_RCR	0x44	/* Rx Config Reg */
#define R8169_TCTR	0x48	/* Timer Count Reg */

#define R8169_9346CR	0x50	/* 9346 Command Reg */
#define R8169_CONFIG0	0x51	/* Config Reg */
#define R8169_CONFIG1	0x52
#define R8169_CONFIG2	0x53
#define R8169_CONFIG3	0x54
#define R8169_CONFIG4	0x55
#define R8169_CONFIG5	0x56

#define R8169_TIMINT	0x58	/* Timer Intrpt Reg */
#define R8169_MULINT	0x5c	/* Multiple Intrpt Reg */
#define R8169_PHYAR	0x60	/* PHY Access Reg */
#define R8169_PHYSTAT	0x6c	/* PHY Status Reg */

#define R8169_W0	0x84	/* Power Management wakeup frames */
#define R8169_W1	0x8c
#define R8169_W2LD	0x94
#define R8169_W2HD	0x9c
#define R8169_W3LD	0xa4
#define R8169_W3HD	0xac
#define R8169_W4LD	0xb4
#define R8169_W4HD	0xbc

#define R8169_CRC0	0xc4	/* Power Management CRC Reg for wakeup frame */
#define R8169_CRC1	0xc6
#define R8169_CRC2	0xc8
#define R8169_CRC3	0xca
#define R8169_CRC4	0xcc

#define R8169_RMS	0xda	/* Rx packet Max Size */
#define R8169_CPLUSCR	0xe0	/* C+ Command Reg */
#define R8169_INTRMIT	0xe2	/* Interrupt Mitigate */
#define R8169_RDSAR_LO	0xe4	/* Rx Descriptor Start Address Reg LOW */
#define R8169_RDSAR_HI	0xe8	/* Rx Descriptor Start Address Reg HIGH */
#define R8169_MTPS	0xec	/* Max Tx Packet Size Reg */

/* Interrupts */
#define PKT_RX			0x0001
#define RX_ERR 			0x0002
#define TX_OK 			0x0004
#define TX_ERR 			0x0008
#define RX_RDU	 		0x0010
#define RX_LC			0x0020
#define RX_FOVW			0x0040
#define TX_TDU			0x0080
#define SW_INT			0x0100
#define RX_FEMP			0x0200

#define TCR_IFG_STD		0x03000000
#define TCR_MXDMA_2048		0x00000700
#define RCR_RXFTH_UNLIM		0x0000e000
#define RCR_MXDMA_1024		0x00000600
#define R9346CR_EEM_NORMAL	0x00
#define R9346CR_EEM_CONFIG	0xc0
#define CPLUS_MULRW		0x00000008

#define MII_CTRL		0x0
#define MII_ANA			0x4
#define MII_1000_CTRL		0x9

#define MII_CTRL_ANE		0x1000
#define MII_CTRL_RAN		0x0200
#define MII_CTRL_DM		0x0100
#define MII_CTRL_SP_1000	0x0040
#define MII_ANA_PAUSE_ASYM	0x0800
#define MII_ANA_PAUSE_SYM	0x0400
#define MII_ANA_100TXFD		0x0100
#define MII_ANA_100TXHD		0x0080
#define MII_ANA_10TFD		0x0040
#define MII_ANA_10THD		0x0020
#define MII_1000C_FULL		0x0200
#define MII_1000C_HALF		0x0100

/* The macros below were copied straight from the linux r8169.c driver. */
enum rtl_register_content {
        /* InterruptStatusBits */
        SYSErr          = 0x8000,
        PCSTimeout      = 0x4000,
        SWInt           = 0x0100,
        TxDescUnavail   = 0x0080,
        RxFIFOOver      = 0x0040,
        LinkChg         = 0x0020,
        RxOverflow      = 0x0010,
        TxErr           = 0x0008,
        TxOK            = 0x0004,
        RxErr           = 0x0002,
        RxOK            = 0x0001,

        RxAckBits = RxFIFOOver | RxOverflow | RxOK,

        /* RxStatusDesc */
        RxFOVF  = (1 << 23),
        RxRWT   = (1 << 22),
        RxRES   = (1 << 21),
        RxRUNT  = (1 << 20),
        RxCRC   = (1 << 19),

        /* ChipCmdBits */
        CmdReset        = 0x10,
        CmdRxEnb        = 0x08,
        CmdTxEnb        = 0x04,
	RxBufEmpty	= 0x01,

        /* TXPoll register p.5 */
        HPQ             = 0x80,         /* Poll cmd on the high prio queue */
        NPQ             = 0x40,         /* Poll cmd on the low prio queue */
        FSWInt          = 0x01,         /* Forced software interrupt */

        /* Cfg9346Bits */
        Cfg9346_Lock    = 0x00,
        Cfg9346_Unlock  = 0xc0,

        /* rx_mode_bits */
        AcceptErr       = 0x20,
        AcceptRunt      = 0x10,
        AcceptBroadcast = 0x08,
        AcceptMulticast = 0x04,
        AcceptMyPhys    = 0x02,
        AcceptAllPhys   = 0x01,

        /* RxConfigBits */
        RxCfgFIFOShift  = 13,
        RxCfgDMAShift   =  8,

        /* TxConfigBits */
        TxInterFrameGapShift = 24,
        TxDMAShift = 8, /* DMA burst value (0-7) is shift this many bits */

        /* Config1 register p.24 */
        LEDS1           = (1 << 7),
        LEDS0           = (1 << 6),
        MSIEnable       = (1 << 5),     /* Enable Message Signaled Interrupt */
        Speed_down      = (1 << 4),
        MEMMAP          = (1 << 3),
        IOMAP           = (1 << 2),
        VPD             = (1 << 1),
        PMEnable        = (1 << 0),     /* Power Management Enable */

        /* Config2 register p. 25 */
        PCI_Clock_66MHz = 0x01,
        PCI_Clock_33MHz = 0x00,

        /* Config3 register p.25 */
        MagicPacket     = (1 << 5),     /* Wake up when receives a Magic Packet */
        LinkUp          = (1 << 4),     /* Wake up when the cable connection is re-established */
        Beacon_en       = (1 << 0),     /* 8168 only. Reserved in the 8168b */

        /* Config5 register p.27 */
        BWF             = (1 << 6),     /* Accept Broadcast wakeup frame */
        MWF             = (1 << 5),     /* Accept Multicast wakeup frame */
        UWF             = (1 << 4),     /* Accept Unicast wakeup frame */
        LanWake         = (1 << 1),     /* LanWake enable/disable */
        PMEStatus       = (1 << 0),     /* PME status can be reset by PCI RST# */

        /* TBICSR p.28 */
        TBIReset        = 0x80000000,
        TBILoopback     = 0x40000000,
        TBINwEnable     = 0x20000000,
        TBINwRestart    = 0x10000000,
        TBILinkOk       = 0x02000000,
        TBINwComplete   = 0x01000000,

        /* CPlusCmd p.31 */
        EnableBist      = (1 << 15),    // 8168 8101
        Mac_dbgo_oe     = (1 << 14),    // 8168 8101
        Normal_mode     = (1 << 13),    // unused
        Force_half_dup  = (1 << 12),    // 8168 8101
        Force_rxflow_en = (1 << 11),    // 8168 8101
        Force_txflow_en = (1 << 10),    // 8168 8101
        Cxpl_dbg_sel    = (1 << 9),     // 8168 8101
        ASF             = (1 << 8),     // 8168 8101
        PktCntrDisable  = (1 << 7),     // 8168 8101
        Mac_dbgo_sel    = 0x001c,       // 8168
        RxVlan          = (1 << 6),
        RxChkSum        = (1 << 5),
        PCIDAC          = (1 << 4),
        PCIMulRW        = (1 << 3),
        INTT_0          = 0x0000,       // 8168
        INTT_1          = 0x0001,       // 8168
        INTT_2          = 0x0002,       // 8168
        INTT_3          = 0x0003,       // 8168

        /* rtl8169_PHYstatus */
        TBI_Enable      = 0x80,
        TxFlowCtrl      = 0x40,
        RxFlowCtrl      = 0x20,
        _1000bpsF       = 0x10,
        _100bps         = 0x08,
        _10bps          = 0x04,
        LinkStatus      = 0x02,
        FullDup         = 0x01,

        /* _TBICSRBit */
        TBILinkOK       = 0x02000000,

        /* DumpCounterCommand */
        CounterDump     = 0x8,
};

enum desc_status_bit {
        DescOwn         = (1 << 31), /* Descriptor is owned by NIC */
        RingEnd         = (1 << 30), /* End of descriptor ring */
        FirstFrag       = (1 << 29), /* First segment of a packet */
        LastFrag        = (1 << 28), /* Final segment of a packet */

        /* Tx private */
        LargeSend       = (1 << 27), /* TCP Large Send Offload (TSO) */
        MSSShift        = 16,        /* MSS value position */
        MSSMask         = 0xfff,     /* MSS value + LargeSend bit: 12 bits */
        IPCS            = (1 << 18), /* Calculate IP checksum */
        UDPCS           = (1 << 17), /* Calculate UDP/IP checksum */
        TCPCS           = (1 << 16), /* Calculate TCP/IP checksum */
        TxVlanTag       = (1 << 17), /* Add VLAN tag */

        /* Rx private */
        PID1            = (1 << 18), /* Protocol ID bit 1/2 */
        PID0            = (1 << 17), /* Protocol ID bit 2/2 */

#define RxProtoUDP      (PID1)
#define RxProtoTCP      (PID0)
#define RxProtoIP       (PID1 | PID0)
#define RxProtoMask     RxProtoIP

        IPFail          = (1 << 16), /* IP checksum failed */
        UDPFail         = (1 << 15), /* UDP/IP checksum failed */
        TCPFail         = (1 << 14), /* TCP/IP checksum failed */
        RxVlanTag       = (1 << 16), /* VLAN tag available */
};

#define RsvdMask        0x3fffc000

struct Desc {
        uint32_t opts1;
        uint32_t opts2;
	uint32_t addr_lo;
       	uint32_t addr_hi;
};

#endif // _r8169_h_
