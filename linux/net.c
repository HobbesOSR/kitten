#include <net/net_namespace.h>
#include <net/neighbour.h>
#include <net/route.h>
#include <lwk/if_arp.h>
#include <lwk/if.h>
#include <linux/if_infiniband.h>
#include <linux/arp_table.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

// needed by linux code to compile but not used by executed code path
struct net init_net;
struct neigh_table arp_tbl;

DEFINE_RWLOCK(dev_base_lock);

static DEFINE_MUTEX(rtnl_mutex);

void rtnl_lock(void)
{
	mutex_lock(&rtnl_mutex);
}
EXPORT_SYMBOL(rtnl_lock);

void rtnl_unlock(void)
{
	/*FIXME Linux has more protection !*/
	mutex_unlock(&rtnl_mutex);
}

int dev_mc_delete(struct net_device *dev, void *addr, int alen, int glbl)
{
	return 0;
}

int dev_mc_add(struct net_device *dev, void *addr, int alen, int glbl)
{
	return 0;
}
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
					__be32_to_cpu( netmask ) );

	memset( in_dev->dev, 0, sizeof(*in_dev->dev) );

	in_dev->dev->type = ARPHRD_INFINIBAND;
	in_dev->dev->addr_len = INFINIBAND_ALEN;
	memcpy( in_dev->dev->dev_addr, hw_addr, in_dev->dev->addr_len );
	memset( in_dev->dev->broadcast, 0xff, MAX_ADDR_LEN );

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
	kmem_free( neigh );
}

int ip_route_output_key(struct net *net, struct rtable **rt,
							struct flowi *flp)
{
	*rt = (struct rtable*) kmem_alloc( sizeof( **rt ) );

	if ( ! *rt ) return -1;

	(*rt)->idev = _ib0_in_device;
	(*rt)->rt_gateway = flp->nl_u.ip4_u.daddr;

	return 0;
}

void ip_rt_put(struct rtable * rt)
{
	kmem_free( rt );
}


struct net_device *ip_dev_find(struct net *net, __be32 addr)
{
	if ( _ib0_in_device && _ib0_in_device->ifa_address == addr ) {
		return _ib0_in_device->dev;
	}
    	return NULL;
}

int neigh_event_send(struct neighbour *neigh, struct sk_buff *skb)
{
	return 0;
}

void local_bh_disable(void)
{
	printk("local_bh_disable: not implemented\n");
}

void local_bh_enable(void)
{
	printk("local_bh_enable: not implemented\n");
}

void tasklet_init(struct tasklet_struct *t,
		  void (*func)(unsigned long), unsigned long data)
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func = func;
	t->data = data;
}


void tasklet_kill(struct tasklet_struct *t)
{
	if (in_interrupt())
		printk("Attempt to kill tasklet from interrupt\n");

	while (test_and_set_bit(TASKLET_STATE_SCHED, &t->state)) {
		do {
			yield();
		} while (test_bit(TASKLET_STATE_SCHED, &t->state));
	}
	tasklet_unlock_wait(t);
	clear_bit(TASKLET_STATE_SCHED, &t->state);
}

void __tasklet_hi_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	local_irq_save(flags);
	t->next = NULL;
	/* *__get_cpu_var(tasklet_hi_vec).tail = t;
	__get_cpu_var(tasklet_hi_vec).tail = &(t->next);
	raise_softirq_irqoff(HI_SOFTIRQ); */
	local_irq_restore(flags);
}
