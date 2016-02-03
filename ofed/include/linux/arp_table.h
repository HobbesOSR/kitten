#ifndef _LINUX_ARP_TABLE_H
#define _LINUX_ARP_TABLE_H

#include <linux/netdevice.h>

typedef unsigned char hw_addr_t[MAX_ARP_ADDR_LEN];

struct arp_table {
	int (*init)( struct arp_table*, __u32 );
	int (*fini)( struct arp_table* );
	int (*set)( struct arp_table*, __be32 addr, hw_addr_t* );	
	int (*delete)( struct arp_table*, __be32 addr );	
	hw_addr_t* (*get)( struct arp_table*, __be32 addr );	
	void* private;
	int mask;
}; 

extern struct arp_table _arp_table_simple;
#endif
