
#include <srdma/dm.h>

#define BUF_LEN 100
int main(int argc, char *argv[] )
{
        srdma::Dm dm;
        srdma::ProcId id = { -1, -1};

	printf("%d\n",getpid());
	char buf[BUF_LEN];
        srdma::Status status;
	while(1) {
		printf("wait\n");
//		getchar();
		dm.recv(buf, BUF_LEN, id, 0xdead, &status );
		printf("%s\n",buf);
	}
}
