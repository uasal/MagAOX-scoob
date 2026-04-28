/** \file shmimTCP.hpp
 * \brief The MagAO-X shared-memory TCP bridge.
 *
 * \ingroup shmimTCP_files
 */

#ifndef shmimTCP_hpp
#define shmimTCP_hpp

#include <ImageStreamIO/ImageStruct.h>
#include <ImageStreamIO/ImageStreamIO.h>

#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../../libMagAOX/libMagAOX.hpp" // Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

namespace MagAOX
{
namespace app
{

/** \defgroup shmimTCP shmimTCP
 * \brief Transfer ImageStreamIO shmims across TCP.
 *
 * \ingroup apps
 */

/** \defgroup shmimTCP_files shmimTCP Files
 * \ingroup shmimTCP
 */

class shmimTCP : public MagAOXApp<true>, public dev::shmimMonitor<shmimTCP>
{
    friend class dev::shmimMonitor<shmimTCP>;

    typedef dev::shmimMonitor<shmimTCP> shmimMonitorT;

    enum class modeT
    {
        transmit,
        receive
    };

#pragma pack(push, 1)
    struct frameHeaderNet
    {
        uint32_t magic;
        uint16_t version;
        uint16_t reserved;

        uint32_t targetNameBytes;
        uint32_t datatype;
        uint32_t width;
        uint32_t height;
        uint32_t depth;

        uint64_t frameBytes;
        uint64_t cnt0;
        uint64_t cnt1;

        uint64_t writetimeSec;
        uint64_t writetimeNsec;
        uint64_t atimeSec;
        uint64_t atimeNsec;
    };
#pragma pack(pop)

    struct frameHeaderHost
    {
        uint32_t datatype{ 0 };
        uint32_t width{ 0 };
        uint32_t height{ 0 };
        uint32_t depth{ 1 };

        uint64_t frameBytes{ 0 };
        uint64_t cnt0{ 0 };
        uint64_t cnt1{ 0 };

        int64_t writetimeSec{ 0 };
        int64_t writetimeNsec{ 0 };
        int64_t atimeSec{ 0 };
        int64_t atimeNsec{ 0 };
    };

    static constexpr uint32_t c_magic{ 0x53484D54 }; // "SHMT"
    static constexpr uint16_t c_version{ 1 };
    static constexpr size_t   c_maxTargetName{ 256 };

  protected:
    /** \name Configurable Parameters
     * @{
     */
    std::string m_modeString{ "transmit" };
    modeT       m_mode{ modeT::transmit };

    std::string m_sourceShmim;
    std::string m_targetShmim{ "shmimTCP" };

    std::string m_remoteHost{ "127.0.0.1" };
    int         m_remotePort{ 30105 };
    std::string m_listenAddress{ "0.0.0.0" };

    int m_connectTimeoutSec{ 2 };
    int m_reconnectSec{ 2 };
    int m_socketTimeoutSec{ 2 };
    ///@}

    std::mutex m_paramMutex;

    size_t m_frameBytes{ 0 };

    std::mutex       m_txMutex;
    int              m_txSockFd{ -1 };
    std::atomic<bool> m_txConnected{ false };
    std::chrono::steady_clock::time_point m_nextReconnect;

    std::thread       m_rxThread;
    std::atomic<bool> m_rxThreadRunning{ false };
    std::atomic<bool> m_rxListening{ false };
    std::atomic<bool> m_rxClientConnected{ false };
    int               m_listenFd{ -1 };
    int               m_clientFd{ -1 };
    std::vector<uint8_t> m_rxFrameBuffer;

    IMAGE       m_targetImage;
    bool        m_targetImageOpen{ false };
    std::string m_targetImageName;
    uint32_t    m_targetWidth{ 0 };
    uint32_t    m_targetHeight{ 0 };
    uint32_t    m_targetDepth{ 1 };
    uint8_t     m_targetDataType{ 0 };
    size_t      m_targetTypeSize{ 0 };

    pcf::IndiProperty m_indiP_sourceShmim;
    pcf::IndiProperty m_indiP_targetShmim;
    pcf::IndiProperty m_indiP_linkState;

    INDI_NEWCALLBACK_DECL( shmimTCP, m_indiP_sourceShmim );
    INDI_NEWCALLBACK_DECL( shmimTCP, m_indiP_targetShmim );

  public:
    shmimTCP();

    ~shmimTCP() noexcept
    {
    }

    virtual void setupConfig();
    virtual void loadConfig();
    virtual int  appStartup();
    virtual int  appLogic();
    virtual int  appShutdown();

  protected:
    int allocate( const dev::shmimT &dummy );
    int processImage( void *curr_src, const dev::shmimT &dummy );

    static uint64_t hostToNet64( uint64_t v );
    static uint64_t netToHost64( uint64_t v );

    static bool sendAll( int fd, const void *data, size_t nBytes );
    static bool recvAll( int fd, void *data, size_t nBytes );

    void closeTxSocket();
    void closeTxSocketLocked();
    void scheduleReconnect();
    int  connectRemote();
    int  connectWithTimeout( int fd, const sockaddr *sa, socklen_t saLen, int timeoutSec );

    void receiveThreadExec();
    int  openListenSocket();
    void closeRxSockets();
    int  receiveClient( int clientFd );

    int  ensureTargetStream( const std::string &name, uint32_t width, uint32_t height, uint32_t depth, uint8_t datatype );
    void destroyTargetStream();
    int  writeFrameToTarget( const frameHeaderHost &header, const uint8_t *frameData );
};

inline shmimTCP::shmimTCP() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    m_nextReconnect = std::chrono::steady_clock::now();

    std::memset( &m_targetImage, 0, sizeof( m_targetImage ) );
}

inline void shmimTCP::setupConfig()
{
    SHMIMMONITOR_SETUP_CONFIG( config );

    config.add( "shmimTCP.mode",
                "",
                "shmimTCP.mode",
                argType::Required,
                "shmimTCP",
                "mode",
                false,
                "string",
                "Transfer mode: transmit or receive." );

    config.add( "shmimTCP.sourceShmim",
                "",
                "shmimTCP.sourceShmim",
                argType::Required,
                "shmimTCP",
                "sourceShmim",
                false,
                "string",
                "Name of local source shmim to monitor and transmit." );

    config.add( "shmimTCP.targetShmim",
                "",
                "shmimTCP.targetShmim",
                argType::Required,
                "shmimTCP",
                "targetShmim",
                false,
                "string",
                "Name of target shmim to create/update on receiver." );

    config.add( "remote.host",
                "",
                "remote.host",
                argType::Required,
                "remote",
                "host",
                false,
                "string",
                "Remote host name or IP used by transmit mode." );

    config.add( "remote.port",
                "",
                "remote.port",
                argType::Required,
                "remote",
                "port",
                false,
                "int",
                "TCP port used for send/receive." );

    config.add( "remote.listenAddress",
                "",
                "remote.listenAddress",
                argType::Required,
                "remote",
                "listenAddress",
                false,
                "string",
                "Local bind address used by receive mode." );

    config.add( "remote.connectTimeoutSec",
                "",
                "remote.connectTimeoutSec",
                argType::Required,
                "remote",
                "connectTimeoutSec",
                false,
                "int",
                "Timeout in seconds for TCP connect attempts in transmit mode." );

    config.add( "remote.reconnectSec",
                "",
                "remote.reconnectSec",
                argType::Required,
                "remote",
                "reconnectSec",
                false,
                "int",
                "Reconnect interval in seconds after a transmit disconnect." );

    config.add( "remote.socketTimeoutSec",
                "",
                "remote.socketTimeoutSec",
                argType::Required,
                "remote",
                "socketTimeoutSec",
                false,
                "int",
                "Send/receive timeout in seconds for TCP sockets." );
}

inline void shmimTCP::loadConfig()
{
    if( shmimMonitorT::loadConfig( config ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__, "Error from shmimMonitorT::loadConfig" } );
        m_shutdown = true;
        return;
    }

    config( m_modeString, "shmimTCP.mode" );
    config( m_sourceShmim, "shmimTCP.sourceShmim" );
    config( m_targetShmim, "shmimTCP.targetShmim" );

    config( m_remoteHost, "remote.host" );
    config( m_remotePort, "remote.port" );
    config( m_listenAddress, "remote.listenAddress" );
    config( m_connectTimeoutSec, "remote.connectTimeoutSec" );
    config( m_reconnectSec, "remote.reconnectSec" );
    config( m_socketTimeoutSec, "remote.socketTimeoutSec" );

    if( m_sourceShmim == "" )
    {
        m_sourceShmim = shmimMonitorT::m_shmimName;
    }
    if( m_sourceShmim == "" )
    {
        m_sourceShmim = configName();
    }
    shmimMonitorT::m_shmimName = m_sourceShmim;

    if( m_targetShmim == "" )
    {
        m_targetShmim = "shmimTCP";
    }

    std::string modeLower = m_modeString;
    for( size_t i = 0; i < modeLower.size(); ++i )
    {
        modeLower[i] = static_cast<char>( ::tolower( static_cast<unsigned char>( modeLower[i] ) ) );
    }

    if( modeLower == "receive" || modeLower == "rx" || modeLower == "server" )
    {
        m_mode = modeT::receive;
    }
    else
    {
        m_mode = modeT::transmit;
        m_modeString = "transmit";
    }

    if( m_remotePort < 1 || m_remotePort > 65535 )
    {
        m_remotePort = 30105;
    }

    if( m_connectTimeoutSec < 1 )
    {
        m_connectTimeoutSec = 1;
    }

    if( m_reconnectSec < 1 )
    {
        m_reconnectSec = 1;
    }

    if( m_socketTimeoutSec < 1 )
    {
        m_socketTimeoutSec = 1;
    }
}

inline int shmimTCP::appStartup()
{
    if( createStandardIndiText( m_indiP_sourceShmim, "sourceShmim", "Source shmim", "shmimTCP" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }
    m_indiP_sourceShmim["current"] = m_sourceShmim;
    m_indiP_sourceShmim["target"]  = m_sourceShmim;
    if( registerIndiPropertyNew( m_indiP_sourceShmim, INDI_NEWCALLBACK( m_indiP_sourceShmim ) ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( createStandardIndiText( m_indiP_targetShmim, "targetShmim", "Target shmim", "shmimTCP" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }
    m_indiP_targetShmim["current"] = m_targetShmim;
    m_indiP_targetShmim["target"]  = m_targetShmim;
    if( registerIndiPropertyNew( m_indiP_targetShmim, INDI_NEWCALLBACK( m_indiP_targetShmim ) ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( createROIndiText( m_indiP_linkState, "linkState", "state", "TCP link state", "shmimTCP", "State" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }
    m_indiP_linkState["state"] = "disconnected";
    if( registerIndiPropertyReadOnly( m_indiP_linkState ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( m_mode == modeT::transmit )
    {
        SHMIMMONITOR_APP_STARTUP;
        state( stateCodes::READY );
    }
    else
    {
        m_rxThreadRunning = true;
        try
        {
            m_rxThread = std::thread( &shmimTCP::receiveThreadExec, this );
        }
        catch( ... )
        {
            m_rxThreadRunning = false;
            return log<software_error, -1>( { __FILE__, __LINE__, "failed to start receive thread" } );
        }

        if( !m_rxThread.joinable() )
        {
            m_rxThreadRunning = false;
            return log<software_error, -1>( { __FILE__, __LINE__, "receive thread did not start" } );
        }

        state( stateCodes::READY );
    }

    return 0;
}

inline int shmimTCP::appLogic()
{
    if( m_mode == modeT::transmit )
    {
        SHMIMMONITOR_APP_LOGIC;

        auto now = std::chrono::steady_clock::now();
        if( !m_txConnected && now >= m_nextReconnect )
        {
            connectRemote();
        }

        if( m_txConnected )
        {
            state( stateCodes::OPERATING );
        }
        else
        {
            state( stateCodes::READY );
        }

        if( shmimMonitorT::updateINDI() < 0 )
        {
            log<software_error>( { __FILE__, __LINE__ } );
        }
    }
    else
    {
        if( !m_rxThreadRunning && !shutdown() )
        {
            return log<software_error, -1>( { __FILE__, __LINE__, "receive thread exited unexpectedly" } );
        }

        if( m_rxClientConnected )
        {
            state( stateCodes::OPERATING );
        }
        else if( m_rxListening )
        {
            state( stateCodes::READY );
        }
        else
        {
            state( stateCodes::CONFIGURING );
        }
    }

    std::string source;
    std::string target;
    {
        std::lock_guard<std::mutex> lock( m_paramMutex );
        source = m_sourceShmim;
        target = m_targetShmim;
    }

    updateIfChanged( m_indiP_sourceShmim, "current", source, INDI_IDLE );
    updateIfChanged( m_indiP_sourceShmim, "target", source, INDI_IDLE );
    updateIfChanged( m_indiP_targetShmim, "current", target, INDI_IDLE );
    updateIfChanged( m_indiP_targetShmim, "target", target, INDI_IDLE );

    if( m_mode == modeT::transmit )
    {
        if( m_txConnected )
        {
            updateIfChanged( m_indiP_linkState, "state", "connected " + m_remoteHost + ":" + std::to_string( m_remotePort ), INDI_OK );
        }
        else
        {
            updateIfChanged(
                m_indiP_linkState, "state", "disconnected " + m_remoteHost + ":" + std::to_string( m_remotePort ), INDI_IDLE );
        }
    }
    else
    {
        if( m_rxClientConnected )
        {
            updateIfChanged( m_indiP_linkState, "state", "client connected", INDI_OK );
        }
        else if( m_rxListening )
        {
            updateIfChanged( m_indiP_linkState,
                             "state",
                             "listening " + m_listenAddress + ":" + std::to_string( m_remotePort ),
                             INDI_IDLE );
        }
        else
        {
            updateIfChanged( m_indiP_linkState, "state", "not listening", INDI_BUSY );
        }
    }

    return 0;
}

inline int shmimTCP::appShutdown()
{
    if( m_mode == modeT::transmit )
    {
        SHMIMMONITOR_APP_SHUTDOWN;
    }

    closeTxSocket();

    closeRxSockets();
    if( m_rxThread.joinable() )
    {
        try
        {
            m_rxThread.join();
        }
        catch( ... )
        {
        }
    }
    m_rxThreadRunning = false;

    destroyTargetStream();

    return 0;
}

inline int shmimTCP::allocate( const dev::shmimT &dummy )
{
    static_cast<void>( dummy );

    if( shmimMonitorT::m_depth == 0 )
    {
        shmimMonitorT::m_depth = 1;
    }

    m_frameBytes = static_cast<size_t>( shmimMonitorT::m_width ) * static_cast<size_t>( shmimMonitorT::m_height ) *
                   shmimMonitorT::m_typeSize;

    std::string msg = "source stream ";
    msg += shmimMonitorT::m_shmimName + " ";
    msg += std::to_string( shmimMonitorT::m_width ) + "x" + std::to_string( shmimMonitorT::m_height ) + "x" +
           std::to_string( shmimMonitorT::m_depth ) + " dtype=" + std::to_string( shmimMonitorT::m_dataType );
    log<text_log>( msg, logPrio::LOG_NOTICE );

    return 0;
}

inline int shmimTCP::processImage( void *curr_src, const dev::shmimT &dummy )
{
    static_cast<void>( dummy );

    if( m_mode != modeT::transmit )
    {
        return 0;
    }

    if( m_frameBytes == 0 )
    {
        return 0;
    }

    if( !m_txConnected )
    {
        return 0;
    }

    std::string targetName;
    {
        std::lock_guard<std::mutex> lock( m_paramMutex );
        targetName = m_targetShmim;
    }

    if( targetName.empty() )
    {
        return 0;
    }

    if( targetName.size() > c_maxTargetName )
    {
        targetName.resize( c_maxTargetName );
    }

    frameHeaderHost headerHost;
    headerHost.datatype   = shmimMonitorT::m_dataType;
    headerHost.width      = shmimMonitorT::m_width;
    headerHost.height     = shmimMonitorT::m_height;
    headerHost.depth      = shmimMonitorT::m_depth == 0 ? 1 : shmimMonitorT::m_depth;
    headerHost.frameBytes = m_frameBytes;

    IMAGE *img = &shmimMonitorT::m_imageStream;
    if( img && img->md )
    {
        uint64_t idx = 0;
        if( headerHost.depth > 1 )
        {
            idx = img->md[0].cnt1 % headerHost.depth;
        }

        headerHost.cnt1 = img->md[0].cnt1;
        headerHost.cnt0 = img->md[0].cnt0;

        if( img->cntarray != nullptr )
        {
            headerHost.cnt0 = img->cntarray[idx];
        }

        timespec wt = img->md[0].writetime;
        timespec at = img->md[0].atime;

        if( img->writetimearray != nullptr )
        {
            wt = img->writetimearray[idx];
        }
        if( img->atimearray != nullptr )
        {
            at = img->atimearray[idx];
        }

        headerHost.writetimeSec  = wt.tv_sec;
        headerHost.writetimeNsec = wt.tv_nsec;
        headerHost.atimeSec      = at.tv_sec;
        headerHost.atimeNsec     = at.tv_nsec;
    }

    frameHeaderNet headerNet;
    std::memset( &headerNet, 0, sizeof( headerNet ) );
    headerNet.magic           = htonl( c_magic );
    headerNet.version         = htons( c_version );
    headerNet.targetNameBytes = htonl( static_cast<uint32_t>( targetName.size() ) );
    headerNet.datatype        = htonl( headerHost.datatype );
    headerNet.width           = htonl( headerHost.width );
    headerNet.height          = htonl( headerHost.height );
    headerNet.depth           = htonl( headerHost.depth );
    headerNet.frameBytes      = hostToNet64( headerHost.frameBytes );
    headerNet.cnt0            = hostToNet64( headerHost.cnt0 );
    headerNet.cnt1            = hostToNet64( headerHost.cnt1 );
    headerNet.writetimeSec    = hostToNet64( static_cast<uint64_t>( headerHost.writetimeSec ) );
    headerNet.writetimeNsec   = hostToNet64( static_cast<uint64_t>( headerHost.writetimeNsec ) );
    headerNet.atimeSec        = hostToNet64( static_cast<uint64_t>( headerHost.atimeSec ) );
    headerNet.atimeNsec       = hostToNet64( static_cast<uint64_t>( headerHost.atimeNsec ) );

    std::lock_guard<std::mutex> lock( m_txMutex );
    if( m_txSockFd < 0 )
    {
        return 0;
    }

    bool ok = sendAll( m_txSockFd, &headerNet, sizeof( headerNet ) );
    if( ok )
    {
        ok = sendAll( m_txSockFd, targetName.data(), targetName.size() );
    }
    if( ok )
    {
        ok = sendAll( m_txSockFd, curr_src, m_frameBytes );
    }

    if( !ok )
    {
        log<text_log>( "transmit socket dropped; scheduling reconnect", logPrio::LOG_WARNING );
        closeTxSocketLocked();
        scheduleReconnect();
    }

    return 0;
}

inline uint64_t shmimTCP::hostToNet64( uint64_t v )
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return v;
#else
    uint32_t lo = static_cast<uint32_t>( v & 0xFFFFFFFFULL );
    uint32_t hi = static_cast<uint32_t>( ( v >> 32 ) & 0xFFFFFFFFULL );

    return ( static_cast<uint64_t>( htonl( lo ) ) << 32 ) | htonl( hi );
#endif
}

inline uint64_t shmimTCP::netToHost64( uint64_t v )
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return v;
#else
    uint32_t lo = static_cast<uint32_t>( v & 0xFFFFFFFFULL );
    uint32_t hi = static_cast<uint32_t>( ( v >> 32 ) & 0xFFFFFFFFULL );

    return ( static_cast<uint64_t>( ntohl( lo ) ) << 32 ) | ntohl( hi );
#endif
}

inline bool shmimTCP::sendAll( int fd, const void *data, size_t nBytes )
{
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>( data );
    size_t         rem = nBytes;

    while( rem > 0 )
    {
        ssize_t n = ::send( fd, ptr, rem, MSG_NOSIGNAL );
        if( n < 0 )
        {
            if( errno == EINTR )
            {
                continue;
            }
            return false;
        }

        if( n == 0 )
        {
            return false;
        }

        ptr += n;
        rem -= static_cast<size_t>( n );
    }

    return true;
}

inline bool shmimTCP::recvAll( int fd, void *data, size_t nBytes )
{
    uint8_t *ptr = reinterpret_cast<uint8_t *>( data );
    size_t   rem = nBytes;

    while( rem > 0 )
    {
        ssize_t n = ::recv( fd, ptr, rem, 0 );
        if( n < 0 )
        {
            if( errno == EINTR )
            {
                continue;
            }
            return false;
        }
        if( n == 0 )
        {
            return false;
        }

        ptr += n;
        rem -= static_cast<size_t>( n );
    }

    return true;
}

inline void shmimTCP::closeTxSocket()
{
    std::lock_guard<std::mutex> lock( m_txMutex );
    closeTxSocketLocked();
}

inline void shmimTCP::closeTxSocketLocked()
{
    if( m_txSockFd >= 0 )
    {
        ::shutdown( m_txSockFd, SHUT_RDWR );
        ::close( m_txSockFd );
        m_txSockFd = -1;
    }

    m_txConnected = false;
}

inline void shmimTCP::scheduleReconnect()
{
    m_nextReconnect = std::chrono::steady_clock::now() + std::chrono::seconds( m_reconnectSec );
}

inline int shmimTCP::connectWithTimeout( int fd, const sockaddr *sa, socklen_t saLen, int timeoutSec )
{
    int flags = fcntl( fd, F_GETFL, 0 );
    if( flags < 0 )
    {
        return -1;
    }

    if( fcntl( fd, F_SETFL, flags | O_NONBLOCK ) < 0 )
    {
        return -1;
    }

    int rv = ::connect( fd, sa, saLen );
    if( rv == 0 )
    {
        fcntl( fd, F_SETFL, flags );
        return 0;
    }

    if( errno != EINPROGRESS )
    {
        return -1;
    }

    fd_set wfds;
    FD_ZERO( &wfds );
    FD_SET( fd, &wfds );

    timeval tv;
    tv.tv_sec  = timeoutSec;
    tv.tv_usec = 0;

    rv = select( fd + 1, nullptr, &wfds, nullptr, &tv );
    if( rv <= 0 )
    {
        return -1;
    }

    int       sockErr = 0;
    socklen_t slen    = sizeof( sockErr );
    if( getsockopt( fd, SOL_SOCKET, SO_ERROR, &sockErr, &slen ) < 0 )
    {
        return -1;
    }
    if( sockErr != 0 )
    {
        errno = sockErr;
        return -1;
    }

    if( fcntl( fd, F_SETFL, flags ) < 0 )
    {
        return -1;
    }

    return 0;
}

inline int shmimTCP::connectRemote()
{
    std::string host;
    int         port;
    {
        std::lock_guard<std::mutex> lock( m_paramMutex );
        host = m_remoteHost;
        port = m_remotePort;
    }

    if( host.empty() || port < 1 || port > 65535 )
    {
        scheduleReconnect();
        return -1;
    }

    std::string portStr = std::to_string( port );

    struct addrinfo hints;
    std::memset( &hints, 0, sizeof( hints ) );
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    if( getaddrinfo( host.c_str(), portStr.c_str(), &hints, &res ) != 0 )
    {
        scheduleReconnect();
        return -1;
    }

    int fd = -1;
    for( struct addrinfo *it = res; it != nullptr; it = it->ai_next )
    {
        fd = socket( it->ai_family, it->ai_socktype, it->ai_protocol );
        if( fd < 0 )
        {
            continue;
        }

        int one = 1;
        setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof( one ) );

        timeval tv;
        tv.tv_sec  = m_socketTimeoutSec;
        tv.tv_usec = 0;
        setsockopt( fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof( tv ) );
        setsockopt( fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) );

        if( connectWithTimeout( fd, it->ai_addr, it->ai_addrlen, m_connectTimeoutSec ) == 0 )
        {
            break;
        }

        ::close( fd );
        fd = -1;
    }

    freeaddrinfo( res );

    if( fd < 0 )
    {
        scheduleReconnect();
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock( m_txMutex );
        closeTxSocketLocked();
        m_txSockFd = fd;
    }

    m_txConnected = true;
    m_nextReconnect = std::chrono::steady_clock::now() + std::chrono::seconds( m_reconnectSec );

    log<text_log>( "connected to " + host + ":" + std::to_string( port ), logPrio::LOG_NOTICE );

    return 0;
}

inline int shmimTCP::openListenSocket()
{
    std::string bindAddr;
    int         port;
    {
        std::lock_guard<std::mutex> lock( m_paramMutex );
        bindAddr = m_listenAddress;
        port     = m_remotePort;
    }

    std::string portStr = std::to_string( port );

    struct addrinfo hints;
    std::memset( &hints, 0, sizeof( hints ) );
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    struct addrinfo *res = nullptr;
    const char *bindAddrPtr = nullptr;
    if( !bindAddr.empty() )
    {
        bindAddrPtr = bindAddr.c_str();
    }

    if( getaddrinfo( bindAddrPtr, portStr.c_str(), &hints, &res ) != 0 )
    {
        return -1;
    }

    int fd = -1;
    for( struct addrinfo *it = res; it != nullptr; it = it->ai_next )
    {
        fd = socket( it->ai_family, it->ai_socktype, it->ai_protocol );
        if( fd < 0 )
        {
            continue;
        }

        int one = 1;
        setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof( one ) );

        if( bind( fd, it->ai_addr, it->ai_addrlen ) == 0 && listen( fd, 2 ) == 0 )
        {
            break;
        }

        ::close( fd );
        fd = -1;
    }

    freeaddrinfo( res );

    if( fd < 0 )
    {
        return -1;
    }

    timeval tv;
    tv.tv_sec  = m_socketTimeoutSec;
    tv.tv_usec = 0;
    setsockopt( fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) );
    setsockopt( fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof( tv ) );

    m_listenFd = fd;
    return 0;
}

inline void shmimTCP::closeRxSockets()
{
    if( m_clientFd >= 0 )
    {
        ::shutdown( m_clientFd, SHUT_RDWR );
        ::close( m_clientFd );
        m_clientFd = -1;
    }

    if( m_listenFd >= 0 )
    {
        ::shutdown( m_listenFd, SHUT_RDWR );
        ::close( m_listenFd );
        m_listenFd = -1;
    }
}

inline void shmimTCP::receiveThreadExec()
{
    if( openListenSocket() < 0 )
    {
        log<software_error>( { __FILE__, __LINE__, "failed opening receive socket" } );
        m_rxThreadRunning = false;
        return;
    }

    m_rxListening = true;
    log<text_log>(
        "listening on " + m_listenAddress + ":" + std::to_string( m_remotePort ), logPrio::LOG_NOTICE );

    while( !shutdown() )
    {
        fd_set rfds;
        FD_ZERO( &rfds );
        FD_SET( m_listenFd, &rfds );

        timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int rv = select( m_listenFd + 1, &rfds, nullptr, nullptr, &tv );
        if( rv < 0 )
        {
            if( errno == EINTR )
            {
                continue;
            }
            break;
        }
        if( rv == 0 )
        {
            continue;
        }

        int clientFd = accept( m_listenFd, nullptr, nullptr );
        if( clientFd < 0 )
        {
            if( errno == EINTR )
            {
                continue;
            }
            continue;
        }

        timeval sotv;
        sotv.tv_sec  = m_socketTimeoutSec;
        sotv.tv_usec = 0;
        setsockopt( clientFd, SOL_SOCKET, SO_RCVTIMEO, &sotv, sizeof( sotv ) );
        setsockopt( clientFd, SOL_SOCKET, SO_SNDTIMEO, &sotv, sizeof( sotv ) );

        m_clientFd          = clientFd;
        m_rxClientConnected = true;
        log<text_log>( "receiver accepted client", logPrio::LOG_NOTICE );

        receiveClient( clientFd );

        ::shutdown( clientFd, SHUT_RDWR );
        ::close( clientFd );
        m_clientFd          = -1;
        m_rxClientConnected = false;
    }

    closeRxSockets();
    m_rxListening     = false;
    m_rxClientConnected = false;
    m_rxThreadRunning = false;
}

inline int shmimTCP::receiveClient( int clientFd )
{
    while( !shutdown() )
    {
        frameHeaderNet netHeader;
        if( !recvAll( clientFd, &netHeader, sizeof( netHeader ) ) )
        {
            return 0;
        }

        if( ntohl( netHeader.magic ) != c_magic || ntohs( netHeader.version ) != c_version )
        {
            log<software_error>( { __FILE__, __LINE__, "invalid shmimTCP frame header" } );
            return -1;
        }

        uint32_t targetNameBytes = ntohl( netHeader.targetNameBytes );
        if( targetNameBytes > c_maxTargetName )
        {
            log<software_error>( { __FILE__, __LINE__, "target name too long" } );
            return -1;
        }

        frameHeaderHost header;
        header.datatype      = ntohl( netHeader.datatype );
        header.width         = ntohl( netHeader.width );
        header.height        = ntohl( netHeader.height );
        header.depth         = ntohl( netHeader.depth );
        header.frameBytes    = netToHost64( netHeader.frameBytes );
        header.cnt0          = netToHost64( netHeader.cnt0 );
        header.cnt1          = netToHost64( netHeader.cnt1 );
        header.writetimeSec  = static_cast<int64_t>( netToHost64( netHeader.writetimeSec ) );
        header.writetimeNsec = static_cast<int64_t>( netToHost64( netHeader.writetimeNsec ) );
        header.atimeSec      = static_cast<int64_t>( netToHost64( netHeader.atimeSec ) );
        header.atimeNsec     = static_cast<int64_t>( netToHost64( netHeader.atimeNsec ) );

        if( header.width == 0 || header.height == 0 || header.frameBytes == 0 )
        {
            log<software_error>( { __FILE__, __LINE__, "invalid frame geometry in header" } );
            return -1;
        }

        size_t recvTypeSize = ImageStreamIO_typesize( static_cast<uint8_t>( header.datatype ) );
        if( recvTypeSize == 0 )
        {
            log<software_error>( { __FILE__, __LINE__, "invalid datatype in frame header" } );
            return -1;
        }

        uint64_t expectedBytes = static_cast<uint64_t>( header.width ) * static_cast<uint64_t>( header.height ) *
                                 static_cast<uint64_t>( recvTypeSize );
        if( expectedBytes != header.frameBytes )
        {
            log<software_error>( { __FILE__, __LINE__, "frame byte count does not match geometry/datatype" } );
            return -1;
        }

        if( header.frameBytes > std::numeric_limits<size_t>::max() )
        {
            log<software_error>( { __FILE__, __LINE__, "frame size exceeds platform size_t" } );
            return -1;
        }

        std::string targetName;
        targetName.resize( targetNameBytes );
        if( targetNameBytes > 0 )
        {
            if( !recvAll( clientFd, targetName.data(), targetNameBytes ) )
            {
                return 0;
            }
        }

        if( targetName.empty() )
        {
            std::lock_guard<std::mutex> lock( m_paramMutex );
            targetName = m_targetShmim;
        }

        if( m_rxFrameBuffer.size() != header.frameBytes )
        {
            m_rxFrameBuffer.resize( static_cast<size_t>( header.frameBytes ) );
        }

        if( !recvAll( clientFd, m_rxFrameBuffer.data(), m_rxFrameBuffer.size() ) )
        {
            return 0;
        }

        if( ensureTargetStream( targetName, header.width, header.height, header.depth, static_cast<uint8_t>( header.datatype ) ) <
            0 )
        {
            return -1;
        }

        if( writeFrameToTarget( header, m_rxFrameBuffer.data() ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

inline int shmimTCP::ensureTargetStream(
    const std::string &name, uint32_t width, uint32_t height, uint32_t depth, uint8_t datatype )
{
    if( depth == 0 )
    {
        depth = 1;
    }

    if( m_targetImageOpen && m_targetImageName == name && m_targetWidth == width && m_targetHeight == height &&
        m_targetDepth == depth && m_targetDataType == datatype )
    {
        return 0;
    }

    destroyTargetStream();

    uint32_t imsize[3];
    imsize[0] = width;
    imsize[1] = height;
    imsize[2] = depth;

    if( ImageStreamIO_createIm_gpu( &m_targetImage,
                                    name.c_str(),
                                    3,
                                    imsize,
                                    datatype,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "failed creating target shmim " + name } );
    }

    m_targetImage.md->cnt1 = depth - 1;

    m_targetImageOpen = true;
    m_targetImageName = name;
    m_targetWidth     = width;
    m_targetHeight    = height;
    m_targetDepth     = depth;
    m_targetDataType  = datatype;
    m_targetTypeSize  = ImageStreamIO_typesize( datatype );
    if( m_targetTypeSize == 0 )
    {
        destroyTargetStream();
        return log<software_error, -1>( { __FILE__, __LINE__, "invalid target datatype" } );
    }

    log<text_log>( "created target stream " + name + " " + std::to_string( width ) + "x" + std::to_string( height ) +
                       "x" + std::to_string( depth ) + " dtype=" + std::to_string( datatype ),
                   logPrio::LOG_NOTICE );

    return 0;
}

inline void shmimTCP::destroyTargetStream()
{
    if( !m_targetImageOpen )
    {
        return;
    }

    ImageStreamIO_destroyIm( &m_targetImage );
    std::memset( &m_targetImage, 0, sizeof( m_targetImage ) );

    m_targetImageOpen = false;
    m_targetImageName.clear();
    m_targetWidth    = 0;
    m_targetHeight   = 0;
    m_targetDepth    = 1;
    m_targetDataType = 0;
    m_targetTypeSize = 0;
}

inline int shmimTCP::writeFrameToTarget( const frameHeaderHost &header, const uint8_t *frameData )
{
    if( !m_targetImageOpen || m_targetTypeSize == 0 )
    {
        return -1;
    }

    size_t expectedBytes =
        static_cast<size_t>( m_targetWidth ) * static_cast<size_t>( m_targetHeight ) * m_targetTypeSize;
    if( header.frameBytes != expectedBytes )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "incoming frame size mismatch for target shmim" } );
    }

    uint64_t idx = 0;
    if( m_targetDepth > 1 )
    {
        idx = header.cnt1 % m_targetDepth;
    }

    char *dest = reinterpret_cast<char *>( m_targetImage.array.raw ) + idx * expectedBytes;

    m_targetImage.md->write = 1;
    std::memcpy( dest, frameData, expectedBytes );

    m_targetImage.md->cnt0 = header.cnt0;
    m_targetImage.md->cnt1 = idx;

    m_targetImage.md->writetime.tv_sec  = header.writetimeSec;
    m_targetImage.md->writetime.tv_nsec = header.writetimeNsec;
    m_targetImage.md->atime.tv_sec      = header.atimeSec;
    m_targetImage.md->atime.tv_nsec     = header.atimeNsec;

    if( m_targetImage.cntarray != nullptr )
    {
        m_targetImage.cntarray[idx] = header.cnt0;
    }

    if( m_targetImage.writetimearray != nullptr )
    {
        m_targetImage.writetimearray[idx] = m_targetImage.md->writetime;
    }

    if( m_targetImage.atimearray != nullptr )
    {
        m_targetImage.atimearray[idx] = m_targetImage.md->atime;
    }

    m_targetImage.md->write = 0;
    ImageStreamIO_sempost( &m_targetImage, -1 );

    return 0;
}

INDI_NEWCALLBACK_DEFN( shmimTCP, m_indiP_sourceShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_sourceShmim, ipRecv );

    std::string source;
    if( indiTargetUpdate( m_indiP_sourceShmim, source, ipRecv, true ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( source.empty() )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "source shmim cannot be empty" } );
    }

    {
        std::lock_guard<std::mutex> lock( m_paramMutex );
        m_sourceShmim          = source;
        shmimMonitorT::m_shmimName = m_sourceShmim;
        shmimMonitorT::m_restart   = true;
    }

    updateIfChanged( m_indiP_sourceShmim, "current", m_sourceShmim, INDI_OK );
    updateIfChanged( m_indiP_sourceShmim, "target", m_sourceShmim, INDI_OK );

    log<text_log>( "set source shmim to " + m_sourceShmim, logPrio::LOG_NOTICE );

    return 0;
}

INDI_NEWCALLBACK_DEFN( shmimTCP, m_indiP_targetShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_targetShmim, ipRecv );

    std::string target;
    if( indiTargetUpdate( m_indiP_targetShmim, target, ipRecv, true ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__ } );
    }

    if( target.empty() )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "target shmim cannot be empty" } );
    }

    if( target.size() > c_maxTargetName )
    {
        target.resize( c_maxTargetName );
    }

    {
        std::lock_guard<std::mutex> lock( m_paramMutex );
        m_targetShmim = target;
    }

    updateIfChanged( m_indiP_targetShmim, "current", m_targetShmim, INDI_OK );
    updateIfChanged( m_indiP_targetShmim, "target", m_targetShmim, INDI_OK );

    log<text_log>( "set target shmim to " + m_targetShmim, logPrio::LOG_NOTICE );

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // shmimTCP_hpp

