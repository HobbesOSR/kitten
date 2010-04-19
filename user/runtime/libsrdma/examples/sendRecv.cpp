#include <srdma/dm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void server(srdma::Dm &);
void client(srdma::Dm &, srdma::ProcId);

int count = 200;

int main(int argc, char *argv[])
{
	srdma::Dm f;

	if (argc == 1) {
		server(f);
	} else {
		srdma::ProcId id;
		id.pid = atoi(argv[1]);
		id.nid = 0x0a000001;
		client(f, id);
	}
	dbg(, "done\n");
	getchar();
	return 0;
}

void server(srdma::Dm & dm)
{
	unsigned char *buf = new unsigned char[count];

	srdma::ProcId id;
	id.nid = -1;
	id.pid = -1;
	srdma::Status status;
	while (1) {
		memset(buf, 0, count);
		printf("waiting\n");
		dm.recv(buf, count, id, 0xdeadbeef, &status);
		printf("count=%lu tag=%#x\n", status.key.count, status.key.tag);
		printf("nid=%#x pid=%d\n", status.key.id.nid,
		       status.key.id.pid);
		for (int i = 0; i < count; i++) {
			if (buf[i] != i) {
				printf("%d != %d\n", i, buf[i]);
			}
		}
		dm.send(buf, count, status.key.id, 0xdeadbeef, &status);
	}
}

void client(srdma::Dm & dm, srdma::ProcId serverId)
{
	unsigned char *sbuf = new unsigned char[count];
	unsigned char *rbuf = new unsigned char[count];

	memset(rbuf, 0, count);
	for (int i = 0; i < count; i++) {
		sbuf[i] = i;
	}
	srdma::Status status;
	srdma::Request req;
	srdma::ProcId id;
	id.nid = -1;
	id.pid = -1;
#if 1
	if (dm.irecv(rbuf, count, id, 0xdeadbeef, req)) {
		printf("iiiiiii\n");
		exit(-1);
	}
#endif
	dm.send(sbuf, count, serverId, 0xdeadbeef, &status);
#if 1
	dm.wait(req, srdma::Dm::Timeout_Infinite, &status);
	printf("count=%lu tag=%#x\n", status.key.count, status.key.tag);
	printf("nid=%#x pid=%d\n", status.key.id.nid, status.key.id.pid);
	for (int i = 0; i < count; i++) {
		if (rbuf[i] != i) {
			printf("%d != %d\n", i, rbuf[i]);
		}
	}
#endif
}
