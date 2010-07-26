#ifndef PCT_PCT_H
#define PCT_PCT_H

#include <rdmaPlus.h>
#include <pct/msgs.h>

using namespace rdmaPlus;

class Pct {
    public:
        Pct();
        ~Pct();
        int Go();

    private:
        Pct( const Pct& );

        void ctrlMsgProcess( CtrlMsg& );
        void jobMsg( ProcId&, struct JobMsg& );

        Rdma                m_rdma;
        struct sigaction    m_prevSigaction;
};

#endif
