
#ifndef PCT_DEBUG_H
#define PCT_DEBUG_H

#include <stdio.h>

#define Info(fmt, args... ) \
    printf( "Info: "fmt, ## args )

#if 0

#define Debug( name, fmt, args... ) \
    printf( "%s::%s():%d: "fmt, #name,__FUNCTION__,__LINE__, ## args )
#define Debug1(  fmt, args... ) \
    printf( "%s():%d: "fmt, __FUNCTION__,__LINE__, ## args )
#else
#define Debug( name, fmt, args... )
#define Debug1( fmt, args... )
#endif

#define Warn( name, fmt, args... ) \
    printf( "WARN:%s::%s():%d: "fmt, #name,__FUNCTION__,__LINE__, ## args )

#endif
