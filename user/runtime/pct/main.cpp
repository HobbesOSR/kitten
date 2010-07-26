#include <sys/utsname.h>
#include <pct/pct.h>

int main(int argc, char *argv[] )
{
    struct utsname name;
    uname( &name );

    printf("%s: %s %s %s %s %s\n", argv[0],
                                    name.sysname,
                                    name.nodename,
                                    name.release,
                                    name.version,
                                    name.machine);

    Pct pct;
    
    return pct.Go();
}
