/** \file elliptecCtrl_test.cpp
 * \brief Catch2 tests for the elliptecCtrl app.
 * \author Claude Code
 */

#include "../../../tests/testXWC.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define protected public
#include "../elliptecCtrl.hpp"
#undef protected

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup elliptecCtrl_unit_test elliptecCtrl Unit Tests
 * \brief Unit tests for the elliptecCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `elliptecCtrl` unit tests.
/** \ingroup elliptecCtrl_unit_test
 */
namespace elliptecCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness for elliptecCtrl, with the INDI properties given their device and names.
class elliptecCtrl_test : public elliptecCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit elliptecCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        m_ipAbsDeg.setDevice( m_configName );
        m_ipAbsDeg.setName( "absDeg" );
        m_ipRelDeg.setDevice( m_configName );
        m_ipRelDeg.setName( "relDeg" );
        m_ipRelMove.setDevice( m_configName );
        m_ipRelMove.setName( "relMove" );
        m_ipVelPct.setDevice( m_configName );
        m_ipVelPct.setName( "velocity" );
        m_ipOptimize.setDevice( m_configName );
        m_ipOptimize.setName( "optimize" );
        m_ipSave.setDevice( m_configName );
        m_ipSave.setName( "save" );
        m_ipHome.setDevice( m_configName );
        m_ipHome.setName( "home" );
        m_ipStop.setDevice( m_configName );
        m_ipStop.setName( "stop" );
        m_ipStageGoto.setDevice( m_configName );
        m_ipStageGoto.setName( "stageGoto" );
    }

    /// Use short read timeouts so that unanswered commands fail quickly.
    void fastTimeouts()
    {
        m_readTimeoutMs     = 50;
        m_busyReadTimeoutMs = 50;
    }

    /// Set a 262144 pulse/rev conversion (ELL14-like), so 90 deg is 0x00010000 pulses.
    void standardPulses()
    {
        m_pulsesPerRev = 262144;
    }
};

/// A fake Elliptec device on a socket pair or a pseudo-terminal.
/** Each chunk the app writes is recorded as one command (the app always waits for a reply before sending the next
 * one).  Commands found in the reply table, or any command when a default reply is set, are answered with the reply
 * plus CRLF.
 */
class fakeElliptec
{
  public:
    /// The kind of fake port.
    enum class mode
    {
        socket, ///< A socket pair, the app side given directly as `m_fd`.
        pty     ///< A pseudo-terminal, which the app opens by path in `openPort_()`.
    };

    /// Create the fake port.
    explicit fakeElliptec( mode m = mode::socket /**< [in] the kind of port to create */ )
    {
        if( m == mode::socket )
        {
            int fds[2];
            if( ::socketpair( AF_UNIX, SOCK_STREAM, 0, fds ) == 0 )
            {
                m_appFd = fds[0];
                m_devFd = fds[1];

                // The app expects a non-blocking descriptor (openPort_ uses O_NONBLOCK).
                int fl = ::fcntl( m_appFd, F_GETFL, 0 );
                ::fcntl( m_appFd, F_SETFL, fl | O_NONBLOCK );
            }
        }
        else
        {
            m_devFd = ::posix_openpt( O_RDWR | O_NOCTTY );
            if( m_devFd >= 0 )
            {
                if( ::grantpt( m_devFd ) == 0 && ::unlockpt( m_devFd ) == 0 )
                {
                    const char *name = ::ptsname( m_devFd );
                    if( name != nullptr )
                    {
                        m_slavePath = name;
                    }
                }

                if( m_slavePath.empty() )
                {
                    ::close( m_devFd );
                    m_devFd = -1;
                }
            }
        }
    }

    /// Stop the responder and close the descriptors.
    ~fakeElliptec()
    {
        stop();

        if( m_appFd >= 0 )
        {
            ::close( m_appFd );
        }

        if( m_devFd >= 0 )
        {
            ::close( m_devFd );
        }
    }

    /// Check that the port was created.
    bool ok() const
    {
        return m_devFd >= 0;
    }

    /// Get the app-side descriptor (socket mode only).
    int appFd() const
    {
        return m_appFd;
    }

    /// Get the pseudo-terminal slave path (pty mode only).
    const std::string &slavePath() const
    {
        return m_slavePath;
    }

    /// Add a canned reply, sent with a trailing CRLF.
    void reply( const std::string &command, /**< [in] the command, e.g. "0gp" */
                const std::string &response /**< [in] the reply, without CRLF */ )
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_replies[command] = response;
    }

    /// Set a reply sent to any command not in the reply table.
    void defaultReply( const std::string &response /**< [in] the reply, without CRLF */ )
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_default    = response;
        m_hasDefault = true;
    }

    /// Write raw bytes to the app side.
    void push( const std::string &bytes /**< [in] the bytes to send */ )
    {
        ssize_t rv = ::write( m_devFd, bytes.data(), bytes.size() );
        static_cast<void>( rv );
    }

    /// Start the responder thread.
    void start()
    {
        m_thread = std::thread( &fakeElliptec::serve, this );
    }

    /// Stop the responder thread.
    void stop()
    {
        if( m_thread.joinable() )
        {
            m_stop = true;
            m_thread.join();
        }
    }

    /// Get a copy of the commands received so far.
    std::vector<std::string> commands()
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        return m_commands;
    }

    /// Forget the commands received so far.
    void clearCommands()
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_commands.clear();
    }

  private:
    /// Responder thread loop.
    void serve()
    {
        while( !m_stop )
        {
            pollfd pfd;
            pfd.fd      = m_devFd;
            pfd.events  = POLLIN;
            pfd.revents = 0;

            int rv = ::poll( &pfd, 1, 20 );

            if( rv <= 0 )
            {
                continue;
            }

            if( !( pfd.revents & POLLIN ) )
            {
                // e.g. POLLHUP on a pty master before the app opens the slave
                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                continue;
            }

            char    buf[256];
            ssize_t nrd = ::read( m_devFd, buf, sizeof( buf ) );

            if( nrd <= 0 )
            {
                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                continue;
            }

            std::string cmd( buf, nrd );
            std::string resp;
            bool        send = false;

            { // mutex scope
                std::lock_guard<std::mutex> lock( m_mutex );
                m_commands.push_back( cmd );

                auto it = m_replies.find( cmd );
                if( it != m_replies.end() )
                {
                    resp = it->second;
                    send = true;
                }
                else if( m_hasDefault )
                {
                    resp = m_default;
                    send = true;
                }
            }

            if( send )
            {
                resp += "\r\n";
                ssize_t nwr = ::write( m_devFd, resp.data(), resp.size() );
                static_cast<void>( nwr );
            }
        }
    }

    int m_appFd{ -1 }; ///< The app side of a socket pair, or -1.

    int m_devFd{ -1 }; ///< The device side: socket pair peer or pty master.

    std::string m_slavePath; ///< The pty slave path, in pty mode.

    std::mutex m_mutex; ///< Protects the reply table and the command log.

    std::map<std::string, std::string> m_replies; ///< Canned replies keyed by command.

    std::string m_default; ///< The default reply.

    bool m_hasDefault{ false }; ///< Whether the default reply is used.

    std::vector<std::string> m_commands; ///< Commands received, in order.

    std::atomic<bool> m_stop{ false }; ///< Flag telling the responder thread to exit.

    std::thread m_thread; ///< The responder thread.
};

/// Write a config file, read it into the app configurator, and remove the file.
/**
 * \returns the return value of readConfig()
 */
int readTestConfig( elliptecCtrl_test              &app,      /**< [in/out] the app, with setupConfig() called */
                    const std::string              &tag,      /**< [in] a unique tag for the file name */
                    const std::vector<std::string> &sections, /**< [in] the config sections */
                    const std::vector<std::string> &keywords, /**< [in] the config keywords */
                    const std::vector<std::string> &values /**< [in] the config values */ )
{
    std::string fname = "/tmp/elliptecCtrl_test_" + tag + "_" + std::to_string( ::getpid() ) + ".conf";
    mx::app::writeConfigFile( fname, sections, keywords, values );
    int rv = app.config.readConfig( fname );
    ::unlink( fname.c_str() );
    return rv;
}

/// Build a number property with a target element for the harness device.
pcf::IndiProperty numberProp( const std::string &name, /**< [in] the property name */
                              double             target /**< [in] the target value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "elliptec" );
    ip.setName( name );
    ip.add( pcf::IndiElement( "target" ) );
    ip["target"].set( target );
    return ip;
}

/// Build a request switch property for the harness device.
pcf::IndiProperty requestProp( const std::string &name, /**< [in] the property name */
                               bool               on /**< [in] the request switch state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "elliptec" );
    ip.setName( name );
    ip.add( pcf::IndiElement( "request" ) );
    ip["request"].setSwitchState( on ? pcf::IndiElement::On : pcf::IndiElement::Off );
    return ip;
}

/// \endcond

/// Verify the configuration defaults.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl configuration defaults", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::setupConfig();
    elliptecCtrl::loadConfig();
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );
    app.setupConfig();
    REQUIRE( readTestConfig( app, "defaults", { "none" }, { "nada" }, { "0" } ) == 0 );
    app.loadConfig();

    REQUIRE( app.m_port == "" );
    REQUIRE( app.m_baud == 9600 );
    REQUIRE( app.m_addr == '0' );
    REQUIRE( app.m_readTimeoutMs == 3000 );
    REQUIRE( app.m_busyReadTimeoutMs == 6000 );
    REQUIRE( app.m_postWriteSleepMs == 0 );
    REQUIRE( app.m_homeOffsetDeg == 0.0 );
    REQUIRE( app.m_allowMultiturn == false );
    REQUIRE( app.m_pulsesPerRev == 0 );
    REQUIRE( app.m_velPercent == 40 );
    REQUIRE( app.m_cmdOptimize == "om" );
    REQUIRE( app.m_cmdSave == "us" );
    REQUIRE( app.m_commMaxMisses == 3 );
    REQUIRE( app.m_userPresetNames.empty() );
    REQUIRE( app.m_userPresetDeg.empty() );
    REQUIRE( app.m_presetNotation == "preset" );
    REQUIRE( app.m_powerMgtEnabled == true );
}

/// Verify configuration overrides and the clamping applied by loadConfig().
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl configuration overrides", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "all values set" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.setupConfig();
        REQUIRE( readTestConfig( app,
                                 "overrides",
                                 { "stage",
                                   "stage",
                                   "stage",
                                   "serial",
                                   "serial",
                                   "serial",
                                   "stage",
                                   "stage",
                                   "stage",
                                   "motion",
                                   "device",
                                   "device",
                                   "comm",
                                   "presets",
                                   "presets" },
                                 { "port",
                                   "baud",
                                   "address",
                                   "readTimeoutMs",
                                   "busyReadTimeoutMs",
                                   "postWriteSleepMs",
                                   "homeOffset",
                                   "allowMultiturn",
                                   "pulsesPerRev",
                                   "velPercent",
                                   "optimizeCmd",
                                   "saveCmd",
                                   "maxPollMisses",
                                   "names",
                                   "positions" },
                                 { "/dev/ttyELL",
                                   "115200",
                                   "b",
                                   "500",
                                   "2500",
                                   "5",
                                   "12.5",
                                   "true",
                                   "143360",
                                   "75",
                                   "o1",
                                   "s1",
                                   "7",
                                   "open,closed",
                                   "10,45.5" } ) == 0 );
        app.loadConfig();

        REQUIRE( app.m_port == "/dev/ttyELL" );
        REQUIRE( app.m_baud == 115200 );
        REQUIRE( app.m_addr == 'B' );
        REQUIRE( app.m_readTimeoutMs == 500 );
        REQUIRE( app.m_busyReadTimeoutMs == 2500 );
        REQUIRE( app.m_postWriteSleepMs == 5 );
        REQUIRE( app.m_homeOffsetDeg == Approx( 12.5 ) );
        REQUIRE( app.m_allowMultiturn == true );
        REQUIRE( app.m_pulsesPerRev == 143360 );
        REQUIRE( app.m_velPercent == 75 );
        REQUIRE( app.m_cmdOptimize == "o1" );
        REQUIRE( app.m_cmdSave == "s1" );
        REQUIRE( app.m_commMaxMisses == 7 );

        REQUIRE( app.m_userPresetNames == std::vector<std::string>{ "open", "closed" } );
        REQUIRE( app.m_userPresetDeg.size() == 2 );
        REQUIRE( app.m_userPresetDeg[0] == Approx( 10.0 ) );
        REQUIRE( app.m_userPresetDeg[1] == Approx( 45.5 ) );
    }

    SECTION( "out of range values are clamped or ignored" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.setupConfig();
        REQUIRE( readTestConfig( app,
                                 "clamps",
                                 { "stage", "serial", "serial", "motion", "comm" },
                                 { "address", "readTimeoutMs", "busyReadTimeoutMs", "velPercent", "maxPollMisses" },
                                 { "z", "4000", "1000", "150", "0" } ) == 0 );
        app.loadConfig();

        REQUIRE( app.m_addr == '0' );               // not a hex digit, so unchanged
        REQUIRE( app.m_busyReadTimeoutMs == 4000 ); // raised to the normal read timeout
        REQUIRE( app.m_velPercent == 100 );         // clamped to 100
        REQUIRE( app.m_commMaxMisses == 1 );        // at least one miss is tolerated
    }

    SECTION( "negative velocity is clamped to 0" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "negvel", { "motion" }, { "velPercent" }, { "-20" } ) == 0 );
        app.loadConfig();

        REQUIRE( app.m_velPercent == 0 );
    }

    SECTION( "preset names without positions default to their 1-based index" )
    {
        // stdMotionStage::loadConfig() fills in default positions, and replaces a 0 position with the index.  For
        // this app the positions are degrees, so a preset configured at 0 deg ends up at 1 deg.
        elliptecCtrl_test app( "elliptec" );
        app.setupConfig();
        REQUIRE( readTestConfig(
                     app, "presetdef", { "presets", "presets" }, { "names", "positions" }, { "a,b,c", "0,90" } ) == 0 );
        app.loadConfig();

        REQUIRE( app.m_userPresetDeg.size() == 3 );
        REQUIRE( app.m_userPresetDeg[0] == Approx( 1.0 ) );
        REQUIRE( app.m_userPresetDeg[1] == Approx( 90.0 ) );
        REQUIRE( app.m_userPresetDeg[2] == Approx( 3.0 ) );
    }
}

/// Verify the degree/pulse conversions.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl degree and pulse conversions", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::degToPulses_( 0.0 );
    elliptecCtrl::pulsesToDeg_( 0 );
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );

    SECTION( "no conversion without pulses per revolution" )
    {
        REQUIRE( app.degToPulses_( 90.0 ) == 0 );
        REQUIRE( app.pulsesToDeg_( 65536 ) == 0.0 );
    }

    SECTION( "conversion with 262144 pulses per revolution" )
    {
        app.standardPulses();

        REQUIRE( app.degToPulses_( 0.0 ) == 0 );
        REQUIRE( app.degToPulses_( 90.0 ) == 65536 );
        REQUIRE( app.degToPulses_( 360.0 ) == 262144 );
        REQUIRE( app.degToPulses_( -90.0 ) == -65536 );
        REQUIRE( app.degToPulses_( 80.0 ) == 58254 ); // 58254.2 rounds down
        REQUIRE( app.degToPulses_( 20.0 ) == 14564 ); // 14563.6 rounds up

        REQUIRE( app.pulsesToDeg_( 65536 ) == Approx( 90.0 ) );
        REQUIRE( app.pulsesToDeg_( 131072 ) == Approx( 180.0 ) );
        REQUIRE( app.pulsesToDeg_( -65536 ) == Approx( -90.0 ) );
    }
}

/// Verify the command framing and the baud rate mapping.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl framing and baud mapping", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::frame_( std::string() );
    elliptecCtrl::to_termios_baud_( 0 );
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );

    REQUIRE( app.frame_( "gs" ) == "0gs" );
    app.m_addr = 'A';
    REQUIRE( app.frame_( "ma00010000" ) == "Ama00010000" );

    REQUIRE( elliptecCtrl::to_termios_baud_( 9600 ) == B9600 );
    REQUIRE( elliptecCtrl::to_termios_baud_( 19200 ) == B19200 );
    REQUIRE( elliptecCtrl::to_termios_baud_( 38400 ) == B38400 );
    REQUIRE( elliptecCtrl::to_termios_baud_( 57600 ) == B57600 );
    REQUIRE( elliptecCtrl::to_termios_baud_( 115200 ) == B115200 );
    REQUIRE( elliptecCtrl::to_termios_baud_( 1234 ) == B9600 );
}

/// Verify the low level serial helpers on a fake port.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl serial helpers", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::writeAll_( std::string() );
    elliptecCtrl::readFrame_( std::string(), 0 );
    elliptecCtrl::drainInput_();
    elliptecCtrl::txrx_( std::string(), nullptr, 0 );
    elliptecCtrl::openPort_();
    elliptecCtrl::closePort_();
    #endif
    // clang-format on

    SECTION( "no port is a hard error" )
    {
        elliptecCtrl_test app( "elliptec" );
        std::string       out;
        REQUIRE( app.writeAll_( "0gs" ) == -1 );
        REQUIRE( app.readFrame_( out, 10 ) == -1 );
        REQUIRE( app.drainInput_() == -1 );
        REQUIRE( app.txrx_( "gs", &out, 10 ) == -1 );
    }

    SECTION( "readFrame_ returns a CRLF terminated frame" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        REQUIRE( port.ok() );
        app.m_fd = port.appFd();

        port.push( "0GS00\r\n" );
        std::string out;
        REQUIRE( app.readFrame_( out, 500 ) == 0 );
        REQUIRE( out == "0GS00\r\n" );
    }

    SECTION( "readFrame_ soft timeout keeps the partial frame" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        app.m_fd = port.appFd();

        port.push( "0GS" );
        std::string out;
        REQUIRE( app.readFrame_( out, 60 ) == 1 );
        REQUIRE( out == "0GS" );
    }

    SECTION( "drainInput_ discards pending input" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        app.m_fd = port.appFd();

        port.push( "stale bytes\r\n" );
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
        REQUIRE( app.drainInput_() == 0 );

        std::string out;
        REQUIRE( app.readFrame_( out, 60 ) == 1 );
        REQUIRE( out.empty() );
    }

    SECTION( "txrx_ frames the command and reads the reply" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        port.reply( "0gs", "0GS00" );
        port.start();
        app.m_fd = port.appFd();

        std::string out;
        REQUIRE( app.txrx_( "gs", &out, 500 ) == 0 );
        REQUIRE( out == "0GS00\r\n" );

        // with no reply pointer only the write is done
        REQUIRE( app.txrx_( "st", nullptr, 500 ) == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0gs", "0st" } );
    }

    SECTION( "txrx_ uses the busy timeout while a command is pending" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        app.m_fd                = port.appFd();
        app.m_busyReadTimeoutMs = 300;
        app.m_pending           = elliptecCtrl::Pending::MoveAbs;

        std::string out;
        auto        t0 = std::chrono::steady_clock::now();
        REQUIRE( app.txrx_( "gs", &out, 10 ) == 1 );
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - t0 );

        REQUIRE( dt.count() >= 250 );
    }

    SECTION( "openPort_ fails for a missing device and closePort_ is safe" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.m_port = "/nonexistent/elliptecCtrl_test_tty";
        REQUIRE( app.openPort_() == -1 );
        REQUIRE( app.m_fd == -1 );

        app.closePort_();
        REQUIRE( app.m_fd == -1 );
    }
}

/// Verify the position, status, and info query parsers.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl query parsing", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::q_position_();
    elliptecCtrl::q_status_();
    elliptecCtrl::q_info_();
    #endif
    // clang-format on

    SECTION( "q_position_ decodes signed hex pulses" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.standardPulses();
        fakeElliptec port;
        port.reply( "0gp", "0PO00010000" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_position_() == 0 );
        REQUIRE( app.m_posPulses == 65536 );
        REQUIRE( app.m_posDeg == Approx( 90.0 ) );
    }

    SECTION( "q_position_ decodes negative positions and lower case" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.standardPulses();
        fakeElliptec port;
        port.reply( "0gp", "0poffff0000" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_position_() == 0 );
        REQUIRE( app.m_posPulses == -65536 );
        REQUIRE( app.m_posDeg == Approx( -90.0 ) );
    }

    SECTION( "q_position_ keeps the previous position on bad replies" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.standardPulses();
        app.m_posPulses = 1234;
        fakeElliptec port;
        port.reply( "0gp", "0PO0001XYZ0" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_position_() == 0 );
        REQUIRE( app.m_posPulses == 1234 );

        port.reply( "0gp", "0PO12" );
        REQUIRE( app.q_position_() == 0 );
        REQUIRE( app.m_posPulses == 1234 );

        port.reply( "0gp", "0GS0000000000" );
        REQUIRE( app.q_position_() == 0 );
        REQUIRE( app.m_posPulses == 1234 );
    }

    SECTION( "q_position_ reports a soft timeout" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.fastTimeouts();
        fakeElliptec port;
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_position_() == 1 );
    }

    SECTION( "q_status_ decodes the status byte" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        port.reply( "0gs", "0GS09" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_status_() == 0 );
        REQUIRE( app.m_gs == 0x09 );

        port.reply( "0gs", "0gs0c" );
        REQUIRE( app.q_status_() == 0 );
        REQUIRE( app.m_gs == 0x0C );

        port.reply( "0gs", "0GS00" );
        REQUIRE( app.q_status_() == 0 );
        REQUIRE( app.m_gs == 0x00 );

        // malformed replies leave the status unchanged
        port.reply( "0gs", "0XX09" );
        REQUIRE( app.q_status_() == 0 );
        REQUIRE( app.m_gs == 0x00 );
    }

    SECTION( "q_status_ reports a soft timeout and a hard error" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.fastTimeouts();
        REQUIRE( app.q_status_() == -1 );

        fakeElliptec port;
        port.start();
        app.m_fd = port.appFd();
        REQUIRE( app.q_status_() == 1 );
    }

    SECTION( "q_info_ reads the pulses per revolution from the last 8 hex digits" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        port.reply( "0in", "0IN061234567820230101016800040000" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_info_() == 0 );
        REQUIRE( app.m_pulsesPerRev == 262144 );
    }

    SECTION( "q_info_ keeps a configured pulses per revolution" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.m_pulsesPerRev = 143360;
        fakeElliptec port;
        port.reply( "0in", "0IN061234567820230101016800040000" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_info_() == 0 );
        REQUIRE( app.m_pulsesPerRev == 143360 );
    }

    SECTION( "q_info_ ignores a non-hex tail" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        port.reply( "0in", "0IN0612345678202301010168GGGGGGGG" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.q_info_() == 0 );
        REQUIRE( app.m_pulsesPerRev == 0 );
    }
}

/// Verify the command strings and the pending state set by each command.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl command formatting", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::cmd_moveAbs_pulses_( 0 );
    elliptecCtrl::cmd_moveRel_pulses_( 0 );
    elliptecCtrl::cmd_setvel_( 0 );
    elliptecCtrl::cmd_home_( 0 );
    elliptecCtrl::cmd_stop_();
    elliptecCtrl::cmd_save_();
    elliptecCtrl::cmd_optimize_wait_();
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );
    fakeElliptec      port;
    port.defaultReply( "0GS00" );
    port.start();
    app.m_fd = port.appFd();

    SECTION( "absolute and relative moves encode 32-bit two's complement hex" )
    {
        REQUIRE( app.cmd_moveAbs_pulses_( 0x1234 ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::MoveAbs );
        REQUIRE( app.m_moving == 1 );

        REQUIRE( app.cmd_moveAbs_pulses_( -1 ) == 0 );

        REQUIRE( app.cmd_moveRel_pulses_( -65536 ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::MoveRel );

        REQUIRE( app.cmd_moveRel_pulses_( 0x7FFFFFFF ) == 0 );

        port.stop();
        REQUIRE( port.commands() ==
                 std::vector<std::string>{ "0ma00001234", "0maFFFFFFFF", "0mrFFFF0000", "0mr7FFFFFFF" } );
    }

    SECTION( "velocity is sent as two hex digits and clamped" )
    {
        REQUIRE( app.cmd_setvel_( 40 ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Velocity );
        REQUIRE( app.cmd_setvel_( 100 ) == 0 );
        REQUIRE( app.cmd_setvel_( 150 ) == 0 );
        REQUIRE( app.cmd_setvel_( -3 ) == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0sv28", "0sv64", "0sv64", "0sv00" } );
    }

    SECTION( "home, stop, save and optimize" )
    {
        REQUIRE( app.cmd_home_( 0 ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Home );
        REQUIRE( app.m_moving == 2 );

        REQUIRE( app.cmd_home_( 1 ) == 0 );

        REQUIRE( app.cmd_stop_() == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Stop );

        REQUIRE( app.cmd_save_() == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Save );

        REQUIRE( app.cmd_optimize_wait_() == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Optimize );
        REQUIRE( app.m_moving == 1 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ho0", "0ho1", "0st", "0us", "0om" } );
    }

    SECTION( "the command aliases and address are used" )
    {
        app.m_addr        = 'C';
        app.m_cmdSave     = "US";
        app.m_cmdOptimize = "OM";

        REQUIRE( app.cmd_save_() == 0 );
        REQUIRE( app.cmd_optimize_wait_() == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "CUS", "COM" } );
    }
}

/// Verify the degree-facing move helpers, including the absolute angle wrapping.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl degree moves", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::moveAbsDeg_( 0.0 );
    elliptecCtrl::moveRelDeg_( 0.0 );
    elliptecCtrl::moveRelDegCmdFromRelMove_();
    elliptecCtrl::moveTo( 0.0 );
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );
    app.standardPulses();
    fakeElliptec port;
    port.defaultReply( "0PO00000000" );
    port.start();
    app.m_fd = port.appFd();

    SECTION( "without multiturn absolute angles wrap to [0, 720)" )
    {
        REQUIRE( app.moveAbsDeg_( 90.0 ) == 0 );
        REQUIRE( app.moveAbsDeg_( -90.0 ) == 0 ); // 630 deg
        REQUIRE( app.moveAbsDeg_( 800.0 ) == 0 ); // 80 deg

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ma00010000", "0ma00070000", "0ma0000E38E" } );
    }

    SECTION( "with multiturn absolute angles are not wrapped" )
    {
        app.m_allowMultiturn = true;
        REQUIRE( app.moveAbsDeg_( -90.0 ) == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0maFFFF0000" } );
    }

    SECTION( "relative moves use the step size" )
    {
        REQUIRE( app.moveRelDeg_( -45.0 ) == 0 );

        app.m_relStepDeg = 90.0;
        REQUIRE( app.moveRelDegCmdFromRelMove_() == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0mrFFFF8000", "0mr00010000" } );
    }

    SECTION( "moveTo(double) is an absolute move" )
    {
        REQUIRE( app.moveTo( 90.0 ) == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ma00010000" } );
    }
}

/// Verify the stdMotionStage preset interface.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl presets", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::presetNumber();
    elliptecCtrl::moveTo( 0.0f );
    elliptecCtrl::buildStageNamePosText_();
    #endif
    // clang-format on

    SECTION( "no presets" )
    {
        elliptecCtrl_test app( "elliptec" );
        REQUIRE( app.presetNumber() == -1.0f );
        REQUIRE( app.moveTo( 1.0f ) == -1 );
        REQUIRE( app.buildStageNamePosText_() == "(none)" );
    }

    SECTION( "presetNumber finds the nearest preset" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.m_userPresetNames = { "in", "out" };
        app.m_userPresetDeg   = { 10.0, 20.0 };

        app.m_posDeg = 11.0;
        REQUIRE( app.presetNumber() == 1.0f );

        app.m_posDeg = 18.0;
        REQUIRE( app.presetNumber() == 2.0f );

        app.m_posDeg = 300.0;
        REQUIRE( app.presetNumber() == 2.0f );
    }

    SECTION( "moveTo(float) moves to the preset angle, clamping the index" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.standardPulses();
        app.m_userPresetNames = { "in", "out" };
        app.m_userPresetDeg   = { 10.0, 20.0 };

        fakeElliptec port;
        port.defaultReply( "0PO00000000" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.moveTo( 2.0f ) == 0 );
        REQUIRE( app.m_moving == 1 );
        REQUIRE( app.moveTo( 5.0f ) == 0 );
        REQUIRE( app.moveTo( 0.0f ) == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ma000038E4", "0ma000038E4", "0ma00001C72" } );
    }

    SECTION( "buildStageNamePosText_ formats the name:deg list" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.m_userPresetNames = { "a", "b", "c", "d" };
        app.m_userPresetDeg   = { 0.0, 12.5, 45.123456789, -7.0 };

        REQUIRE( app.buildStageNamePosText_() == "a:0, b:12.5, c:45.123457, d:-7" );

        // mismatched lengths use the shorter list
        app.m_userPresetDeg = { 1.0, 2.0 };
        REQUIRE( app.buildStageNamePosText_() == "a:1, b:2" );
    }
}

/// Verify stop() and startHoming().
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl stop and homing", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::stop();
    elliptecCtrl::startHoming();
    #endif
    // clang-format on

    SECTION( "stop clears the pending command" )
    {
        elliptecCtrl_test app( "elliptec" );
        fakeElliptec      port;
        port.reply( "0st", "0GS00" );
        port.start();
        app.m_fd         = port.appFd();
        app.m_pending    = elliptecCtrl::Pending::MoveAbs;
        app.m_moving     = 1;
        app.m_statusHint = "Moving";

        REQUIRE( app.stop() == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::None );
        REQUIRE( app.m_moving == 0 );
        REQUIRE( app.m_statusHint.empty() );
    }

    SECTION( "stop without a port fails" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.m_moving = 1;

        REQUIRE( app.stop() == -1 );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "startHoming sends ho0 and reads the position" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.standardPulses();
        fakeElliptec port;
        port.reply( "0ho0", "0GS09" );
        port.reply( "0gp", "0PO00000000" );
        port.start();
        app.m_fd = port.appFd();

        REQUIRE( app.startHoming() == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Home );
        REQUIRE( app.m_moving == 2 );
        REQUIRE( app.m_statusHint == "Homing" );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ho0", "0gp" } );
    }

    SECTION( "startHoming without a port fails" )
    {
        elliptecCtrl_test app( "elliptec" );
        REQUIRE( app.startHoming() == -1 );
    }
}

/// Verify how pollDevice_() resolves the pending command and motion state.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl pollDevice_ state resolution", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::pollDevice_();
    elliptecCtrl::updateStatus_();
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );
    app.standardPulses();
    fakeElliptec port;
    port.reply( "0gp", "0PO00010000" );

    SECTION( "busy while homing" )
    {
        port.reply( "0gs", "0GS09" );
        port.start();
        app.m_fd      = port.appFd();
        app.m_pending = elliptecCtrl::Pending::Home;

        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_gs == 0x09 );
        REQUIRE( app.m_moving == 2 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Home );
        REQUIRE( app.m_posDeg == Approx( 90.0 ) );
    }

    SECTION( "busy while moving" )
    {
        port.reply( "0gs", "0GS09" );
        port.start();
        app.m_fd      = port.appFd();
        app.m_pending = elliptecCtrl::Pending::MoveAbs;

        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "busy with nothing pending is external motion" )
    {
        port.reply( "0gs", "0GS09" );
        port.start();
        app.m_fd     = port.appFd();
        app.m_moving = 0;

        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "idle after homing marks the stage homed" )
    {
        port.reply( "0gs", "0GS00" );
        port.start();
        app.m_fd         = port.appFd();
        app.m_pending    = elliptecCtrl::Pending::Home;
        app.m_statusHint = "Homing";

        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_homed == true );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::None );
        REQUIRE( app.m_moving == 0 );
        REQUIRE( app.m_statusHint.empty() );
    }

    SECTION( "idle after homing applies the home offset" )
    {
        port.reply( "0gs", "0GS00" );
        port.reply( "0mr00002000", "0PO00012000" );
        port.start();
        app.m_fd            = port.appFd();
        app.m_pending       = elliptecCtrl::Pending::Home;
        app.m_homeOffsetDeg = 11.25; // 8192 pulses

        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_homed == true );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::OffsetRel );
        REQUIRE( app.m_moving == 1 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0gp", "0gs", "0mr00002000" } );
    }

    SECTION( "idle completes a pending move" )
    {
        port.reply( "0gs", "0GS00" );
        port.start();
        app.m_fd         = port.appFd();
        app.m_pending    = elliptecCtrl::Pending::MoveRel;
        app.m_moving     = 1;
        app.m_statusHint = "Moving";

        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::None );
        REQUIRE( app.m_moving == 0 );
        REQUIRE( app.m_statusHint.empty() );
    }

    SECTION( "idle with nothing pending reflects the homed state" )
    {
        port.reply( "0gs", "0GS00" );
        port.start();
        app.m_fd = port.appFd();

        app.m_homed = false;
        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_moving == -1 );

        app.m_homed = true;
        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_moving == 0 );
    }

    SECTION( "soft misses are tolerated up to the budget" )
    {
        // no gs reply, so every poll has a soft timeout
        port.start();
        app.m_fd = port.appFd();
        app.fastTimeouts();
        app.m_commMaxMisses = 2;

        REQUIRE( app.pollDevice_() == 1 );
        REQUIRE( app.m_commMisses == 1 );
        REQUIRE( app.pollDevice_() == 1 );
        REQUIRE( app.m_commMisses == 2 );
        REQUIRE( app.pollDevice_() == -1 );
        REQUIRE( app.m_commMisses == 0 );
    }

    SECTION( "a successful poll resets the miss count" )
    {
        port.reply( "0gs", "0GS00" );
        port.start();
        app.m_fd         = port.appFd();
        app.m_commMisses = 2;

        REQUIRE( app.pollDevice_() == 0 );
        REQUIRE( app.m_commMisses == 0 );
    }

    SECTION( "no port is a hard error" )
    {
        REQUIRE( app.pollDevice_() == -1 );
    }
}

/// Verify the number INDI callbacks (absDeg, relDeg, velocity).
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl number INDI callbacks", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::newCallBack_m_ipAbsDeg( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipRelDeg( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipVelPct( pcf::IndiProperty() );
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );
    app.standardPulses();
    fakeElliptec port;
    port.defaultReply( "0PO00010000" );
    port.start();
    app.m_fd = port.appFd();

    SECTION( "absDeg starts an absolute move" )
    {
        REQUIRE( app.newCallBack_m_ipAbsDeg( numberProp( "absDeg", 90.0 ) ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::MoveAbs );
        REQUIRE( app.m_moving == 1 );
        REQUIRE( app.m_statusHint == "Moving" );
        REQUIRE( app.m_posDeg == Approx( 90.0 ) );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ma00010000", "0gp" } );
    }

    SECTION( "absDeg rejects a wrong device or a missing target" )
    {
        pcf::IndiProperty ip = numberProp( "absDeg", 90.0 );
        ip.setDevice( "other" );
        REQUIRE( app.newCallBack_m_ipAbsDeg( ip ) == -1 );

        pcf::IndiProperty empty( pcf::IndiProperty::Number );
        empty.setDevice( "elliptec" );
        empty.setName( "absDeg" );
        REQUIRE( app.newCallBack_m_ipAbsDeg( empty ) == -1 );

        port.stop();
        REQUIRE( port.commands().empty() );
    }

    SECTION( "relDeg sets the clamped step size without moving" )
    {
        REQUIRE( app.newCallBack_m_ipRelDeg( numberProp( "relDeg", 5.5 ) ) == 0 );
        REQUIRE( app.m_relStepDeg == Approx( 5.5 ) );

        REQUIRE( app.newCallBack_m_ipRelDeg( numberProp( "relDeg", 900.0 ) ) == 0 );
        REQUIRE( app.m_relStepDeg == Approx( 720.0 ) );

        REQUIRE( app.newCallBack_m_ipRelDeg( numberProp( "relDeg", -1000.0 ) ) == 0 );
        REQUIRE( app.m_relStepDeg == Approx( -720.0 ) );

        REQUIRE( app.newCallBack_m_ipRelDeg( numberProp( "absDeg", 1.0 ) ) == -1 );

        port.stop();
        REQUIRE( port.commands().empty() );
    }

    SECTION( "velocity sends the clamped percentage" )
    {
        REQUIRE( app.newCallBack_m_ipVelPct( numberProp( "velocity", 55 ) ) == 0 );
        REQUIRE( app.m_velPercent == 55 );

        REQUIRE( app.newCallBack_m_ipVelPct( numberProp( "velocity", 150 ) ) == 0 );
        REQUIRE( app.m_velPercent == 100 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0sv37", "0sv64" } );
    }

    SECTION( "velocity fails without a port and keeps the old value" )
    {
        port.stop();
        app.m_fd = -1;

        REQUIRE( app.newCallBack_m_ipVelPct( numberProp( "velocity", 55 ) ) == -1 );
        REQUIRE( app.m_velPercent == 40 );
    }
}

/// Verify the request switch INDI callbacks (relMove, optimize, save, home, stop).
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl request switch INDI callbacks", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::newCallBack_m_ipRelMove( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipOptimize( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipSave( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipHome( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipStop( pcf::IndiProperty() );
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );
    app.standardPulses();
    fakeElliptec port;
    port.defaultReply( "0GS00" );
    port.start();
    app.m_fd = port.appFd();

    SECTION( "requests that are off, or have no request element, do nothing" )
    {
        REQUIRE( app.newCallBack_m_ipRelMove( requestProp( "relMove", false ) ) == 0 );
        REQUIRE( app.newCallBack_m_ipOptimize( requestProp( "optimize", false ) ) == 0 );
        REQUIRE( app.newCallBack_m_ipSave( requestProp( "save", false ) ) == 0 );
        REQUIRE( app.newCallBack_m_ipHome( requestProp( "home", false ) ) == 0 );
        REQUIRE( app.newCallBack_m_ipStop( requestProp( "stop", false ) ) == 0 );

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "elliptec" );
        ip.setName( "home" );
        REQUIRE( app.newCallBack_m_ipHome( ip ) == 0 );

        REQUIRE( app.m_pending == elliptecCtrl::Pending::None );

        port.stop();
        REQUIRE( port.commands().empty() );
    }

    SECTION( "relMove moves by the step size" )
    {
        app.m_relStepDeg = 90.0;
        REQUIRE( app.newCallBack_m_ipRelMove( requestProp( "relMove", true ) ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::MoveRel );
        REQUIRE( app.m_moving == 1 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0mr00010000", "0gp" } );
    }

    SECTION( "optimize and save send their command aliases" )
    {
        REQUIRE( app.newCallBack_m_ipOptimize( requestProp( "optimize", true ) ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Optimize );
        REQUIRE( app.m_statusHint == "Running Optimization Routine" );

        REQUIRE( app.newCallBack_m_ipSave( requestProp( "save", true ) ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Save );
        REQUIRE( app.m_statusHint == "Saving Tuning Parameters" );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0om", "0us" } );
    }

    SECTION( "home starts homing and stop stops" )
    {
        REQUIRE( app.newCallBack_m_ipHome( requestProp( "home", true ) ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::Home );
        REQUIRE( app.m_moving == 2 );

        REQUIRE( app.newCallBack_m_ipStop( requestProp( "stop", true ) ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::None );
        REQUIRE( app.m_moving == 0 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ho0", "0gp", "0st" } );
    }

    SECTION( "a wrong property name is rejected" )
    {
        REQUIRE( app.newCallBack_m_ipHome( requestProp( "stop", true ) ) == -1 );
        REQUIRE( app.newCallBack_m_ipStop( requestProp( "home", true ) ) == -1 );

        port.stop();
        REQUIRE( port.commands().empty() );
    }
}

/// Verify the stageGoto preset selection callback.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl stageGoto INDI callback", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::newCallBack_m_ipStageGoto( pcf::IndiProperty() );
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );
    app.standardPulses();
    fakeElliptec port;
    port.defaultReply( "0PO00000000" );
    port.start();
    app.m_fd = port.appFd();

    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "elliptec" );
    ip.setName( "stageGoto" );
    ip.add( pcf::IndiElement( "in" ) );
    ip.add( pcf::IndiElement( "out" ) );
    ip["in"].setSwitchState( pcf::IndiElement::Off );
    ip["out"].setSwitchState( pcf::IndiElement::On );

    SECTION( "no presets configured does nothing" )
    {
        REQUIRE( app.newCallBack_m_ipStageGoto( ip ) == 0 );

        port.stop();
        REQUIRE( port.commands().empty() );
    }

    SECTION( "the selected preset is moved to" )
    {
        app.m_userPresetNames = { "in", "out" };
        app.m_userPresetDeg   = { 10.0, 20.0 };

        REQUIRE( app.newCallBack_m_ipStageGoto( ip ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::MoveAbs );
        REQUIRE( app.m_moving == 1 );
        REQUIRE( app.m_statusHint == "Moving to preset: out" );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0ma000038E4", "0gp" } );
    }

    SECTION( "no selection does nothing" )
    {
        app.m_userPresetNames = { "in", "out" };
        app.m_userPresetDeg   = { 10.0, 20.0 };
        ip["out"].setSwitchState( pcf::IndiElement::Off );

        REQUIRE( app.newCallBack_m_ipStageGoto( ip ) == 0 );
        REQUIRE( app.m_pending == elliptecCtrl::Pending::None );

        port.stop();
        REQUIRE( port.commands().empty() );
    }
}

/// Verify the appLogic() guards and the connection sequence on a pseudo-terminal.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl appLogic", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::appLogic();
    elliptecCtrl::appShutdown();
    elliptecCtrl::openPort_();
    #endif
    // clang-format on

    SECTION( "INITIALIZED is an error" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.state( stateCodes::INITIALIZED );
        REQUIRE( app.appLogic() == -1 );
    }

    SECTION( "nothing is done while power is not on" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.m_port = "/nonexistent/elliptecCtrl_test_tty";
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_connected == false );
        REQUIRE( app.m_moving == -1 );
    }

    SECTION( "a missing port leaves the app NOTCONNECTED" )
    {
        elliptecCtrl_test app( "elliptec" );
        app.m_powerState       = 1;
        app.m_powerTargetState = 1;
        app.m_startupDelayMs   = 0;
        app.m_port             = "/nonexistent/elliptecCtrl_test_tty";

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_connected == false );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( app.m_moving == -2 );
    }

    SECTION( "connecting queries the device and then polls it" )
    {
        fakeElliptec port( fakeElliptec::mode::pty );

        if( !port.ok() )
        {
            WARN( "no pseudo-terminal available, skipping the connection test" );
            return;
        }

        port.reply( "0in", "0IN061234567820230101016800040000" );
        port.reply( "0gp", "0PO00010000" );
        port.reply( "0gs", "0GS00" );
        port.reply( "0sv28", "0GS00" );
        port.start();

        elliptecCtrl_test app( "elliptec" );
        app.m_powerState       = 1;
        app.m_powerTargetState = 1;
        app.m_startupDelayMs   = 0;
        app.m_port             = port.slavePath();

        REQUIRE( app.appLogic() == 0 );

        REQUIRE( app.m_connected == true );
        REQUIRE( app.m_fd >= 0 );
        REQUIRE( app.m_pulsesPerRev == 262144 );
        REQUIRE( app.m_posPulses == 65536 );
        REQUIRE( app.m_posDeg == Approx( 90.0 ) );
        REQUIRE( app.m_gs == 0x00 );

        // The velocity command leaves Pending::Velocity, which the idle poll resolves to not-moving.
        REQUIRE( app.m_pending == elliptecCtrl::Pending::None );
        REQUIRE( app.m_moving == 0 );

        // Without appStartup() the telemetry log thread is not running, so the telemeter logic fails the app.
        REQUIRE( app.state() == stateCodes::FAILURE );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( app.m_fd == -1 );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "0in", "0gp", "0gs", "0sv28", "0gs", "0gp", "0gs" } );
    }
}

/// Verify the telemetry recording hooks.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl telemetry recording", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::recordStage( true );
    elliptecCtrl::recordTelem( nullptr );
    elliptecCtrl::checkRecordTimes();
    #endif
    // clang-format on

    elliptecCtrl_test app( "elliptec" );

    SECTION( "a forced record updates the telemetry timestamp" )
    {
        telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.recordStage( true ) == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordTelem forces a record" )
    {
        telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const telem_stage *>( nullptr ) ) == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records only after the interval has elapsed" )
    {
        telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );

        timespec last = telem_stage::lastRecord;
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec == last.tv_sec );
        REQUIRE( telem_stage::lastRecord.tv_nsec == last.tv_nsec );
    }
}

} // namespace elliptecCtrlTest

} // namespace libXWCTest
