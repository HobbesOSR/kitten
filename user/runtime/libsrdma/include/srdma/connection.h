
#include <list>
#include <netinet/in.h>
#include <netdb.h>
#include <srdma/dbg.h>
#include <srdma/types.h>
#include <srdma/endPoint.h>
#include <srdma/dat/core.h>
#include <srdma/dtoFactory.h>

namespace srdma {

static ProcId ProcIdFromCr(DAT_CR_PARAM & param);

class Connection {

    public:
	Connection( dat::core& );
	~Connection();
	bool event( DAT_EVENT& );
	EndPoint* find( ProcId& id );
	int disconnect( ProcId& id );

        struct priv_data {
                Pid pid;
        };

    private:

	bool accept_event( DAT_CR_ARRIVAL_EVENT_DATA& );
	bool established_event( DAT_CONNECTION_EVENT_DATA& );
	bool disconnected_event( DAT_CONNECTION_EVENT_DATA& );
	bool rejected_event( DAT_CONNECTION_EVENT_DATA& );
	bool connect(EndPoint & ep, ProcId & id);
	EndPoint* findEp( DAT_EP_HANDLE handle );

    private:
	struct ProcIdCmp {
                bool operator() (const ProcId & lh, const ProcId & rh) {
                        if (lh.nid != rh.nid)
                                return (lh.nid < rh.nid);
                        if (lh.pid != rh.pid)
                                return (lh.nid < rh.nid);
                        return false;
                }
        };

	typedef std::map < ProcId, EndPoint *, ProcIdCmp > epMap_t;

	epMap_t         	m_epMap;
	std::list<EndPoint* >   m_epDisList;
	pthread_mutex_t	m_lock;
	dat::core&	m_core;
	DtoFactory	m_dtoFactory;
};	

inline Connection::Connection( dat::core& core ) :
	m_core( core ),
	m_dtoFactory( m_core, sizeof(Msg ) )

{
	pthread_mutex_init(&m_lock, NULL);
}

inline Connection::~Connection()
{
        for ( epMap_t::iterator iter = m_epMap.begin();
                                        iter != m_epMap.end(); iter++ )
        {
                dbg(Connection,"\n");
                iter->second->disconnect();
                delete iter->second;
        }

        while ( ! m_epDisList.empty() ) {
                delete m_epDisList.front();
                m_epDisList.pop_front();
        }
}

inline EndPoint *Connection::find(ProcId & id)
{
        EndPoint *ep = NULL;
        epMap_t::iterator iter;

        pthread_mutex_lock(&m_lock);

        dbg(Connection, "ProcId=%#x:%d\n", id.nid, id.pid);

        if ((iter = m_epMap.find(id)) == m_epMap.end()) {
                ep = new EndPoint( m_core, m_dtoFactory, id );
                if (ep == NULL) {
                        dbgFail(Connection, "createEp()\n");
                        // need to cleanup
			pthread_mutex_unlock(&m_lock);
                        return NULL;
                }
                m_epMap[id] = ep;
		pthread_mutex_unlock(&m_lock);

                if ( connect( *ep, id ) ) {
                        dbgFail(Connection, "ep_connect()\n");
                        // need to cleanup
			delete ep;
			pthread_mutex_lock(&m_lock);
			m_epMap.erase( id );
			pthread_mutex_unlock(&m_lock);
			return NULL;
                }
        } else {
		pthread_mutex_unlock(&m_lock);
                dbg(Connection, "found\n");
                ep = iter->second;
        }

        return ep;
}

static bool getRemoteAddr(Nid nid, DAT_SOCK_ADDR & addr)
{
        struct addrinfo *target;
        char tmp[16];

        snprintf(tmp, 16, "%d.%d.%d.%d", nid >> 24 & 0xff,
                 nid >> 16 & 0xff, nid >> 8 & 0xff, nid & 0xff);

        dbg(Connection, "node=%s\n", tmp);

        if (getaddrinfo(tmp, NULL, NULL, &target) != 0) {
                dbgFail(Connection, "remote name resolution\n");
                return true;
        }

        memcpy( &addr, target->ai_addr, sizeof( addr ) );
        freeaddrinfo( target );
        return false;
}

inline bool Connection::connect(EndPoint & ep, ProcId & id)
{
        DAT_SOCK_ADDR remote_addr;
        dbg( Connection, "id=%#x:%d\n", id.nid, id.pid );

        if ( getRemoteAddr( id.nid, remote_addr) ) {
                return true;
        }

        struct priv_data pdata;

        pdata.pid = getpid();

        return ep.connect(&remote_addr, id.pid, DAT_TIMEOUT_INFINITE,
                   sizeof(pdata), (DAT_PVOID) & pdata, (DAT_QOS) 0);
}


inline bool Connection::event( DAT_EVENT& event )
{
	bool ret = true;
        dbg(Connection, "enter\n");
        
        pthread_mutex_lock(&m_lock);
        switch (event.event_number) {
        case DAT_CONNECTION_EVENT_ESTABLISHED:
                ret = established_event( event.event_data.connect_event_data );
                break;
        case DAT_CONNECTION_EVENT_DISCONNECTED:
                ret = disconnected_event(event.event_data.connect_event_data );
                break;
        case DAT_CONNECTION_EVENT_NON_PEER_REJECTED:
                ret = rejected_event(event.event_data.connect_event_data );
                break;
        case DAT_CONNECTION_REQUEST_EVENT:
                ret = accept_event( event.event_data.cr_arrival_event_data );
                break;
        default:
		dbgFail( Connection,"unknown event\n");
                break;
        }
        pthread_mutex_unlock(&m_lock);
	return ret;
}

inline bool Connection::accept_event( DAT_CR_ARRIVAL_EVENT_DATA& event )
{
        DAT_RETURN ret;
        DAT_CR_HANDLE h_cr = DAT_HANDLE_NULL;
        DAT_CR_PARAM cr_param = { 0 };
        DAT_CR_PARAM_MASK mask = (DAT_CR_PARAM_MASK) DAT_CR_FIELD_ALL;
        EndPoint *endPoint;

        dbg(Connection, "\n");

        h_cr = event.cr_handle;

        if ((ret = dat_cr_query(h_cr, mask, &cr_param)) != DAT_SUCCESS) {
                dbgFail(Connection, "dat_cr_query ret=%#x\n", ret);
                return true;
        }

        ProcId id = ProcIdFromCr( cr_param );

        dbg(Connection, "id=%#x:%d\n", id.nid, id.pid);

        epMap_t::iterator iter;
        if ((iter = m_epMap.find(id)) == m_epMap.end()) {
                m_epMap[id] = endPoint = 
				new EndPoint( m_core, m_dtoFactory, id );
        } else {
                // pick the winner and reject is loser
                dbg(Connection, "AAAAAAAAAAAAAAAAAA\n");
        }
        dbg(Connection, "new EndPoint %p\n", endPoint);

        if ((ret =
             dat_cr_accept(h_cr, endPoint->handle(), 0, NULL)) != DAT_SUCCESS) {
                dbgFail(Connection, "dat_cr_accept ret=%#x\n", ret);
                return true;
        }

        return false;

	dbg( Connection,"\n");
}

inline bool Connection::established_event( DAT_CONNECTION_EVENT_DATA& event )
{
        dbg(Connection,"enter\n");
	EndPoint* ep = findEp( event.ep_handle );

        if ( ! ep ) {
                dbgFail(Connection, "couldn't find ep_handle=%p\n",
							event.ep_handle);
                return true;
        }

	if ( ! ep->accept() ) {
        	ep->wakeup();
	}

	ep->postRecvBuf();

        return false;
}
inline bool Connection::rejected_event( DAT_CONNECTION_EVENT_DATA& event )
{
        dbg(Connection,"enter\n");
	EndPoint* ep = findEp( event.ep_handle );
        if ( ! ep ) {
                dbgFail(Connection, "couldn't find ep_handle=%p\n",
							event.ep_handle);
                return true;
        }

	if ( ! ep->accept() ) {
        	ep->wakeup(true);
	}
	return false;
}

inline EndPoint* Connection::findEp( DAT_EP_HANDLE ep_handle )
{
        epMap_t::iterator iter;

        for (iter = m_epMap.begin(); iter != m_epMap.end(); iter++) {
                dbg(Connection, "handle()=%p\n", (*iter).second->handle());
                if ((*iter).second->handle() == ep_handle) {
                        return  (*iter).second;
                }
        }
	return NULL;
}


inline bool Connection::disconnected_event(DAT_CONNECTION_EVENT_DATA & event)
{
        dbg(Connection, "ep_handle=%p\n", event.ep_handle);
#if 1 
        while ( ! m_epDisList.empty() ) {
                delete m_epDisList.front();
                m_epDisList.pop_front();
        }
#endif

        epMap_t::iterator iter;
        EndPoint *ep = NULL;

        for (iter = m_epMap.begin(); iter != m_epMap.end(); iter++) {
                dbg(Connection, "handle()=%p\n", (*iter).second->handle());
                if ((*iter).second->handle() == event.ep_handle) {
                        ep = (*iter).second;
                        break;
                }
        }

        if ( ! ep ) {
                dbgFail(Connection, "couldn't find ep_handle=%p\n",
							event.ep_handle);
                return true;
        }

        m_epDisList.push_back( ep );
        m_epMap.erase(iter);

        return false;
}

inline int Connection::disconnect( ProcId& id )
{
        EndPoint* ep=NULL;
        epMap_t::iterator iter;

        dbg(Connection,"id=%#x:%d\n", id.nid, id.pid );

        pthread_mutex_lock(&m_lock);

        if ((iter = m_epMap.find(id)) != m_epMap.end()) {
                ep = iter->second;
        }
        if ( ! ep ) {
		dbgFail(Connection,"couldn't find ep\n");
        	pthread_mutex_unlock(&m_lock);
		return -1;
	}
        ep->disconnect();

        pthread_mutex_unlock(&m_lock);

        return 0;
}

static inline ProcId ProcIdFromCr(DAT_CR_PARAM & param)
{
        ProcId id;
        struct sockaddr_in *sock =
            (struct sockaddr_in *)param.remote_ia_address_ptr;
        struct Connection::priv_data *pdata = (struct Connection::priv_data *)param.private_data;
        id.pid = pdata->pid;
        id.nid = ntohl(sock->sin_addr.s_addr);
        return id;
}

} // namespace srdma
