#include <srdma/dm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void server(srdma::Dm &);
void client(srdma::Dm &, srdma::ProcId);

int count = 1024 * 1024 * 100;

int main(int argc, char *argv[])
{
	srdma::Dm f;

	if (argc == 1) {
		server(f);
	} else {
		srdma::ProcId id;
		id.pid = atoi(argv[1]);
		id.nid = 0xa0000fe;
		client(f, id);
	}
	dbg(, "done\n");
	getchar();
	return 0;
}

void server(srdma::Dm & dm)
{
	unsigned char *buf = new unsigned char[count];

	memset(buf, 0, count);
	dm.memRegionRegister(buf, count, 0xdead);
	buf[count - 1] = 0xf0;
	while (1) {

		srdma::Event event;
		memset(&event, 0, sizeof(event));
		while (dm.memRegionGetEvent(0xdead, event) <= 0) ;

		printf("op=%d addr=%p length=%lu\n",
		       event.op, event.addr, event.length);

		++buf[count - 1];
	}
}

void client(srdma::Dm & dm, srdma::ProcId serverId)
{
	unsigned char *buf = new unsigned char[count];

	dm.memRead(serverId, 0xdead, 0, buf, count);
	printf("buf=%#x\n", buf[count - 1]);
	getchar();

	dm.memRead(serverId, 0xdead, 0, buf, count);
	printf("buf=%#x\n", buf[count - 1]);
	getchar();

	dm.memRead(serverId, 0xdead, 0, buf, count);
	printf("buf=%#x\n", buf[count - 1]);
}
