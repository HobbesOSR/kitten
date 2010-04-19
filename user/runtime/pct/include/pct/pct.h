
#ifndef PCT_PCT_H
#define PCT_PCT_H

#include <srdma/dm.h>
#include <pct/msgs.h>
#include <map>
#include <vector>
#include <pthread.h>

using namespace srdma;

class Job;
class FileSystem;
class Pct {
        typedef std::map<JobId,Job*> JobMap; 
    public:
        Pct();
        ~Pct();
        int Go();

    private:
        Pct( const Pct& );

        void ctrlMsgProcess( CtrlMsg& );

        void jobMsg( ProcId&, struct JobMsg& );

        Dm                      m_rdma;
        JobMap                  m_jobMap;
	int			m_ompiFd;
//        FileSystem*             m_fileSystem;
};

#endif
