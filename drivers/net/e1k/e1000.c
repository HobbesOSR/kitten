#include <lwk/driver.h>
#include <lwk/netdev.h>
#include <lwk/interrupt.h>
#include <lwip/netif.h>
#include <lwip/inet.h>
#include <lwip/tcpip.h>
#include <lwip/etharp.h>
#include <arch/page.h>
#include <arch/proto.h>
#include <arch/io.h>

#include "e1000.h"

//volatile uint32_t *e1000; // MMIO address to access E1000 BAR

static uint8_t mac_addr[6];
static char ipaddr[16];
static char netmask[16];
static char gateway[16];

static const int e1000_debug = 1;

static struct netif e1000_netif;

struct tx_desc tx_desc_array[E1000_TXDESC] __attribute__ ((aligned (16)));
struct tx_pkt tx_pkt_bufs[E1000_TXDESC];

struct rcv_desc rcv_desc_array[E1000_RCVDESC] __attribute__ ((aligned (16)));
struct rcv_pkt rcv_pkt_bufs[E1000_RCVDESC];

static int
e1000_hw_init(void)
{
        uint32_t i;

        e1000 = (uint32_t *) E1000_MMIOADDR;

        // Initialize tx buffer array
        memset(tx_desc_array, 0x0, sizeof(struct tx_desc) * E1000_TXDESC);
        memset(tx_pkt_bufs, 0x0, sizeof(struct tx_pkt) * E1000_TXDESC);
        for (i = 0; i < E1000_TXDESC; i++) {
                tx_desc_array[i].addr = __pa(tx_pkt_bufs[i].buf);
                tx_desc_array[i].status |= E1000_TXD_STAT_DD;
        }

        // Initialize rcv desc buffer array
        memset(rcv_desc_array, 0x0, sizeof(struct rcv_desc) * E1000_RCVDESC);
        memset(rcv_pkt_bufs, 0x0, sizeof(struct rcv_pkt) * E1000_RCVDESC);
        for (i = 0; i < E1000_RCVDESC; i++) {
                rcv_desc_array[i].addr = __pa(rcv_pkt_bufs[i].buf);
        }

        /* Transmit initialization */
        // Program the Transmit Descriptor Base Address Registers
        e1000[E1000_TDBAL] = __pa(tx_desc_array);
        e1000[E1000_TDBAH] = 0x0;

        // Set the Transmit Descriptor Length Register
        e1000[E1000_TDLEN] = sizeof(struct tx_desc) * E1000_TXDESC;

        // Set the Transmit Descriptor Head and Tail Registers
        e1000[E1000_TDH] = 0x0;
        e1000[E1000_TDT] = 0x0;

        // Initialize the Transmit Control Register 
        e1000[E1000_TCTL] |= E1000_TCTL_EN;
        e1000[E1000_TCTL] |= E1000_TCTL_PSP;
        e1000[E1000_TCTL] &= ~E1000_TCTL_CT;
        e1000[E1000_TCTL] |= (0x10) << 4;
        e1000[E1000_TCTL] &= ~E1000_TCTL_COLD;
        e1000[E1000_TCTL] |= (0x40) << 12;

        // Program the Transmit IPG Register
        e1000[E1000_TIPG] = 0x0;
        e1000[E1000_TIPG] |= (0x6) << 20; // IPGR2 
        e1000[E1000_TIPG] |= (0x4) << 10; // IPGR1
        e1000[E1000_TIPG] |= 0xA; // IPGR

        /* Receive Initialization */
        // Program the Receive Address Registers
        e1000[E1000_EERD] = 0x0;
        e1000[E1000_EERD] |= E1000_EERD_START;
        while (!(e1000[E1000_EERD] & E1000_EERD_DONE));
        e1000[E1000_RAL] = e1000[E1000_EERD] >> 16;

        e1000[E1000_EERD] = 0x1 << 8;
        e1000[E1000_EERD] |= E1000_EERD_START;
        while (!(e1000[E1000_EERD] & E1000_EERD_DONE));
        e1000[E1000_RAL] |= e1000[E1000_EERD] & 0xffff0000;

        e1000[E1000_EERD] = 0x2 << 8;
        e1000[E1000_EERD] |= E1000_EERD_START;
        while (!(e1000[E1000_EERD] & E1000_EERD_DONE));
        e1000[E1000_RAH] = e1000[E1000_EERD] >> 16;

        e1000[E1000_RAH] |= 0x1 << 31;

        // Program the Receive Descriptor Base Address Registers
        e1000[E1000_RDBAL] = __pa(rcv_desc_array);
        e1000[E1000_RDBAH] = 0x0;

        // Set the Receive Descriptor Length Register
        e1000[E1000_RDLEN] = sizeof(struct rcv_desc) * E1000_RCVDESC;

        // Set the Receive Descriptor Head and Tail Registers
        e1000[E1000_RDH] = 0x0;
        e1000[E1000_RDT] = 0x0;

        // Initialize the Receive Control Register
        e1000[E1000_RCTL] |= E1000_RCTL_EN;
        e1000[E1000_RCTL] &= ~E1000_RCTL_LPE;
        e1000[E1000_RCTL] &= ~E1000_RCTL_LBM;
        e1000[E1000_RCTL] &= ~E1000_RCTL_RDMTS;
        e1000[E1000_RCTL] &= ~E1000_RCTL_MO;
        e1000[E1000_RCTL] |= E1000_RCTL_BAM;
        e1000[E1000_RCTL] &= ~E1000_RCTL_SZ; // 2048 byte size
        e1000[E1000_RCTL] |= E1000_RCTL_SECRC;

        return 0;
}

static int
e1000_tx(char *data, int len)
{
        if (len > TX_PKT_SIZE) { // pkt too long
                return -1;
        }

        uint32_t tdt = e1000[E1000_TDT];

        // Check if next tx desc is free
        if (tx_desc_array[tdt].status & E1000_TXD_STAT_DD) {
                memmove(tx_pkt_bufs[tdt].buf, data, len);
                tx_desc_array[tdt].length = len;

                tx_desc_array[tdt].status &= ~E1000_TXD_STAT_DD;
                tx_desc_array[tdt].cmd |= E1000_TXD_CMD_RS;
                tx_desc_array[tdt].cmd |= E1000_TXD_CMD_EOP;

                e1000[E1000_TDT] = (tdt + 1) % E1000_TXDESC;
        }
        else { // tx queue is full!
                return -2;
        }
        
        return 0;
}

static int
e1000_rx(char *data)
{
        uint32_t rdt, len;
        rdt = e1000[E1000_RDT];
        
        if (rcv_desc_array[rdt].status & E1000_RXD_STAT_DD) {
                if (!(rcv_desc_array[rdt].status & E1000_RXD_STAT_EOP)) {
                        panic("Don't allow jumbo frames!\n");
                }
                len = rcv_desc_array[rdt].length;
                memmove(data, rcv_pkt_bufs[rdt].buf, len);
                rcv_desc_array[rdt].status &= ~E1000_RXD_STAT_DD;
                rcv_desc_array[rdt].status &= ~E1000_RXD_STAT_EOP;
                e1000[E1000_RDT] = (rdt + 1) % E1000_RCVDESC;

                return len;
        }

        return -1; // receive empty
}

static void e1000_clear_irq( uint32_t interrupts ) {

//	outw(interrupts, E1000_ISR);
}


static irqreturn_t e1000_interrupt( int vector, void *priv ) {

//	uint16_t status = inw(E1000_ISR);

	if( e1000_debug ) {
		printk("Interrupt Received\n");
//		if( status & PKT_RX )  printk("Receive OK\n");
//		if( status & RX_ERR )  printk("Receive Error\n");
//		if( status & TX_OK )   printk("Transmit OK\n");
//		if( status & TX_ERR )  printk("Transmit Error\n");
//		if( status & RX_RDU )  printk("Receive Descriptor Unavailable\n");
//		if( status & RX_LC )   printk("Link Change\n");
//		if( status & RX_FOVW ) printk("Receive FIFO Overflow\n");
//		if( status & TX_TDU )  printk("Transmit Descriptor Unavailable\n");
//		if( status & SW_INT )  printk("Software Interrupt\n");
//		if( status & RX_FEMP ) printk("Receive FIFO Empty\n");
	}

//	if( status & PKT_RX ) {
//		e1000_clear_irq(status);
//		e1000_rx(&e1000_netif );
//		if( e1000_debug ) {
//			if( inb(E1000_CR) & RxBufEmpty ) {
//				printk("Receive Buffer Empty\n");
//			} else {
//				printk("Packet present in RX buffer\n");
//			}
//		}
//		printk("Finished e1000_rx\n");	
//	} else if( status == TX_OK ) {
//		if( e1000_debug ) {
//			printk("Packet successfully transmitted\n");
//		}
//		e1000_clear_irq(TX_OK);
//	}

//	e1000_clear_irq(status);
	return IRQ_HANDLED;
}

static err_t e1000_net_init( struct netif * const netif ) {

        printk( "%s: Initializing %p (%p)\n", __func__, netif, &e1000_netif );

        netif->mtu              = E1000_MTU;
        netif->flags            = 0
                | NETIF_FLAG_LINK_UP
                | NETIF_FLAG_UP
                | NETIF_FLAG_ETHARP
                ;


        netif->hwaddr_len       = 6;
        netif->hwaddr[0]        = mac_addr[0];
        netif->hwaddr[1]        = mac_addr[1];
        netif->hwaddr[2]        = mac_addr[2];
        netif->hwaddr[3]        = mac_addr[3];
        netif->hwaddr[4]        = mac_addr[4];
        netif->hwaddr[5]        = mac_addr[5];

        netif->name[0]          = 'e';
        netif->name[1]          = 'p';

        netif->linkoutput     = e1000_tx;

        netif->output           = etharp_output;

        return ERR_OK;
}

int e1000_init( void ) {
        e1000_hw_init();

        irq_request(E1000_IDTVEC, &e1000_interrupt, 0, "e1000", NULL);

        struct ip_addr ip = {inet_addr(ipaddr)};
        struct ip_addr nm = {inet_addr(netmask)};
        struct ip_addr gw = {inet_addr(gateway)};

        netif_add(&e1000_netif, &ip, &nm, &gw, 0, e1000_net_init, tcpip_input);

        printk("E1000 Configured\n");
        printk("\tIP: %x\n", ip.addr);
        printk("\tNETMASK: %x\n", nm.addr);
        printk("\tGATEWAY: %x\n", gw.addr);

        return 0;
}

DRIVER_INIT("net", e1000_init);

DRIVER_PARAM_STRING(ipaddr, ipaddr, sizeof(ipaddr));
DRIVER_PARAM_STRING(netmask, netmask, sizeof(netmask));
DRIVER_PARAM_STRING(gateway, gateway, sizeof(gateway));
