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
	dm.memRegionRegister(buf, count, 0xdead );
	while (1) {

		printf("waiting\n");
		while (!buf[count - 1]) {
			;
		}
		printf("buf=%#x\n", buf[count - 1]);

		srdma::Event event;
		memset(&event, 0, sizeof(event));
		while ( dm.memRegionGetEvent( 0xdead, event) <= 0) ;

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

void client(srdma::Dm & dm, srdma::ProcId serverId)
{
	unsigned char *buf = new unsigned char[count];

	for (int i = 0; i < count; i++) {
		buf[i] = i;
	}
	buf[count - 1] = 0xf0;
	dm.memWrite(serverId, 0xdead, 0, buf, count);
	getchar();
	buf[count - 1] = 0xf1;
	dm.memWrite(serverId, 0xdead, 0, buf, count);
	getchar();
	buf[count - 1] = 0xf2;
	dm.memWrite(serverId, 0xdead, 0, buf, count);
}
