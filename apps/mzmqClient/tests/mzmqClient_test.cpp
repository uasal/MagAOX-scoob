/** \file mzmqClient_test.cpp
 * \brief Catch2 tests for the mzmqClient app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup mzmqClient_files
 */

#include "../../../tests/testXWC.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// milkzmqClient.hpp is found in stubs/ (see stubs/milkzmqClient.hpp)
#include "../mzmqClient.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Controllable state of the milkzmq client stand-in.
struct milkzmqClientStubState
{
    int imageThreadStartReturn{ 0 }; ///< Value returned by `imageThreadStart()`; no thread is launched if not 0.

    int exitThread{ -1 }; ///< Number of an image thread which exits immediately, -1 for none.

    std::vector<size_t> imageThreadStartCalls; ///< Thread numbers passed to `imageThreadStart()`.

    std::vector<size_t> imageThreadKillCalls; ///< Thread numbers passed to `imageThreadKill()`.
};

/// The milkzmq client stand-in state, reset by each harness.
milkzmqClientStubState g_milkzmqClientStub;

namespace milkzmq
{

std::atomic_bool milkzmqClient::m_timeToDie{ false };

milkzmqClient::milkzmqClient()
{
}

milkzmqClient::~milkzmqClient()
{
    m_timeToDie.store( true, std::memory_order_relaxed );

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

int milkzmqClient::shMemImName( const std::string &name )
{
    return shMemImName( name, name );
}

int milkzmqClient::shMemImName( const std::string &name, const std::string &localName )
{
    s_imageThread nt;

    nt.m_mzc            = this;
    nt.m_imageName      = name;
    nt.m_localImageName = localName;

    m_imageThreads.push_back( nt );

    return 0;
}

std::string milkzmqClient::shMemImName( size_t imno )
{
    if( imno >= m_imageThreads.size() )
    {
        return "";
    }

    return m_imageThreads[imno].m_imageName;
}

std::string milkzmqClient::localShMemImName( size_t imno )
{
    if( imno >= m_imageThreads.size() )
    {
        return "";
    }

    return m_imageThreads[imno].m_localImageName;
}

int milkzmqClient::imageThreadStart( size_t thno )
{
    g_milkzmqClientStub.imageThreadStartCalls.push_back( thno );

    if( g_milkzmqClientStub.imageThreadStartReturn != 0 )
    {
        if( g_milkzmqClientStub.imageThreadStartReturn < 0 )
        {
            reportError( "stub image thread start failure", __FILE__, __LINE__ );
        }

        return g_milkzmqClientStub.imageThreadStartReturn;
    }

    if( g_milkzmqClientStub.exitThread == static_cast<int>( thno ) )
    {
        *m_imageThreads[thno].m_thread = std::thread( []() {} );
    }
    else
    {
        *m_imageThreads[thno].m_thread = std::thread(
            []()
            {
                while( !m_timeToDie.load( std::memory_order_relaxed ) )
                {
                    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
                }
            } );
    }

    return 0;
}

int milkzmqClient::imageThreadKill( size_t thno )
{
    g_milkzmqClientStub.imageThreadKillCalls.push_back( thno );
    m_timeToDie.store( true, std::memory_order_relaxed );
    return 0;
}

void milkzmqClient::reportInfo( const std::string &msg )
{
    std::cerr << "milkzmqClient stub info: " << msg << "\n";
}

void milkzmqClient::reportNotice( const std::string &msg )
{
    std::cerr << "milkzmqClient stub notice: " << msg << "\n";
}

void milkzmqClient::reportWarning( const std::string &msg )
{
    std::cerr << "milkzmqClient stub warning: " << msg << "\n";
}

void milkzmqClient::reportError( const std::string &msg, const std::string &file, int line )
{
    std::cerr << "milkzmqClient stub error: " << msg << " " << file << " " << line << "\n";
}

} // namespace milkzmq

/// \endcond

namespace libXWCTest
{

/** \defgroup mzmqClient_unit_test mzmqClient Unit Tests
 * \brief Unit tests for the mzmqClient application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `mzmqClient` unit tests.
/** \ingroup mzmqClient_unit_test
 */
namespace mzmqClientTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing mzmqClient internals.
class mzmqClient_test : public mzmqClient
{
  public:
    /// Construct a harness with the given device name, resetting the stand-in state.
    explicit mzmqClient_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        g_milkzmqClientStub = milkzmqClientStubState();
        m_timeToDie.store( false, std::memory_order_relaxed );
    }

    using mzmqClient::m_address;
    using mzmqClient::m_argv0;
    using mzmqClient::m_imagePort;
    using mzmqClient::m_powerMgtEnabled;
    using mzmqClient::m_shMemImNames;
    using mzmqClient::reportError;
    using mzmqClient::reportInfo;
    using mzmqClient::reportNotice;
    using mzmqClient::reportWarning;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Get the number of image threads.
    /**
     * \returns the size of `m_imageThreads`
     */
    size_t numThreads()
    {
        return m_imageThreads.size();
    }

    /// Check whether an image thread is joinable.
    /**
     * \returns true if the thread is joinable
     */
    bool threadJoinable( size_t n /**< [in] the thread number */ )
    {
        return m_imageThreads[n].m_thread->joinable();
    }

    /// Replace a thread object that `appLogic()` already joined with `pthread_tryjoin_np()`.
    /** The old `std::thread` still believes it is joinable, so it is intentionally leaked instead of being
     * joined again or destroyed (which would call `std::terminate()`).
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

/// Verify the mzmqClient configuration defaults.
/**
 * \ingroup mzmqClient_unit_test
 */
TEST_CASE( "mzmqClient configuration defaults", "[mzmqClient]" )
{
    // clang-format off
    #ifdef MZMQCLIENT_TEST_DOXYGEN_REF
    mzmqClient::mzmqClient();
    mzmqClient::setupConfig();
    mzmqClient::loadConfig();
    #endif
    // clang-format on

    mzmqClient_test app( "mzc" );

    mx::app::writeConfigFile( "/tmp/mzmqClient_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.configure( "/tmp/mzmqClient_test_defaults.conf" );

    REQUIRE( app.m_argv0 == "mzc" );
    REQUIRE( app.m_address == "" );
    REQUIRE( app.m_imagePort == 5556 );
    REQUIRE( app.m_shMemImNames.empty() );
    REQUIRE( app.m_powerMgtEnabled == false );

    std::remove( "/tmp/mzmqClient_test_defaults.conf" );
}

/// Verify the mzmqClient configuration overrides.
/**
 * \ingroup mzmqClient_unit_test
 */
TEST_CASE( "mzmqClient configuration overrides", "[mzmqClient]" )
{
    // clang-format off
    #ifdef MZMQCLIENT_TEST_DOXYGEN_REF
    mzmqClient::setupConfig();
    mzmqClient::loadConfig();
    #endif
    // clang-format on

    mzmqClient_test app( "mzc" );

    mx::app::writeConfigFile( "/tmp/mzmqClient_test_overrides.conf",
                              { "server", "server", "server" },
                              { "address", "imagePort", "shmimNames" },
                              { "localhost", "7001", "camwfs,camtip" } );
    app.configure( "/tmp/mzmqClient_test_overrides.conf" );

    REQUIRE( app.m_argv0 == "mzc" );
    REQUIRE( app.m_address == "localhost" );
    REQUIRE( app.m_imagePort == 7001 );
    REQUIRE( app.m_shMemImNames.size() == 2 );
    REQUIRE( app.m_shMemImNames[0] == "camwfs" );
    REQUIRE( app.m_shMemImNames[1] == "camtip" );

    std::remove( "/tmp/mzmqClient_test_overrides.conf" );
}

/// Verify `appStartup()`, `appLogic()` and `appShutdown()` manage one image thread per configured stream.
/**
 * \ingroup mzmqClient_unit_test
 */
TEST_CASE( "mzmqClient image thread lifecycle", "[mzmqClient]" )
{
    // clang-format off
    #ifdef MZMQCLIENT_TEST_DOXYGEN_REF
    mzmqClient::appStartup();
    mzmqClient::appLogic();
    mzmqClient::appShutdown();
    #endif
    // clang-format on

    mzmqClient_test app( "mzc" );

    SECTION( "two streams" )
    {
        app.m_shMemImNames = { "camwfs", "camtip" };

        REQUIRE( app.appStartup() == 0 );

        REQUIRE( app.numThreads() == 2 );
        REQUIRE( app.shMemImName( 0 ) == "camwfs" );
        REQUIRE( app.localShMemImName( 0 ) == "camwfs" );
        REQUIRE( app.shMemImName( 1 ) == "camtip" );
        REQUIRE( app.localShMemImName( 1 ) == "camtip" );

        REQUIRE( g_milkzmqClientStub.imageThreadStartCalls == std::vector<size_t>( { 0, 1 } ) );
        REQUIRE( app.threadJoinable( 0 ) );
        REQUIRE( app.threadJoinable( 1 ) );

        // the threads are still running
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.appLogic() == 0 );

        REQUIRE( app.appShutdown() == 0 );

        REQUIRE( milkzmq::milkzmqClient::m_timeToDie.load() == true );
        REQUIRE( g_milkzmqClientStub.imageThreadKillCalls == std::vector<size_t>( { 0, 1 } ) );
        REQUIRE( app.threadJoinable( 0 ) == false );
        REQUIRE( app.threadJoinable( 1 ) == false );
    }

    SECTION( "no streams" )
    {
        REQUIRE( app.appStartup() == 0 );

        REQUIRE( app.numThreads() == 0 );
        REQUIRE( g_milkzmqClientStub.imageThreadStartCalls.empty() );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_milkzmqClientStub.imageThreadKillCalls.empty() );
    }
}

/// Verify `appStartup()` fails when an image thread start reports an error.
/**
 * \ingroup mzmqClient_unit_test
 */
TEST_CASE( "mzmqClient appStartup image thread start failure", "[mzmqClient]" )
{
    // clang-format off
    #ifdef MZMQCLIENT_TEST_DOXYGEN_REF
    mzmqClient::appStartup();
    #endif
    // clang-format on

    mzmqClient_test app( "mzc" );
    app.m_shMemImNames = { "camwfs", "camtip" };

    // appStartup() treats a positive return from imageThreadStart() as a failure
    g_milkzmqClientStub.imageThreadStartReturn = 1;

    REQUIRE( app.appStartup() == -1 );

    // startup stops at the first failure
    REQUIRE( g_milkzmqClientStub.imageThreadStartCalls == std::vector<size_t>( { 0 } ) );
    REQUIRE( app.threadJoinable( 0 ) == false );
}

/// Verify `appLogic()` reports an image thread which has exited.
/**
 * \ingroup mzmqClient_unit_test
 */
TEST_CASE( "mzmqClient appLogic detects an exited image thread", "[mzmqClient]" )
{
    // clang-format off
    #ifdef MZMQCLIENT_TEST_DOXYGEN_REF
    mzmqClient::appLogic();
    #endif
    // clang-format on

    mzmqClient_test app( "mzc" );
    app.m_shMemImNames = { "camwfs", "camtip" };

    // the second thread exits immediately while the first keeps running
    g_milkzmqClientStub.exitThread = 1;

    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.appLogicUntilExit() == -1 );

    // appLogic() joined the exited thread
    app.abandonThread( 1 );

    REQUIRE( app.appShutdown() == 0 );
    REQUIRE( app.threadJoinable( 0 ) == false );
}

/// Verify the milkzmq status and error reports are logged without error.
/**
 * \ingroup mzmqClient_unit_test
 */
TEST_CASE( "mzmqClient status and error reporting", "[mzmqClient]" )
{
    // clang-format off
    #ifdef MZMQCLIENT_TEST_DOXYGEN_REF
    mzmqClient::reportInfo( "" );
    mzmqClient::reportNotice( "" );
    mzmqClient::reportWarning( "" );
    mzmqClient::reportError( "", "", 0 );
    #endif
    // clang-format on

    mzmqClient_test app( "mzc" );

    REQUIRE_NOTHROW( app.reportInfo( "info message" ) );
    REQUIRE_NOTHROW( app.reportNotice( "notice message" ) );
    REQUIRE_NOTHROW( app.reportWarning( "warning message" ) );
    REQUIRE_NOTHROW( app.reportError( "error message", __FILE__, __LINE__ ) );

    // the app overrides are called through the milkzmq base class
    milkzmq::milkzmqClient &base = app;
    REQUIRE_NOTHROW( base.reportError( "error through the base", __FILE__, __LINE__ ) );
}

} // namespace mzmqClientTest

} // namespace libXWCTest
