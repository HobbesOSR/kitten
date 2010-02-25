#include <linux/arp_table.h>
#include <linux/byteorder.h>
#include <lwk/string.h>
#include <lwk/kmem.h>

static int init( struct arp_table* tbl, __u32 netmask )
{
	tbl->mask = ~netmask;

	tbl->private = kmem_alloc( tbl->mask + 1 * sizeof(hw_addr_t) );
	if ( !tbl->private ) return -1;
	return 0;
}

static int fini( struct arp_table* tbl )
{
	if ( tbl->private ) {
		kmem_free( tbl->private );
	}
	return 0;
}

static int set( struct arp_table* tbl, __be32 addr, hw_addr_t* hw_addr )
{
	int index = be32_to_cpu( addr ) & tbl->mask;

	memcpy( ((hw_addr_t*)tbl->private)[index], hw_addr, sizeof(*hw_addr) );
	return 0;
}

static int delete( struct arp_table* tbl, __be32 addr )
{
	return 0;
}

static hw_addr_t* get( struct arp_table* tbl, __be32 addr )
{
	int index = be32_to_cpu( addr ) & tbl->mask;
	return &((hw_addr_t*)tbl->private)[index];
}

struct arp_table _arp_table_simple = {
	.init = init,
	.fini = fini,
	.set = set,
	.delete = delete,
	.get = get,
	.private = NULL,
	.mask = 0
};
