/** \file koolanceCtrl_test.cpp
 * \brief Catch2 tests for the koolanceCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup koolanceCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../koolanceCtrl.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup koolanceCtrl_unit_test koolanceCtrl Unit Tests
 * \brief Unit tests for the koolanceCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `koolanceCtrl` unit tests.
/** \ingroup koolanceCtrl_unit_test
 */
namespace koolanceCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Directory the test harness points the telemetry logger at.
const char *telemPath = "/tmp/koolanceCtrl_test_telem";

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

/// Test harness exposing koolanceCtrl internals.
class koolanceCtrl_test : public koolanceCtrl
{
  public:
    /// Construct a harness with the given device name, and name the level properties as initialConnect() does.
    explicit koolanceCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        static_cast<void>( m_tel.logPath( telemPath ) );
        m_indiP_pumplvl.setDevice( device );
        m_indiP_pumplvl.setName( "pump_level" );
        m_indiP_fanlvl.setDevice( device );
        m_indiP_fanlvl.setName( "fan_level" );
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
        static_cast<void>( m_tel.logPath( telemPath ) );
    }

    /// Get the protocol response length.
    size_t protocolChars() const
    {
        return m_protocolChars;
    }

    /// Set the protocol response length.
    void protocolChars( size_t pc /**< [in] the new protocol response length */ )
    {
        m_protocolChars = pc;
    }

    /// Get whether INDI has been set up.
    bool indiSetup() const
    {
        return m_indiSetup;
    }

    /// Get the liquid temperature.
    float liqTemp() const
    {
        return m_liqTemp;
    }

    /// Get the flow rate.
    float flowRate() const
    {
        return m_flowRate;
    }

    /// Get the pump level.
    int pumpLvl() const
    {
        return m_pumpLvl;
    }

    /// Set the pump level.
    void pumpLvl( int l /**< [in] the new pump level */ )
    {
        m_pumpLvl = l;
    }

    /// Get the pump RPM.
    int pumpRPM() const
    {
        return m_pumpRPM;
    }

    /// Get the fan RPM.
    int fanRPM() const
    {
        return m_fanRPM;
    }

    /// Get the fan level.
    int fanLvl() const
    {
        return m_fanLvl;
    }

    /// Set the fan level.
    void fanLvl( int l /**< [in] the new fan level */ )
    {
        m_fanLvl = l;
    }

    /// Set the telemetry values directly.
    void setValues( float liq,  /**< [in] liquid temperature */
                    float flow, /**< [in] flow rate */
                    int   pl,   /**< [in] pump level */
                    int   prpm, /**< [in] pump RPM */
                    int   frpm, /**< [in] fan RPM */
                    int   fl /**< [in] fan level */ )
    {
        m_liqTemp  = liq;
        m_flowRate = flow;
        m_pumpLvl  = pl;
        m_pumpRPM  = prpm;
        m_fanRPM   = frpm;
        m_fanLvl   = fl;
    }

    /// Access the pump level property.
    pcf::IndiProperty &pumplvlProp()
    {
        return m_indiP_pumplvl;
    }

    /// Access the fan level property.
    pcf::IndiProperty &fanlvlProp()
    {
        return m_indiP_fanlvl;
    }

    /// Whether a new-property registration exists for key, and whether it has a callback.
    /** \returns -1 if not registered, 0 if registered with no callback, 1 if registered with a callback.
     */
    int newCallBackRegistered( const std::string &key /**< [in] the device.name key */ )
    {
        auto it = m_indiNewCallBacks.find( key );
        if( it == m_indiNewCallBacks.end() )
        {
            return -1;
        }
        return ( it->second.callBack != nullptr ) ? 1 : 0;
    }

    /// Get the shutdown flag.
    int shutdown() const
    {
        return m_shutdown;
    }
};

/// A fake serial device on the far end of a socket pair.
class fakeTty
{
  public:
    /// Create the socket pair.
    fakeTty()
    {
        m_ok = ( socketpair( AF_UNIX, SOCK_STREAM, 0, m_fds ) == 0 );
    }

    /// Close the sockets that are still open.
    ~fakeTty()
    {
        if( m_ok )
        {
            if( m_appOpen )
            {
                close( m_fds[0] );
            }
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

    /// Record that the app closed its end, so it is not closed again.
    void releaseAppFd()
    {
        m_appOpen = false;
    }

    /// Queue bytes for the app to read.
    bool send( const std::vector<unsigned char> &bytes /**< [in] the bytes to queue */ )
    {
        return write( m_fds[1], bytes.data(), bytes.size() ) == static_cast<ssize_t>( bytes.size() );
    }

    /// Read up to nBytes of what the app sent, waiting at most timeoutMs for each chunk.
    std::vector<unsigned char> readBytes( size_t nBytes, /**< [in] the maximum number of bytes to read */
                                          int    timeoutMs /**< [in] the per-chunk poll timeout in msec */ )
    {
        std::vector<unsigned char> out;
        while( out.size() < nBytes )
        {
            pollfd pfd;
            pfd.fd      = m_fds[1];
            pfd.events  = POLLIN;
            pfd.revents = 0;
            if( poll( &pfd, 1, timeoutMs ) <= 0 )
            {
                break;
            }

            unsigned char buf[128];
            ssize_t       rv = read( m_fds[1], buf, std::min( sizeof( buf ), nBytes - out.size() ) );
            if( rv <= 0 )
            {
                break;
            }
            out.insert( out.end(), buf, buf + rv );
        }
        return out;
    }

  private:
    int  m_fds[2]{ -1, -1 }; ///< The socket pair: [0] is the app end, [1] is the fake device end.
    bool m_ok{ false };      ///< True if the socket pair was created.
    bool m_appOpen{ true };  ///< False once the app has closed its end.
};

/// The 3-byte status request command.
std::vector<unsigned char> statusCmd()
{
    return { 0xCF, 0x01, 0x08 };
}

/// Build a status response of the given length with known field values.
/** Liquid temperature 25.3 C, flow 4.2, fan 1200 RPM, pump 2700 RPM, fan level 55, pump level 7.
 */
std::vector<unsigned char> statusResponse( size_t len /**< [in] the response length (43 or 51) */ )
{
    std::vector<unsigned char> resp( len, 0 );
    resp[0]  = 0xCF;
    resp[2]  = 0x08; // 2253 = 0x08CD -> (2253-2000)/10 = 25.3
    resp[3]  = 0xCD;
    resp[8]  = 0x04; // 1200 = 0x04B0
    resp[9]  = 0xB0;
    resp[10] = 0x0A; // 2700 = 0x0A8C
    resp[11] = 0x8C;
    resp[12] = 0x00; // 42 -> 4.2
    resp[13] = 0x2A;
    resp[15] = 55;
    resp[17] = 7;
    return resp;
}

/// Build the expected protocol 2 (51 byte) set command.
std::vector<unsigned char> expectedSetCmd( int fanLvl, /**< [in] the fan level in the command */
                                           int pumpLvl /**< [in] the pump level in the command */ )
{
    std::vector<unsigned char> com( 51, 0 );
    com[0]  = 0xCF;
    com[1]  = 0x04;
    com[15] = fanLvl;
    com[17] = pumpLvl;
    for( size_t n = 20; n < 50; ++n )
    {
        com[n] = 0xAA;
    }
    com[44] = 0;
    com[45] = 1;
    com[46] = 0;
    com[47] = 0;
    com[48] = 0;
    com[49] = 0;

    int checksum = 0;
    for( size_t n = 0; n < 50; ++n )
    {
        checksum += com[n];
    }
    com[50] = checksum % 100;
    return com;
}

/// \endcond

/// Verify koolanceCtrl construction defaults.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl construction defaults", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );

    REQUIRE( app.protocolChars() == 0 );
    REQUIRE( app.indiSetup() == false );
    REQUIRE( app.liqTemp() == 0 );
    REQUIRE( app.flowRate() == 0 );
    REQUIRE( app.pumpLvl() == 0 );
    REQUIRE( app.pumpRPM() == 0 );
    REQUIRE( app.fanRPM() == 0 );
    REQUIRE( app.fanLvl() == 0 );
}

/// Verify koolanceCtrl configuration defaults and overrides.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl configuration", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::setupConfig();
    koolanceCtrl::loadConfig();
    koolanceCtrl::loadConfigImpl(config);
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        koolanceCtrl_test app( "kool" );

        mx::app::writeConfigFile( "/tmp/koolanceCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
        app.configure( "/tmp/koolanceCtrl_test_defaults.conf" );
        std::remove( "/tmp/koolanceCtrl_test_defaults.conf" );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_baudRate == B9600 );
        REQUIRE( app.m_idVendor == "" );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
    }

    SECTION( "overrides" )
    {
        koolanceCtrl_test app( "kool" );

        mx::app::writeConfigFile( "/tmp/koolanceCtrl_test_override.conf",
                                  { "usb", "usb", "usb", "usb", "telemeter" },
                                  { "idVendor", "idProduct", "serial", "baud", "maxInterval" },
                                  { "fffe", "fffd", "KOOL1", "19200", "3.5" } );
        app.configure( "/tmp/koolanceCtrl_test_override.conf" );
        std::remove( "/tmp/koolanceCtrl_test_override.conf" );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_idVendor == "fffe" );
        REQUIRE( app.m_idProduct == "fffd" );
        REQUIRE( app.m_serial == "KOOL1" );
        REQUIRE( app.m_baudRate == B19200 );
        REQUIRE( app.m_maxInterval == Approx( 3.5 ) );
    }
}

/// Verify initialConnect() determines the protocol from the response length and sets up INDI.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl initialConnect protocol detection", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::initialConnect();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    fakeTty           dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    SECTION( "43 bytes is protocol 1, read-only levels" )
    {
        REQUIRE( dev.send( statusResponse( 43 ) ) );
        REQUIRE( app.initialConnect() == 0 );
        REQUIRE( dev.readBytes( 3, 500 ) == statusCmd() );

        REQUIRE( app.protocolChars() == 43 );
        REQUIRE( app.indiSetup() == true );
        REQUIRE( app.pumplvlProp().getName() == "pump_level" );
        REQUIRE( app.pumplvlProp().getPerm() == pcf::IndiProperty::ReadOnly );
        REQUIRE( app.pumplvlProp().find( "current" ) );
        REQUIRE_FALSE( app.pumplvlProp().find( "target" ) );
        REQUIRE( app.fanlvlProp().getName() == "fan_level" );
        REQUIRE( app.fanlvlProp().find( "current" ) );
        REQUIRE( app.newCallBackRegistered( "kool.pump_level" ) == 0 );
        REQUIRE( app.newCallBackRegistered( "kool.fan_level" ) == 0 );
    }

    SECTION( "51 bytes is protocol 2, settable levels" )
    {
        REQUIRE( dev.send( statusResponse( 51 ) ) );
        REQUIRE( app.initialConnect() == 0 );
        REQUIRE( dev.readBytes( 3, 500 ) == statusCmd() );

        REQUIRE( app.protocolChars() == 51 );
        REQUIRE( app.indiSetup() == true );
        REQUIRE( app.pumplvlProp().getPerm() == pcf::IndiProperty::ReadWrite );
        REQUIRE( app.pumplvlProp().find( "current" ) );
        REQUIRE( app.pumplvlProp().find( "target" ) );
        REQUIRE( app.fanlvlProp().find( "target" ) );
        REQUIRE( app.newCallBackRegistered( "kool.pump_level" ) == 1 );
        REQUIRE( app.newCallBackRegistered( "kool.fan_level" ) == 1 );
    }

    SECTION( "any other length is an error" )
    {
        REQUIRE( dev.send( std::vector<unsigned char>( 20, 0 ) ) );
        REQUIRE( app.initialConnect() == -1 );
        REQUIRE( app.protocolChars() == 0 );
        REQUIRE( app.indiSetup() == false );
        REQUIRE( app.m_fileDescrip == dev.appFd() );
    }

    SECTION( "no response closes the device" )
    {
        app.state( stateCodes::CONNECTED );
        REQUIRE( app.initialConnect() == -1 );
        dev.releaseAppFd();
        REQUIRE( app.m_fileDescrip == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    if( app.m_fileDescrip == dev.appFd() )
    {
        app.m_fileDescrip = 0;
    }
}

/// Verify getStatus() decodes the status response for both protocols.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl getStatus decodes the response", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::getStatus();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    fakeTty           dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    size_t len = GENERATE( 43, 51 );
    app.protocolChars( len );

    REQUIRE( dev.send( statusResponse( len ) ) );
    REQUIRE( app.getStatus() == 0 );
    REQUIRE( dev.readBytes( 3, 500 ) == statusCmd() );

    REQUIRE( app.liqTemp() == Approx( 25.3 ) );
    REQUIRE( app.flowRate() == Approx( 4.2 ) );
    REQUIRE( app.fanRPM() == 1200 );
    REQUIRE( app.pumpRPM() == 2700 );
    REQUIRE( app.fanLvl() == 55 );
    REQUIRE( app.pumpLvl() == 7 );
    REQUIRE( app.m_fileDescrip == dev.appFd() );

    app.m_fileDescrip = 0;
}

/// Verify getStatus() decodes a liquid temperature below the 200 raw offset as negative.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl getStatus temperature offset", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::getStatus();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    fakeTty           dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    app.protocolChars( 51 );

    std::vector<unsigned char> resp = statusResponse( 51 );
    resp[2]                         = 0x07; // 1950 = 0x079E -> -5.0
    resp[3]                         = 0x9E;

    REQUIRE( dev.send( resp ) );
    REQUIRE( app.getStatus() == 0 );
    REQUIRE( app.liqTemp() == Approx( -5.0 ) );

    app.m_fileDescrip = 0;
}

/// Verify getStatus() error handling closes the device and returns to NOTCONNECTED.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl getStatus errors", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::getStatus();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    app.protocolChars( 51 );
    app.state( stateCodes::READY );

    SECTION( "write error" )
    {
        app.m_fileDescrip = -1;
        REQUIRE( app.getStatus() == -1 );
        REQUIRE( app.m_fileDescrip == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "wrong response size" )
    {
        fakeTty dev;
        REQUIRE( dev.ok() );
        app.m_fileDescrip = dev.appFd();

        REQUIRE( dev.send( std::vector<unsigned char>( 60, 0 ) ) );
        REQUIRE( app.getStatus() == -1 );
        dev.releaseAppFd();
        REQUIRE( app.m_fileDescrip == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "no response" )
    {
        fakeTty dev;
        REQUIRE( dev.ok() );
        app.m_fileDescrip = dev.appFd();

        REQUIRE( app.getStatus() == -1 );
        dev.releaseAppFd();
        REQUIRE( app.m_fileDescrip == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }
}

/// Verify setPumpLvl() and setFanLvl() build the protocol 2 command with checksum.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl set level commands", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::setPumpLvl(1);
    koolanceCtrl::setFanLvl(1);
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    fakeTty           dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    app.fanLvl( 40 );
    app.pumpLvl( 6 );

    SECTION( "protocol 2 pump level" )
    {
        app.protocolChars( 51 );
        REQUIRE( app.setPumpLvl( 9 ) == 0 );
        REQUIRE( dev.readBytes( 51, 500 ) == expectedSetCmd( 40, 9 ) );
    }

    SECTION( "protocol 2 fan level" )
    {
        app.protocolChars( 51 );
        REQUIRE( app.setFanLvl( 75 ) == 0 );
        REQUIRE( dev.readBytes( 51, 500 ) == expectedSetCmd( 75, 6 ) );
    }

    SECTION( "protocol 1 is read-only" )
    {
        app.protocolChars( 43 );
        REQUIRE( app.setPumpLvl( 9 ) == 0 );
        REQUIRE( app.setFanLvl( 75 ) == 0 );
        REQUIRE( dev.readBytes( 51, 100 ).empty() );
    }

    SECTION( "write errors close the device" )
    {
        app.protocolChars( 51 );
        app.state( stateCodes::READY );
        app.m_fileDescrip = -1;
        REQUIRE( app.setPumpLvl( 9 ) == -1 );
        REQUIRE( app.m_fileDescrip == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );

        app.m_fileDescrip = -1;
        REQUIRE( app.setFanLvl( 75 ) == -1 );
        REQUIRE( app.m_fileDescrip == 0 );
    }

    app.m_fileDescrip = 0;
}

/// Verify the pump level new-callback validates the name and range and sends the command.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl pump level new callback", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::newCallBack_m_indiP_pumplvl(pcf::IndiProperty());
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    fakeTty           dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    app.protocolChars( 51 );
    app.fanLvl( 30 );

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "kool" );
    ip.setName( "pump_level" );

    SECTION( "wrong name is rejected" )
    {
        ip.setName( "wrong" );
        ip.add( pcf::IndiElement( "target", 5 ) );
        REQUIRE( app.newCallBack_m_indiP_pumplvl( ip ) == -1 );
        REQUIRE( dev.readBytes( 51, 100 ).empty() );
    }

    SECTION( "no elements is out of range and ignored" )
    {
        REQUIRE( app.newCallBack_m_indiP_pumplvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 100 ).empty() );
    }

    SECTION( "out of range levels are ignored" )
    {
        int lvl = GENERATE( 0, 11, -3 );
        ip.add( pcf::IndiElement( "target", lvl ) );
        REQUIRE( app.newCallBack_m_indiP_pumplvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 100 ).empty() );
    }

    SECTION( "valid target sends the command" )
    {
        int lvl = GENERATE( 1, 5, 10 );
        ip.add( pcf::IndiElement( "target", lvl ) );
        REQUIRE( app.newCallBack_m_indiP_pumplvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 500 ) == expectedSetCmd( 30, lvl ) );
    }

    SECTION( "current is used when there is no target" )
    {
        ip.add( pcf::IndiElement( "current", 4 ) );
        REQUIRE( app.newCallBack_m_indiP_pumplvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 500 ) == expectedSetCmd( 30, 4 ) );
    }

    SECTION( "target takes precedence over current" )
    {
        ip.add( pcf::IndiElement( "current", 4 ) );
        ip.add( pcf::IndiElement( "target", 8 ) );
        REQUIRE( app.newCallBack_m_indiP_pumplvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 500 ) == expectedSetCmd( 30, 8 ) );
    }

    app.m_fileDescrip = 0;
}

/// Verify the fan level new-callback validates the name and range and sends the command.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl fan level new callback", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::newCallBack_m_indiP_fanlvl(pcf::IndiProperty());
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    fakeTty           dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();
    app.protocolChars( 51 );
    app.pumpLvl( 3 );

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "kool" );
    ip.setName( "fan_level" );

    SECTION( "wrong name is rejected" )
    {
        ip.setName( "pump_level" );
        ip.add( pcf::IndiElement( "target", 50 ) );
        REQUIRE( app.newCallBack_m_indiP_fanlvl( ip ) == -1 );
        REQUIRE( dev.readBytes( 51, 100 ).empty() );
    }

    SECTION( "out of range levels are ignored" )
    {
        int lvl = GENERATE( -1, 101 );
        ip.add( pcf::IndiElement( "target", lvl ) );
        REQUIRE( app.newCallBack_m_indiP_fanlvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 100 ).empty() );
    }

    SECTION( "valid target sends the command" )
    {
        int lvl = GENERATE( 0, 50, 100 );
        ip.add( pcf::IndiElement( "target", lvl ) );
        REQUIRE( app.newCallBack_m_indiP_fanlvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 500 ) == expectedSetCmd( lvl, 3 ) );
    }

    SECTION( "protocol 1 accepts but sends nothing" )
    {
        app.protocolChars( 43 );
        ip.add( pcf::IndiElement( "target", 50 ) );
        REQUIRE( app.newCallBack_m_indiP_fanlvl( ip ) == 0 );
        REQUIRE( dev.readBytes( 51, 100 ).empty() );
    }

    app.m_fileDescrip = 0;
}

/// Verify recordCooler() records only on change or when forced, and the telemeter interval hooks.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl telemetry recording", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::recordCooler(true);
    koolanceCtrl::recordTelem(nullptr);
    koolanceCtrl::checkRecordTimes();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    app.setValues( 20.0, 3.0, 5, 2000, 1000, 50 );

    SECTION( "recordCooler change detection" )
    {
        MagAOX::logger::telem_cooler::lastRecord = { 0, 0 };
        REQUIRE( app.recordCooler( true ) == 0 );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_sec != 0 );

        MagAOX::logger::telem_cooler::lastRecord = { 0, 0 };
        REQUIRE( app.recordCooler( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_sec == 0 );

        app.setValues( 20.5, 3.0, 5, 2000, 1000, 50 );
        REQUIRE( app.recordCooler( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_sec != 0 );

        MagAOX::logger::telem_cooler::lastRecord = { 0, 0 };
        app.setValues( 20.5, 3.0, 5, 2000, 1000, 51 );
        REQUIRE( app.recordCooler( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_sec != 0 );
    }

    SECTION( "recordTelem forces a record" )
    {
        REQUIRE( app.recordCooler( true ) == 0 );
        MagAOX::logger::telem_cooler::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_sec != 0 );
    }

    SECTION( "checkRecordTimes records only when stale" )
    {
        MagAOX::logger::telem_cooler::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_sec != 0 );

        timespec fresh = MagAOX::logger::telem_cooler::lastRecord;
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_sec == fresh.tv_sec );
        REQUIRE( MagAOX::logger::telem_cooler::lastRecord.tv_nsec == fresh.tv_nsec );
    }
}

/// Verify appStartup() refuses to start from UNINITIALIZED.
/**
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl appStartup requires initialization", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::appStartup();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );
    REQUIRE( app.state() == stateCodes::UNINITIALIZED );
    REQUIRE( app.appStartup() == -1 );
}

/// Verify the appLogic() state machine without hardware.
/**
 * The telemetry thread is not running in the test, so `telemeter::appLogic()` sets FAILURE at the end of the READY
 * branch.
 *
 * \ingroup koolanceCtrl_unit_test
 */
TEST_CASE( "koolanceCtrl appLogic", "[koolanceCtrl]" )
{
    // clang-format off
    #ifdef KOOLANCECTRL_TEST_DOXYGEN_REF
    koolanceCtrl::appLogic();
    koolanceCtrl::appShutdown();
    #endif
    // clang-format on

    koolanceCtrl_test app( "kool" );

    // IDs which match no real device.
    app.m_idVendor  = "fffe";
    app.m_idProduct = "fffd";
    app.m_serial    = "koolanceCtrl_test";

    SECTION( "NODEVICE stays NODEVICE when the device is not found" )
    {
        app.state( stateCodes::NODEVICE );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
    }

    SECTION( "NOTCONNECTED with a failed connect goes back to NODEVICE" )
    {
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
    }

    SECTION( "READY reads the status" )
    {
        fakeTty dev;
        REQUIRE( dev.ok() );
        app.m_fileDescrip = dev.appFd();
        app.protocolChars( 51 );
        app.state( stateCodes::READY );

        REQUIRE( dev.send( statusResponse( 51 ) ) );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( dev.readBytes( 3, 500 ) == statusCmd() );
        REQUIRE( app.liqTemp() == Approx( 25.3 ) );
        REQUIRE( app.pumpLvl() == 7 );
        REQUIRE( app.shutdown() == 1 );

        app.m_fileDescrip = 0;
    }

    REQUIRE( app.appShutdown() == 0 );
}

} // namespace koolanceCtrlTest

} // namespace libXWCTest
