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
#include <lwip/netif.h>
#include <lwip/inet.h>
#include <lwip/ip.h>
#include <lwip/etharp.h>
#include <arch/page.h>
#include <arch/proto.h>
#include <arch/io.h>

#include "rtl8139.h"


#define RX_BUF_SIZE (32768 + 16 + 2048)
#define TX_BUF_SIZE 1536
#define TX_FIFO_THRESH 256

uint8_t rx_buf[RX_BUF_SIZE];
uint8_t tx_buf[4][TX_BUF_SIZE] __attribute__((aligned(4)));

uint32_t cur_tx;
uint32_t cur_rx;
uint32_t tx_flag = (TX_FIFO_THRESH << 11) & 0x003f0000;
uint32_t pkt_cntr = 0;


uint8_t mac_addr[6];
static char ipaddr[16];
static char netmask[16];
static char gateway[16];

static const int rtl8139_debug = 1;

static struct netif rtl8139_netif;


static const uint32_t rtl8139_intr_mask = 
	PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | 
	TxErr | TxOK | RxErr | RxOK;





static err_t rtl8139_tx(struct netif * const netif, struct pbuf * const p) {
  uint32_t entry = cur_tx % 4;
  uint32_t size = p->tot_len;
  uint8_t * msg_buf = tx_buf[entry];

  if (rtl8139_debug) {
    printk("Transmitting packet.  cur_tx = %d\n", cur_tx);
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


  //memcpy(tx_buf + (entry * TX_BUF_SIZE), packet, size);

  outl(tx_flag | size, RTL8139_TSD0 + (entry * 4));

  cur_tx++;

  if (rtl8139_debug) {
    printk("Packet transmitted.  cur_tx = %d\n", cur_tx);
  }

  return ERR_OK;
}



static __inline__ void wrap_copy(uint8_t * dst, const unsigned char * ring,
				 u32 offset, unsigned int size)
{
  u32 left = RX_BUF_SIZE - offset;

  if (size > left) {
    memcpy(dst, ring + offset, left);
    memcpy(dst + left, ring, size - left);
  } else
    memcpy(dst, ring + offset, size);
}

static void rtl8139_rx(struct netif * const netif) {


  if (rtl8139_debug) {
    printk("Packet Received\n");  
  }

  while( (inb(RTL8139_CR) & RxBufEmpty) == 0){
    uint32_t ring_offset = cur_rx % RX_BUF_SIZE;
    uint8_t * rx_status = (rx_buf + ring_offset);

    if (rtl8139_debug) {
      printk("RX Status = %x\n", *(uint32_t *)rx_status);
    }

    // Parse the packet header written by the device
    // The packet header is 4 bytes long... 
    uint32_t rx_size = (uint32_t)((((uint32_t)*(rx_status + 3)) << 8) | (uint32_t)(*(rx_status + 2)));
    uint32_t pkt_len = rx_size - 4;
    uint8_t * pkt_buf = rx_status + 4;

    if (rtl8139_debug) {
      printk("Packet RX Size = %d\n", pkt_len);
      int i;
      for (i = 0; i < pkt_len; i++){
	printk(" %x ", *((uint8_t *)(pkt_buf + i)));
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
	wrap_copy( q->payload, rx_buf, (ring_offset + 4 + offset) % RX_BUF_SIZE, q->len );
	offset += q->len;
      }
    
    cur_rx = ((cur_rx + rx_size + 4 + 3) % RX_BUF_SIZE) & ~3;
    outw((uint16_t)(cur_rx - 16), RTL8139_CAPR);
    
    pkt_cntr++;

    if (rtl8139_debug) {
      printk("Packet counter at %d\n", pkt_cntr);
    }

    // clear ISR
    outw(RxAckBits, RTL8139_ISR);

    // Receive the packet
    // We have to figure out if its arp or not... 

    printk("netif input=%p\n", netif->input);

    if( netif->input( p, netif ) != ERR_OK )
      {
	printk( KERN_ERR
		"%s: Packet receive failed!\n",
		__func__
		);
	pbuf_free( p );
	return;
      }




    printk("Packet Processed\n");
  }
}

static void rtl8139_clear_irq(uint32_t interrupts) {
  outw(interrupts, RTL8139_ISR);
}


static irqreturn_t
rtl8139_interrupt(
	unsigned int		vector,
	void *			priv
)
{
  uint32_t status = inw(RTL8139_ISR);
  
  if (rtl8139_debug) {
    printk("Interrupt Received: 0x%x\n", status);
  }

  if( status & PKT_RX ) {
    rtl8139_rx(&rtl8139_netif );
    rtl8139_clear_irq(status);
  } else if (status == TX_OK) {
    printk("Packet successfully transmitted\n");
    rtl8139_clear_irq(TX_OK);
  }

  if (inb(RTL8139_CR) & RxBufEmpty) {
    printk("Receive Buffer Empty\n");
  } else {
    printk("Packet present in RX buffer\n");
  }

  return IRQ_HANDLED;
}


static void rtl8139_hw_init( void ) {
  if (rtl8139_debug) {
    printk("Initializing rtl8139 hardware\n");
  }

  cur_tx = 0;
  cur_rx = 0;
  
  /* Reset the chip */
  outb(CmdReset, RTL8139_CR);
  
  //while( inb(RTL8139_CR) != 0 )
  /*udelay(10)*/;
  
  /* Unlock Config[01234] and BMCR register writes */
  outb(Cfg9346_Unlock, RTL8139_9346CR);
  
  /* Enable Tx/Rx before setting transfer thresholds */
  outb(CmdRxEnb | CmdTxEnb, RTL8139_CR);
  
  /* Using 32K ring */
  outl(RxCfgRcv32K | RxNoWrap | (7 << RxCfgFIFOShift) | (7 << RxCfgDMAShift)
       | AcceptBroadcast | AcceptMyPhys, 
       RTL8139_RCR);
  
  //	outdw(RTL8139_TCR, RxCfgRcv32K | RxNoWrap | (7 << RxCfgFIFOShift) | (7 << RxCfgDMAShift));
  
  outl(TxIFG96 | (6 << TxDMAShift) | (8 << TxRetryShift), RTL8139_TCR);
  
  /* Lock Config[01234] and BMCR register writes */
  outb(Cfg9346_Lock, RTL8139_9346CR);
  
  /* init Rx ring buffer DMA address */
  outl((uint32_t)__pa(rx_buf), RTL8139_RBSTART);
  
  /* init Tx buffer DMA addresses (4 registers) */
  {
    int i;
    for(i = 0; i < 4; i++) {
      outl((uint32_t)__pa(tx_buf[i]), RTL8139_TSAD0 + (i * 4));
    }
  }
  
  
  /* missed packet counter */
  outl(0, RTL8139_MPC);
  
  // rtl8139_set_rx_mode does some stuff here.
  outl(inl(RTL8139_RCR) | AcceptBroadcast | AcceptMulticast | AcceptMyPhys | AcceptAllPhys, 
       RTL8139_RCR);
  
  // Set Multicast address to all 1's
  {
    int i;
    for(i = 0; i < 8; i++) {
      outb(0xff, RTL8139_MAR0 + i);
    }
  }
  
  /* Read in the MAC address */
  mac_addr[0] = inb(RTL8139_IDR0);
  mac_addr[1] = inb(RTL8139_IDR1);
  mac_addr[2] = inb(RTL8139_IDR2);
  mac_addr[3] = inb(RTL8139_IDR3);
  mac_addr[4] = inb(RTL8139_IDR4);
  mac_addr[5] = inb(RTL8139_IDR5);

  if (rtl8139_debug) {
    printk("RTL8139 MAC Addr=(%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", 
	   mac_addr[0],
	   mac_addr[1],
	   mac_addr[2],
	   mac_addr[3],
	   mac_addr[4],
	   mac_addr[5]);
  }

  /* no early-rx interrupts */
  outw(inw(RTL8139_MULINT) & MultiIntrClear, RTL8139_MULINT);
  
  /* make sure RxTx has started */
  if(!(inb(RTL8139_CR) & CmdRxEnb) || !(inb(RTL8139_CR) & CmdTxEnb))
    outb(CmdRxEnb | CmdTxEnb, RTL8139_CR);
  
  /* Enable all known interrupts by setting the interrupt mask. */
  outw(rtl8139_intr_mask, RTL8139_IMR);
  

  if (rtl8139_debug) {
    printk("8139 initialized\n");
  }
}


static err_t rtl8139_net_init(struct netif * const netif) {
  printk( "%s: Initializing %p (%p)\n", __func__, netif, &rtl8139_netif );
  
  netif->mtu		= RTL8139_MTU;
  netif->flags		= 0
    | NETIF_FLAG_LINK_UP
    | NETIF_FLAG_UP
    ;
  

  netif->hwaddr_len	= 6;
  netif->hwaddr[0]	= mac_addr[0];
  netif->hwaddr[1]	= mac_addr[1];
  netif->hwaddr[2]	= mac_addr[2];
  netif->hwaddr[3]	= mac_addr[3];
  netif->hwaddr[4]	= mac_addr[4];
  netif->hwaddr[5]	= mac_addr[5];
  
  netif->name[0]		= 'n';
  netif->name[1]		= 'e';
  
  netif->linkoutput     = rtl8139_tx;

  netif->output		= etharp_output;
  
  return ERR_OK;
}


void rtl8139_init( void ) {
	rtl8139_hw_init();
	
	irq_request(
		    RTL8139_IDTVEC,
		    &rtl8139_interrupt,
		    0,
		    "rtl8139",
		    NULL
		    );

	struct ip_addr ip = {inet_addr(ipaddr)};
	struct ip_addr nm = {inet_addr(netmask)};
	struct ip_addr gw = {inet_addr(gateway)};
  
  	netif_add( &rtl8139_netif, 
		   &ip, 
		   &nm, 
		   &gw, 
		   0, 
		   rtl8139_net_init, 
		   ethernet_input
 		   );

	printk("RTL8139 Configured\n");
	printk("\tIP: %x\n", ip.addr);
	printk("\tNETMASK: %x\n", nm.addr);
	printk("\tGATEWAY: %x\n", gw.addr);


}


driver_init("net", rtl8139_init);

driver_param_string(ipaddr, ipaddr, sizeof(ipaddr));
driver_param_string(netmask, netmask, sizeof(netmask));
driver_param_string(gateway, gateway, sizeof(gateway));
