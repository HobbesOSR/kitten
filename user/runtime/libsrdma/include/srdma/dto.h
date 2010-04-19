#ifndef srdma_dto_H
#define srdma_dto_H

#include <srdma/dtoFactory.h>
#include <map>
#include <deque>

namespace srdma {

class Dto {

      public:			// function
	Dto( dat::core& );

	bool event(DAT_DTO_COMPLETION_EVENT_DATA &);
	bool postSe( Request& req);
	RecvDto *findRecvDto(Key &);
	int registerRegion( void*, Size, MemRegion::Id );
	int unregisterRegion( MemRegion::Id );
	int getRegionEvent( MemRegion::Id, Event& );
	int memOp(EndPoint&, MemOp op, MemRegion::Id region,
			MemRegion::Addr addr, void *local, Size length);

	int recv_post( Request& );
	int send_post( EndPoint& ep, Request & req);


      private:			// functions
	bool recv_event(DAT_DTO_COMPLETION_EVENT_DATA &);
	bool send_event(DAT_DTO_COMPLETION_EVENT_DATA &);
	bool rdma_read_event(DAT_DTO_COMPLETION_EVENT_DATA &);
	bool rdma_write_event(DAT_DTO_COMPLETION_EVENT_DATA &);

	bool reqMsg( RecvDto& );
	bool ackMsg( CtlMsgDto& );
        bool memEvent( MemRegion::Id, MemOp, MemRegion::Addr, Size);

	bool rmrReqMsg( CtlMsgDto& dto );
	bool rmrRespMsg( CtlMsgDto& dto );
	bool rdmaRespMsg( CtlMsgDto& dto );

	Request *reqFind(Key &);
	bool keyCmp(const Key &, const Key &);

      private:			// types
	struct KeyCmp {
		bool operator() (const Key & lh, const Key & rh) {
			if (lh.count != rh.count)
				return (lh.count < rh.count);
			if (lh.tag != rh.tag)
				return (lh.tag < rh.tag);
			if (lh.id.nid != rh.id.nid)
				return (lh.tag < rh.tag);
			if (lh.id.pid != rh.id.pid)
				return (lh.tag < rh.tag);
			return false;
		}
	};

      public:			// types
	typedef std::multimap < Key, RecvDto *, KeyCmp > dtoMap_t;
	typedef std::multimap < Key, Request *, KeyCmp > reqMap_t;

      public:			// data
	dtoMap_t m_rcvdMap;
	reqMap_t m_recvMap;
	std::map < MemRegion::Id, MemRegion* > m_regionMap;

      private:			// data
	dat::core&	m_core;
	pthread_mutex_t	m_lock;
};

inline Dto::Dto( dat::core& core ): 
	m_core(core)
{
	pthread_mutex_init(&m_lock, NULL);
}

} // namespace srdma

#endif
