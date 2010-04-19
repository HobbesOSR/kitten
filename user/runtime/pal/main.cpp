
#include <pal/pal.h>
#include <pal/config.h>

#include <signal.h>

static Pal pal;

static void sighandler( int sig )
{
    pal.Sighandler( sig );
}

int main(int argc, char *argv[] )
{
    signal( SIGINT, sighandler );

    Config config( argc, argv );

    return pal.Run( config );
}
