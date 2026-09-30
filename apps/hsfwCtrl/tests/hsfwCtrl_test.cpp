/** \file hsfwCtrl_test.cpp
 * \brief Catch2 tests for the hsfwCtrl app.
 * \author Claude Code
 *
 * \ingroup hsfwCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Exposes the protected state of the app, of dev::stdMotionStage, of MagAOXApp (power state), and of the telemetry
// logManager (so the telemetry thread can be marked as running without starting it).
#define protected public
#include "../hsfwCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// Fake libhsfw state shared by the stub functions below.
struct hsfwStubState
{
    std::vector<std::wstring> serials; ///< Storage for the serial numbers of the enumerated wheels.

    std::vector<hsfw_wheel_info> devices; ///< The linked list of wheels returned by enumerate_wheels().

    hsfw_wheel wheel{ 42 }; ///< The wheel handle returned by open_hsfw().

    bool openFails{ false }; ///< If true, open_hsfw() returns NULL.

    int statusReturn{ 0 }; ///< Value returned by get_hsfw_status().

    wheel_status status{ 1, 1, 0, 0, 0 }; ///< Status reported by get_hsfw_status().

    int homeReturn{ 0 }; ///< Value returned by home_hsfw().

    int moveReturn{ 0 }; ///< Value returned by move_hsfw().

    int enumerateCalls{ 0 }; ///< Number of enumerate_wheels() calls.

    int freeCalls{ 0 }; ///< Number of wheels_free_enumeration() calls.

    int openCalls{ 0 }; ///< Number of open_hsfw() calls.

    int closeCalls{ 0 }; ///< Number of close_hsfw() calls.

    int statusCalls{ 0 }; ///< Number of get_hsfw_status() calls.

    int clearErrorCalls{ 0 }; ///< Number of clear_error_hsfw() calls.

    int homeCalls{ 0 }; ///< Number of home_hsfw() calls.

    int exitCalls{ 0 }; ///< Number of exit_hsfw() calls.

    std::vector<unsigned short> moveTargets; ///< Every position sent to move_hsfw(), in order.

    hsfw_wheel *lastMoveWheel{ nullptr }; ///< Wheel passed to the last move_hsfw().

    hsfw_wheel *lastHomeWheel{ nullptr }; ///< Wheel passed to the last home_hsfw().

    hsfw_wheel *lastStatusWheel{ nullptr }; ///< Wheel passed to the last get_hsfw_status().

    hsfw_wheel *lastClosed{ nullptr }; ///< Wheel passed to the last close_hsfw().

    hsfw_wheel_info *lastFreed{ nullptr }; ///< List passed to the last wheels_free_enumeration().

    unsigned short lastOpenVendor{ 0 }; ///< Vendor ID passed to the last open_hsfw().

    unsigned short lastOpenProduct{ 0 }; ///< Product ID passed to the last open_hsfw().

    std::wstring lastOpenSerial; ///< Serial number passed to the last open_hsfw().

    /// Set the wheels reported by enumerate_wheels(), as (vendor, product, serial) entries.
    void setDevices(
        const std::vector<std::tuple<unsigned short, unsigned short, std::wstring>> &devs /**< [in] the wheels */ )
    {
        serials.clear();
        devices.clear();

        for( const auto &d : devs )
        {
            serials.push_back( std::get<2>( d ) );
        }

        devices.resize( devs.size() );
        for( size_t n = 0; n < devs.size(); ++n )
        {
            devices[n].vendor_id     = std::get<0>( devs[n] );
            devices[n].product_id    = std::get<1>( devs[n] );
            devices[n].serial_number = serials[n].data();
            devices[n].next          = ( n + 1 < devs.size() ) ? &devices[n + 1] : nullptr;
        }
    }
};

/// The global fake libhsfw state.
hsfwStubState g_hsfwStub;

/// Reset the fake libhsfw state before a test.
void resetHsfwStub()
{
    g_hsfwStub = hsfwStubState();
}

/// Set the status reported by get_hsfw_status().
void setHsfwStatus( unsigned short position, /**< [in] the filter position */
                    int            isHomed,  /**< [in] the homed flag */
                    int            isHoming, /**< [in] the homing flag */
                    int            isMoving, /**< [in] the moving flag */
                    int            errorState /**< [in] the error state */ )
{
    g_hsfwStub.status.position    = position;
    g_hsfwStub.status.is_homed    = isHomed;
    g_hsfwStub.status.is_homing   = isHoming;
    g_hsfwStub.status.is_moving   = isMoving;
    g_hsfwStub.status.error_state = errorState;
}

} // namespace

hsfw_wheel_info *enumerate_wheels()
{
    ++g_hsfwStub.enumerateCalls;

    if( g_hsfwStub.devices.empty() )
    {
        return nullptr;
    }

    return &g_hsfwStub.devices[0];
}

void wheels_free_enumeration( hsfw_wheel_info *devs )
{
    ++g_hsfwStub.freeCalls;
    g_hsfwStub.lastFreed = devs;
}

hsfw_wheel *open_hsfw( unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number )
{
    ++g_hsfwStub.openCalls;
    g_hsfwStub.lastOpenVendor  = vendor_id;
    g_hsfwStub.lastOpenProduct = product_id;
    g_hsfwStub.lastOpenSerial  = serial_number ? std::wstring( serial_number ) : std::wstring();

    if( g_hsfwStub.openFails )
    {
        return nullptr;
    }

    return &g_hsfwStub.wheel;
}

int close_hsfw( hsfw_wheel *wheel )
{
    ++g_hsfwStub.closeCalls;
    g_hsfwStub.lastClosed = wheel;
    return 0;
}

int get_hsfw_status( hsfw_wheel *wheel, wheel_status *status )
{
    ++g_hsfwStub.statusCalls;
    g_hsfwStub.lastStatusWheel = wheel;

    if( g_hsfwStub.statusReturn < 0 )
    {
        return g_hsfwStub.statusReturn;
    }

    *status = g_hsfwStub.status;
    return 0;
}

int clear_error_hsfw( hsfw_wheel *wheel )
{
    static_cast<void>( wheel );
    ++g_hsfwStub.clearErrorCalls;
    return 0;
}

int home_hsfw( hsfw_wheel *wheel )
{
    ++g_hsfwStub.homeCalls;
    g_hsfwStub.lastHomeWheel = wheel;
    return g_hsfwStub.homeReturn;
}

int move_hsfw( hsfw_wheel *wheel, unsigned short position )
{
    g_hsfwStub.lastMoveWheel = wheel;
    g_hsfwStub.moveTargets.push_back( position );
    return g_hsfwStub.moveReturn;
}

int exit_hsfw()
{
    ++g_hsfwStub.exitCalls;
    return 0;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup hsfwCtrl_unit_test hsfwCtrl Unit Tests
 * \brief Unit tests for the hsfwCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `hsfwCtrl` unit tests.
/** \ingroup hsfwCtrl_unit_test
 */
namespace hsfwCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Directory the test harness points the telemetry logger at.
const char *telemPath = "/tmp/hsfwCtrl_test_telem";

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

/// Test harness for hsfwCtrl.
class hsfwCtrl_test : public hsfwCtrl
{
  public:
    /// Construct a harness with the given device name.
    /** Resets the fake libhsfw state, points telemetry at /tmp, and marks the telemetry thread as running so
     * `telemeter::appLogic()` does not force FAILURE.
     */
    explicit hsfwCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        resetHsfwStub();
        m_configName = device;
        static_cast<void>( m_tel.logPath( telemPath ) );
        m_tel.m_logThreadRunning = true;
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
        static_cast<void>( m_tel.logPath( telemPath ) );
    }

    /// Configure a 5-filter wheel and create the stdMotionStage INDI properties.
    /** Filters open, J, H, Ks, dark at positions 1..5.
     */
    void setupWheel()
    {
        m_presetNames     = { "open", "J", "H", "Ks", "dark" };
        m_presetPositions = { 1, 2, 3, 4, 5 };
        dev::stdMotionStage<hsfwCtrl>::appStartup();
    }

    /// Set the power state and target.
    void power( int st /**< [in] the power state and target, 1 for on, 0 for off */ )
    {
        m_powerState       = st;
        m_powerTargetState = st;
    }

    /// Mark the wheel as connected to the fake wheel handle, with power on.
    void connectWheel()
    {
        m_wheel = &g_hsfwStub.wheel;
        power( 1 );
    }
};

/// Build a switch property with the given elements.
pcf::IndiProperty switchProp( const std::string                               &device, /**< [in] the property device */
                              const std::string                               &name,   /**< [in] the property name */
                              const std::vector<std::pair<std::string, bool>> &els /**< [in] elements and states */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    for( const auto &e : els )
    {
        ip.add( pcf::IndiElement( e.first, e.second ? pcf::IndiElement::On : pcf::IndiElement::Off ) );
    }

    return ip;
}

/// \endcond

/// Verify hsfwCtrl construction defaults.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl construction defaults", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );

    REQUIRE( app.m_presetNotation == "filter" );
    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_wheel == nullptr );
    REQUIRE( app.m_pos == Approx( 0.0 ) );
    REQUIRE( app.m_serialNumber.empty() );
    REQUIRE( app.m_moving == 0 );
    REQUIRE( app.m_fractionalPresets == true );
}

/// Verify hsfwCtrl configuration defaults and overrides, including the stdMotionStage presets.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl configuration", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::setupConfig();
    hsfwCtrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        hsfwCtrl_test app( "fw" );

        mx::app::writeConfigFile( "/tmp/hsfwCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
        app.configure( "/tmp/hsfwCtrl_test_defaults.conf" );
        std::remove( "/tmp/hsfwCtrl_test_defaults.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_serialNumber.empty() );
        REQUIRE( app.m_powerOnHome == false );
        REQUIRE( app.m_homePreset == -1 );
        REQUIRE( app.m_presetNames.empty() );
        REQUIRE( app.m_presetPositions.empty() );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
    }

    SECTION( "overrides" )
    {
        hsfwCtrl_test app( "fw" );

        mx::app::writeConfigFile( "/tmp/hsfwCtrl_test_override.conf",
                                  { "stage", "stage", "stage", "filters", "filters", "telemeter" },
                                  { "serialNumber", "powerOnHome", "homePreset", "names", "positions", "maxInterval" },
                                  { "HSFW12345", "true", "3", "open,J,H,Ks,dark", "1,0,3,0,5", "2.5" } );
        app.configure( "/tmp/hsfwCtrl_test_override.conf" );
        std::remove( "/tmp/hsfwCtrl_test_override.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_serialNumber == std::wstring( L"HSFW12345" ) );
        REQUIRE( app.m_powerOnHome == true );
        REQUIRE( app.m_homePreset == 3 );
        REQUIRE( app.m_presetNames == std::vector<std::string>{ "open", "J", "H", "Ks", "dark" } );
        REQUIRE( app.m_presetPositions.size() == 5 );
        REQUIRE( app.m_presetPositions[0] == Approx( 1.0 ) );
        REQUIRE( app.m_presetPositions[1] == Approx( 2.0 ) ); // 0 is replaced by the index + 1
        REQUIRE( app.m_presetPositions[2] == Approx( 3.0 ) );
        REQUIRE( app.m_presetPositions[3] == Approx( 4.0 ) );
        REQUIRE( app.m_presetPositions[4] == Approx( 5.0 ) );
        REQUIRE( app.m_maxInterval == Approx( 2.5 ) );
    }

    SECTION( "names without positions use the default positions" )
    {
        hsfwCtrl_test app( "fw" );

        mx::app::writeConfigFile( "/tmp/hsfwCtrl_test_names.conf", { "filters" }, { "names" }, { "a,b,c" } );
        app.configure( "/tmp/hsfwCtrl_test_names.conf" );
        std::remove( "/tmp/hsfwCtrl_test_names.conf" );

        REQUIRE( app.m_presetNames.size() == 3 );
        REQUIRE( app.m_presetPositions.size() == 3 );
        REQUIRE( app.m_presetPositions[0] == Approx( 1.0 ) );
        REQUIRE( app.m_presetPositions[1] == Approx( 2.0 ) );
        REQUIRE( app.m_presetPositions[2] == Approx( 3.0 ) );
    }
}

/// Verify presetNumber() converts the wheel position to a zero-based preset index.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl presetNumber", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::presetNumber();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );

    auto [pos, expected] =
        GENERATE( table<double, int>( { { 1.0, 0 }, { 2.0, 1 }, { 5.0, 4 }, { 8.0, 7 }, { 0.0, -1 }, { 2.7, 1 } } ) );

    app.m_pos = pos;
    REQUIRE( app.presetNumber() == expected );
}

/// Verify moveTo() rounds the filter position to a single hardware target (the hsfw bounce fix).
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl moveTo sends one rounded hardware target", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::moveTo(1.0);
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();
    app.connectWheel();
    app.state( stateCodes::READY );

    SECTION( "rounding to the nearest filter" )
    {
        auto [filters, hwTarget] = GENERATE( table<double, unsigned short>(
            { { 1.0, 1 }, { 0.5, 1 }, { 2.49, 2 }, { 2.5, 3 }, { 3.0, 3 }, { 4.6, 5 }, { 8.0, 8 } } ) );

        app.m_moving = 0;
        REQUIRE( app.moveTo( filters ) == 0 );
        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ hwTarget } );
        REQUIRE( g_hsfwStub.lastMoveWheel == &g_hsfwStub.wheel );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "each request issues exactly one hardware move" )
    {
        REQUIRE( app.moveTo( 3.0 ) == 0 );
        REQUIRE( app.moveTo( 2.9 ) == 0 );
        REQUIRE( app.moveTo( 5.0 ) == 0 );
        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ 3, 3, 5 } );
    }

    SECTION( "positions below 0.5 are rejected" )
    {
        // The modulo normalization adds 8 until the value is >= 8.5, and then rejects any value >= 8.5, so every
        // position below 0.5 is currently an error.
        auto filters = GENERATE( 0.4, 0.0, -1.0, -7.6 );

        app.m_moving = 0;
        REQUIRE( app.moveTo( filters ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
        REQUIRE( app.m_moving == 0 );
    }

    SECTION( "POWEROFF does nothing" )
    {
        app.state( stateCodes::POWEROFF );
        app.m_moving = -2;
        REQUIRE( app.moveTo( 3.0 ) == 0 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
        REQUIRE( app.m_moving == -2 );
    }

    SECTION( "a libhsfw error is reported with power on" )
    {
        g_hsfwStub.moveReturn = -1;
        REQUIRE( app.moveTo( 2.0 ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ 2 } );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "a libhsfw error while powering off returns an error without logging" )
    {
        g_hsfwStub.moveReturn = -1;
        app.power( 0 );
        REQUIRE( app.moveTo( 2.0 ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ 2 } );
    }
}

/// Verify startHoming() and stop().
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl startHoming and stop", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::startHoming();
    hsfwCtrl::stop();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();
    app.connectWheel();
    app.state( stateCodes::NOTHOMED );
    app.m_moving = -1;

    SECTION( "startHoming homes the wheel" )
    {
        REQUIRE( app.startHoming() == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 1 );
        REQUIRE( g_hsfwStub.lastHomeWheel == &g_hsfwStub.wheel );
        REQUIRE( app.m_moving == 2 );
    }

    SECTION( "startHoming does nothing in POWEROFF" )
    {
        app.state( stateCodes::POWEROFF );
        REQUIRE( app.startHoming() == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 0 );
        REQUIRE( app.m_moving == -1 );
    }

    SECTION( "startHoming reports a libhsfw error" )
    {
        g_hsfwStub.homeReturn = 1;
        REQUIRE( app.startHoming() == -1 );
        REQUIRE( g_hsfwStub.homeCalls == 1 );
        REQUIRE( app.m_moving == -1 );

        app.power( 0 );
        REQUIRE( app.startHoming() == -1 );
        REQUIRE( g_hsfwStub.homeCalls == 2 );
    }

    SECTION( "stop does not talk to the wheel" )
    {
        REQUIRE( app.stop() == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 0 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
        REQUIRE( g_hsfwStub.statusCalls == 0 );
    }
}

/// Verify the stdMotionStage preset and preset-name callbacks move the wheel once per request.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl preset callbacks", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    dev::stdMotionStage<hsfwCtrl>::newCallBack_m_indiP_preset(pcf::IndiProperty());
    dev::stdMotionStage<hsfwCtrl>::newCallBack_m_indiP_presetName(pcf::IndiProperty());
    dev::stdMotionStage<hsfwCtrl>::st_newCallBack_stdMotionStage(nullptr, pcf::IndiProperty());
    hsfwCtrl::moveTo(1.0);
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();
    app.connectWheel();
    app.state( stateCodes::READY );

    SECTION( "preset target moves to the rounded position" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fw" );
        ip.setName( "filter" );
        ip.add( pcf::IndiElement( "target", 3.6 ) );

        app.m_movingState     = 1;
        app.m_presetNameIndex = 1;
        REQUIRE( app.newCallBack_m_indiP_preset( ip ) == 0 );
        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ 4 } );
        REQUIRE( app.m_preset_target == Approx( 3.6 ) );
        REQUIRE( app.m_movingState == 0 );
        REQUIRE( app.m_presetNameIndex == -1 );
        REQUIRE( app.m_moving == 1 );
    }

    SECTION( "preset with the wrong device is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "other" );
        ip.setName( "filter" );
        ip.add( pcf::IndiElement( "target", 2.0 ) );

        REQUIRE( app.newCallBack_m_indiP_preset( ip ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
    }

    SECTION( "preset with no value is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fw" );
        ip.setName( "filter" );

        REQUIRE( app.newCallBack_m_indiP_preset( ip ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
    }

    SECTION( "a full preset-name property with one selection moves once" )
    {
        pcf::IndiProperty ip =
            switchProp( "fw",
                        "filterName",
                        { { "open", false }, { "J", false }, { "H", true }, { "Ks", false }, { "dark", false } } );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == 0 );
        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ 3 } );
        REQUIRE( app.m_preset_target == Approx( 3.0 ) );
        REQUIRE( app.m_movingState == 1 );
        REQUIRE( app.m_presetNameIndex == 2 );
    }

    SECTION( "preset-name uses the configured preset position" )
    {
        app.m_presetPositions = { 1, 2, 3, 4, 7 };
        pcf::IndiProperty ip  = switchProp( "fw", "filterName", { { "dark", true } } );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == 0 );
        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ 7 } );
        REQUIRE( app.m_preset_target == Approx( 7.0 ) );
    }

    SECTION( "unknown preset name is rejected" )
    {
        pcf::IndiProperty ip = switchProp( "fw", "filterName", { { "bogus", true }, { "J", false } } );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
    }

    SECTION( "more than one preset name is rejected" )
    {
        pcf::IndiProperty ip = switchProp( "fw", "filterName", { { "J", true }, { "Ks", true } } );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
    }

    SECTION( "no preset name on is a no-op" )
    {
        pcf::IndiProperty ip = switchProp( "fw", "filterName", { { "J", false }, { "H", false } } );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == 0 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
    }

    SECTION( "preset-name with the wrong device is rejected" )
    {
        pcf::IndiProperty ip = switchProp( "other", "filterName", { { "J", true } } );

        REQUIRE( app.newCallBack_m_indiP_presetName( ip ) == -1 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
    }

    SECTION( "the static dispatcher routes by name" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fw" );
        ip.setName( "filter" );
        ip.add( pcf::IndiElement( "target", 5.0 ) );
        REQUIRE( hsfwCtrl::st_newCallBack_stdMotionStage( &app, ip ) == 0 );

        pcf::IndiProperty nm = switchProp( "fw", "filterName", { { "open", true } } );
        REQUIRE( hsfwCtrl::st_newCallBack_stdMotionStage( &app, nm ) == 0 );

        pcf::IndiProperty bad( pcf::IndiProperty::Number );
        bad.setDevice( "fw" );
        bad.setName( "unknown" );
        REQUIRE( hsfwCtrl::st_newCallBack_stdMotionStage( &app, bad ) == -1 );

        REQUIRE( g_hsfwStub.moveTargets == std::vector<unsigned short>{ 5, 1 } );
    }
}

/// Verify the stdMotionStage home and stop callbacks.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl home and stop callbacks", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    dev::stdMotionStage<hsfwCtrl>::newCallBack_m_indiP_home(pcf::IndiProperty());
    dev::stdMotionStage<hsfwCtrl>::newCallBack_m_indiP_stop(pcf::IndiProperty());
    hsfwCtrl::startHoming();
    hsfwCtrl::stop();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();
    app.connectWheel();
    app.state( stateCodes::NOTHOMED );
    app.m_moving = -1;

    SECTION( "home request starts homing" )
    {
        app.m_movingState    = 1;
        pcf::IndiProperty ip = switchProp( "fw", "home", { { "request", true } } );

        REQUIRE( app.newCallBack_m_indiP_home( ip ) == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 1 );
        REQUIRE( app.m_moving == 2 );
        REQUIRE( app.m_movingState == 0 );
    }

    SECTION( "home request off is a no-op" )
    {
        pcf::IndiProperty ip = switchProp( "fw", "home", { { "request", false } } );

        REQUIRE( app.newCallBack_m_indiP_home( ip ) == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 0 );
        REQUIRE( app.m_moving == -1 );
    }

    SECTION( "home with the wrong device or name is rejected" )
    {
        pcf::IndiProperty ip = switchProp( "other", "home", { { "request", true } } );
        REQUIRE( app.newCallBack_m_indiP_home( ip ) == -1 );

        pcf::IndiProperty ip2 = switchProp( "fw", "stop", { { "request", true } } );
        REQUIRE( app.newCallBack_m_indiP_home( ip2 ) == -1 );

        REQUIRE( g_hsfwStub.homeCalls == 0 );
    }

    SECTION( "stop request calls stop without moving the wheel" )
    {
        app.m_movingState    = 1;
        pcf::IndiProperty ip = switchProp( "fw", "stop", { { "request", true } } );

        REQUIRE( app.newCallBack_m_indiP_stop( ip ) == 0 );
        REQUIRE( app.m_movingState == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 0 );
        REQUIRE( g_hsfwStub.moveTargets.empty() );
    }

    SECTION( "stop with the wrong device is rejected" )
    {
        app.m_movingState    = 1;
        pcf::IndiProperty ip = switchProp( "other", "stop", { { "request", true } } );

        REQUIRE( app.newCallBack_m_indiP_stop( ip ) == -1 );
        REQUIRE( app.m_movingState == 1 );
    }

    SECTION( "the static dispatcher routes home and stop" )
    {
        pcf::IndiProperty ip = switchProp( "fw", "home", { { "request", true } } );
        REQUIRE( hsfwCtrl::st_newCallBack_stdMotionStage( &app, ip ) == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 1 );

        pcf::IndiProperty st = switchProp( "fw", "stop", { { "request", true } } );
        REQUIRE( hsfwCtrl::st_newCallBack_stdMotionStage( &app, st ) == 0 );
        REQUIRE( g_hsfwStub.homeCalls == 1 );
    }
}

/// Verify appStartup() refuses to start from UNINITIALIZED and appLogic() refuses INITIALIZED.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl startup state checks", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::appStartup();
    hsfwCtrl::appLogic();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );

    SECTION( "appStartup from UNINITIALIZED" )
    {
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
        REQUIRE( app.appStartup() == -1 );
    }

    SECTION( "appLogic in INITIALIZED" )
    {
        app.state( stateCodes::INITIALIZED );
        REQUIRE( app.appLogic() == -1 );
        REQUIRE( g_hsfwStub.enumerateCalls == 0 );
    }
}

/// Verify appLogic() device discovery and connection through libhsfw.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl appLogic device discovery and connection", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::appLogic();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();
    app.m_serialNumber = L"HSFW1";

    SECTION( "POWERON with no wheels goes to NODEVICE" )
    {
        app.power( 1 );
        app.state( stateCodes::POWERON );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( g_hsfwStub.enumerateCalls == 1 );
        REQUIRE( g_hsfwStub.freeCalls == 0 );
        REQUIRE( g_hsfwStub.openCalls == 0 );
    }

    SECTION( "NODEVICE with other wheels stays in NODEVICE and frees the list" )
    {
        g_hsfwStub.setDevices( { { 0x10c4, 0x82cd, L"OTHER1" }, { 0x10c4, 0x82cd, L"OTHER2" } } );
        app.power( 1 );
        app.state( stateCodes::NODEVICE );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( g_hsfwStub.enumerateCalls == 1 );
        REQUIRE( g_hsfwStub.freeCalls == 1 );
        REQUIRE( g_hsfwStub.lastFreed == &g_hsfwStub.devices[0] );
        REQUIRE( g_hsfwStub.openCalls == 0 );
    }

    SECTION( "NODEVICE finds the wheel but waits while powered off" )
    {
        g_hsfwStub.setDevices( { { 0x10c4, 0x82cd, L"OTHER1" }, { 0x10c4, 0x82cd, L"HSFW1" } } );
        app.power( 0 );
        app.state( stateCodes::NODEVICE );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( g_hsfwStub.enumerateCalls == 1 );
        REQUIRE( g_hsfwStub.freeCalls == 1 );
        REQUIRE( g_hsfwStub.openCalls == 0 );
    }

    SECTION( "NODEVICE with power on connects and polls the status in one pass" )
    {
        g_hsfwStub.setDevices( { { 0x10c4, 0x82cd, L"OTHER1" }, { 0x1234, 0x5678, L"HSFW1" } } );
        setHsfwStatus( 2, 1, 0, 0, 0 );
        app.power( 1 );
        app.state( stateCodes::NODEVICE );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( g_hsfwStub.enumerateCalls == 2 );
        // The second enumeration, in NOTCONNECTED, is not freed after a successful open.
        REQUIRE( g_hsfwStub.freeCalls == 1 );
        REQUIRE( g_hsfwStub.openCalls == 1 );
        REQUIRE( g_hsfwStub.lastOpenVendor == 0x1234 );
        REQUIRE( g_hsfwStub.lastOpenProduct == 0x5678 );
        REQUIRE( g_hsfwStub.lastOpenSerial == std::wstring( L"HSFW1" ) );
        REQUIRE( app.m_wheel == &g_hsfwStub.wheel );
        REQUIRE( g_hsfwStub.statusCalls == 1 );
        REQUIRE( g_hsfwStub.lastStatusWheel == &g_hsfwStub.wheel );
        REQUIRE( app.m_pos == Approx( 2.0 ) );
        REQUIRE( app.m_moving == 0 );
        REQUIRE( app.m_preset == Approx( 2.0 ) );
        REQUIRE( app.m_preset_target == Approx( 2.0 ) );
    }

    SECTION( "NOTCONNECTED waits while powered off" )
    {
        g_hsfwStub.setDevices( { { 0x10c4, 0x82cd, L"HSFW1" } } );
        app.power( 0 );
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
        REQUIRE( g_hsfwStub.enumerateCalls == 0 );
    }

    SECTION( "NOTCONNECTED with no wheels goes to NODEVICE" )
    {
        app.power( 1 );
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( g_hsfwStub.enumerateCalls == 1 );
        REQUIRE( g_hsfwStub.openCalls == 0 );
    }

    SECTION( "NOTCONNECTED when the wheel disappeared goes to NODEVICE" )
    {
        g_hsfwStub.setDevices( { { 0x10c4, 0x82cd, L"OTHER1" } } );
        app.power( 1 );
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( g_hsfwStub.freeCalls == 1 );
        REQUIRE( g_hsfwStub.openCalls == 0 );
    }

    SECTION( "NOTCONNECTED with an open failure goes to NODEVICE" )
    {
        g_hsfwStub.setDevices( { { 0x10c4, 0x82cd, L"HSFW1" } } );
        g_hsfwStub.openFails = true;
        app.power( 1 );
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( g_hsfwStub.openCalls == 1 );
        REQUIRE( app.m_wheel == nullptr );
        REQUIRE( g_hsfwStub.statusCalls == 0 );
    }

    SECTION( "NOTCONNECTED closes a previously open wheel before reopening" )
    {
        hsfw_wheel oldWheel{ 7 };
        app.m_wheel = &oldWheel;
        g_hsfwStub.setDevices( { { 0x10c4, 0x82cd, L"HSFW1" } } );
        setHsfwStatus( 1, 1, 0, 0, 0 );
        app.power( 1 );
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( g_hsfwStub.closeCalls == 1 );
        REQUIRE( g_hsfwStub.lastClosed == &oldWheel );
        REQUIRE( app.m_wheel == &g_hsfwStub.wheel );
        REQUIRE( app.state() == stateCodes::READY );
    }
}

/// Verify appLogic() maps the wheel status to the app state, moving flag and preset.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl appLogic status polling", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::appLogic();
    hsfwCtrl::presetNumber();
    hsfwCtrl::startHoming();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();
    app.connectWheel();
    app.state( stateCodes::READY );

    SECTION( "state mapping" )
    {
        auto [homed, homing, moving, expState, expMoving] =
            GENERATE( table<int, int, int, stateCodes::stateCodeT, int>( {
                { 0, 0, 0, stateCodes::NOTHOMED, -1 },
                { 0, 1, 0, stateCodes::HOMING, 2 },
                { 1, 1, 0, stateCodes::HOMING, 2 },
                { 1, 0, 1, stateCodes::OPERATING, 1 },
                { 1, 0, 0, stateCodes::READY, 0 },
            } ) );

        setHsfwStatus( 4, homed, homing, moving, 0 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == expState );
        REQUIRE( app.m_moving == expMoving );
        REQUIRE( app.m_pos == Approx( 4.0 ) );
        REQUIRE( app.m_preset == Approx( 4.0 ) );
        REQUIRE( app.m_preset_target == Approx( 4.0 ) );
        REQUIRE( g_hsfwStub.homeCalls == 0 );
        REQUIRE( g_hsfwStub.clearErrorCalls == 0 );
    }

    SECTION( "not homed with powerOnHome starts homing" )
    {
        app.m_powerOnHome = true;
        setHsfwStatus( 0, 0, 0, 0, 0 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTHOMED );
        REQUIRE( g_hsfwStub.homeCalls == 1 );
        REQUIRE( app.m_moving == 2 );
    }

    SECTION( "position 0 gives preset 0" )
    {
        setHsfwStatus( 0, 1, 0, 0, 0 );
        app.m_preset        = 3;
        app.m_preset_target = 3;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_preset == Approx( 0.0 ) );
        REQUIRE( app.m_preset_target == Approx( 0.0 ) );
    }

    SECTION( "an error state is cleared" )
    {
        setHsfwStatus( 1, 1, 0, 0, 5 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( g_hsfwStub.clearErrorCalls == 1 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "a status error leaves the state unchanged" )
    {
        g_hsfwStub.statusReturn = -1;
        app.m_pos               = 3;
        app.m_moving            = 0;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( g_hsfwStub.statusCalls == 1 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( app.m_pos == Approx( 3.0 ) );
    }

    SECTION( "no status poll while powered off" )
    {
        app.power( 0 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( g_hsfwStub.statusCalls == 0 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "a stopped telemetry thread is a FAILURE" )
    {
        app.m_tel.m_logThreadRunning = false;
        setHsfwStatus( 1, 1, 0, 0, 0 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::FAILURE );
        REQUIRE( app.m_shutdown == 1 );
    }
}

/// Verify appShutdown() closes the wheel and shuts down libhsfw.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl appShutdown", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::appShutdown();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );

    SECTION( "with an open wheel" )
    {
        app.m_wheel = &g_hsfwStub.wheel;
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_hsfwStub.closeCalls == 1 );
        REQUIRE( g_hsfwStub.lastClosed == &g_hsfwStub.wheel );
        REQUIRE( g_hsfwStub.exitCalls == 1 );
    }

    SECTION( "without a wheel" )
    {
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_hsfwStub.closeCalls == 0 );
        REQUIRE( g_hsfwStub.exitCalls == 1 );
    }
}

/// Verify the power-off hooks.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl power off hooks", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::onPowerOff();
    hsfwCtrl::whilePowerOff();
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();

    SECTION( "onPowerOff marks the stage as powered off" )
    {
        app.m_moving = 1;
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( app.m_moving == -2 );
        REQUIRE( g_hsfwStub.statusCalls == 0 );
    }

    SECTION( "whilePowerOff does not talk to the wheel" )
    {
        REQUIRE( app.whilePowerOff() == 0 );
        REQUIRE( g_hsfwStub.statusCalls == 0 );
        REQUIRE( g_hsfwStub.enumerateCalls == 0 );
        REQUIRE( app.m_shutdown == 0 );
    }

    SECTION( "whilePowerOff with a stopped telemetry thread still returns 0" )
    {
        app.m_tel.m_logThreadRunning = false;
        REQUIRE( app.whilePowerOff() == 0 );
        REQUIRE( app.state() == stateCodes::FAILURE );
        REQUIRE( app.m_shutdown == 1 );
    }
}

/// Verify the telemetry interface records the stage state.
/**
 * \ingroup hsfwCtrl_unit_test
 */
TEST_CASE( "hsfwCtrl telemetry", "[hsfwCtrl]" )
{
    // clang-format off
    #ifdef HSFWCTRL_TEST_DOXYGEN_REF
    hsfwCtrl::checkRecordTimes();
    hsfwCtrl::recordTelem(nullptr);
    hsfwCtrl::recordStage(false);
    #endif
    // clang-format on

    hsfwCtrl_test app( "fw" );
    app.setupWheel();
    app.m_pos    = 3;
    app.m_preset = 3;

    SECTION( "recordTelem forces a record" )
    {
        telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records when the interval has elapsed" )
    {
        telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes does not record within the interval" )
    {
        timespec now;
        clock_gettime( CLOCK_REALTIME, &now );
        telem_stage::lastRecord = now;
        app.m_maxInterval       = 1000;
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec == now.tv_sec );
        REQUIRE( telem_stage::lastRecord.tv_nsec == now.tv_nsec );
    }

    SECTION( "recordStage with force records" )
    {
        telem_stage::lastRecord = { 0, 0 };
        REQUIRE( app.recordStage( true ) == 0 );
        REQUIRE( telem_stage::lastRecord.tv_sec > 0 );
    }
}

} // namespace hsfwCtrlTest

} // namespace libXWCTest
