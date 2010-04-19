
#include <srdma/dm.h>

#define BUF_LEN 100
int main(int argc, char *argv[] )
{
	srdma::Status status;
        srdma::Dm dm;
        srdma::ProcId id;
        id.pid = atoi(argv[1]);
        id.nid = 0xa0000fe;

	char buf[BUF_LEN];
	strcpy(buf, "hello world" );
	
	dm.send(buf, BUF_LEN, id, 0xdead, &status );
	getchar();
}
