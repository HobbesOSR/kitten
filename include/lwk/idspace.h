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
 * Used to request any available ID... pass as 'request' arg to id_alloc().
 */
#define ANY_ID (-1)

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

extern int
idspace_destroy(
	struct idspace *	spc
);

extern int
idspace_alloc_id(
	struct idspace *	spc,
	id_t			request,
	id_t *			id
);

extern int
idspace_free_id(
	struct idspace *	spc,
	id_t			id
);

#endif
