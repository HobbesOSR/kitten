#ifndef srdma_msgHdr_H
#define srdma_msgHdr_H

#include <dat2/udat.h>
#include <srdma/types.h>
#include <srdma/mem.h>

namespace srdma {

struct MsgHdr {
	enum Type { REQ=1, ACK, RMR_REQ, RMR_RESP, RDMA_RESP } type;

	DAT_DTO_COOKIE cookie;

	int count; // for debug

	union {
		struct {
			Tag tag;
			Size	length;
			DAT_RMR_TRIPLET rmr_triplet;
		} req;

		struct {
		} ack;

		struct {
			MemRegion::Id id;
		} rmrReq;

		struct {
			DAT_RMR_TRIPLET rmr;
		} rmrResp;

		struct {
			MemOp op;
			MemRegion::Id id;
			MemRegion::Addr addr;
			Size length;
		} rdmaResp;
	} u;
};

struct Msg {
	static const unsigned int ShortMsgLength = 512 - sizeof(MsgHdr);
	struct MsgHdr hdr;
	unsigned char body[ShortMsgLength];
};

} // namespace srdma

#endif
