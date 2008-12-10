/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LWK_IDSPACE_H
#define _LWK_IDSPACE_H

/**
 * Opaque ID space type.
 * An ID space consists of a range of IDs and keeps track of which are
 * allocated and which are available for allocation.
 */
struct idspace;

/**
 * Numeric identifier type.
 * Guaranteed to be able to contain a pointer.
 */
typedef unsigned int id_t;

/**
 * Used to request any available ID.
 */
#define ANY_ID ((id_t) (-1))

/**
 * Represents an error return from idspace_alloc_id().
 */
#define ERROR_ID (ANY_ID)

/**
 * Creates an ID space.
 *
 * \returns Pointer to ID space if successful, NULL if allocation failed.
 */
extern struct idspace *
idspace_create(
	id_t			min_id,
	id_t			max_id
);

/**
 * Destroys and ID space.
 */
extern void
idspace_destroy(
	struct idspace *	spc
);

/**
 * Allocates an ID from an ID space.
 *
 * Pass the specific ID to allocate in @request, or ANY_ID to request any
 * available ID.
 *
 * \returns The allocated ID if successful, ERROR_ID otherwise if there are
 *          no IDs left or if the requested ID has already been allocated.
 */
extern id_t
idspace_alloc_id(
	struct idspace *	spc,
	id_t			request
);

/**
 * Frees an ID previously allocated from an ID space.
 *
 * \returns 0 if successful, negative if there was an error.
 */
extern int
idspace_free_id(
	struct idspace *	spc,
	id_t			id
);

#endif
