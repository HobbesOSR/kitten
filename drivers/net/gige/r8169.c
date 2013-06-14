/*
 * Adapted from Linux by:
 * Author: Matt Wojcik
 * Author: Peter Kamm
 *
 * Ported to Kitten by:
 * Jack Lange (jarusl@cs.northwestern.edu)
 * redistribute, and modify it as specified in the file "COPYING"
 */


#include <lwk/driver.h>
#include <lwk/netdev.h>
#include <lwk/interrupt.h>
#include <lwk/pci/pci.h>
#include <lwip/netif.h>
#include <lwip/inet.h>
#include <lwip/tcpip.h>
#include <lwip/etharp.h>
#include <arch/page.h>
#include <arch/proto.h>
#include <arch/io.h>

#include "r8169.h"

#define PHYOUT(val, reg) outl(0x80000000 | ((reg) & 0x1F) << 16 | ((val) & 0xFFFF), R8169_PHYAR)
#define IOADDR(dev, reg) (dev->mmio_paddr+reg)

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define RX_BUF_SIZE 1536
#define TX_BUF_SIZE 1536

typedef uint8_t r8169_rx_buf_t[RX_RING_SIZE*RX_BUF_SIZE] __attribute__((aligned(8)));
typedef uint8_t r8169_tx_buf_t[TX_RING_SIZE*TX_BUF_SIZE] __attribute__((aligned(8)));
typedef struct Desc r8169_rx_ring_t[RX_RING_SIZE] __attribute__((aligned(256)));
typedef struct Desc r8169_tx_ring_t[TX_RING_SIZE] __attribute__((aligned(256)));

// Device-specific structure
typedef struct r8169_device_s {
	pci_dev_t *pci_dev;

	uint32_t cur_rx;
	uint32_t cur_tx;

	r8169_rx_buf_t rx_buf;
	r8169_tx_buf_t tx_buf;
	r8169_tx_buf_t tx_hi_buf;

	r8169_rx_ring_t rx_ring;
	r8169_tx_ring_t tx_ring;
	r8169_tx_ring_t tx_hi_ring;

	pci_bar_t mem_bar;
	pci_bar_t io_bar;

	paddr_t	mmio_paddr;
	vaddr_t	mmio_vaddr;
	uint32_t io_base;

	uint8_t mac_addr[6];

	char ip_str[16];
	char nm_str[16];
	char gw_str[16];
} r8169_device_t;

static struct netif r8169_netif;

static r8169_device_t r8169_state;

static const int r8169_debug = 0;

static const uint32_t r8169_intr_mask = 
	SYSErr | TxDescUnavail | RxFIFOOver | LinkChg | RxOverflow | 
	TxErr | TxOK | RxErr | RxOK;

static err_t r8169_tx( struct netif * const netif, struct pbuf * const p ) {
	r8169_device_t *dev = netif->state;

	if( r8169_debug ) {
		printk("Transmitting packet.  cur_tx = %d\n", dev->cur_tx);
	}

	unsigned int tx_entry = dev->cur_tx % TX_RING_SIZE;
	struct Desc *tx_desc = dev->tx_ring + tx_entry;
	int32_t pkt_len = p->tot_len;
	uint64_t paddr = (uint64_t)tx_desc->addr_hi << 32 | tx_desc->addr_lo;
	uint8_t *pkt_data = (uint8_t *)(__va(paddr));

	if( tx_desc->opts1 & DescOwn ) {
		if( r8169_debug ) {
			printk("TX processing finished - SHOULD NOT HAPPEN\n");
		}
		return ERR_OK;
	}


	struct pbuf * q;
	size_t offset = 0;
	for( q = p ; q != NULL ; q = q->next )
	{
		memcpy(pkt_data + offset, q->payload, q->len );
		offset += q->len;
	}

	if( r8169_debug ) {
		printk("Packet TX Size = %d\n", pkt_len);
		int i;
		for (i = 0; i < pkt_len; i++){
			printk(" %x ", *(pkt_data + i));
		}
		printk("\n");
	} 

	tx_desc->opts1 = DescOwn | FirstFrag | LastFrag | pkt_len;
	outb(NPQ, IOADDR(dev, R8169_TPPOLL));

	dev->cur_tx++;

	if( r8169_debug ) {
		printk("Packet transmitted.  cur_tx = %d\n", dev->cur_tx);
	}

	return ERR_OK;
}

static void r8169_rx( struct netif * const netif ) {
	r8169_device_t *dev = netif->state;

	if( r8169_debug ) {
		printk("Packet Received\n");  
	}

	while( (inb(IOADDR(dev, R8169_CR)) & RxBufEmpty) == 0 ) {
		unsigned int rx_entry = dev->cur_rx % RX_RING_SIZE;
		struct Desc *rx_desc = dev->rx_ring + rx_entry;
		int32_t pkt_len = (rx_desc->opts1 & 0x00001fff);
		uint64_t paddr = (uint64_t)rx_desc->addr_hi << 32 | rx_desc->addr_lo;
		uint8_t *pkt_data = (uint8_t *)(__va(paddr));

		if( rx_desc->opts1 & DescOwn ) {
			if( r8169_debug ) {
				printk("RX processing finished\n");
			}
			break;
		}

		if( r8169_debug ) {
			printk("Packet RX Size = %u\n", pkt_len);
			uint16_t i;
			for (i = 0; i < pkt_len; i++){
				printk(" %x ", *((uint8_t *)(pkt_data + i)));
			}
			printk("\n");
		}

		struct pbuf * p = pbuf_alloc( PBUF_RAW, pkt_len, PBUF_POOL );
		if( !p ) {
			printk( KERN_ERR "%s: Unable to allocate pbuf! dropping\n", __func__);
			return;
		}
    
		memcpy(p->payload, pkt_data, pkt_len);
		p->tot_len = pkt_len;
		p->next = 0x0;
   
		rx_desc->opts1 |= DescOwn;
		outw(RxAckBits, IOADDR(dev, R8169_ISR));

		dev->cur_rx++;

		if( netif->input( p, netif ) != ERR_OK ) {
			printk( KERN_ERR "%s: Packet receive failed!\n", __func__);
			pbuf_free( p );
			return;
		}

		if( r8169_debug ) {
			printk("Packet Processed.  cur_rx = %d\n", dev->cur_rx);
		}
	}
}

static void r8169_clear_irq( r8169_device_t *dev, uint32_t interrupts ) {

	outw(interrupts, IOADDR(dev, R8169_ISR));
}


static irqreturn_t r8169_interrupt( int vector, void *priv ) {
	r8169_device_t *dev = &r8169_state; 
	uint16_t status = inw(IOADDR(dev, R8169_ISR));

	if( r8169_debug ) {
//		printk("Interrupt Received: 0x%x\n", status);
		if( status & PKT_RX )  printk("Receive OK\n");
		if( status & RX_ERR )  printk("Receive Error\n");
		if( status & TX_OK )   printk("Transmit OK\n");
		if( status & TX_ERR )  printk("Transmit Error\n");
		if( status & RX_RDU )  printk("Receive Descriptor Unavailable\n");
		if( status & RX_LC )   printk("Link Change\n");
		if( status & RX_FOVW ) printk("Receive FIFO Overflow\n");
		if( status & TX_TDU )  printk("Transmit Descriptor Unavailable\n");
		if( status & SW_INT )  printk("Software Interrupt\n");
		if( status & RX_FEMP ) printk("Receive FIFO Empty\n");
	}

	if( status & PKT_RX ) {
		r8169_clear_irq(dev, status);
		r8169_rx(&r8169_netif );
		if( r8169_debug ) {
			if( inb(IOADDR(dev, R8169_CR)) & RxBufEmpty ) {
				printk("Receive Buffer Empty\n");
			} else {
				printk("Packet present in RX buffer\n");
			}
		}
	}

	if( status & TX_OK ) {
		if( r8169_debug ) {
			printk("Packet successfully transmitted\n");
		}
		r8169_clear_irq(dev, TX_OK);
	}

	if( status & RX_LC ) {
		printk("PHY Status : %02x\n", inb(IOADDR(dev, R8169_PHYSTAT)));
	}

	r8169_clear_irq(dev, status);
	return IRQ_HANDLED;
}

static err_t r8169_hw_init( struct netif * const netif ) {
	r8169_device_t *dev = netif->state;
	uint64_t paddr;

	if( r8169_debug ) {
		printk("Initializing r8169 hardware\n");
	}

	// Figure out where the r8169 registers are mapped into (physical) memory
	pcicfg_bar_decode(&dev->pci_dev->cfg, 0, &dev->mem_bar);
	dev->mmio_paddr = dev->mem_bar.address;
	dev->mmio_vaddr = (vaddr_t) __va(dev->mmio_paddr);

	if( r8169_debug ) {
		printk("R8169 mmio_paddr:   0x%lx\n", dev->mmio_paddr);
		printk("R8169 mmio_vaddr:   0x%lx\n", dev->mmio_vaddr);
	}

	// Figure out where the r8169 I/O ports are located
	pcicfg_bar_decode(&dev->pci_dev->cfg, 4, &dev->io_bar);
	dev->io_base   = dev->io_bar.address;

	dev->cur_tx = 0;
	dev->cur_rx = 0;

	outw(0x0000, IOADDR(dev, R8169_IMR));

	/* Reset the chip */
	outb(CmdReset, IOADDR(dev, R8169_CR));
  
	outw(0xffff, IOADDR(dev, R8169_ISR));

	/* Setup PHY */
	PHYOUT(MII_CTRL_ANE | MII_CTRL_DM | MII_CTRL_SP_1000, MII_CTRL);
	PHYOUT(MII_1000C_FULL | MII_1000C_HALF, MII_1000_CTRL);

	/* Unlock Config[01234] and BMCR register writes */
	outb(R9346CR_EEM_CONFIG, IOADDR(dev, R8169_9346CR));
  
	outw(CPLUS_MULRW, IOADDR(dev, R8169_CPLUSCR));
	outb(0x00, IOADDR(dev, R8169_INTRMIT));

	/* Enable Tx/Rx before setting transfer thresholds */
	outb(CmdRxEnb | CmdTxEnb, IOADDR(dev, R8169_CR));
 
	/* Allocate Rx Buffers */
	for( unsigned int i = 0; i < RX_RING_SIZE; i++ ) {
		paddr = __pa(dev->rx_buf + (i * RX_BUF_SIZE));
		dev->rx_ring[i].addr_lo = paddr & 0xfffffffful;
		dev->rx_ring[i].addr_hi = paddr >> 32;
		dev->rx_ring[i].opts1 = (1 << 31) | ((i==RX_RING_SIZE-1) ? (1 << 30) : 0) | RX_BUF_SIZE; 
		dev->rx_ring[i].opts2 = 0; 
	}

	/* Initialize Rx */
	outw(RX_BUF_SIZE, IOADDR(dev, R8169_RMS));
	outl(RCR_RXFTH_UNLIM | RCR_MXDMA_1024, IOADDR(dev, R8169_RCR));
	paddr = __pa(dev->rx_ring);
	outl(paddr & 0xfffffffful, IOADDR(dev, R8169_RDSAR_LO));
	outl(paddr >> 32, IOADDR(dev, R8169_RDSAR_HI));

	/* Allocate Tx Buffers */
	for( unsigned int i = 0; i < TX_RING_SIZE; i++ ) {
		paddr = __pa(dev->tx_buf + (i * TX_BUF_SIZE));
		dev->tx_ring[i].addr_lo = paddr & 0xfffffffful;
		dev->tx_ring[i].addr_hi = paddr >> 32;
		dev->tx_ring[i].opts1 = ((i==TX_RING_SIZE-1) ? (1 << 30) : 0); 
		dev->tx_ring[i].opts2 = 0; 
	}

	/* Initialize Tx */
	outw((TX_BUF_SIZE>>7)+1, IOADDR(dev, R8169_MTPS));
	outl(TCR_MXDMA_2048 | TCR_IFG_STD, IOADDR(dev, R8169_TCR));
	paddr = __pa(dev->tx_ring);
	outl(paddr & 0xfffffffful, IOADDR(dev, R8169_TNPDS_LO));
	outl(paddr >> 32, IOADDR(dev, R8169_TNPDS_HI));

	/* Allocate Tx Buffers */
	for( unsigned int i = 0; i < TX_RING_SIZE; i++ ) {
		paddr = __pa(dev->tx_hi_buf + (i * TX_BUF_SIZE));
		dev->tx_hi_ring[i].addr_lo = paddr & 0xfffffffful;
		dev->tx_hi_ring[i].addr_hi = paddr >> 32;
		dev->tx_hi_ring[i].opts1 = ((i==TX_RING_SIZE-1) ? (1 << 30) : 0); 
		dev->tx_hi_ring[i].opts2 = 0; 
	}

	/* Initialize Tx */
	outw((TX_BUF_SIZE>>7)+1, IOADDR(dev, R8169_MTPS));
	outl(TCR_MXDMA_2048 | TCR_IFG_STD, IOADDR(dev, R8169_TCR));
	paddr = __pa(dev->tx_hi_ring);
	outl(paddr & 0xfffffffful, IOADDR(dev, R8169_THPDS_LO));
	outl(paddr >> 32, IOADDR(dev, R8169_THPDS_HI));

	/* Lock Config[01234] and BMCR register writes */
	outb(R9346CR_EEM_CONFIG, IOADDR(dev, R8169_9346CR));
 
	/* missed packet counter */
	outl(0, IOADDR(dev, R8169_TCTR));
  
	// r8169_set_rx_mode does some stuff here.
	outl(inl(IOADDR(dev, R8169_RCR)) | AcceptBroadcast | AcceptMulticast | AcceptMyPhys | AcceptAllPhys, 
	     IOADDR(dev, R8169_RCR));
  
	// Set Multicast address to all 1's
	for( unsigned int i = 0; i < 8; i++ ) {
		outb(0xff, IOADDR(dev, R8169_MAR7) + i);
	}
  
	/* Read in the MAC address */
	dev->mac_addr[0] = inb(IOADDR(dev, R8169_IDR0));
	dev->mac_addr[1] = inb(IOADDR(dev, R8169_IDR1));
	dev->mac_addr[2] = inb(IOADDR(dev, R8169_IDR2));
	dev->mac_addr[3] = inb(IOADDR(dev, R8169_IDR3));
	dev->mac_addr[4] = inb(IOADDR(dev, R8169_IDR4));
	dev->mac_addr[5] = inb(IOADDR(dev, R8169_IDR5));
  
	if( r8169_debug ) {
		printk("R8169 MAC Addr (%.2x:%.2x:%.2x:%.2x:%.2x:%.2x)\n", 
		       dev->mac_addr[0],
		       dev->mac_addr[1],
		       dev->mac_addr[2],
		       dev->mac_addr[3],
		       dev->mac_addr[4],
		       dev->mac_addr[5]);
	}

	/* no early-rx interrupts */
	outw(inw(IOADDR(dev, R8169_MULINT)) & 0xf000, IOADDR(dev, R8169_MULINT));

	/* make sure RxTx has started */
	if( !(inb(IOADDR(dev, R8169_CR)) & CmdRxEnb) || !(inb(IOADDR(dev, R8169_CR)) & CmdTxEnb) )
		outb(CmdRxEnb | CmdTxEnb, IOADDR(dev, R8169_CR));
	
	irq_request(R8169_IDTVEC, &r8169_interrupt, 0, "r8169", NULL);

	/* Enable all known interrupts by setting the interrupt mask. */
	outw(r8169_intr_mask, IOADDR(dev, R8169_IMR));

	netif->mtu		= R8169_MTU;
	netif->flags		= 0
		| NETIF_FLAG_LINK_UP
		| NETIF_FLAG_UP
		| NETIF_FLAG_ETHARP
		;

	netif->hwaddr_len	= 6;
	netif->hwaddr[0]	= dev->mac_addr[0];
	netif->hwaddr[1]	= dev->mac_addr[1];
	netif->hwaddr[2]	= dev->mac_addr[2];
	netif->hwaddr[3]	= dev->mac_addr[3];
	netif->hwaddr[4]	= dev->mac_addr[4];
	netif->hwaddr[5]	= dev->mac_addr[5];
  
	netif->name[0]		= 'e';
	netif->name[1]		= 'n';
  
	netif->linkoutput     = r8169_tx;

	netif->output		= etharp_output;
  
	if( r8169_debug ) {
		printk("r8169 initialized\n");
	}

	return ERR_OK;
}


int r8169_init( void ) {
	r8169_device_t *dev = &r8169_state;

	struct ip_addr ip = {inet_addr(dev->ip_str)};
	struct ip_addr nm = {inet_addr(dev->nm_str)};
	struct ip_addr gw = {inet_addr(dev->gw_str)};

	pci_dev_t *pci_dev;
	if ((pci_dev = pci_lookup_device(0x10ec, 0x8168)) == NULL) {
		printk("r8169 device not found\n");
		return -1;
	}

	// Remember our PCI info
	dev->pci_dev = pci_dev;

	printk("R8169 IP address:   %s\n", dev->ip_str);
	printk("R8169 Netmask:      %s\n", dev->nm_str);
	printk("R8169 Gateway:      %s\n", dev->gw_str);

	// Remember our PCI info
  	netif_add(&r8169_netif, &ip, &nm, &gw, dev, r8169_hw_init, tcpip_input);

	return 0;
}


DRIVER_INIT("net", r8169_init);

DRIVER_PARAM_STRING(ip, r8169_state.ip_str, sizeof(r8169_state.ip_str));
DRIVER_PARAM_STRING(nm, r8169_state.nm_str, sizeof(r8169_state.nm_str));
DRIVER_PARAM_STRING(gw, r8169_state.gw_str, sizeof(r8169_state.gw_str));
