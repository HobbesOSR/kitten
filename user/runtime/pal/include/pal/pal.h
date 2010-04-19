
#ifndef PAL_PAH_H
#define PAL_PAH_H

#include <vector>
#include <srdma/dm.h>
#include <pct/msgs.h>

class Config;
class Job;

using namespace srdma;

class Pal {
    public:
        Pal( );
        ~Pal();
        int Run( Config& );
        void Sighandler( int );

    private:
        
        Pal(const Pal&);

        bool do_ctrl( CtrlMsg& );

        Job*    m_job;
        Dm      m_rdma;
};
#endif
