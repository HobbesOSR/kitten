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
 * that is compatible with the hash table it must contain an id_t id
 * and a struct hlist_node for chaining, and pass the offsets of these
 * values to the htable_create() function as shown in this example:
 *
 * \code
 * #include <lwk/htable.h>
 * #include <lwk/list.h>
 *
 * struct foo {
 *	id_t		id;
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
 *		htable_id_hash,
 *		htable_id_key_compare
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
 *
 * Converts the input key to an index in the hash table.
 * \returns a 64-bit hash of the key with a value less than 2^@order.
 */
typedef uint64_t (*ht_hash_func_t)(
	const void *		key,
	size_t			order
);

/**
 * Keys comparison function type.
 *
 * \note The key comparison function should be stable for
 * forward-compatability, but it is not required right now.
 *
 * \returns 0 if the keys are equal,
 * negative if @key_in_search < @key_in_table
 * positive if @key_in_search > @key_in_table
 */
typedef int (*ht_key_compare_func_t)(
	const void *		key_in_search,
	const void *		key_in_table
);


/** Create a hash table.
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
htable_id_hash(
	const void *		key,
	size_t			order
);

extern int
htable_id_key_compare(
	const void *		key1,
	const void *		key2
);


struct htable_iter
{
	struct htable *		ht;
	struct hlist_node *	node;
	int			index;
};


extern struct htable_iter
htable_iter(
	struct htable *		ht
);

extern void *
htable_next(
	struct htable_iter *	iter
);


#endif
