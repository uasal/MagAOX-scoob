/** \file filterWheelCtrl_test.cpp
 * \brief Catch2 tests for the filterWheelCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup filterWheelCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// Exposes the protected state of the app, of dev::stdMotionStage, and of the telemetry logManager (so the
// telemetry thread can be marked as running without starting it).
#define protected public
#include "../filterWheelCtrl.hpp"
#undef protected

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup filterWheelCtrl_unit_test filterWheelCtrl Unit Tests
 * \brief Unit tests for the filterWheelCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `filterWheelCtrl` unit tests.
/** \ingroup filterWheelCtrl_unit_test
 */
namespace filterWheelCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Directory the test harness points the telemetry logger at.
const char *telemPath = "/tmp/filterWheelCtrl_test_telem";

/// Removes the telemetry directory at program exit (telemetry is written when each app is destroyed).
struct telemCleanup
{
    /// Remove the telemetry directory.
    ~telemCleanup()
    {
        std::error_code ec;
        std::filesystem::remove_all( telemPath, ec );
    }
};

/// The cleanup object.
telemCleanup s_telemCleanup;

/// Test harness for filterWheelCtrl.
class filterWheelCtrl_test : public filterWheelCtrl
{
  public:
    /// Construct a harness with the given device name.
    /** Creates the counts property as appStartup() does, points telemetry at /tmp, and marks the telemetry thread
     * as running so `telemeter::appLogic()` does not force FAILURE.
     */
    explicit filterWheelCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        static_cast<void>( m_tel.logPath( telemPath ) );
        m_tel.m_logThreadRunning = true;
        m_readTimeOut            = 200;
        m_writeTimeOut           = 200;

        createStandardIndiNumber<long>( m_indiP_counts,
                                        "counts",
                                        std::numeric_limits<long>::lowest(),
                                        std::numeric_limits<long>::max(),
                                        0.0,
                                        "%ld" );
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
        static_cast<void>( m_tel.logPath( telemPath ) );
    }

    /// Configure a 6-filter wheel and create the stdMotionStage INDI properties.
    /** Filters f1..f6 at positions 1..6, 6000 counts per revolution, home offset 100.
     */
    void setupWheel()
    {
        m_presetNames     = { "f1", "f2", "f3", "f4", "f5", "f6" };
        m_presetPositions = { 1, 2, 3, 4, 5, 6 };
        m_circleSteps     = 6000;
        m_homeOffset      = 100;
        dev::stdMotionStage<filterWheelCtrl>::appStartup();
    }
};

/// A fake MCBL controller on the far end of a socket pair.
/** Records each `\r` terminated command, and answers the GAST, GN and POS queries with configured replies followed
 * by `\r\n`.
 */
class fakeMCBL
{
  public:
    /// Create the socket pair.
    fakeMCBL()
    {
        m_ok = ( socketpair( AF_UNIX, SOCK_STREAM, 0, m_fds ) == 0 );
    }

    /// Stop the responder and close the sockets.
    ~fakeMCBL()
    {
        stop();
        if( m_ok )
        {
            close( m_fds[0] );
            close( m_fds[1] );
        }
    }

    /// Whether the socket pair was created.
    bool ok() const
    {
        return m_ok;
    }

    /// The file descriptor for the app to use.
    int appFd() const
    {
        return m_fds[0];
    }

    /// Set the query replies (without the trailing `\r\n`).  Call before start().
    void replies( const std::string &gast, /**< [in] reply to GAST (home switch status) */
                  const std::string &gn,   /**< [in] reply to GN (speed) */
                  const std::string &pos /**< [in] reply to POS (position) */ )
    {
        m_gast = gast;
        m_gn   = gn;
        m_pos  = pos;
    }

    /// Set whether queries are answered.  Call before start().
    void silent( bool s /**< [in] true to never answer */ )
    {
        m_silent = s;
    }

    /// Start the responder thread.
    void start()
    {
        m_stop   = false;
        m_thread = std::thread( [this]() { run(); } );
    }

    /// Stop the responder thread after it has consumed everything already written.
    void stop()
    {
        m_stop = true;
        if( m_thread.joinable() )
        {
            m_thread.join();
        }
    }

    /// The commands received, without the `\r`.  Only valid after stop().
    const std::vector<std::string> &commands() const
    {
        return m_commands;
    }

  private:
    /// Send the reply to a query command.
    void reply( const std::string &cmd /**< [in] the command received */ )
    {
        if( m_silent )
        {
            return;
        }

        std::string resp;
        if( cmd == "GAST" )
        {
            resp = m_gast + "\r\n";
        }
        else if( cmd == "GN" )
        {
            resp = m_gn + "\r\n";
        }
        else if( cmd == "POS" )
        {
            resp = m_pos + "\r\n";
        }
        else
        {
            return;
        }

        if( write( m_fds[1], resp.data(), resp.size() ) < 0 )
        {
            return;
        }
    }

    /// The responder loop.
    void run()
    {
        std::string cur;
        while( true )
        {
            bool stopping = m_stop;

            pollfd pfd;
            pfd.fd      = m_fds[1];
            pfd.events  = POLLIN;
            pfd.revents = 0;
            if( poll( &pfd, 1, stopping ? 0 : 10 ) <= 0 )
            {
                if( stopping )
                {
                    break;
                }
                continue;
            }

            char    buf[256];
            ssize_t rv = read( m_fds[1], buf, sizeof( buf ) );
            if( rv <= 0 )
            {
                if( stopping )
                {
                    break;
                }
                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                continue;
            }

            for( ssize_t k = 0; k < rv; ++k )
            {
                if( buf[k] == '\r' )
                {
                    m_commands.push_back( cur );
                    reply( cur );
                    cur.clear();
                }
                else
                {
                    cur += buf[k];
                }
            }
        }

        if( !cur.empty() )
        {
            m_commands.push_back( cur );
        }
    }

    int                      m_fds[2]{ -1, -1 }; ///< The socket pair: [0] is the app end, [1] is the fake end.
    bool                     m_ok{ false };      ///< True if the socket pair was created.
    std::string              m_gast{ "0" };      ///< Reply to GAST.
    std::string              m_gn{ "0" };        ///< Reply to GN.
    std::string              m_pos{ "0" };       ///< Reply to POS.
    bool                     m_silent{ false };  ///< If true, queries are not answered.
    std::atomic<bool>        m_stop{ false };    ///< Flag to stop the responder thread.
    std::thread              m_thread;           ///< The responder thread.
    std::vector<std::string> m_commands;         ///< Commands received.
};

/// The commands sent by home().
std::vector<std::string> homeCommands()
{
    return { "EN", "HA4", "HL4", "CAHOSEQ", "HP0", "HOSP3000.000000", "GOHOSEQ" };
}

/// The commands sent by moveToRaw().
std::vector<std::string> moveCommands( long counts /**< [in] the absolute position in counts */ )
{
    return { "EN", "LA" + std::to_string( counts ), "M" };
}

/// The commands sent by one pass of the status queries in appLogic().
std::vector<std::string> queryCommands()
{
    return { "GAST", "GN", "POS" };
}

/// Concatenate command lists.
std::vector<std::string> operator+( std::vector<std::string>        a, /**< [in] the first list */
                                    const std::vector<std::string> &b /**< [in] the second list */ )
{
    a.insert( a.end(), b.begin(), b.end() );
    return a;
}

/// \endcond

/// Verify filterWheelCtrl construction defaults.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl construction defaults", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl();
    #endif
    // clang-format on

    filterWheelCtrl app;

    REQUIRE( app.m_presetNotation == "filter" );
    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_motorType == 2 );
    REQUIRE( app.m_writeTimeOut == 1000 );
    REQUIRE( app.m_readTimeOut == 1000 );
    REQUIRE( app.m_acceleration == Approx( 100 ) );
    REQUIRE( app.m_deceleration == Approx( 50 ) );
    REQUIRE( app.m_motorSpeed == Approx( 3000 ) );
    REQUIRE( app.m_circleSteps == 0 );
    REQUIRE( app.m_homeOffset == 0 );
    REQUIRE( app.m_switch == false );
    REQUIRE( app.m_rawPos == 0 );
    REQUIRE( app.m_homingState == 0 );
    REQUIRE( app.appShutdown() == 0 );
}

/// Verify filterWheelCtrl configuration defaults and overrides, including the stdMotionStage presets.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl configuration", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::setupConfig();
    filterWheelCtrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        filterWheelCtrl_test app( "fw" );

        mx::app::writeConfigFile( "/tmp/filterWheelCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
        app.configure( "/tmp/filterWheelCtrl_test_defaults.conf" );
        std::remove( "/tmp/filterWheelCtrl_test_defaults.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_baudRate == B9600 );
        REQUIRE( app.m_writeTimeOut == 200 ); // unchanged from the harness value
        REQUIRE( app.m_readTimeOut == 200 );
        REQUIRE( app.m_acceleration == Approx( 100 ) );
        REQUIRE( app.m_deceleration == Approx( 50 ) );
        REQUIRE( app.m_motorSpeed == Approx( 3000 ) );
        REQUIRE( app.m_circleSteps == 0 );
        REQUIRE( app.m_homeOffset == 0 );
        REQUIRE( app.m_powerOnHome == false );
        REQUIRE( app.m_homePreset == -1 );
        REQUIRE( app.m_presetNames.empty() );
        REQUIRE( app.m_presetPositions.empty() );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
    }

    SECTION( "overrides" )
    {
        filterWheelCtrl_test app( "fw" );

        mx::app::writeConfigFile( "/tmp/filterWheelCtrl_test_override.conf",
                                  { "usb",
                                    "usb",
                                    "usb",
                                    "usb",
                                    "timeouts",
                                    "timeouts",
                                    "motor",
                                    "motor",
                                    "motor",
                                    "motor",
                                    "stage",
                                    "stage",
                                    "stage",
                                    "filters",
                                    "filters",
                                    "telemeter" },
                                  { "idVendor",
                                    "idProduct",
                                    "serial",
                                    "baud",
                                    "write",
                                    "read",
                                    "acceleration",
                                    "deceleration",
                                    "speed",
                                    "circleSteps",
                                    "homeOffset",
                                    "powerOnHome",
                                    "homePreset",
                                    "names",
                                    "positions",
                                    "maxInterval" },
                                  { "fffe",
                                    "fffd",
                                    "FW1",
                                    "19200",
                                    "1500",
                                    "2500",
                                    "120",
                                    "60",
                                    "2000",
                                    "54000",
                                    "-150",
                                    "true",
                                    "2",
                                    "open,J,H,Ks",
                                    "1,0,3.5,0",
                                    "4.5" } );
        app.configure( "/tmp/filterWheelCtrl_test_override.conf" );
        std::remove( "/tmp/filterWheelCtrl_test_override.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_idVendor == "fffe" );
        REQUIRE( app.m_idProduct == "fffd" );
        REQUIRE( app.m_serial == "FW1" );
        REQUIRE( app.m_baudRate == B19200 );
        REQUIRE( app.m_writeTimeOut == 1500 );
        REQUIRE( app.m_readTimeOut == 2500 );
        REQUIRE( app.m_acceleration == Approx( 120 ) );
        REQUIRE( app.m_deceleration == Approx( 60 ) );
        REQUIRE( app.m_motorSpeed == Approx( 2000 ) );
        REQUIRE( app.m_circleSteps == 54000 );
        REQUIRE( app.m_homeOffset == -150 );
        REQUIRE( app.m_powerOnHome == true );
        REQUIRE( app.m_homePreset == 2 );
        REQUIRE( app.m_presetNames == std::vector<std::string>{ "open", "J", "H", "Ks" } );
        REQUIRE( app.m_presetPositions.size() == 4 );
        REQUIRE( app.m_presetPositions[0] == Approx( 1.0 ) );
        REQUIRE( app.m_presetPositions[1] == Approx( 2.0 ) ); // 0 is replaced by the index + 1
        REQUIRE( app.m_presetPositions[2] == Approx( 3.5 ) );
        REQUIRE( app.m_presetPositions[3] == Approx( 4.0 ) );
        REQUIRE( app.m_maxInterval == Approx( 4.5 ) );
    }

    SECTION( "names without positions use the default positions" )
    {
        filterWheelCtrl_test app( "fw" );

        mx::app::writeConfigFile( "/tmp/filterWheelCtrl_test_names.conf", { "filters" }, { "names" }, { "a,b,c" } );
        app.configure( "/tmp/filterWheelCtrl_test_names.conf" );
        std::remove( "/tmp/filterWheelCtrl_test_names.conf" );

        REQUIRE( app.m_presetNames.size() == 3 );
        REQUIRE( app.m_presetPositions.size() == 3 );
        REQUIRE( app.m_presetPositions[0] == Approx( 1.0 ) );
        REQUIRE( app.m_presetPositions[1] == Approx( 2.0 ) );
        REQUIRE( app.m_presetPositions[2] == Approx( 3.0 ) );
    }
}

/// Verify presetNumber() rounds the preset position and wraps it around the wheel.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl presetNumber wraps around the wheel", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::presetNumber();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();

    auto [preset, expected] = GENERATE( table<float, int>( { { 1.0f, 0 },
                                                             { 1.4f, 0 },
                                                             { 1.6f, 1 },
                                                             { 3.0f, 2 },
                                                             { 6.4f, 5 },
                                                             { 6.6f, 0 },
                                                             { 7.0f, 0 },
                                                             { 13.0f, 0 },
                                                             { 0.4f, 5 },
                                                             { -5.0f, 0 } } ) );

    app.m_preset = preset;
    REQUIRE( app.presetNumber() == expected );
}

/// Verify onPowerOnConnect() sends the controller setup commands.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl onPowerOnConnect setup commands", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::onPowerOnConnect();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    fakeMCBL             dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    dev.start();

    REQUIRE( app.onPowerOnConnect() == 0 );
    dev.stop();

    REQUIRE( dev.commands() ==
             std::vector<std::string>{ "ANSW0", "MOTTYP2", "AC100.000000", "DEC50.000000", "SP3000.000000" } );

    app.m_fileDescrip = 0;
}

/// Verify the motion commands: home(), stop(), moveToRaw(), moveToRawRelative(), and startHoming().
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl motion commands", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::home();
    filterWheelCtrl::stop();
    filterWheelCtrl::moveToRaw(0);
    filterWheelCtrl::moveToRawRelative(0);
    filterWheelCtrl::startHoming();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();
    fakeMCBL dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    dev.start();

    SECTION( "home" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.home() == 0 );
        dev.stop();
        REQUIRE( dev.commands() == homeCommands() );
        REQUIRE( app.state() == stateCodes::HOMING );
    }

    SECTION( "stop" )
    {
        app.m_homingState = 3;
        REQUIRE( app.stop() == 0 );
        dev.stop();
        REQUIRE( dev.commands() == std::vector<std::string>{ "DI", "DI" } );
        REQUIRE( app.m_homingState == 0 );
    }

    SECTION( "moveToRaw" )
    {
        app.m_moving = 0;
        REQUIRE( app.moveToRaw( 12345 ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == moveCommands( 12345 ) );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "moveToRawRelative" )
    {
        app.m_moving = 0;
        REQUIRE( app.moveToRawRelative( -20000 ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == std::vector<std::string>{ "EN", "LR-20000", "M" } );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "startHoming" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.startHoming() == 0 );
        dev.stop();
        REQUIRE( dev.commands() == homeCommands() );
        REQUIRE( app.m_homingState == 1 );
        REQUIRE( app.m_moving == 2 );
        REQUIRE( app.state() == stateCodes::HOMING );
    }

    app.m_fileDescrip = 0;
}

/// Verify moveTo() converts filter units to motor counts.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl moveTo converts filters to counts", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::moveTo(1.0);
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    fakeMCBL             dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    SECTION( "configured wheel: offset + circleSteps/nFilters*(filter-1)" )
    {
        app.setupWheel();

        auto [filter, counts] =
            GENERATE( table<double, long>( { { 1.0, 100 }, { 2.5, 1600 }, { 3.0, 2100 }, { 6.0, 5100 } } ) );

        dev.start();
        REQUIRE( app.moveTo( filter ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == moveCommands( counts ) );
    }

    SECTION( "no circleSteps: the value is used as counts" )
    {
        app.m_circleSteps = 0;
        dev.start();
        REQUIRE( app.moveTo( 7.9 ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == moveCommands( 7 ) );
    }

    app.m_fileDescrip = 0;
}

/// Verify the status queries parse the controller responses.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl status queries", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::getSwitch();
    filterWheelCtrl::getMoving();
    filterWheelCtrl::getPos();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    fakeMCBL             dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    SECTION( "getPos parses the position" )
    {
        auto [reply, pos] =
            GENERATE( table<std::string, long>( { { "12345", 12345 }, { "-77", -77 }, { "junk", 0 } } ) );
        dev.replies( "0", "0", reply );
        dev.start();
        app.m_rawPos = 999;
        REQUIRE( app.getPos() == 0 );
        dev.stop();
        REQUIRE( app.m_rawPos == pos );
        REQUIRE( dev.commands() == std::vector<std::string>{ "POS" } );
    }

    SECTION( "getMoving compares the speed to 10% of the motor speed" )
    {
        auto [reply, homing, moving] = GENERATE( table<std::string, int, int>( { { "0", 0, 0 },
                                                                                 { "300", 0, 0 },
                                                                                 { "301", 0, 1 },
                                                                                 { "-2500", 0, 1 },
                                                                                 { "2500", 1, 2 },
                                                                                 { "junk", 0, 0 } } ) );
        dev.replies( "0", reply, "0" );
        dev.start();
        app.m_homingState = homing;
        app.m_moving      = -1;
        REQUIRE( app.getMoving() == 0 );
        dev.stop();
        REQUIRE( app.m_moving == moving );
        REQUIRE( dev.commands() == std::vector<std::string>{ "GN" } );
    }

    SECTION( "getSwitch sends GAST" )
    {
        // The response still contains the "\r\n" terminator, so it never equals "1011" and m_switch stays false.
        dev.replies( "1011", "0", "0" );
        dev.start();
        app.m_switch = true;
        REQUIRE( app.getSwitch() == 0 );
        dev.stop();
        REQUIRE( app.m_switch == false );
        REQUIRE( dev.commands() == std::vector<std::string>{ "GAST" } );
    }

    SECTION( "no response is an error and leaves the state unchanged" )
    {
        dev.silent( true );
        dev.start();
        app.m_rawPos = 42;
        app.m_moving = 1;
        REQUIRE( app.getPos() < 0 );
        REQUIRE( app.getMoving() < 0 );
        REQUIRE( app.getSwitch() < 0 );
        dev.stop();
        REQUIRE( app.m_rawPos == 42 );
        REQUIRE( app.m_moving == 1 );
    }

    app.m_fileDescrip = 0;
}

/// Verify the counts new-callback validates the property and moves to the requested counts.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl counts new callback", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::newCallBack_m_indiP_counts(pcf::IndiProperty());
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();
    fakeMCBL dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    dev.start();

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "fw" );
    ip.setName( "counts" );

    SECTION( "wrong device" )
    {
        ip.setDevice( "other" );
        ip.add( pcf::IndiElement( "target", 3100 ) );
        REQUIRE( app.newCallBack_m_indiP_counts( ip ) == -1 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    SECTION( "wrong name" )
    {
        ip.setName( "position" );
        ip.add( pcf::IndiElement( "target", 3100 ) );
        REQUIRE( app.newCallBack_m_indiP_counts( ip ) == -1 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    SECTION( "target" )
    {
        app.m_movingState     = 1;
        app.m_presetNameIndex = 2;
        ip.add( pcf::IndiElement( "target", 3100 ) );
        REQUIRE( app.newCallBack_m_indiP_counts( ip ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == moveCommands( 3100 ) );
        REQUIRE( app.m_preset_target == Approx( 4.0 ) );
        REQUIRE( app.m_movingState == 0 );
        REQUIRE( app.m_presetNameIndex == -1 );
    }

    SECTION( "current is used when there is no target" )
    {
        ip.add( pcf::IndiElement( "current", 1100 ) );
        REQUIRE( app.newCallBack_m_indiP_counts( ip ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == moveCommands( 1100 ) );
        REQUIRE( app.m_preset_target == Approx( 2.0 ) );
    }

    app.m_fileDescrip = 0;
}

/// Verify the stdMotionStage preset and preset-name callbacks move the wheel.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl preset callbacks", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    dev::stdMotionStage<filterWheelCtrl>::newCallBack_m_indiP_preset(pcf::IndiProperty());
    dev::stdMotionStage<filterWheelCtrl>::newCallBack_m_indiP_presetName(pcf::IndiProperty());
    dev::stdMotionStage<filterWheelCtrl>::st_newCallBack_stdMotionStage(nullptr, pcf::IndiProperty());
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();
    fakeMCBL dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    dev.start();

    SECTION( "preset target moves in filter units" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fw" );
        ip.setName( "filter" );
        ip.add( pcf::IndiElement( "target", 2.0 ) );
        app.m_movingState = 1;

        REQUIRE( app.newCallBack_m_indiP_preset( ip ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == moveCommands( 1100 ) );
        REQUIRE( app.m_preset_target == Approx( 2.0 ) );
        REQUIRE( app.m_movingState == 0 );
    }

    SECTION( "preset with no value is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fw" );
        ip.setName( "filter" );

        REQUIRE( app.newCallBack_m_indiP_preset( ip ) == -1 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    SECTION( "preset name selects the preset position" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "fw" );
        ip.setName( "filterName" );
        ip.add( pcf::IndiElement( "f2", pcf::IndiElement::Off ) );
        ip.add( pcf::IndiElement( "f3", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == moveCommands( 2100 ) );
        REQUIRE( app.m_preset_target == Approx( 3.0 ) );
        REQUIRE( app.m_movingState == 1 );
        REQUIRE( app.m_presetNameIndex == 2 );
    }

    SECTION( "unknown preset name is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "fw" );
        ip.setName( "filterName" );
        ip.add( pcf::IndiElement( "bogus", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == -1 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    SECTION( "more than one preset name is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "fw" );
        ip.setName( "filterName" );
        ip.add( pcf::IndiElement( "f1", pcf::IndiElement::On ) );
        ip.add( pcf::IndiElement( "f4", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == -1 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    SECTION( "no preset name on is a no-op" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "fw" );
        ip.setName( "filterName" );
        ip.add( pcf::IndiElement( "f1", pcf::IndiElement::Off ) );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == 0 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    SECTION( "wrong device is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "other" );
        ip.setName( "filterName" );
        ip.add( pcf::IndiElement( "f1", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == -1 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    SECTION( "the static dispatcher routes by name" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fw" );
        ip.setName( "filter" );
        ip.add( pcf::IndiElement( "target", 6.0 ) );

        REQUIRE( filterWheelCtrl::st_newCallBack_stdMotionStage( &app, ip ) == 0 );

        pcf::IndiProperty bad( pcf::IndiProperty::Number );
        bad.setDevice( "fw" );
        bad.setName( "unknown" );
        REQUIRE( filterWheelCtrl::st_newCallBack_stdMotionStage( &app, bad ) == -1 );

        dev.stop();
        REQUIRE( dev.commands() == moveCommands( 5100 ) );
    }

    app.m_fileDescrip = 0;
}

/// Verify the stdMotionStage home and stop callbacks.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl home and stop callbacks", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    dev::stdMotionStage<filterWheelCtrl>::newCallBack_m_indiP_home(pcf::IndiProperty());
    dev::stdMotionStage<filterWheelCtrl>::newCallBack_m_indiP_stop(pcf::IndiProperty());
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();
    fakeMCBL dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    dev.start();

    SECTION( "home request starts homing" )
    {
        app.state( stateCodes::READY );
        app.m_movingState = 1;

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "fw" );
        ip.setName( "home" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_home( ip ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == homeCommands() );
        REQUIRE( app.m_homingState == 1 );
        REQUIRE( app.m_movingState == 0 );
        REQUIRE( app.state() == stateCodes::HOMING );
    }

    SECTION( "home request off is a no-op" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "fw" );
        ip.setName( "home" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::Off ) );

        REQUIRE( app.newCallBack_m_indiP_home( ip ) == 0 );

        ip.setName( "stop" );
        REQUIRE( app.newCallBack_m_indiP_home( ip ) == -1 );

        dev.stop();
        REQUIRE( dev.commands().empty() );
        REQUIRE( app.m_homingState == 0 );
    }

    SECTION( "stop request disables the motor" )
    {
        app.m_homingState = 2;
        app.m_movingState = 2;

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "fw" );
        ip.setName( "stop" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_stop( ip ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == std::vector<std::string>{ "DI", "DI" } );
        REQUIRE( app.m_homingState == 0 );
        REQUIRE( app.m_movingState == 0 );
    }

    SECTION( "stop with the wrong device is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "other" );
        ip.setName( "stop" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_stop( ip ) == -1 );
        dev.stop();
        REQUIRE( dev.commands().empty() );
    }

    app.m_fileDescrip = 0;
}

/// Verify appLogic() device discovery and connection states without hardware.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl appLogic without a device", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::appLogic();
    filterWheelCtrl::appStartup();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );

    // IDs which match no real device.
    app.m_idVendor  = "fffe";
    app.m_idProduct = "fffd";
    app.m_serial    = "filterWheelCtrl_test";

    SECTION( "appStartup refuses to start from UNINITIALIZED" )
    {
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
        REQUIRE( app.appStartup() == -1 );
    }

    SECTION( "INITIALIZED is an error" )
    {
        app.state( stateCodes::INITIALIZED );
        REQUIRE( app.appLogic() == -1 );
    }

    SECTION( "POWERON with no device name goes to NODEVICE" )
    {
        app.state( stateCodes::POWERON );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
    }

    SECTION( "POWERON with a stale device name fails to connect and goes to NODEVICE" )
    {
        app.m_deviceName = "/tmp/filterWheelCtrl_test_no_such_tty";
        app.state( stateCodes::POWERON );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_deviceName == "" );
    }

    SECTION( "ERROR while powered off waits and returns" )
    {
        app.m_powerState = 0;
        app.state( stateCodes::ERROR );
        REQUIRE( app.appLogic() == 0 );
    }
}

/// Verify appLogic() in CONNECTED sets up the controller and goes to NOTHOMED or starts homing.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl appLogic CONNECTED", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::appLogic();
    filterWheelCtrl::onPowerOnConnect();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();
    fakeMCBL dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    std::vector<std::string> setup = { "ANSW0", "MOTTYP2", "AC100.000000", "DEC50.000000", "SP3000.000000" };

    SECTION( "no power-on homing goes to NOTHOMED" )
    {
        dev.replies( "0", "0", "2100" );
        dev.start();
        app.m_powerOnHome = false;
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );
        dev.stop();

        REQUIRE( app.state() == stateCodes::NOTHOMED );
        REQUIRE( dev.commands() == setup + queryCommands() );
        REQUIRE( app.m_rawPos == 2100 );
        REQUIRE( app.m_preset == Approx( 3.0 ) );
    }

    SECTION( "power-on homing starts the homing sequence" )
    {
        dev.replies( "0", "3000", "0" );
        dev.start();
        app.m_powerOnHome = true;
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );
        dev.stop();

        REQUIRE( app.state() == stateCodes::HOMING );
        REQUIRE( app.m_homingState == 1 );
        REQUIRE( app.m_moving == 2 );
        REQUIRE( dev.commands() == setup + homeCommands() + queryCommands() );
    }

    app.m_fileDescrip = 0;
}

/// Verify the appLogic() homing sequence steps.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl appLogic homing sequence", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::appLogic();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();
    fakeMCBL dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    dev.replies( "0", "0", "2100" ); // stopped, at filter 3
    dev.start();

    app.m_homePreset = 4; // f5 at position 5

    app.state( stateCodes::HOMING );
    app.m_homingState = 1;

    // 1: back off
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_homingState == 2 );
    REQUIRE( app.state() == stateCodes::HOMING );

    // 2: home
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_homingState == 3 );
    REQUIRE( app.state() == stateCodes::HOMING );

    // 3: move to the home offset
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_homingState == 4 );

    // 4: move to the home preset
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_homingState == 5 );
    REQUIRE( app.m_preset_target == Approx( 5.0 ) );

    // 5: done
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_homingState == 0 );
    REQUIRE( app.state() == stateCodes::READY );
    REQUIRE( app.m_preset_target == Approx( 3.0 ) );
    REQUIRE( app.m_preset == Approx( 3.0 ) );

    dev.stop();

    std::vector<std::string> expected = queryCommands() + std::vector<std::string>{ "EN", "LR-50000", "M" } +
                                        queryCommands() + homeCommands() + queryCommands() + moveCommands( 100 ) +
                                        queryCommands() + moveCommands( 4100 ) + queryCommands();
    REQUIRE( dev.commands() == expected );

    app.m_fileDescrip = 0;
}

/// Verify the appLogic() preset move sequence and moving detection.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl appLogic moves", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::appLogic();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();
    fakeMCBL dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    SECTION( "READY and moving goes to OPERATING" )
    {
        dev.replies( "0", "2500", "1100" );
        dev.start();
        app.state( stateCodes::READY );

        REQUIRE( app.appLogic() == 0 );
        dev.stop();

        REQUIRE( app.state() == stateCodes::OPERATING );
        REQUIRE( app.m_moving == 1 );
        REQUIRE( app.m_preset == Approx( 2.0 ) );
        REQUIRE( dev.commands() == queryCommands() );
    }

    SECTION( "preset move: back off, then move to the target, then READY" )
    {
        dev.replies( "0", "0", "1100" );
        dev.start();
        app.state( stateCodes::OPERATING );
        app.m_movingState   = 1;
        app.m_preset_target = 4.0;

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_movingState == 2 );
        REQUIRE( app.state() == stateCodes::OPERATING );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_movingState == 3 );
        REQUIRE( app.state() == stateCodes::OPERATING );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_movingState == 0 );
        REQUIRE( app.state() == stateCodes::READY );

        dev.stop();

        std::vector<std::string> expected = queryCommands() + std::vector<std::string>{ "EN", "LR-20000", "M" } +
                                            queryCommands() + moveCommands( 3100 ) + queryCommands();
        REQUIRE( dev.commands() == expected );
    }

    SECTION( "a non-preset move stopping goes straight to READY" )
    {
        dev.replies( "0", "0", "1100" );
        dev.start();
        app.state( stateCodes::OPERATING );
        app.m_movingState = 0;

        REQUIRE( app.appLogic() == 0 );
        dev.stop();

        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( dev.commands() == queryCommands() );
    }

    SECTION( "no response from the controller goes to NOTCONNECTED" )
    {
        dev.silent( true );
        dev.start();
        app.state( stateCodes::READY );

        REQUIRE( app.appLogic() == 0 );
        dev.stop();

        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( dev.commands() == std::vector<std::string>{ "GAST" } );
    }

    app.m_fileDescrip = 0;
}

/// Verify the power-off hooks and position telemetry.
/**
 * \ingroup filterWheelCtrl_unit_test
 */
TEST_CASE( "filterWheelCtrl power off and telemetry", "[filterWheelCtrl]" )
{
    // clang-format off
    #ifdef FILTERWHEELCTRL_TEST_DOXYGEN_REF
    filterWheelCtrl::onPowerOff();
    filterWheelCtrl::whilePowerOff();
    filterWheelCtrl::recordPosition(true);
    filterWheelCtrl::recordStage(true);
    filterWheelCtrl::recordTelem((const telem_position *)nullptr);
    filterWheelCtrl::checkRecordTimes();
    #endif
    // clang-format on

    filterWheelCtrl_test app( "fw" );
    app.setupWheel();

    SECTION( "onPowerOff marks the stage powered off and records telemetry" )
    {
        MagAOX::logger::telem_stage::lastRecord    = { 0, 0 };
        MagAOX::logger::telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( app.m_moving == -2 );
        REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );
        REQUIRE( MagAOX::logger::telem_position::lastRecord.tv_sec != 0 );
        REQUIRE( app.whilePowerOff() == 0 );
    }

    SECTION( "recordPosition records on change or when forced" )
    {
        app.m_rawPos = 500;
        REQUIRE( app.recordPosition( true ) == 0 );

        MagAOX::logger::telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.recordPosition( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_position::lastRecord.tv_sec == 0 );

        app.m_rawPos = 501;
        REQUIRE( app.recordPosition( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_position::lastRecord.tv_sec != 0 );

        MagAOX::logger::telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_position *>( nullptr ) ) == 0 );
        REQUIRE( MagAOX::logger::telem_position::lastRecord.tv_sec != 0 );

        MagAOX::logger::telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_stage *>( nullptr ) ) == 0 );
        REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );
    }

    SECTION( "checkRecordTimes records stale telemetry" )
    {
        MagAOX::logger::telem_stage::lastRecord    = { 0, 0 };
        MagAOX::logger::telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );
        REQUIRE( MagAOX::logger::telem_position::lastRecord.tv_sec != 0 );
    }
}

} // namespace filterWheelCtrlTest

} // namespace libXWCTest
