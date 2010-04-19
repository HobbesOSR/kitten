#ifndef srdma_endpoint_H
#define srdma_endpoint_H

#include <srdma/types.h>
#include <srdma/dat/ep.h>
#include <srdma/dat/core.h>
#include <srdma/dtoFactory.h>
#include <srdma/msgHdr.h>
#include <map>
#include <set>

using namespace dat;

namespace srdma {

class EndPoint {
	enum { Accept, Connect } m_state;

      public:
	EndPoint( dat::core&, DtoFactory&, ProcId);
	~EndPoint();

	bool wakeup( bool flag = false );
	bool wait();
	bool connect(DAT_IA_ADDRESS_PTR, DAT_CONN_QUAL,
		     DAT_TIMEOUT, DAT_COUNT, const DAT_PVOID, DAT_QOS,
		     DAT_CONNECT_FLAGS flags = DAT_CONNECT_DEFAULT_FLAG);

	void disconnect();
	void postRecvBuf();

	void post_recv(DAT_COUNT, DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
		       DAT_COMPLETION_FLAGS flags =
		       DAT_COMPLETION_DEFAULT_FLAG);

	void post_send(DAT_COUNT, DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
		       DAT_COMPLETION_FLAGS flags =
		       DAT_COMPLETION_DEFAULT_FLAG);

	void post_rdma_read(DAT_COUNT,
			    DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
			    DAT_RMR_TRIPLET &, DAT_COMPLETION_FLAGS flags =
			    DAT_COMPLETION_DEFAULT_FLAG);

	void post_rdma_write(DAT_COUNT,
			     DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
			     DAT_RMR_TRIPLET &, DAT_COMPLETION_FLAGS flags =
			     DAT_COMPLETION_DEFAULT_FLAG);

	bool findRMR( MemRegion::Id id, DAT_RMR_TRIPLET & rmr);
	bool invalidateRMR( MemRegion::Id id );

	ProcId id();
	DAT_EP_HANDLE handle();

	BaseDto* allocDto( Request* req = NULL );
	void freeDto( BaseDto* dto );

	bool accept() {
		return m_state == Accept;
	}
      private:
	EndPoint();
	EndPoint(const EndPoint &);

	void query(DAT_EP_PARAM & params);

	std::map < MemRegion::Id, DAT_RMR_TRIPLET > m_rmrMap;
	std::set< BaseDto* > m_dtoSet;

	dat::core&	m_core;
	DtoFactory&	m_dtoFactory;
	dat::ep* 	m_ep;
	DAT_EVD_HANDLE 	m_soft_evd_handle;
	DAT_EVENT* 	m_event;
	ProcId 		m_id;

	pthread_mutex_t	m_lock;
	int		m_msgCount;
};

#define MSG_BUF_COUNT     3
#define MSG_IOV_COUNT     2
#define MAX_RDMA_RD    4

inline	EndPoint::EndPoint(dat::core& core, DtoFactory& dtoFactory, ProcId id) :
	m_state( Accept ),
	m_core(core),
	m_dtoFactory(dtoFactory),
	m_id(id),
	m_msgCount(0)
{
	DAT_EP_ATTR ep_attr;
	int burst = 10;
	dbg(EndPoint, "\n");

	memset(&ep_attr, 0, sizeof(ep_attr));
	ep_attr.service_type = DAT_SERVICE_TYPE_RC;
	ep_attr.max_rdma_size = 0x1000*0x1000;
	ep_attr.qos = (DAT_QOS) 0;
	ep_attr.recv_completion_flags = (DAT_COMPLETION_FLAGS) 0;

	ep_attr.max_recv_dtos = MSG_BUF_COUNT + (burst * 3);
	ep_attr.max_request_dtos = MSG_BUF_COUNT + (burst * 3) + MAX_RDMA_RD;

	ep_attr.max_recv_iov = MSG_IOV_COUNT;
	ep_attr.max_request_iov = MSG_IOV_COUNT;

	ep_attr.max_rdma_read_in = MAX_RDMA_RD;
	ep_attr.max_rdma_read_out = MAX_RDMA_RD;
	ep_attr.request_completion_flags = DAT_COMPLETION_DEFAULT_FLAG;

	ep_attr.ep_transport_specific_count = 0;
	ep_attr.ep_transport_specific = NULL;
	ep_attr.ep_provider_specific_count = 0;
	ep_attr.ep_provider_specific = NULL;

	m_ep = new dat::ep(m_core.ia_handle(), m_core.pz_handle(), 
		m_core.dto_evd_handle(), m_core.conn_evd_handle(), ep_attr);

	dbg(EndPoint, "max_message_size %d\n", ep_attr.max_message_size);
	dbg(EndPoint, "max_rdma_size %d\n", ep_attr.max_rdma_size);
	dbg(EndPoint, "ep=%p\n", this);

        m_event = new DAT_EVENT;
        m_event->event_number = DAT_SOFTWARE_EVENT;
        m_event->event_data.software_event_data.pointer = NULL;

	DAT_RETURN ret;
        ret = dat_evd_create(m_core.ia_handle(), 1, DAT_HANDLE_NULL,
                 DAT_EVD_SOFTWARE_FLAG, &m_soft_evd_handle);
        if ( ret != DAT_SUCCESS ) {
                dbgFail(EndPoint, "dat_evd_create() %s\n",
						errorStr(ret).c_str() );
                throw ret;
        }                  

	pthread_mutex_init(&m_lock,NULL);
}

inline EndPoint::~EndPoint()
{
	dbg(EndPoint,"\n");
	while ( ! m_dtoSet.empty() ) {
		freeDto( *m_dtoSet.begin() );
	}
	delete m_event;
	delete m_ep;
}

inline BaseDto* EndPoint::allocDto( Request* req )
{
	BaseDto* dto;
	pthread_mutex_lock(&m_lock);
	if ( req ) {
		dto = m_dtoFactory.allocSend( * this, *req ); 
	} else {
		dto = m_dtoFactory.allocCtlMsg( * this ); 
	}
	if ( m_dtoSet.find( dto ) != m_dtoSet.end() ) {
        	dbgFail( EndPoint, "dto=%p\n", dto );
		throw -1;
     	}
     	m_dtoSet.insert( dto );
	pthread_mutex_unlock(&m_lock);
	return dto;
}

inline void EndPoint::freeDto( BaseDto* dto )
{
	pthread_mutex_lock(&m_lock);
	if ( m_dtoSet.find( dto ) == m_dtoSet.end() ) {
        	dbgFail( EndPoint, "\n" );
		throw -1;
     	}
     	m_dtoSet.erase( dto );
	m_dtoFactory.free( dto );
	pthread_mutex_unlock(&m_lock);
}

inline void EndPoint::postRecvBuf( )
{       

	RecvDto& dto = * (RecvDto*) allocDto();

        dbg( EndPoint, "dto=%p\n",&dto);
        
        post_recv(dto.num_segments(), dto.lmr_triplet(), dto.cookie());
}       

inline void EndPoint::post_recv(DAT_COUNT count, DAT_LMR_TRIPLET & lmr,
				DAT_DTO_COOKIE cookie,
				DAT_COMPLETION_FLAGS flags)
{
	m_ep->post_recv(count, lmr, cookie, flags);
}

inline void EndPoint::post_send(DAT_COUNT count, DAT_LMR_TRIPLET & lmr,
				DAT_DTO_COOKIE cookie,
				DAT_COMPLETION_FLAGS flags)
{
	BaseDto& dto = *(BaseDto*)cookie.as_ptr;
	Msg& msg = *(Msg*) dto.lmr_triplet().virtual_address;
	dbg( EndPoint, "count=%d\n", m_msgCount );
	msg.hdr.count = m_msgCount++;
	m_ep->post_send(count, lmr, cookie, flags);
}

inline void EndPoint::post_rdma_read(DAT_COUNT count,
				     DAT_LMR_TRIPLET & lmr,
				     DAT_DTO_COOKIE cookie,
				     DAT_RMR_TRIPLET & rmr,
				     DAT_COMPLETION_FLAGS flags)
{
	dbg(EndPoint,"addr=%#lx lenght=%#lx ctx=%#lx\n",
					(unsigned long) rmr.virtual_address,
					(unsigned long) rmr.segment_length,
					(unsigned long) rmr.rmr_context);

	m_ep->post_rdma_read(count, lmr, cookie, rmr, flags);
}

inline void EndPoint::post_rdma_write(DAT_COUNT count,
				      DAT_LMR_TRIPLET & lmr,
				      DAT_DTO_COOKIE cookie,
				      DAT_RMR_TRIPLET & rmr,
				      DAT_COMPLETION_FLAGS flags)
{
	dbg(EndPoint,"addr=%#lx lenght=%#lx ctx=%#lx\n",
					(unsigned long) rmr.virtual_address,
					(unsigned long) rmr.segment_length,
					(unsigned long) rmr.rmr_context);
	m_ep->post_rdma_write(count, lmr, cookie, rmr, flags);
}

inline bool EndPoint::connect(DAT_IA_ADDRESS_PTR remote_ia_addr,
			      DAT_CONN_QUAL remote_conn_qual,
			      DAT_TIMEOUT timeout,
			      DAT_COUNT private_data_size,
			      const DAT_PVOID private_data,
			      DAT_QOS qos, DAT_CONNECT_FLAGS flags)
{
	dbg(EndPoint,"enter\n");

	m_state = Connect;

	m_ep->connect(remote_ia_addr, remote_conn_qual,
		      timeout, private_data_size, private_data, qos, flags);

	if ( wait() ) {
		dbgFail( EndPpoint,"connect()\n");
		return true;
	}

	dbg(EndPoint,"\n");
	return false;
}

inline bool EndPoint::wakeup( bool flag )
{
	DAT_RETURN ret;

	dbg(EndPoint,"\n");
	
	m_event->event_data.software_event_data.pointer = (void*) flag;
        if ( (ret = dat_evd_post_se(m_soft_evd_handle, m_event) ) != 
					DAT_SUCCESS) {
                dbgFail(EndPoint, "dat_evd_post_se() %s\n",
					errorStr(ret).c_str());
                return  true;
        }
	return false;
}

inline bool EndPoint::wait()
{
	DAT_RETURN ret;
	DAT_EVENT event;
	DAT_COUNT nmore;

	dbg(EndPoint,"wait\n");
	ret = dat_evd_wait( m_soft_evd_handle, DAT_TIMEOUT_INFINITE,
                           1, &event, &nmore);
        if (ret != DAT_SUCCESS) {
                dbgFail(EndPoint, "dat_evd_wait() %s\n",errorStr(ret).c_str());
                throw ret;
        }
	return event.event_data.software_event_data.pointer;
}

inline void EndPoint::query(DAT_EP_PARAM & params)
{
	m_ep->query(params);
}

inline DAT_EP_HANDLE EndPoint::handle()
{
	return m_ep->handle();
}

inline ProcId EndPoint::id()
{
	return m_id;
}

inline bool EndPoint::invalidateRMR( MemRegion::Id id )
{
	dbg(EndPoint, "id=%#x\n", id);
	if (m_rmrMap.find(id) != m_rmrMap.end()) {
		m_rmrMap.erase( id );
		return true;
	}
	return false;
}

inline bool EndPoint::findRMR( MemRegion::Id id, DAT_RMR_TRIPLET & rmr )
{
	dbg(EndPoint, "id=%#x\n", id);
	if (m_rmrMap.find(id) != m_rmrMap.end()) {
		rmr = m_rmrMap[id];
		return true;
	}

        CtlMsgDto& dto = *(CtlMsgDto*) allocDto( );

        Msg& msg = *(Msg*)dto.lmr_triplet().virtual_address;

        msg.hdr.type = MsgHdr::RMR_REQ;
        msg.hdr.u.rmrReq.id = id;
	msg.hdr.cookie.as_ptr = &dto;

        post_send( dto.num_segments(), dto.lmr_triplet(), dto.cookie(),
                             DAT_COMPLETION_SUPPRESS_FLAG );

        dbg( EndPoint, "\n");
	wait();
        dbg( EndPoint, "\n");

	rmr = m_rmrMap[id] = msg.hdr.u.rmrResp.rmr;
	
	freeDto(&dto);
	return true;
}


inline void EndPoint::disconnect()
{
	dbg(EndPoint, "\n");
	m_ep->disconnect();
}

} // namespace srdma

#endif
