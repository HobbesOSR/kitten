
#ifndef srdma_dbg_H
#define srdma_dbg_H

#include <stdio.h>

namespace srdma {

#define dbgFail(name, fmt, args...) \
printf("%s::%s():%i:FAILED: " fmt, #name, __FUNCTION__, __LINE__, ## args)

#if 0 
#define dbg(name, fmt, args...) \
printf("%s::%s():%i: " fmt, #name, __FUNCTION__, __LINE__, ## args)
#else
#define dbg(name, fmt, args...) 
#endif

} // namespace srdma

#endif
