/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LWK_HTABLE_H
#define _LWK_HTABLE_H

#include <lwk/types.h>

/** Opaque hash table type */
struct htable;

/** Opaque hash table chaining type */
struct hlist_node;


/**
 * \file Hash table API.
 *
 * Hash tables are implemented in a generic way to allow them to
 * contain arbitrary structures with chaining.  To create a structure
 * that is compatible with the hash table it must contain an lwk_id_t id
 * and a struct hlist_node for chaining, and pass the offsets of these
 * values to the htable_create() function as shown in this example:
 *
 * \code
 * #include <lwk/htable.h>
 * #include <lwk/list.h>
 *
 * struct foo {
 *	lwk_id_t		id;
 *	struct hlist_node	ht_link;
 *	// Other stuff ...
 * };
 *
 * struct htable * foo_table;
 *
 * void foo_init( void )
 * {
 *	foo_table = htable_create(
 *		7,
 *		offsetof( struct foo, id ),
 *		offsetof( struct foo, ht_link ),
 *		0 // default hash function used
 *	);
 *
 *	if( !foo_table )
 *		panic( "Unable to create foo_table!" );
 * }
 * \endcode
 *
 * It is possible to substitute a different has function to hash
 * strings or other objects.
 *
 * \todo write documentation on replacing hash function
 */


/**
 * Hash function type.
 * Converts the input key to an index in the hash table.
 */
typedef uint64_t (*ht_hash_func_t)( const void *key, size_t order);

/**
 * Keys equal function type.
 * \returns 0 if the keys are equal, < 0 if key1 < key2 and >0 if key1 > key2
 */
typedef int (*ht_key_compare_func_t)(
	const void *key_in_search,
	const void *key_in_table
);


/** Create a hash table.
 *
 * If @hash is NULL, the default hash_long() will be used instead.
 *
 * \returns Pointer to table if successful, NULL if allocation failed.
 */
extern struct htable *
htable_create(
	size_t			order,
	size_t			obj_key_offset,
	size_t			obj_hlist_node_offset,
	ht_hash_func_t		hash,
	ht_key_compare_func_t	key_compare
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
	const void *		key
);

extern uint64_t
htable_hash_id(
	const void *		key,
	size_t			order
);

extern int
htable_id_key_compare(
	const void *		key1,
	const void *		key2
);

#endif
