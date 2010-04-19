
#ifndef JOB_PAH_H
#define JOB_PAH_H

#include <srdma/dm.h>
#include <pct/msgs.h>
#include <vector>

using namespace srdma;
class Config;

class Job {
        static const int ImageId = 0xfead0001;
        static const int NidRankId = 0xfead0002;
        static const int NidPidId = 0xfead0003;

    public:
        Job( Dm&, Config& );
        ~Job();
        bool Load();
        bool JobMsg( ProcId&, struct JobMsg& );
        bool Kill( int sig );

    private:
        
        Job(const Job&);

        bool sendCtrlMsg( struct CtrlMsg& msg );
        bool havePidPart( struct JobMsg::NidPidMapPart& );
        bool childStart( );

        Config&             m_config;
        Dm&                 m_rdma;
        bool                m_kill;
        ProcId              m_myNodeId;
        ProcId              m_rootNodeId;

        JobId               m_jobId;

        std::vector<ProcId> m_nidPidMap;

        MemRegion::Handle      m_nidRnkMap_region;
        MemRegion::Handle      m_appImage_region;
        MemRegion::Handle      m_nidPidMap_region;

#if 0
        memRegion::Id imageKey()     { return m_rdma.memRegionId( m_appImage_region ); }
        MemRegion::Id nidRnkMapKey() { return m_rdma.memRegionId( m_nidRnkMap_region ); }
        MemRegion::Id nidPidMapKey() { return m_rdma.memRegionId( m_nidPidMap_region ); }
#endif
};
#endif
