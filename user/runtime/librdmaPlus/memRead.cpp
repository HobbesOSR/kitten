#include <rdma++.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void server(rdma::rdma &);
void client(rdma::rdma &, rdma::ProcId);

int count = 1024 * 1024 * 100;
//int count = 1024;// * 1024 * 100;

int main(int argc, char *argv[])
{
	rdma::rdma f;

	if (argc == 1) {
		server(f);
	} else {
		rdma::ProcId id;
		id.pid = atoi(argv[1]);
		id.nid = 0xa0000fe;
		client(f, id);
	}
	dbg(, "done\n");
	getchar();
	return 0;
}

void server(rdma::rdma & dm)
{
	unsigned char *buf = new unsigned char[count];

	memset(buf, 0, count);
    dbg(,"buf=%p\n",buf);
	dm.memRegionRegister(buf, count, 0xad);
	buf[count - 1] = 0xf0;
	while (1) {

		rdma::Event event;
		memset(&event, 0, sizeof(event));
		while (dm.memRegionGetEvent(0xad, event) <= 0) ;

		printf("op=%d addr=%p length=%lu\n",
		       event.op, event.addr, event.length);

		++buf[count - 1];
	}
}

void client( rdma::rdma & dm, rdma::ProcId serverId)
{
	unsigned char *buf = new unsigned char[count];

    dm.connect( serverId );
	dm.memRead(serverId, 0xad, 0, buf, count);
	printf("buf=%#x\n", buf[count - 1]);
	getchar();

	dm.memRead(serverId, 0xad, 0, buf, count);
	printf("buf=%#x\n", buf[count - 1]);
	getchar();

	dm.memRead( serverId, 0xad, 0, buf, count);
	printf("buf=%#x\n", buf[count - 1]);
    dm.disconnect( serverId );
}
