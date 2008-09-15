/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LWK_HTABLE_H
#define _LWK_HTABLE_H

#include <lwk/idspace.h>

/**
 * Hash table object.
 */
typedef void * htable_t;

/**
 * Hash table API.
 */
extern int htable_create(size_t tbl_order,
                         size_t obj_key_offset, size_t obj_link_offset,
                         htable_t *tbl);
extern int htable_destroy(htable_t tbl);
extern int htable_add(htable_t tbl, void *obj);
extern int htable_del(htable_t tbl, void *obj);
extern void *htable_lookup(htable_t tbl, id_t key);

#endif
