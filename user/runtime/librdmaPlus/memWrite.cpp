#include <rdma++.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//using namespace rdma;

void server(rdma::rdma &);
void client(rdma::rdma &, rdma::ProcId);

int count = 1024 * 1024 * 100;

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
	dm.memRegionRegister(buf, count, 0xad );
	while (1) {

		printf("waiting\n");
		while (!buf[count - 1]) {
			;
		}
		printf("buf=%#x\n", buf[count - 1]);

		rdma::Event event;
		memset(&event, 0, sizeof(event));
		while ( dm.memRegionGetEvent( 0xad, event) <= 0) ;

		printf("op=%d addr=%p length=%lu\n",
		       event.op, event.addr, event.length);

		for (int i = 0; i < count - 1; i++) {
			if (buf[i] != (i & 0xff)) {
				printf("error i=%d &0xff%d %d\n", i,
				       i & 0xff, buf[i]);
				exit(1);
			}
		}
		memset(buf, 0, count);
	}
}

void client( rdma::rdma & dm, rdma::ProcId serverId)
{
	unsigned char *buf = new unsigned char[count];

    dm.connect(serverId);
	for (int i = 0; i < count; i++) {
		buf[i] = i;
	}
	buf[count - 1] = 0xf0;
	dm.memWrite(serverId, 0xad, 0, buf, count);
	getchar();
	buf[count - 1] = 0xf1;
	dm.memWrite(serverId, 0xad, 0, buf, count);
	getchar();
	buf[count - 1] = 0xf2;
	dm.memWrite(serverId, 0xad, 0, buf, count);
    dm.disconnect(serverId);
}
