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
htable_create(
	size_t			order,
	size_t			obj_key_offset,
	size_t			obj_hlist_node_offset,
	ht_hash_func_t		hash,
	ht_keys_equal_func_t	keys_equal
);

extern int
htable_destroy(
	struct htable *		ht
);

extern int
htable_add(
	struct htable *		ht,
	void *			obj
);

extern int
htable_del(
	struct htable *		ht,
	void *			obj
);

extern void *
htable_lookup(
	struct htable *		ht,
	void *			key
);

uint64_t
htable_hash_id(
	void *			key,
	size_t			order
);

bool
htable_id_keys_equal(
	void *			key1,
	void *			key2
);

#endif
