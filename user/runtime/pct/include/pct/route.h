#ifndef _PCT_ROUTE_H  
#define _PCT_ROUTE_H  

#include <rdmaPlus.h>
#include <pct/debug.h>
#include <list>

using namespace rdmaPlus;

class Route {
    public:
        struct Port {
            uint start;
            uint end;
            uint nid;
        };

    public:
        void add( uint nid, uint start = -1, uint end = -1 ){
             
            Debug( Route, "nid=%#x start=%d end=%d\n", nid, start, end );
            if ( start == (uint) -1 ) {
                m_defaultNid = nid;
                return; 
            }
            struct Port tmp;
            tmp.start = start;
            tmp.end = end;
            tmp.nid = nid;

            m_portMap.push_back( tmp );
        }

	    Nid rank2nid( uint rank ) {
            uint nid = m_defaultNid;
            std::list<Port>::iterator iter = m_portMap.begin();
            Debug( Route, "rank=%d\n", rank);
            for( ; iter != m_portMap.end(); ++iter ) {
                Debug(Route,"start=%d end=%d nid=%#x\n",
                                    iter->start,iter->end,iter->nid);
                if ( rank >= iter->start && rank < iter->end ) {
                    nid = iter->nid;
                    break;
                }
            }
            Debug( Route, "return nid=%#x\n", nid);
            return nid;
        }

    private:
        std::list<Port> m_portMap;
        uint            m_defaultNid;
};

#endif
