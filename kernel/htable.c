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
};

static id_t
obj2key(const struct htable *ht, const void *obj)
{
	return *((id_t *)((uintptr_t)obj + ht->obj_key_offset));
}

static struct hlist_node *
obj2node(const struct htable *ht, const void *obj)
{
	return (struct hlist_node *)((uintptr_t)obj + ht->obj_link_offset);
}

static void *
node2obj(const struct htable *ht, const struct hlist_node *node)
{
	return (void *)((uintptr_t)node - ht->obj_link_offset);
}

static id_t
node2key(const struct htable *ht, const struct hlist_node *node)
{
	return obj2key(ht, node2obj(ht, node));
}

static struct hlist_head *
key2head(const struct htable *ht, id_t key)
{
	return &ht->tbl[hash_long(key, ht->tbl_order)];
}

static struct hlist_head *
obj2head(const struct htable *ht, const void *obj)
{
	return &ht->tbl[hash_long(obj2key(ht, obj), ht->tbl_order)];
}

int
htable_create(size_t tbl_order,
              size_t obj_key_offset, size_t obj_link_offset, htable_t *tbl)
{
	struct htable *ht;
	size_t tbl_size;

	if (!(ht = kmem_alloc(sizeof(*ht))))
		return -ENOMEM;

	ht->tbl_order = tbl_order;
	tbl_size = (1 << tbl_order);

	if (!(ht->tbl = kmem_alloc(tbl_size * sizeof(struct hlist_head))))
		return -ENOMEM;

	ht->obj_key_offset  = obj_key_offset;
	ht->obj_link_offset = obj_link_offset;
	ht->num_entries     = 0;

	*tbl = ht;
	return 0;
}

int
htable_destroy(htable_t tbl)
{
	struct htable *ht = tbl;
	if (ht->num_entries)
		return -EEXIST;
	kmem_free(ht->tbl);
	kmem_free(ht);
	return 0;
}

int
htable_add(htable_t tbl, void *obj)
{
	struct htable *ht = tbl;
	hlist_add_head(obj2node(ht, obj), obj2head(ht, obj));
	++ht->num_entries;
	return 0;
}

int
htable_del(htable_t tbl, void *obj)
{
	struct htable *ht = tbl;
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
htable_lookup(htable_t tbl, id_t key)
{
	struct htable *ht = tbl;
	struct hlist_node *node;
	hlist_for_each(node, key2head(ht, key)) {
		if (key == node2key(ht, node))
			return node2obj(ht, node);
	}
	return NULL;
}
