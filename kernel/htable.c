/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/list.h>
#include <lwk/htable.h>
#include <lwk/idspace.h>
#include <lwk/hash.h>

struct htable {
	ht_hash_func_t		hash;
	ht_keys_equal_func_t	keys_equal;
	size_t			obj_key_offset;
	size_t			obj_hlist_node_offset;
	size_t			num_entries;
	size_t			order;
	struct hlist_head	tbl[0];
};

static void *
obj2key(const struct htable *ht, const void *obj)
{
	return (void *)((uintptr_t)obj + ht->obj_key_offset);
}

static struct hlist_node *
obj2node(const struct htable *ht, const void *obj)
{
	return (struct hlist_node *)((uintptr_t)obj + ht->obj_hlist_node_offset);
}

static void *
node2obj(const struct htable *ht, const struct hlist_node *node)
{
	return (void *)((uintptr_t)node - ht->obj_hlist_node_offset);
}

static void *
node2key(const struct htable *ht, const struct hlist_node *node)
{
	return obj2key(ht, node2obj(ht, node));
}

static struct hlist_head *
key2head(struct htable *ht, void *key)
{
	return &ht->tbl[ht->hash(key, ht->order)];
}

static struct hlist_head *
obj2head(struct htable *ht, const void *obj)
{
	return &ht->tbl[ht->hash(obj2key(ht, obj), ht->order)];
}

struct htable *
ht_create(
	size_t			order,
	size_t			obj_key_offset,
	size_t			obj_hlist_node_offset,
	ht_hash_func_t		hash,
	ht_keys_equal_func_t	keys_equal
)
{
	if (!hash || !keys_equal)
		return NULL;

	struct htable *ht;
	size_t ht_struct_size = sizeof(*ht) + (1 << order) * sizeof(ht->tbl[0]);

	ht = kmem_alloc(ht_struct_size);
	if (!ht)
		return NULL;

	ht->order			= order;
	ht->obj_key_offset		= obj_key_offset;
	ht->obj_hlist_node_offset	= obj_hlist_node_offset;
	ht->hash			= hash;
	ht->keys_equal			= keys_equal;
	ht->num_entries			= 0;

	return ht;
}

int
ht_destroy(
	struct htable *		ht
)
{
	if (ht->num_entries)
		return -EEXIST;
	kmem_free(ht);
	return 0;
}

int
ht_add(
	struct htable *		ht,
	void *			obj
)
{
	hlist_add_head(obj2node(ht, obj), obj2head(ht, obj));
	++ht->num_entries;
	return 0;
}

int
ht_del(
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
ht_lookup(
	struct htable *		ht,
	void *			key
)
{
	struct hlist_node *node;
	hlist_for_each(node, key2head(ht, key)) {
		if (ht->keys_equal(key, node2key(ht, node)))
			return node2obj(ht, node);
	}
	return NULL;
}

uint64_t
ht_hash_id(
	void *			key,
	size_t			order
)
{
	id_t *id = key;
	return hash_long(*id, order);
}

bool
ht_id_keys_equal(
	void *			key1,
	void *			key2
)
{
	id_t *id1 = key1;
	id_t *id2 = key2;
	return (*id1 == *id2);
}
