
#include <pal/debug.h>
#include <pal/config.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <queue>


Config::Config( int argc, char* argv[]) :
    m_pctPid( 1 ),
    m_jobId( 1000 ),
    m_heapLen( 1024L*1024L*1024L * 4L ),
//    m_heapLen( 1024L*1024L*512*2 ),
    //m_heapLen( 1024L*1024L*16 ),
    m_stackLen( 1024*1024 ),
    m_fanout( 4 ),
    m_ranksPer( 1 )
{
    std::string nidList;

    while (1) {
        static struct option opts[] = {
            {"nl",1,0,'n'},    // nid list
            {"cpus",1,0,'c'},  
            {"fanout",1,0,'f'}, 
            {0,0,0,0}
        };

        int c = getopt_long_only( argc, argv, "+", opts, NULL );

        if ( c == -1 ) {
            break;
        }

        switch( c ) {
            case 'n':
                Debug( Config, "-nl %s\n",optarg);
                nidList = optarg;
                break;

            case 'c':
                Debug( Config, "-cpus %s\n",optarg);
                m_ranksPer = atoi(optarg);
                break;

            case 'f':
                Debug( Config, "-fanout %s\n",optarg);
                m_fanout = atoi(optarg);
                break;
        }
    }
    
    std::string cmdLine;
    for ( ; optind < argc; optind++ ) {
        cmdLine += argv[optind];
        if ( optind + 1 < argc ) {
            cmdLine += " ";
        } 
    }
    
    Debug( Config, "cmdLine=\"%s\"\n",cmdLine.c_str());
    Debug( Config, "nidList=\"%s\"\n",nidList.c_str());

    if ( initImage( cmdLine ) ) {
        throw -1;
    }
    if ( initNidRnkMap( nidList ) ) {
        throw -1;
    }
}


bool Config::initImage( std::string& cmdLine )
{
    int fd;

    std::string argv = cmdLine;
    std::string exeName = argv.substr( 0, argv.find(" ") );

    Debug( Config, "exe=%s\n",exeName.c_str());
    if ( ( fd = open( exeName.c_str(), O_RDONLY) ) == -1 ) {
        perror("open app");
        return true;
    }

    struct stat stat;
    if ( fstat( fd, &stat ) == - 1 ) {
        perror("fstat app");
        return true;
    }

    m_elfLen = stat.st_size;

    void *app_image;
    if ( ( app_image = mmap( NULL, m_elfLen,
        PROT_READ, MAP_SHARED, fd, 0 ) ) == MAP_FAILED ) {
        perror("mmap app");
        return true;
    }


    std::string env;
    char** envp = environ;
    while ( *envp ) {
        env += *envp;
        if ( *(envp + 1) ) {
            env += " ";
        }
        ++envp;
    }

    //Debug( Config, "env=\"%s\"\n",env.c_str());

    m_cmdLen = cmdLine.size() + 1;
    m_envLen = env.size() + 1;

    m_appImage.resize( m_elfLen + m_cmdLen + m_envLen );

    size_t offset = (size_t) m_appImage.data();

    memcpy( (void*) offset, app_image, m_elfLen);
    offset += m_elfLen;
    memcpy( (void*) offset, cmdLine.data(), m_cmdLen );
    offset += m_cmdLen;
    memcpy( (void*) offset, env.data(), m_envLen );

    munmap( app_image, m_elfLen );
    close( fd );
    return false;
}

bool Config::initNidRnkMap( std::string& nidList )
{
    std::queue<std::string> foo;
    std::string tmp = nidList;

    while ( tmp.size() ) {

        if ( ! tmp.compare( 0, 1, ",") ) {
            tmp = tmp.substr( 1 );
            continue;
        }

        if ( ! tmp.compare( 0, 2, "..") ) {
            foo.push( "-" );
            tmp = tmp.substr( 2 );
            continue;
        }

        size_t len = 0;
        while ( len < tmp.size() && isalnum( tmp.at(len) ) ) ++len;

        foo.push( tmp.substr( 0, len ) );

        tmp = tmp.substr( len );
    }

    Nid prev = -1;
    bool range = false;
    while ( ! foo.empty() ) {
        NidRnk tmp;

        if ( ! foo.front().compare( 0, 1, "-" ) ) {
            foo.pop();
            range = true;
            continue;
        }
        Nid cur = strtol( foo.front().c_str(), NULL ,0 );
        Debug( Config, "cur=%#x\n", cur );

        if ( range ) {
            ++prev;
            for ( ; prev <= cur; ++prev ) {
                tmp.nid = prev ;
                m_nidRnkMap.push_back( tmp );
            }
            range = false;
        } else {
            tmp.nid = cur;
            m_nidRnkMap.push_back( tmp );
            prev = cur;
        }
        foo.pop();
    }

    for ( uint nid = 0; nid < nNids(); nid++ ) {
        m_nidRnkMap[nid].baseRank = nid * m_ranksPer;
        m_nidRnkMap[nid].ranksPer = m_ranksPer;
    }

#if 1 
    std::vector<struct NidRnk>::iterator iter;
    for ( iter = m_nidRnkMap.begin(); iter != m_nidRnkMap.end(); ++iter ) {
        Debug(Config,"nid=%#x baseRank=%d ranksPer=%d\n",
                        iter->nid, iter->baseRank, iter->ranksPer);
    }
#endif

    return false;
}

