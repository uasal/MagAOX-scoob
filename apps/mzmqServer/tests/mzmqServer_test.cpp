/** \file mzmqServer_test.cpp
 * \brief Catch2 tests for the mzmqServer app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup mzmqServer_files
 */

#include "../../../tests/testXWC.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// mzmqServer.hpp includes <sys/syscall.h> inside namespace MagAOX::app; including it here first makes that a no-op.
#include <sys/syscall.h>
#include <unistd.h>

// milkzmqServer.hpp is found in stubs/ (see stubs/milkzmqServer.hpp)
#include "../mzmqServer.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Controllable state of the milkzmq server stand-in.
struct milkzmqServerStubState
{
    int serverThreadStartReturn{ 0 }; ///< Value returned by `serverThreadStart()`; no thread is launched if not 0.

    bool serverThreadExits{ false }; ///< Whether the server thread exits immediately.

    int imageThreadStartReturn{ 0 }; ///< Value returned by `imageThreadStart()`; no thread is launched if not 0.

    int exitThread{ -1 }; ///< Number of an image thread which exits immediately, -1 for none.

    int serverThreadStartCalls{ 0 }; ///< Number of calls to `serverThreadStart()`.

    int serverThreadKillCalls{ 0 }; ///< Number of calls to `serverThreadKill()`.

    int defaultCompressionCalls{ 0 }; ///< Number of calls to `defaultCompression()`.

    std::vector<size_t> imageThreadStartCalls; ///< Thread numbers passed to `imageThreadStart()`.

    std::vector<size_t> imageThreadKillCalls; ///< Thread numbers passed to `imageThreadKill()`.
};

/// The milkzmq server stand-in state, reset by each harness.
milkzmqServerStubState g_milkzmqServerStub;

namespace milkzmq
{

std::atomic_bool milkzmqServer::m_timeToDie{ false };

/// Thread body which waits until `m_timeToDie` is set.
void waitForTimeToDie()
{
    while( !milkzmqServer::m_timeToDie.load( std::memory_order_relaxed ) )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
}

milkzmqServer::milkzmqServer()
{
}

milkzmqServer::~milkzmqServer()
{
    m_timeToDie.store( true, std::memory_order_relaxed );

    if( m_serverThread.joinable() )
    {
        m_serverThread.join();
    }

    for( size_t n = 0; n < m_imageThreads.size(); ++n )
    {
        if( m_imageThreads[n].m_thread != nullptr )
        {
            if( m_imageThreads[n].m_thread->joinable() )
            {
                m_imageThreads[n].m_thread->join();
            }
            delete m_imageThreads[n].m_thread;
            m_imageThreads[n].m_thread = nullptr;
        }
    }
}

int milkzmqServer::imagePort()
{
    return m_imagePort;
}

int milkzmqServer::shMemImName( const std::string &name )
{
    s_imageThread nt;

    nt.m_mzs       = this;
    nt.m_imageName = name;

    m_imageThreads.push_back( nt );

    return 0;
}

size_t milkzmqServer::numImages()
{
    return m_imageThreads.size();
}

std::string milkzmqServer::shMemImName( size_t n )
{
    return m_imageThreads[n].m_imageName;
}

int milkzmqServer::usecSleep()
{
    return m_usecSleep;
}

float milkzmqServer::fpsTgt()
{
    return m_fpsTgt;
}

float milkzmqServer::fpsGain()
{
    return m_fpsGain;
}

void milkzmqServer::noCompression()
{
    m_xrifDifferenceMethod = XRIF_DIFFERENCE_NONE;
    m_xrifReorderMethod    = XRIF_REORDER_NONE;
    m_xrifCompressMethod   = XRIF_COMPRESS_NONE;
}

void milkzmqServer::defaultCompression()
{
    ++g_milkzmqServerStub.defaultCompressionCalls;

    m_xrifDifferenceMethod = XRIF_DIFFERENCE_PIXEL;
    m_xrifReorderMethod    = XRIF_REORDER_BYTEPACK_RENIBBLE;
    m_xrifCompressMethod   = XRIF_COMPRESS_LZ4;
}

int milkzmqServer::xrifDifferenceMethod()
{
    return m_xrifDifferenceMethod;
}

int milkzmqServer::xrifReorderMethod()
{
    return m_xrifReorderMethod;
}

int milkzmqServer::xrifCompressMethod()
{
    return m_xrifCompressMethod;
}

int milkzmqServer::serverThreadStart()
{
    ++g_milkzmqServerStub.serverThreadStartCalls;

    if( g_milkzmqServerStub.serverThreadStartReturn != 0 )
    {
        if( g_milkzmqServerStub.serverThreadStartReturn < 0 )
        {
            reportError( "stub server thread start failure", __FILE__, __LINE__ );
        }

        return g_milkzmqServerStub.serverThreadStartReturn;
    }

    if( g_milkzmqServerStub.serverThreadExits )
    {
        m_serverThread = std::thread( []() {} );
    }
    else
    {
        m_serverThread = std::thread( waitForTimeToDie );
    }

    return 0;
}

int milkzmqServer::serverThreadKill()
{
    ++g_milkzmqServerStub.serverThreadKillCalls;
    m_timeToDie.store( true, std::memory_order_relaxed );
    return 0;
}

int milkzmqServer::imageThreadStart( size_t thno )
{
    g_milkzmqServerStub.imageThreadStartCalls.push_back( thno );

    if( g_milkzmqServerStub.imageThreadStartReturn != 0 )
    {
        if( g_milkzmqServerStub.imageThreadStartReturn < 0 )
        {
            reportError( "stub image thread start failure", __FILE__, __LINE__ );
        }

        return g_milkzmqServerStub.imageThreadStartReturn;
    }

    if( g_milkzmqServerStub.exitThread == static_cast<int>( thno ) )
    {
        *m_imageThreads[thno].m_thread = std::thread( []() {} );
    }
    else
    {
        *m_imageThreads[thno].m_thread = std::thread( waitForTimeToDie );
    }

    return 0;
}

int milkzmqServer::imageThreadKill( size_t thno )
{
    g_milkzmqServerStub.imageThreadKillCalls.push_back( thno );
    m_timeToDie.store( true, std::memory_order_relaxed );
    return 0;
}

void milkzmqServer::reportInfo( const std::string &msg )
{
    std::cerr << "milkzmqServer stub info: " << msg << "\n";
}

void milkzmqServer::reportNotice( const std::string &msg )
{
    std::cerr << "milkzmqServer stub notice: " << msg << "\n";
}

void milkzmqServer::reportWarning( const std::string &msg )
{
    std::cerr << "milkzmqServer stub warning: " << msg << "\n";
}

void milkzmqServer::reportError( const std::string &msg, const std::string &file, int line )
{
    std::cerr << "milkzmqServer stub error: " << msg << " " << file << " " << line << "\n";
}

} // namespace milkzmq

/// \endcond

namespace libXWCTest
{

/** \defgroup mzmqServer_unit_test mzmqServer Unit Tests
 * \brief Unit tests for the mzmqServer application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `mzmqServer` unit tests.
/** \ingroup mzmqServer_unit_test
 */
namespace mzmqServerTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing mzmqServer internals.
class mzmqServer_test : public mzmqServer
{
  public:
    /// Construct a harness with the given device name, resetting the stand-in state.
    explicit mzmqServer_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        g_milkzmqServerStub = milkzmqServerStubState();
        m_timeToDie.store( false, std::memory_order_relaxed );
    }

    using mzmqServer::m_argv0;
    using mzmqServer::m_compress;
    using mzmqServer::m_powerMgtEnabled;
    using mzmqServer::m_shMemImNames;
    using mzmqServer::reportError;
    using mzmqServer::reportInfo;
    using mzmqServer::reportNotice;
    using mzmqServer::reportWarning;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Check whether the server thread is joinable.
    /**
     * \returns true if the server thread is joinable
     */
    bool serverJoinable()
    {
        return m_serverThread.joinable();
    }

    /// Check whether an image thread is joinable.
    /**
     * \returns true if the thread is joinable
     */
    bool threadJoinable( size_t n /**< [in] the thread number */ )
    {
        return m_imageThreads[n].m_thread->joinable();
    }

    /// Replace the server thread object after `appLogic()` joined it with `pthread_tryjoin_np()`.
    /** The old `std::thread` still believes it is joinable, so it is moved into an intentionally leaked object
     * instead of being joined again or destroyed (which would call `std::terminate()`).
     */
    void abandonServerThread()
    {
        std::thread *leaked = new std::thread;
        std::swap( *leaked, m_serverThread );
    }

    /// Replace an image thread object after `appLogic()` joined it with `pthread_tryjoin_np()`.
    /** The old `std::thread` is intentionally leaked, see abandonServerThread().
     */
    void abandonThread( size_t n /**< [in] the thread number */ )
    {
        m_imageThreads[n].m_thread = new std::thread;
    }

    /// Call `appLogic()` until it reports an exited thread or the attempts run out.
    /**
     * \returns the last value returned by `appLogic()`
     */
    int appLogicUntilExit()
    {
        int rv = 0;
        for( int n = 0; n < 1000 && rv == 0; ++n )
        {
            rv = appLogic();
            if( rv == 0 )
            {
                std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
            }
        }
        return rv;
    }
};
/// \endcond

/// Verify the mzmqServer configuration defaults.
/**
 * \ingroup mzmqServer_unit_test
 */
TEST_CASE( "mzmqServer configuration defaults", "[mzmqServer]" )
{
    // clang-format off
    #ifdef MZMQSERVER_TEST_DOXYGEN_REF
    mzmqServer::mzmqServer();
    mzmqServer::setupConfig();
    mzmqServer::loadConfig();
    #endif
    // clang-format on

    mzmqServer_test app( "mzs" );

    mx::app::writeConfigFile( "/tmp/mzmqServer_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.configure( "/tmp/mzmqServer_test_defaults.conf" );

    REQUIRE( app.m_argv0 == "mzs" );
    REQUIRE( app.imagePort() == 5556 );
    REQUIRE( app.m_shMemImNames.empty() );
    REQUIRE( app.usecSleep() == 100 );
    REQUIRE( app.fpsTgt() == Approx( 10.0 ) );
    REQUIRE( app.fpsGain() == Approx( 0.1 ) );
    REQUIRE( app.m_compress == false );
    REQUIRE( app.m_powerMgtEnabled == false );

    std::remove( "/tmp/mzmqServer_test_defaults.conf" );
}

/// Verify the mzmqServer configuration overrides.
/**
 * \ingroup mzmqServer_unit_test
 */
TEST_CASE( "mzmqServer configuration overrides", "[mzmqServer]" )
{
    // clang-format off
    #ifdef MZMQSERVER_TEST_DOXYGEN_REF
    mzmqServer::setupConfig();
    mzmqServer::loadConfig();
    #endif
    // clang-format on

    mzmqServer_test app( "mzs" );

    mx::app::writeConfigFile( "/tmp/mzmqServer_test_overrides.conf",
                              { "server", "server", "server", "server", "server", "server" },
                              { "imagePort", "shmimNames", "usecSleep", "fpsTgt", "fpsGain", "compress" },
                              { "6001", "camwfs,camtip,dm00disp", "250", "25.5", "0.25", "true" } );
    app.configure( "/tmp/mzmqServer_test_overrides.conf" );

    REQUIRE( app.m_argv0 == "mzs" );
    REQUIRE( app.imagePort() == 6001 );
    REQUIRE( app.m_shMemImNames.size() == 3 );
    REQUIRE( app.m_shMemImNames[0] == "camwfs" );
    REQUIRE( app.m_shMemImNames[1] == "camtip" );
    REQUIRE( app.m_shMemImNames[2] == "dm00disp" );
    REQUIRE( app.usecSleep() == 250 );
    REQUIRE( app.fpsTgt() == Approx( 25.5 ) );
    REQUIRE( app.fpsGain() == Approx( 0.25 ) );
    REQUIRE( app.m_compress == true );

    std::remove( "/tmp/mzmqServer_test_overrides.conf" );
}

/// Verify `appStartup()` applies compression only when configured.
/**
 * \ingroup mzmqServer_unit_test
 */
TEST_CASE( "mzmqServer appStartup compression", "[mzmqServer]" )
{
    // clang-format off
    #ifdef MZMQSERVER_TEST_DOXYGEN_REF
    mzmqServer::appStartup();
    #endif
    // clang-format on

    mzmqServer_test app( "mzs" );

    SECTION( "compression off" )
    {
        REQUIRE( app.appStartup() == 0 );

        REQUIRE( g_milkzmqServerStub.defaultCompressionCalls == 0 );
        REQUIRE( app.xrifDifferenceMethod() == XRIF_DIFFERENCE_NONE );
        REQUIRE( app.xrifReorderMethod() == XRIF_REORDER_NONE );
        REQUIRE( app.xrifCompressMethod() == XRIF_COMPRESS_NONE );
    }

    SECTION( "compression on" )
    {
        app.m_compress = true;

        REQUIRE( app.appStartup() == 0 );

        REQUIRE( g_milkzmqServerStub.defaultCompressionCalls == 1 );
        REQUIRE( app.xrifDifferenceMethod() == XRIF_DIFFERENCE_PIXEL );
        REQUIRE( app.xrifReorderMethod() == XRIF_REORDER_BYTEPACK_RENIBBLE );
        REQUIRE( app.xrifCompressMethod() == XRIF_COMPRESS_LZ4 );
    }

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify `appStartup()`, `appLogic()` and `appShutdown()` manage the server thread and one image thread per stream.
/**
 * \ingroup mzmqServer_unit_test
 */
TEST_CASE( "mzmqServer thread lifecycle", "[mzmqServer]" )
{
    // clang-format off
    #ifdef MZMQSERVER_TEST_DOXYGEN_REF
    mzmqServer::appStartup();
    mzmqServer::appLogic();
    mzmqServer::appShutdown();
    #endif
    // clang-format on

    mzmqServer_test app( "mzs" );
    app.m_shMemImNames = { "camwfs", "camtip" };

    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.numImages() == 2 );
    REQUIRE( app.shMemImName( 0 ) == "camwfs" );
    REQUIRE( app.shMemImName( 1 ) == "camtip" );

    REQUIRE( g_milkzmqServerStub.serverThreadStartCalls == 1 );
    REQUIRE( g_milkzmqServerStub.imageThreadStartCalls == std::vector<size_t>( { 0, 1 } ) );
    REQUIRE( app.serverJoinable() );
    REQUIRE( app.threadJoinable( 0 ) );
    REQUIRE( app.threadJoinable( 1 ) );

    // all threads are still running
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.appLogic() == 0 );

    REQUIRE( app.appShutdown() == 0 );

    REQUIRE( milkzmq::milkzmqServer::m_timeToDie.load() == true );
    REQUIRE( g_milkzmqServerStub.serverThreadKillCalls == 1 );
    REQUIRE( g_milkzmqServerStub.imageThreadKillCalls == std::vector<size_t>( { 0, 1 } ) );
    REQUIRE( app.serverJoinable() == false );
    REQUIRE( app.threadJoinable( 0 ) == false );
    REQUIRE( app.threadJoinable( 1 ) == false );
}

/// Verify `appStartup()` fails when the server or an image thread cannot be started.
/**
 * \ingroup mzmqServer_unit_test
 */
TEST_CASE( "mzmqServer appStartup thread start failures", "[mzmqServer]" )
{
    // clang-format off
    #ifdef MZMQSERVER_TEST_DOXYGEN_REF
    mzmqServer::appStartup();
    #endif
    // clang-format on

    mzmqServer_test app( "mzs" );
    app.m_shMemImNames = { "camwfs", "camtip" };

    SECTION( "server thread start failure" )
    {
        g_milkzmqServerStub.serverThreadStartReturn = -1;

        REQUIRE( app.appStartup() == -1 );

        REQUIRE( g_milkzmqServerStub.serverThreadStartCalls == 1 );
        REQUIRE( g_milkzmqServerStub.imageThreadStartCalls.empty() );
        REQUIRE( app.serverJoinable() == false );
    }

    SECTION( "image thread start failure" )
    {
        // appStartup() treats a positive return from imageThreadStart() as a failure
        g_milkzmqServerStub.imageThreadStartReturn = 1;

        REQUIRE( app.appStartup() == -1 );

        REQUIRE( g_milkzmqServerStub.serverThreadStartCalls == 1 );
        REQUIRE( g_milkzmqServerStub.imageThreadStartCalls == std::vector<size_t>( { 0 } ) );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( app.serverJoinable() == false );
    }
}

/// Verify `appLogic()` reports a server or image thread which has exited.
/**
 * \ingroup mzmqServer_unit_test
 */
TEST_CASE( "mzmqServer appLogic detects exited threads", "[mzmqServer]" )
{
    // clang-format off
    #ifdef MZMQSERVER_TEST_DOXYGEN_REF
    mzmqServer::appLogic();
    #endif
    // clang-format on

    mzmqServer_test app( "mzs" );
    app.m_shMemImNames = { "camwfs", "camtip" };

    SECTION( "server thread exits" )
    {
        g_milkzmqServerStub.serverThreadExits = true;

        REQUIRE( app.appStartup() == 0 );

        REQUIRE( app.appLogicUntilExit() == -1 );

        // appLogic() joined the exited server thread
        app.abandonServerThread();

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( app.threadJoinable( 0 ) == false );
        REQUIRE( app.threadJoinable( 1 ) == false );
    }

    SECTION( "image thread exits" )
    {
        // the second image thread exits immediately while the others keep running
        g_milkzmqServerStub.exitThread = 1;

        REQUIRE( app.appStartup() == 0 );

        REQUIRE( app.appLogicUntilExit() == -1 );

        // appLogic() joined the exited image thread
        app.abandonThread( 1 );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( app.serverJoinable() == false );
        REQUIRE( app.threadJoinable( 0 ) == false );
    }
}

/// Verify the milkzmq status and error reports are logged without error.
/**
 * \ingroup mzmqServer_unit_test
 */
TEST_CASE( "mzmqServer status and error reporting", "[mzmqServer]" )
{
    // clang-format off
    #ifdef MZMQSERVER_TEST_DOXYGEN_REF
    mzmqServer::reportInfo( "" );
    mzmqServer::reportNotice( "" );
    mzmqServer::reportWarning( "" );
    mzmqServer::reportError( "", "", 0 );
    #endif
    // clang-format on

    mzmqServer_test app( "mzs" );

    REQUIRE_NOTHROW( app.reportInfo( "info message" ) );
    REQUIRE_NOTHROW( app.reportNotice( "notice message" ) );
    REQUIRE_NOTHROW( app.reportWarning( "warning message" ) );
    REQUIRE_NOTHROW( app.reportError( "error message", __FILE__, __LINE__ ) );

    // the app overrides are called through the milkzmq base class
    milkzmq::milkzmqServer &base = app;
    REQUIRE_NOTHROW( base.reportError( "error through the base", __FILE__, __LINE__ ) );
}

} // namespace mzmqServerTest

} // namespace libXWCTest
