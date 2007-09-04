#ifndef _LWK_INIT_H
#define _LWK_INIT_H

#include <lwk/compiler.h>

/* These macros are used to mark some functions or 
 * initialized data (doesn't apply to uninitialized data)
 * as `initialization' functions. The kernel can take this
 * as hint that the function is used only during the initialization
 * phase and free up used memory resources after
 *
 * Usage:
 * For functions:
 * 
 * You should add __init immediately before the function name, like:
 *
 * static void __init initme(int x, int y)
 * {
 *    extern int z; z = x * y;
 * }
 *
 * If the function has a prototype somewhere, you can also add
 * __init between closing brace of the prototype and semicolon:
 *
 * extern int initialize_foobar_device(int, int, int) __init;
 *
 * For initialized data:
 * You should insert __initdata between the variable name and equal
 * sign followed by value, e.g.:
 *
 * static int init_variable __initdata = 0;
 * static char linux_logo[] __initdata = { 0x32, 0x36, ... };
 *
 * Don't forget to initialize data not at file scope, i.e. within a function,
 * as gcc otherwise puts the data into the bss section and not into the init
 * section.
 * 
 * Also note, that this data cannot be "const".
 */

/* These are for everybody (although not all archs will actually
   discard it in modules) */
#define __init		__attribute__ ((__section__ (".init.text")))
#define __initdata	__attribute__ ((__section__ (".init.data")))
#define __exitdata	__attribute__ ((__section__(".exit.data")))
#define __exit_call	__attribute_used__ __attribute__ ((__section__ (".exitcall.exit")))
#define __exit		__attribute_used__ __attribute__ ((__section__(".exit.text")))
#define __cpuinitdata	__initdata
#define __cpuinit	__init

/* For assembly routines */
#define __INIT		.section	".init.text","ax"
#define __FINIT		.previous
#define __INITDATA	.section	".init.data","aw"

#ifndef __ASSEMBLY__

/* Defined in init/main.c */
#define COMMAND_LINE_SIZE       1024
extern char lwk_command_line[COMMAND_LINE_SIZE];

/* used by init/main.c */
extern void setup_arch(char **);

extern void init(void);

#endif /* !__ASSEMBLY__ */
  
#endif /* _LWK_INIT_H */
