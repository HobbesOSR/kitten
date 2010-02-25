#include <net/net_namespace.h>
#include <net/neighbour.h>
#include <net/route.h>
#include <lwk/if_arp.h>
#include <lwk/if.h>
#include <linux/if_infiniband.h>
#include <linux/arp_table.h>

// needed by linux code to compile but not used by executed code path
struct net init_net;
struct neigh_table arp_tbl;

// we allow one device and it's ib0
static struct in_device* _ib0_in_device = NULL;

static int arp_set( struct sockaddr_in*, hw_addr_t* );
static int arp_delete( struct sockaddr_in* );

int sys_lwk_arp( struct lwk_arpreq* req )
{
	struct lwk_arpreq  kbuf;

	if( copy_from_user( &kbuf, (void*) req, sizeof(kbuf) ) )
		return -EFAULT;

	if ( strncmp( kbuf.arp_dev, "ib0", 3 ) ) {
		return -EINVAL;
	}

	if ( ! _ib0_in_device ) {
		return -EINVAL;
	}
      
	if ( kbuf.arp_flags == ATF_PUBL ) {
		return arp_set( (struct sockaddr_in*)
					&kbuf.arp_pa, (hw_addr_t*)kbuf.arp_ha );
	} else if ( ! ( kbuf.arp_flags & ATF_PUBL ) ) {
		return arp_delete( (struct sockaddr_in*) &kbuf.arp_pa );
	} else {
		return -EINVAL;
	}
}

static int arp_set( struct sockaddr_in* in_addr, hw_addr_t* hw_addr )
{
	if ( _ib0_in_device->arp_table->set( _ib0_in_device->arp_table,
				in_addr->sin_addr.s_addr, hw_addr ) )
	{ 
		return -EINVAL;
	}
	return 0;
}

static int arp_delete( struct sockaddr_in* in_addr )
{
	if ( _ib0_in_device->arp_table->delete( _ib0_in_device->arp_table,
				in_addr->sin_addr.s_addr ) )
	{ 
		return -EINVAL;
	}
	return 0;
}

static hw_addr_t* arp_find( __be32 addr )
{
	return _ib0_in_device->arp_table->get( _ib0_in_device->arp_table,
				addr );
}

static struct in_device* ib_dev_alloc(__be32 addr, __be32 netmask, hw_addr_t* );
static int ib_dev_destroy( struct in_device * ); 

int sys_lwk_ifconfig( struct lwk_ifreq* req )
{
	struct lwk_ifreq  kbuf;

	if( copy_from_user( &kbuf, (void*) req, sizeof(kbuf) ) )
		return -EFAULT;
      
	if ( strncmp( kbuf.ifr_name, "ib0", 3 ) ) {
		return -EINVAL;
	}

	if ( _ib0_in_device ) {
		ib_dev_destroy(_ib0_in_device );
	}

	_ib0_in_device = ib_dev_alloc(
		((struct sockaddr_in*) &kbuf.ifr_addr)->sin_addr.s_addr,
		((struct sockaddr_in*) &kbuf.ifr_netmask)->sin_addr.s_addr,
			&kbuf.ifr_hwaddr ); 

	return _ib0_in_device ? 0 : -EINVAL; 
}

static struct in_device* ib_dev_alloc(__be32 addr, __be32 netmask, 
			hw_addr_t* hw_addr )
{
	struct in_device* in_dev = (struct in_device*) 
				kmem_alloc( sizeof( *in_dev ) );
	if ( ! in_dev )  return NULL;

	in_dev->dev = (struct net_device*) kmem_alloc( sizeof( *in_dev->dev ) );

	if ( ! in_dev->dev ) {
		kmem_free( in_dev );
		return NULL;
	}

	in_dev->ifa_address = addr;
	in_dev->arp_table = &_arp_table_simple;
	in_dev->arp_table->init( in_dev->arp_table,
					__be32_to_cpu( netmask ) + 1 );

	memset( in_dev->dev, 0, sizeof(*in_dev->dev) );

	in_dev->dev->type = ARPHRD_INFINIBAND;
	in_dev->dev->addr_len = INFINIBAND_ALEN;
	memcpy( in_dev->dev->dev_addr, hw_addr, in_dev->dev->addr_len );
	memset( in_dev->dev->broadcast, 0xff, MAX_ADDR_LEN );

	LINUX_DBG( FALSE, "addr=%#x\n", __be32_to_cpu(addr));
	LINUX_DBG( FALSE, "netmask=%#x\n", __be32_to_cpu(netmask));

	return in_dev;
}
static int ib_dev_destroy( struct in_device* dev )
{
	dev->arp_table->fini( dev->arp_table ); 
	kmem_free( dev->dev );
	kmem_free( dev );
	return 0;
}

struct neighbour* neigh_lookup(struct neigh_table *tbl, const void *pkey,
						struct net_device *dev)
{
	struct neighbour* neigh;
	hw_addr_t*   	  dst_hw_addr;

	LINUX_DBG( FALSE, "\n" );

	dst_hw_addr = arp_find( *(__be32*) pkey );
	if ( ! dst_hw_addr ) return NULL;

	neigh = (struct neighbour*) kmem_alloc( sizeof(*neigh) );
	if ( ! neigh  ) return NULL;

	neigh->dev = dev;
	neigh->nud_state = NUD_VALID;

	memcpy( neigh->ha, dst_hw_addr, MAX_ADDR_LEN );

	return neigh;
}

void neigh_release(struct neighbour *neigh)
{
	LINUX_DBG( FALSE, "\n");
	kmem_free( neigh );
}

int ip_route_output_key(struct net *net, struct rtable **rt,
							struct flowi *flp)
{
	LINUX_DBG( FALSE, "dst=%#x src=%#x\n",
					be32_to_cpu(flp->nl_u.ip4_u.daddr),
					be32_to_cpu(flp->nl_u.ip4_u.saddr));

	*rt = (struct rtable*) kmem_alloc( sizeof( **rt ) );

	if ( ! *rt ) return -1;

	(*rt)->idev = _ib0_in_device;
	(*rt)->rt_gateway = flp->nl_u.ip4_u.daddr;

	return 0;
}

void ip_rt_put(struct rtable * rt)
{
	LINUX_DBG( FALSE, "\n");
	kmem_free( rt );
}

int neigh_event_send(struct neighbour *neigh, struct sk_buff *skb)
{
	LINUX_DBG(TRUE,"\n");
	return 0;
}
