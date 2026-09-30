/** \file closedLoopIndi_test.cpp
 * \brief Catch2 tests for the closedLoopIndi app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * The callback bodies are live in this translation unit (testMacrosINDI.hpp is not included).  The
 * device/name validation tests are in closedLoopIndi_indi_test.cpp.
 *
 * \ingroup closedLoopIndi_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../closedLoopIndi.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup closedLoopIndi_unit_test closedLoopIndi Unit Tests
 * \brief Unit tests for the closedLoopIndi application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `closedLoopIndi` unit tests.
/** \ingroup closedLoopIndi_unit_test
 */
namespace closedLoopIndiTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Path of the configuration file written by the tests.
const std::string confPath = "/tmp/closedLoopIndi_test_config.conf";

/// Test harness exposing closedLoopIndi internals.
class closedLoopIndi_test : public closedLoopIndi
{
  public:
    /// Construct a harness with the given device name.
    explicit closedLoopIndi_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    using closedLoopIndi::m_commands;
    using closedLoopIndi::m_counter;
    using closedLoopIndi::m_ctrlCurrents;
    using closedLoopIndi::m_ctrlDevices;
    using closedLoopIndi::m_ctrlProperties;
    using closedLoopIndi::m_ctrlTargets;
    using closedLoopIndi::m_currents;
    using closedLoopIndi::m_defaultGains;
    using closedLoopIndi::m_delta0;
    using closedLoopIndi::m_delta1;
    using closedLoopIndi::m_fsmStates;
    using closedLoopIndi::m_gains;
    using closedLoopIndi::m_ggain;
    using closedLoopIndi::m_inputCounterElement;
    using closedLoopIndi::m_inputDevice;
    using closedLoopIndi::m_inputElements;
    using closedLoopIndi::m_inputProperty;
    using closedLoopIndi::m_intMat;
    using closedLoopIndi::m_loopClosed;
    using closedLoopIndi::m_measurements;
    using closedLoopIndi::m_operatingOK;
    using closedLoopIndi::m_references;
    using closedLoopIndi::m_upstreamDevice;
    using closedLoopIndi::m_upstreamFollowClosed;
    using closedLoopIndi::m_upstreamProperty;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfigImpl()`.
    /**
     * \returns the return value of `loadConfigImpl()`
     */
    int configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        return loadConfigImpl( config );
    }

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configureVoid( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }
};

/// A configuration, keyed by `section.keyword`.
typedef std::map<std::string, std::string> confMapT;

/// The minimal valid configuration used by most tests.
/**
 * \returns the configuration map
 */
confMapT standardConfig()
{
    return confMapT( { { "input.device", "inputdev" },
                       { "input.property", "measurement" },
                       { "ctrl.devices", "ctrl0dev,ctrl1dev" },
                       { "ctrl.properties", "prop0,prop1" },
                       { "loop.gains", "0.5" } } );
}

/// Write a configuration map to a file.
void writeConfig( const std::string &fname, /**< [in] the configuration file path */
                  const confMapT    &conf   /**< [in] the configuration, keyed by `section.keyword` */
)
{
    std::vector<std::string> sections;
    std::vector<std::string> keywords;
    std::vector<std::string> values;

    for( auto &kv : conf )
    {
        size_t dot = kv.first.find( '.' );
        sections.push_back( kv.first.substr( 0, dot ) );
        keywords.push_back( kv.first.substr( dot + 1 ) );
        values.push_back( kv.second );
    }

    if( sections.size() == 0 )
    {
        sections.push_back( "none" );
        keywords.push_back( "nada" );
        values.push_back( "0" );
    }

    mx::app::writeConfigFile( fname, sections, keywords, values );
}

/// Configure a harness with the standard configuration and run `appStartup()`.
/**
 * \returns the return value of `appStartup()`
 */
int startStandard( closedLoopIndi_test &app,                    /**< [in] the harness to start */
                   const confMapT      &conf = standardConfig() /**< [in] [optional] the configuration */
)
{
    writeConfig( confPath, conf );
    int rv = app.configure( confPath );
    std::remove( confPath.c_str() );

    if( rv < 0 )
    {
        return rv;
    }

    return app.appStartup();
}

/// Build a Number property with the given elements.
/**
 * \returns the INDI property
 */
pcf::IndiProperty numberProp( const std::string              &device, /**< [in] INDI device name */
                              const std::string              &name,   /**< [in] INDI property name */
                              const std::vector<std::string> &els,    /**< [in] element names */
                              const std::vector<float>       &vals    /**< [in] element values, one per element */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    for( size_t n = 0; n < els.size(); ++n )
    {
        ip.add( pcf::IndiElement( els[n], vals[n] ) );
    }
    return ip;
}

/// Build a Switch property with a single element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty switchProp( const std::string                &device, /**< [in] INDI device name */
                              const std::string                &name,   /**< [in] INDI property name */
                              const std::string                &el,     /**< [in] element name */
                              pcf::IndiElement::SwitchStateType state   /**< [in] the switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// Build a Text property with a single `state` element, as sent by a device's `fsm` property.
/**
 * \returns the INDI property
 */
pcf::IndiProperty fsmProp( const std::string &device, /**< [in] INDI device name */
                           const std::string &state   /**< [in] the FSM state string */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Text );
    ip.setDevice( device );
    ip.setName( "fsm" );
    ip.add( pcf::IndiElement( "state", state ) );
    return ip;
}

/// Build the input measurement property sent by the input device.
/**
 * \returns the INDI property
 */
pcf::IndiProperty inputProp( float x,      /**< [in] the first disturbance */
                             float y,      /**< [in] the second disturbance */
                             int   counter /**< [in] the frame counter */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "inputdev" );
    ip.setName( "measurement" );
    ip.add( pcf::IndiElement( "x", x ) );
    ip.add( pcf::IndiElement( "y", y ) );
    ip.add( pcf::IndiElement( "counter", counter ) );
    return ip;
}

/// \endcond

/// Verify the closedLoopIndi configuration defaults and overrides.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi configuration", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::setupConfig();
    closedLoopIndi::loadConfig();
    closedLoopIndi::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    SECTION( "defaults with the minimal required configuration" )
    {
        writeConfig( confPath, standardConfig() );

        closedLoopIndi_test app( "loop" );
        REQUIRE( app.configure( confPath ) == 0 );
        REQUIRE( app.shutdown() == 0 );

        REQUIRE( app.m_inputDevice == "inputdev" );
        REQUIRE( app.m_inputProperty == "measurement" );
        REQUIRE( app.m_inputElements == std::vector<std::string>( { "x", "y" } ) );
        REQUIRE( app.m_inputCounterElement == "counter" );
        REQUIRE( app.m_references.rows() == 2 );
        REQUIRE( app.m_references.cols() == 1 );
        REQUIRE( app.m_references( 0, 0 ) == 0.0f );
        REQUIRE( app.m_references( 1, 0 ) == 0.0f );
        REQUIRE( app.m_ctrlDevices == std::vector<std::string>( { "ctrl0dev", "ctrl1dev" } ) );
        REQUIRE( app.m_ctrlProperties == std::vector<std::string>( { "prop0", "prop1" } ) );
        REQUIRE( app.m_ctrlCurrents == std::vector<std::string>( { "current", "current" } ) );
        REQUIRE( app.m_ctrlTargets == std::vector<std::string>( { "target", "target" } ) );
        REQUIRE( app.m_operatingOK == false );
        REQUIRE( app.m_intMat.rows() == 2 );
        REQUIRE( app.m_intMat.cols() == 2 );
        REQUIRE( app.m_intMat( 0, 0 ) == 1.0f );
        REQUIRE( app.m_intMat( 0, 1 ) == 0.0f );
        REQUIRE( app.m_intMat( 1, 0 ) == 0.0f );
        REQUIRE( app.m_intMat( 1, 1 ) == 1.0f );
        REQUIRE( app.m_ggain == 0.0f );
        REQUIRE( app.m_defaultGains.size() == 1 );
        REQUIRE( app.m_defaultGains[0] == Approx( 0.5f ) );
        REQUIRE( app.m_upstreamDevice == "" );
        REQUIRE( app.m_upstreamProperty == "loop_state" );
        REQUIRE( app.m_upstreamFollowClosed == false );
    }

    SECTION( "overrides" )
    {
        confMapT conf                     = standardConfig();
        conf["input.elements"]            = "dx,dy";
        conf["input.counterElement"]      = "frame";
        conf["input.references"]          = "0.1,-0.2";
        conf["ctrl.currents"]             = "pos,pos2";
        conf["ctrl.targets"]              = "tgt,tgt2";
        conf["ctrl.operatingOK"]          = "true";
        conf["loop.intMat00"]             = "2";
        conf["loop.intMat01"]             = "0.5";
        conf["loop.intMat10"]             = "-0.5";
        conf["loop.intMat11"]             = "3";
        conf["loop.gain"]                 = "0.3";
        conf["loop.gains"]                = "0.7,0.8";
        conf["loop.upstream"]             = "upLoop";
        conf["loop.upstreamProperty"]     = "upState";
        conf["loop.upstreamFollowClosed"] = "true";
        writeConfig( confPath, conf );

        closedLoopIndi_test app( "loop" );
        REQUIRE( app.configure( confPath ) == 0 );
        REQUIRE( app.shutdown() == 0 );

        REQUIRE( app.m_inputElements == std::vector<std::string>( { "dx", "dy" } ) );
        REQUIRE( app.m_inputCounterElement == "frame" );
        REQUIRE( app.m_references( 0, 0 ) == Approx( 0.1f ) );
        REQUIRE( app.m_references( 1, 0 ) == Approx( -0.2f ) );
        REQUIRE( app.m_ctrlCurrents == std::vector<std::string>( { "pos", "pos2" } ) );
        REQUIRE( app.m_ctrlTargets == std::vector<std::string>( { "tgt", "tgt2" } ) );
        REQUIRE( app.m_operatingOK == true );
        REQUIRE( app.m_intMat( 0, 0 ) == Approx( 2.0f ) );
        REQUIRE( app.m_intMat( 0, 1 ) == Approx( 0.5f ) );
        REQUIRE( app.m_intMat( 1, 0 ) == Approx( -0.5f ) );
        REQUIRE( app.m_intMat( 1, 1 ) == Approx( 3.0f ) );
        REQUIRE( app.m_ggain == Approx( 0.3f ) );
        REQUIRE( app.m_defaultGains.size() == 2 );
        REQUIRE( app.m_defaultGains[0] == Approx( 0.7f ) );
        REQUIRE( app.m_defaultGains[1] == Approx( 0.8f ) );
        REQUIRE( app.m_upstreamDevice == "upLoop" );
        REQUIRE( app.m_upstreamProperty == "upState" );
        REQUIRE( app.m_upstreamFollowClosed == true );
    }

    SECTION( "a single ctrl device is used for both axes" )
    {
        confMapT conf        = standardConfig();
        conf["ctrl.devices"] = "stage";
        writeConfig( confPath, conf );

        closedLoopIndi_test app( "loop" );
        REQUIRE( app.configure( confPath ) == 0 );
        REQUIRE( app.m_ctrlDevices == std::vector<std::string>( { "stage", "stage" } ) );
    }

    SECTION( "loadConfig() wraps loadConfigImpl()" )
    {
        writeConfig( confPath, standardConfig() );

        closedLoopIndi_test app( "loop" );
        app.configureVoid( confPath );
        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_inputDevice == "inputdev" );
    }

    std::remove( confPath.c_str() );
}

/// Verify `loadConfigImpl()` rejects inconsistent configurations and sets the shutdown flag.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi configuration errors", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    confMapT conf = standardConfig();

    SECTION( "no input device" )
    {
        conf.erase( "input.device" );
    }

    SECTION( "no input property" )
    {
        conf.erase( "input.property" );
    }

    SECTION( "three input elements" )
    {
        conf["input.elements"] = "x,y,z";
    }

    SECTION( "one reference" )
    {
        conf["input.references"] = "1";
    }

    SECTION( "no ctrl devices" )
    {
        conf.erase( "ctrl.devices" );
    }

    SECTION( "three ctrl devices" )
    {
        conf["ctrl.devices"] = "a,b,c";
    }

    SECTION( "one ctrl property" )
    {
        conf["ctrl.properties"] = "prop0";
    }

    SECTION( "one ctrl target" )
    {
        conf["ctrl.targets"] = "target";
    }

    SECTION( "one ctrl current" )
    {
        conf["ctrl.currents"] = "current";
    }

    writeConfig( confPath, conf );

    closedLoopIndi_test app( "loop" );
    REQUIRE( app.configure( confPath ) == -1 );
    REQUIRE( app.shutdown() == 1 );

    std::remove( confPath.c_str() );
}

/// Verify `appStartup()` creates the properties, registers the controllers, and expands the gains.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi appStartup", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::appStartup();
    closedLoopIndi::appShutdown();
    #endif
    // clang-format on

    SECTION( "standard configuration" )
    {
        confMapT conf            = standardConfig();
        conf["input.references"] = "0.25,-0.5";
        conf["loop.gain"]        = "0.4";

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );

        // A single loop.gains value is applied to both axes.
        REQUIRE( app.m_defaultGains.size() == 2 );
        REQUIRE( app.m_gains.size() == 2 );
        REQUIRE( app.m_gains[0] == Approx( 0.5f ) );
        REQUIRE( app.m_gains[1] == Approx( 0.5f ) );

        REQUIRE( app.m_currents.size() == 2 );
        REQUIRE( app.m_currents[0] < -1e14 );
        REQUIRE( app.m_currents[1] < -1e14 );

        REQUIRE( app.m_measurements.rows() == 2 );
        REQUIRE( app.m_measurements.cols() == 1 );
        REQUIRE( ( app.m_measurements == 0.0f ).all() );
        REQUIRE( app.m_commands.rows() == 2 );
        REQUIRE( app.m_commands.cols() == 2 );
        REQUIRE( ( app.m_commands == 0.0f ).all() );

        REQUIRE( app.m_indiP_reference0.getDevice() == "loop" );
        REQUIRE( app.m_indiP_reference0.getName() == "reference0" );
        REQUIRE( app.m_indiP_reference0["current"].get<float>() == Approx( 0.25f ) );
        REQUIRE( app.m_indiP_reference0["target"].get<float>() == Approx( 0.25f ) );
        REQUIRE( app.m_indiP_reference1.getName() == "reference1" );
        REQUIRE( app.m_indiP_reference1["current"].get<float>() == Approx( -0.5f ) );

        REQUIRE( app.m_indiP_ggain.getName() == "loop_gain" );
        REQUIRE( app.m_indiP_ggain["current"].get<float>() == Approx( 0.4f ) );
        REQUIRE( app.m_indiP_ctrlEnabled.getName() == "loop_state" );
        REQUIRE( app.m_indiP_ctrlEnabled["toggle"].getSwitchState() == pcf::IndiElement::Off );
        REQUIRE( app.m_indiP_counterReset.getName() == "counter_reset" );
        REQUIRE( app.m_indiP_deltas.getName() == "deltas" );
        REQUIRE( app.m_indiP_deltas.find( "delta0" ) );
        REQUIRE( app.m_indiP_deltas.find( "delta1" ) );

        REQUIRE( app.m_indiP_inputs.getDevice() == "inputdev" );
        REQUIRE( app.m_indiP_inputs.getName() == "measurement" );
        REQUIRE( app.m_indiP_ctrl0_fsm.getDevice() == "ctrl0dev" );
        REQUIRE( app.m_indiP_ctrl0_fsm.getName() == "fsm" );
        REQUIRE( app.m_indiP_ctrl0.getDevice() == "ctrl0dev" );
        REQUIRE( app.m_indiP_ctrl0.getName() == "prop0" );
        REQUIRE( app.m_indiP_ctrl1_fsm.getDevice() == "ctrl1dev" );
        REQUIRE( app.m_indiP_ctrl1_fsm.getName() == "fsm" );
        REQUIRE( app.m_indiP_ctrl1.getDevice() == "ctrl1dev" );
        REQUIRE( app.m_indiP_ctrl1.getName() == "prop1" );

        // No upstream loop configured.
        REQUIRE( app.m_indiP_upstream.getName() == "" );

        REQUIRE( app.appShutdown() == 0 );
    }

    SECTION( "a shared ctrl device registers one fsm property" )
    {
        confMapT conf        = standardConfig();
        conf["ctrl.devices"] = "stage";

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );

        REQUIRE( app.m_indiP_ctrl0_fsm.getDevice() == "stage" );
        REQUIRE( app.m_indiP_ctrl1_fsm.getName() == "" );
        REQUIRE( app.m_indiP_ctrl0.getDevice() == "stage" );
        REQUIRE( app.m_indiP_ctrl1.getDevice() == "stage" );
        REQUIRE( app.m_indiP_ctrl1.getName() == "prop1" );
    }

    SECTION( "an upstream loop is monitored" )
    {
        confMapT conf                 = standardConfig();
        conf["loop.upstream"]         = "upLoop";
        conf["loop.upstreamProperty"] = "upState";

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );

        REQUIRE( app.m_indiP_upstream.getDevice() == "upLoop" );
        REQUIRE( app.m_indiP_upstream.getName() == "upState" );
    }

    SECTION( "per-axis gains are kept" )
    {
        confMapT conf      = standardConfig();
        conf["loop.gains"] = "0.2,0.9";

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );

        REQUIRE( app.m_gains.size() == 2 );
        REQUIRE( app.m_gains[0] == Approx( 0.2f ) );
        REQUIRE( app.m_gains[1] == Approx( 0.9f ) );
    }

    SECTION( "three gains are rejected" )
    {
        confMapT conf      = standardConfig();
        conf["loop.gains"] = "0.2,0.9,1.0";

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == -1 );
    }

    SECTION( "no gains are rejected" )
    {
        confMapT conf = standardConfig();
        conf.erase( "loop.gains" );

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == -1 );
    }
}

/// Verify `appLogic()` reports READY with the loop open and OPERATING with it closed.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi appLogic state", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::appLogic();
    #endif
    // clang-format on

    closedLoopIndi_test app( "loop" );
    REQUIRE( startStandard( app ) == 0 );

    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.state() == stateCodes::READY );

    app.m_loopClosed = true;
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.state() == stateCodes::OPERATING );

    app.m_loopClosed = false;
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.state() == stateCodes::READY );
}

/// Verify `toggleLoop()` changes the loop state only on a transition.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi toggleLoop state transitions", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::toggleLoop( true );
    #endif
    // clang-format on

    closedLoopIndi_test app( "loop" );
    REQUIRE( startStandard( app ) == 0 );

    REQUIRE( app.m_loopClosed == false );

    REQUIRE( app.toggleLoop( false ) == 0 );
    REQUIRE( app.m_loopClosed == false );

    REQUIRE( app.toggleLoop( true ) == 0 );
    REQUIRE( app.m_loopClosed == true );

    REQUIRE( app.toggleLoop( true ) == 0 );
    REQUIRE( app.m_loopClosed == true );

    REQUIRE( app.toggleLoop( false ) == 0 );
    REQUIRE( app.m_loopClosed == false );
}

/// Verify `updateLoop()` waits for ready controllers and applies the interaction matrix to the residuals.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi updateLoop control law", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::updateLoop();
    closedLoopIndi::sendCommands( std::vector<float>() );
    #endif
    // clang-format on

    confMapT conf            = standardConfig();
    conf["input.references"] = "0.1,0.2";
    conf["loop.intMat00"]    = "1";
    conf["loop.intMat01"]    = "0.5";
    conf["loop.intMat10"]    = "0";
    conf["loop.intMat11"]    = "2";

    closedLoopIndi_test app( "loop" );
    REQUIRE( startStandard( app, conf ) == 0 );

    app.m_measurements( 0, 0 ) = 1.1f;
    app.m_measurements( 1, 0 ) = 0.7f;
    app.m_currents             = { 10.0f, 20.0f };

    SECTION( "no controller states: nothing is computed" )
    {
        REQUIRE( app.updateLoop() == 0 );
        REQUIRE( app.m_delta0 == 0.0f );
        REQUIRE( app.m_delta1 == 0.0f );
        REQUIRE( app.m_commands.cols() == 2 );
    }

    SECTION( "only one controller ready: nothing is computed" )
    {
        app.m_fsmStates["ctrl0dev"] = "READY";
        REQUIRE( app.updateLoop() == 0 );
        REQUIRE( app.m_delta0 == 0.0f );
    }

    SECTION( "a controller not ready: nothing is computed" )
    {
        app.m_fsmStates["ctrl0dev"] = "READY";
        app.m_fsmStates["ctrl1dev"] = "NOTHOMED";
        REQUIRE( app.updateLoop() == 0 );
        REQUIRE( app.m_delta0 == 0.0f );
    }

    SECTION( "OPERATING controllers are not ready unless operatingOK" )
    {
        app.m_fsmStates["ctrl0dev"] = "OPERATING";
        app.m_fsmStates["ctrl1dev"] = "READY";
        REQUIRE( app.updateLoop() == 0 );
        REQUIRE( app.m_delta0 == 0.0f );

        app.m_operatingOK = true;
        REQUIRE( app.updateLoop() == 0 );
        REQUIRE( app.m_delta0 == Approx( 1.0f ) );
    }

    SECTION( "ready controllers, loop open: residuals and commands are computed" )
    {
        app.m_fsmStates["ctrl0dev"] = "READY";
        app.m_fsmStates["ctrl1dev"] = "READY";

        REQUIRE( app.updateLoop() == 0 );

        REQUIRE( app.m_delta0 == Approx( 1.0f ) );
        REQUIRE( app.m_delta1 == Approx( 0.5f ) );
        REQUIRE( app.m_commands.rows() == 2 );
        REQUIRE( app.m_commands.cols() == 1 );
        REQUIRE( app.m_commands( 0, 0 ) == Approx( 1.0f * 1.0f + 0.5f * 0.5f ) );
        REQUIRE( app.m_commands( 1, 0 ) == Approx( 0.0f * 1.0f + 2.0f * 0.5f ) );
    }

    SECTION( "ready controllers, loop closed: commands are sent" )
    {
        app.m_fsmStates["ctrl0dev"] = "READY";
        app.m_fsmStates["ctrl1dev"] = "READY";
        app.m_loopClosed            = true;
        app.m_ggain                 = 0.5f;

        // Without an INDI driver the sends fail and are logged, but sendCommands() still reports success.
        REQUIRE( app.updateLoop() == 0 );
        REQUIRE( app.m_commands( 0, 0 ) == Approx( 1.25f ) );
        REQUIRE( app.m_commands( 1, 0 ) == Approx( 1.0f ) );
    }
}

/// Verify the input set callback updates the measurements only for a new frame counter.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi input callback", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::setCallBack_m_indiP_inputs( pcf::IndiProperty() );
    #endif
    // clang-format on

    closedLoopIndi_test app( "loop" );
    REQUIRE( startStandard( app ) == 0 );

    SECTION( "a new counter updates the measurements" )
    {
        REQUIRE( app.setCallBack_m_indiP_inputs( inputProp( 1.5f, -2.5f, 7 ) ) == 0 );
        REQUIRE( app.m_counter == 7 );
        REQUIRE( app.m_measurements( 0, 0 ) == Approx( 1.5f ) );
        REQUIRE( app.m_measurements( 1, 0 ) == Approx( -2.5f ) );

        // The same counter is ignored.
        REQUIRE( app.setCallBack_m_indiP_inputs( inputProp( 3.0f, 4.0f, 7 ) ) == 0 );
        REQUIRE( app.m_measurements( 0, 0 ) == Approx( 1.5f ) );

        // A different counter is accepted.
        REQUIRE( app.setCallBack_m_indiP_inputs( inputProp( 3.0f, 4.0f, 8 ) ) == 0 );
        REQUIRE( app.m_counter == 8 );
        REQUIRE( app.m_measurements( 0, 0 ) == Approx( 3.0f ) );
        REQUIRE( app.m_measurements( 1, 0 ) == Approx( 4.0f ) );
    }

    SECTION( "a new frame runs the loop when the controllers are ready" )
    {
        app.m_fsmStates["ctrl0dev"] = "READY";
        app.m_fsmStates["ctrl1dev"] = "READY";

        REQUIRE( app.setCallBack_m_indiP_inputs( inputProp( 0.5f, 0.25f, 1 ) ) == 0 );
        REQUIRE( app.m_delta0 == Approx( 0.5f ) );
        REQUIRE( app.m_delta1 == Approx( 0.25f ) );
        REQUIRE( app.m_commands( 0, 0 ) == Approx( 0.5f ) );
        REQUIRE( app.m_commands( 1, 0 ) == Approx( 0.25f ) );
    }

    SECTION( "missing elements are rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_inputs(
                     numberProp( "inputdev", "measurement", { "x", "counter" }, { 1.0f, 3.0f } ) ) == -1 );
        REQUIRE( app.setCallBack_m_indiP_inputs(
                     numberProp( "inputdev", "measurement", { "y", "counter" }, { 1.0f, 3.0f } ) ) == -1 );
        REQUIRE( app.setCallBack_m_indiP_inputs(
                     numberProp( "inputdev", "measurement", { "x", "y" }, { 1.0f, 3.0f } ) ) == -1 );
        REQUIRE( app.m_counter == -1 );
    }

    SECTION( "wrong device is rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_inputs(
                     numberProp( "other", "measurement", { "x", "y", "counter" }, { 1.0f, 2.0f, 3.0f } ) ) == -1 );
        REQUIRE( app.m_counter == -1 );
    }
}

/// Verify the reference, gain, loop state and counter reset new-property callbacks.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi new-property callbacks", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::newCallBack_m_indiP_reference0( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_reference1( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_ggain( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_ctrlEnabled( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_counterReset( pcf::IndiProperty() );
    #endif
    // clang-format on

    closedLoopIndi_test app( "loop" );
    REQUIRE( startStandard( app ) == 0 );

    SECTION( "reference0 target" )
    {
        REQUIRE( app.newCallBack_m_indiP_reference0( numberProp( "loop", "reference0", { "target" }, { 1.25f } ) ) ==
                 0 );
        REQUIRE( app.m_references( 0, 0 ) == Approx( 1.25f ) );
        REQUIRE( app.m_references( 1, 0 ) == 0.0f );
    }

    SECTION( "reference1 current fallback" )
    {
        REQUIRE( app.newCallBack_m_indiP_reference1( numberProp( "loop", "reference1", { "current" }, { -0.75f } ) ) ==
                 0 );
        REQUIRE( app.m_references( 1, 0 ) == Approx( -0.75f ) );
        REQUIRE( app.m_references( 0, 0 ) == 0.0f );
    }

    SECTION( "reference with no value is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_reference0( numberProp( "loop", "reference0", { "other" }, { 1.0f } ) ) ==
                 -1 );
        REQUIRE( app.newCallBack_m_indiP_reference1( numberProp( "loop", "reference1", { "other" }, { 1.0f } ) ) ==
                 -1 );
        REQUIRE( app.m_references( 0, 0 ) == 0.0f );
        REQUIRE( app.m_references( 1, 0 ) == 0.0f );
    }

    SECTION( "reference from the wrong device is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_reference0( numberProp( "other", "reference0", { "target" }, { 1.0f } ) ) ==
                 -1 );
        REQUIRE( app.m_references( 0, 0 ) == 0.0f );
    }

    SECTION( "global gain" )
    {
        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "loop", "loop_gain", { "target" }, { 0.6f } ) ) == 0 );
        REQUIRE( app.m_ggain == Approx( 0.6f ) );

        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "loop", "loop_gain", { "other" }, { 0.1f } ) ) == -1 );
        REQUIRE( app.m_ggain == Approx( 0.6f ) );

        REQUIRE( app.newCallBack_m_indiP_ggain( numberProp( "loop", "ggain", { "target" }, { 0.1f } ) ) == -1 );
        REQUIRE( app.m_ggain == Approx( 0.6f ) );
    }

    SECTION( "loop state toggle" )
    {
        REQUIRE( app.newCallBack_m_indiP_ctrlEnabled(
                     switchProp( "loop", "loop_state", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_loopClosed == true );

        REQUIRE( app.newCallBack_m_indiP_ctrlEnabled(
                     switchProp( "loop", "loop_state", "toggle", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_loopClosed == false );

        REQUIRE( app.newCallBack_m_indiP_ctrlEnabled(
                     switchProp( "other", "loop_state", "toggle", pcf::IndiElement::On ) ) == -1 );
        REQUIRE( app.m_loopClosed == false );
    }

    SECTION( "counter reset" )
    {
        app.m_counter = 42;

        REQUIRE( app.newCallBack_m_indiP_counterReset(
                     switchProp( "loop", "counter_reset", "request", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_counter == 42 );

        REQUIRE( app.newCallBack_m_indiP_counterReset(
                     switchProp( "loop", "counter_reset", "request", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_counter == -1 );

        // After a reset the next frame is accepted even if the counter restarted.
        REQUIRE( app.setCallBack_m_indiP_inputs( inputProp( 1.0f, 2.0f, 0 ) ) == 0 );
        REQUIRE( app.m_counter == 0 );
    }
}

/// Verify the controller FSM and current-value set callbacks.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi controller callbacks", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::setCallBack_m_indiP_ctrl0_fsm( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_ctrl0( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_ctrl1_fsm( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_ctrl1( pcf::IndiProperty() );
    #endif
    // clang-format on

    SECTION( "fsm states are stored per device" )
    {
        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app ) == 0 );

        REQUIRE( app.setCallBack_m_indiP_ctrl0_fsm( fsmProp( "ctrl0dev", "READY" ) ) == 0 );
        REQUIRE( app.setCallBack_m_indiP_ctrl1_fsm( fsmProp( "ctrl1dev", "OPERATING" ) ) == 0 );
        REQUIRE( app.m_fsmStates["ctrl0dev"] == "READY" );
        REQUIRE( app.m_fsmStates["ctrl1dev"] == "OPERATING" );

        // A property without a state element is ignored.
        pcf::IndiProperty ip( pcf::IndiProperty::Text );
        ip.setDevice( "ctrl0dev" );
        ip.setName( "fsm" );
        REQUIRE( app.setCallBack_m_indiP_ctrl0_fsm( ip ) == 0 );
        REQUIRE( app.m_fsmStates["ctrl0dev"] == "READY" );

        // The wrong device is rejected.
        REQUIRE( app.setCallBack_m_indiP_ctrl0_fsm( fsmProp( "ctrl1dev", "ERROR" ) ) == -1 );
        REQUIRE( app.m_fsmStates["ctrl1dev"] == "OPERATING" );
    }

    SECTION( "current values are cached per axis" )
    {
        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app ) == 0 );

        REQUIRE( app.setCallBack_m_indiP_ctrl0( numberProp( "ctrl0dev", "prop0", { "current" }, { 3.5f } ) ) == 0 );
        REQUIRE( app.m_currents[0] == Approx( 3.5f ) );
        REQUIRE( app.m_currents[1] < -1e14 );

        REQUIRE( app.setCallBack_m_indiP_ctrl1( numberProp( "ctrl1dev", "prop1", { "current" }, { -4.5f } ) ) == 0 );
        REQUIRE( app.m_currents[1] == Approx( -4.5f ) );

        // Missing current element is ignored.
        REQUIRE( app.setCallBack_m_indiP_ctrl0( numberProp( "ctrl0dev", "prop0", { "target" }, { 9.0f } ) ) == 0 );
        REQUIRE( app.m_currents[0] == Approx( 3.5f ) );

        // Wrong property is rejected.
        REQUIRE( app.setCallBack_m_indiP_ctrl1( numberProp( "ctrl1dev", "prop0", { "current" }, { 9.0f } ) ) == -1 );
        REQUIRE( app.m_currents[1] == Approx( -4.5f ) );
    }

    SECTION( "configured current element names are used" )
    {
        confMapT conf         = standardConfig();
        conf["ctrl.currents"] = "pos,angle";

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );

        REQUIRE( app.setCallBack_m_indiP_ctrl0( numberProp( "ctrl0dev", "prop0", { "current" }, { 1.0f } ) ) == 0 );
        REQUIRE( app.m_currents[0] < -1e14 );

        REQUIRE( app.setCallBack_m_indiP_ctrl0( numberProp( "ctrl0dev", "prop0", { "pos" }, { 1.0f } ) ) == 0 );
        REQUIRE( app.m_currents[0] == Approx( 1.0f ) );

        REQUIRE( app.setCallBack_m_indiP_ctrl1( numberProp( "ctrl1dev", "prop1", { "angle" }, { 2.0f } ) ) == 0 );
        REQUIRE( app.m_currents[1] == Approx( 2.0f ) );
    }
}

/// Verify the upstream loop set callback follows the upstream loop state.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi upstream loop callback", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::setCallBack_m_indiP_upstream( pcf::IndiProperty() );
    #endif
    // clang-format on

    confMapT conf         = standardConfig();
    conf["loop.upstream"] = "upLoop";

    SECTION( "upstream closing does not close this loop by default" )
    {
        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );

        REQUIRE( app.setCallBack_m_indiP_upstream(
                     switchProp( "upLoop", "loop_state", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_loopClosed == false );
    }

    SECTION( "upstream closing closes this loop when following closed" )
    {
        conf["loop.upstreamFollowClosed"] = "true";

        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );

        REQUIRE( app.setCallBack_m_indiP_upstream(
                     switchProp( "upLoop", "loop_state", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_loopClosed == true );
    }

    SECTION( "upstream opening always opens this loop" )
    {
        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );
        app.m_loopClosed = true;

        REQUIRE( app.setCallBack_m_indiP_upstream(
                     switchProp( "upLoop", "loop_state", "toggle", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_loopClosed == false );
    }

    SECTION( "no toggle element or wrong device" )
    {
        closedLoopIndi_test app( "loop" );
        REQUIRE( startStandard( app, conf ) == 0 );
        app.m_loopClosed = true;

        REQUIRE( app.setCallBack_m_indiP_upstream(
                     switchProp( "upLoop", "loop_state", "other", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_loopClosed == true );

        REQUIRE( app.setCallBack_m_indiP_upstream(
                     switchProp( "other", "loop_state", "toggle", pcf::IndiElement::Off ) ) == -1 );
        REQUIRE( app.m_loopClosed == true );
    }
}

} // namespace closedLoopIndiTest

} // namespace libXWCTest
