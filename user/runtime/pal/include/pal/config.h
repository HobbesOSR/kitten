
#ifndef PAL_CONFIG_H
#define PAL_CONFIG_H

#include <vector>
#include <sys/types.h>
#include <pct/msgs.h>
#include <string>

class Config {
    
    public:
        Config( int argc, char* argv[] );

        JobId  jobId()    { return m_jobId; }
        uint   pctPid()   { return m_pctPid; }
        size_t elfLen()   { return m_elfLen; }
        uint   cmdLen()   { return m_cmdLen; }
        uint   envLen()   { return m_envLen; }
        size_t heapLen()  { return m_heapLen; }
        size_t stackLen() { return m_stackLen; }
        uint   fanout()   { return m_fanout; }
        uint   nNids()    { return m_nidRnkMap.size(); }
        uint   nRanks()   { return m_ranksPer * nNids(); } 
        Nid    baseNid()  { return m_nidRnkMap[0].nid; }
        size_t appImageLen() { 
            return elfLen() + cmdLen() + envLen();
        }

        std::vector<uint8_t>&        appImage()  { return m_appImage; }
        std::vector<struct NidRnk>&  nidRnkMap() { return m_nidRnkMap; }

    private:

        Pid         m_pctPid;
        JobId       m_jobId;

        size_t      m_elfLen;
        uint        m_cmdLen;
        uint        m_envLen;
        size_t      m_heapLen;
        size_t      m_stackLen;

        uint        m_fanout;
        uint        m_ranksPer;

        std::vector<uint8_t>        m_appImage;
        std::vector<struct NidRnk>  m_nidRnkMap;

        bool initImage( std::string& cmdLine );        
        bool initNidRnkMap( std::string& nidList );        
};
#endif
