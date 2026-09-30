/** \file xt1121DCDU_test.cpp
 * \brief Catch2 tests for the xt1121DCDU app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup xt1121DCDU_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "../xt1121DCDU.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup xt1121DCDU_unit_test xt1121DCDU Unit Tests
 * \brief Unit tests for the xt1121DCDU application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `xt1121DCDU` unit tests.
/** \ingroup xt1121DCDU_unit_test
 */
namespace xt1121DCDUTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing xt1121DCDU internals.
class xt1121DCDU_test : public xt1121DCDU
{
  public:
    /// Construct a harness with the given device name.
    explicit xt1121DCDU_test( const std::string &device /**< [in] INDI device name */ )
    {
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
        return dev::outletController<xt1121DCDU>::loadConfig( config );
    }

    /// The xt1121Ctrl device name.
    using xt1121DCDU::m_deviceName;

    /// The xt1121Ctrl channel numbers used for outlets 0-7.
    using xt1121DCDU::m_channelNumbers;

    /// The registered set-property callbacks.
    using xt1121DCDU::m_indiSetCallBacks;

    /// The registered new-property callbacks.
    using xt1121DCDU::m_indiNewCallBacks;

    /// The channel-name helper.
    using xt1121DCDU::xtChannelName;

    /// The outlet-to-property helper.
    using xt1121DCDU::xtChannelProperty;

    /// Set callback for outlet 0.
    using xt1121DCDU::setCallBack_ip_ch0;

    /// Set callback for outlet 1.
    using xt1121DCDU::setCallBack_ip_ch1;

    /// Set callback for outlet 2.
    using xt1121DCDU::setCallBack_ip_ch2;

    /// Set callback for outlet 3.
    using xt1121DCDU::setCallBack_ip_ch3;

    /// Set callback for outlet 4.
    using xt1121DCDU::setCallBack_ip_ch4;

    /// Set callback for outlet 5.
    using xt1121DCDU::setCallBack_ip_ch5;

    /// Set callback for outlet 6.
    using xt1121DCDU::setCallBack_ip_ch6;

    /// Set callback for outlet 7.
    using xt1121DCDU::setCallBack_ip_ch7;

    /// Call the set callback for an outlet by number.
    /**
     * \returns the callback return value, or -99 for an invalid outlet number
     */
    int setCallBack( int                      outletNum, /**< [in] the outlet number, 0-7 */
                     const pcf::IndiProperty &ip /**< [in] the property to deliver */ )
    {
        switch( outletNum )
        {
        case 0:
            return setCallBack_ip_ch0( ip );
        case 1:
            return setCallBack_ip_ch1( ip );
        case 2:
            return setCallBack_ip_ch2( ip );
        case 3:
            return setCallBack_ip_ch3( ip );
        case 4:
            return setCallBack_ip_ch4( ip );
        case 5:
            return setCallBack_ip_ch5( ip );
        case 6:
            return setCallBack_ip_ch6( ip );
        case 7:
            return setCallBack_ip_ch7( ip );
        default:
            return -99;
        }
    }
};

/// \endcond

/// Build an xt1121Ctrl channel property as it would arrive in a set callback.
/**
 * \returns the property with a `current` element, and optionally a `target` element
 */
pcf::IndiProperty xtChannel( const std::string &device,  /**< [in] the device name */
                             const std::string &name,    /**< [in] the property name, e.g. ch00 */
                             int                current, /**< [in] the value of the current element */
                             bool withCurrent = true /**< [in] [optional] if false, no current element is added */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    if( withCurrent )
    {
        ip.add( pcf::IndiElement( "current", current ) );
    }
    ip.add( pcf::IndiElement( "target", current ) );
    return ip;
}

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
/** Channel `pump` is outlet 1 (index 0); channel `camera` is outlets 2 and 3 (indices 1 and 2) with orders and delays.
 */
void writeStdConfig( const std::string &file /**< [in] the config file path */ )
{
    mx::app::writeConfigFile(
        file,
        { "device", "device", "pump", "camera", "camera", "camera", "camera", "camera" },
        { "name", "channelNumbers", "outlet", "outlets", "onOrder", "offOrder", "onDelays", "offDelays" },
        { "xtdev", "8,9,10,11,12,13,14,15", "1", "2,3", "1,0", "0,1", "0,150", "0,345" } );
}

/// Verify the constructor sets up an 8-outlet, 1-based controller.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU construction", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::xt1121DCDU();
    dev::outletController<xt1121DCDU>::setNumberOfOutlets(8);
    #endif
    // clang-format on

    xt1121DCDU_test app( "dcdu" );

    REQUIRE( app.m_firstOne == true );
    REQUIRE( app.m_outletStates.size() == 8 );
    for( size_t n = 0; n < app.m_outletStates.size(); ++n )
    {
        REQUIRE( app.m_outletStates[n] == OUTLET_STATE_UNKNOWN );
        REQUIRE( app.outletState( n ) == OUTLET_STATE_UNKNOWN );
    }
    REQUIRE( app.numChannels() == 0 );
    REQUIRE( app.state() == stateCodes::UNINITIALIZED );
}

/// Verify configuration defaults and overrides, and the channel specifications loaded from config.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU configuration", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::setupConfig();
    xt1121DCDU::loadConfig();
    dev::outletController<xt1121DCDU>::loadConfig(config);
    dev::outletController<xt1121DCDU>::channelOutlets("");
    dev::outletController<xt1121DCDU>::channelOnOrder("");
    dev::outletController<xt1121DCDU>::channelOffOrder("");
    dev::outletController<xt1121DCDU>::channelOnDelays("");
    dev::outletController<xt1121DCDU>::channelOffDelays("");
    #endif
    // clang-format on

    SECTION( "defaults: channel numbers 0-7" )
    {
        std::string file = "/tmp/xt1121DCDU_test_defaults.conf";
        mx::app::writeConfigFile( file, { "device", "pump" }, { "name", "outlet" }, { "xtdev", "4" } );

        xt1121DCDU_test app( "dcdu" );
        app.configure( file );

        REQUIRE( app.m_deviceName == "xtdev" );
        REQUIRE( app.m_channelNumbers == std::vector<int>( { 0, 1, 2, 3, 4, 5, 6, 7 } ) );

        REQUIRE( app.numChannels() == 1 );
        REQUIRE( app.channelOutlets( "pump" ) == std::vector<size_t>( { 3 } ) ); // 1-based config
        REQUIRE( app.channelOnOrder( "pump" ).size() == 0 );
        REQUIRE( app.channelOffOrder( "pump" ).size() == 0 );
        REQUIRE( app.channelOnDelays( "pump" ).size() == 0 );
        REQUIRE( app.channelOffDelays( "pump" ).size() == 0 );

        remove( file.c_str() );
    }

    SECTION( "overrides and multi-outlet channels" )
    {
        std::string file = "/tmp/xt1121DCDU_test_overrides.conf";
        writeStdConfig( file );

        xt1121DCDU_test app( "dcdu" );
        app.configure( file );

        REQUIRE( app.m_deviceName == "xtdev" );
        REQUIRE( app.m_channelNumbers == std::vector<int>( { 8, 9, 10, 11, 12, 13, 14, 15 } ) );

        REQUIRE( app.numChannels() == 2 );
        REQUIRE( app.channelOutlets( "pump" ) == std::vector<size_t>( { 0 } ) );
        REQUIRE( app.channelOutlets( "camera" ) == std::vector<size_t>( { 1, 2 } ) );
        REQUIRE( app.channelOnOrder( "camera" ) == std::vector<size_t>( { 1, 0 } ) );
        REQUIRE( app.channelOffOrder( "camera" ) == std::vector<size_t>( { 0, 1 } ) );
        REQUIRE( app.channelOnDelays( "camera" ) == std::vector<unsigned>( { 0, 150 } ) );
        REQUIRE( app.channelOffDelays( "camera" ) == std::vector<unsigned>( { 0, 345 } ) );

        remove( file.c_str() );
    }
}

/// Verify the outletController channel-config error returns with the xt1121DCDU 1-based outlets.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU channel configuration errors", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    dev::outletController<xt1121DCDU>::loadConfig(config);
    #endif
    // clang-format on

    std::string file = "/tmp/xt1121DCDU_test_errors.conf";

    SECTION( "valid channels" )
    {
        mx::app::writeConfigFile( file, { "device", "a", "b" }, { "name", "outlet", "outlet" }, { "xtdev", "1", "8" } );
        xt1121DCDU_test app( "dcdu" );
        REQUIRE( app.configureOutlets( file ) == 0 );
        REQUIRE( app.channelOutlets( "a" ) == std::vector<size_t>( { 0 } ) );
        REQUIRE( app.channelOutlets( "b" ) == std::vector<size_t>( { 7 } ) );
    }

    SECTION( "no channel sections" )
    {
        mx::app::writeConfigFile( file, { "device" }, { "name" }, { "xtdev" } );
        xt1121DCDU_test app( "dcdu" );
        REQUIRE( app.configureOutlets( file ) == OUTLET_E_NOCHANNELS );
        REQUIRE( app.numChannels() == 0 );
    }

    SECTION( "unused section without an outlet keyword" )
    {
        mx::app::writeConfigFile( file, { "device", "notachannel" }, { "name", "foo" }, { "xtdev", "1" } );
        xt1121DCDU_test app( "dcdu" );
        REQUIRE( app.configureOutlets( file ) == OUTLET_E_NOVALIDCH );
        REQUIRE( app.numChannels() == 0 );
    }

    SECTION( "outlet 0 is invalid for a 1-based device" )
    {
        mx::app::writeConfigFile( file, { "device", "a" }, { "name", "outlet" }, { "xtdev", "0" } );
        xt1121DCDU_test app( "dcdu" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    SECTION( "outlet beyond the device range is invalid" )
    {
        mx::app::writeConfigFile( file, { "device", "a" }, { "name", "outlet" }, { "xtdev", "10" } );
        xt1121DCDU_test app( "dcdu" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    SECTION( "onOrder size mismatch" )
    {
        mx::app::writeConfigFile(
            file, { "device", "a", "a" }, { "name", "outlets", "onOrder" }, { "xtdev", "1,2", "0" } );
        xt1121DCDU_test app( "dcdu" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    SECTION( "offDelays size mismatch" )
    {
        mx::app::writeConfigFile(
            file, { "device", "a", "a" }, { "name", "outlets", "offDelays" }, { "xtdev", "1,2", "0,1,2" } );
        xt1121DCDU_test app( "dcdu" );
        REQUIRE( app.configureOutlets( file ) == -1 );
    }

    remove( file.c_str() );
}

/// Verify the xt1121Ctrl channel-name helper.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU xtChannelName", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::xtChannelName(0);
    #endif
    // clang-format on

    xt1121DCDU_test app( "dcdu" );

    REQUIRE( app.xtChannelName( 0 ) == "ch00" );
    REQUIRE( app.xtChannelName( 1 ) == "ch01" );
    REQUIRE( app.xtChannelName( 7 ) == "ch07" );
    REQUIRE( app.xtChannelName( 9 ) == "ch09" );
    REQUIRE( app.xtChannelName( 10 ) == "ch10" );
    REQUIRE( app.xtChannelName( 15 ) == "ch15" );
    REQUIRE( app.xtChannelName( 16 ) == "ch16" );

    REQUIRE( app.xtChannelName( -1 ) == "" );
    REQUIRE( app.xtChannelName( 17 ) == "" );
    REQUIRE( app.xtChannelName( 100 ) == "" );
}

/// Verify the outlet-number to INDI-property mapping.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU xtChannelProperty", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::xtChannelProperty(0);
    #endif
    // clang-format on

    xt1121DCDU_test app( "dcdu" );

    std::vector<pcf::IndiProperty *> props;
    for( int n = 0; n < 8; ++n )
    {
        pcf::IndiProperty *ip = app.xtChannelProperty( n );
        REQUIRE( ip != nullptr );
        for( size_t k = 0; k < props.size(); ++k )
        {
            REQUIRE( props[k] != ip ); // each outlet has its own property
        }
        props.push_back( ip );
    }

    REQUIRE( app.xtChannelProperty( -1 ) == nullptr );
    REQUIRE( app.xtChannelProperty( 8 ) == nullptr );
    REQUIRE( app.xtChannelProperty( 16 ) == nullptr );

    // The set callback for outlet n stores into the property returned for outlet n.
    for( int n = 0; n < 8; ++n )
    {
        REQUIRE( app.setCallBack( n, xtChannel( "xtdev", app.xtChannelName( n ), 1 ) ) == 0 );
        REQUIRE( app.xtChannelProperty( n )->getName() == app.xtChannelName( n ) );
    }
}

/// Verify updateOutletState maps the xt1121Ctrl `current` value to outlet states.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU updateOutletState", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::updateOutletState(0);
    #endif
    // clang-format on

    xt1121DCDU_test app( "dcdu" );

    SECTION( "no current element leaves the state unknown" )
    {
        app.m_outletStates[2] = OUTLET_STATE_ON;
        REQUIRE( app.updateOutletState( 2 ) == 0 );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_UNKNOWN );
    }

    SECTION( "current 0 is off, 1 is on, anything else is unknown" )
    {
        *app.xtChannelProperty( 3 ) = xtChannel( "xtdev", "ch03", 0 );
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_OFF );

        *app.xtChannelProperty( 3 ) = xtChannel( "xtdev", "ch03", 1 );
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_ON );

        *app.xtChannelProperty( 3 ) = xtChannel( "xtdev", "ch03", 2 );
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_UNKNOWN );

        *app.xtChannelProperty( 3 ) = xtChannel( "xtdev", "ch03", -1 );
        REQUIRE( app.updateOutletState( 3 ) == 0 );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_UNKNOWN );
    }

    SECTION( "only the requested outlet changes" )
    {
        *app.xtChannelProperty( 5 ) = xtChannel( "xtdev", "ch05", 1 );
        REQUIRE( app.updateOutletState( 5 ) == 0 );
        for( int n = 0; n < 8; ++n )
        {
            if( n == 5 )
            {
                REQUIRE( app.outletState( n ) == OUTLET_STATE_ON );
            }
            else
            {
                REQUIRE( app.outletState( n ) == OUTLET_STATE_UNKNOWN );
            }
        }
    }

    SECTION( "invalid outlet numbers are rejected" )
    {
        REQUIRE( app.updateOutletState( -1 ) == -1 );
        REQUIRE( app.updateOutletState( 8 ) == -1 );
    }

    SECTION( "updateOutletStates updates all outlets" )
    {
        for( int n = 0; n < 8; ++n )
        {
            *app.xtChannelProperty( n ) = xtChannel( "xtdev", app.xtChannelName( n ), n % 2 );
        }
        REQUIRE( app.updateOutletStates() == 0 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == ( n % 2 ? OUTLET_STATE_ON : OUTLET_STATE_OFF ) );
        }
    }
}

/// Verify the xt1121Ctrl set callbacks store the property and update the matching outlet state.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU channel set callbacks", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::setCallBack_ip_ch0(pcf::IndiProperty());
    xt1121DCDU::setCallBack_ip_ch1(pcf::IndiProperty());
    xt1121DCDU::setCallBack_ip_ch2(pcf::IndiProperty());
    xt1121DCDU::setCallBack_ip_ch3(pcf::IndiProperty());
    xt1121DCDU::setCallBack_ip_ch4(pcf::IndiProperty());
    xt1121DCDU::setCallBack_ip_ch5(pcf::IndiProperty());
    xt1121DCDU::setCallBack_ip_ch6(pcf::IndiProperty());
    xt1121DCDU::setCallBack_ip_ch7(pcf::IndiProperty());
    #endif
    // clang-format on

    xt1121DCDU_test app( "dcdu" );

    SECTION( "each callback updates only its own outlet" )
    {
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.setCallBack( n, xtChannel( "xtdev", app.xtChannelName( n ), 1 ) ) == 0 );
            for( int k = 0; k < 8; ++k )
            {
                REQUIRE( app.outletState( k ) == ( k <= n ? OUTLET_STATE_ON : OUTLET_STATE_UNKNOWN ) );
            }
        }

        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.setCallBack( n, xtChannel( "xtdev", app.xtChannelName( n ), 0 ) ) == 0 );
            REQUIRE( app.outletState( n ) == OUTLET_STATE_OFF );
        }
    }

    SECTION( "the received property is stored" )
    {
        REQUIRE( app.setCallBack_ip_ch4( xtChannel( "xtdev", "ch12", 1 ) ) == 0 );
        pcf::IndiProperty *ip = app.xtChannelProperty( 4 );
        REQUIRE( ip->getDevice() == "xtdev" );
        REQUIRE( ip->getName() == "ch12" );
        REQUIRE( ip->find( "current" ) );
        REQUIRE( ( *ip )["current"].get<int>() == 1 );
        REQUIRE( app.outletState( 4 ) == OUTLET_STATE_ON );
    }

    SECTION( "a property without current sets the outlet unknown" )
    {
        REQUIRE( app.setCallBack_ip_ch6( xtChannel( "xtdev", "ch06", 1 ) ) == 0 );
        REQUIRE( app.outletState( 6 ) == OUTLET_STATE_ON );

        REQUIRE( app.setCallBack_ip_ch6( xtChannel( "xtdev", "ch06", 1, false ) ) == 0 );
        REQUIRE( app.outletState( 6 ) == OUTLET_STATE_UNKNOWN );
    }

    SECTION( "the callbacks do not validate device or name" )
    {
        // The set callbacks copy whatever they receive; validation is left to the INDI dispatch by device.name key.
        REQUIRE( app.setCallBack_ip_ch0( xtChannel( "other", "wrong", 1 ) ) == 0 );
        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_ON );
    }
}

/// Verify turnOutletOn/turnOutletOff reject bad outlets, and fail without an INDI driver.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU turnOutletOn and turnOutletOff", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::turnOutletOn(0);
    xt1121DCDU::turnOutletOff(0);
    #endif
    // clang-format on

    xt1121DCDU_test app( "dcdu" );

    REQUIRE( app.turnOutletOn( -1 ) == -1 );
    REQUIRE( app.turnOutletOn( 8 ) == -1 );
    REQUIRE( app.turnOutletOff( -1 ) == -1 );
    REQUIRE( app.turnOutletOff( 8 ) == -1 );

    // Valid outlets go to sendNewProperty, which fails because no INDI driver exists in the test.
    *app.xtChannelProperty( 0 ) = xtChannel( "xtdev", "ch00", 0 );
    REQUIRE( app.turnOutletOn( 0 ) == -1 );
    REQUIRE( app.turnOutletOff( 0 ) == -1 );

    // The outlet state is not changed by a request, only by the device's reply.
    REQUIRE( app.updateOutletState( 0 ) == 0 );
    REQUIRE( app.outletState( 0 ) == OUTLET_STATE_OFF );

    // The INDI mutex is released after each call.
    std::unique_lock<std::mutex> lock( app.m_indiMutex, std::try_to_lock );
    REQUIRE( lock.owns_lock() );
}

/// Verify channel state aggregation from outlet states.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU channelState", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    dev::outletController<xt1121DCDU>::channelState("");
    #endif
    // clang-format on

    std::string file = "/tmp/xt1121DCDU_test_chstate.conf";
    writeStdConfig( file );

    xt1121DCDU_test app( "dcdu" );
    app.configure( file );

    REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_UNKNOWN );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_UNKNOWN );

    // camera is outlets 1 and 2
    REQUIRE( app.setCallBack( 1, xtChannel( "xtdev", "ch09", 1 ) ) == 0 );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_INTERMEDIATE );

    REQUIRE( app.setCallBack( 2, xtChannel( "xtdev", "ch10", 1 ) ) == 0 );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_ON );

    REQUIRE( app.setCallBack( 1, xtChannel( "xtdev", "ch09", 0 ) ) == 0 );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_INTERMEDIATE );

    REQUIRE( app.setCallBack( 2, xtChannel( "xtdev", "ch10", 0 ) ) == 0 );
    REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_OFF );

    // pump is outlet 0, unaffected
    REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_UNKNOWN );
    REQUIRE( app.setCallBack( 0, xtChannel( "xtdev", "ch08", 1 ) ) == 0 );
    REQUIRE( app.channelState( "pump" ) == OUTLET_STATE_ON );

    remove( file.c_str() );
}

/// Verify turnChannelOn/turnChannelOff skip channels already in the requested state, and otherwise send requests.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU turnChannelOn and turnChannelOff", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    dev::outletController<xt1121DCDU>::turnChannelOn("");
    dev::outletController<xt1121DCDU>::turnChannelOff("");
    #endif
    // clang-format on

    std::string file = "/tmp/xt1121DCDU_test_turnch.conf";
    writeStdConfig( file );

    xt1121DCDU_test app( "dcdu" );
    app.configure( file );

    SECTION( "already on / already off is a no-op" )
    {
        REQUIRE( app.setCallBack( 1, xtChannel( "xtdev", "ch09", 1 ) ) == 0 );
        REQUIRE( app.setCallBack( 2, xtChannel( "xtdev", "ch10", 1 ) ) == 0 );
        REQUIRE( app.turnChannelOn( "camera" ) == 0 );

        REQUIRE( app.setCallBack( 0, xtChannel( "xtdev", "ch08", 0 ) ) == 0 );
        REQUIRE( app.turnChannelOff( "pump" ) == 0 );
    }

    SECTION( "a change requires sending to the xt1121Ctrl, which fails without INDI" )
    {
        REQUIRE( app.setCallBack( 0, xtChannel( "xtdev", "ch08", 0 ) ) == 0 );
        REQUIRE( app.turnChannelOn( "pump" ) == -1 );

        REQUIRE( app.setCallBack( 0, xtChannel( "xtdev", "ch08", 1 ) ) == 0 );
        REQUIRE( app.turnChannelOff( "pump" ) == -1 );

        // intermediate channels are turned on or off
        REQUIRE( app.setCallBack( 1, xtChannel( "xtdev", "ch09", 1 ) ) == 0 );
        REQUIRE( app.setCallBack( 2, xtChannel( "xtdev", "ch10", 0 ) ) == 0 );
        REQUIRE( app.turnChannelOn( "camera" ) == -1 );
        REQUIRE( app.turnChannelOff( "camera" ) == -1 );
    }

    remove( file.c_str() );
}

/// Verify the outletController channel new-property callback.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU newCallBack_channels", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    dev::outletController<xt1121DCDU>::newCallBack_channels(pcf::IndiProperty());
    dev::outletController<xt1121DCDU>::st_newCallBack_channels(nullptr, pcf::IndiProperty());
    #endif
    // clang-format on

    std::string file = "/tmp/xt1121DCDU_test_newch.conf";
    writeStdConfig( file );

    xt1121DCDU_test app( "dcdu" );
    app.configure( file );

    REQUIRE( app.setCallBack( 0, xtChannel( "xtdev", "ch08", 1 ) ) == 0 ); // pump on

    SECTION( "rejected when not READY" )
    {
        REQUIRE( app.state() != stateCodes::READY );
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "Off" ) ) == -1 );
    }

    SECTION( "READY" )
    {
        app.state( stateCodes::READY );

        // no change needed
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "On" ) ) == 0 );
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "on" ) ) == 0 );
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "", "ON" ) ) == 0 );

        // unrecognized or empty targets are ignored
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "bogus" ) ) == 0 );
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "" ) ) == 0 );

        // a change goes to turnChannelOff, which fails here because there is no INDI driver
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "off" ) ) == -1 );
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "", "Off" ) ) == -1 );

        // target takes precedence over state
        REQUIRE( app.newCallBack_channels( channelRequest( "dcdu", "pump", "On", "Off" ) ) == 0 );

        // the static wrapper dispatches to the member
        REQUIRE( dev::outletController<xt1121DCDU>::st_newCallBack_channels(
                     static_cast<xt1121DCDU *>( &app ), channelRequest( "dcdu", "pump", "On" ) ) == 0 );

        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_ON );
    }

    remove( file.c_str() );
}

/// Verify appStartup registers the xt1121Ctrl channel properties and the outletController properties.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU appStartup", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::appStartup();
    dev::outletController<xt1121DCDU>::appStartup();
    #endif
    // clang-format on

    std::string file = "/tmp/xt1121DCDU_test_startup.conf";

    SECTION( "configured channel numbers are registered as set properties" )
    {
        writeStdConfig( file );

        xt1121DCDU_test app( "dcdu" );
        app.configure( file );

        REQUIRE( app.appStartup() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );

        for( int n = 0; n < 8; ++n )
        {
            pcf::IndiProperty *ip = app.xtChannelProperty( n );
            REQUIRE( ip->getDevice() == "xtdev" );
            REQUIRE( ip->getName() == app.xtChannelName( 8 + n ) );
            REQUIRE( app.m_indiSetCallBacks.count( "xtdev." + app.xtChannelName( 8 + n ) ) == 1 );
        }
        REQUIRE( app.m_indiSetCallBacks.size() == 8 );

        // The outlet states property has 1-based elements.
        REQUIRE( app.m_indiP_outletStates.getName() == "outlet" );
        REQUIRE( app.m_indiP_outletStates.getDevice() == "dcdu" );
        REQUIRE( app.m_indiP_outletStates.getNumElements() == 8 );
        REQUIRE_FALSE( app.m_indiP_outletStates.find( "0" ) );
        REQUIRE( app.m_indiP_outletStates.find( "1" ) );
        REQUIRE( app.m_indiP_outletStates.find( "8" ) );

        // Channel properties with state and target elements.
        REQUIRE( app.m_channels["pump"].m_indiP_prop.getName() == "pump" );
        REQUIRE( app.m_channels["pump"].m_indiP_prop.find( "state" ) );
        REQUIRE( app.m_channels["pump"].m_indiP_prop.find( "target" ) );
        REQUIRE( app.m_indiNewCallBacks.count( "dcdu.pump" ) == 1 );
        REQUIRE( app.m_indiNewCallBacks.count( "dcdu.camera" ) == 1 );

        // Static channel information.
        REQUIRE( app.m_indiP_chOutlets["pump"].get() == "0" );
        REQUIRE( app.m_indiP_chOutlets["camera"].get() == "1,2" );
        REQUIRE( app.m_indiP_chOnDelays["camera"].get<double>() == Approx( 150 ) );
        REQUIRE( app.m_indiP_chOffDelays["camera"].get<double>() == Approx( 345 ) );
        REQUIRE( app.m_indiP_chOnDelays["pump"].get<double>() == Approx( 0 ) );
        REQUIRE( app.m_indiP_stateTimes["pump"].get<int>() == 0 );
    }

    SECTION( "default channel numbers" )
    {
        mx::app::writeConfigFile( file, { "device", "pump" }, { "name", "outlet" }, { "xtdev", "1" } );

        xt1121DCDU_test app( "dcdu" );
        app.configure( file );

        REQUIRE( app.appStartup() == 0 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.xtChannelProperty( n )->getName() == app.xtChannelName( n ) );
        }
    }

    SECTION( "wrong number of channel numbers fails" )
    {
        mx::app::writeConfigFile( file,
                                  { "device", "device", "pump" },
                                  { "name", "channelNumbers", "outlet" },
                                  { "xtdev", "0,1,2,3,4,5,6", "1" } );

        xt1121DCDU_test app( "dcdu" );
        app.configure( file );
        REQUIRE( app.m_channelNumbers.size() == 7 );

        REQUIRE( app.appStartup() == -1 );
        REQUIRE( app.m_indiSetCallBacks.size() == 0 );
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
    }

    SECTION( "duplicate channel numbers fail to register" )
    {
        mx::app::writeConfigFile( file,
                                  { "device", "device", "pump" },
                                  { "name", "channelNumbers", "outlet" },
                                  { "xtdev", "0,0,2,3,4,5,6,7", "1" } );

        xt1121DCDU_test app( "dcdu" );
        app.configure( file );

        REQUIRE( app.appStartup() == -1 );
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
    }

    remove( file.c_str() );
}

/// Verify the appLogic state machine.
/**
 * \ingroup xt1121DCDU_unit_test
 */
TEST_CASE( "xt1121DCDU appLogic", "[xt1121DCDU]" )
{
    // clang-format off
    #ifdef XT1121DCDU_TEST_DOXYGEN_REF
    xt1121DCDU::appLogic();
    xt1121DCDU::appShutdown();
    dev::outletController<xt1121DCDU>::updateINDI();
    #endif
    // clang-format on

    std::string file = "/tmp/xt1121DCDU_test_logic.conf";
    writeStdConfig( file );

    xt1121DCDU_test app( "dcdu" );
    app.configure( file );

    SECTION( "POWERON goes to READY and updates outlet states from the stored properties" )
    {
        *app.xtChannelProperty( 0 ) = xtChannel( "xtdev", "ch08", 1 );
        *app.xtChannelProperty( 1 ) = xtChannel( "xtdev", "ch09", 0 );

        app.state( stateCodes::POWERON );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );

        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_ON );
        REQUIRE( app.outletState( 1 ) == OUTLET_STATE_OFF );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_UNKNOWN );
        REQUIRE( app.updateINDI() == 0 ); // no driver: no-op
    }

    SECTION( "READY with the INDI mutex held skips the update" )
    {
        app.state( stateCodes::READY );
        *app.xtChannelProperty( 0 ) = xtChannel( "xtdev", "ch08", 1 );

        { // mutex scope
            std::lock_guard<std::mutex> guard( app.m_indiMutex );
            REQUIRE( app.appLogic() == 0 );
        }
        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_UNKNOWN );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_ON );
    }

    SECTION( "any other state is a failure" )
    {
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == -1 );
        REQUIRE( app.state() == stateCodes::FAILURE );
    }

    SECTION( "appShutdown does nothing" )
    {
        REQUIRE( app.appShutdown() == 0 );
    }

    remove( file.c_str() );
}

} // namespace xt1121DCDUTest

} // namespace libXWCTest
