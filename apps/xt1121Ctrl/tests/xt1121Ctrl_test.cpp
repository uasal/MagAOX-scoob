/** \file xt1121Ctrl_test.cpp
 * \brief Catch2 tests for the xt1121Ctrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup xt1121Ctrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "../xt1121Ctrl.hpp"

// Included after the app header so the callback bodies stay live (see tests/testMacrosINDI.hpp).
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup xt1121Ctrl_unit_test xt1121Ctrl Unit Tests
 * \brief Unit tests for the xt1121Ctrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `xt1121Ctrl` unit tests.
/** \ingroup xt1121Ctrl_unit_test
 */
namespace xt1121CtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing xt1121Ctrl internals.
class xt1121Ctrl_test : public xt1121Ctrl
{
  public:
    /// Construct a harness with the given device name, and name the channel properties as appStartup() does.
    explicit xt1121Ctrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch00, ch00 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch01, ch01 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch02, ch02 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch03, ch03 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch04, ch04 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch05, ch05 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch06, ch06 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch07, ch07 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch08, ch08 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch09, ch09 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch10, ch10 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch11, ch11 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch12, ch12 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch13, ch13 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch14, ch14 );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_ch15, ch15 );
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
    }

    /// The device address.
    using xt1121Ctrl::m_address;

    /// The device port.
    using xt1121Ctrl::m_port;

    /// The modbus object (always nullptr in these tests).
    using xt1121Ctrl::m_mb;

    /// The callback gate.
    using xt1121Ctrl::m_callbacksEnabled;

    /// The input-only channel flags.
    using xt1121Ctrl::m_inputOnly;

    /// The shutdown flag.
    using xt1121Ctrl::m_shutdown;

    /// The power state.
    using xt1121Ctrl::m_powerState;

    /// The power-on loop counter.
    using xt1121Ctrl::m_powerOnCounter;

    /// The power-on wait time.
    using xt1121Ctrl::m_powerOnWait;

    /// The registered new-property callbacks.
    using xt1121Ctrl::m_indiNewCallBacks;

    /// Get the channel property for a channel number.
    /**
     * \returns a reference to m_indiP_chXX, or to m_indiP_ch00 for an invalid channel
     */
    pcf::IndiProperty &chProp( int chNo /**< [in] the channel number, 0-15 */ )
    {
        switch( chNo )
        {
        case 1:
            return m_indiP_ch01;
        case 2:
            return m_indiP_ch02;
        case 3:
            return m_indiP_ch03;
        case 4:
            return m_indiP_ch04;
        case 5:
            return m_indiP_ch05;
        case 6:
            return m_indiP_ch06;
        case 7:
            return m_indiP_ch07;
        case 8:
            return m_indiP_ch08;
        case 9:
            return m_indiP_ch09;
        case 10:
            return m_indiP_ch10;
        case 11:
            return m_indiP_ch11;
        case 12:
            return m_indiP_ch12;
        case 13:
            return m_indiP_ch13;
        case 14:
            return m_indiP_ch14;
        case 15:
            return m_indiP_ch15;
        default:
            return m_indiP_ch00;
        }
    }

    /// Call the new callback for a channel number.
    /**
     * \returns the callback return value, or -99 for an invalid channel
     */
    int newCallBack( int                      chNo, /**< [in] the channel number, 0-15 */
                     const pcf::IndiProperty &ip /**< [in] the property to deliver */ )
    {
        switch( chNo )
        {
        case 0:
            return newCallBack_m_indiP_ch00( ip );
        case 1:
            return newCallBack_m_indiP_ch01( ip );
        case 2:
            return newCallBack_m_indiP_ch02( ip );
        case 3:
            return newCallBack_m_indiP_ch03( ip );
        case 4:
            return newCallBack_m_indiP_ch04( ip );
        case 5:
            return newCallBack_m_indiP_ch05( ip );
        case 6:
            return newCallBack_m_indiP_ch06( ip );
        case 7:
            return newCallBack_m_indiP_ch07( ip );
        case 8:
            return newCallBack_m_indiP_ch08( ip );
        case 9:
            return newCallBack_m_indiP_ch09( ip );
        case 10:
            return newCallBack_m_indiP_ch10( ip );
        case 11:
            return newCallBack_m_indiP_ch11( ip );
        case 12:
            return newCallBack_m_indiP_ch12( ip );
        case 13:
            return newCallBack_m_indiP_ch13( ip );
        case 14:
            return newCallBack_m_indiP_ch14( ip );
        case 15:
            return newCallBack_m_indiP_ch15( ip );
        default:
            return -99;
        }
    }
};

/// \endcond

/// Get the INDI name of a channel property.
/**
 * \returns chXX with XX the zero-padded channel number
 */
std::string chName( int chNo /**< [in] the channel number */ )
{
    return std::string( chNo < 10 ? "ch0" : "ch" ) + std::to_string( chNo );
}

/// Build a channel request property as a client would send it.
/**
 * \returns the property, with `target` and/or `current` elements added only when not empty
 */
pcf::IndiProperty
chRequest( const std::string &device, /**< [in] the device name */
           const std::string &name,   /**< [in] the property name */
           const std::string &target, /**< [in] the target value, not added if empty */
           const std::string &current = "" /**< [in] [optional] the current value, not added if empty */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    if( target != "" )
    {
        ip.add( pcf::IndiElement( "target", target ) );
    }
    if( current != "" )
    {
        ip.add( pcf::IndiElement( "current", current ) );
    }
    return ip;
}

/// Verify construction defaults.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl construction", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::xt1121Ctrl();
    #endif
    // clang-format on

    xt1121Ctrl_test app( "xt" );

    REQUIRE( app.m_port == 502 );
    REQUIRE( app.m_address == "" );
    REQUIRE( app.m_mb == nullptr );
    REQUIRE( app.m_callbacksEnabled.load() == true );
    REQUIRE( app.m_powerOnWait == 2 );
    REQUIRE( xt1121Ctrl::numChannels == 16 );
    REQUIRE( xt1121Ctrl::numRegisters == 4 );

    for( int n = 0; n < 16; ++n )
    {
        REQUIRE( app.channel( n ) == 0 );
        REQUIRE( app.m_inputOnly[n] == false );
    }
}

/// Verify configuration defaults and overrides, including input-only channels.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl configuration", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::setupConfig();
    xt1121Ctrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        std::string file = "/tmp/xt1121Ctrl_test_defaults.conf";
        mx::app::writeConfigFile( file, { "none" }, { "nada" }, { "0" } );

        xt1121Ctrl_test app( "xt" );
        app.configure( file );

        REQUIRE( app.m_address == "" );
        REQUIRE( app.m_port == 502 );
        for( int n = 0; n < 16; ++n )
        {
            REQUIRE( app.m_inputOnly[n] == false );
        }

        remove( file.c_str() );
    }

    SECTION( "overrides" )
    {
        std::string file = "/tmp/xt1121Ctrl_test_overrides.conf";
        mx::app::writeConfigFile( file,
                                  { "device", "device", "device" },
                                  { "address", "port", "inputOnly" },
                                  { "192.168.0.10", "5020", "0,5,15" } );

        xt1121Ctrl_test app( "xt" );
        app.configure( file );

        REQUIRE( app.m_address == "192.168.0.10" );
        REQUIRE( app.m_port == 5020 );
        for( int n = 0; n < 16; ++n )
        {
            REQUIRE( app.m_inputOnly[n] == ( n == 0 || n == 5 || n == 15 ) );
        }

        remove( file.c_str() );
    }

    SECTION( "out of range input-only channels are skipped" )
    {
        std::string file = "/tmp/xt1121Ctrl_test_badinput.conf";
        mx::app::writeConfigFile( file, { "device" }, { "inputOnly" }, { "3,16,-1" } );

        xt1121Ctrl_test app( "xt" );
        app.configure( file );

        for( int n = 0; n < 16; ++n )
        {
            REQUIRE( app.m_inputOnly[n] == ( n == 3 ) );
        }

        remove( file.c_str() );
    }
}

/// Verify appStartup creates and registers the 16 channel properties.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl appStartup", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::appStartup();
    #endif
    // clang-format on

    xt1121Ctrl_test app( "xt" );

    REQUIRE( app.appStartup() == 0 );
    REQUIRE( app.m_indiNewCallBacks.size() == 16 );

    for( int n = 0; n < 16; ++n )
    {
        pcf::IndiProperty &ip = app.chProp( n );
        REQUIRE( ip.getDevice() == "xt" );
        REQUIRE( ip.getName() == chName( n ) );
        REQUIRE( ip.getType() == pcf::IndiProperty::Number );
        REQUIRE( ip.find( "current" ) );
        REQUIRE( ip.find( "target" ) );
        REQUIRE( ip["current"].get<int>() == -1 );
        REQUIRE( app.m_indiNewCallBacks.count( "xt." + chName( n ) ) == 1 );
    }

    // a second startup would register duplicates
    REQUIRE( app.appStartup() == -1 );
}

/// Verify the channel new callbacks reject mismatched properties (callback bodies live).
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl INDI new callback validation", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::newCallBack_m_indiP_ch00(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch01(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch02(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch03(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch04(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch05(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch06(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch07(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch08(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch09(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch10(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch11(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch12(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch13(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch14(pcf::IndiProperty());
    xt1121Ctrl::newCallBack_m_indiP_ch15(pcf::IndiProperty());
    #endif
    // clang-format on

    // With live bodies, a matching property with no elements makes channelSetCallback return 0.
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch00 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch01 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch02 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch03 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch04 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch05 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch06 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch07 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch08 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch09 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch10 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch11 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch12 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch13 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch14 );
    XWCTEST_INDI_NEW_CALLBACK( xt1121Ctrl, ch15 );
}

/// Verify the new callbacks set and clear the matching channel.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl channel new callbacks", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::newCallBack_m_indiP_ch00(pcf::IndiProperty());
    xt1121Ctrl::channelSetCallback(0, m_indiP_ch00, pcf::IndiProperty());
    #endif
    // clang-format on

    xt1121Ctrl_test app( "xt" );
    REQUIRE( app.appStartup() == 0 );

    SECTION( "each callback sets only its own channel" )
    {
        for( int n = 0; n < 16; ++n )
        {
            REQUIRE( app.newCallBack( n, chRequest( "xt", chName( n ), "1" ) ) == 0 );
            for( int k = 0; k < 16; ++k )
            {
                REQUIRE( app.channel( k ) == ( k <= n ? 1 : 0 ) );
            }
        }

        for( int n = 0; n < 16; ++n )
        {
            REQUIRE( app.newCallBack( n, chRequest( "xt", chName( n ), "0" ) ) == 0 );
            for( int k = 0; k < 16; ++k )
            {
                REQUIRE( app.channel( k ) == ( k <= n ? 0 : 1 ) );
            }
        }
    }

    SECTION( "a request for another channel's property is rejected" )
    {
        REQUIRE( app.newCallBack( 3, chRequest( "xt", "ch04", "1" ) ) == -1 );
        REQUIRE( app.newCallBack( 3, chRequest( "other", "ch03", "1" ) ) == -1 );
        REQUIRE( app.channel( 3 ) == 0 );
        REQUIRE( app.channel( 4 ) == 0 );
    }
}

/// Verify the target/current logic of channelSetCallback.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl channelSetCallback", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::channelSetCallback(0, m_indiP_ch00, pcf::IndiProperty());
    #endif
    // clang-format on

    xt1121Ctrl_test app( "xt" );
    REQUIRE( app.appStartup() == 0 );

    pcf::IndiProperty &ip = app.chProp( 2 );

    SECTION( "target sets and clears" )
    {
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "1" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 1 );

        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "0" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 0 );
    }

    SECTION( "any non-zero target sets" )
    {
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "5" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 1 );
    }

    SECTION( "current is used when there is no target" )
    {
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "", "1" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 1 );

        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "", "0" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 0 );
    }

    SECTION( "target takes precedence over current" )
    {
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "1", "0" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 1 );

        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "0", "1" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 0 );
    }

    SECTION( "no target or current does nothing" )
    {
        app.setChannel( 2 );
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 1 );
    }

    SECTION( "input-only channels can not be set" )
    {
        REQUIRE( app.setInputOnly( 2 ) == 0 );
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "1" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 0 );
    }

    SECTION( "disabled callbacks do nothing" )
    {
        app.m_callbacksEnabled = false;
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "1" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 0 );
    }

    SECTION( "callbacks do nothing during shutdown" )
    {
        app.m_shutdown = 1;
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "1" ) ) == 0 );
        REQUIRE( app.channel( 2 ) == 0 );
    }

    SECTION( "the INDI mutex is released" )
    {
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "1" ) ) == 0 );
        std::unique_lock<std::mutex> lock( app.m_indiMutex, std::try_to_lock );
        REQUIRE( lock.owns_lock() );
    }

    SECTION( "the resulting register bits match the channel" )
    {
        REQUIRE( app.channelSetCallback( 2, ip, chRequest( "xt", "ch02", "1" ) ) == 0 );
        REQUIRE( app.channelSetCallback( 9, app.chProp( 9 ), chRequest( "xt", "ch09", "1" ) ) == 0 );

        uint16_t regs[xt1121Ctrl::numRegisters];
        REQUIRE( app.setRegisters( regs ) == 0 );
        REQUIRE( regs[0] == 4 );
        REQUIRE( regs[1] == 0 );
        REQUIRE( regs[2] == 2 );
        REQUIRE( regs[3] == 0 );
    }
}

/// Verify getState without a modbus connection.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl getState", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::getState();
    #endif
    // clang-format on

    xt1121Ctrl_test app( "xt" );
    REQUIRE( app.appStartup() == 0 );

    SECTION( "no modbus connection is an error" )
    {
        REQUIRE( app.getState() == -1 );
    }

    SECTION( "during shutdown nothing is read" )
    {
        app.m_shutdown = 1;
        REQUIRE( app.getState() == 0 );
    }
}

/// Verify the appLogic state machine paths that do not open a modbus connection.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl appLogic", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::appLogic();
    #endif
    // clang-format on

    xt1121Ctrl_test app( "xt" );
    REQUIRE( app.appStartup() == 0 );

    SECTION( "POWERON waits for the power-on delay, then goes to NOTCONNECTED" )
    {
        app.m_powerState     = 0; // keeps NOTCONNECTED from trying to connect
        app.m_powerOnCounter = 0;
        app.state( stateCodes::POWERON );

        // m_powerOnWait is 2 s and the default loop pause is 1 s
        for( int n = 0; n < 3; ++n )
        {
            REQUIRE( app.appLogic() == 0 );
            REQUIRE( app.state() == stateCodes::POWERON );
        }

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "POWERON without a power-on counter goes straight to NOTCONNECTED" )
    {
        app.m_powerState     = 0;
        app.m_powerOnCounter = -1;
        app.state( stateCodes::POWERON );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "NOTCONNECTED and ERROR do nothing while powered off" )
    {
        app.m_powerState = 0;

        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );

        app.state( stateCodes::ERROR );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_mb == nullptr );
    }

    SECTION( "CONNECTED with a failed read goes to ERROR" )
    {
        app.m_powerState = 1;
        app.state( stateCodes::CONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "READY or OPERATING with a failed read goes to ERROR when powered" )
    {
        app.m_powerState = 1;

        app.state( stateCodes::READY );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );

        app.state( stateCodes::OPERATING );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "READY with a failed read stays READY when powered off" )
    {
        app.m_powerState = 0;
        app.state( stateCodes::READY );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
    }
}

/// Verify the power-off hooks and appShutdown.
/**
 * \ingroup xt1121Ctrl_unit_test
 */
TEST_CASE( "xt1121Ctrl power off and shutdown", "[xt1121Ctrl]" )
{
    // clang-format off
    #ifdef XT1121CTRL_TEST_DOXYGEN_REF
    xt1121Ctrl::onPowerOff();
    xt1121Ctrl::whilePowerOff();
    xt1121Ctrl::appShutdown();
    #endif
    // clang-format on

    xt1121Ctrl_test app( "xt" );
    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.onPowerOff() == 0 );
    { // mutex scope
        std::unique_lock<std::mutex> lock( app.m_indiMutex, std::try_to_lock );
        REQUIRE( lock.owns_lock() );
    }

    REQUIRE( app.whilePowerOff() == 0 );

    REQUIRE( app.appShutdown() == 0 );
    REQUIRE( app.m_callbacksEnabled.load() == false );
    REQUIRE( app.m_mb == nullptr );

    // callbacks are disabled after shutdown
    REQUIRE( app.newCallBack( 1, chRequest( "xt", "ch01", "1" ) ) == 0 );
    REQUIRE( app.channel( 1 ) == 0 );
}

} // namespace xt1121CtrlTest

} // namespace libXWCTest
