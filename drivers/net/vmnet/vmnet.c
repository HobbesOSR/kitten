/* Paravirtualized network driver for the Palacios VMM
 * Jack Lange, 2009
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


// These are the hypercall numbers (must match with Palacios)
#define TX_HYPERCALL      0x300
#define RX_HYPERCALL      0x301
#define MACADDR_HYPERCALL 0x302

#define VMNET_IRQ       11
#define VMNET_IDTVEC    59
#define VMNET_MTU      1500

#define RX_BUF_SIZE 1536
#define TX_BUF_SIZE 1536

uint8_t rx_buf[RX_BUF_SIZE];
uint8_t tx_buf[TX_BUF_SIZE];

uint32_t tx_ctr = 0;
uint32_t rx_ctr = 0;

uint8_t mac_addr[6];

static const int vmnet_debug = 1;

static struct netif vmnet_netif;


#define VMMCALL ".byte 0x0f,0x01,0xd9\r\n"

static int get_mac(uintptr_t address) {
    uint32_t call_num = MACADDR_HYPERCALL;
    int ret = 0;

    asm volatile (
            VMMCALL
            : "=a"(ret)
            : "a"(call_num),"b"(address)
            );

    return ret;
}


static int tx_pkt(uintptr_t address, uint32_t pkt_len) {
    uint32_t call_num = TX_HYPERCALL;
    int ret = 0;

    asm volatile (
            VMMCALL
            : "=a"(ret)
            : "a"(call_num),"b"(address),"c"(pkt_len)
            );

    return ret;
}


static int rx_pkt(uintptr_t address, uint32_t * pkt_len) {
    int ret = 0;
    uint32_t call_num = RX_HYPERCALL;

    asm volatile (
            VMMCALL
            : "=a"(ret),"=c"(*pkt_len)
            : "a"(call_num),"b"(address)
            );


    return ret;
}


static err_t vmnet_tx(struct netif * const netif, struct pbuf * const p) {
    uint32_t size = p->tot_len;

    if (vmnet_debug) {
       printk("Transmitting packet.  cur_tx = %d\n", tx_ctr);
    }

    struct pbuf * q;
    size_t offset = 0;
    for ( q = p ; q != NULL ; q = q->next )
       {
           memcpy( tx_buf + offset, q->payload, q->len );
           offset += q->len;
       }


    if (vmnet_debug) {
       printk("Packet TX Size = %d\n", size);
       int i;
       for (i = 0; i < size; i++){
           printk(" %x ", *((uint8_t *)(tx_buf + i)));
       }
       printk("\n");
    }


    if (tx_pkt(TX_HYPERCALL, __pa(tx_buf)) == -1) {
       printk("Could not transmit packet\n");
       return ERR_IF;
    }

    if (vmnet_debug) {
       printk("Packet transmitted.  cur_tx = %d\n", tx_ctr);
    }

    tx_ctr++;

    return ERR_OK;
}




static void vmnet_rx(struct netif * const netif) {
    uint32_t pkt_len = 0;


    if (vmnet_debug) {
       printk("Packet Received\n");
    }

    // VMM RX  packet
    if (rx_pkt(__pa(rx_buf), &pkt_len) == -1) {
       printk("Could not receive packet\n");
       return;
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
       memcpy( q->payload, rx_buf + offset, q->len );
       offset += q->len;
      }


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


    rx_ctr++;


    printk("Packet Processed\n");
}

static irqreturn_t
vmnet_interrupt(
       unsigned int            vector,
       void *                  priv
)
{

    if (vmnet_debug) {
       printk("Interrupt Received\n");
    }

    vmnet_rx(&vmnet_netif );

    return IRQ_HANDLED;
}


static void vmnet_hw_init( void ) {

   get_mac(__pa(mac_addr));

    if (vmnet_debug) {
       printk("VMNET MAC Addr=(%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
              mac_addr[0],
              mac_addr[1],
              mac_addr[2],
              mac_addr[3],
              mac_addr[4],
              mac_addr[5]);
    }


    if (vmnet_debug) {
       printk("VMNET initialized\n");
    }
}


static err_t vmnet_net_init(struct netif * const netif) {
  printk( "%s: Initializing %p (%p)\n", __func__, netif, &vmnet_netif );

  netif->mtu           = VMNET_MTU;
  netif->flags         = 0
    | NETIF_FLAG_LINK_UP
    | NETIF_FLAG_UP
    ;


  netif->hwaddr_len    = 6;
  netif->hwaddr[0]     = mac_addr[0];
  netif->hwaddr[1]     = mac_addr[1];
  netif->hwaddr[2]     = mac_addr[2];
  netif->hwaddr[3]     = mac_addr[3];
  netif->hwaddr[4]     = mac_addr[4];
  netif->hwaddr[5]     = mac_addr[5];

  netif->name[0]               = 'v';
  netif->name[1]               = 'm';

  netif->linkoutput     = vmnet_tx;

  netif->output                = etharp_output;

  return ERR_OK;
}



void vmnet_init( void ) {
    vmnet_hw_init();

    irq_request(
          VMNET_IDTVEC,
          &vmnet_interrupt,
          0,
          "vmnet",
          NULL
          );



  //  struct ip_addr ipaddr = { htonl(0 | 192 << 24 | 168 << 16 | 1 << 8 | 2 << 0) };
  //struct ip_addr ipaddr = { htonl(0 | 127 << 24 | 1 << 0) };
  struct ip_addr ipaddr = { htonl(0 | 172 << 24 | 20 << 16 | 5 << 0) };
  struct ip_addr netmask = { htonl(0xffffff00) };
  struct ip_addr gw = { htonl(0x00000000) };


  netif_add( &vmnet_netif,
            &ipaddr,
            &netmask,
            &gw,
            0,
            vmnet_net_init,
            ethernet_input
            );
}


driver_init("net", vmnet_init);
