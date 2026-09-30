/** \file kcubeCtrl_test.cpp
 * \brief Catch2 tests for the kcubeCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * The app's `#include "tmcController.hpp"` resolves to `stubs/tmcController.hpp`, whose member functions are
 * defined here and drive per-controller fake state.  testMacrosINDI.hpp is included after the app header, so the
 * INDI callback bodies are live; the validation macros still apply because every callback returns 0 for an empty
 * property when the app is not READY or OPERATING.
 *
 * \ingroup kcubeCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <map>
#include <string>
#include <vector>

#include <unistd.h>

#include "../kcubeCtrl.hpp"

#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Fake state of one stubbed tmcController.
struct tmcAxisStub
{
    std::map<std::string, int> m_rv; ///< Return value of each member function by name (0 if not set).

    std::vector<std::string> m_calls; ///< Names of the member functions called, in order.

    int m_vendor{ 0x0403 }; ///< Vendor id reported by vendor().

    int m_product{ 0xfaf0 }; ///< Product id reported by product().

    bool m_lastQuiet{ false }; ///< The quiet flag passed to the last open().

    int m_lastChan{ -1 }; ///< Channel passed to the last mod_set_chanenablestate().

    tmcController::EnableState m_enable{ tmcController::EnableState::disabled }; ///< The last enable state set.

    float m_outputVolts{ 0 }; ///< Output voltage fraction reported by pz_req_outputvolts().

    float m_lastSetVolts{ -1 }; ///< Output voltage fraction passed to the last pz_set_outputvolts().

    tmcController::KMMIParams m_mmi; ///< Device-side MMI parameters.

    tmcController::TPZIOSettings m_tios; ///< Device-side piezo I/O settings.

    int m_errmsgCalls{ 0 }; ///< Number of base-class error message calls.
};

/// Access the fake state of every stubbed controller, keyed by controller address.
std::map<const tmcController *, tmcAxisStub> &tmcStubs()
{
    static std::map<const tmcController *, tmcAxisStub> stubs;
    return stubs;
}

/// Access the fake state of one controller.
tmcAxisStub &tmcStub( const tmcController *c /**< [in] the controller */ )
{
    return tmcStubs()[c];
}

/// Record a call and return the configured return value.
/**
 * \returns the configured return value for `name`, or 0
 */
int tmcCall( const tmcController *c, /**< [in] the controller */
             const std::string   &name /**< [in] the member function name */ )
{
    tmcAxisStub &st = tmcStub( c );
    st.m_calls.push_back( name );

    auto it = st.m_rv.find( name );
    if( it == st.m_rv.end() )
    {
        return 0;
    }

    return it->second;
}

/// Number of ftdi_get_error_string calls.
int &ftdiErrorStringCalls()
{
    static int calls = 0;
    return calls;
}

const char *ftdi_get_error_string( struct ftdi_context *ftdi )
{
    static_cast<void>( ftdi );
    ++ftdiErrorStringCalls();
    return "stub ftdi error";
}

void tmcController::HWInfo::dump( std::ostream &os ) const
{
    os << "serial: " << serialNumber << " model: " << modelNumber;
}

void tmcController::KMMIParams::dump( std::ostream &os ) const
{
    os << "DispBrightness: " << DispBrightness;
}

void tmcController::TPZIOSettings::dump( std::ostream &os ) const
{
    os << "VoltageLimit: " << static_cast<int>( VoltageLimit );
}

tmcController::tmcController()
{
}

tmcController::~tmcController()
{
    tmcStubs().erase( this );
}

std::string tmcController::serial() const
{
    return m_serial;
}

void tmcController::serial( const std::string &ser )
{
    m_serial = ser;
}

int tmcController::vendor() const
{
    return tmcStub( this ).m_vendor;
}

int tmcController::product() const
{
    return tmcStub( this ).m_product;
}

int tmcController::open( bool quiet )
{
    tmcStub( this ).m_lastQuiet = quiet;
    return tmcCall( this, "open" );
}

int tmcController::connect()
{
    return tmcCall( this, "connect" );
}

int tmcController::hw_req_info( HWInfo &hwi )
{
    hwi.serialNumber = 12345678;
    hwi.modelNumber  = "KPZ101";
    return tmcCall( this, "hw_req_info" );
}

int tmcController::hw_stop_updatemsgs()
{
    return tmcCall( this, "hw_stop_updatemsgs" );
}

int tmcController::mod_identify()
{
    return tmcCall( this, "mod_identify" );
}

int tmcController::mod_set_chanenablestate( uint8_t chan, EnableState es )
{
    int rv = tmcCall( this, "mod_set_chanenablestate" );
    if( rv == 0 )
    {
        tmcStub( this ).m_lastChan = chan;
        tmcStub( this ).m_enable   = es;
    }
    return rv;
}

int tmcController::kpz_req_kcubemmiparams( KMMIParams &par )
{
    par = tmcStub( this ).m_mmi;
    return tmcCall( this, "kpz_req_kcubemmiparams" );
}

int tmcController::kpz_set_kcubemmiparams( const KMMIParams &par )
{
    int rv = tmcCall( this, "kpz_set_kcubemmiparams" );
    if( rv == 0 )
    {
        tmcStub( this ).m_mmi = par;
    }
    return rv;
}

int tmcController::pz_req_tpz_iosettings( TPZIOSettings &tios )
{
    tios = tmcStub( this ).m_tios;
    return tmcCall( this, "pz_req_tpz_iosettings" );
}

int tmcController::pz_set_tpz_iosettings( const TPZIOSettings &tios )
{
    int rv = tmcCall( this, "pz_set_tpz_iosettings" );
    if( rv == 0 )
    {
        tmcStub( this ).m_tios = tios;
    }
    return rv;
}

int tmcController::pz_req_outputvolts( float &ov )
{
    ov = tmcStub( this ).m_outputVolts;
    return tmcCall( this, "pz_req_outputvolts" );
}

int tmcController::pz_set_outputvolts( float ov )
{
    int rv = tmcCall( this, "pz_set_outputvolts" );
    if( rv == 0 )
    {
        tmcStub( this ).m_lastSetVolts = ov;
    }
    return rv;
}

void tmcController::ftdiErrmsg(
    const std::string &src, const std::string &msg, int rv, const std::string &file, int line )
{
    static_cast<void>( src );
    static_cast<void>( msg );
    static_cast<void>( rv );
    static_cast<void>( file );
    static_cast<void>( line );
    ++tmcStub( this ).m_errmsgCalls;
}

void tmcController::otherErrmsg( const std::string &src, const std::string &msg, const std::string &file, int line )
{
    static_cast<void>( src );
    static_cast<void>( msg );
    static_cast<void>( file );
    static_cast<void>( line );
    ++tmcStub( this ).m_errmsgCalls;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup kcubeCtrl_unit_test kcubeCtrl Unit Tests
 * \brief Unit tests for the kcubeCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `kcubeCtrl` unit tests.
/** \ingroup kcubeCtrl_unit_test
 */
namespace kcubeCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing the kcubeCtrl internals, with the INDI properties given their device and names.
class kcubeCtrl_test : public kcubeCtrl
{
  public:
    using kcubeCtrl::m_axis1Enabled;
    using kcubeCtrl::m_axis2Enabled;
    using kcubeCtrl::m_isSet;
    using kcubeCtrl::m_kAxis1;
    using kcubeCtrl::m_kAxis2;
    using MagAOXApp<true>::config;
    using MagAOXApp<true>::m_powerMgtEnabled;
    using MagAOXApp<true>::m_powerState;

    /// Construct a harness with the given device name.
    explicit kcubeCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( axis1_identify );
        XWCTEST_SETUP_INDI_NEW_PROP( axis1_enable );
        XWCTEST_SETUP_INDI_NEW_PROP( axis1_voltage );
        XWCTEST_SETUP_INDI_NEW_PROP( axis2_identify );
        XWCTEST_SETUP_INDI_NEW_PROP( axis2_enable );
        XWCTEST_SETUP_INDI_NEW_PROP( axis2_voltage );
        XWCTEST_SETUP_INDI_NEW_PROP( set );
    }

    /// Fake state of the axis 1 controller.
    tmcAxisStub &axis1()
    {
        return tmcStub( &m_kAxis1 );
    }

    /// Fake state of the axis 2 controller.
    tmcAxisStub &axis2()
    {
        return tmcStub( &m_kAxis2 );
    }
};

/// Write a config file, read it into the app configurator, and remove the file.
/**
 * \returns the return value of readConfig()
 */
int readTestConfig( kcubeCtrl_test                 &app,      /**< [in,out] the app, with setupConfig() called */
                    const std::string              &tag,      /**< [in] a unique tag for the file name */
                    const std::vector<std::string> &sections, /**< [in] the config sections */
                    const std::vector<std::string> &keywords, /**< [in] the config keywords */
                    const std::vector<std::string> &values /**< [in] the config values */ )
{
    std::string fname = "/tmp/kcubeCtrl_test_" + tag + "_" + std::to_string( ::getpid() ) + ".conf";
    mx::app::writeConfigFile( fname, sections, keywords, values );
    int rv = app.config.readConfig( fname );
    ::unlink( fname.c_str() );
    return rv;
}

/// Build a switch property for the harness device.
pcf::IndiProperty switchProp( const std::string &name,    /**< [in] the property name */
                              const std::string &element, /**< [in] the switch element name */
                              bool               on /**< [in] the switch state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "kcube" );
    ip.setName( name );
    ip.add( pcf::IndiElement( element ) );
    ip[element].setSwitchState( on ? pcf::IndiElement::On : pcf::IndiElement::Off );
    return ip;
}

/// Build a number property with a target element for the harness device.
pcf::IndiProperty numberProp( const std::string &name, /**< [in] the property name */
                              float              target /**< [in] the target value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "kcube" );
    ip.setName( name );
    ip.add( pcf::IndiElement( "target" ) );
    ip["target"].set( target );
    return ip;
}

/// The call sequence of axisNInitialize().
const std::vector<std::string> &initCalls()
{
    static const std::vector<std::string> calls = { "hw_req_info",
                                                    "mod_set_chanenablestate",
                                                    "hw_stop_updatemsgs",
                                                    "kpz_req_kcubemmiparams",
                                                    "kpz_set_kcubemmiparams",
                                                    "kpz_req_kcubemmiparams",
                                                    "pz_req_tpz_iosettings",
                                                    "pz_set_tpz_iosettings",
                                                    "pz_req_tpz_iosettings" };
    return calls;
}

/// \endcond

/// Verify the kcubeCtrl INDI callback validators accept only the expected properties.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl INDI callbacks validate device and property names", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::newCallBack_m_indiP_axis1_identify( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis1_enable( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis1_voltage( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis2_identify( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis2_enable( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis2_voltage( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_set( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( kcubeCtrl, axis1_identify );
    XWCTEST_INDI_NEW_CALLBACK( kcubeCtrl, axis1_enable );
    XWCTEST_INDI_NEW_CALLBACK( kcubeCtrl, axis1_voltage );
    XWCTEST_INDI_NEW_CALLBACK( kcubeCtrl, axis2_identify );
    XWCTEST_INDI_NEW_CALLBACK( kcubeCtrl, axis2_enable );
    XWCTEST_INDI_NEW_CALLBACK( kcubeCtrl, axis2_voltage );
    XWCTEST_INDI_NEW_CALLBACK( kcubeCtrl, set );
}

/// Verify the constructor defaults and the trivial hooks.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl construction defaults", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::kcubeCtrl();
    kcubeCtrl::modIdentify();
    kcubeCtrl::appShutdown();
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );

    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_axis1Enabled == false );
    REQUIRE( app.m_axis2Enabled == false );
    REQUIRE( app.m_isSet == false );
    REQUIRE( app.m_kAxis1.serial() == "" );
    REQUIRE( app.m_kAxis2.serial() == "" );
    REQUIRE( app.modIdentify() == 0 );
    REQUIRE( app.appShutdown() == 0 );
}

/// Verify the configuration of the axis serial numbers.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl configuration", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::setupConfig();
    kcubeCtrl::loadConfigImpl( mx::app::appConfigurator() );
    kcubeCtrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "defaults leave the serial numbers unchanged" )
    {
        kcubeCtrl_test app( "kcube" );
        app.m_kAxis1.serial( "preset1" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "defaults", { "none" }, { "nada" }, { "0" } ) == 0 );
        app.loadConfig();

        REQUIRE( app.m_kAxis1.serial() == "preset1" );
        REQUIRE( app.m_kAxis2.serial() == "" );
    }

    SECTION( "the serial numbers are set per axis" )
    {
        kcubeCtrl_test app( "kcube" );
        app.setupConfig();
        REQUIRE( readTestConfig(
                     app, "serials", { "axis1", "axis2" }, { "serial", "serial" }, { "29250001", "29250002" } ) == 0 );
        REQUIRE( app.loadConfigImpl( app.config ) == 0 );

        REQUIRE( app.m_kAxis1.serial() == "29250001" );
        REQUIRE( app.m_kAxis2.serial() == "29250002" );
    }
}

/// Verify appStartup() creates the INDI properties and moves to NODEVICE.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl appStartup", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::appStartup();
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );
    REQUIRE( app.appStartup() == 0 );
    REQUIRE( app.state() == stateCodes::NODEVICE );

    REQUIRE( app.m_indiP_axis1_identify.getDevice() == "kcube" );
    REQUIRE( app.m_indiP_axis1_identify.getName() == "axis1_identify" );
    REQUIRE( app.m_indiP_axis1_identify.find( "request" ) );
    REQUIRE( app.m_indiP_axis2_identify.getName() == "axis2_identify" );
    REQUIRE( app.m_indiP_axis2_identify.find( "request" ) );

    REQUIRE( app.m_indiP_axis1_enable.getName() == "axis1_enable" );
    REQUIRE( app.m_indiP_axis1_enable.find( "toggle" ) );
    REQUIRE( app.m_indiP_axis2_enable.getName() == "axis2_enable" );
    REQUIRE( app.m_indiP_axis2_enable.find( "toggle" ) );

    REQUIRE( app.m_indiP_axis1_voltage.getName() == "axis1_voltage" );
    REQUIRE( app.m_indiP_axis1_voltage["current"].get<float>() == 0 );
    REQUIRE( app.m_indiP_axis1_voltage["target"].get<float>() == 0 );
    REQUIRE( app.m_indiP_axis2_voltage.getName() == "axis2_voltage" );
    REQUIRE( app.m_indiP_axis2_voltage["current"].get<float>() == 0 );
    REQUIRE( app.m_indiP_axis2_voltage["target"].get<float>() == 0 );

    REQUIRE( app.m_indiP_set.getName() == "set" );
    REQUIRE( app.m_indiP_set.find( "toggle" ) );
}

/// Verify axis initialization disables the output, dims the display and sets the 150 V limit.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl axis initialization", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::axis1Initialize();
    kcubeCtrl::axis2Initialize();
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );
    app.m_axis1Enabled = true;
    app.m_axis2Enabled = true;
    app.m_isSet        = true;

    app.axis1().m_mmi.DispBrightness = 80;
    app.axis2().m_mmi.DispBrightness = 60;
    app.axis1().m_enable             = tmcController::EnableState::enabled;
    app.axis2().m_enable             = tmcController::EnableState::enabled;
    app.axis1().m_tios.VoltageLimit  = tmcController::VoltLimit::V75;
    app.axis2().m_tios.VoltageLimit  = tmcController::VoltLimit::V100;

    SECTION( "axis 1" )
    {
        REQUIRE( app.axis1Initialize() == 0 );
        REQUIRE( app.axis1().m_calls == initCalls() );
        REQUIRE( app.axis1().m_lastChan == 0x01 );
        REQUIRE( app.axis1().m_enable == tmcController::EnableState::disabled );
        REQUIRE( app.axis1().m_mmi.DispBrightness == 0 );
        REQUIRE( app.axis1().m_tios.VoltageLimit == tmcController::VoltLimit::V150 );
        REQUIRE( app.m_axis1Enabled == false );
        REQUIRE( app.m_isSet == false );

        // Axis 2 is untouched.
        REQUIRE( app.axis2().m_calls.empty() );
        REQUIRE( app.m_axis2Enabled == true );
    }

    SECTION( "axis 2" )
    {
        REQUIRE( app.axis2Initialize() == 0 );
        REQUIRE( app.axis2().m_calls == initCalls() );
        REQUIRE( app.axis2().m_lastChan == 0x01 );
        REQUIRE( app.axis2().m_enable == tmcController::EnableState::disabled );
        REQUIRE( app.axis2().m_mmi.DispBrightness == 0 );
        REQUIRE( app.axis2().m_tios.VoltageLimit == tmcController::VoltLimit::V150 );
        REQUIRE( app.m_axis2Enabled == false );
        REQUIRE( app.m_isSet == false );

        REQUIRE( app.axis1().m_calls.empty() );
        REQUIRE( app.m_axis1Enabled == true );
    }

    SECTION( "a failed hardware query stops initialization" )
    {
        // Powered off, so the error return comes right after the 1 s power-state wait.
        app.m_powerState                = 0;
        app.axis1().m_rv["hw_req_info"] = -1;
        REQUIRE( app.axis1Initialize() == -1 );
        REQUIRE( app.axis1().m_calls == std::vector<std::string>{ "hw_req_info" } );
        REQUIRE( app.m_axis1Enabled == true );
    }
}

/// Verify the enable, disable and voltage commands, including voltage clamping and conversion.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl axis enable and voltage commands", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::axis1Enable();
    kcubeCtrl::axis1Disable();
    kcubeCtrl::axis1Voltage( 0.0f );
    kcubeCtrl::axis2Enable();
    kcubeCtrl::axis2Disable();
    kcubeCtrl::axis2Voltage( 0.0f );
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );

    SECTION( "enable and disable axis 1" )
    {
        REQUIRE( app.axis1Enable() == 0 );
        REQUIRE( app.m_axis1Enabled == true );
        REQUIRE( app.axis1().m_enable == tmcController::EnableState::enabled );
        REQUIRE( app.axis1().m_lastChan == 0x01 );

        app.m_isSet = true;
        REQUIRE( app.axis1Disable() == 0 );
        REQUIRE( app.m_axis1Enabled == false );
        REQUIRE( app.m_isSet == false );
        REQUIRE( app.axis1().m_enable == tmcController::EnableState::disabled );
        REQUIRE( app.axis2().m_calls.empty() );
    }

    SECTION( "enable and disable axis 2" )
    {
        REQUIRE( app.axis2Enable() == 0 );
        REQUIRE( app.m_axis2Enabled == true );
        REQUIRE( app.axis2().m_enable == tmcController::EnableState::enabled );

        app.m_isSet = true;
        REQUIRE( app.axis2Disable() == 0 );
        REQUIRE( app.m_axis2Enabled == false );
        REQUIRE( app.m_isSet == false );
        REQUIRE( app.axis2().m_enable == tmcController::EnableState::disabled );
        REQUIRE( app.axis1().m_calls.empty() );
    }

    SECTION( "a failed enable leaves the axis disabled" )
    {
        app.m_powerState                            = 0;
        app.axis1().m_rv["mod_set_chanenablestate"] = -1;
        REQUIRE( app.axis1Enable() == -1 );
        REQUIRE( app.m_axis1Enabled == false );
    }

    SECTION( "voltages are sent as a fraction of 150 V" )
    {
        float v = 75;
        REQUIRE( app.axis1Voltage( v ) == 0 );
        REQUIRE( v == 75 );
        REQUIRE( app.axis1().m_lastSetVolts == Approx( 0.5 ) );

        v = 150;
        REQUIRE( app.axis2Voltage( v ) == 0 );
        REQUIRE( app.axis2().m_lastSetVolts == Approx( 1.0 ) );

        v = 0;
        REQUIRE( app.axis2Voltage( v ) == 0 );
        REQUIRE( app.axis2().m_lastSetVolts == Approx( 0.0 ).margin( 1e-9 ) );
    }

    SECTION( "voltages are clamped to 0 to 150 V" )
    {
        float v = -10;
        REQUIRE( app.axis1Voltage( v ) == 0 );
        REQUIRE( v == 0 );
        REQUIRE( app.axis1().m_lastSetVolts == Approx( 0.0 ).margin( 1e-9 ) );

        v = 200;
        REQUIRE( app.axis1Voltage( v ) == 0 );
        REQUIRE( v == 150 );
        REQUIRE( app.axis1().m_lastSetVolts == Approx( 1.0 ) );

        v = -1;
        REQUIRE( app.axis2Voltage( v ) == 0 );
        REQUIRE( v == 0 );

        v = 151;
        REQUIRE( app.axis2Voltage( v ) == 0 );
        REQUIRE( v == 150 );
        REQUIRE( app.axis2().m_lastSetVolts == Approx( 1.0 ) );
    }
}

/// Verify the set() and rest() sequences.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl set and rest", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::set();
    kcubeCtrl::rest();
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );

    SECTION( "set enables both axes at 75 V" )
    {
        REQUIRE( app.set() == 0 );
        REQUIRE( app.m_isSet == true );
        REQUIRE( app.m_axis1Enabled == true );
        REQUIRE( app.m_axis2Enabled == true );
        REQUIRE( app.axis1().m_calls == std::vector<std::string>{ "mod_set_chanenablestate", "pz_set_outputvolts" } );
        REQUIRE( app.axis2().m_calls == std::vector<std::string>{ "mod_set_chanenablestate", "pz_set_outputvolts" } );
        REQUIRE( app.axis1().m_lastSetVolts == Approx( 0.5 ) );
        REQUIRE( app.axis2().m_lastSetVolts == Approx( 0.5 ) );

        // A second set does nothing.
        REQUIRE( app.set() == 0 );
        REQUIRE( app.axis1().m_calls.size() == 2 );
    }

    SECTION( "rest does nothing when not set" )
    {
        REQUIRE( app.rest() == 0 );
        REQUIRE( app.axis1().m_calls.empty() );
        REQUIRE( app.axis2().m_calls.empty() );
    }

    SECTION( "rest zeroes and disables both axes" )
    {
        REQUIRE( app.set() == 0 );
        app.axis1().m_calls.clear();
        app.axis2().m_calls.clear();

        REQUIRE( app.rest() == 0 );
        REQUIRE( app.m_isSet == false );
        REQUIRE( app.m_axis1Enabled == false );
        REQUIRE( app.m_axis2Enabled == false );
        REQUIRE( app.axis1().m_calls == std::vector<std::string>{ "pz_set_outputvolts", "mod_set_chanenablestate" } );
        REQUIRE( app.axis2().m_calls == std::vector<std::string>{ "pz_set_outputvolts", "mod_set_chanenablestate" } );
        REQUIRE( app.axis1().m_lastSetVolts == Approx( 0.0 ).margin( 1e-9 ) );
        REQUIRE( app.axis2().m_lastSetVolts == Approx( 0.0 ).margin( 1e-9 ) );
        REQUIRE( app.axis1().m_enable == tmcController::EnableState::disabled );
        REQUIRE( app.axis2().m_enable == tmcController::EnableState::disabled );
    }
}

/// Verify the appLogic() state machine driven through the stubbed controllers.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl appLogic", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::appLogic();
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );
    REQUIRE( app.appStartup() == 0 );
    REQUIRE( app.state() == stateCodes::NODEVICE );

    SECTION( "both devices missing stays in NODEVICE" )
    {
        app.axis1().m_rv["open"] = -3;
        app.axis2().m_rv["open"] = -3;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.axis1().m_calls == std::vector<std::string>{ "open" } );
        REQUIRE( app.axis2().m_calls == std::vector<std::string>{ "open" } );
        REQUIRE( app.axis1().m_lastQuiet == false );
    }

    SECTION( "one missing device is an ERROR" )
    {
        app.axis2().m_rv["open"] = -3;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.axis1().m_calls == std::vector<std::string>{ "open" } );
    }

    SECTION( "an open error is an ERROR" )
    {
        app.axis1().m_rv["open"] = -1;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "the full sequence reaches READY in one pass" )
    {
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );

        std::vector<std::string> expected = { "open", "connect" };
        expected.insert( expected.end(), initCalls().begin(), initCalls().end() );
        expected.push_back( "pz_req_outputvolts" );

        REQUIRE( app.axis1().m_calls == expected );
        REQUIRE( app.axis2().m_calls == expected );
        REQUIRE( app.axis1().m_mmi.DispBrightness == 0 );
        REQUIRE( app.axis2().m_tios.VoltageLimit == tmcController::VoltLimit::V150 );
    }

    SECTION( "ERROR retries the open" )
    {
        app.axis1().m_rv["open"] = -1;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );

        app.axis1().m_rv["open"] = 0;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "READY with both axes enabled is OPERATING" )
    {
        app.state( stateCodes::READY );
        app.m_axis1Enabled = true;
        app.m_axis2Enabled = true;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::OPERATING );
        REQUIRE( app.axis1().m_calls == std::vector<std::string>{ "pz_req_outputvolts" } );
        REQUIRE( app.axis2().m_calls == std::vector<std::string>{ "pz_req_outputvolts" } );

        app.m_axis2Enabled = false;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "a failed connect while powered off returns -1" )
    {
        app.state( stateCodes::NOTCONNECTED );
        app.m_powerState            = 0;
        app.axis1().m_rv["connect"] = -1;
        REQUIRE( app.appLogic() == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( app.axis2().m_calls.empty() );
    }

    SECTION( "a failed voltage query while powered on is an ERROR" )
    {
        app.state( stateCodes::READY );
        app.m_powerState                       = 1;
        app.axis1().m_rv["pz_req_outputvolts"] = -1;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.axis2().m_calls.empty() );
    }
}

/// Verify the INDI callbacks drive the axis commands.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl INDI callbacks", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    kcubeCtrl::newCallBack_m_indiP_axis1_identify( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis1_enable( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis1_voltage( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis2_identify( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis2_enable( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_axis2_voltage( pcf::IndiProperty() );
    kcubeCtrl::newCallBack_m_indiP_set( pcf::IndiProperty() );
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );
    REQUIRE( app.appStartup() == 0 );

    SECTION( "requests are ignored before READY" )
    {
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.newCallBack_m_indiP_axis1_identify( switchProp( "axis1_identify", "request", true ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_axis1_enable( switchProp( "axis1_enable", "toggle", true ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_axis2_voltage( numberProp( "axis2_voltage", 50 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_set( switchProp( "set", "toggle", true ) ) == 0 );
        REQUIRE( app.axis1().m_calls.empty() );
        REQUIRE( app.axis2().m_calls.empty() );
        REQUIRE( app.m_isSet == false );
    }

    SECTION( "identify only in READY" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_axis1_identify( switchProp( "axis1_identify", "request", true ) ) == 0 );
        REQUIRE( app.axis1().m_calls == std::vector<std::string>{ "mod_identify" } );

        app.axis2().m_rv["mod_identify"] = -4;
        REQUIRE( app.newCallBack_m_indiP_axis2_identify( switchProp( "axis2_identify", "request", true ) ) == -4 );
        REQUIRE( app.axis2().m_calls == std::vector<std::string>{ "mod_identify" } );

        REQUIRE( app.newCallBack_m_indiP_axis1_identify( switchProp( "axis1_identify", "request", false ) ) == 0 );
        REQUIRE( app.axis1().m_calls.size() == 1 );

        app.state( stateCodes::OPERATING );
        REQUIRE( app.newCallBack_m_indiP_axis1_identify( switchProp( "axis1_identify", "request", true ) ) == 0 );
        REQUIRE( app.axis1().m_calls.size() == 1 );
    }

    SECTION( "enable toggles" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_axis1_enable( switchProp( "axis1_enable", "toggle", true ) ) == 0 );
        REQUIRE( app.m_axis1Enabled == true );
        REQUIRE( app.newCallBack_m_indiP_axis2_enable( switchProp( "axis2_enable", "toggle", true ) ) == 0 );
        REQUIRE( app.m_axis2Enabled == true );

        app.state( stateCodes::OPERATING );
        REQUIRE( app.newCallBack_m_indiP_axis1_enable( switchProp( "axis1_enable", "toggle", false ) ) == 0 );
        REQUIRE( app.m_axis1Enabled == false );
        REQUIRE( app.newCallBack_m_indiP_axis2_enable( switchProp( "axis2_enable", "toggle", false ) ) == 0 );
        REQUIRE( app.m_axis2Enabled == false );
        REQUIRE( app.axis2().m_enable == tmcController::EnableState::disabled );
    }

    SECTION( "voltage targets are clamped and converted" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_axis1_voltage( numberProp( "axis1_voltage", 90 ) ) == 0 );
        REQUIRE( app.axis1().m_lastSetVolts == Approx( 0.6 ) );

        REQUIRE( app.newCallBack_m_indiP_axis2_voltage( numberProp( "axis2_voltage", 300 ) ) == 0 );
        REQUIRE( app.axis2().m_lastSetVolts == Approx( 1.0 ) );

        REQUIRE( app.newCallBack_m_indiP_axis2_voltage( numberProp( "axis2_voltage", -20 ) ) == 0 );
        REQUIRE( app.axis2().m_lastSetVolts == Approx( 0.0 ).margin( 1e-9 ) );
    }

    SECTION( "set and rest" )
    {
        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_m_indiP_set( switchProp( "set", "toggle", true ) ) == 0 );
        REQUIRE( app.m_isSet == true );
        REQUIRE( app.m_axis1Enabled == true );
        REQUIRE( app.m_axis2Enabled == true );

        REQUIRE( app.newCallBack_m_indiP_set( switchProp( "set", "toggle", false ) ) == 0 );
        REQUIRE( app.m_isSet == false );
        REQUIRE( app.m_axis1Enabled == false );
        REQUIRE( app.m_axis2Enabled == false );
    }

    SECTION( "wrong device or name is rejected" )
    {
        app.state( stateCodes::READY );

        pcf::IndiProperty ip = switchProp( "axis1_enable", "toggle", true );
        ip.setDevice( "other" );
        REQUIRE( app.newCallBack_m_indiP_axis1_enable( ip ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_axis1_enable( switchProp( "axis2_enable", "toggle", true ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_set( switchProp( "axis1_enable", "toggle", true ) ) == -1 );
        REQUIRE( app.axis1().m_calls.empty() );
        REQUIRE( app.m_axis1Enabled == false );
    }
}

/// Verify the MagAO-X error reporting of the tmcCon controller wrapper.
/**
 * \ingroup kcubeCtrl_unit_test
 */
TEST_CASE( "kcubeCtrl controller error messages", "[kcubeCtrl]" )
{
    // clang-format off
    #ifdef KCUBECTRL_TEST_DOXYGEN_REF
    tmcCon<kcubeCtrl>::ftdiErrmsg( std::string(), std::string(), 0, std::string(), 0 );
    tmcCon<kcubeCtrl>::otherErrmsg( std::string(), std::string(), std::string(), 0 );
    #endif
    // clang-format on

    kcubeCtrl_test app( "kcube" );

    SECTION( "libftdi1 errors include the libftdi1 error string" )
    {
        int calls = ftdiErrorStringCalls();
        app.m_kAxis1.ftdiErrmsg( "open", "failed", -5, __FILE__, __LINE__ );
        REQUIRE( ftdiErrorStringCalls() == calls + 1 );

        // The override logs instead of calling the base class.
        REQUIRE( app.axis1().m_errmsgCalls == 0 );
    }

    SECTION( "other errors are logged by the override" )
    {
        int calls = ftdiErrorStringCalls();
        app.m_kAxis2.otherErrmsg( "connect", "failed", __FILE__, __LINE__ );
        REQUIRE( ftdiErrorStringCalls() == calls );
        REQUIRE( app.axis2().m_errmsgCalls == 0 );
    }

    SECTION( "virtual dispatch reaches the override" )
    {
        tmcController &base  = app.m_kAxis1;
        int            calls = ftdiErrorStringCalls();
        base.ftdiErrmsg( "read", "failed", -1, __FILE__, __LINE__ );
        REQUIRE( ftdiErrorStringCalls() == calls + 1 );
        REQUIRE( app.axis1().m_errmsgCalls == 0 );
    }
}

} // namespace kcubeCtrlTest

} // namespace libXWCTest
