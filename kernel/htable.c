/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/list.h>
#include <lwk/htable.h>
#include <lwk/hash.h>

struct htable {
	size_t              tbl_order;
	struct hlist_head * tbl;
	size_t              obj_key_offset;
	size_t              obj_link_offset;
	size_t              num_entries;
	hash_func_t		hash;
};


/** Return the key used for an object in a hash table */
static inline lwk_id_t
obj2key(const struct htable *ht, const void *obj)
{
	return *((lwk_id_t *)((uintptr_t)obj + ht->obj_key_offset));
}

/** Return the containing node for an object in the hash table */
static inline struct hlist_node *
obj2node(const struct htable *ht, const void *obj)
{
	return (struct hlist_node *)((uintptr_t)obj + ht->obj_link_offset);
}

/** Return the object contained in the node */
static inline void *
node2obj(const struct htable *ht, const struct hlist_node *node)
{
	return (void *)((uintptr_t)node - ht->obj_link_offset);
}

/** Return the key used for a node in the hash table */
static inline lwk_id_t
node2key(const struct htable *ht, const struct hlist_node *node)
{
	return obj2key(ht, node2obj(ht, node));
}

/** Return the first item in the chain for the given key */
static inline struct hlist_head *
key2head(const struct htable *ht, lwk_id_t key)
{
	const uint64_t index = ht->hash
		? ht->hash( key, ht->tbl_order )
		: hash_long( key, ht->tbl_order );
	return &ht->tbl[ index ];
}

/** Return the first item in the chain for a given object */
static inline struct hlist_head *
obj2head(const struct htable *ht, const void *obj)
{
	return key2head( ht, obj2key( ht, obj ) );
}


struct htable *
htable_create(
	size_t			tbl_order,
	size_t			obj_key_offset,
	size_t			obj_link_offset,
	hash_func_t		hash
)
{
	size_t tbl_size = 1 << tbl_order;

	struct htable *ht = kmem_alloc(sizeof(*ht));
	if( !ht )
		goto fail_ht_alloc;

	ht->tbl = kmem_alloc(tbl_size * sizeof(struct hlist_head));

	if( !ht->tbl )
		goto fail_tbl_alloc;

	ht->tbl_order		= tbl_order;
	ht->obj_key_offset	= obj_key_offset;
	ht->obj_link_offset	= obj_link_offset;
	ht->num_entries		= 0;
	ht->hash		= hash;

	return ht;

	kmem_free( ht->tbl );
fail_tbl_alloc:
	kmem_free( ht );
fail_ht_alloc:
	return NULL;
}


int
htable_destroy(
	struct htable *		ht
)
{
	if (ht->num_entries)
		return -EEXIST;
	kmem_free(ht->tbl);
	kmem_free(ht);
	return 0;
}


int
htable_add(
	struct htable *		ht,
	void *			obj
)
{
	hlist_add_head(obj2node(ht, obj), obj2head(ht, obj));
	++ht->num_entries;
	return 0;
}


int
htable_del(
	struct htable *		ht,
	void *			obj
)
{
	struct hlist_node *node;
	hlist_for_each(node, obj2head(ht, obj)) {
		if (obj == node2obj(ht, node)) {
			hlist_del(node);
			--ht->num_entries;
			return 0;
		}
	}

	return -ENOENT;
}


void *
htable_lookup(
	struct htable *		ht,
	lwk_id_t			key
)
{
	struct hlist_node *node;
	hlist_for_each(node, key2head(ht, key)) {
		if (key == node2key(ht, node))
			return node2obj(ht, node);
	}
	return NULL;
}
