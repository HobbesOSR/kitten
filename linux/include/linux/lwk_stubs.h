#ifndef __LINUX_LWK_STUBS_H
#define __LINUX_LWK_STUBS_H

#include <lwk/print.h>

#define TRUE 1
#define FALSE 0
#define LINUX_DBG(flag, fmt, args...) \
{\
    if ( flag ) {\
        print("HALTING:%s:%s():%i: " fmt, __FILE__,__FUNCTION__, __LINE__, ## args);\
    while(1);\
    } else {\
        print("%s:%s():%i: " fmt, __FILE__,__FUNCTION__, __LINE__, ## args);\
    }\
}

#endif
