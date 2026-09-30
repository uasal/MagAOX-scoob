/** \file acronameUsbHub_test.cpp
 * \brief Catch2 tests for the acronameUsbHub app.
 * \author Claude Code
 *
 * \ingroup acronameUsbHub_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "../acronameUsbHub.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Fake hub state driven by the BrainStem2 stub functions.
struct brainStemStubState
{
    /// Reset to a disconnected hub with all ports off and no errors.
    void reset()
    {
        connectErr          = aErrNone;
        connectCalls        = 0;
        lastLinkType        = INVALID;
        lastSerial          = 0;
        connected           = false;
        disconnectCalls     = 0;
        getPortStateErr     = aErrNone;
        getPortStateErrPort = -1;
        getPortStateCalls   = 0;
        portStateChannels.clear();
        setPortEnableErr  = aErrNone;
        setPortDisableErr = aErrNone;
        enabledPorts.clear();
        disabledPorts.clear();
        for( size_t n = 0; n < aUSBHUB3P_NUM_USB_PORTS; ++n )
        {
            portState[n] = 0;
        }
        sysInitCalls   = 0;
        sysModule      = nullptr;
        model          = aMODULE_TYPE_USBHub3p;
        version        = 0x02070003;
        serial         = 0;
        modelNameCalls = 0;
        versionCalls   = 0;
    }

    aErr     connectErr{ aErrNone };  ///< Return code of Module::connect.
    int      connectCalls{ 0 };       ///< Number of Module::connect calls.
    linkType lastLinkType{ INVALID }; ///< The transport passed to the last Module::connect.
    uint32_t lastSerial{ 0 };         ///< The serial number passed to the last Module::connect.
    bool     connected{ false };      ///< Value returned by Module::isConnected, set by a successful connect.
    int      disconnectCalls{ 0 };    ///< Number of Module::disconnect calls.

    aErr getPortStateErr{ aErrNone }; ///< Return code of USBClass::getPortState.
    int  getPortStateErrPort{ -1 };   ///< If >= 0, getPortStateErr is returned only for this port.
    int  getPortStateCalls{ 0 };      ///< Number of USBClass::getPortState calls.

    std::vector<int> portStateChannels; ///< Ports passed to USBClass::getPortState, in order.

    aErr setPortEnableErr{ aErrNone };  ///< Return code of USBClass::setPortEnable.
    aErr setPortDisableErr{ aErrNone }; ///< Return code of USBClass::setPortDisable.

    std::vector<int> enabledPorts;  ///< Ports passed to USBClass::setPortEnable, in order.
    std::vector<int> disabledPorts; ///< Ports passed to USBClass::setPortDisable, in order.

    uint32_t portState[aUSBHUB3P_NUM_USB_PORTS]{}; ///< Port state bit fields; bit 0 is set/cleared by enable/disable.

    int      sysInitCalls{ 0 };              ///< Number of SystemClass::init calls.
    Module  *sysModule{ nullptr };           ///< The module passed to the last SystemClass::init.
    uint8_t  model{ aMODULE_TYPE_USBHub3p }; ///< Model reported by SystemClass::getModel.
    uint32_t version{ 0x02070003 };          ///< Version reported by SystemClass::getVersion.
    uint32_t serial{ 0 };                    ///< Serial number reported by SystemClass::getSerialNumber.
    int      modelNameCalls{ 0 };            ///< Number of aDefs_GetModelName calls.
    int      versionCalls{ 0 };              ///< Number of aVersion_ParseString calls.
};

/// The global fake hub state used by all stubs.
brainStemStubState g_brainStem;

extern "C"
{
    const char *aDefs_GetModelName( const int modelNum )
    {
        ++g_brainStem.modelNameCalls;
        if( modelNum == aMODULE_TYPE_USBHub3p )
        {
            return "USBHub3p";
        }
        return "Unknown";
    }

    void aVersion_ParseString( uint32_t build, char *string, size_t len )
    {
        ++g_brainStem.versionCalls;
        snprintf( string, len, "%u.%u.%u", build >> 24, ( build >> 16 ) & 0xFF, build & 0xFFFF );
    }
}

namespace Acroname
{
namespace BrainStem
{

aErr Module::connect( const linkType type, const uint32_t serialNum )
{
    ++g_brainStem.connectCalls;
    g_brainStem.lastLinkType = type;
    g_brainStem.lastSerial   = serialNum;
    if( g_brainStem.connectErr == aErrNone )
    {
        g_brainStem.connected = true;
    }
    return g_brainStem.connectErr;
}

bool Module::isConnected( void )
{
    return g_brainStem.connected;
}

aErr Module::disconnect( void )
{
    ++g_brainStem.disconnectCalls;
    g_brainStem.connected = false;
    return aErrNone;
}

void SystemClass::init( Module *pModule, const uint8_t index )
{
    static_cast<void>( index );
    ++g_brainStem.sysInitCalls;
    g_brainStem.sysModule = pModule;
}

aErr SystemClass::getModel( uint8_t *model )
{
    *model = g_brainStem.model;
    return aErrNone;
}

aErr SystemClass::getVersion( uint32_t *build )
{
    *build = g_brainStem.version;
    return aErrNone;
}

aErr SystemClass::getSerialNumber( uint32_t *serialNumber )
{
    *serialNumber = g_brainStem.serial;
    return aErrNone;
}

aErr USBClass::setPortEnable( const uint8_t channel )
{
    g_brainStem.enabledPorts.push_back( channel );
    if( g_brainStem.setPortEnableErr != aErrNone )
    {
        return g_brainStem.setPortEnableErr;
    }
    if( channel < aUSBHUB3P_NUM_USB_PORTS )
    {
        g_brainStem.portState[channel] |= 1;
    }
    return aErrNone;
}

aErr USBClass::setPortDisable( const uint8_t channel )
{
    g_brainStem.disabledPorts.push_back( channel );
    if( g_brainStem.setPortDisableErr != aErrNone )
    {
        return g_brainStem.setPortDisableErr;
    }
    if( channel < aUSBHUB3P_NUM_USB_PORTS )
    {
        g_brainStem.portState[channel] &= ~static_cast<uint32_t>( 1 );
    }
    return aErrNone;
}

aErr USBClass::getPortState( const uint8_t channel, uint32_t *state )
{
    ++g_brainStem.getPortStateCalls;
    g_brainStem.portStateChannels.push_back( channel );
    if( g_brainStem.getPortStateErr != aErrNone &&
        ( g_brainStem.getPortStateErrPort < 0 || g_brainStem.getPortStateErrPort == channel ) )
    {
        return g_brainStem.getPortStateErr; // state is not written on error
    }
    if( channel < aUSBHUB3P_NUM_USB_PORTS )
    {
        *state = g_brainStem.portState[channel];
    }
    return aErrNone;
}

} // namespace BrainStem
} // namespace Acroname

/// \endcond

namespace libXWCTest
{

/** \defgroup acronameUsbHub_unit_test acronameUsbHub Unit Tests
 * \brief Unit tests for the acronameUsbHub application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `acronameUsbHub` unit tests.
/** \ingroup acronameUsbHub_unit_test
 */
namespace acronameUsbHubTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing acronameUsbHub internals.
class acronameUsbHub_test : public acronameUsbHub
{
  public:
    /// Construct a harness with the given device name, resetting the BrainStem stub.
    explicit acronameUsbHub_test( const std::string &device /**< [in] INDI device name */ )
    {
        g_brainStem.reset();
        m_configName = device;
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
    }

    /// Run setupConfig(), read the given config file, and run only the outletController loadConfig().
    /**
     * \returns the return value of dev::outletController::loadConfig
     */
    int configureOutlets( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        return dev::outletController<acronameUsbHub>::loadConfig( config );
    }

    /// The configured hub serial number.
    using acronameUsbHub::m_serialNumber;

    /// The BrainStem hub handle.
    using acronameUsbHub::m_hub;

    /// Whether the hub is connected.
    using acronameUsbHub::m_connected;

    /// Whether power management is enabled.
    using acronameUsbHub::m_powerMgtEnabled;

    /// The registered new-property callbacks.
    using acronameUsbHub::m_indiNewCallBacks;
};

/// \endcond

/// Build a channel request property as a client would send it.
/**
 * \returns the property with a `target` and/or a `state` element
 */
pcf::IndiProperty
channelRequest( const std::string &device, /**< [in] the device name */
                const std::string &name,   /**< [in] the channel name */
                const std::string &target, /**< [in] the target value, not added if empty */
                const std::string &state = "" /**< [in] [optional] the state value, not added if empty */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Text );
    ip.setDevice( device );
    ip.setName( name );
    if( target != "" )
    {
        ip.add( pcf::IndiElement( "target", target ) );
    }
    if( state != "" )
    {
        ip.add( pcf::IndiElement( "state", state ) );
    }
    return ip;
}

/// Write the standard two-channel config file used by several tests.
/** Serial number 123456; channel `camera` is outlet 2; channel `pump` is outlets 6 and 7 with orders and short delays.
 */
void writeStdConfig( const std::string &file /**< [in] the config file path */ )
{
    mx::app::writeConfigFile( file,
                              { "device", "camera", "pump", "pump", "pump", "pump", "pump" },
                              { "serialNumber", "outlet", "outlets", "onOrder", "offOrder", "onDelays", "offDelays" },
                              { "123456", "2", "6,7", "1,0", "0,1", "0,1", "0,1" } );
}

/// Verify the constructor sets up an 8-outlet, 0-based controller without power management.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub construction and destruction", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    acronameUsbHub::acronameUsbHub();
    acronameUsbHub::~acronameUsbHub();
    dev::outletController<acronameUsbHub>::setNumberOfOutlets(8);
    #endif
    // clang-format on

    {
        acronameUsbHub_test app( "hub" );

        REQUIRE( app.m_powerMgtEnabled == false );
        REQUIRE( app.m_firstOne == false );
        REQUIRE( app.m_serialNumber == 0 );
        REQUIRE( app.m_connected == false );
        REQUIRE( app.m_outletStates.size() == 8 );
        for( size_t n = 0; n < app.m_outletStates.size(); ++n )
        {
            REQUIRE( app.outletState( n ) == OUTLET_STATE_UNKNOWN );
        }
        REQUIRE( app.numChannels() == 0 );
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
        REQUIRE( g_brainStem.disconnectCalls == 0 );
    }

    // The destructor disconnects the hub.
    REQUIRE( g_brainStem.disconnectCalls == 1 );
}

/// Verify configuration defaults and overrides, and the channel specifications loaded from config.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub configuration", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    acronameUsbHub::setupConfig();
    acronameUsbHub::loadConfig();
    dev::outletController<acronameUsbHub>::loadConfig(config);
    dev::outletController<acronameUsbHub>::channelOutlets("");
    dev::outletController<acronameUsbHub>::channelOnOrder("");
    dev::outletController<acronameUsbHub>::channelOffOrder("");
    dev::outletController<acronameUsbHub>::channelOnDelays("");
    dev::outletController<acronameUsbHub>::channelOffDelays("");
    #endif
    // clang-format on

    SECTION( "defaults: serial number 0 and no channels" )
    {
        std::string file = "/tmp/acronameUsbHub_test_defaults.conf";
        mx::app::writeConfigFile( file, { "none" }, { "nada" }, { "0" } );

        acronameUsbHub_test app( "hub" );
        app.configure( file ); // the outletController error (no valid channels) is ignored by loadConfig

        REQUIRE( app.m_serialNumber == 0 );
        REQUIRE( app.numChannels() == 0 );

        remove( file.c_str() );
    }

    SECTION( "overrides and multi-outlet channels, 0-based outlets" )
    {
        std::string file = "/tmp/acronameUsbHub_test_overrides.conf";
        writeStdConfig( file );

        acronameUsbHub_test app( "hub" );
        app.configure( file );

        REQUIRE( app.m_serialNumber == 123456 );

        REQUIRE( app.numChannels() == 2 );
        REQUIRE( app.channelOutlets( "camera" ) == std::vector<size_t>( { 2 } ) );
        REQUIRE( app.channelOnOrder( "camera" ).size() == 0 );
        REQUIRE( app.channelOffDelays( "camera" ).size() == 0 );
        REQUIRE( app.channelOutlets( "pump" ) == std::vector<size_t>( { 6, 7 } ) );
        REQUIRE( app.channelOnOrder( "pump" ) == std::vector<size_t>( { 1, 0 } ) );
        REQUIRE( app.channelOffOrder( "pump" ) == std::vector<size_t>( { 0, 1 } ) );
        REQUIRE( app.channelOnDelays( "pump" ) == std::vector<unsigned>( { 0, 1 } ) );
        REQUIRE( app.channelOffDelays( "pump" ) == std::vector<unsigned>( { 0, 1 } ) );

        remove( file.c_str() );
    }

    SECTION( "the maximum serial number fits in uint32" )
    {
        std::string file = "/tmp/acronameUsbHub_test_serial.conf";
        mx::app::writeConfigFile( file, { "device" }, { "serialNumber" }, { "4294967295" } );

        acronameUsbHub_test app( "hub" );
        app.configure( file );

        REQUIRE( app.m_serialNumber == 4294967295u );

        remove( file.c_str() );
    }
}

/// Verify the outletController channel-config error returns with the acronameUsbHub 0-based outlets.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub channel configuration errors", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    dev::outletController<acronameUsbHub>::loadConfig(config);
    #endif
    // clang-format on

    std::string file = "/tmp/acronameUsbHub_test_errors.conf";

    SECTION( "valid channels, outlet 0 allowed" )
    {
        mx::app::writeConfigFile(
            file, { "device", "a", "b" }, { "serialNumber", "outlet", "outlet" }, { "1", "0", "7" } );
        acronameUsbHub_test app( "hub" );
        REQUIRE( app.configureOutlets( file ) == 0 );
        REQUIRE( app.channelOutlets( "a" ) == std::vector<size_t>( { 0 } ) );
        REQUIRE( app.channelOutlets( "b" ) == std::vector<size_t>( { 7 } ) );
    }

    SECTION( "no channel sections" )
    {
        mx::app::writeConfigFile( file, { "device" }, { "serialNumber" }, { "1" } );
        acronameUsbHub_test app( "hub" );
        REQUIRE( app.configureOutlets( file ) == OUTLET_E_NOCHANNELS );
        REQUIRE( app.numChannels() == 0 );
    }

    SECTION( "unused section without an outlet keyword" )
    {
        mx::app::writeConfigFile( file, { "device", "notachannel" }, { "serialNumber", "foo" }, { "1", "1" } );
        acronameUsbHub_test app( "hub" );
        REQUIRE( app.configureOutlets( file ) == OUTLET_E_NOVALIDCH );
        REQUIRE( app.numChannels() == 0 );
    }

    SECTION( "outlet beyond the device range is invalid" )
    {
        mx::app::writeConfigFile( file, { "device", "a" }, { "serialNumber", "outlet" }, { "1", "9" } );
        acronameUsbHub_test app( "hub" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    SECTION( "onOrder size mismatch" )
    {
        mx::app::writeConfigFile(
            file, { "device", "a", "a" }, { "serialNumber", "outlets", "onOrder" }, { "1", "1,2", "0" } );
        acronameUsbHub_test app( "hub" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    SECTION( "offOrder size mismatch" )
    {
        mx::app::writeConfigFile(
            file, { "device", "a", "a" }, { "serialNumber", "outlets", "offOrder" }, { "1", "1,2", "0,1,2" } );
        acronameUsbHub_test app( "hub" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    SECTION( "onDelays size mismatch" )
    {
        mx::app::writeConfigFile(
            file, { "device", "a", "a" }, { "serialNumber", "outlets", "onDelays" }, { "1", "1,2", "0" } );
        acronameUsbHub_test app( "hub" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    remove( file.c_str() );
}

/// Verify updateOutletState maps the hub port state to outlet states and handles errors.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub updateOutletState", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    acronameUsbHub::updateOutletState(0);
    #endif
    // clang-format on

    acronameUsbHub_test app( "hub" );

    SECTION( "bit 0 of the port state is the outlet state" )
    {
        g_brainStem.portState[3] = 1;
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_ON );
        REQUIRE( g_brainStem.portStateChannels == std::vector<int>( { 3 } ) );

        g_brainStem.portState[3] = 0;
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_OFF );

        // other bits (data enabled, speed, device attached) do not matter
        g_brainStem.portState[3] = ( 1u << 1 ) | ( 1u << 3 ) | ( 1u << 23 );
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_OFF );

        g_brainStem.portState[3] = ( 1u << 23 ) | 1u;
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_ON );
    }

    SECTION( "only the requested outlet changes" )
    {
        g_brainStem.portState[5] = 1;
        REQUIRE( app.updateOutletState( 5 ) == 0 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == ( n == 5 ? OUTLET_STATE_ON : OUTLET_STATE_UNKNOWN ) );
        }
    }

    SECTION( "timeout and connection errors fail and leave the state unchanged" )
    {
        g_brainStem.portState[1]    = 1;
        g_brainStem.getPortStateErr = aErrTimeout;
        REQUIRE( app.updateOutletState( 1 ) == -1 );
        REQUIRE( app.outletState( 1 ) == OUTLET_STATE_UNKNOWN );

        g_brainStem.getPortStateErr = aErrConnection;
        REQUIRE( app.updateOutletState( 1 ) == -1 );
        REQUIRE( app.outletState( 1 ) == OUTLET_STATE_UNKNOWN );
    }

    SECTION( "other errors are not reported and the outlet reads as off" )
    {
        // Only timeout and connection errors are checked; for any other error the unread state (0) is used.
        g_brainStem.portState[1]    = 1;
        app.m_outletStates[1]       = OUTLET_STATE_ON;
        g_brainStem.getPortStateErr = aErrIO;
        REQUIRE( app.updateOutletState( 1 ) == 0 );
        REQUIRE( app.outletState( 1 ) == OUTLET_STATE_OFF );
    }
}

/// Verify updateOutletStates reads every port and stops at the first failure.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub updateOutletStates", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    dev::outletController<acronameUsbHub>::updateOutletStates();
    #endif
    // clang-format on

    acronameUsbHub_test app( "hub" );

    for( int n = 0; n < 8; ++n )
    {
        g_brainStem.portState[n] = n % 2;
    }

    SECTION( "all ports read" )
    {
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( g_brainStem.portStateChannels == std::vector<int>( { 0, 1, 2, 3, 4, 5, 6, 7 } ) );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == ( n % 2 ? OUTLET_STATE_ON : OUTLET_STATE_OFF ) );
        }
    }

    SECTION( "a timeout on port 3 stops the update" )
    {
        g_brainStem.getPortStateErr     = aErrTimeout;
        g_brainStem.getPortStateErrPort = 3;

        REQUIRE( app.updateOutletStates() == -1 );
        REQUIRE( g_brainStem.portStateChannels == std::vector<int>( { 0, 1, 2, 3 } ) );
        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_OFF );
        REQUIRE( app.outletState( 1 ) == OUTLET_STATE_ON );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_OFF );
        for( int n = 3; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == OUTLET_STATE_UNKNOWN );
        }
    }
}

/// Verify turnOutletOn/turnOutletOff command the right port and handle errors.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub turnOutletOn and turnOutletOff", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    acronameUsbHub::turnOutletOn(0);
    acronameUsbHub::turnOutletOff(0);
    #endif
    // clang-format on

    acronameUsbHub_test app( "hub" );

    SECTION( "success" )
    {
        REQUIRE( app.turnOutletOn( 4 ) == 0 );
        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 4 } ) );
        REQUIRE( g_brainStem.disabledPorts.size() == 0 );

        // The stored state is not changed by a command, only by updateOutletState.
        REQUIRE( app.outletState( 4 ) == OUTLET_STATE_UNKNOWN );
        REQUIRE( app.updateOutletState( 4 ) == 0 );
        REQUIRE( app.outletState( 4 ) == OUTLET_STATE_ON );

        REQUIRE( app.turnOutletOff( 4 ) == 0 );
        REQUIRE( g_brainStem.disabledPorts == std::vector<int>( { 4 } ) );
        REQUIRE( app.updateOutletState( 4 ) == 0 );
        REQUIRE( app.outletState( 4 ) == OUTLET_STATE_OFF );
    }

    SECTION( "timeout and connection errors fail" )
    {
        g_brainStem.setPortEnableErr  = aErrTimeout;
        g_brainStem.setPortDisableErr = aErrTimeout;
        REQUIRE( app.turnOutletOn( 0 ) == -1 );
        REQUIRE( app.turnOutletOff( 0 ) == -1 );

        g_brainStem.setPortEnableErr  = aErrConnection;
        g_brainStem.setPortDisableErr = aErrConnection;
        REQUIRE( app.turnOutletOn( 7 ) == -1 );
        REQUIRE( app.turnOutletOff( 7 ) == -1 );

        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 0, 7 } ) );
        REQUIRE( g_brainStem.disabledPorts == std::vector<int>( { 0, 7 } ) );
    }

    SECTION( "other errors are not reported" )
    {
        g_brainStem.setPortEnableErr  = aErrBusy;
        g_brainStem.setPortDisableErr = aErrParam;
        REQUIRE( app.turnOutletOn( 2 ) == 0 );
        REQUIRE( app.turnOutletOff( 2 ) == 0 );
    }
}

/// Verify channel state aggregation from the hub port states.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub channelState", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    dev::outletController<acronameUsbHub>::channelState("");
    #endif
    // clang-format on

    std::string file = "/tmp/acronameUsbHub_test_chstate.conf";
    writeStdConfig( file );

    acronameUsbHub_test app( "hub" );
    app.configure( file );

    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_UNKNOWN );
    REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_UNKNOWN );

    REQUIRE( app.updateOutletStates() == 0 );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_OFF );
    REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_OFF );

    g_brainStem.portState[6] = 1;
    REQUIRE( app.updateOutletStates() == 0 );
    REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_INTERMEDIATE );

    g_brainStem.portState[7] = 1;
    REQUIRE( app.updateOutletStates() == 0 );
    REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_ON );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_OFF );

    g_brainStem.portState[2] = 1;
    REQUIRE( app.updateOutletStates() == 0 );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_ON );

    remove( file.c_str() );
}

/// Verify turnChannelOn/turnChannelOff command the hub ports in the configured order.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub turnChannelOn and turnChannelOff", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    dev::outletController<acronameUsbHub>::turnChannelOn("");
    dev::outletController<acronameUsbHub>::turnChannelOff("");
    #endif
    // clang-format on

    std::string file = "/tmp/acronameUsbHub_test_turnch.conf";
    writeStdConfig( file );

    acronameUsbHub_test app( "hub" );
    app.configure( file );
    REQUIRE( app.updateOutletStates() == 0 ); // all off

    SECTION( "on and off in the configured orders" )
    {
        REQUIRE( app.turnChannelOn( "pump" ) == 0 );
        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 7, 6 } ) ); // onOrder=1,0
        REQUIRE( app.m_channels["pump"].m_stateTime.tv_sec > 0 );

        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_ON );

        REQUIRE( app.turnChannelOff( "pump" ) == 0 );
        REQUIRE( g_brainStem.disabledPorts == std::vector<int>( { 6, 7 } ) ); // offOrder=0,1

        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_OFF );
    }

    SECTION( "single-outlet channel" )
    {
        REQUIRE( app.turnChannelOn( "camera" ) == 0 );
        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 2 } ) );
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_ON );
        REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_OFF );
    }

    SECTION( "already on / already off is a no-op" )
    {
        REQUIRE( app.turnChannelOff( "pump" ) == 0 );
        REQUIRE( g_brainStem.disabledPorts.size() == 0 );

        g_brainStem.portState[2] = 1;
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.turnChannelOn( "camera" ) == 0 );
        REQUIRE( g_brainStem.enabledPorts.size() == 0 );
    }

    SECTION( "an intermediate channel is commanded" )
    {
        g_brainStem.portState[6] = 1;
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_INTERMEDIATE );

        REQUIRE( app.turnChannelOn( "pump" ) == 0 );
        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 7, 6 } ) );
    }

    SECTION( "the state delay blocks a change too soon after the last one" )
    {
        REQUIRE( app.turnChannelOn( "camera" ) == 0 );
        REQUIRE( app.updateOutletStates() == 0 );

        app.m_stateDelay = 1000; // seconds
        REQUIRE( app.turnChannelOff( "camera" ) == 0 );
        REQUIRE( g_brainStem.disabledPorts.size() == 0 );

        app.m_stateDelay = 0;
        REQUIRE( app.turnChannelOff( "camera" ) == 0 );
        REQUIRE( g_brainStem.disabledPorts == std::vector<int>( { 2 } ) );
    }

    SECTION( "a port error fails and stops the sequence" )
    {
        g_brainStem.setPortEnableErr = aErrTimeout;
        REQUIRE( app.turnChannelOn( "pump" ) == -1 );
        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 7 } ) );
        REQUIRE( app.m_channels["pump"].m_stateTime.tv_sec == 0 );

        g_brainStem.portState[6] = 1;
        g_brainStem.portState[7] = 1;
        REQUIRE( app.updateOutletStates() == 0 );
        g_brainStem.setPortDisableErr = aErrConnection;
        REQUIRE( app.turnChannelOff( "pump" ) == -1 );
        REQUIRE( g_brainStem.disabledPorts == std::vector<int>( { 6 } ) );
    }

    remove( file.c_str() );
}

/// Verify the outletController channel new-property callback drives the hub.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub newCallBack_channels", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    dev::outletController<acronameUsbHub>::newCallBack_channels(pcf::IndiProperty());
    dev::outletController<acronameUsbHub>::st_newCallBack_channels(nullptr, pcf::IndiProperty());
    #endif
    // clang-format on

    std::string file = "/tmp/acronameUsbHub_test_newch.conf";
    writeStdConfig( file );

    acronameUsbHub_test app( "hub" );
    app.configure( file );
    REQUIRE( app.updateOutletStates() == 0 ); // all off

    SECTION( "rejected when not READY" )
    {
        REQUIRE( app.state() != stateCodes::READY );
        REQUIRE( app.newCallBack_channels( channelRequest( "hub", "camera", "On" ) ) == -1 );
        REQUIRE( g_brainStem.enabledPorts.size() == 0 );
    }

    SECTION( "READY" )
    {
        app.state( stateCodes::READY );

        // unrecognized or empty targets are ignored
        REQUIRE( app.newCallBack_channels( channelRequest( "hub", "camera", "bogus" ) ) == 0 );
        REQUIRE( app.newCallBack_channels( channelRequest( "hub", "camera", "" ) ) == 0 );
        REQUIRE( g_brainStem.enabledPorts.size() == 0 );

        // case-insensitive target
        REQUIRE( app.newCallBack_channels( channelRequest( "hub", "camera", "on" ) ) == 0 );
        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 2 } ) );
        REQUIRE( app.updateOutletStates() == 0 );

        // target takes precedence over state
        REQUIRE( app.newCallBack_channels( channelRequest( "hub", "camera", "On", "Off" ) ) == 0 );
        REQUIRE( g_brainStem.disabledPorts.size() == 0 );

        // state is used when there is no target
        REQUIRE( app.newCallBack_channels( channelRequest( "hub", "camera", "", "OFF" ) ) == 0 );
        REQUIRE( g_brainStem.disabledPorts == std::vector<int>( { 2 } ) );
        REQUIRE( app.updateOutletStates() == 0 );

        // the static wrapper dispatches to the member
        REQUIRE( dev::outletController<acronameUsbHub>::st_newCallBack_channels(
                     static_cast<acronameUsbHub *>( &app ), channelRequest( "hub", "pump", "On" ) ) == 0 );
        REQUIRE( g_brainStem.enabledPorts == std::vector<int>( { 2, 7, 6 } ) );
    }

    remove( file.c_str() );
}

/// Verify appStartup sets up the outletController INDI properties and goes to NOTCONNECTED.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub appStartup", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    acronameUsbHub::appStartup();
    dev::outletController<acronameUsbHub>::setupINDI();
    #endif
    // clang-format on

    std::string file = "/tmp/acronameUsbHub_test_startup.conf";
    writeStdConfig( file );

    acronameUsbHub_test app( "hub" );
    app.configure( file );

    REQUIRE( app.appStartup() == 0 );
    REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    REQUIRE( g_brainStem.connectCalls == 0 ); // connecting is left to appLogic

    // The outlet states property has 0-based elements.
    REQUIRE( app.m_indiP_outletStates.getName() == "outlet" );
    REQUIRE( app.m_indiP_outletStates.getDevice() == "hub" );
    REQUIRE( app.m_indiP_outletStates.getNumElements() == 8 );
    REQUIRE( app.m_indiP_outletStates.find( "0" ) );
    REQUIRE( app.m_indiP_outletStates.find( "7" ) );
    REQUIRE_FALSE( app.m_indiP_outletStates.find( "8" ) );

    // Channel properties with state and target elements, and their callbacks.
    REQUIRE( app.m_channels["camera"].m_indiP_prop.getName() == "camera" );
    REQUIRE( app.m_channels["camera"].m_indiP_prop.getDevice() == "hub" );
    REQUIRE( app.m_channels["camera"].m_indiP_prop.find( "state" ) );
    REQUIRE( app.m_channels["camera"].m_indiP_prop.find( "target" ) );
    REQUIRE( app.m_indiNewCallBacks.count( "hub.camera" ) == 1 );
    REQUIRE( app.m_indiNewCallBacks.count( "hub.pump" ) == 1 );

    // Static channel information.
    REQUIRE( app.m_indiP_chOutlets["camera"].get() == "2" );
    REQUIRE( app.m_indiP_chOutlets["pump"].get() == "6,7" );
    REQUIRE( app.m_indiP_chOnDelays["pump"].get<double>() == Approx( 1 ) );
    REQUIRE( app.m_indiP_chOffDelays["pump"].get<double>() == Approx( 1 ) );
    REQUIRE( app.m_indiP_chOnDelays["camera"].get<double>() == Approx( 0 ) );
    REQUIRE( app.m_indiP_stateTimes["camera"].get<int>() == 0 );

    remove( file.c_str() );
}

/// Verify the appLogic connection state machine.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub appLogic", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    acronameUsbHub::appLogic();
    dev::outletController<acronameUsbHub>::updateINDI();
    #endif
    // clang-format on

    std::string file = "/tmp/acronameUsbHub_test_logic.conf";
    writeStdConfig( file );

    acronameUsbHub_test app( "hub" );
    app.configure( file );

    SECTION( "POWERON with a failed connection stays NOTCONNECTED" )
    {
        g_brainStem.connectErr = aErrNotFound;
        app.state( stateCodes::POWERON );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( app.m_connected == false );
        REQUIRE( g_brainStem.connectCalls == 1 );
        REQUIRE( g_brainStem.lastLinkType == USB );
        REQUIRE( g_brainStem.lastSerial == 123456 );
        REQUIRE( g_brainStem.sysInitCalls == 0 );
        REQUIRE( g_brainStem.getPortStateCalls == 0 );

        // retried on the next call
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( g_brainStem.connectCalls == 2 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "a successful connection queries the system entity, goes READY and reads the ports" )
    {
        g_brainStem.serial       = 123456;
        g_brainStem.portState[2] = 1;
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( app.m_connected == true );
        REQUIRE( g_brainStem.connectCalls == 1 );
        REQUIRE( g_brainStem.sysInitCalls == 1 );
        REQUIRE( g_brainStem.sysModule == static_cast<Module *>( &app.m_hub ) );
        REQUIRE( g_brainStem.modelNameCalls == 1 );
        REQUIRE( g_brainStem.versionCalls == 1 );

        // The READY branch runs in the same call.
        REQUIRE( g_brainStem.getPortStateCalls == 8 );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_ON );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_OFF );
        REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_ON );

        // Subsequent calls just update the outlets.
        g_brainStem.portState[2] = 0;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( g_brainStem.connectCalls == 1 );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_OFF );
    }

    SECTION( "READY with a lost connection disconnects and goes NOTCONNECTED" )
    {
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        int reads = g_brainStem.getPortStateCalls;

        g_brainStem.connected = false;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( app.m_connected == false );
        REQUIRE( g_brainStem.disconnectCalls == 1 );
        REQUIRE( g_brainStem.getPortStateCalls == reads );

        // then reconnects
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( g_brainStem.connectCalls == 2 );
    }

    SECTION( "NOTCONNECTED while marked connected disconnects before reconnecting" )
    {
        app.m_connected        = true;
        g_brainStem.connectErr = aErrBusy;
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( g_brainStem.disconnectCalls == 1 );
        REQUIRE( g_brainStem.connectCalls == 1 );
        REQUIRE( app.m_connected == false );
    }

    SECTION( "a port read error in READY is not an appLogic error" )
    {
        app.state( stateCodes::NOTCONNECTED );
        g_brainStem.getPortStateErr = aErrTimeout;

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_UNKNOWN );
    }

    SECTION( "other states do nothing" )
    {
        app.state( stateCodes::FAILURE );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::FAILURE );
        REQUIRE( g_brainStem.connectCalls == 0 );
    }

    // The INDI mutex is released after each call.
    std::unique_lock<std::mutex> lock( app.m_indiMutex, std::try_to_lock );
    REQUIRE( lock.owns_lock() );

    remove( file.c_str() );
}

/// Verify the power-off hooks and appShutdown.
/**
 * \ingroup acronameUsbHub_unit_test
 */
TEST_CASE( "acronameUsbHub onPowerOff, whilePowerOff and appShutdown", "[acronameUsbHub]" )
{
    // clang-format off
    #ifdef ACRONAMEUSBHUB_TEST_DOXYGEN_REF
    acronameUsbHub::onPowerOff();
    acronameUsbHub::whilePowerOff();
    acronameUsbHub::appShutdown();
    #endif
    // clang-format on

    std::string file = "/tmp/acronameUsbHub_test_poweroff.conf";
    writeStdConfig( file );

    acronameUsbHub_test app( "hub" );
    app.configure( file );
    REQUIRE( app.appStartup() == 0 );

    SECTION( "connected: disconnects and marks all outlets off" )
    {
        for( int n = 0; n < 8; ++n )
        {
            g_brainStem.portState[n] = 1;
        }
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_connected == true );
        REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_ON );

        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( app.m_connected == false );
        REQUIRE( g_brainStem.disconnectCalls == 1 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == OUTLET_STATE_OFF );
        }
        REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_OFF );
        REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_OFF );
    }

    SECTION( "not connected: no disconnect" )
    {
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_brainStem.disconnectCalls == 0 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == OUTLET_STATE_OFF );
        }
    }

    REQUIRE( app.whilePowerOff() == 0 );
    REQUIRE( app.appShutdown() == 0 );

    std::unique_lock<std::mutex> lock( app.m_indiMutex, std::try_to_lock );
    REQUIRE( lock.owns_lock() );

    remove( file.c_str() );
}

} // namespace acronameUsbHubTest

} // namespace libXWCTest
