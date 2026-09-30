/** \file alignLoop_test.cpp
 * \brief Catch2 tests for the alignLoop app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup alignLoop_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "../alignLoop.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup alignLoop_unit_test alignLoop Unit Tests
 * \brief Unit tests for the alignLoop application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `alignLoop` unit tests.
/** \ingroup alignLoop_unit_test
 */
namespace alignLoopTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing alignLoop internals.
class alignLoop_test : public alignLoop
{
  public:
    /// Construct a harness with the given device name.
    explicit alignLoop_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    using alignLoop::m_commands;
    using alignLoop::m_ctrlCurrents;
    using alignLoop::m_ctrlDevices;
    using alignLoop::m_ctrlEnabled;
    using alignLoop::m_ctrlProperties;
    using alignLoop::m_ctrlTargets;
    using alignLoop::m_currents;
    using alignLoop::m_defaultGains;
    using alignLoop::m_delta0;
    using alignLoop::m_delta1;
    using alignLoop::m_gains;
    using alignLoop::m_ggain;
    using alignLoop::m_intMat;
    using alignLoop::m_intMatFile;
    using alignLoop::m_measurements;
    using alignLoop::m_upstreamDevice;
    using alignLoop::m_upstreamFollowClosed;
    using alignLoop::m_upstreamProperty;

    using dev::shmimMonitor<alignLoop>::m_height;
    using dev::shmimMonitor<alignLoop>::m_shmimName;
    using dev::shmimMonitor<alignLoop>::m_width;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Set up a two-axis loop the way `appStartup()` does, without starting the shmimMonitor thread.
    void setupLoop()
    {
        m_ctrlDevices    = { "stage0", "stage1" };
        m_ctrlProperties = { "position", "position" };
        m_ctrlCurrents   = { "current", "current" };
        m_ctrlTargets    = { "target", "target" };

        m_gains = { 1.0f, 1.0f };

        createStandardIndiNumber<unsigned>( m_indiP_ggain, "loop_gain", 0, 1, 0, "%0.2f" );
        createStandardIndiToggleSw( m_indiP_ctrlEnabled, "loop_state" );

        m_currents.resize( m_ctrlDevices.size(), -1e15 );

        m_commands.resize( 2, 2 );
        m_commands.setZero();

        m_intMat.resize( 2, 2 );
        m_intMat.setZero();
        m_intMat( 1, 0 ) = 0.926;
        m_intMat( 1, 1 ) = -0.370;
        m_intMat( 0, 0 ) = 0.185;
        m_intMat( 0, 1 ) = 0.926;

        m_width  = 2;
        m_height = 1;
        m_measurements.resize( 2, 1 );
    }
};

/// Build a Number property with a single element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty numberProp( const std::string &device, /**< [in] INDI device name */
                              const std::string &name,   /**< [in] INDI property name */
                              const std::string &el,     /**< [in] element name */
                              float              value   /**< [in] element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, value ) );
    return ip;
}

/// Build a Switch property with a single `toggle` element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty toggleProp( const std::string                &device, /**< [in] INDI device name */
                              const std::string                &name,   /**< [in] INDI property name */
                              pcf::IndiElement::SwitchStateType state   /**< [in] the toggle state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( "toggle", state ) );
    return ip;
}

/// \endcond

/// Verify the alignLoop configuration defaults and overrides.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop configuration", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::setupConfig();
    alignLoop::loadConfig();
    alignLoop::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/alignLoop_test_config.conf";

    SECTION( "defaults" )
    {
        mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

        alignLoop_test app( "align" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_ctrlDevices.empty() );
        REQUIRE( app.m_ctrlProperties.empty() );
        REQUIRE( app.m_ctrlCurrents.empty() );
        REQUIRE( app.m_ctrlTargets.empty() );
        REQUIRE( app.m_defaultGains.empty() );
        REQUIRE( app.m_ggain == 0.0f );
        REQUIRE( app.m_intMatFile == "" );
        REQUIRE( app.m_upstreamDevice == "" );
        REQUIRE( app.m_upstreamProperty == "loop_state" );
        REQUIRE( app.m_upstreamFollowClosed == false );
        REQUIRE( app.m_shmimName == "align" );
    }

    SECTION( "overrides" )
    {
        mx::app::writeConfigFile(
            fname,
            { "ctrl", "ctrl", "ctrl", "ctrl", "loop", "loop", "loop", "loop", "loop", "loop", "shmimMonitor" },
            { "devices",
              "properties",
              "currents",
              "targets",
              "gain",
              "intMat",
              "gains",
              "upstream",
              "upstreamProperty",
              "upstreamFollowClosed",
              "shmimName" },
            { "stage0,stage1",
              "position,pos2",
              "current,cur2",
              "target,tgt2",
              "0.4",
              "intmat.fits",
              "0.5,0.25",
              "upLoop",
              "upState",
              "true",
              "alignMeas" } );

        alignLoop_test app( "align" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_ctrlDevices == std::vector<std::string>( { "stage0", "stage1" } ) );
        REQUIRE( app.m_ctrlProperties == std::vector<std::string>( { "position", "pos2" } ) );
        REQUIRE( app.m_ctrlCurrents == std::vector<std::string>( { "current", "cur2" } ) );
        REQUIRE( app.m_ctrlTargets == std::vector<std::string>( { "target", "tgt2" } ) );
        REQUIRE( app.m_ggain == Approx( 0.4f ) );
        REQUIRE( app.m_intMatFile == "intmat.fits" );
        REQUIRE( app.m_defaultGains.size() == 2 );
        REQUIRE( app.m_defaultGains[0] == Approx( 0.5f ) );
        REQUIRE( app.m_defaultGains[1] == Approx( 0.25f ) );
        REQUIRE( app.m_upstreamDevice == "upLoop" );
        REQUIRE( app.m_upstreamProperty == "upState" );
        REQUIRE( app.m_upstreamFollowClosed == true );
        REQUIRE( app.m_shmimName == "alignMeas" );
    }

    std::remove( fname.c_str() );
}

/// Verify `toggleLoop()` changes the loop state only on a transition.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop toggleLoop state transitions", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::toggleLoop( true );
    #endif
    // clang-format on

    alignLoop_test app( "align" );
    app.setupLoop();

    REQUIRE( app.m_ctrlEnabled == false );

    // Opening an open loop does nothing.
    REQUIRE( app.toggleLoop( false ) == 0 );
    REQUIRE( app.m_ctrlEnabled == false );

    // Close.
    REQUIRE( app.toggleLoop( true ) == 0 );
    REQUIRE( app.m_ctrlEnabled == true );

    // Closing a closed loop does nothing.
    REQUIRE( app.toggleLoop( true ) == 0 );
    REQUIRE( app.m_ctrlEnabled == true );

    // Open.
    REQUIRE( app.toggleLoop( false ) == 0 );
    REQUIRE( app.m_ctrlEnabled == false );
}

/// Verify `allocate()` sizes the measurement buffer and `appShutdown()` succeeds without threads.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop allocate and shutdown", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::allocate( dev::shmimT() );
    alignLoop::appShutdown();
    #endif
    // clang-format on

    alignLoop_test app( "align" );
    app.m_width  = 2;
    app.m_height = 3;

    REQUIRE( app.allocate( dev::shmimT() ) == 0 );
    REQUIRE( app.m_measurements.rows() == 2 );
    REQUIRE( app.m_measurements.cols() == 3 );

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify `processImage()` extracts the residuals and applies the interaction matrix once currents are known.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop processImage control math", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::processImage( nullptr, dev::shmimT() );
    alignLoop::sendCommands( std::vector<float>() );
    #endif
    // clang-format on

    float meas[2] = { 1.0f, 2.0f };

    SECTION( "no controller currents yet: residuals only" )
    {
        alignLoop_test app( "align" );
        app.setupLoop();

        REQUIRE( app.processImage( meas, dev::shmimT() ) == 0 );

        REQUIRE( app.m_measurements( 0, 0 ) == 1.0f );
        REQUIRE( app.m_measurements( 1, 0 ) == 2.0f );
        REQUIRE( app.m_delta0 == 1.0f );
        REQUIRE( app.m_delta1 == 2.0f );

        // No commands were computed.
        REQUIRE( app.m_commands.rows() == 2 );
        REQUIRE( app.m_commands.cols() == 2 );
        REQUIRE( ( app.m_commands == 0.0f ).all() );
    }

    SECTION( "currents known, loop open: commands are computed but not sent" )
    {
        alignLoop_test app( "align" );
        app.setupLoop();
        app.m_currents = { 10.0f, 20.0f };
        app.m_ggain    = 0.5f;

        REQUIRE( app.processImage( meas, dev::shmimT() ) == 0 );

        REQUIRE( app.m_commands.rows() == 2 );
        REQUIRE( app.m_commands.cols() == 1 );
        REQUIRE( app.m_commands( 0, 0 ) == Approx( 0.185 * 1 + 0.926 * 2 ) );
        REQUIRE( app.m_commands( 1, 0 ) == Approx( 0.926 * 1 - 0.370 * 2 ) );
    }

    SECTION( "currents known, loop closed: commands are sent" )
    {
        alignLoop_test app( "align" );
        app.setupLoop();
        app.m_currents    = { 10.0f, 20.0f };
        app.m_ggain       = 0.5f;
        app.m_ctrlEnabled = true;

        // Without an INDI driver the sends fail and are logged, but sendCommands() still reports success.
        REQUIRE( app.processImage( meas, dev::shmimT() ) == 0 );
        REQUIRE( app.m_commands( 0, 0 ) == Approx( 2.037 ) );
        REQUIRE( app.m_commands( 1, 0 ) == Approx( 0.186 ) );
    }

    SECTION( "sendCommands with no controllers" )
    {
        alignLoop_test app( "align" );

        std::vector<float> cmds;
        REQUIRE( app.sendCommands( cmds ) == 0 );
    }
}

/// Verify `setCallBack_ctrl()` caches the current value only for matching device, property and element.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop controller set callback", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::setCallBack_ctrl( pcf::IndiProperty() );
    alignLoop::st_setCallBack_ctrl( nullptr, pcf::IndiProperty() );
    #endif
    // clang-format on

    alignLoop_test app( "align" );
    app.setupLoop();

    SECTION( "matching update is cached" )
    {
        REQUIRE( app.setCallBack_ctrl( numberProp( "stage1", "position", "current", 3.5f ) ) == 0 );
        REQUIRE( app.m_currents[0] < -1e14 );
        REQUIRE( app.m_currents[1] == Approx( 3.5f ) );
    }

    SECTION( "static wrapper routes to the instance" )
    {
        REQUIRE( alignLoop::st_setCallBack_ctrl( &app, numberProp( "stage0", "position", "current", -1.25f ) ) == 0 );
        REQUIRE( app.m_currents[0] == Approx( -1.25f ) );
        REQUIRE( app.m_currents[1] < -1e14 );
    }

    SECTION( "wrong property is ignored" )
    {
        REQUIRE( app.setCallBack_ctrl( numberProp( "stage0", "other", "current", 3.5f ) ) == 0 );
        REQUIRE( app.m_currents[0] < -1e14 );
    }

    SECTION( "missing current element is ignored" )
    {
        REQUIRE( app.setCallBack_ctrl( numberProp( "stage0", "position", "target", 3.5f ) ) == 0 );
        REQUIRE( app.m_currents[0] < -1e14 );
    }

    SECTION( "unknown device is ignored" )
    {
        REQUIRE( app.setCallBack_ctrl( numberProp( "stage9", "position", "current", 3.5f ) ) == 0 );
        REQUIRE( app.m_currents[0] < -1e14 );
        REQUIRE( app.m_currents[1] < -1e14 );
    }
}

/// Verify the `loop_gain` new-property callback.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop loop_gain callback", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::newCallBack_m_indiP_ggain( pcf::IndiProperty() );
    #endif
    // clang-format on

    alignLoop_test app( "align" );
    app.setupLoop();
    app.m_ggain = 0.1f;

    SECTION( "target sets the gain" )
    {
        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "align", "loop_gain", "target", 0.35f ) ) == 0 );
        REQUIRE( app.m_ggain == Approx( 0.35f ) );
    }

    SECTION( "current is used when there is no target" )
    {
        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "align", "loop_gain", "current", 0.2f ) ) == 0 );
        REQUIRE( app.m_ggain == Approx( 0.2f ) );
    }

    SECTION( "wrong property name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "align", "wrong", "target", 0.35f ) ) == -1 );
        REQUIRE( app.m_ggain == Approx( 0.1f ) );
    }

    SECTION( "wrong device is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "wrong", "loop_gain", "target", 0.35f ) ) == -1 );
        REQUIRE( app.m_ggain == Approx( 0.1f ) );
    }

    SECTION( "no target or current is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "align", "loop_gain", "other", 0.35f ) ) == -1 );
        REQUIRE( app.m_ggain == Approx( 0.1f ) );
    }
}

/// Verify the `loop_state` new-property callback opens and closes the loop.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop loop_state callback", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::newCallBack_m_indiP_ctrlEnabled( pcf::IndiProperty() );
    #endif
    // clang-format on

    alignLoop_test app( "align" );
    app.setupLoop();

    SECTION( "toggle on closes and toggle off opens" )
    {
        REQUIRE( app.newCallBack_m_indiP_ctrlEnabled( toggleProp( "align", "loop_state", pcf::IndiElement::On ) ) ==
                 0 );
        REQUIRE( app.m_ctrlEnabled == true );

        REQUIRE( app.newCallBack_m_indiP_ctrlEnabled( toggleProp( "align", "loop_state", pcf::IndiElement::Off ) ) ==
                 0 );
        REQUIRE( app.m_ctrlEnabled == false );
    }

    SECTION( "wrong property name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_ctrlEnabled( toggleProp( "align", "wrong", pcf::IndiElement::On ) ) == -1 );
        REQUIRE( app.m_ctrlEnabled == false );
    }
}

/// Verify the upstream loop set-property callback follows the upstream loop state.
/**
 * \ingroup alignLoop_unit_test
 */
TEST_CASE( "alignLoop upstream loop callback", "[alignLoop]" )
{
    // clang-format off
    #ifdef ALIGNLOOP_TEST_DOXYGEN_REF
    alignLoop::setCallBack_m_indiP_upstream( pcf::IndiProperty() );
    #endif
    // clang-format on

    alignLoop_test app( "align" );
    app.setupLoop();
    app.m_indiP_upstream.setDevice( "upLoop" );
    app.m_indiP_upstream.setName( "loop_state" );

    SECTION( "upstream closing does not close this loop by default" )
    {
        REQUIRE( app.setCallBack_m_indiP_upstream( toggleProp( "upLoop", "loop_state", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_ctrlEnabled == false );
    }

    SECTION( "upstream closing closes this loop when following closed" )
    {
        app.m_upstreamFollowClosed = true;
        REQUIRE( app.setCallBack_m_indiP_upstream( toggleProp( "upLoop", "loop_state", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_ctrlEnabled == true );
    }

    SECTION( "upstream opening always opens this loop" )
    {
        app.m_ctrlEnabled = true;
        REQUIRE( app.setCallBack_m_indiP_upstream( toggleProp( "upLoop", "loop_state", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_ctrlEnabled == false );
    }

    SECTION( "a property without a toggle element is ignored" )
    {
        app.m_ctrlEnabled = true;
        REQUIRE( app.setCallBack_m_indiP_upstream( numberProp( "upLoop", "loop_state", "current", 1.0f ) ) == 0 );
        REQUIRE( app.m_ctrlEnabled == true );
    }

    SECTION( "a wrong property name does not change the loop" )
    {
        // Note: the callback logs the mismatch but returns 0 (log<software_error> with the default return value).
        app.m_ctrlEnabled = true;
        app.setCallBack_m_indiP_upstream( toggleProp( "upLoop", "wrong", pcf::IndiElement::Off ) );
        REQUIRE( app.m_ctrlEnabled == true );
    }
}

} // namespace alignLoopTest

} // namespace libXWCTest
