/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LWK_IDSPACE_H
#define _LWK_IDSPACE_H

/**
 * ID space object.
 * An ID space consists of a range of IDs and keeps track of which are
 * allocated and which are available for allocation.
 */
typedef void * idspace_t;

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
 * ID space API.
 */
int idspace_create(id_t min_id, id_t max_id, idspace_t *idspace);
int idspace_destroy(idspace_t idspace);
int idspace_alloc_id(idspace_t idspace, id_t request, id_t *id);
int idspace_free_id(idspace_t idspace, id_t id);

#endif
