#include <srdma/dm.h>
#include <string.h>
#include <stdlib.h>

namespace srdma {

Dm::Dm()
{
	dbg(Dm, "enter\n");

	m_eventFunctor = new functor_t(this, &Dm::eventHandler);
	m_core.setHandler( m_eventFunctor );

	m_dto = new Dto( m_core );
	m_connection = new Connection( m_core );

        struct addrinfo *target;

        if ( getaddrinfo( "ib0", NULL, NULL, &target ) != 0 ) {
                dbgFail(Dm, "name resolution\n");
		throw -1;
        }

	m_procId.nid = ntohl( 
		((struct sockaddr_in*)target->ai_addr)->sin_addr.s_addr );
	m_procId.pid = getpid();

	dbg( Dm, "ProcId %#x:%d\n", m_procId.nid, m_procId.pid );

	m_core.listen( m_procId.pid );
}

Dm::~Dm()
{
	dbg(Dm, "enter\n");

	m_core.shutdown();

	delete m_connection;
	delete m_dto;
	delete m_eventFunctor;
}

void Dm::eventHandler(DAT_EVENT & event)
{
	dbg(Dm, "enter\n");

	switch (event.event_number) {
	case DAT_CONNECTION_EVENT_ESTABLISHED:
	case DAT_CONNECTION_EVENT_DISCONNECTED:
	case DAT_CONNECTION_EVENT_NON_PEER_REJECTED:
	case DAT_CONNECTION_REQUEST_EVENT:
		m_connection->event( event );
		break;
	case DAT_DTO_COMPLETION_EVENT:
		m_dto->event(event.event_data.dto_completion_event_data);
		break;
	case DAT_IB_EXTENSION_RANGE_BASE:
		dbg( Dm, "DAT_ID_EXTENSION_RANGE_BASE\n" );
		break;
	default:
		break;
	}
	dbg(Dm, "return\n");
}

int Dm::wait( Request & req, Timeout timeout, Status * status )
{
	DAT_RETURN ret;
	DAT_EVENT event;
	DAT_COUNT nmore;

	if ( timeout == Timeout_Infinite ) dbg(Dm, "\n");

	ret = dat_evd_wait( req.evd_handle(), timeout, 1, &event, &nmore );
	if ( DAT_GET_TYPE(ret) == DAT_TIMEOUT_EXPIRED) {
		return 0;
	}

	if ( ret != DAT_SUCCESS ) {
		dbgFail(Dm, "dat_evd_wait() %s\n", errorStr(ret).c_str());
		return -1;
	}

	dbg(Dm, "\n");

	if (status) {
		status->key = req.key();
	}
	
	BaseDto& dto = *(BaseDto*) event.event_data.software_event_data.pointer;
        EndPoint& ep = dto.endPoint();
        ep.freeDto( &dto );

	return 1;
}

}  // namespace srdma 
