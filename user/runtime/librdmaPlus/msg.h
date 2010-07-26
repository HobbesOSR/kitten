#ifndef _rdma_msg_H
#define _rdma_msg_H

#include <types.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <infiniband/verbs.h>
#include <memRegion.h>

namespace rdmaPlus {

struct MsgHdr {
	typedef enum { Req=1, Ack, RdmaResp } Type;

    Type type;
    uint64_t    cookie;
	union {
		struct {
			Tag tag;
			Size	length;
            uint32_t rkey;
            uint64_t raddr;
		} req;

		struct {
			memOp_t op;
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

class MsgObj {
    public:

        MsgObj( struct ibv_pd* pd, MsgHdr::Type type, uint64_t cookie = 0,
                        uint32_t rkey = 0, uint64_t raddr = 0,
                        Size length = 0, Tag tag = 0 ) 
        {
            m_msg.hdr.type = type;
            m_mr = ibv_reg_mr( pd, &m_msg, sizeof(m_msg),
                                                (ibv_access_flags) 0 );   
            if ( ! m_mr ) {
                terminal( MsgObj, "ibv_reg_mr() %s\n", strerror(errno));
            }

            m_msg.hdr.cookie = cookie;
            if ( type == MsgHdr::Req ) {
                m_msg.hdr.u.req.tag = tag;
                m_msg.hdr.u.req.rkey = rkey;
                m_msg.hdr.u.req.raddr = raddr;
                m_msg.hdr.u.req.length = length;
            }
        } 

        ~MsgObj( )
        {
            if (ibv_dereg_mr( m_mr ) ) {
                terminal( MsgObj, "ibv_reg_mr()\n");
            }
        }

        Msg& msg() { return m_msg; }
        u_int32_t   lkey() { return m_mr->lkey; }

    private:
        struct ibv_mr*  m_mr;
        Msg             m_msg;
};

} // namespace rdmaPlus

#endif
