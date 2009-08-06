/* Never include this file directly.  Include <lwk/compiler.h> instead.  */

/* These definitions are for GCC v4.x.  */
#include <lwk/compiler-gcc.h>

#ifdef CONFIG_FORCED_INLINING
# undef inline
# undef __inline__
# undef __inline
# define inline			inline		__attribute__((always_inline))
# define __inline__		__inline__	__attribute__((always_inline))
# define __inline		__inline	__attribute__((always_inline))
#endif

#define __attribute_used__	__attribute__((__used__))
#define __must_check 		__attribute__((warn_unused_result))
#define __compiler_offsetof(a,b) __builtin_offsetof(a,b)
#define __always_inline		inline __attribute__((always_inline))

/*
 * A trick to suppress uninitialized variable warning without generating any
 * code
 */
#define uninitialized_var(x) x = x
