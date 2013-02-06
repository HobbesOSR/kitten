#include <lwk/smp.h>

/*
 * Call a function on all other processors
 */
int smp_call_function(void(*func)(void *info), void *info, int wait);
