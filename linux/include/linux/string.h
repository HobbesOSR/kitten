#ifndef __LINUX_STRING_H
#define __LINUX_STRING_H

#include <lwk/string.h>

extern char *kstrdup(const char *s, gfp_t gfp);
extern char *kstrndup(const char *s, size_t len, gfp_t gfp);
extern void *kmemdup(const void *src, size_t len, gfp_t gfp);

#endif
