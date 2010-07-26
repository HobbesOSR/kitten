
#ifndef PAL_PAH_H
#define PAL_PAH_H

#include <vector>
#include <rdmaPlus.h>
#include <pct/msgs.h>

class Config;
class Job;

using namespace rdmaPlus;

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
        Rdma      m_rdma;
};
#endif
