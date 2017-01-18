#ifndef _CHECKS_H
#define _CHECKS_H (1)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * FIXME: Add MPI rank to the printf's to stderr
 */

#define SYSCALL_CHECK( arg )                                            \
{                                                                       \
        int _rc = ( arg );                                              \
        if( ( _rc ) != ( 0 ) ){                                         \
                fprintf( stderr, "%s:%d ERROR\n",                       \
                                 __FILE__, __LINE__ );                  \
                perror( #arg " Call Failed" );                          \
                abort();                                                \
        }                                                               \
}

#define NULL_CHECK( arg )                                               \
{                                                                       \
        void *_rc = ( arg );                                            \
                                                                        \
        if ( ( _rc ) == NULL ){                                         \
                fprintf( stderr, "%s:%d -- " #arg                       \
                                " -- Return Failed\n",                  \
                                __FILE__, __LINE__                      \
                );                                                      \
        }                                                               \
}

#define MPI_CHECK( arg )                                                \
if( ( arg ) != MPI_SUCCESS ){                                           \
        fprintf( stderr, "%s:%d -- " #arg " Call Failed\n",             \
                         __FILE__, __LINE__                             \
        );                                                              \
}

#ifdef WITH_DEBUG

#define DPRINTF( fmt, ... )                                             \
do{                                                                     \
        fprintf( stderr, "%s:%d\t " fmt, __FILE__,                      \
                         __LINE__, ##__VA_ARGS__ );                     \
        fflush( NULL );                                                 \
}while( 0 )

#else /* WITH_DEBUG */

#define DPRINTF( fmt, ... ) do{}while( 0 )

#endif /* WITH_DEBUG */


#define PRINTF( fmt, ... )                                              \
fprintf( stderr, fmt, ##__VA_ARGS__ )

#define IPRINTF( fmt, ... )                                             \
fprintf( stderr, "%s:%d\t " fmt, __FILE__,                              \
                 __LINE__, ##__VA_ARGS__ )


/*
#define EPRINTF( fmt, ... )                                             \
{                                                                       \
        fprintf( stderr, "%s:%d\t **** " fmt " ****",                   \
                         __FILE__, __LINE__, ##__VA_ARGS__ ); \
        fflush( NULL );                                                 \
        PRINT_BTRACE();                                                 \
        fflush( NULL );                                                 \
        abort();                                                        \
}
*/

#define KPRINTF( fmt, ... ) do{}while( 0 )

#endif /* _CHECKS_H */
