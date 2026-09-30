/** \file hwpTracker_test.cpp
 * \brief Catch2 tests for the hwpTracker app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup hwpTracker_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

#include "../hwpTracker.hpp"

// Included after the app header so the callback bodies stay live.  The device/name validation
// macros still work because every callback returns 0 for a property with no elements.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup hwpTracker_unit_test hwpTracker Unit Tests
 * \brief Unit tests for the hwpTracker application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `hwpTracker` unit tests.
/** \ingroup hwpTracker_unit_test
 */
namespace hwpTrackerTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing hwpTracker internals.
class hwpTracker_test : public hwpTracker
{
  public:
    /// Construct a harness with the given device name.
    explicit hwpTracker_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( tracking );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_hwpSetPos, hwp_position );

        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_teldata, tcsi, teldata );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_stagePolRot, stagepolrot, position );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_stagePolRotFsm, stagepolrot, fsm );
    }

    using hwpTracker::m_altitude;
    using hwpTracker::m_devName;
    using hwpTracker::m_hwpActualPos;
    using hwpTracker::m_hwpCurPos;
    using hwpTracker::m_hwpPosName;
    using hwpTracker::m_hwpSetPos;
    using hwpTracker::m_hwpTrackingOffset;
    using hwpTracker::m_indiP_hwpActualPos;
    using hwpTracker::m_indiP_hwpPosName;
    using hwpTracker::m_indiP_hwpSetPos;
    using hwpTracker::m_indiP_hwpStagePos;
    using hwpTracker::m_indiP_hwpTrackingOffset;
    using hwpTracker::m_indiP_stagePolRot;
    using hwpTracker::m_indiP_stagePolRotFsm;
    using hwpTracker::m_indiP_teldata;
    using hwpTracker::m_indiP_tracking;
    using hwpTracker::m_parang;
    using hwpTracker::m_pupilOffset;
    using hwpTracker::m_sign;
    using hwpTracker::m_tcsDevName;
    using hwpTracker::m_tracking;
    using hwpTracker::m_updateInterval;
    using hwpTracker::m_zero;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();

        // Keep telemetry records out of the queue so nothing is written at destruction.
        m_tel.m_logLevel = logPrio::LOG_INFO;
    }

    /// Point the telemetry logger at a scratch directory and run `appStartup()`.
    /** The telemetry log level stays at INFO, so telemetry records are dropped and no file is written.
     *
     * \returns the value returned by `appStartup()`
     */
    int startup()
    {
        std::filesystem::create_directories( "/tmp/hwpTracker_test_telem" );
        m_tel.logPath( "/tmp/hwpTracker_test_telem" );
        m_tel.writePause( 1000000 );
        return appStartup();
    }

    /// Build the outbound stage position property as `appStartup()` does, without starting telemetry.
    void setupStageProperty()
    {
        m_indiP_hwpStagePos = pcf::IndiProperty( pcf::IndiProperty::Number );
        m_indiP_hwpStagePos.setDevice( m_devName );
        m_indiP_hwpStagePos.setName( "position" );
        m_indiP_hwpStagePos.add( pcf::IndiElement( "target" ) );
    }

    /// Get the most recent stage target written by `updateHwpPos()`.
    /**
     * \returns the `target` element of the outbound stage position property
     */
    float stageTarget()
    {
        return m_indiP_hwpStagePos["target"].get<float>();
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

/// Build a single-element INDI property carrying a text value.
/**
 * \returns the INDI property
 */
pcf::IndiProperty makeProp( pcf::IndiProperty::Type type,   /**< [in] INDI property type */
                            const std::string      &device, /**< [in] INDI device name */
                            const std::string      &name,   /**< [in] INDI property name */
                            const std::string      &el,     /**< [in] element name */
                            const std::string      &value   /**< [in] element value as text */
)
{
    pcf::IndiProperty ip( type );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el ) );
    ip[el].setValue( value );
    return ip;
}

/// Build a tracking toggle request.
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
/// \endcond

/// Verify the hwpTracker INDI callback validators accept only the expected properties.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker INDI callbacks validate device and property names", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::newCallBack_m_indiP_tracking( pcf::IndiProperty() );
    hwpTracker::newCallBack_m_indiP_hwpSetPos( pcf::IndiProperty() );
    hwpTracker::setCallBack_m_indiP_teldata( pcf::IndiProperty() );
    hwpTracker::setCallBack_m_indiP_stagePolRot( pcf::IndiProperty() );
    hwpTracker::setCallBack_m_indiP_stagePolRotFsm( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( hwpTracker, tracking );
    XWCTEST_INDI_ARBNEW_CALLBACK( hwpTracker, newCallBack_m_indiP_hwpSetPos, hwp_position );
    XWCTEST_INDI_SET_CALLBACK( hwpTracker, m_indiP_teldata, tcsi, teldata );
    XWCTEST_INDI_SET_CALLBACK( hwpTracker, m_indiP_stagePolRot, stagepolrot, position );
    XWCTEST_INDI_SET_CALLBACK( hwpTracker, m_indiP_stagePolRotFsm, stagepolrot, fsm );
}

/// Verify the hwpTracker configuration defaults.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker configuration defaults", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::setupConfig();
    hwpTracker::loadConfig();
    hwpTracker::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/hwpTracker_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

    hwpTracker_test app( "hwptrack" );
    app.configure( fname );

    REQUIRE( app.shutdown() == 0 );
    REQUIRE( app.m_zero == Approx( 360.0 ) );
    REQUIRE( app.m_sign == -1 );
    REQUIRE( app.m_devName == "stagepolrot" );
    REQUIRE( app.m_tcsDevName == "tcsi" );
    REQUIRE( app.m_updateInterval == Approx( 10.0 ) );
    REQUIRE( app.m_pupilOffset == Approx( 0.0 ) );
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
    REQUIRE_FALSE( app.m_tracking );

    std::remove( fname.c_str() );
}

/// Verify hwpTracker configuration overrides, including the telemeter interval.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker configuration overrides", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::setupConfig();
    hwpTracker::loadConfig();
    #endif
    // clang-format on

    const std::string fname = "/tmp/hwpTracker_test_overrides.conf";
    mx::app::writeConfigFile( fname,
                              { "hwp", "hwp", "hwp", "tcs", "tracking", "tracking", "telemeter" },
                              { "zero", "sign", "devName", "devName", "updateInterval", "pupilOffset", "maxInterval" },
                              { "180", "1", "stagehwp", "tcsx", "2.5", "-7.25", "30" } );

    hwpTracker_test app( "hwptrack" );
    app.configure( fname );

    REQUIRE( app.shutdown() == 0 );
    REQUIRE( app.m_zero == Approx( 180.0 ) );
    REQUIRE( app.m_sign == 1 );
    REQUIRE( app.m_devName == "stagehwp" );
    REQUIRE( app.m_tcsDevName == "tcsx" );
    REQUIRE( app.m_updateInterval == Approx( 2.5 ) );
    REQUIRE( app.m_pupilOffset == Approx( -7.25 ) );
    REQUIRE( app.m_maxInterval == Approx( 30.0 ) );

    std::remove( fname.c_str() );
}

/// Verify `getHwpStatus()` maps the current HWP angle to named polarization states.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker getHwpStatus maps angles to position names", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::getHwpStatus();
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );

    app.m_hwpCurPos = 0;
    REQUIRE( app.getHwpStatus() == "Qplus" );

    app.m_hwpCurPos = -0.4;
    REQUIRE( app.getHwpStatus() == "Qplus" );

    app.m_hwpCurPos = 45.3;
    REQUIRE( app.getHwpStatus() == "Qminus" );

    app.m_hwpCurPos = 22.5;
    REQUIRE( app.getHwpStatus() == "Uplus" );

    app.m_hwpCurPos = 67.2;
    REQUIRE( app.getHwpStatus() == "Uminus" );

    app.m_hwpCurPos = 0.6;
    REQUIRE( app.getHwpStatus() == "Unknown" );

    app.m_hwpCurPos = 90;
    REQUIRE( app.getHwpStatus() == "Unknown" );
}

/// Verify `getHwpTrackingOffset()` combines parallactic angle, altitude, and pupil offset.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker getHwpTrackingOffset computes the tracking offset", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::getHwpTrackingOffset();
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );

    app.m_parang      = 20;
    app.m_altitude    = 50;
    app.m_pupilOffset = 5;
    app.getHwpTrackingOffset();
    REQUIRE( app.m_hwpTrackingOffset == Approx( 45.0 ) );

    app.m_parang      = -60;
    app.m_altitude    = 30;
    app.m_pupilOffset = 0;
    app.getHwpTrackingOffset();
    REQUIRE( app.m_hwpTrackingOffset == Approx( 60.0 ) );
}

/// Verify `updateHwpPos()` converts the HWP angle into a stage target using zero and sign.
/**
 * With no INDI driver `sendNewProperty()` fails and is logged, but the target element is set first.
 *
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker updateHwpPos computes the stage target", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::updateHwpPos();
    hwpTracker::recordPolTrack( false );
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );
    app.setupStageProperty();

    app.m_hwpSetPos         = 10;
    app.m_hwpTrackingOffset = 45;
    app.updateHwpPos();
    REQUIRE( app.stageTarget() == Approx( 305.0 ) );

    app.m_zero = 100;
    app.m_sign = 1;
    app.updateHwpPos();
    REQUIRE( app.stageTarget() == Approx( 155.0 ) );
}

/// Verify `appStartup()` builds and registers the INDI properties.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker appStartup creates and registers properties", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::appStartup();
    hwpTracker::appShutdown();
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );
    app.m_devName    = "stagehwp";
    app.m_tcsDevName = "tcs2";

    REQUIRE( app.startup() == 0 );
    REQUIRE( app.state() == stateCodes::READY );

    REQUIRE( app.m_indiP_tracking.getName() == "tracking" );
    REQUIRE( app.m_indiP_tracking.find( "toggle" ) );
    REQUIRE( app.hasNewCallBack( "hwptrack.tracking" ) );

    REQUIRE( app.m_indiP_hwpSetPos.getName() == "hwp_position" );
    REQUIRE( app.m_indiP_hwpSetPos.find( "target" ) );
    REQUIRE( app.m_indiP_hwpSetPos.find( "current" ) );
    REQUIRE( app.hasNewCallBack( "hwptrack.hwp_position" ) );

    REQUIRE( app.hasSetCallBack( "tcs2.teldata" ) );
    REQUIRE( app.hasSetCallBack( "stagehwp.position" ) );
    REQUIRE( app.hasSetCallBack( "stagehwp.fsm" ) );

    REQUIRE( app.m_indiP_hwpTrackingOffset.getName() == "hwp_tracking_offset" );
    REQUIRE( app.m_indiP_hwpTrackingOffset.find( "value" ) );
    REQUIRE( app.m_indiP_hwpPosName.getName() == "hwp_position_name" );
    REQUIRE( app.m_indiP_hwpPosName.find( "value" ) );
    REQUIRE( app.m_indiP_hwpActualPos.getName() == "hwp_position_actual" );
    REQUIRE( app.m_indiP_hwpActualPos.find( "value" ) );

    REQUIRE( app.m_indiP_hwpStagePos.getDevice() == "stagehwp" );
    REQUIRE( app.m_indiP_hwpStagePos.getName() == "position" );
    REQUIRE( app.m_indiP_hwpStagePos.find( "target" ) );

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify the teldata callback converts zenith distance to altitude and stores the parallactic angle.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker teldata callback stores altitude and parallactic angle", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::setCallBack_m_indiP_teldata( pcf::IndiProperty() );
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );

    pcf::IndiProperty ip = makeProp( pcf::IndiProperty::Number, "tcsi", "teldata", "zd", "30" );
    ip.add( pcf::IndiElement( "pa" ) );
    ip["pa"].setValue( std::string( "-12.5" ) );

    REQUIRE( app.setCallBack_m_indiP_teldata( ip ) == 0 );
    REQUIRE( app.m_altitude == Approx( 60.0 ) );
    REQUIRE( app.m_parang == Approx( -12.5 ) );

    // Updates missing `pa` are ignored.
    REQUIRE( app.setCallBack_m_indiP_teldata( makeProp( pcf::IndiProperty::Number, "tcsi", "teldata", "zd", "10" ) ) ==
             0 );
    REQUIRE( app.m_altitude == Approx( 60.0 ) );

    // Updates from the wrong device are rejected.
    REQUIRE( app.setCallBack_m_indiP_teldata( makeProp( pcf::IndiProperty::Number, "tcsx", "teldata", "zd", "10" ) ) ==
             -1 );
}

/// Verify the HWP set-position callback stores the target and commands the stage.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker hwp_position callback sets the HWP angle", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::newCallBack_m_indiP_hwpSetPos( pcf::IndiProperty() );
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );
    app.setupStageProperty();

    REQUIRE( app.newCallBack_m_indiP_hwpSetPos(
                 makeProp( pcf::IndiProperty::Number, "hwptrack", "hwp_position", "target", "22.5" ) ) == 0 );
    REQUIRE( app.m_hwpSetPos == Approx( 22.5 ) );
    REQUIRE( app.stageTarget() == Approx( 337.5 ) );

    // A request without a target is ignored.
    REQUIRE( app.newCallBack_m_indiP_hwpSetPos(
                 makeProp( pcf::IndiProperty::Number, "hwptrack", "hwp_position", "current", "45" ) ) == 0 );
    REQUIRE( app.m_hwpSetPos == Approx( 22.5 ) );
}

/// Verify the tracking toggle applies and removes the tracking offset.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker tracking toggle applies and clears the offset", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::newCallBack_m_indiP_tracking( pcf::IndiProperty() );
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );
    app.setupStageProperty();

    app.m_hwpSetPos = 10;
    app.m_parang    = 20;
    app.m_altitude  = 50;

    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "hwptrack", pcf::IndiElement::On ) ) == 0 );
    REQUIRE( app.m_tracking );
    REQUIRE( app.m_hwpTrackingOffset == Approx( 40.0 ) );
    REQUIRE( app.stageTarget() == Approx( 310.0 ) );

    REQUIRE( app.newCallBack_m_indiP_tracking( trackingRequest( "hwptrack", pcf::IndiElement::Off ) ) == 0 );
    REQUIRE_FALSE( app.m_tracking );
    REQUIRE( app.m_hwpTrackingOffset == Approx( 0.0 ) );
    REQUIRE( app.stageTarget() == Approx( 350.0 ) );

    // A request without a toggle element changes nothing.
    pcf::IndiProperty noToggle( pcf::IndiProperty::Switch );
    noToggle.setDevice( "hwptrack" );
    noToggle.setName( "tracking" );
    REQUIRE( app.newCallBack_m_indiP_tracking( noToggle ) == 0 );
    REQUIRE_FALSE( app.m_tracking );
}

/// Verify the stage position callback derives the HWP angle and position name.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker stage position callback derives the HWP angle", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::setCallBack_m_indiP_stagePolRot( pcf::IndiProperty() );
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );

    auto stage = []( const std::string &current )
    { return makeProp( pcf::IndiProperty::Number, "stagepolrot", "position", "current", current ); };

    REQUIRE( app.setCallBack_m_indiP_stagePolRot( stage( "315" ) ) == 0 );
    REQUIRE( app.m_hwpActualPos == Approx( 45.0 ) );
    REQUIRE( app.m_hwpCurPos == Approx( 45.0 ) );
    REQUIRE( app.m_hwpPosName == "Qminus" );

    REQUIRE( app.setCallBack_m_indiP_stagePolRot( stage( "337.5" ) ) == 0 );
    REQUIRE( app.m_hwpPosName == "Uplus" );

    // Values within 0.01 deg of zero snap to zero.
    REQUIRE( app.setCallBack_m_indiP_stagePolRot( stage( "360.005" ) ) == 0 );
    REQUIRE( app.m_hwpActualPos == 0 );
    REQUIRE( app.m_hwpPosName == "Qplus" );

    // The tracking offset is removed from the reported angle, rounded to 0.01 deg.
    app.m_hwpTrackingOffset = 10;
    REQUIRE( app.setCallBack_m_indiP_stagePolRot( stage( "282.4567" ) ) == 0 );
    REQUIRE( app.m_hwpActualPos == Approx( 77.5433 ).margin( 1e-3 ) );
    REQUIRE( app.m_hwpCurPos == Approx( 67.54 ).margin( 1e-4 ) );
    REQUIRE( app.m_hwpPosName == "Uminus" );

    app.m_hwpTrackingOffset = 0;
    REQUIRE( app.setCallBack_m_indiP_stagePolRot( stage( "300" ) ) == 0 );
    REQUIRE( app.m_hwpPosName == "Unknown" );

    // Updates without `current` are ignored.
    REQUIRE( app.setCallBack_m_indiP_stagePolRot(
                 makeProp( pcf::IndiProperty::Number, "stagepolrot", "position", "target", "0" ) ) == 0 );
    REQUIRE( app.m_hwpPosName == "Unknown" );
}

/// Verify the stage FSM callback mirrors the stage state into the app state.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker stage fsm callback mirrors the stage state", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::setCallBack_m_indiP_stagePolRotFsm( pcf::IndiProperty() );
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );

    REQUIRE( app.setCallBack_m_indiP_stagePolRotFsm(
                 makeProp( pcf::IndiProperty::Text, "stagepolrot", "fsm", "state", "HOMING" ) ) == 0 );
    REQUIRE( app.state() == stateCodes::HOMING );

    REQUIRE( app.setCallBack_m_indiP_stagePolRotFsm(
                 makeProp( pcf::IndiProperty::Text, "stagepolrot", "fsm", "state", "READY" ) ) == 0 );
    REQUIRE( app.state() == stateCodes::READY );

    REQUIRE( app.setCallBack_m_indiP_stagePolRotFsm(
                 makeProp( pcf::IndiProperty::Text, "stagepolrot", "fsm", "other", "ERROR" ) ) == 0 );
    REQUIRE( app.state() == stateCodes::READY );
}

/// Verify `appLogic()` updates the stage while tracking and honors the update interval.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker appLogic updates the stage while tracking", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::appLogic();
    hwpTracker::checkRecordTimes();
    hwpTracker::recordTelem( nullptr );
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );
    REQUIRE( app.startup() == 0 );

    app.m_updateInterval = 1000;

    // Not tracking resets the static update timestamp and leaves the stage alone.
    app.m_tracking = false;
    REQUIRE( app.appLogic() == 0 );

    app.m_tracking  = true;
    app.m_hwpSetPos = 0;
    app.m_parang    = 20;
    app.m_altitude  = 50;
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_hwpTrackingOffset == Approx( 40.0 ) );
    REQUIRE( app.stageTarget() == Approx( 320.0 ) );

    // Within the update interval nothing is recomputed.
    app.m_altitude = 60;
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_hwpTrackingOffset == Approx( 40.0 ) );

    // Stopping and restarting tracking re-arms an immediate update.
    app.m_tracking = false;
    REQUIRE( app.appLogic() == 0 );
    app.m_tracking = true;
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.m_hwpTrackingOffset == Approx( 50.0 ) );
    REQUIRE( app.stageTarget() == Approx( 310.0 ) );

    REQUIRE( app.recordTelem( nullptr ) == 0 );

    // Leave the static timestamp reset for any later test.
    app.m_tracking = false;
    REQUIRE( app.appLogic() == 0 );
}

/// Verify `appLogic()` reports failure when the telemetry thread is not running.
/**
 * \ingroup hwpTracker_unit_test
 */
TEST_CASE( "hwpTracker appLogic fails without the telemetry thread", "[hwpTracker]" )
{
    // clang-format off
    #ifdef HWPTRACKER_TEST_DOXYGEN_REF
    hwpTracker::appLogic();
    #endif
    // clang-format on

    hwpTracker_test app( "hwptrack" );

    app.m_tracking = false;
    REQUIRE( app.appLogic() == -1 );
    REQUIRE( app.state() == stateCodes::FAILURE );
    REQUIRE( app.shutdown() == 1 );
}

} // namespace hwpTrackerTest

} // namespace libXWCTest
