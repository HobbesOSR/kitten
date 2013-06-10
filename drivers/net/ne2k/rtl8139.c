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

#include "rtl8139.h"

#define IOADDR(dev, reg) (dev->mmio_paddr+reg)

#define RX_BUF_SIZE (32 * 1024)
#define TX_BUF_SIZE 1536
#define TX_FIFO_THRESH 256

typedef uint8_t rtl8139_rx_buf_t[RX_BUF_SIZE + 1500];
typedef uint8_t rtl8139_tx_buf_t[4][TX_BUF_SIZE] __attribute__((aligned(4)));

// Device-specific structure
typedef struct rtl8139_device_s {
	pci_dev_t *pci_dev;

	uint32_t pkt_cntr;

	uint32_t cur_rx;
	uint32_t cur_tx;

	rtl8139_rx_buf_t rx_buf;
	rtl8139_tx_buf_t tx_buf;

	pci_bar_t mem_bar;
	pci_bar_t io_bar;

	paddr_t	mmio_paddr;
	vaddr_t	mmio_vaddr;
	uint32_t io_base;

	uint8_t mac_addr[6];

	char ip_str[16];
	char nm_str[16];
	char gw_str[16];
} rtl8139_device_t;

static struct netif rtl8139_netif;

static rtl8139_device_t rtl8139_state;

static const int rtl8139_debug = 1;

static uint32_t tx_flag = (TX_FIFO_THRESH << 11) & 0x003f0000;

static const uint32_t rtl8139_intr_mask = 
	PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | 
	TxErr | TxOK | RxErr | RxOK;


static err_t rtl8139_tx(struct netif * const netif, struct pbuf * const p) {
	rtl8139_device_t *dev = netif->state;
	uint32_t entry = dev->cur_tx % 4;
	uint32_t size = p->tot_len;
	uint8_t * msg_buf = dev->tx_buf[entry];
	
	
	if (rtl8139_debug) {
		printk("Transmitting packet.  cur_tx = %d\n", dev->cur_tx);
	}


	struct pbuf * q;
	size_t offset = 0;
	for( q = p ; q != NULL ; q = q->next )
		{
			memcpy( msg_buf + offset, q->payload, q->len );
			offset += q->len;
		}

	if (rtl8139_debug) {
		printk("Packet TX Size = %d\n", size);
		int i;
		for (i = 0; i < size; i++){
			printk(" %x ", *((uint8_t *)(msg_buf + i)));
		}
		printk("\n");
	} 

	outl(tx_flag | size, IOADDR(dev, RTL8139_TSD0) + (entry * 4));

	dev->cur_tx++;

	if (rtl8139_debug) {
		printk("Packet transmitted.  cur_tx = %d\n", dev->cur_tx);
	}

	return ERR_OK;
}



static __inline__ void wrap_copy(uint8_t * dst, const unsigned char * ring,
				 u32 offset, unsigned int size)
{
	memcpy(dst, ring + offset, size);
}

static void rtl8139_rx(struct netif * const netif) {
	rtl8139_device_t *dev = netif->state;

	if (rtl8139_debug) {
		printk("Packet Received\n");  
	}

	while( (inb(IOADDR(dev, RTL8139_CR)) & RxBufEmpty) == 0){
		uint16_t ring_offset = dev->cur_rx % RX_BUF_SIZE;
		uint8_t * buf_ptr = (dev->rx_buf + ring_offset);

		// Parse the packet header written by the device
		// The packet header is 4 bytes long... 
		uint16_t rx_status = *(uint16_t *)buf_ptr;
		uint16_t rx_size =  *(uint16_t *)(buf_ptr + 2);

		uint16_t pkt_len = rx_size - 4;
		uint8_t * pkt_data = buf_ptr + 4;
		
		if (rtl8139_debug) {
			printk("RX Status = %x\n", rx_status);
		}



		if (!(rx_status & 0x1)) {
			// Receive not OK...
			// Should handle error
			printk("Error in packet RX size=%x\n", rx_size);
			while (1);
		}

		if (rtl8139_debug) {
			printk("Packet RX Size = %u\n", pkt_len);
			uint16_t i;
			for (i = 0; i < pkt_len; i++){
				printk(" %x ", *((uint8_t *)(pkt_data + i)));
			}
			printk("\n");
		}


		// Read packet....
		// Get a pbuf using our external payload
		// We tell the lwip library that we can use up to the entire
		// size of the skb so that it can resize for reuse.
		// \todo This is broken?
		//struct pbuf * p = pbuf_alloc( PBUF_RAW, len, PBUF_REF );
    
		// Allocate a pool-allocated buffer and copy into it
		struct pbuf * p = pbuf_alloc( PBUF_RAW, pkt_len, PBUF_POOL );
		if( !p )
			{
				printk( KERN_ERR
					"%s: Unable to allocate pbuf! dropping\n",
					__func__
					);
				return;
			}
    
		//p->payload = data;
		//p->tot_len = len;
    
		// Copy the memory into the pbuf payload
		struct pbuf * q = p;
		uint32_t offset = 0;
		for( q = p ; q != NULL ; q = q->next )
			{
				// This is ugly...
				wrap_copy( q->payload, dev->rx_buf, (ring_offset + 4 + offset) , q->len );
				offset += q->len;
			}
    
		dev->cur_rx = ((dev->cur_rx + rx_size + 4 + 3) % RX_BUF_SIZE) & ~3;
		outw((uint16_t)(dev->cur_rx - 16), IOADDR(dev, RTL8139_CAPR));
    
		dev->pkt_cntr++;

		if (rtl8139_debug) {
			printk("cur_rx moved to %u\n", dev->cur_rx);
			printk("Packet counter at %d\n", dev->pkt_cntr);
		}

		// clear ISR
		outw(RxAckBits, IOADDR(dev, RTL8139_ISR));

		// Receive the packet
		// We have to figure out if its arp or not... 

		if( netif->input( p, netif ) != ERR_OK )
			{
				printk( KERN_ERR
					"%s: Packet receive failed!\n",
					__func__
					);
				pbuf_free( p );
				return;
			}




		if (rtl8139_debug) {
			printk("Packet Processed\n");
		}
	}
}

static void rtl8139_clear_irq( rtl8139_device_t *dev, uint32_t interrupts ) {
	outw(interrupts, IOADDR(dev, RTL8139_ISR));
}


static irqreturn_t
rtl8139_interrupt(
	int			vector,
	void *			priv
)
{
	rtl8139_device_t *dev = &rtl8139_state; 
	uint16_t status = inw(IOADDR(dev, RTL8139_ISR));
  
	if (rtl8139_debug) {
		printk("Interrupt Received: 0x%x\n", status);
	}

	if( status & PKT_RX ) {
		rtl8139_clear_irq(dev, status);
		rtl8139_rx(&rtl8139_netif );
		
		if (rtl8139_debug) {
			if (inb(IOADDR(dev, RTL8139_CR)) & RxBufEmpty) {
				printk("Receive Buffer Empty\n");
			} else {
				printk("Packet present in RX buffer\n");
			}
		}
	}

	if (status & TX_OK) {
		if (rtl8139_debug) {
			printk("Packet successfully transmitted\n");
		}
		rtl8139_clear_irq(dev, TX_OK);
	}
	

	return IRQ_HANDLED;
}

static err_t rtl8139_hw_init(struct netif * const netif) {
	rtl8139_device_t *dev = netif->state;
  
	if (rtl8139_debug) {
		printk("Initializing rtl8139 hardware\n");
	}

	// Figure out where the rtl8139 registers are mapped into (physical) memory
	pcicfg_bar_decode(&dev->pci_dev->cfg, 0, &dev->mem_bar);
	dev->mmio_paddr = dev->mem_bar.address;
	dev->mmio_vaddr = (vaddr_t) __va(dev->mmio_paddr);

	if( rtl8139_debug ) {
		printk("R8139 mmio_paddr:   0x%lx\n", dev->mmio_paddr);
		printk("R8139 mmio_vaddr:   0x%lx\n", dev->mmio_vaddr);
	}

	// Figure out where the rtl8139 I/O ports are located
	pcicfg_bar_decode(&dev->pci_dev->cfg, 4, &dev->io_bar);
	dev->io_base   = dev->io_bar.address;

	dev->pkt_cntr = 0;
	dev->cur_tx = 0;
	dev->cur_rx = 0;
  
	/* Reset the chip */
	outb(CmdReset, IOADDR(dev, RTL8139_CR));
  
	/* Unlock Config[01234] and BMCR register writes */
	outb(Cfg9346_Unlock, IOADDR(dev, RTL8139_9346CR));
  
	/* Enable Tx/Rx before setting transfer thresholds */
	outb(CmdRxEnb | CmdTxEnb, IOADDR(dev, RTL8139_CR));
  
	/* Using 32K ring */
	outl(RxCfgRcv32K | RxNoWrap | (7 << RxCfgFIFOShift) | (7 << RxCfgDMAShift)
	     | AcceptBroadcast | AcceptMyPhys, 
	     IOADDR(dev, RTL8139_RCR));
  
	outl(TxIFG96 | (6 << TxDMAShift) | (8 << TxRetryShift), IOADDR(dev, RTL8139_TCR));
  
	/* Lock Config[01234] and BMCR register writes */
	outb(Cfg9346_Lock, IOADDR(dev, RTL8139_9346CR));
  
	/* init Rx ring buffer DMA address */
	outl((uint32_t)__pa(dev->rx_buf), IOADDR(dev, RTL8139_RBSTART));
  
	/* init Tx buffer DMA addresses (4 registers) */
	{
		int i;
		for(i = 0; i < 4; i++) {
			outl((uint32_t)__pa(dev->tx_buf[i]), IOADDR(dev, RTL8139_TSAD0) + (i * 4));
		}
	}
  
  
	/* missed packet counter */
	outl(0, IOADDR(dev, RTL8139_MPC));
  
	// rtl8139_set_rx_mode does some stuff here.
	outl(inl(IOADDR(dev, RTL8139_RCR)) | AcceptBroadcast | AcceptMulticast | AcceptMyPhys | AcceptAllPhys, 
	     IOADDR(dev, RTL8139_RCR));
  
	// Set Multicast address to all 1's
	{
		int i;
		for(i = 0; i < 8; i++) {
			outb(0xff, IOADDR(dev, RTL8139_MAR0) + i);
		}
	}
  
	/* Read in the MAC address */
	dev->mac_addr[0] = inb(IOADDR(dev, RTL8139_IDR0));
	dev->mac_addr[1] = inb(IOADDR(dev, RTL8139_IDR1));
	dev->mac_addr[2] = inb(IOADDR(dev, RTL8139_IDR2));
	dev->mac_addr[3] = inb(IOADDR(dev, RTL8139_IDR3));
	dev->mac_addr[4] = inb(IOADDR(dev, RTL8139_IDR4));
	dev->mac_addr[5] = inb(IOADDR(dev, RTL8139_IDR5));
  
	if (rtl8139_debug) {
		printk("RTL8139 MAC Addr=(%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", 
		       dev->mac_addr[0],
		       dev->mac_addr[1],
		       dev->mac_addr[2],
		       dev->mac_addr[3],
		       dev->mac_addr[4],
		       dev->mac_addr[5]);
	}

	/* no early-rx interrupts */
	outw(inw(IOADDR(dev, RTL8139_MULINT)) & MultiIntrClear, IOADDR(dev, RTL8139_MULINT));
  
	/* make sure RxTx has started */
	if(!(inb(IOADDR(dev, RTL8139_CR)) & CmdRxEnb) || !(inb(IOADDR(dev, RTL8139_CR)) & CmdTxEnb))
		outb(CmdRxEnb | CmdTxEnb, IOADDR(dev, RTL8139_CR));
	
	irq_request( RTL8139_IDTVEC, &rtl8139_interrupt, 0,  "rtl8139", NULL );

	/* Enable all known interrupts by setting the interrupt mask. */
	outw(rtl8139_intr_mask, IOADDR(dev, RTL8139_IMR));
  
	netif->mtu		= RTL8139_MTU;
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
  
	netif->name[0]		= 'n';
	netif->name[1]		= 'e';
  
	netif->linkoutput     = rtl8139_tx;

	netif->output		= etharp_output;
  
	if (rtl8139_debug) {
		printk("8139 initialized\n");
	}

	return ERR_OK;
}


int rtl8139_init( void ) {
	rtl8139_device_t *dev = &rtl8139_state;

	struct ip_addr ip = {inet_addr(dev->ip_str)};
	struct ip_addr nm = {inet_addr(dev->nm_str)};
	struct ip_addr gw = {inet_addr(dev->gw_str)};

	pci_dev_t *pci_dev;
	if ((pci_dev = pci_lookup_device(0x10ec, 0x8168)) == NULL) {
		printk("rtl8139 device not found\n");
		return -1;
	}

	// Remember our PCI info
	dev->pci_dev = pci_dev;

	printk("R8139 IP address:   %s\n", dev->ip_str);
	printk("R8139 Netmask:      %s\n", dev->nm_str);
	printk("R8139 Gateway:      %s\n", dev->gw_str);

	// Remember our PCI info
  	netif_add(&rtl8139_netif, &ip, &nm, &gw, dev, rtl8139_hw_init, tcpip_input);

	return 0;
}


DRIVER_INIT("net", rtl8139_init);

DRIVER_PARAM_STRING(ip, rtl8139_state.ip_str, sizeof(rtl8139_state.ip_str));
DRIVER_PARAM_STRING(nm, rtl8139_state.nm_str, sizeof(rtl8139_state.nm_str));
DRIVER_PARAM_STRING(gw, rtl8139_state.gw_str, sizeof(rtl8139_state.gw_str));
