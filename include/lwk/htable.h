/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LWK_HTABLE_H
#define _LWK_HTABLE_H

#include <lwk/types.h>

/**
 * Opaque hash table type.
 */
struct htable;

/**
 * Hash function type.
 * Converts the input key to an index in the hash table.
 */
typedef uint64_t (*ht_hash_func_t)(void *key, size_t order);

/**
 * Keys equal function type.
 * Returns true if the two keys passed in are equal, false otherwise.
 */
typedef bool (*ht_keys_equal_func_t)(void *key1, void *key2);

extern struct htable *
ht_create(
	size_t			order,
	size_t			obj_key_offset,
	size_t			obj_hlist_node_offset,
	ht_hash_func_t		hash,
	ht_keys_equal_func_t	keys_equal
);

extern int
ht_destroy(
	struct htable *		ht
);

extern int
ht_add(
	struct htable *		ht,
	void *			obj
);

extern int
ht_del(
	struct htable *		ht,
	void *			obj
);

extern void *
ht_lookup(
	struct htable *		ht,
	void *			key
);

uint64_t
ht_hash_id(
	void *			key,
	size_t			order
);

bool
ht_id_keys_equal(
	void *			key1,
	void *			key2
);

#endif
