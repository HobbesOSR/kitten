#ifndef _LWK_SORT_H
#define _LWK_SORT_H

#include <lwk/types.h>

void sort(void *base, size_t num, size_t size,
	  int (*cmp)(const void *, const void *),
	  void (*swap)(void *, void *, int));

#endif
