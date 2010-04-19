
#ifndef srdma_dm_H
#define srdma_dm_H

#include <map>
#include <list>

#include <srdma/dat/core.h>
#include <srdma/dtoFactory.h>
#include <srdma/connection.h>
#include <srdma/dto.h>
#include <srdma/dbg.h>

namespace srdma {

class Dm {

      public:

	typedef DAT_TIMEOUT Timeout;
        static const Timeout Timeout_Infinite = DAT_TIMEOUT_INFINITE;

	Dm();
	~Dm();

	int recv(void *, Size, ProcId &, Tag, Status *);
	int irecv(void *, Size, ProcId &, Tag, Request &);
	int send(void *, Size, ProcId &, Tag, Status *);
	int isend(void *, Size, ProcId &, Tag, Request &);
	int wait(Request &, Timeout, Status * s = NULL);

	int memRegionRegister(void *, Size, MemRegion::Id );
	int memRegionUnregister( MemRegion::Id );
	int memRegionGetEvent( MemRegion::Id, Event &);
	int memRead(ProcId &, MemRegion::Id, MemRegion::Addr far,
							void *local, Size);
	int memWrite(ProcId &, MemRegion::Id, MemRegion::Addr far,
							void *local, Size);
	int disconnect( ProcId& );

	Nid nid();
	Pid pid();

      private:

	typedef dat::EventHandler < Dm, void, DAT_EVENT & >functor_t;

	void eventHandler(DAT_EVENT &);

	dat::core::handler_t*	m_eventFunctor;
	dat::core		m_core;
	Connection*		m_connection;
	Dto* 			m_dto;
	ProcId			m_procId;
};

inline int Dm::memRegionRegister(void *addr, Size size, MemRegion::Id id )
{
	return m_dto->registerRegion( addr, size, id ); 
}

inline int Dm::memRegionUnregister( MemRegion::Id id )
{
	return m_dto->unregisterRegion( id ); 
}

inline int Dm::memRegionGetEvent( MemRegion::Id id, Event& event )
{
	return m_dto->getRegionEvent( id, event );
}

inline int Dm::memRead(ProcId & id, MemRegion::Id region, MemRegion::Addr addr,
                       void *local, Size length )
{
        EndPoint*       ep = m_connection->find( id );
	if ( ! ep ) {
                dbgFail(Dm, "m_connection->find()\n");
                return -1;
        }
	return m_dto->memOp( *ep, Read, region, addr, local, length );
}

inline int Dm::memWrite(ProcId & id, MemRegion::Id region, MemRegion::Addr addr,
                        void *local, Size length)
{
        EndPoint*       ep = m_connection->find( id );
	if ( ! ep ) {
                dbgFail(Dm, "m_connection->find()\n");
                return -1;
        }
	return m_dto->memOp( *ep, Write, region, addr, local, length );
}

inline int Dm::send(void *buf, Size count, ProcId & id, Tag tag,
							Status * status)
{
        dbg(Dm, "\n");
        Request req;
        int ret = isend( buf, count, id, tag, req );
        if (ret) return ret;

        return wait( req, Timeout_Infinite, status);
}

inline int Dm::isend(void *buf, Size count, ProcId & id, Tag tag,
							Request & req)
{
	EndPoint* ep;
        if ( ( ep = m_connection->find( id ) ) == NULL) {
                dbgFail(Dm, "m_connection->find()\n");
                return -1;
        }

        return m_dto->send_post( *ep, req.init( m_core, buf, count, id, tag ) );
}

inline int Dm::recv(void *buf, Size count, ProcId & id,
						Tag tag, Status * status)
{
        dbg(Dm, "\n");
        Request req;
        int ret = irecv(buf, count, id, tag, req);
        if (ret)
                return ret;
        return wait(req, Timeout_Infinite, status);
}

inline int Dm::irecv(void *buf, Size count, ProcId & id,
						Tag tag, Request & req)
{
        return m_dto->recv_post( req.init( m_core, buf, count, id, tag ) );
}

inline int Dm::disconnect( ProcId& id )
{
	return m_connection->disconnect( id );
}
	

inline Pid Dm::pid()
{
        return m_procId.pid;
}
        
inline Nid Dm::nid()
{
        return m_procId.nid;
}

} // namespace srdma 
#endif

