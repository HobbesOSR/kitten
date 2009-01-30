#ifndef _LWK_PALACIOS_H
#define _LWK_PALACIOS_H

#include <lwk/types.h>

extern int
v3_start_guest(
	paddr_t		iso_start,
	size_t		iso_size
);

#endif
