
#include <srdma/dm.h>

int main(int argc, char *argv[] )
{


        srdma::Dm dm;
        srdma::ProcId id;
#if 0 
        id.pid = 65535;
        id.nid = 0xa000002;
#else
        id.pid = atoi(argv[1]);
        id.nid = 0xa000001;
#endif

        srdma::EndPoint* ep = dm.epFind( id );
	getchar();
	ep->disconnect();	
	getchar();
	delete ep;
	getchar();
}
