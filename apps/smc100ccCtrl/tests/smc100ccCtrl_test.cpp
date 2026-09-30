/** \file smc100ccCtrl_test.cpp
 * \brief Catch2 tests for the smc100ccCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * The INDI callback bodies are live in this translation unit.  The validation-mode checks of every callback are in
 * smc100ccCtrl_indi_test.cpp.
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define protected public
#include "../smc100ccCtrl.hpp"
#undef protected

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup smc100ccCtrl_unit_test smc100ccCtrl Unit Tests
 * \brief Unit tests for the smc100ccCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `smc100ccCtrl` unit tests.
/** \ingroup smc100ccCtrl_unit_test
 */
namespace smc100ccCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness for smc100ccCtrl, with the INDI properties given their device and names.
class smc100ccCtrl_test : public smc100ccCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit smc100ccCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        m_indiP_position.setDevice( m_configName );
        m_indiP_position.setName( "position" );
        m_indiP_preset.setDevice( m_configName );
        m_indiP_preset.setName( "preset" );
        m_indiP_presetName.setDevice( m_configName );
        m_indiP_presetName.setName( "presetName" );
        m_indiP_home.setDevice( m_configName );
        m_indiP_home.setName( "home" );
        m_indiP_stop.setDevice( m_configName );
        m_indiP_stop.setName( "stop" );
    }

    /// Point the app at a fake tty descriptor and use short timeouts.
    void attach( int fd /**< [in] the app side of the fake tty */ )
    {
        m_fileDescrip  = fd;
        m_readTimeout  = 100;
        m_writeTimeout = 100;
    }

    /// Point the app at an invalid descriptor so that every write times out immediately.
    void detach()
    {
        m_fileDescrip  = -1;
        m_readTimeout  = 1;
        m_writeTimeout = 1;
    }
};

/// A fake SMC100CC controller on a socket pair.
/** Replies are written to the device side before the app sends its command, so they are waiting in the app side
 * when it reads.  Each reply must be queued just before the single read that consumes it, because the tty reader
 * takes everything available in one read.
 */
class fakeSmc
{
  public:
    /// Create the socket pair.
    fakeSmc()
    {
        int fds[2];
        if( ::socketpair( AF_UNIX, SOCK_STREAM, 0, fds ) == 0 )
        {
            m_appFd = fds[0];
            m_devFd = fds[1];
        }
    }

    /// Close both ends of the socket pair.
    ~fakeSmc()
    {
        if( m_appFd >= 0 )
        {
            ::close( m_appFd );
        }

        if( m_devFd >= 0 )
        {
            ::close( m_devFd );
        }
    }

    /// Whether the socket pair was created.
    bool ok() const
    {
        return m_appFd >= 0 && m_devFd >= 0;
    }

    /// The descriptor to give to the app.
    int appFd() const
    {
        return m_appFd;
    }

    /// Queue a reply for the app to read.
    void reply( const std::string &resp /**< [in] the raw reply, including the CRLF terminator */ )
    {
        ssize_t rv = ::write( m_devFd, resp.data(), resp.size() );
        static_cast<void>( rv );
    }

    /// Return everything the app has written so far.
    std::string received()
    {
        std::string out;
        char        buf[1024];

        while( true )
        {
            struct pollfd pfd;
            pfd.fd      = m_devFd;
            pfd.events  = POLLIN;
            pfd.revents = 0;

            if( ::poll( &pfd, 1, 0 ) <= 0 )
            {
                break;
            }

            ssize_t n = ::read( m_devFd, buf, sizeof( buf ) );
            if( n <= 0 )
            {
                break;
            }

            out.append( buf, n );
        }

        return out;
    }

  protected:
    int m_appFd{ -1 }; ///< The app side of the socket pair.

    int m_devFd{ -1 }; ///< The fake device side of the socket pair.
};

/// Write a config file, read it into the app configurator, and remove the file.
/**
 * \returns the return value of readConfig()
 */
int readTestConfig( smc100ccCtrl_test              &app,      /**< [in,out] the app, with setupConfig() called */
                    const std::string              &tag,      /**< [in] a unique tag for the file name */
                    const std::vector<std::string> &sections, /**< [in] the config sections */
                    const std::vector<std::string> &keywords, /**< [in] the config keywords */
                    const std::vector<std::string> &values /**< [in] the config values */ )
{
    std::string fname = "/tmp/smc100ccCtrl_test_" + tag + "_" + std::to_string( ::getpid() ) + ".conf";
    mx::app::writeConfigFile( fname, sections, keywords, values );
    int rv = app.config.readConfig( fname );
    ::unlink( fname.c_str() );
    return rv;
}

/// Build a `position` number property for the harness device.
pcf::IndiProperty positionProp( const std::string &device, /**< [in] the property device */
                                const std::string &name,   /**< [in] the property name */
                                double             target /**< [in] the target value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
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
    ip.setDevice( "smc" );
    ip.setName( name );
    ip.add( pcf::IndiElement( "request" ) );
    ip["request"].setSwitchState( on ? pcf::IndiElement::On : pcf::IndiElement::Off );
    return ip;
}

/// \endcond

/// Verify the constructor defaults.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl construction defaults", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::smc100ccCtrl();
    #endif
    // clang-format on

    smc100ccCtrl_test app( "smc" );

    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_powerOnWait == 5 );
    REQUIRE( app.m_defaultPositions == false );
    REQUIRE( app.m_homingOffset == 0 );
    REQUIRE( app.m_opDelta == 0 );
    REQUIRE( app.m_position == 0 );
    REQUIRE( app.m_target == 0 );
    REQUIRE( app.m_wasHoming == false );
    REQUIRE( app.m_powerOnHomed == false );
    REQUIRE( app.m_moveOp == true );
}

/// Verify the configuration defaults.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl configuration defaults", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::setupConfig();
    smc100ccCtrl::loadConfig();
    #endif
    // clang-format on

    smc100ccCtrl_test app( "smc" );
    app.setupConfig();
    REQUIRE( readTestConfig( app, "defaults", { "none" }, { "nada" }, { "0" } ) == 0 );
    app.loadConfig();

    // No USB device is configured, which loadConfig() tolerates.
    REQUIRE( app.m_shutdown == 0 );

    REQUIRE( app.m_homingOffset == 0 );
    REQUIRE( app.m_opDelta == 0 );
    REQUIRE( app.m_baudRate == B57600 );
    REQUIRE( app.m_readTimeout == 1000 );
    REQUIRE( app.m_writeTimeout == 1000 );
    REQUIRE( app.m_powerOnHome == false );
    REQUIRE( app.m_homePreset == -1 );
    REQUIRE( app.m_presetNames.empty() );
    REQUIRE( app.m_presetPositions.empty() );
    REQUIRE( app.m_maxInterval == 10.0 );
}

/// Verify configuration overrides.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl configuration overrides", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "all values set" )
    {
        smc100ccCtrl_test app( "smc" );
        app.setupConfig();
        REQUIRE(
            readTestConfig(
                app,
                "overrides",
                { "stage", "stage", "stage", "stage", "usb", "device", "device", "presets", "presets", "telemeter" },
                { "homingOffset",
                  "opDelta",
                  "powerOnHome",
                  "homePreset",
                  "baud",
                  "readTimeout",
                  "writeTimeout",
                  "names",
                  "positions",
                  "maxInterval" },
                { "2.5", "0.1", "true", "1", "9600", "250", "300", "in,out", "1.5,3.0", "5" } ) == 0 );
        app.loadConfig();

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_homingOffset == Approx( 2.5 ) );
        REQUIRE( app.m_opDelta == Approx( 0.1 ) );
        REQUIRE( app.m_powerOnHome == true );
        REQUIRE( app.m_homePreset == 1 );
        REQUIRE( app.m_baudRate == B9600 );
        REQUIRE( app.m_readTimeout == 250 );
        REQUIRE( app.m_writeTimeout == 300 );
        REQUIRE( app.m_presetNames.size() == 2 );
        REQUIRE( app.m_presetNames[0] == "in" );
        REQUIRE( app.m_presetNames[1] == "out" );
        REQUIRE( app.m_presetPositions.size() == 2 );
        REQUIRE( app.m_presetPositions[0] == Approx( 1.5 ) );
        REQUIRE( app.m_presetPositions[1] == Approx( 3.0 ) );
        REQUIRE( app.m_maxInterval == Approx( 5.0 ) );
    }

    SECTION( "preset names without positions are not given default positions" )
    {
        smc100ccCtrl_test app( "smc" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nopos", { "presets" }, { "names" }, { "a,b,c" } ) == 0 );
        app.loadConfig();

        REQUIRE( app.m_presetNames.size() == 3 );
        REQUIRE( app.m_presetPositions.empty() );
    }
}

/// Verify the appStartup() error paths reached before any thread is started.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl appStartup error paths", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::appStartup();
    smc100ccCtrl::appShutdown();
    #endif
    // clang-format on

    SECTION( "UNINITIALIZED state is refused" )
    {
        smc100ccCtrl_test app( "smc" );
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
        REQUIRE( app.appStartup() == -1 );

        // The position property is registered before the state check.
        REQUIRE( app.m_indiP_position.getName() == "position" );
        REQUIRE( app.m_indiP_position.getDevice() == "smc" );
        REQUIRE( app.m_indiP_position.find( "current" ) );
        REQUIRE( app.m_indiP_position.find( "target" ) );
    }

    SECTION( "a preset name without a position is refused" )
    {
        smc100ccCtrl_test app( "smc" );
        app.state( stateCodes::INITIALIZED );
        app.m_presetNames     = { "in", "out" };
        app.m_presetPositions = { 1.0 };
        REQUIRE( app.appStartup() == -1 );

        // The "none" preset is not inserted on this error.
        REQUIRE( app.m_presetNames.size() == 2 );
    }

    SECTION( "appShutdown does nothing" )
    {
        smc100ccCtrl_test app( "smc" );
        REQUIRE( app.appShutdown() == 0 );
    }
}

/// Verify command construction and response splitting.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl command construction and response splitting", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::makeCom( std::string(), std::string() );
    smc100ccCtrl::splitResponse( 0, std::string(), std::string(), std::string() );
    #endif
    // clang-format on

    smc100ccCtrl_test app( "smc" );

    SECTION( "makeCom prefixes axis 1 and appends CRLF" )
    {
        std::string com;
        REQUIRE( app.makeCom( com, "TS" ) == 0 );
        REQUIRE( com == "1TS\r\n" );

        REQUIRE( app.makeCom( com, "PW1" ) == 0 );
        REQUIRE( com == "1PW1\r\n" );
    }

    int         axis = -1;
    std::string com, val;

    SECTION( "single digit axis with a value" )
    {
        std::string resp = "1TS000033\r\n";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == 0 );
        REQUIRE( axis == 1 );
        REQUIRE( com == "TS" );
        REQUIRE( val == "000033" );
    }

    SECTION( "single digit axis with a one-character value" )
    {
        std::string resp = "1TE@\r\n";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == 0 );
        REQUIRE( axis == 1 );
        REQUIRE( com == "TE" );
        REQUIRE( val == "@" );
    }

    SECTION( "single digit axis with no value" )
    {
        std::string resp = "1TP";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == 0 );
        REQUIRE( axis == 1 );
        REQUIRE( com == "TP" );
        REQUIRE( val == "" );

        resp = "1TP\r\n";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == 0 );
        REQUIRE( com == "TP" );
        REQUIRE( val == "" );
    }

    SECTION( "two digit axis with a value" )
    {
        std::string resp = "12TS00000A\r\n";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == 0 );
        REQUIRE( axis == 12 );
        REQUIRE( com == "TS" );
        REQUIRE( val == "00000A" );
    }

    SECTION( "two digit axis with no value" )
    {
        std::string resp = "12TS";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == 0 );
        REQUIRE( axis == 12 );
        REQUIRE( com == "TS" );
        REQUIRE( val == "" );
    }

    SECTION( "two digit axis that is too short" )
    {
        std::string resp = "12T";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == -1 );
        REQUIRE( com == "" );
        REQUIRE( val == "" );
    }

    SECTION( "a response starting with a letter is returned whole" )
    {
        std::string resp = "ABC";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == 0 );
        REQUIRE( axis == 0 );
        REQUIRE( com == "" );
        REQUIRE( val == "ABC" );
    }

    SECTION( "a response shorter than 3 characters is an error" )
    {
        std::string resp = "1T";
        REQUIRE( app.splitResponse( axis, com, val, resp ) == -1 );
    }
}

/// Verify controller state queries through getCtrlState().
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl controller state query", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::getCtrlState( std::string() );
    #endif
    // clang-format on

    fakeSmc dev;
    REQUIRE( dev.ok() );

    smc100ccCtrl_test app( "smc" );
    app.attach( dev.appFd() );

    std::string state = "xx";

    SECTION( "a valid reply gives the last two characters" )
    {
        dev.reply( "1TS000033\r\n" );
        REQUIRE( app.getCtrlState( state ) == 0 );
        REQUIRE( state == "33" );
        REQUIRE( dev.received() == "1TS\r\n" );
    }

    SECTION( "a not-referenced state" )
    {
        dev.reply( "1TS00000A\r\n" );
        REQUIRE( app.getCtrlState( state ) == 0 );
        REQUIRE( state == "0A" );
    }

    SECTION( "the wrong axis is an error" )
    {
        dev.reply( "2TS000033\r\n" );
        REQUIRE( app.getCtrlState( state ) == -1 );
        REQUIRE( state == "xx" );
    }

    SECTION( "the wrong command is an error" )
    {
        dev.reply( "1TP000033\r\n" );
        REQUIRE( app.getCtrlState( state ) == -1 );
        REQUIRE( state == "xx" );
    }

    SECTION( "the wrong value length is an error" )
    {
        dev.reply( "1TS0033\r\n" );
        REQUIRE( app.getCtrlState( state ) == -1 );
        REQUIRE( state == "xx" );
    }

    SECTION( "a reply with no command is an error" )
    {
        dev.reply( "ABC\r\n" );
        REQUIRE( app.getCtrlState( state ) == -1 );
    }

    SECTION( "no reply is an error" )
    {
        REQUIRE( app.getCtrlState( state ) == -1 );
        REQUIRE( dev.received() == "1TS\r\n" );
    }

    SECTION( "a write failure is an error" )
    {
        app.detach();
        REQUIRE( app.getCtrlState( state ) == -1 );
    }
}

/// Verify position and connection queries.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl position and connection queries", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::getPosition( 0.0 );
    smc100ccCtrl::testConnection();
    #endif
    // clang-format on

    fakeSmc dev;
    REQUIRE( dev.ok() );

    smc100ccCtrl_test app( "smc" );
    app.attach( dev.appFd() );

    double pos = -99;

    SECTION( "getPosition parses the TP reply" )
    {
        dev.reply( "1TP12.3456\r\n" );
        REQUIRE( app.getPosition( pos ) == 0 );
        REQUIRE( pos == Approx( 12.3456 ) );
        REQUIRE( dev.received() == "1TP\r\n" );
    }

    SECTION( "getPosition parses a negative position" )
    {
        dev.reply( "1TP-0.5\r\n" );
        REQUIRE( app.getPosition( pos ) == 0 );
        REQUIRE( pos == Approx( -0.5 ) );
    }

    SECTION( "getPosition rejects a non-numeric reply" )
    {
        dev.reply( "1TPxyz\r\n" );
        REQUIRE( app.getPosition( pos ) == -1 );
        REQUIRE( pos == -99 );
    }

    SECTION( "getPosition fails without a reply" )
    {
        REQUIRE( app.getPosition( pos ) == -1 );
        REQUIRE( pos == -99 );
    }

    SECTION( "testConnection sends TS and accepts any reply" )
    {
        dev.reply( "1TS00000A\r\n" );
        REQUIRE( app.testConnection() == 0 );
        REQUIRE( dev.received() == "1TS\r\n" );
    }

    SECTION( "testConnection fails without a reply" )
    {
        REQUIRE( app.testConnection() == -1 );
    }
}

/// Verify decoding of the TE error codes by getLastError().
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl error code decoding", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::getLastError( std::string() );
    #endif
    // clang-format on

    fakeSmc dev;
    REQUIRE( dev.ok() );

    smc100ccCtrl_test app( "smc" );
    app.attach( dev.appFd() );

    std::string errStr;

    SECTION( "no error" )
    {
        dev.reply( "1TE@\r\n" );
        REQUIRE( app.getLastError( errStr ) == 0 );
        REQUIRE( errStr == "" );
        REQUIRE( dev.received() == "1TE\r\n" );
    }

    SECTION( "each documented error code" )
    {
        std::vector<std::pair<char, std::string>> codes = {
            { 'A', "Unknown message code or floating point controller address." },
            { 'B', "Controller address not correct." },
            { 'C', "Parameter missing or out of range." },
            { 'D', "Command not allowed." },
            { 'E', "Home sequence already started." },
            { 'F', "ESP stage name unknown." },
            { 'G', "Displacement out of limits." },
            { 'H', "Command not allowed in NOT REFERENCED state." },
            { 'I', "Command not allowed in CONFIGURATION state." },
            { 'J', "Command not allowed in DISABLE state." },
            { 'K', "Command not allowed in READY state." },
            { 'L', "Command not allowed in HOMING state." },
            { 'M', "UCommand not allowed in MOVING state." },
            { 'N', "Current position out of software limit." },
            { 'S', "Communication Time Out." },
            { 'U', "Error during EEPROM access." },
            { 'V', "Error during command execution." },
            { 'W', "Command not allowed for PP version." },
            { 'X', "Command not allowed for CC version." },
            { 'Z', "unknown status" } };

        for( auto &code : codes )
        {
            errStr = "";
            dev.reply( std::string( "1TE" ) + code.first + "\r\n" );
            INFO( "code " << code.first );
            REQUIRE( app.getLastError( errStr ) == -1 );
            REQUIRE( errStr == code.second );
            REQUIRE( dev.received() == "1TE\r\n" );
        }
    }

    SECTION( "a truncated reply" )
    {
        dev.reply( "1\r\n" );
        REQUIRE( app.getLastError( errStr ) == -1 );
        REQUIRE( errStr == "Unknown output; controller not responding correctly." );
    }

    SECTION( "a write failure reports the tty error" )
    {
        app.detach();
        REQUIRE( app.getLastError( errStr ) == -1 );
        REQUIRE( errStr != "" );
    }

    SECTION( "a write failure while powering off is silent" )
    {
        app.detach();
        app.m_powerTargetState = 0;
        REQUIRE( app.getLastError( errStr ) == -1 );
        REQUIRE( errStr == "" );
    }
}

/// Verify the motion commands moveTo(), stop() and startHoming().
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl motion commands", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::moveTo( 0.0 );
    smc100ccCtrl::stop();
    smc100ccCtrl::startHoming();
    #endif
    // clang-format on

    fakeSmc dev;
    REQUIRE( dev.ok() );

    smc100ccCtrl_test app( "smc" );
    app.attach( dev.appFd() );
    app.state( stateCodes::READY );
    app.m_opDelta = 0.5;

    SECTION( "a large move sets OPERATING" )
    {
        dev.reply( "1TE@\r\n" );
        REQUIRE( app.moveTo( 2.0 ) == 0 );
        REQUIRE( dev.received() == "1PA2.000000\r\n1TE\r\n" );
        REQUIRE( app.m_moveOp == true );
        REQUIRE( app.state() == stateCodes::OPERATING );
    }

    SECTION( "a move smaller than opDelta does not set OPERATING" )
    {
        app.m_position = 1.0;
        dev.reply( "1TE@\r\n" );
        REQUIRE( app.moveTo( 1.25 ) == 0 );
        REQUIRE( dev.received() == "1PA1.250000\r\n1TE\r\n" );
        REQUIRE( app.m_moveOp == false );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "a controller error fails the move" )
    {
        dev.reply( "1TEG\r\n" );
        REQUIRE( app.moveTo( 100.0 ) == -1 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "a write failure fails the move" )
    {
        app.detach();
        REQUIRE( app.moveTo( 2.0 ) == -1 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "stop sends ST" )
    {
        REQUIRE( app.stop() == 0 );
        REQUIRE( dev.received() == "1ST\r\n" );
    }

    SECTION( "stop fails on a write failure" )
    {
        app.detach();
        REQUIRE( app.stop() == -1 );
    }

    SECTION( "startHoming sends OR" )
    {
        REQUIRE( app.startHoming() == 0 );
        REQUIRE( dev.received() == "1OR\r\n" );
    }

    SECTION( "startHoming fails on a write failure" )
    {
        app.detach();
        REQUIRE( app.startHoming() == -1 );
    }
}

/// Verify the preset number lookup.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl preset number lookup", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::presetNumber();
    #endif
    // clang-format on

    smc100ccCtrl_test app( "smc" );

    // As after appStartup(), with the "none" preset first.
    app.m_presetNames     = { "none", "in", "out" };
    app.m_presetPositions = { -1, 1.0, 2.5 };

    app.m_position = 1.0;
    REQUIRE( app.presetNumber() == 1 );

    app.m_position = 2.5;
    REQUIRE( app.presetNumber() == 2 );

    app.m_position = 2.5005;
    REQUIRE( app.presetNumber() == 2 );

    app.m_position = 2.51;
    REQUIRE( app.presetNumber() == 0 );

    // The "none" position is never matched.
    app.m_position = -1;
    REQUIRE( app.presetNumber() == 0 );
}

/// Verify the position INDI callback.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl position INDI callback", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::newCallBack_m_indiP_position( pcf::IndiProperty() );
    #endif
    // clang-format on

    fakeSmc dev;
    REQUIRE( dev.ok() );

    smc100ccCtrl_test app( "smc" );
    app.attach( dev.appFd() );

    SECTION( "wrong device or name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_position( positionProp( "wrong", "position", 1.0 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_position( positionProp( "smc", "wrong", 1.0 ) ) == -1 );
        REQUIRE( dev.received() == "" );
    }

    SECTION( "a request outside READY or OPERATING is ignored" )
    {
        app.state( stateCodes::NOTHOMED );
        REQUIRE( app.newCallBack_m_indiP_position( positionProp( "smc", "position", 3.0 ) ) == 0 );
        REQUIRE( app.m_target == 0 );
        REQUIRE( dev.received() == "" );
    }

    SECTION( "a target in READY moves the stage" )
    {
        app.state( stateCodes::READY );
        dev.reply( "1TE@\r\n" );
        REQUIRE( app.newCallBack_m_indiP_position( positionProp( "smc", "position", 3.0 ) ) == 0 );
        REQUIRE( app.m_target == Approx( 3.0 ) );
        REQUIRE( dev.received() == "1PA3.000000\r\n1TE\r\n" );
    }

    SECTION( "a target in OPERATING moves the stage" )
    {
        app.state( stateCodes::OPERATING );
        dev.reply( "1TE@\r\n" );
        REQUIRE( app.newCallBack_m_indiP_position( positionProp( "smc", "position", -1.5 ) ) == 0 );
        REQUIRE( app.m_target == Approx( -1.5 ) );
        REQUIRE( dev.received() == "1PA-1.500000\r\n1TE\r\n" );
    }
}

/// Verify the stdMotionStage INDI callbacks drive the SMC100CC commands.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl motion stage INDI callbacks", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::newCallBack_m_indiP_home( pcf::IndiProperty() );
    smc100ccCtrl::newCallBack_m_indiP_stop( pcf::IndiProperty() );
    smc100ccCtrl::newCallBack_m_indiP_preset( pcf::IndiProperty() );
    smc100ccCtrl::newCallBack_m_indiP_presetName( pcf::IndiProperty() );
    #endif
    // clang-format on

    fakeSmc dev;
    REQUIRE( dev.ok() );

    smc100ccCtrl_test app( "smc" );
    app.attach( dev.appFd() );
    app.state( stateCodes::READY );

    SECTION( "home request sends OR" )
    {
        REQUIRE( app.newCallBack_m_indiP_home( requestProp( "home", true ) ) == 0 );
        REQUIRE( dev.received() == "1OR\r\n" );
    }

    SECTION( "home switch off does nothing" )
    {
        REQUIRE( app.newCallBack_m_indiP_home( requestProp( "home", false ) ) == 0 );
        REQUIRE( dev.received() == "" );
    }

    SECTION( "stop request sends ST" )
    {
        REQUIRE( app.newCallBack_m_indiP_stop( requestProp( "stop", true ) ) == 0 );
        REQUIRE( dev.received() == "1ST\r\n" );
    }

    SECTION( "wrong names are rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_home( requestProp( "stop", true ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_stop( requestProp( "home", true ) ) == -1 );
        REQUIRE( dev.received() == "" );
    }

    SECTION( "preset names select the configured position" )
    {
        app.m_presetNames     = { "none", "in", "out" };
        app.m_presetPositions = { -1, 1.0, 2.5 };
        REQUIRE( app.dev::stdMotionStage<smc100ccCtrl>::appStartup() == 0 );

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "smc" );
        ip.setName( "presetName" );
        ip.add( pcf::IndiElement( "out" ) );
        ip["out"].setSwitchState( pcf::IndiElement::On );

        dev.reply( "1TE@\r\n" );
        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == 0 );
        REQUIRE( app.m_preset_target == Approx( 2.5 ) );
        REQUIRE( app.m_movingState == 1 );
        REQUIRE( dev.received() == "1PA2.500000\r\n1TE\r\n" );
    }

    SECTION( "an unknown preset name is rejected" )
    {
        app.m_presetNames     = { "none", "in", "out" };
        app.m_presetPositions = { -1, 1.0, 2.5 };
        REQUIRE( app.dev::stdMotionStage<smc100ccCtrl>::appStartup() == 0 );

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "smc" );
        ip.setName( "presetName" );
        ip.add( pcf::IndiElement( "sideways" ) );
        ip["sideways"].setSwitchState( pcf::IndiElement::On );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == -1 );
        REQUIRE( dev.received() == "" );
    }

    SECTION( "a numeric preset target moves to that position" )
    {
        app.m_presetNames     = { "none", "in", "out" };
        app.m_presetPositions = { -1, 1.0, 2.5 };
        REQUIRE( app.dev::stdMotionStage<smc100ccCtrl>::appStartup() == 0 );

        dev.reply( "1TE@\r\n" );
        REQUIRE( app.newCallBack_m_indiP_preset( positionProp( "smc", "preset", 2.0 ) ) == 0 );
        REQUIRE( app.m_preset_target == Approx( 2.0 ) );
        REQUIRE( app.m_movingState == 0 );
        REQUIRE( dev.received() == "1PA2.000000\r\n1TE\r\n" );
    }
}

/// Verify the appLogic() state machine for controller states reachable without an INDI driver.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl appLogic controller states", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::appLogic();
    #endif
    // clang-format on

    fakeSmc dev;
    REQUIRE( dev.ok() );

    smc100ccCtrl_test app( "smc" );
    app.attach( dev.appFd() );

    SECTION( "INITIALIZED is an error" )
    {
        app.state( stateCodes::INITIALIZED );
        REQUIRE( app.appLogic() == -1 );
        REQUIRE( dev.received() == "" );
    }

    SECTION( "no state reply leaves the state unchanged" )
    {
        app.state( stateCodes::CONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( dev.received() == "1TS\r\n" );
    }

    SECTION( "NOT REFERENCED gives NOTHOMED" )
    {
        app.state( stateCodes::CONNECTED );
        dev.reply( "1TS00000A\r\n" );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTHOMED );
        REQUIRE( dev.received() == "1TS\r\n" );
        REQUIRE( app.m_powerOnHomed == false );
    }

    SECTION( "NOT REFERENCED with powerOnHome starts homing once" )
    {
        app.state( stateCodes::CONNECTED );
        app.m_powerOnHome = true;
        dev.reply( "1TS00000A\r\n" );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTHOMED );
        REQUIRE( dev.received() == "1TS\r\n1OR\r\n" );
        REQUIRE( app.m_powerOnHomed == true );

        dev.reply( "1TS00000A\r\n" );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( dev.received() == "1TS\r\n" );
    }

    SECTION( "HOMING states set HOMING and the homing flags" )
    {
        app.state( stateCodes::CONNECTED );
        dev.reply( "1TS00001E\r\n" );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::HOMING );
        REQUIRE( app.m_moving == 1 );
        REQUIRE( app.m_wasHoming == true );
    }

    SECTION( "MOVING for a small move sets m_moving without OPERATING" )
    {
        app.state( stateCodes::CONNECTED );
        app.m_moveOp = false;
        dev.reply( "1TS000028\r\n" );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_moving == 1 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }

    SECTION( "READY after homing moves to the homing offset" )
    {
        app.state( stateCodes::CONNECTED );
        app.m_wasHoming    = true;
        app.m_homingOffset = 1.5;
        app.m_readTimeout  = 20;
        dev.reply( "1TS000032\r\n" );

        // The TE reply to the move is not queued, so the move fails, but the command is sent.
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_wasHoming == false );
        REQUIRE( dev.received() == "1TS\r\n1PA1.500000\r\n1TE\r\n" );
    }

    SECTION( "DISABLE states re-enable the stage" )
    {
        app.state( stateCodes::CONNECTED );
        dev.reply( "1TS00003C\r\n" );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( dev.received() == "1TS\r\n1MM1\r\n" );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }
}

/// Verify the power-off hook and telemetry recording.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl power off and telemetry", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::onPowerOff();
    smc100ccCtrl::recordPosition( true );
    smc100ccCtrl::recordStage( true );
    smc100ccCtrl::recordTelem( static_cast<const telem_stage *>( nullptr ) );
    smc100ccCtrl::recordTelem( static_cast<const telem_position *>( nullptr ) );
    smc100ccCtrl::checkRecordTimes();
    #endif
    // clang-format on

    smc100ccCtrl_test app( "smc" );

    SECTION( "onPowerOff clears the power-on homed flag and records telemetry" )
    {
        app.m_powerOnHomed         = true;
        telem_stage::lastRecord    = { 0, 0 };
        telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( app.m_powerOnHomed == false );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
        REQUIRE( telem_position::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordTelem forces a record of each type" )
    {
        telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const telem_position *>( nullptr ) ) == 0 );
        REQUIRE( telem_position::lastRecord.tv_sec > 0 );

        telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const telem_stage *>( nullptr ) ) == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordPosition only records a changed position unless forced" )
    {
        app.m_position = 4.25;
        REQUIRE( app.recordPosition( true ) == 0 );

        telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.recordPosition() == 0 );
        REQUIRE( telem_position::lastRecord.tv_sec == 0 );

        app.m_position = 4.5;
        REQUIRE( app.recordPosition() == 0 );
        REQUIRE( telem_position::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records stale telemetry" )
    {
        telem_stage::lastRecord    = { 0, 0 };
        telem_position::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
        REQUIRE( telem_position::lastRecord.tv_sec > 0 );
    }
}

} // namespace smc100ccCtrlTest

} // namespace libXWCTest
