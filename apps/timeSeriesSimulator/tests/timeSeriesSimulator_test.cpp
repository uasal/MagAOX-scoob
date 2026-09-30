/** \file timeSeriesSimulator_test.cpp
 * \brief Catch2 tests for the timeSeriesSimulator app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup timeSeriesSimulator_files
 */

#include "../../../tests/testXWC.hpp"

// Included before the app header: INDI_VALIDATE_CALLBACK_PROPS returns 0 after a successful device/name check.
// The callback bodies call m_indiDriver->sendSetProperty() without a null check, so they cannot run in unit tests.
#include "../../../tests/testMacrosINDI.hpp"

#include <cstdio>
#include <string>

#include "../timeSeriesSimulator.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup timeSeriesSimulator_unit_test timeSeriesSimulator Unit Tests
 * \brief Unit tests for the timeSeriesSimulator application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `timeSeriesSimulator` unit tests.
/** \ingroup timeSeriesSimulator_unit_test
 */
namespace timeSeriesSimulatorTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing timeSeriesSimulator internals.
class timeSeriesSimulator_test : public timeSeriesSimulator
{
  public:
    /// Construct a harness with the given device name.
    explicit timeSeriesSimulator_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_ARB_NEW_PROP( function, function );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( duty_cycle, duty_cycle );
    }

    using timeSeriesSimulator::amplitude;
    using timeSeriesSimulator::duty_cycle;
    using timeSeriesSimulator::function;
    using timeSeriesSimulator::gizmos;
    using timeSeriesSimulator::gizmosInMotion;
    using timeSeriesSimulator::gizmoTimeToTarget;
    using timeSeriesSimulator::m_loopPause;
    using timeSeriesSimulator::m_startup_delay;
    using timeSeriesSimulator::myFunction;
    using timeSeriesSimulator::n_gizmos;
    using timeSeriesSimulator::period;
    using timeSeriesSimulator::SimFunction;
    using timeSeriesSimulator::simsensor;
    using timeSeriesSimulator::startTimeSec;

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

    /// Get the simulated sensor output.
    /**
     * \returns the `value` element of the `function_out` property
     */
    double sensorValue()
    {
        return simsensor["value"].get<double>();
    }

    /// Run the simulated sensor update with a given elapsed time.
    void updateSimsensorAt( double elapsed /**< [in] seconds elapsed since the start time */ )
    {
        startTimeSec = mx::sys::get_curr_time() - elapsed;
        updateSimsensor();
    }
};
/// \endcond

/// Verify the timeSeriesSimulator defaults, including the configurable startup delay.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator configuration defaults", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::timeSeriesSimulator();
    timeSeriesSimulator::setupConfig();
    timeSeriesSimulator::loadConfig();
    timeSeriesSimulator::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    mx::app::writeConfigFile( "/tmp/timeSeriesSimulator_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.configure( "/tmp/timeSeriesSimulator_test_defaults.conf" );

    REQUIRE( app.m_startup_delay == 0u );

    // 200 ms sampling from the constructor
    REQUIRE( app.m_loopPause == 200000000UL );

    REQUIRE( app.myFunction == timeSeriesSimulator_test::SimFunction::square );
    REQUIRE( app.amplitude == 1.0 );
    REQUIRE( app.period == 5.0 );
    REQUIRE( app.n_gizmos == 2 );
    REQUIRE( app.gizmoTimeToTarget == 1.0 );

    std::remove( "/tmp/timeSeriesSimulator_test_defaults.conf" );
}

/// Verify the timeSeriesSimulator startup delay configuration override.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator configuration overrides", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::setupConfig();
    timeSeriesSimulator::loadConfig();
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    mx::app::writeConfigFile( "/tmp/timeSeriesSimulator_test_overrides.conf", { "" }, { "startup_delay" }, { "3" } );
    app.configure( "/tmp/timeSeriesSimulator_test_overrides.conf" );

    REQUIRE( app.m_startup_delay == 3u );

    std::remove( "/tmp/timeSeriesSimulator_test_overrides.conf" );
}

/// Verify `appStartup()` creates the function, duty cycle, sensor, and gizmo properties.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator appStartup creates the INDI properties", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::appStartup();
    timeSeriesSimulator::appShutdown();
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.state() == stateCodes::READY );

    REQUIRE( app.function.getName() == "function" );
    REQUIRE( app.function.getDevice() == "tss" );
    REQUIRE( app.function.getType() == pcf::IndiProperty::Switch );
    REQUIRE( app.function["sin"].getSwitchState() == pcf::IndiElement::Off );
    REQUIRE( app.function["cos"].getSwitchState() == pcf::IndiElement::Off );
    REQUIRE( app.function["square"].getSwitchState() == pcf::IndiElement::On );
    REQUIRE( app.function["constant"].getSwitchState() == pcf::IndiElement::Off );

    REQUIRE( app.simsensor.getName() == "function_out" );
    REQUIRE( app.simsensor.getState() == pcf::IndiProperty::Ok );

    REQUIRE( app.duty_cycle.getName() == "duty_cycle" );
    REQUIRE( app.duty_cycle["period"].get<double>() == Approx( 5.0 ) );
    REQUIRE( app.duty_cycle["amplitude"].get<double>() == Approx( 1.0 ) );

    REQUIRE( app.gizmos.size() == 2 );
    REQUIRE( app.gizmos[0]->getName() == "gizmo_0000" );
    REQUIRE( app.gizmos[1]->getName() == "gizmo_0001" );
    REQUIRE( app.gizmos[0]->getDevice() == "tss" );
    REQUIRE( ( *app.gizmos[0] )["current"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
    REQUIRE( ( *app.gizmos[0] )["target"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
    REQUIRE( app.gizmosInMotion.empty() );

    REQUIRE( app.hasNewCallBack( "tss.function" ) );
    REQUIRE( app.hasNewCallBack( "tss.function_out" ) );
    REQUIRE( app.hasNewCallBack( "tss.duty_cycle" ) );
    REQUIRE( app.hasNewCallBack( "tss.gizmo_0000" ) );
    REQUIRE( app.hasNewCallBack( "tss.gizmo_0001" ) );

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify the linear interpolation used for gizmo motion.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator lerp interpolates linearly", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::lerp( 0, 0, 0, 0, 0 );
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    REQUIRE( app.lerp( 0, 0, 1, 10, 0.5 ) == Approx( 5.0 ) );
    REQUIRE( app.lerp( 0, 10, 2, 0, 1 ) == Approx( 5.0 ) );
    REQUIRE( app.lerp( 0, 3, 1, 7, 0 ) == Approx( 3.0 ) );
    REQUIRE( app.lerp( 0, 3, 1, 7, 1 ) == Approx( 7.0 ) );
    REQUIRE( app.lerp( 1, 2, 3, 6, 2 ) == Approx( 4.0 ) );

    // extrapolation beyond the end point
    REQUIRE( app.lerp( 0, 0, 1, 1, 2 ) == Approx( 2.0 ) );
}

/// Verify the simulated sensor waveforms.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator updateSimsensor waveforms", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::updateSimsensor();
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    REQUIRE( app.appStartup() == 0 );

    app.amplitude = 2.0;
    app.period    = 4.0;

    SECTION( "sine" )
    {
        app.myFunction = timeSeriesSimulator_test::SimFunction::sin;

        // a quarter period: at the peak, so insensitive to the time between setting and reading the clock
        app.updateSimsensorAt( 1.0 );
        REQUIRE( app.sensorValue() == Approx( 2.0 ).margin( 1e-3 ) );

        // three quarters of a period: at the trough
        app.updateSimsensorAt( 3.0 );
        REQUIRE( app.sensorValue() == Approx( -2.0 ).margin( 1e-3 ) );
    }

    SECTION( "cosine" )
    {
        app.myFunction = timeSeriesSimulator_test::SimFunction::cos;

        // half a period: at the trough
        app.updateSimsensorAt( 2.0 );
        REQUIRE( app.sensorValue() == Approx( -2.0 ).margin( 1e-3 ) );

        // a full period: at the peak
        app.updateSimsensorAt( 4.0 );
        REQUIRE( app.sensorValue() == Approx( 2.0 ).margin( 1e-3 ) );
    }

    SECTION( "square" )
    {
        app.myFunction = timeSeriesSimulator_test::SimFunction::square;

        // low during even periods
        app.updateSimsensorAt( 2.0 );
        REQUIRE( app.sensorValue() == Approx( 0.0 ).margin( 1e-12 ) );

        // high during odd periods
        app.updateSimsensorAt( 6.0 );
        REQUIRE( app.sensorValue() == Approx( 2.0 ) );

        app.updateSimsensorAt( 10.0 );
        REQUIRE( app.sensorValue() == Approx( 0.0 ).margin( 1e-12 ) );
    }

    SECTION( "constant" )
    {
        app.myFunction = timeSeriesSimulator_test::SimFunction::constant;

        app.updateSimsensorAt( 1.7 );
        REQUIRE( app.sensorValue() == Approx( 2.0 ) );

        app.amplitude = -0.25;
        app.updateSimsensorAt( 123.4 );
        REQUIRE( app.sensorValue() == Approx( -0.25 ) );
    }
}

/// Verify gizmo motion requests and their interpolation to the target.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator gizmo motion", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::requestGizmoTarget( nullptr, 0 );
    timeSeriesSimulator::updateGizmos();
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    REQUIRE( app.appStartup() == 0 );

    pcf::IndiProperty *g0 = app.gizmos[0];
    pcf::IndiProperty *g1 = app.gizmos[1];

    SECTION( "a request records the start and target positions" )
    {
        app.requestGizmoTarget( g0, 50.0 );

        REQUIRE( ( *g0 )["target"].get<double>() == Approx( 50.0 ) );
        REQUIRE( app.gizmosInMotion.size() == 1 );
        REQUIRE( app.gizmosInMotion.count( "gizmo_0000" ) == 1 );

        auto req = app.gizmosInMotion["gizmo_0000"];
        REQUIRE( req->property == g0 );
        REQUIRE( req->startPos == Approx( 0.0 ).margin( 1e-12 ) );
        REQUIRE( req->targetPos == Approx( 50.0 ) );

        // a second request for the same gizmo updates the existing request
        app.requestGizmoTarget( g0, 75.0 );
        REQUIRE( app.gizmosInMotion.size() == 1 );
        REQUIRE( app.gizmosInMotion["gizmo_0000"]->targetPos == Approx( 75.0 ) );

        // a request for another gizmo adds a request
        app.requestGizmoTarget( g1, 10.0 );
        REQUIRE( app.gizmosInMotion.size() == 2 );
    }

    SECTION( "a gizmo in motion is interpolated between start and target" )
    {
        app.gizmoTimeToTarget = 2.0;
        app.requestGizmoTarget( g1, 80.0 );

        // pretend the request was made one second ago, half of the time to target
        app.gizmosInMotion["gizmo_0001"]->requestTime -= 1.0;

        app.updateGizmos();

        REQUIRE( ( *g1 )["current"].get<double>() == Approx( 40.0 ).margin( 0.5 ) );
        REQUIRE( ( *g1 )["target"].get<double>() == Approx( 80.0 ) );
        REQUIRE( app.gizmosInMotion.count( "gizmo_0001" ) == 1 );
    }

    SECTION( "a gizmo that has just started has barely moved" )
    {
        app.gizmoTimeToTarget = 1000.0;
        app.requestGizmoTarget( g0, 75.0 );

        app.updateGizmos();

        REQUIRE( ( *g0 )["current"].get<double>() == Approx( 0.0 ).margin( 0.1 ) );
        REQUIRE( app.gizmosInMotion.count( "gizmo_0000" ) == 1 );
    }

    SECTION( "a gizmo past its time to target arrives and stops" )
    {
        app.gizmoTimeToTarget = 0.0;
        app.requestGizmoTarget( g0, 33.0 );

        app.updateGizmos();

        REQUIRE( ( *g0 )["current"].get<double>() == Approx( 33.0 ) );
        REQUIRE( ( *g0 )["target"].get<double>() == Approx( 33.0 ) );
        REQUIRE( g0->getState() == pcf::IndiProperty::Ok );
        REQUIRE( app.gizmosInMotion.empty() );

        // a new request starts from the arrived position
        app.gizmoTimeToTarget = 1000.0;
        app.requestGizmoTarget( g0, 0.0 );
        REQUIRE( app.gizmosInMotion["gizmo_0000"]->startPos == Approx( 33.0 ) );
    }
}

/// Verify `appLogic()` updates the simulated sensor and the gizmos.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator appLogic updates the outputs", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::appLogic();
    timeSeriesSimulator::updateVals();
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    REQUIRE( app.appStartup() == 0 );

    app.myFunction        = timeSeriesSimulator_test::SimFunction::constant;
    app.amplitude         = 3.0;
    app.gizmoTimeToTarget = 0.0;
    app.requestGizmoTarget( app.gizmos[1], 12.0 );

    REQUIRE( app.appLogic() == 0 );

    REQUIRE( app.sensorValue() == Approx( 3.0 ) );
    REQUIRE( ( *app.gizmos[1] )["current"].get<double>() == Approx( 12.0 ) );
    REQUIRE( app.gizmosInMotion.empty() );
}

/// Verify the gizmo NEW callback ignores properties that are not gizmo target requests.
/**
 * A gizmo property with a `target` element is not sent here: the callback body calls
 * `m_indiDriver->sendSetProperty()` without a null check, and there is no INDI driver in the unit tests.
 *
 * \ingroup timeSeriesSimulator_unit_test
 */
TEST_CASE( "timeSeriesSimulator newCallBack_gizmos ignores non-requests", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::newCallBack_gizmos( pcf::IndiProperty() );
    #endif
    // clang-format on

    timeSeriesSimulator_test app( "tss" );

    REQUIRE( app.appStartup() == 0 );

    SECTION( "a gizmo property without a target" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "tss" );
        ip.setName( "gizmo_0000" );
        ip.add( pcf::IndiElement( "current" ) );
        ip["current"] = 42.0;

        REQUIRE( app.newCallBack_gizmos( ip ) == 0 );
        REQUIRE( app.gizmosInMotion.empty() );
        REQUIRE( ( *app.gizmos[0] )["target"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
    }

    SECTION( "a target for an unknown property" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "tss" );
        ip.setName( "gizmo_9999" );
        ip.add( pcf::IndiElement( "target" ) );
        ip["target"] = 42.0;

        REQUIRE( app.newCallBack_gizmos( ip ) == 0 );
        REQUIRE( app.gizmosInMotion.empty() );
    }
}

/// Verify the INDI NEW callbacks for the function and duty cycle properties validate the device and name.
/**
 * \ingroup timeSeriesSimulator_unit_test
 */
SCENARIO( "timeSeriesSimulator INDI callbacks validate the property", "[timeSeriesSimulator]" )
{
    // clang-format off
    #ifdef TIMESERIESSIMULATOR_TEST_DOXYGEN_REF
    timeSeriesSimulator::newCallBack_function( pcf::IndiProperty() );
    timeSeriesSimulator::newCallBack_duty_cycle( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_ARBNEW_CALLBACK( timeSeriesSimulator, newCallBack_function, function );
    XWCTEST_INDI_ARBNEW_CALLBACK( timeSeriesSimulator, newCallBack_duty_cycle, duty_cycle );
}

} // namespace timeSeriesSimulatorTest

} // namespace libXWCTest
