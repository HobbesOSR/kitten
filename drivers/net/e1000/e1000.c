/*
 * Copyright (c) 2011 Joshua Cornutt <jcornutt@randomaccessit.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <lwk/driver.h>
#include <lwk/netdev.h>
#include <lwk/interrupt.h>
#include <lwk/delay.h>
#include <lwk/pci/pci.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/etharp.h>
#include <lwip/inet.h>
#include <arch/page.h>
#include <arch/proto.h>
#include <arch/io.h>
#include <arch/fixmap.h>
#include <arch/apicdef.h>
#include <arch/io_apic.h>


#define NUM_RX_DESCRIPTORS	16
#define NUM_TX_DESCRIPTORS	16


// Macros that read/write e1000 memory mapped registers
#define mmio_read32(offset)         *((volatile uint32_t *)(dev->mmio_vaddr + (offset)))
#define mmio_write32(offset, value) *((volatile uint32_t *)(dev->mmio_vaddr + (offset))) = (value)
//#define mmio_write32(offset, value) e1000_write_reg_io(dev, offset, value)


// E1000 Ethernet Controller Register Offsets
#define E1000_REG_CTRL     0x00000	// Device Control
#define E1000_REG_STATUS   0x00008	// Device Status
#define E1000_REG_EECD     0x00010	// EEPROM/Flash Control/Data
#define E1000_REG_EERD     0x00014	// EEPROM Read
#define E1000_REG_FLA      0x0001C	// Flash Access
#define E1000_REG_CTRL_EXT 0x00018	// Extended Device Control
#define E1000_REG_MDIC     0x00020	// MDI Control
#define E1000_REG_RCTL     0x00100	// Receive Control
#define E1000_REG_RDBAL    0x02800	// Receive Descriptor Base Low
#define E1000_REG_RDBAH    0x02804	// Receive Descriptor Base High
#define E1000_REG_RDLEN    0x02808	// Receive Descriptor Length
#define E1000_REG_RDH      0x02810	// Receive Descriptor Head
#define E1000_REG_RDT      0x02818	// Receive Descriptor Tail
#define E1000_REG_TCTL     0x00400	// Transmit Control
#define E1000_REG_TDBAL    0x03800	// Transmit Descriptor Base Low
#define E1000_REG_TDBAH    0x03804	// Transmit Descriptor Base High
#define E1000_REG_TDLEN    0x03808	// Transmit Descriptor Length
#define E1000_REG_TDH      0x03810	// Transmit Descriptor Head
#define E1000_REG_TDT      0x03818	// Transmit Descriptor Tail
#define E1000_REG_MTA      0x05200	// Multicast Table Array (n)
#define E1000_REG_ICR      0x000C0	// Interrupt Cause Read
#define E1000_REG_IMS      0x000D0	// Interrupt Mask Set/Read
#define E1000_REG_ICS      0x000C8	// Interrupt Cause Set
#define E1000_REG_RAL      0x05400	// Receive Address Low
#define E1000_REG_RAH      0x05404	// Receive Address High
#define E1000_REG_TXCW     0x00178	// Transmit Configuration Word
#define E1000_REG_PBA      0x01000	// Packet Buffer Allocation


// E1000 PHY register offsets
#define E1000_PHYREG_PCTRL       0	// PHY Control Register
#define E1000_PHYREG_PSTATUS     1	// PHY Status Register
#define E1000_PHYREG_ANA         4	// Auto-Negotiation Advertisement Register
#define E1000_PHYREG_ANE         6	// Auto-Negotiation Expansion Register
#define E1000_PHYREG_GCON        9	// 1000BASE-T Control Register 
#define E1000_PHYREG_GSTATUS    10	// 1000BASE-T Status Register 
#define E1000_PHYREG_EPSTATUS   15	// Extended PHY Status Register
#define E1000_PHYREG_PSSTAT     17	// PHY Specific Status Register


// E1000 Control Register bit offsets
#define CTRL_FD			(1 << 0)
#define CTRL_ASDE		(1 << 5)
#define CTRL_SLU		(1 << 6)


// Macro that busy polls for 1 us before going on
#define wait_microsecond(count) do { udelay(count); } while (0)


// E1000 Receive (RX) descriptor structure
typedef struct __attribute__((packed)) e1000_rx_desc_s 
{
	volatile uint64_t	address;
	
	volatile uint16_t	length;
	volatile uint16_t	checksum;
	volatile uint8_t	status;
	volatile uint8_t	errors;
	volatile uint16_t	special;
} e1000_rx_desc_t;


// E1000 Transmit (TX) descriptor structure
typedef struct __attribute__((packed)) e1000_tx_desc_s 
{
	volatile uint64_t	address;
	
	volatile uint16_t	length;
	volatile uint8_t	cso;
	volatile uint8_t	cmd;
	volatile uint8_t	sta;
	volatile uint8_t	css;
	volatile uint16_t	special;
} e1000_tx_desc_t;


// Device-specific structure
typedef struct e1000_device_s
{
	pci_dev_t *			pci_dev;

	char				ip_str[16];
	char				nm_str[16];
	char				gw_str[16];

	ip_addr_t			ip;
	ip_addr_t			nm;
	ip_addr_t			gw;

	pci_bar_t			mem_bar;
	pci_bar_t			io_bar;

	paddr_t				mmio_paddr;
	vaddr_t				mmio_vaddr;
	uint32_t			io_base;
	
	volatile uint8_t		*rx_desc_base;
	volatile e1000_rx_desc_t	*rx_desc[NUM_RX_DESCRIPTORS];	// receive descriptor buffer
	volatile uint16_t		rx_tail;
	
	volatile uint8_t		*tx_desc_base;
	volatile e1000_tx_desc_t	*tx_desc[NUM_TX_DESCRIPTORS];	// transmit descriptor buffer
	volatile uint16_t		tx_tail;
	
} e1000_device_t;


// Lightweight IP network interface structure for E1000 driver.
// Only one E1000 instance is supported, so this is a global.
static struct netif e1000_netif;


// E1000 specific state info.
// E1000_netif->state points to this.
// Only one E1000 instance is supported, so this is a global.
static e1000_device_t e1000_state;


// Reads an E1000 register using I/O space accesses
static uint32_t
e1000_read_reg_io(e1000_device_t *dev, uint32_t offset)
{
	uint64_t io_addr_port = dev->io_base;
	uint64_t io_data_port = dev->io_base + 4;

	outl(offset, io_addr_port);
	return inl(io_data_port);
}


// Writes an E1000 register using I/O space accesses
static void
e1000_write_reg_io(e1000_device_t *dev, uint32_t offset, uint32_t value)
{
	uint64_t io_addr_port = dev->io_base;
	uint64_t io_data_port = dev->io_base + 4;

	outl(offset, io_addr_port);
	outl(value,  io_data_port);
}


// Reads a value from the E1000's EEPROM memory
static uint16_t
e1000_eeprom_read( e1000_device_t *dev, uint8_t ADDR)
{
	uint16_t DATA = 0;
	uint32_t tmp = 0;
	mmio_write32( E1000_REG_EERD, (1) | ((uint32_t)(ADDR) << 8) );
	
	while( !((tmp = mmio_read32(E1000_REG_EERD)) & (1 << 4)) )
		wait_microsecond(1);
		
	DATA = (uint16_t)((tmp >> 16) & 0xFFFF);
	return DATA;
}


/****************************************************
 ** Management Data Input/Output (MDI/O) Interface **
 ** "This interface allows upper-layer devices to  **
 **  monitor and control the state of the PHY."    **
 ****************************************************/
#define MDIC_PHYADD			(1 << 21)
#define MDIC_OP_WRITE			(1 << 26)
#define MDIC_OP_READ			(2 << 26)
#define MDIC_R				(1 << 28)
#define MDIC_I				(1 << 29)
#define MDIC_E				(1 << 30)


static uint16_t e1000_phy_read(e1000_device_t *dev, int MDIC_REGADD)
{
	uint16_t MDIC_DATA = 0;

	mmio_write32( E1000_REG_MDIC, (((MDIC_REGADD & 0x1F) << 16) | MDIC_PHYADD | MDIC_OP_READ) );

	while( !(mmio_read32(E1000_REG_MDIC) & (MDIC_R | MDIC_E)) )
	{
		wait_microsecond(1);
	}

	if( mmio_read32(E1000_REG_MDIC) & MDIC_E )
	{
		printk("E1000: MDI READ ERROR\n");
		return -1;
	}

	MDIC_DATA = (uint16_t)(mmio_read32(E1000_REG_MDIC) & 0xFFFF);

	return MDIC_DATA;
}


static void e1000_phy_write( e1000_device_t *dev, int MDIC_REGADD, uint16_t MDIC_DATA )
{
	mmio_write32( E1000_REG_MDIC, ((MDIC_DATA & 0xFFFF) | ((MDIC_REGADD & 0x1F) << 16) | MDIC_PHYADD | MDIC_OP_WRITE) );
									
	while( !(mmio_read32(E1000_REG_MDIC) & (MDIC_R | MDIC_E)) )
	{
		wait_microsecond(1);
	}
	
	if( mmio_read32(E1000_REG_MDIC) & MDIC_E )
	{
		printk("E1000: MDI WRITE ERROR\n");
		return;
	}
}

static void e1000_reset_phy(e1000_device_t *dev)
{
	uint16_t ctrl;

	ctrl = e1000_phy_read(dev, 0);
	ctrl |= 0x8000;
	e1000_phy_write(dev, 0, ctrl);
	wait_microsecond(1);
}


static void print_phy(e1000_device_t *dev)
{
	uint16_t pctrl;
	uint16_t pautoneg;
	uint16_t pstatus;
	uint16_t pinte;
	uint16_t pints;

	pautoneg = e1000_phy_read(dev, 4);
	pctrl    = e1000_phy_read(dev, 0);  // PHY Control Register
	pstatus  = e1000_phy_read(dev, 1);  // PHY Status Register
	pstatus  = e1000_phy_read(dev, 1);  // PHY Status Register (read twice to get current link active value)
	pinte    = e1000_phy_read(dev, 18);
	pints    = e1000_phy_read(dev, 19);

	printk("=========================================================================\n");
	
	printk("PCTRL = 0x%04x\n", pctrl);
	printk("-------------------------\n");
	printk("   .reserved            = 0x%04x\n", (pctrl & 0x3F));
	printk("   .speed_selection_msb = %d\n", ((pctrl >> 6) & 0x1));
	printk("   .collision_test      = %d\n", ((pctrl >> 7) & 0x1));
	printk("   .duplex_mode         = %d\n", ((pctrl >> 8) & 0x1));
	printk("   .restart_autoneg     = %d\n", ((pctrl >> 9) & 0x1));
	printk("   .isolate             = %d\n", ((pctrl >> 10) & 0x1));
	printk("   .power_down          = %d\n", ((pctrl >> 11) & 0x1));
	printk("   .autoneg_enable      = %d\n", ((pctrl >> 12) & 0x1));
	printk("   .speed_selection_lsb = %d\n", ((pctrl >> 13) & 0x1));
	printk("   .loopback            = %d\n", ((pctrl >> 14) & 0x1));
	printk("   .reset               = %d\n", ((pctrl >> 15) & 0x1));

	printk("AUTONEG = 0x%04x\n", pautoneg);
	printk("-------------------------\n");
	printk("    .selector_field     = %d\n", (pautoneg & 0x1F));
	printk("    .10base-tx_half     = %d\n", ((pautoneg >> 5)& 0x1));
	printk("    .10base-tx_full     = %d\n", ((pautoneg >> 6)& 0x1));
	printk("    .100base-tx_half    = %d\n", ((pautoneg >> 7)& 0x1));
	printk("    .100base-tx_full    = %d\n", ((pautoneg >> 8)& 0x1));
	printk("    .100base-t4         = %d\n", ((pautoneg >> 9)& 0x1));
	printk("    .pause              = %d\n", ((pautoneg >> 10)& 0x1));
	printk("    .asymmetric_pause   = %d\n", ((pautoneg >> 11)& 0x1));
	printk("    .rsvd               = %d\n", ((pautoneg >> 12)& 0x1));
	printk("    .remote_fault       = %d\n", ((pautoneg >> 13)& 0x1));
	printk("    .rsvd               = %d\n", ((pautoneg >> 14)& 0x1));
	printk("    .next_page          = %d\n", ((pautoneg >> 15)& 0x1));

	printk("PSTATUS = 0x%04x\n", pstatus);
	printk("-------------------------\n");
	printk("   .extended_cap        = %d\n", ((pstatus >> 0) & 0x1));
	printk("   .jabber_detect       = %d\n", ((pstatus >> 1) & 0x1));
	printk("   .link_status         = %d\n", ((pstatus >> 2) & 0x1));
	printk("   .autoneg_ability     = %d\n", ((pstatus >> 3) & 0x1));
	printk("   .remote_fault        = %d\n", ((pstatus >> 4) & 0x1));
	printk("   .autoneg_complete    = %d\n", ((pstatus >> 5) & 0x1));
	printk("   .mf_preamble_supp    = %d\n", ((pstatus >> 6) & 0x1));
	printk("   .reserved            = %d\n", ((pstatus >> 7) & 0x1));
	printk("   .extended_status     = %d\n", ((pstatus >> 8) & 0x1));
	printk("   .100base_t2_half     = %d\n", ((pstatus >> 9) & 0x1));
	printk("   .100base_t2_full     = %d\n", ((pstatus >> 10) & 0x1));
	printk("   .10mb_half           = %d\n", ((pstatus >> 11) & 0x1));
	printk("   .10mb_full           = %d\n", ((pstatus >> 12) & 0x1));
	printk("   .100base_x_half      = %d\n", ((pstatus >> 13) & 0x1));
	printk("   .100base_x_full      = %d\n", ((pstatus >> 14) & 0x1));
	printk("   .100base_t4          = %d\n", ((pstatus >> 15) & 0x1));


	printk("PINTE = 0x%04x\n", pinte);
	printk("-------------------------\n");
	printk("   .jabber              = %d\n", ((pinte >> 0) & 0x1));
	printk("   .polarity_changed    = %d\n", ((pinte >> 1) & 0x1));
	printk("   .reserved            = 0x%01x\n", ((pinte >> 2) & 0x3));
	printk("   .energy_detect       = %d\n", ((pinte >> 4) & 0x1));
	printk("   .downshift           = %d\n", ((pinte >> 5) & 0x1));
	printk("   .mdi_crossover       = %d\n", ((pinte >> 6) & 0x1));
	printk("   .fifo_overunflow     = %d\n", ((pinte >> 7) & 0x1));
	printk("   .false_carrier       = %d\n", ((pinte >> 8) & 0x1));
	printk("   .symbol_error        = %d\n", ((pinte >> 9) & 0x1));
	printk("   .link_status_change  = %d\n", ((pinte >> 10) & 0x1));
	printk("   .autoneg_complete    = %d\n", ((pinte >> 11) & 0x1));
	printk("   .page_received       = %d\n", ((pinte >> 12) & 0x1));
	printk("   .duplex_changed      = %d\n", ((pinte >> 13) & 0x1));
	printk("   .speed_changed       = %d\n", ((pinte >> 14) & 0x1));
	printk("   .autoneg_error       = %d\n", ((pinte >> 15) & 0x1));

	printk("PINTS = 0x%04x\n", pints);
	printk("-------------------------\n");
	printk("   .jabber              = %d\n", ((pints >> 0) & 0x1));
	printk("   .polarity_changed    = %d\n", ((pints >> 1) & 0x1));
	printk("   .reserved            = 0x%01x\n", ((pints >> 2) & 0x3));
	printk("   .energy_detect       = %d\n", ((pints >> 4) & 0x1));
	printk("   .downshift           = %d\n", ((pints >> 5) & 0x1));
	printk("   .mdi_crossover       = %d\n", ((pints >> 6) & 0x1));
	printk("   .fifo_overunflow     = %d\n", ((pints >> 7) & 0x1));
	printk("   .false_carrier       = %d\n", ((pints >> 8) & 0x1));
	printk("   .symbol_error        = %d\n", ((pints >> 9) & 0x1));
	printk("   .link_status_change  = %d\n", ((pints >> 10) & 0x1));
	printk("   .autoneg_complete    = %d\n", ((pints >> 11) & 0x1));
	printk("   .page_received       = %d\n", ((pints >> 12) & 0x1));
	printk("   .duplex_changed      = %d\n", ((pints >> 13) & 0x1));
	printk("   .speed_changed       = %d\n", ((pints >> 14) & 0x1));
	printk("   .autoneg_error       = %d\n", ((pints >> 15) & 0x1));

	printk("=========================================================================\n");
}


static void print_general_regs(e1000_device_t *dev)
{
	uint32_t ctrl;   // Device Control Register
	uint32_t status; // Device Status Register
	uint32_t ims;    // Interrupt Mask Set/Read Register
	uint32_t rctl;   // Recieve Control Register

	ctrl   = mmio_read32(E1000_REG_CTRL);
	status = mmio_read32(E1000_REG_STATUS);
	ims    = mmio_read32(E1000_REG_IMS);
	rctl   = mmio_read32(E1000_REG_RCTL);

	printk("=========================================================================\n");

	printk("CTRL = 0x%08x\n", ctrl);
	printk("-------------------------\n");
	printk("   .full_duplex         = %d\n", ((ctrl >> 0) & 0x1));
	printk("   .reserved            = 0x%01x\n", ((ctrl >> 1) & 0x3));
	printk("   .link_reset          = %d\n", ((ctrl >> 3) & 0x1));
	printk("   .reserved            = %d\n", ((ctrl >> 4) & 0x1));
	printk("   .auto_speed_detect   = %d\n", ((ctrl >> 5) & 0x1));
	printk("   .set_link_up         = %d\n", ((ctrl >> 6) & 0x1));
	printk("   .invert_loss_of_sig  = %d\n", ((ctrl >> 7) & 0x1));
	printk("   .speed_selection     = 0x%01x\n", ((ctrl >> 8) & 0x3));
	printk("   .reserved            = %d\n", ((ctrl >> 10) & 0x1));
	printk("   .en_force_speed      = %d\n", ((ctrl >> 11) & 0x1));
	printk("   .en_force_duplex     = %d\n", ((ctrl >> 12) & 0x1));
	printk("   .reserved            = 0x%02x\n", ((ctrl >> 13) & 0x1F));
	printk("   .sdp0_data           = %d\n", ((ctrl >> 18) & 0x1));
	printk("   .sdp1_data           = %d\n", ((ctrl >> 19) & 0x1));
	printk("   .d3cold_wakeup_en    = %d\n", ((ctrl >> 20) & 0x1));
	printk("   .phy_powermang_en    = %d\n", ((ctrl >> 21) & 0x1));
	printk("   .spd0_iodir          = %d\n", ((ctrl >> 22) & 0x1));
	printk("   .spd1_iodir          = %d\n", ((ctrl >> 23) & 0x1));
	printk("   .reserved            = %d\n", ((ctrl >> 24) & 0x3));
	printk("   .device_reset        = %d\n", ((ctrl >> 26) & 0x1));
	printk("   .rx_flowctrl_en      = %d\n", ((ctrl >> 27) & 0x1));
	printk("   .tx_flowctrl_en      = %d\n", ((ctrl >> 28) & 0x1));
	printk("   .reserved            = %d\n", ((ctrl >> 29) & 0x1));
	printk("   .vlan_mode_enable    = %d\n", ((ctrl >> 30) & 0x1));
	printk("   .phy_reset           = %d\n", ((ctrl >> 31) & 0x1));

	printk("STATUS = 0x%08x\n", status);
	printk("-------------------------\n");
	printk("   .link_is_full_duplex = %d\n", ((status >> 0) & 0x1));
	printk("   .link_up_indication  = %d\n", ((status >> 1) & 0x1));
	printk("   .function_id         = 0x%01x\n", ((status >> 2) & 0x3));
	printk("   .tx_paused           = %d\n", ((status >> 4) & 0x1));
	printk("   .tbi_serdes_mode     = %d\n", ((status >> 5) & 0x1));
	printk("   .speed               = 0x%01x\n", ((status >> 6) & 0x3));
	printk("   .auto_speed_detected = 0x%01x\n", ((status >> 8) & 0x3));
	printk("   .reserved            = %d\n", ((status >> 10) & 0x1));
	printk("   .pci66mhz            = %d\n", ((status >> 11) & 0x1));
	printk("   .pci64bit            = %d\n", ((status >> 12) & 0x1));
	printk("   .pcix_mode           = %d\n", ((status >> 13) & 0x1));
	printk("   .pcix_speed          = 0x%01x\n", ((status >> 14) & 0x3));
	printk("   .reserved            = 0x%04x\n", ((status >> 16) & 0xffff));

	printk("IMS = 0x%08x\n", ims);
	printk("-------------------------\n");
	printk("   .tx_desc_wb          = %d\n", ((ims >> 0) & 0x1));
	printk("   .tx_q_empty          = %d\n", ((ims >> 1) & 0x1));
	printk("   .link_status_change  = %d\n", ((ims >> 2) & 0x1));
	printk("   .rx_sequence_error   = %d\n", ((ims >> 3) & 0x1));
	printk("   .rx_min_thresh_hit   = %d\n", ((ims >> 4) & 0x1));
	printk("   .reserved            = %d\n", ((ims >> 5) & 0x1));
	printk("   .rx_fifo_overrun     = %d\n", ((ims >> 6) & 0x1));
	printk("   .rx_timer_interrupt  = %d\n", ((ims >> 7) & 0x1));
	printk("   .reserved            = %d\n", ((ims >> 8) & 0x1));
	printk("   .mdio_access_done    = %d\n", ((ims >> 9) & 0x1));
	printk("   .rx_cfg              = %d\n", ((ims >> 10) & 0x1));
	printk("   .reserved            = %d\n", ((ims >> 11) & 0x1));
	printk("   .phy_interrupts      = %d\n", ((ims >> 12) & 0x1));
	printk("   .general_interrupts  = 0x%01x\n", ((ims >> 13) & 0x3));
	printk("   .tx_desc_low_thresh  = %d\n", ((ims >> 15) & 0x1));
	printk("   .small_rx_packet     = %d\n", ((ims >> 16) & 0x1));
	printk("   .reserved            = 0x%04x\n", ((ims >> 17) & 0x7fff));

	// Print Interrupt Cause Register.
	// This is probably only valid once an interrupt has fired, but...
	//print_icr();

	printk("RCTL = 0x%08x\n", rctl);
	printk("-------------------------\n");
	printk("   .reserved            = %d\n", ((rctl >> 0) & 0x1));
	printk("   .enable              = %d\n", ((rctl >> 1) & 0x1));
	printk("   .store_bad_packets   = %d\n", ((rctl >> 2) & 0x1));
	printk("   .unicast_promisc_en  = %d\n", ((rctl >> 3) & 0x1));
	printk("   .multicast_promisc_en= %d\n", ((rctl >> 4) & 0x1));
	printk("   .long_packet_rx_en   = %d\n", ((rctl >> 5) & 0x1));
	printk("   .loopback_mode       = 0x%01x\n", ((rctl >> 6) & 0x3));
	printk("   .rx_desc_min_thresh  = 0x%01x\n", ((rctl >> 8) & 0x3));
	printk("   .reserved            = 0x%01x\n", ((rctl >> 10) & 0x3));
	printk("   .multicast_offset    = 0x%01x\n", ((rctl >> 12) & 0x3));
	printk("   .reserved            = %d\n", ((rctl >> 14) & 0x1));
	printk("   .accept_bcasts       = %d\n", ((rctl >> 15) & 0x1));
	printk("   .rx_buf_size         = 0x%01x\n", ((rctl >> 16) & 0x3));
	printk("   .vlan_filter_en      = %d\n", ((rctl >> 18) & 0x1));
	printk("   .canonical_form_en   = %d\n", ((rctl >> 19) & 0x1));
	printk("   .canonical_form_bit  = %d\n", ((rctl >> 20) & 0x1));
	printk("   .reserved            = %d\n", ((rctl >> 21) & 0x1));
	printk("   .discard_pause_frames= %d\n", ((rctl >> 22) & 0x1));
	printk("   .pass_mac_ctrl_frames= %d\n", ((rctl >> 23) & 0x1));
	printk("   .reserved            = %d\n", ((rctl >> 24) & 0x1));
	printk("   .buffer_size_ext     = %d\n", ((rctl >> 25) & 0x1));
	printk("   .strip_eth_crc       = %d\n", ((rctl >> 26) & 0x1));
	printk("   .reserved            = 0x%02x\n", ((rctl >> 27) & 0x1F));

	printk("=========================================================================\n");
}


#define RCTL_EN				(1 << 1)
#define RCTL_SBP			(1 << 2)
#define RCTL_UPE			(1 << 3)
#define RCTL_MPE			(1 << 4)
#define RCTL_LPE			(1 << 5)
#define RDMTS_HALF			(0 << 8)
#define RDMTS_QUARTER			(1 << 8)
#define RDMTS_EIGHTH			(2 << 8)
#define RCTL_BAM			(1 << 15)
#define RCTL_BSIZE_256			(3 << 16)
#define RCTL_BSIZE_512			(2 << 16)
#define RCTL_BSIZE_1024			(1 << 16)
#define RCTL_BSIZE_2048			(0 << 16)
#define RCTL_BSIZE_4096			((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192			((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384		((1 << 16) | (1 << 25))
#define RCTL_SECRC			(1 << 26)


static void e1000_rx_enable(struct netif *netif)
{
	e1000_device_t *dev = netif->state;
	mmio_write32( E1000_REG_RCTL, mmio_read32(E1000_REG_RCTL) | (RCTL_EN) );
}


static int e1000_rx_init(struct netif *netif)
{
	int i;
	e1000_device_t *dev = netif->state;
	
	// unaligned base address
	uint64_t tmpbase = (uint64_t)kmem_alloc((sizeof(e1000_rx_desc_t) * NUM_RX_DESCRIPTORS) + 16);
	// aligned base address
	dev->rx_desc_base = (tmpbase % 16) ? (uint8_t *)((tmpbase) + 16 - (tmpbase % 16)) : (uint8_t *)tmpbase;
	
	for( i = 0; i < NUM_RX_DESCRIPTORS; i++ )
	{
		dev->rx_desc[i] = (e1000_rx_desc_t *)(dev->rx_desc_base + (i * 16));
		dev->rx_desc[i]->address = (uint64_t)__pa(kmem_alloc(8192+16)); // packet buffer size (8K)
		dev->rx_desc[i]->status = 0;
	}
	
	// setup the receive descriptor ring buffer (TODO: >32-bits may be broken in this code)
	mmio_write32( E1000_REG_RDBAH, (uint32_t)((uint64_t)__pa(dev->rx_desc_base) >> 32) );
	mmio_write32( E1000_REG_RDBAL, (uint32_t)((uint64_t)__pa(dev->rx_desc_base) & 0xFFFFFFFF) );
	
	// receive buffer length; NUM_RX_DESCRIPTORS 16-byte descriptors
	mmio_write32( E1000_REG_RDLEN, (uint32_t)(NUM_RX_DESCRIPTORS * 16) );
	
	// setup head and tail pointers
	mmio_write32( E1000_REG_RDH, 0 );
	mmio_write32( E1000_REG_RDT, NUM_RX_DESCRIPTORS );
	dev->rx_tail = 0;
	
	// set the receieve control register (promisc ON, 8K pkt size)
	mmio_write32( E1000_REG_RCTL, (RCTL_SBP | RCTL_UPE | RCTL_MPE | RDMTS_HALF | RCTL_SECRC | 
									RCTL_LPE | RCTL_BAM | RCTL_BSIZE_8192) );
	return 0;
}


#define TCTL_EN				(1 << 1)
#define TCTL_PSP			(1 << 3)


static int e1000_tx_init(struct netif *netif)
{
	int i;
	e1000_device_t *dev = netif->state;

	
	uint64_t tmpbase = (uint64_t)kmem_alloc((sizeof(e1000_tx_desc_t) * NUM_TX_DESCRIPTORS) + 16);
	dev->tx_desc_base = (tmpbase % 16) ? (uint8_t *)((tmpbase) + 16 - (tmpbase % 16)) : (uint8_t *)tmpbase;

	for( i = 0; i < NUM_TX_DESCRIPTORS; i++ )
	{
		dev->tx_desc[i] = (e1000_tx_desc_t *)(dev->tx_desc_base + (i * 16));
		dev->tx_desc[i]->address = 0;
		dev->tx_desc[i]->cmd = 0;
	}

	// setup the transmit descriptor ring buffer
	mmio_write32( E1000_REG_TDBAH, (uint32_t)((uint64_t)__pa(dev->tx_desc_base) >> 32) );
	mmio_write32( E1000_REG_TDBAL, (uint32_t)((uint64_t)__pa(dev->tx_desc_base) & 0xFFFFFFFF) );
	
	// transmit buffer length; NUM_TX_DESCRIPTORS 16-byte descriptors
	mmio_write32( E1000_REG_TDLEN, (uint32_t)(NUM_TX_DESCRIPTORS * 16) );
	
	// setup head and tail pointers
	mmio_write32( E1000_REG_TDH, 0 );
	mmio_write32( E1000_REG_TDT, 0 );
	dev->tx_tail = 0;
	
	// set the transmit control register (padshortpackets)
	mmio_write32( E1000_REG_TCTL, (TCTL_EN | TCTL_PSP) );
	return 0;
}


static err_t e1000_tx_poll(struct netif *netif, struct pbuf *pkt)
{
	e1000_device_t *dev = netif->state;
	struct pbuf *q;
	int count = 0;

	printk("In e1000_tx_poll\n");

	// Count the number of entries in the pbuf
	for (q = pkt; q != NULL ; q = q->next)
		++count;
	if (count != 1) {
		printk("E1000: trying to transmit pbuf with more than one region, count=%d\n", count);
		return -1;
	}

	printk("E1000: transmitting packet (%u bytes) [h=%u, t=%u]\n", pkt->tot_len, mmio_read32(E1000_REG_TDH), dev->tx_tail);
	
	dev->tx_desc[dev->tx_tail]->address = (uint64_t) __pa(pkt->payload);
	dev->tx_desc[dev->tx_tail]->length = pkt->len;
	dev->tx_desc[dev->tx_tail]->cmd = ((1 << 3) | (3));
	
	// update the tail so the hardware knows it's ready
	int oldtail = dev->tx_tail;
	dev->tx_tail = (dev->tx_tail + 1) % NUM_TX_DESCRIPTORS;
	mmio_write32(E1000_REG_TDT, dev->tx_tail);

	while( !(dev->tx_desc[oldtail]->sta & 0xF) )
	{
		for (int i = 0; i < 50; i++) {
			wait_microsecond(20000);
		}
	}
	
	printk("E1000: TX DONE: transmit status = 0x%01x\n", (dev->tx_desc[oldtail]->sta & 0xF));

	return 0;
}


// This can be used stand-alone or from an interrupt handler
static void e1000_rx_poll(struct netif *netif)
{
	e1000_device_t *dev = netif->state;

	while( (dev->rx_desc[dev->rx_tail]->status & (1 << 0)) )
	{
		// raw packet and packet length (excluding CRC)
		uint8_t *pkt = __va((void *)dev->rx_desc[dev->rx_tail]->address);
		uint16_t pktlen = dev->rx_desc[dev->rx_tail]->length;
		bool dropflag = false;

		if( pktlen < 60 )
		{
			printk("E1000: short packet (%u bytes)\n", pktlen);
			dropflag = true;
		}

		// while not technically an error, there is no support in this driver
		if( !(dev->rx_desc[dev->rx_tail]->status & (1 << 1)) )
		{
			printk("E1000: no EOP set! (len=%u, 0x%x 0x%x 0x%x)\n", 
				pktlen, pkt[0], pkt[1], pkt[2]);
			dropflag = true;
		}
		
		if( dev->rx_desc[dev->rx_tail]->errors )
		{
			printk("E1000: rx errors (0x%x)\n", dev->rx_desc[dev->rx_tail]->errors);
			dropflag = true;
		}

		if( !dropflag )
		{
			// send the packet to higher layers for parsing
		}
		
		// update RX tail pointer
		dev->rx_desc[dev->rx_tail]->status = (uint16_t)(0);
		dev->rx_tail = (dev->rx_tail + 1) % NUM_RX_DESCRIPTORS;
		
		// write the tail to the device
		mmio_write32(E1000_REG_RDT, dev->rx_tail);
	}
}


static irqreturn_t e1000_interrupt_handler(int vector, void *priv)
{
	struct netif *netif = priv;	
	e1000_device_t *dev = netif->state;

	printk("In e1000_interrupt_handler\n");
	
	if( netif == NULL )
	{
		printk("e1000: UNKNOWN IRQ / INVALID DEVICE\n");
		return -1;
	}
	
	// read the pending interrupt status
	uint32_t icr = mmio_read32(E1000_REG_ICR);

	// tx success stuff
	icr &= ~(3);

	// LINK STATUS CHANGE
	if( icr & (1 << 2) )
	{
		icr &= ~(1 << 2);
		mmio_write32(E1000_REG_CTRL, (mmio_read32(E1000_REG_CTRL) | CTRL_SLU));		
		
		// debugging info (probably not necessary in most scenarios)
		printk("e1000: Link Status Change, STATUS = 0x%08x\n", mmio_read32(E1000_REG_STATUS));
		printk("e1000: PHY CONTROL  = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_PCTRL));
		printk("e1000: PHY STATUS   = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_PSTATUS));
		printk("e1000: PHY PSSTAT   = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_PSSTAT));
		printk("e1000: PHY ANA      = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_ANA));
		printk("e1000: PHY ANE      = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_ANE));
		printk("e1000: PHY GCON     = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_GCON));
		printk("e1000: PHY GSTATUS  = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_GSTATUS));
		printk("e1000: PHY EPSTATUS = 0x%04x\n", e1000_phy_read(dev, E1000_PHYREG_EPSTATUS));
	}
	
	// RX underrun / min threshold
	if( icr & (1 << 6) || icr & (1 << 4) )
	{
		icr &= ~((1 << 6) | (1 << 4));
		printk(" underrun (rx_head = %u, rx_tail = %u)\n", mmio_read32(E1000_REG_RDH), dev->rx_tail);
		
		volatile int i;
		for(i = 0; i < NUM_RX_DESCRIPTORS; i++)
		{
			if( dev->rx_desc[i]->status )
				printk(" pending descriptor (i=%u, status=0x%x)\n", i, dev->rx_desc[i]->status );
		}
		
		e1000_rx_poll(netif);
	}

	// packet is pending
	if( icr & (1 << 7) )
	{
		icr &= ~(1 << 7);
		e1000_rx_poll(netif);
	}
	
	if( icr )
		printk("E1000: unhandled interrupt #%u received! (ICR = 0x%08x)\n", vector, icr);
	
	// clearing the pending interrupts
	mmio_read32(E1000_REG_ICR);

	return IRQ_HANDLED;
}


/***************************************************
 ** Intel 825xx-series Chipset Driver Entry Point **
 ***************************************************/
err_t e1000_hw_init(struct netif *netif)
{
	e1000_device_t *dev = netif->state;

	// "Name" the interface
	netif->name[0] = 'e';
	netif->name[1] = 'n';

	// Figure out where the e1000 registers are mapped into (physical) memory
	pcicfg_bar_decode(&dev->pci_dev->cfg, 0, &dev->mem_bar);
	dev->mmio_paddr = dev->mem_bar.address;
	dev->mmio_vaddr = (vaddr_t) __va(dev->mmio_paddr);
	printk("E1000 mmio_paddr:   0x%lx\n", dev->mmio_paddr);
	printk("E1000 mmio_vaddr:   0x%lx\n", dev->mmio_vaddr);

	// Figure out where the e1000 I/O ports are located
	pcicfg_bar_decode(&dev->pci_dev->cfg, 4, &dev->io_bar);
	dev->io_base   = dev->io_bar.address;
	printk("E1000 io_base:      0x%x\n", dev->io_base);
	
	// Get our MAC address
	uint16_t mac16 = e1000_eeprom_read(dev, 0);
	netif->hwaddr_len = 6;
	netif->hwaddr[0] = (mac16 & 0xFF);
	netif->hwaddr[1] = (mac16 >> 8) & 0xFF;
	mac16 = e1000_eeprom_read(dev, 1);
	netif->hwaddr[2] = (mac16 & 0xFF);
	netif->hwaddr[3] = (mac16 >> 8) & 0xFF;
	mac16 = e1000_eeprom_read(dev, 2);
	netif->hwaddr[4] = (mac16 & 0xFF);
	netif->hwaddr[5] = (mac16 >> 8) & 0xFF;

	printk("E1000 MAC address:  %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
	       netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
	       netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);

	// Register our interrupt handler
	int vector = ioapic_pcidev_vector(dev->pci_dev->cfg.bus, dev->pci_dev->cfg.slot, 0);
	if (vector == -1) {
		printk("E1000: Failed to find interrupt vector.  Assuming 66!\n");
		vector = 66;
	}
	printk("E1000 IDT Vector:   %d\n", vector);
	irq_request(vector, &e1000_interrupt_handler, 0, "e1000", netif);

	// Initialize the rest of the Lightweight IP netif structure
	netif->mtu        = 1500;
	netif->flags      = (NETIF_FLAG_LINK_UP | NETIF_FLAG_ETHARP);
	netif->linkoutput = e1000_tx_poll;
	netif->output     = etharp_output;

	// Set the E1000 LINK UP
	mmio_write32(E1000_REG_CTRL, (mmio_read32(E1000_REG_CTRL) | CTRL_SLU));
	
	// Initialize the Multicase Table Array
	for( int i = 0; i < 128; i++ )
		mmio_write32(E1000_REG_MTA + (i * 4), 0);

	// Enable PCI bus mastering, so the card can initiate memory requests.
	// On VMware, E1000 bus mastering is disabled after power on, so the
	// card can't actually send or receive anything until this bit is set.
	uint16_t cmd = pci_read(dev->pci_dev, PCIR_COMMAND, 2);
	cmd |= PCIM_CMD_BUSMASTEREN;
	pci_write(dev->pci_dev, PCIR_COMMAND, 2, cmd);
		
	// enable all interrupts (and clear existing pending ones)
	mmio_write32( E1000_REG_IMS, 0x1F6DC );
	mmio_read32(E1000_REG_ICR);
	
	// Initialize the E1000 transmit and receive state
	e1000_rx_init(netif);
	e1000_tx_init(netif);

	e1000_rx_enable(netif);
	
	printk("E1000: Configuration complete\n");
	
	return 0;
}

int e1000_init(void)
{
	e1000_device_t *dev = &e1000_state;
	
	// First thing, make sure there is an e1000 device present
	pci_dev_t *pci_dev;
	if ((pci_dev = pci_lookup_device(0x8086, 0x100f)) == NULL)
		return -1;
	//pcicfg_hdr_print(&pci_dev->cfg);

	// Remember our PCI info
	dev->pci_dev = pci_dev;

	// Figure out our IP info
	printk("E1000 IP address:   %s\n", dev->ip_str);
	printk("E1000 Netmask:      %s\n", dev->nm_str);
	printk("E1000 Gateway:      %s\n", dev->gw_str);

	// Convert IP strings to lightweight IP address structures
	dev->ip.addr = inet_addr(dev->ip_str);
	dev->nm.addr = inet_addr(dev->nm_str);
	dev->gw.addr = inet_addr(dev->gw_str);

	// Tell Lightweight IP (lwip) about the new interface.
	// The e1000_hw_init() function passed in is called immediately by netif_add().
	netif_add(&e1000_netif, &dev->ip, &dev->nm, &dev->gw, dev, e1000_hw_init, tcpip_input);

	return 0;
}


DRIVER_INIT("net", e1000_init);

DRIVER_PARAM_STRING(ip, e1000_state.ip_str, sizeof(e1000_state.ip_str));
DRIVER_PARAM_STRING(nm, e1000_state.nm_str, sizeof(e1000_state.nm_str));
DRIVER_PARAM_STRING(gw, e1000_state.gw_str, sizeof(e1000_state.gw_str));
