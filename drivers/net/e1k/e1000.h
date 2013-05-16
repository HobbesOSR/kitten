#ifndef _e1000_h_
#define _e1000_h_

volatile uint32_t *e1000; // MMIO address to access E1000 BAR

#define E1000_VENDORID 0x8086
#define E1000_DEVICEID 0x100e

#define E1000_IRQ	 11
#define E1000_IDTVEC	 64
#define E1000_MTU      1500

#define E1000_MMIOADDR 0xc100 /* MMIO Address for E1000 BAR 0 */

#define E1000_TXDESC 64
#define E1000_RCVDESC 64
#define TX_PKT_SIZE 1518
#define RCV_PKT_SIZE 2048

// Register Set
#define E1000_STATUS   0x00008/4  /* Device Status - RO */
#define E1000_EERD     0x00014/4  /* EEPROM Read - RW */

#define E1000_EERD_START 0x01
#define E1000_EERD_DONE  0x10

#define E1000_TDBAL    0x03800/4  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804/4  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808/4  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810/4  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818/4  /* TX Descripotr Tail - RW */

#define E1000_RDBAL    0x02800/4  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804/4  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808/4  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810/4  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818/4  /* RX Descriptor Tail - RW */
#define E1000_RAL      0x05400/4  /* Receive Address Low - RW */
#define E1000_RAH      0x05404/4  /* Receive Address High - RW */

#define E1000_TCTL     0x00400/4  /* TX Control - RW */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

#define E1000_RCTL     0x00100/4  /* RX Control - RW */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM            0x000000C0    /* loopback mode */
#define E1000_RCTL_RDMTS          0x00000300    /* rx min threshold size */
#define E1000_RCTL_MO             0x00003000    /* multicast offset shift */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SZ             0x00030000    /* rx buffer size */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_TIPG     0x00410/4  /* TX Inter-packet gap -RW */

/* Transmit Descriptor bit definitions */
#define E1000_TXD_CMD_RS     0x00000008 /* Report Status */
#define E1000_TXD_CMD_EOP    0x00000001 /* End of Packet */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */

/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */

struct tx_desc
{
        uint64_t addr;
        uint16_t length;
        uint8_t cso;
        uint8_t cmd;
        uint8_t status;
        uint8_t css;
        uint16_t special;
} __attribute__((packed));

struct rcv_desc
{
        uint64_t addr;
        uint16_t length;
        uint16_t chksum;
        uint8_t status;
        uint8_t errors;
        uint16_t special;
} __attribute__((packed));

struct tx_pkt
{
        uint8_t buf[TX_PKT_SIZE];
} __attribute__((packed));

struct rcv_pkt
{
        uint8_t buf[RCV_PKT_SIZE];
} __attribute__((packed));

#endif  // _e1000_h_

