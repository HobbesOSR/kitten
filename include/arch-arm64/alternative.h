#ifndef __ARM64_ALTERNATIVE_H__
#define __ARM64_ALTERNATIVE_H__

/* JRL: 
 * We are just going to ignore all the workarounds, but leave the macros in  
 * place for documentation purposes. 
 */


/*
 * Usage: asm(ALTERNATIVE(oldinstr, newinstr, feature));
 *
 * Usage: asm(ALTERNATIVE(oldinstr, newinstr, feature, CONFIG_FOO));
 * N.B. If CONFIG_FOO is specified, but not selected, the whole block
 *      will be omitted, including oldinstr.
 */
#define ALTERNATIVE(orig, alt, ...) orig


#endif