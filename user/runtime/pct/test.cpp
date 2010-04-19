
#include <srdma/dm.h>

int main(int argc, char *argv[] )
{
        srdma::Dm dm;
        srdma::ProcId id = { -1, -1};

	printf("%d\n",getpid());
	char buf[100];
        srdma::Status status;
	dm.recv(buf,100, id, 0xdead, &status );
	printf("%s\n",buf);
}
