/*
 *  linux/lib/kasprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <stdarg.h>
#include <lwk/types.h>
#include <lwk/string.h>
#include <lwk/print.h>
#include <lwk/kmem.h>

/* Simplified asprintf. */
char *
kvasprintf(
	int			__unused(level),
	const char *		fmt,
	va_list			ap
)
{
	unsigned int len;
	char *p;
	va_list aq;

	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	p = kmem_alloc(len+1);
	if (!p)
		return NULL;

	vsnprintf(p, len+1, fmt, ap);

	return p;
}


char *
kasprintf(
	int			level,
	const char *		fmt,
	...
)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(level, fmt, ap);
	va_end(ap);

	return p;
}
