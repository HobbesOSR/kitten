#ifndef _X86_64_STRING_H_
#define _X86_64_STRING_H_

#ifdef __KERNEL__
/* Written 2002 by Andi Kleen */ 



/* Even with __builtin_ the compiler may decide to use the out of line
   function. */

#define __HAVE_ARCH_MEMCPY 1
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4
extern void *memcpy(void *to, const void *from, size_t len);
#else
extern void *__memcpy(void *to, const void *from, size_t len); 
#define memcpy(dst,src,len) \
	({ size_t __len = (len);				\
	   void *__ret;						\
	   if (__builtin_constant_p(len) && __len >= 64)	\
		 __ret = __memcpy((dst),(src),__len);		\
	   else							\
		 __ret = __builtin_memcpy((dst),(src),__len);	\
	   __ret; }) 
#endif

#define __HAVE_ARCH_MEMSET
void *memset(void *s, int c, size_t n);

#define __HAVE_ARCH_MEMMOVE
void * memmove(void * dest,const void *src,size_t count);

int memcmp(const void * cs,const void * ct,size_t count);
size_t strlen(const char * s);
char *strcpy(char * dest,const char *src);
char *strcat(char * dest, const char * src);
int strcmp(const char * cs,const char * ct);

#endif /* __KERNEL__ */

#endif
