/** \file pi335Ctrl_test.cpp
 * \brief Catch2 tests for the pi335Ctrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup pi335Ctrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../pi335Ctrl.hpp"

namespace MagAOX
{
namespace app
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing pi335Ctrl internals (a friend of pi335Ctrl).
class pi335Ctrl_test : public pi335Ctrl
{
  public:
    /// Construct a harness with the given device name.
    explicit pi335Ctrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        makePosProp( m_indiP_pos1, "pos_1" );
        makePosProp( m_indiP_pos2, "pos_2" );
        makePosProp( m_indiP_pos3, "pos_3" );

        m_flatCommand.resize( 3, 1 );
        m_flatCommand.setZero();
    }

    using pi335Ctrl::m_flatCommand;
    using pi335Ctrl::m_flatLoaded;
    using pi335Ctrl::m_flatSet;
    using pi335Ctrl::m_homePos1;
    using pi335Ctrl::m_homePos2;
    using pi335Ctrl::m_homePos3;
    using pi335Ctrl::m_homingStart;
    using pi335Ctrl::m_homingState;
    using pi335Ctrl::m_indiP_pos1;
    using pi335Ctrl::m_indiP_pos2;
    using pi335Ctrl::m_indiP_pos3;
    using pi335Ctrl::m_max1;
    using pi335Ctrl::m_max2;
    using pi335Ctrl::m_max3;
    using pi335Ctrl::m_min1;
    using pi335Ctrl::m_min2;
    using pi335Ctrl::m_min3;
    using pi335Ctrl::m_pos1Set;
    using pi335Ctrl::m_pos2Set;
    using pi335Ctrl::m_pos3Set;
    using pi335Ctrl::m_posTol;
    using pi335Ctrl::m_powerMgtEnabled;
    using pi335Ctrl::m_servoState;

    /// Access the private number of axes.
    /**
     * \returns a reference to m_naxes
     */
    int &naxes()
    {
        return m_naxes;
    }

    /// Access the private controller identification.
    /**
     * \returns a reference to m_ctrl
     */
    std::string &ctrl()
    {
        return m_ctrl;
    }

    /// Access the private stage identification.
    /**
     * \returns a reference to m_stage
     */
    std::string &stage()
    {
        return m_stage;
    }

    /// Access the private flag recording whether axis 3 INDI property was registered.
    /**
     * \returns a reference to m_pos_3_sent
     */
    bool &pos3Sent()
    {
        return m_pos_3_sent;
    }

    /// Access the private flag controlling whether ATZ is used for homing.
    /**
     * \returns a reference to m_actuallyATZ
     */
    bool &actuallyATZ()
    {
        return m_actuallyATZ;
    }

    /// Get the configured DM shmim name.
    /**
     * \returns the shmimMonitor stream name
     */
    std::string dmShmimName()
    {
        return dev::shmimMonitor<pi335Ctrl>::m_shmimName;
    }

    /// Read a config file and run loadConfigImpl().
    /**
     * \returns the loadConfigImpl() return value
     */
    int loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        setupConfig();
        config.readConfig( fname );
        return loadConfigImpl( config );
    }

  protected:
    /// Create a position property with current and target elements.
    void makePosProp( pcf::IndiProperty &prop, /**< [out] the property to create */
                      const std::string &name  /**< [in] the property name */
    )
    {
        prop = pcf::IndiProperty( pcf::IndiProperty::Number );
        prop.setDevice( m_configName );
        prop.setName( name );
        prop.add( pcf::IndiElement( "current" ) );
        prop.add( pcf::IndiElement( "target" ) );
    }
};
/// \endcond

} // namespace app
} // namespace MagAOX

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup pi335Ctrl_unit_test pi335Ctrl Unit Tests
 * \brief Unit tests for the pi335Ctrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `pi335Ctrl` unit tests.
/** \ingroup pi335Ctrl_unit_test
 */
namespace pi335CtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// A fake E-727 controller on the far end of a socketpair.
/** A background thread splits received bytes into newline-terminated commands, records them, and answers
 * the ones that have a scripted response.  Commands with no scripted response get no reply, so the app sees a
 * read timeout.
 */
class fakePI335
{
  public:
    /// Create the socketpair and start the responder thread.
    fakePI335()
    {
        if( socketpair( AF_UNIX, SOCK_STREAM, 0, m_fds ) != 0 )
        {
            m_fds[0] = -1;
            m_fds[1] = -1;
            return;
        }

        m_thread = std::thread( &fakePI335::run, this );
    }

    /// Stop the responder thread and close the sockets.
    ~fakePI335()
    {
        m_stop = true;
        if( m_thread.joinable() )
        {
            m_thread.join();
        }

        if( m_fds[0] >= 0 )
        {
            close( m_fds[0] );
        }

        if( m_fds[1] >= 0 )
        {
            close( m_fds[1] );
        }
    }

    /// Get the file descriptor the app should use.
    /**
     * \returns the app-side socket descriptor
     */
    int appFd() const
    {
        return m_fds[0];
    }

    /// Script a response to a command.
    void respond( const std::string &cmd, /**< [in] the command, without the trailing newline */
                  const std::string &resp /**< [in] the response, without the trailing newline */
    )
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_responses[cmd] = resp;
    }

    /// Get a copy of the commands received so far.
    /**
     * \returns the received commands, in order, without newlines
     */
    std::vector<std::string> lines()
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        return m_lines;
    }

    /// Wait until at least \p n commands have been received.
    /**
     * \returns true if the commands arrived before the timeout
     */
    bool waitForLines( size_t n,               /**< [in] the number of commands to wait for */
                       int    timeoutMs = 2000 /**< [in] [optional] the timeout in milliseconds */
    )
    {
        for( int t = 0; t < timeoutMs; t += 5 )
        {
            { // mutex scope
                std::lock_guard<std::mutex> lock( m_mutex );
                if( m_lines.size() >= n )
                {
                    return true;
                }
            }
            std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
        }

        return false;
    }

  protected:
    /// The responder thread loop.
    void run()
    {
        std::string buf;

        while( !m_stop )
        {
            pollfd pfd;
            pfd.fd      = m_fds[1];
            pfd.events  = POLLIN;
            pfd.revents = 0;

            if( poll( &pfd, 1, 20 ) <= 0 )
            {
                continue;
            }

            char    tmp[256];
            ssize_t nrd = read( m_fds[1], tmp, sizeof( tmp ) );
            if( nrd <= 0 )
            {
                continue;
            }

            buf.append( tmp, nrd );

            size_t p;
            while( ( p = buf.find( '\n' ) ) != std::string::npos )
            {
                std::string line = buf.substr( 0, p );
                buf.erase( 0, p + 1 );

                std::string reply;
                bool        haveReply = false;

                { // mutex scope
                    std::lock_guard<std::mutex> lock( m_mutex );
                    m_lines.push_back( line );

                    auto it = m_responses.find( line );
                    if( it != m_responses.end() )
                    {
                        reply     = it->second + "\n";
                        haveReply = true;
                    }
                }

                if( haveReply )
                {
                    [[maybe_unused]] ssize_t nwr = write( m_fds[1], reply.data(), reply.size() );
                }
            }
        }
    }

    int m_fds[2] = { -1, -1 }; ///< The socketpair: [0] is the app side, [1] is the fake controller side.

    std::thread m_thread; ///< The responder thread.

    std::atomic<bool> m_stop{ false }; ///< Flag telling the responder thread to exit.

    std::mutex m_mutex; ///< Protects m_lines and m_responses.

    std::vector<std::string> m_lines; ///< Commands received, in order.

    std::map<std::string, std::string> m_responses; ///< Scripted responses keyed by command.
};
/// \endcond

/// Script the responses of an E-727 with an S-335 two-axis stage.
void scriptS335( fakePI335 &fake /**< [in.out] the fake controller */ )
{
    fake.respond( "*IDN?", "(c)2016 Physik Instrumente (PI) GmbH & Co. KG, E-727.3SDA, 0116047977, 13.21.00.02" );
    fake.respond( "CST? 1", "1=S-335.2SH" );
    fake.respond( "CST? 2", "2=S-335.2SH" );
    fake.respond( "CST? 3", "3=0" );
    fake.respond( "TMN? 1", "1=-1.5" );
    fake.respond( "TMX? 1", "1=36.5" );
    fake.respond( "TMN? 2", "2=-2.5" );
    fake.respond( "TMX? 2", "2=37.5" );
}

/// Verify construction defaults.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl construction defaults", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::pi335Ctrl();
    #endif
    // clang-format on

    pi335Ctrl_test app( "pi335" );

    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.naxes() == 2 );
    REQUIRE( app.actuallyATZ() == true );
    REQUIRE( app.pos3Sent() == false );
    REQUIRE( app.m_posTol == Approx( 0.05 ) );
    REQUIRE( app.m_min1 == Approx( 0 ) );
    REQUIRE( app.m_max1 == Approx( 35 ) );
    REQUIRE( app.m_min2 == Approx( 0 ) );
    REQUIRE( app.m_max2 == Approx( 35 ) );
    REQUIRE( app.m_min3 == Approx( 0 ) );
    REQUIRE( app.m_max3 == Approx( 0 ) );
    REQUIRE( app.m_servoState == 0 );
    REQUIRE( app.m_homingState == 0 );
}

/// Verify configuration defaults.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl configuration defaults", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::setupConfig();
    pi335Ctrl::loadConfigImpl( config );
    #endif
    // clang-format on

    pi335Ctrl_test app( "pi335" );

    const std::string fname = "/tmp/pi335Ctrl_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );
    std::filesystem::remove( fname );

    REQUIRE( app.m_baudRate == B115200 );
    REQUIRE( app.m_readTimeout == 1000 );
    REQUIRE( app.m_writeTimeout == 1000 );

    REQUIRE( app.naxes() == 2 );
    REQUIRE( app.m_homePos1 == Approx( 17.5 ) );
    REQUIRE( app.m_homePos2 == Approx( 17.5 ) );
    REQUIRE( app.m_homePos3 == Approx( 0.0 ) );

    REQUIRE( app.calibRelDir() == "ttmpupil" );
    REQUIRE( app.calibPath().size() >= 9 );
    REQUIRE( app.calibPath().substr( app.calibPath().size() - 9 ) == "/ttmpupil" );
}

/// Verify configuration overrides.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl configuration overrides", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::setupConfig();
    pi335Ctrl::loadConfigImpl( config );
    #endif
    // clang-format on

    pi335Ctrl_test app( "pi335" );

    const std::string fname = "/tmp/pi335Ctrl_test_overrides.conf";
    mx::app::writeConfigFile( fname,
                              { "stage", "stage", "stage", "stage", "dm", "dm", "usb", "device", "device" },
                              { "naxes",
                                "homePos1",
                                "homePos2",
                                "homePos3",
                                "calibRelDir",
                                "shmimName",
                                "baud",
                                "readTimeout",
                                "writeTimeout" },
                              { "3", "10.5", "11.5", "5.25", "pitest", "pitestdm", "9600", "250", "300" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );
    std::filesystem::remove( fname );

    REQUIRE( app.naxes() == 3 );
    REQUIRE( app.m_homePos1 == Approx( 10.5 ) );
    REQUIRE( app.m_homePos2 == Approx( 11.5 ) );
    REQUIRE( app.m_homePos3 == Approx( 5.25 ) );

    REQUIRE( app.m_baudRate == B9600 );
    REQUIRE( app.m_readTimeout == 250 );
    REQUIRE( app.m_writeTimeout == 300 );

    REQUIRE( app.calibRelDir() == "pitest" );
    REQUIRE( app.calibPath().substr( app.calibPath().size() - 7 ) == "/pitest" );

    REQUIRE( app.dmShmimName() == "pitestdm" );
    REQUIRE( app.shmimFlat() == "pitestdm00" );
}

/// Verify position and open-loop value queries and response parsing.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl position and sva queries", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::getCom( resp, "POS?", 1 );
    pi335Ctrl::getPos( pos, 1 );
    pi335Ctrl::getSva( sva, 1 );
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();
    app.m_readTimeout = 200;

    SECTION( "getCom appends the axis for axes 1 and 2" )
    {
        fake.respond( "POS? 1", "1=1" );
        fake.respond( "POS? 2", "2=2" );
        fake.respond( "POS?", "none" );

        std::string resp;
        REQUIRE( app.getCom( resp, "POS?", 1 ) == 0 );
        REQUIRE( resp.find( "1=1" ) == 0 );

        REQUIRE( app.getCom( resp, "POS?", 2 ) == 0 );
        REQUIRE( resp.find( "2=2" ) == 0 );

        REQUIRE( app.getCom( resp, "POS?", 0 ) == 0 );
        REQUIRE( resp.find( "none" ) == 0 );

        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines.size() == 3 );
        REQUIRE( lines[0] == "POS? 1" );
        REQUIRE( lines[1] == "POS? 2" );
        REQUIRE( lines[2] == "POS?" );
    }

    SECTION( "getCom reports a timeout" )
    {
        std::string resp;
        REQUIRE( app.getCom( resp, "POS?", 1 ) == -1 );
    }

    SECTION( "getPos parses the position" )
    {
        fake.respond( "POS? 1", "1=12.25" );
        fake.respond( "POS? 2", "2=-3.5" );

        float pos = 0;
        REQUIRE( app.getPos( pos, 1 ) == 0 );
        REQUIRE( pos == Approx( 12.25 ) );

        REQUIRE( app.getPos( pos, 2 ) == 0 );
        REQUIRE( pos == Approx( -3.5 ) );
    }

    SECTION( "getPos rejects a response without '='" )
    {
        fake.respond( "POS? 1", "garbage" );

        float pos = 7;
        REQUIRE( app.getPos( pos, 1 ) == -1 );
        REQUIRE( pos == Approx( 7 ) );
    }

    SECTION( "getPos fails on timeout" )
    {
        float pos = 7;
        REQUIRE( app.getPos( pos, 1 ) == -1 );
        REQUIRE( pos == Approx( 7 ) );
    }

    SECTION( "getSva parses the open loop value" )
    {
        fake.respond( "SVA? 1", "1=45.5" );
        fake.respond( "SVA? 2", "2=0.125" );

        float sva = 0;
        REQUIRE( app.getSva( sva, 1 ) == 0 );
        REQUIRE( sva == Approx( 45.5 ) );

        REQUIRE( app.getSva( sva, 2 ) == 0 );
        REQUIRE( sva == Approx( 0.125 ) );
    }

    SECTION( "getSva rejects a bad response" )
    {
        fake.respond( "SVA? 2", "error" );

        float sva = 3;
        REQUIRE( app.getSva( sva, 2 ) == -1 );
        REQUIRE( sva == Approx( 3 ) );
    }
}

/// Verify testConnection() identification and limit parsing.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl testConnection", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::testConnection();
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();
    app.m_readTimeout = 200;
    app.m_homePos1    = 16;
    app.m_homePos2    = 18;
    app.m_homePos3    = 4;

    SECTION( "S-335 two-axis stage" )
    {
        scriptS335( fake );

        REQUIRE( app.testConnection() == 0 );

        REQUIRE( app.ctrl() == "E-727.3SDA,0116047977,13.21.00.02" );
        REQUIRE( app.stage() == "S-335.2SH" );
        REQUIRE( app.naxes() == 2 );
        REQUIRE( app.actuallyATZ() == true );
        REQUIRE( app.pos3Sent() == false );

        REQUIRE( app.m_min1 == Approx( -1.5 ) );
        REQUIRE( app.m_max1 == Approx( 36.5 ) );
        REQUIRE( app.m_min2 == Approx( -2.5 ) );
        REQUIRE( app.m_max2 == Approx( 37.5 ) );

        REQUIRE( app.m_flatLoaded == true );
        REQUIRE( app.m_flatCommand.rows() == 3 );
        REQUIRE( app.m_flatCommand.cols() == 1 );
        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 16 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 18 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 0 ) );

        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines.size() == 8 );
        REQUIRE( lines[0] == "*IDN?" );
        REQUIRE( lines[1] == "CST? 1" );
        REQUIRE( lines[3] == "CST? 3" );
        REQUIRE( lines[7] == "TMX? 2" );
    }

    SECTION( "S-325 three-axis stage" )
    {
        fake.respond( "*IDN?", "(c)2016 Physik Instrumente (PI) GmbH & Co. KG, E-727.3SDA, 0116047977, 13.21.00.02" );
        fake.respond( "CST? 1", "1=S-325.3SL" );
        fake.respond( "CST? 2", "2=S-325.3SL" );
        fake.respond( "CST? 3", "3=S-325.3SL" );
        fake.respond( "TMN? 1", "1=0" );
        fake.respond( "TMX? 1", "1=10" );
        fake.respond( "TMN? 2", "2=0" );
        fake.respond( "TMX? 2", "2=11" );
        fake.respond( "TMN? 3", "3=-1" );
        fake.respond( "TMX? 3", "3=12" );

        REQUIRE( app.testConnection() == 0 );

        REQUIRE( app.stage() == "S-325.3SL" );
        REQUIRE( app.naxes() == 3 );
        REQUIRE( app.actuallyATZ() == false );
        REQUIRE( app.pos3Sent() == true );

        REQUIRE( app.m_indiP_pos3.getName() == "pos_3" );
        REQUIRE( app.m_indiP_pos3.find( "current" ) );
        REQUIRE( app.m_indiP_pos3.find( "target" ) );

        REQUIRE( app.m_max2 == Approx( 11 ) );
        REQUIRE( app.m_min3 == Approx( -1 ) );
        REQUIRE( app.m_max3 == Approx( 12 ) );

        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 16 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 18 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 4 ) );
        REQUIRE( app.m_flatLoaded == true );
    }

    SECTION( "unknown controller is rejected" )
    {
        fake.respond( "*IDN?", "Some Other Controller" );

        REQUIRE( app.testConnection() == -1 );
        REQUIRE( app.m_flatLoaded == false );
    }

    SECTION( "unknown stage is rejected" )
    {
        scriptS335( fake );
        fake.respond( "CST? 1", "1=S-999" );

        REQUIRE( app.testConnection() == -1 );
        REQUIRE( app.m_flatLoaded == false );
    }

    SECTION( "bad limit response is rejected" )
    {
        scriptS335( fake );
        fake.respond( "TMX? 1", "nonsense" );

        REQUIRE( app.testConnection() == -1 );
        REQUIRE( app.m_min1 == Approx( -1.5 ) );
        REQUIRE( app.m_max1 == Approx( 35 ) );
        REQUIRE( app.m_flatLoaded == false );
    }

    SECTION( "no response is an error" )
    {
        REQUIRE( app.testConnection() == -1 );
    }
}

/// Verify the move commands, including range checks.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl move commands", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::move_1( 0 );
    pi335Ctrl::move_2( 0 );
    pi335Ctrl::move_3( 0 );
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();

    SECTION( "in-range moves send MOV and record the set point" )
    {
        REQUIRE( app.move_1( 12.5 ) == 0 );
        REQUIRE( app.m_pos1Set == Approx( 12.5 ) );

        REQUIRE( app.move_2( 35 ) == 0 );
        REQUIRE( app.m_pos2Set == Approx( 35 ) );

        REQUIRE( fake.waitForLines( 2 ) );
        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines[0] == "MOV 1 12.500000" );
        REQUIRE( lines[1] == "MOV 2 35.000000" );
    }

    SECTION( "out-of-range moves are rejected" )
    {
        app.m_pos1Set = 3;
        app.m_pos2Set = 4;

        REQUIRE( app.move_1( -0.1 ) == -1 );
        REQUIRE( app.move_1( 35.1 ) == -1 );
        REQUIRE( app.move_2( -1 ) == -1 );
        REQUIRE( app.move_2( 40 ) == -1 );

        REQUIRE( app.m_pos1Set == Approx( 3 ) );
        REQUIRE( app.m_pos2Set == Approx( 4 ) );

        // A sentinel command shows nothing was sent before it
        REQUIRE( app.move_1( 1 ) == 0 );
        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "MOV 1 1.000000" );
    }

    SECTION( "axis 3 requires three axes" )
    {
        app.m_max3 = 20;
        REQUIRE( app.move_3( 5 ) == -1 );

        app.naxes() = 3;
        REQUIRE( app.move_3( 21 ) == -1 );
        REQUIRE( app.move_3( 5 ) == 0 );
        REQUIRE( app.m_pos3Set == Approx( 5 ) );

        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "MOV 3 5.000000" );
    }

    SECTION( "custom limits are honored" )
    {
        app.m_min1 = 10;
        app.m_max1 = 20;

        REQUIRE( app.move_1( 5 ) == -1 );
        REQUIRE( app.move_1( 15 ) == 0 );
        REQUIRE( app.m_pos1Set == Approx( 15 ) );
    }
}

/// Verify the homing sequence steps.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl homing", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::home();
    pi335Ctrl::home_1();
    pi335Ctrl::home_2();
    pi335Ctrl::home_3();
    pi335Ctrl::homeState( 1 );
    pi335Ctrl::finishInit();
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();
    app.m_readTimeout = 200;
    app.state( stateCodes::NOTHOMED );

    SECTION( "home refuses with servos on" )
    {
        app.m_servoState = 1;
        REQUIRE( app.home() == -1 );
        REQUIRE( app.state() == stateCodes::NOTHOMED );

        REQUIRE( app.home_1() == -1 );
        REQUIRE( app.home_2() == -1 );
        REQUIRE( app.home_3() == -1 );
        REQUIRE( app.finishInit() == -1 );
    }

    SECTION( "home starts ATZ on axis 1" )
    {
        app.m_homingState = 2;

        REQUIRE( app.home() == 0 );
        REQUIRE( app.state() == stateCodes::HOMING );
        REQUIRE( app.m_homingState == 0 );
        REQUIRE( app.m_homingStart > 0 );

        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "ATZ 1 NaN" );
    }

    SECTION( "each home step requires the previous one" )
    {
        app.m_homingState = 1;
        REQUIRE( app.home_1() == -1 );
        REQUIRE( app.home_3() == -1 );
        REQUIRE( app.home_2() == 0 );

        app.m_homingState = 2;
        REQUIRE( app.home_2() == -1 );
        REQUIRE( app.home_3() == 0 );

        REQUIRE( fake.waitForLines( 2 ) );
        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines[0] == "ATZ 2 NaN" );
        REQUIRE( lines[1] == "ATZ 3 NaN" );
    }

    SECTION( "without ATZ no ATZ command is sent" )
    {
        app.actuallyATZ() = false;

        REQUIRE( app.home() == 0 );
        REQUIRE( app.state() == stateCodes::HOMING );

        // A sentinel command shows nothing was sent before it
        REQUIRE( app.move_1( 1 ) == 0 );
        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "MOV 1 1.000000" );

        // homeState reports complete without querying
        REQUIRE( app.homeState( 1 ) == 1 );
        REQUIRE( fake.lines().size() == 1 );
    }

    SECTION( "homeState parses the ATZ? response" )
    {
        fake.respond( "ATZ? 1", "1=1" );
        fake.respond( "ATZ? 2", "2=0" );

        REQUIRE( app.homeState( 1 ) == 1 );
        REQUIRE( app.homeState( 2 ) == 0 );
    }

    SECTION( "homeState reports errors" )
    {
        fake.respond( "ATZ? 1", "junk" );

        REQUIRE( app.homeState( 1 ) == -1 );

        // No response for axis 2
        REQUIRE( app.homeState( 2 ) == -1 );
    }

    SECTION( "finishInit requires homing to be complete" )
    {
        app.naxes()       = 2;
        app.m_homingState = 1;
        REQUIRE( app.finishInit() == -1 );

        app.naxes()       = 3;
        app.m_homingState = 2;
        REQUIRE( app.finishInit() == -1 );

        REQUIRE( app.m_servoState == 0 );
    }
}

/// Verify initDM() turns servos off and starts homing.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl initDM", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::initDM();
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    fake.respond( "SVA? 1", "1=0" );
    fake.respond( "SVA? 2", "2=0" );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();
    app.m_readTimeout = 200;
    app.m_servoState  = 1;
    app.state( stateCodes::NOTHOMED );

    REQUIRE( app.initDM() == 0 );

    REQUIRE( app.m_servoState == 0 );
    REQUIRE( app.state() == stateCodes::HOMING );
    REQUIRE( app.m_homingState == 0 );

    REQUIRE( fake.waitForLines( 5 ) );
    std::vector<std::string> lines = fake.lines();
    REQUIRE( lines[0] == "SVA? 1" );
    REQUIRE( lines[1] == "SVA? 2" );
    REQUIRE( lines[2] == "SVO 1 0" );
    REQUIRE( lines.back() == "ATZ 1 NaN" );
}

/// Verify releaseDM() turns servos off and zeros the open-loop outputs.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl releaseDM", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::releaseDM();
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();
    app.m_flatSet     = true;
    app.state( stateCodes::OPERATING );

    SECTION( "servos on" )
    {
        app.m_servoState = 1;

        REQUIRE( app.releaseDM() == 0 );

        REQUIRE( app.m_servoState == 0 );
        REQUIRE( app.m_flatSet == false );
        REQUIRE( app.state() == stateCodes::NOTHOMED );

        REQUIRE( fake.waitForLines( 4 ) );
        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines[0] == "SVO 1 0" );
        REQUIRE( lines[1] == "SVO 2 0" );
        REQUIRE( lines[2] == "SVA 1 0" );
        REQUIRE( lines[3] == "SVA 2 0" );
    }

    SECTION( "servos already off" )
    {
        app.m_servoState = 0;

        REQUIRE( app.releaseDM() == 0 );
        REQUIRE( app.m_flatSet == false );
        REQUIRE( app.state() == stateCodes::NOTHOMED );

        // A sentinel command shows exactly two commands were sent before it
        REQUIRE( app.move_1( 1 ) == 0 );
        REQUIRE( fake.waitForLines( 3 ) );
        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines[0] == "SVA 1 0" );
        REQUIRE( lines[1] == "SVA 2 0" );
        REQUIRE( lines[2] == "MOV 1 1.000000" );
    }
}

/// Verify zeroDM(), commandDM() and updateFlat().
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl DM commands", "[pi335Ctrl]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::zeroDM();
    pi335Ctrl::commandDM( curr_src );
    pi335Ctrl::updateFlat( 0, 0, 0 );
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();

    SECTION( "zeroDM moves the axes to 0" )
    {
        app.m_pos1Set = 5;
        app.m_pos2Set = 6;

        REQUIRE( app.zeroDM() == 0 );
        REQUIRE( app.m_pos1Set == Approx( 0 ) );
        REQUIRE( app.m_pos2Set == Approx( 0 ) );

        REQUIRE( fake.waitForLines( 2 ) );
        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines[0] == "MOV 1 0.000000" );
        REQUIRE( lines[1] == "MOV 2 0.000000" );
    }

    SECTION( "zeroDM with three axes" )
    {
        app.naxes()   = 3;
        app.m_max3    = 10;
        app.m_pos3Set = 5;

        REQUIRE( app.zeroDM() == 0 );
        REQUIRE( app.m_pos3Set == Approx( 0 ) );

        REQUIRE( fake.waitForLines( 3 ) );
        REQUIRE( fake.lines()[2] == "MOV 3 0.000000" );
    }

    SECTION( "commandDM does nothing unless OPERATING" )
    {
        app.state( stateCodes::READY );

        float cmd[3] = { 5, 6, 7 };
        REQUIRE( app.commandDM( cmd ) == 0 );
        REQUIRE( app.m_pos1Set == Approx( 0 ) );
        REQUIRE( app.m_pos2Set == Approx( 0 ) );
    }

    SECTION( "commandDM moves both axes when OPERATING" )
    {
        app.state( stateCodes::OPERATING );

        float cmd[3] = { 5, 6, 7 };
        REQUIRE( app.commandDM( cmd ) == 0 );
        REQUIRE( app.m_pos1Set == Approx( 5 ) );
        REQUIRE( app.m_pos2Set == Approx( 6 ) );
        REQUIRE( app.m_pos3Set == Approx( 0 ) );

        REQUIRE( fake.waitForLines( 2 ) );
        std::vector<std::string> lines = fake.lines();
        REQUIRE( lines[0] == "MOV 1 5.000000" );
        REQUIRE( lines[1] == "MOV 2 6.000000" );
    }

    SECTION( "commandDM moves three axes when configured" )
    {
        app.state( stateCodes::OPERATING );
        app.naxes() = 3;
        app.m_max3  = 10;

        float cmd[3] = { 5, 6, 7 };
        REQUIRE( app.commandDM( cmd ) == 0 );
        REQUIRE( app.m_pos3Set == Approx( 7 ) );

        REQUIRE( fake.waitForLines( 3 ) );
        REQUIRE( fake.lines()[2] == "MOV 3 7.000000" );
    }

    SECTION( "commandDM stops at the first out-of-range axis" )
    {
        app.state( stateCodes::OPERATING );

        float cmd[3] = { 50, 6, 0 };
        REQUIRE( app.commandDM( cmd ) == -1 );
        REQUIRE( app.m_pos1Set == Approx( 0 ) );
        REQUIRE( app.m_pos2Set == Approx( 0 ) );

        float cmd2[3] = { 5, 60, 0 };
        REQUIRE( app.commandDM( cmd2 ) == -1 );
        REQUIRE( app.m_pos1Set == Approx( 5 ) );
        REQUIRE( app.m_pos2Set == Approx( 0 ) );
    }

    SECTION( "updateFlat stores the flat command" )
    {
        app.state( stateCodes::READY );

        REQUIRE( app.updateFlat( 1.5, 2.5, 3.5 ) == 0 );
        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 1.5 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 2.5 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 3.5 ) );
    }

    SECTION( "updateFlat when OPERATING propagates via setFlat (no flat stream configured)" )
    {
        app.state( stateCodes::OPERATING );

        REQUIRE( app.updateFlat( 4, 5, 6 ) == 0 );
        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 4 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 5 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 6 ) );
    }
}

/// Verify the axis 1 and axis 2 position INDI callbacks.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl position INDI callbacks for axes 1 and 2", "[pi335Ctrl][indi]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::newCallBack_m_indiP_pos1( ip );
    pi335Ctrl::newCallBack_m_indiP_pos2( ip );
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();
    app.m_pos1Set     = 1;
    app.m_pos2Set     = 2;
    app.m_pos3Set     = 3;

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "pi335" );
    ip.setName( "pos_1" );

    SECTION( "wrong device is rejected" )
    {
        ip.setDevice( "wrong" );
        ip.add( pcf::IndiElement( "target", 10.0f ) );
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_pos1( ip ) == -1 );
        REQUIRE( app.m_pos1Set == Approx( 1 ) );
    }

    SECTION( "wrong name is rejected" )
    {
        ip.setName( "pos_2" );
        ip.add( pcf::IndiElement( "target", 10.0f ) );
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_pos1( ip ) == -1 );
        REQUIRE( app.m_pos1Set == Approx( 1 ) );

        ip.setName( "pos_1" );
        REQUIRE( app.newCallBack_m_indiP_pos2( ip ) == -1 );
        REQUIRE( app.m_pos2Set == Approx( 2 ) );
    }

    SECTION( "no value is ignored" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_pos1( ip ) == 0 );
        REQUIRE( app.m_pos1Set == Approx( 1 ) );
    }

    SECTION( "a target is rejected unless READY or OPERATING" )
    {
        app.state( stateCodes::NOTHOMED );
        ip.add( pcf::IndiElement( "target", 10.0f ) );
        REQUIRE( app.newCallBack_m_indiP_pos1( ip ) == -1 );
        REQUIRE( app.m_pos1Set == Approx( 1 ) );
    }

    SECTION( "READY: axis 1 target moves the axis and updates the flat" )
    {
        app.state( stateCodes::READY );
        ip.add( pcf::IndiElement( "target", 10.0f ) );

        REQUIRE( app.newCallBack_m_indiP_pos1( ip ) == 0 );
        REQUIRE( app.m_pos1Set == Approx( 10 ) );
        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 10 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 2 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 3 ) );

        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "MOV 1 10.000000" );
    }

    SECTION( "READY: current is used when there is no target" )
    {
        app.state( stateCodes::READY );
        ip.setName( "pos_2" );
        ip.add( pcf::IndiElement( "current", 11.0f ) );

        REQUIRE( app.newCallBack_m_indiP_pos2( ip ) == 0 );
        REQUIRE( app.m_pos2Set == Approx( 11 ) );
        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 1 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 11 ) );

        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "MOV 2 11.000000" );
    }

    SECTION( "READY: target takes precedence over current" )
    {
        app.state( stateCodes::READY );
        ip.add( pcf::IndiElement( "current", 11.0f ) );
        ip.add( pcf::IndiElement( "target", 12.0f ) );

        REQUIRE( app.newCallBack_m_indiP_pos1( ip ) == 0 );
        REQUIRE( app.m_pos1Set == Approx( 12 ) );
    }

    SECTION( "READY: out-of-range target is rejected" )
    {
        app.state( stateCodes::READY );
        ip.add( pcf::IndiElement( "target", 100.0f ) );

        REQUIRE( app.newCallBack_m_indiP_pos1( ip ) == -1 );
        REQUIRE( app.m_pos1Set == Approx( 1 ) );
    }

    SECTION( "OPERATING: target only updates the flat" )
    {
        app.state( stateCodes::OPERATING );
        ip.setName( "pos_2" );
        ip.add( pcf::IndiElement( "target", 9.0f ) );

        REQUIRE( app.newCallBack_m_indiP_pos2( ip ) == 0 );
        REQUIRE( app.m_pos2Set == Approx( 2 ) );
        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 1 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 9 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 3 ) );

        // A sentinel command shows no move was sent
        REQUIRE( app.move_1( 1 ) == 0 );
        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "MOV 1 1.000000" );
    }
}

/// Verify the axis 3 position INDI callback.
/**
 * \ingroup pi335Ctrl_unit_test
 */
TEST_CASE( "pi335Ctrl position INDI callback for axis 3", "[pi335Ctrl][indi]" )
{
    // clang-format off
    #ifdef PI335CTRL_TEST_DOXYGEN_REF
    pi335Ctrl::newCallBack_m_indiP_pos3( ip );
    #endif
    // clang-format on

    fakePI335 fake;
    REQUIRE( fake.appFd() >= 0 );

    pi335Ctrl_test app( "pi335" );
    app.m_fileDescrip = fake.appFd();
    app.naxes()       = 3;
    app.m_max3        = 10;
    app.m_pos1Set     = 1;
    app.m_pos2Set     = 2;
    app.m_pos3Set     = 3;

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "pi335" );
    ip.setName( "pos_3" );

    SECTION( "wrong name is rejected" )
    {
        ip.setName( "pos_1" );
        ip.add( pcf::IndiElement( "target", 5.0f ) );
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_pos3( ip ) == -1 );
        REQUIRE( app.m_pos3Set == Approx( 3 ) );
    }

    SECTION( "no value is ignored" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_pos3( ip ) == 0 );
        REQUIRE( app.m_pos3Set == Approx( 3 ) );
    }

    SECTION( "READY: target moves axis 3 and updates the flat" )
    {
        app.state( stateCodes::READY );
        ip.add( pcf::IndiElement( "target", 5.0f ) );

        REQUIRE( app.newCallBack_m_indiP_pos3( ip ) == 0 );
        REQUIRE( app.m_pos3Set == Approx( 5 ) );
        REQUIRE( app.m_flatCommand( 0, 0 ) == Approx( 1 ) );
        REQUIRE( app.m_flatCommand( 1, 0 ) == Approx( 2 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 5 ) );

        REQUIRE( fake.waitForLines( 1 ) );
        REQUIRE( fake.lines()[0] == "MOV 3 5.000000" );
    }

    SECTION( "OPERATING: target only updates the flat" )
    {
        app.state( stateCodes::OPERATING );
        ip.add( pcf::IndiElement( "target", 6.0f ) );

        REQUIRE( app.newCallBack_m_indiP_pos3( ip ) == 0 );
        REQUIRE( app.m_pos3Set == Approx( 3 ) );
        REQUIRE( app.m_flatCommand( 2, 0 ) == Approx( 6 ) );
    }

    SECTION( "a target is rejected unless READY or OPERATING" )
    {
        app.state( stateCodes::HOMING );
        ip.add( pcf::IndiElement( "target", 5.0f ) );
        REQUIRE( app.newCallBack_m_indiP_pos3( ip ) == -1 );
        REQUIRE( app.m_pos3Set == Approx( 3 ) );
    }
}

} // namespace pi335CtrlTest

} // namespace libXWCTest
