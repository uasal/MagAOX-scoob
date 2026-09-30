/** \file kTracker_test.cpp
 * \brief Catch2 tests for the kTracker app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup kTracker_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <string>

#include "../kTracker.hpp"

// Included after the app header so the callback bodies stay live.  The device/name validation
// macros still work because both callbacks return 0 for a property with no elements.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup kTracker_unit_test kTracker Unit Tests
 * \brief Unit tests for the kTracker application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `kTracker` unit tests.
/** \ingroup kTracker_unit_test
 */
namespace kTrackerTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing kTracker internals.
class kTracker_test : public kTracker
{
  public:
    /// Construct a harness with the given device name.
    explicit kTracker_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( tracking );

        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_teldata, tcsi, teldata );
    }

    using kTracker::m_devName;
    using kTracker::m_haveZD;
    using kTracker::m_indiP_kpos;
    using kTracker::m_indiP_teldata;
    using kTracker::m_indiP_tracking;
    using kTracker::m_lastUpdate;
    using kTracker::m_sign;
    using kTracker::m_tcsDevName;
    using kTracker::m_tracking;
    using kTracker::m_updateInterval;
    using kTracker::m_zd;
    using kTracker::m_zero;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Check whether a NEW property callback is registered under a unique key.
    /**
     * \returns true if the key is registered, false otherwise
     */
    bool hasNewCallBack( const std::string &key /**< [in] the `device.name` key */ )
    {
        return m_indiNewCallBacks.count( key ) > 0;
    }

    /// Check whether a SET property callback is registered under a unique key.
    /**
     * \returns true if the key is registered, false otherwise
     */
    bool hasSetCallBack( const std::string &key /**< [in] the `device.name` key */ )
    {
        return m_indiSetCallBacks.count( key ) > 0;
    }
};

/// Build a tracking toggle request for the harness device.
/**
 * \returns the INDI switch property carrying the `toggle` element
 */
pcf::IndiProperty trackingRequest( const std::string                &device, /**< [in] INDI device name */
                                   pcf::IndiElement::SwitchStateType state   /**< [in] requested toggle state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( "tracking" );
    ip.add( pcf::IndiElement( "toggle", state ) );
    return ip;
}

/// Build a `teldata` update carrying a zenith distance string.
/**
 * \returns the INDI number property carrying the `zd` element
 */
pcf::IndiProperty teldataUpdate( const std::string &device, /**< [in] TCS device name */
                                 const std::string &zd      /**< [in] zenith distance value as text */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( "teldata" );
    ip.add( pcf::IndiElement( "zd" ) );
    ip["zd"].setValue( zd );
    return ip;
}
/// \endcond

/// Verify the kTracker INDI callback validators accept only the expected properties.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker INDI callbacks validate device and property names", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::newCallBack_m_indiP_tracking( pcf::IndiProperty() );
    kTracker::setCallBack_m_indiP_teldata( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( kTracker, tracking );
    XWCTEST_INDI_SET_CALLBACK( kTracker, m_indiP_teldata, tcsi, teldata );
}

/// Verify the kTracker configuration defaults.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker configuration defaults", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::setupConfig();
    kTracker::loadConfig();
    kTracker::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/kTracker_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

    kTracker_test app( "ktrack" );
    app.configure( fname );

    REQUIRE( app.shutdown() == 0 );
    REQUIRE( app.m_zero == Approx( 0.0 ) );
    REQUIRE( app.m_sign == 1 );
    REQUIRE( app.m_devName == "stagek" );
    REQUIRE( app.m_tcsDevName == "tcsi" );
    REQUIRE( app.m_updateInterval == Approx( 10.0 ) );
    REQUIRE_FALSE( app.m_tracking );
    REQUIRE_FALSE( app.m_haveZD );

    std::remove( fname.c_str() );
}

/// Verify kTracker configuration overrides are loaded.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker configuration overrides", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::setupConfig();
    kTracker::loadConfig();
    #endif
    // clang-format on

    const std::string fname = "/tmp/kTracker_test_overrides.conf";
    mx::app::writeConfigFile( fname,
                              { "k", "k", "k", "tcs", "tracking" },
                              { "zero", "sign", "devName", "devName", "updateInterval" },
                              { "-40.5", "-1", "stagekx", "tcsx", "2.5" } );

    kTracker_test app( "ktrack" );
    app.configure( fname );

    REQUIRE( app.shutdown() == 0 );
    REQUIRE( app.m_zero == Approx( -40.5 ) );
    REQUIRE( app.m_sign == -1 );
    REQUIRE( app.m_devName == "stagekx" );
    REQUIRE( app.m_tcsDevName == "tcsx" );
    REQUIRE( app.m_updateInterval == Approx( 2.5 ) );

    std::remove( fname.c_str() );
}

/// Verify `appStartup()` builds and registers the INDI properties.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker appStartup creates and registers properties", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::appStartup();
    kTracker::appShutdown();
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    app.m_devName    = "stagek2";
    app.m_tcsDevName = "tcs2";

    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.state() == stateCodes::READY );

    REQUIRE( app.m_indiP_tracking.getDevice() == "ktrack" );
    REQUIRE( app.m_indiP_tracking.getName() == "tracking" );
    REQUIRE( app.m_indiP_tracking.find( "toggle" ) );
    REQUIRE( app.m_indiP_tracking["toggle"].getSwitchState() == pcf::IndiElement::Off );
    REQUIRE( app.hasNewCallBack( "ktrack.tracking" ) );

    REQUIRE( app.m_indiP_teldata.getDevice() == "tcs2" );
    REQUIRE( app.m_indiP_teldata.getName() == "teldata" );
    REQUIRE( app.hasSetCallBack( "tcs2.teldata" ) );

    REQUIRE( app.m_indiP_kpos.getDevice() == "stagek2" );
    REQUIRE( app.m_indiP_kpos.getName() == "position" );
    REQUIRE( app.m_indiP_kpos.find( "target" ) );

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify the tracking toggle callback starts and stops tracking.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker tracking toggle callback sets tracking state", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::newCallBack_m_indiP_tracking( pcf::IndiProperty() );
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    REQUIRE( app.appStartup() == 0 );

    app.m_lastUpdate = 123.0;
    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "ktrack", pcf::IndiElement::On ) ) == 0 );
    REQUIRE( app.m_tracking );
    REQUIRE( app.m_lastUpdate == 0 );

    app.m_lastUpdate = 456.0;
    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "ktrack", pcf::IndiElement::Off ) ) == 0 );
    REQUIRE_FALSE( app.m_tracking );
    REQUIRE( app.m_lastUpdate == 0 );
}

/// Verify the tracking callback ignores requests without a toggle element and wrong devices.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker tracking callback ignores malformed requests", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::newCallBack_m_indiP_tracking( pcf::IndiProperty() );
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    REQUIRE( app.appStartup() == 0 );

    pcf::IndiProperty noToggle( pcf::IndiProperty::Switch );
    noToggle.setDevice( "ktrack" );
    noToggle.setName( "tracking" );
    noToggle.add( pcf::IndiElement( "other", pcf::IndiElement::On ) );

    REQUIRE( app.newCallBack_m_indiP_tracking( noToggle ) == 0 );
    REQUIRE_FALSE( app.m_tracking );

    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "other", pcf::IndiElement::On ) ) == -1 );
    REQUIRE_FALSE( app.m_tracking );
}

/// Verify the teldata callback stores a valid zenith distance.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker teldata callback stores zenith distance", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::setCallBack_m_indiP_teldata( pcf::IndiProperty() );
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.setCallBack_m_indiP_teldata( teldataUpdate( "tcsi", "32.25" ) ) == 0 );
    REQUIRE( app.m_haveZD );
    REQUIRE( app.m_zd == Approx( 32.25 ) );

    REQUIRE( app.setCallBack_m_indiP_teldata( teldataUpdate( "tcsi", "-5.5" ) ) == 0 );
    REQUIRE( app.m_zd == Approx( -5.5 ) );
}

/// Verify the teldata callback rejects wrong sources and ignores updates without `zd`.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker teldata callback ignores invalid updates", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::setCallBack_m_indiP_teldata( pcf::IndiProperty() );
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.setCallBack_m_indiP_teldata( teldataUpdate( "tcsx", "10" ) ) == -1 );
    REQUIRE_FALSE( app.m_haveZD );

    pcf::IndiProperty noZD( pcf::IndiProperty::Number );
    noZD.setDevice( "tcsi" );
    noZD.setName( "teldata" );
    noZD.add( pcf::IndiElement( "pa" ) );
    noZD["pa"].setValue( std::string( "12" ) );

    REQUIRE( app.setCallBack_m_indiP_teldata( noZD ) == 0 );
    REQUIRE_FALSE( app.m_haveZD );
    REQUIRE( app.m_zd == Approx( 0.0 ) );
}

/// Verify `appLogic()` resets the update timestamp while tracking is disabled.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker appLogic idles while not tracking", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::appLogic();
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    REQUIRE( app.appStartup() == 0 );

    app.m_tracking   = false;
    app.m_haveZD     = true;
    app.m_lastUpdate = 99.0;

    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_lastUpdate == 0 );
}

/// Verify `appLogic()` waits for a zenith distance before commanding the stage.
/**
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker appLogic waits for a zenith distance", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::appLogic();
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    REQUIRE( app.appStartup() == 0 );

    app.m_tracking   = true;
    app.m_haveZD     = false;
    app.m_lastUpdate = 0;

    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_lastUpdate == 0 );
}

/// Verify `appLogic()` dispatches an update and then honors the update interval.
/**
 * With no INDI driver `sendNewProperty()` fails and is logged, but the update bookkeeping still advances.
 *
 * \ingroup kTracker_unit_test
 */
TEST_CASE( "kTracker appLogic dispatches updates at the configured interval", "[kTracker]" )
{
    // clang-format off
    #ifdef KTRACKER_TEST_DOXYGEN_REF
    kTracker::appLogic();
    #endif
    // clang-format on

    kTracker_test app( "ktrack" );
    REQUIRE( app.appStartup() == 0 );

    app.m_zero           = -40;
    app.m_sign           = 1;
    app.m_updateInterval = 1000;
    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "ktrack", pcf::IndiElement::On ) ) == 0 );
    REQUIRE( app.setCallBack_m_indiP_teldata( teldataUpdate( "tcsi", "30" ) ) == 0 );

    REQUIRE( app.appLogic() == 0 );
    double first = app.m_lastUpdate;
    REQUIRE( first > 0 );

    // Within the update interval nothing is dispatched.
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_lastUpdate == first );

    // Turning tracking off and on re-arms an immediate update.
    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "ktrack", pcf::IndiElement::Off ) ) == 0 );
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_lastUpdate == 0 );

    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "ktrack", pcf::IndiElement::On ) ) == 0 );
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_lastUpdate >= first );
}

} // namespace kTrackerTest

} // namespace libXWCTest
