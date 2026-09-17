/** \file wccCtrl_test.cpp
 * \brief Catch2 tests for the wccCtrl app.
 * \author Adam Schilperoort
 *
 * \ingroup wccCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <fstream>

#include "../wccCtrl.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup wccCtrl_unit_test wccCtrl Unit Tests
 * \brief Unit tests for the wccCtrl application.
 *
 * The astrometry and visit file handling are covered by
 * \ref wccCommon_unit_test. What is tested here is the controller layer: the
 * acquisition state machine's naming, configuration parsing, and the guide and
 * roll star selection that has to degrade gracefully when only part of the sensor
 * array is configured.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `wccCtrl` unit tests.
/** \ingroup wccCtrl_unit_test
 */
namespace wccCtrlTest
{

/// Expose the protected surface of wccCtrl for testing.
/** \cond
 */
class wccCtrlTester : public wccCtrl
{
  public:
    /// Register the config targets, read a file, and load it.
    /** This has to use the app's own `config` member: setupConfig() registers its
     * targets there, and a separate appConfigurator would have none of them, so
     * every lookup would silently miss.
     */
    int testLoadConfigFile( const std::string &path )
    {
        setupConfig();
        config.readConfig( path );
        return loadConfigImpl( config );
    }

    /// Number of sensors the configuration produced.
    size_t nSensors()
    {
        return m_sensors.size();
    }

    /// A sensor's logical name.
    std::string sensorName( size_t i )
    {
        return m_sensors[i]->m_name;
    }

    /// A sensor's input stream name.
    std::string sensorStream( size_t i )
    {
        return m_sensors[i]->m_shmimIn;
    }

    /// Look up a sensor by name through the controller's tolerant matcher.
    int findSensor( const std::string &n )
    {
        return sensorByName( n );
    }

    /// Install a resolved selection directly, then resolve its stars onto sensors.
    /** Bypasses the INDI mirroring so the sensor matching can be tested on its own.
     */
    int testResolveStars( const MagAOX::wcc::visitSelection &sel )
    {
        m_selection = sel;
        m_selection.m_valid = true;
        return resolveStars();
    }

    /// Index into the sensor list of the selected guide sensor.
    int guideSensor()
    {
        return m_guideSensor;
    }

    /// Index into the sensor list of the selected roll sensor.
    int rollSensor()
    {
        return m_rollSensor;
    }

    /// The selected guide star.
    const MagAOX::wcc::visitStar &guideStar()
    {
        return m_guideStar;
    }

    /// The selected roll star.
    const MagAOX::wcc::visitStar &rollStar()
    {
        return m_rollStarSel;
    }

    /// The current acquisition state.
    wccAcqState state()
    {
        return acqState();
    }

    /// The fast loop period actually in use, after rate-limit clamping.
    double trackPeriod()
    {
        return m_trackPeriod;
    }

    /// Convert a pixel displacement on a sensor into a field shift.
    void testPixelToFieldShift( size_t i, double x, double y, double dx, double dy, double &fx, double &fy )
    {
        pixelToFieldShift( i, x, y, dx, dy, fx, fy );
    }

    /// The focal plane model the configuration built.
    MagAOX::wcc::focalPlaneModel &focalPlane()
    {
        return m_focalPlane;
    }
};
/** \endcond
 */

/// Write a minimal but valid wccCtrl configuration to a file.
/** \returns the path written
 *
 * \ingroup wccCtrl_unit_test
 */
inline std::string writeCtrlConfig( const std::string &path /**< [in] where to write */,
                                    const std::string &sensors /**< [in] acq.sensors value */,
                                    const std::string &sections /**< [in] the sensor sections */ )
{
    std::ofstream fout( path );

    fout << "[visit]\ndevice=visitsim\n";
    fout << "[telescope]\ndevice=telsim\ndiameter=6.5\nf_number=12.0\nparity=-1\n";
    fout << "[catalog]\npath=/dev/null\n";
    fout << "[acq]\nsensors=" << sensors << "\n";
    fout << sections;

    return path;
}

/// Verify the acquisition state names are complete and unique.
/**
 * \ingroup wccCtrl_unit_test
 */
TEST_CASE( "wccCtrl acquisition state names are complete and unique", "[wccCtrl]" )
{
    // clang-format off
    #ifdef WCCCTRL_TEST_DOXYGEN_REF
    wccCtrl();
    wccCtrl::appStartup;
    wccCtrl::appLogic;
    wccCtrl::appShutdown;
    wccAcqStateName;
    #endif
    // clang-format on

    SECTION( "default construction succeeds" )
    {
        wccCtrl app;

        REQUIRE( true );
    }

    SECTION( "every state has a distinct, non-empty name" )
    {
        const wccAcqState all[] = { wccAcqState::idle,          wccAcqState::loaded,
                                    wccAcqState::configuring,   wccAcqState::confirming,
                                    wccAcqState::acquiring,     wccAcqState::solving,
                                    wccAcqState::offsetting,    wccAcqState::waitTelescope,
                                    wccAcqState::verifying,     wccAcqState::rolling,
                                    wccAcqState::verifyRoll,    wccAcqState::reconfiguring,
                                    wccAcqState::trackingStart, wccAcqState::tracking,
                                    wccAcqState::complete,      wccAcqState::failed };

        std::vector<std::string> names;

        for( wccAcqState s : all )
        {
            const std::string n = wccAcqStateName( s );
            REQUIRE_FALSE( n.empty() );

            // A duplicated name would make the INDI acq_state property ambiguous.
            for( const std::string &prev : names )
            {
                REQUIRE( n != prev );
            }

            names.push_back( n );
        }

        REQUIRE( names.size() == 16 );
        REQUIRE( wccAcqStateName( wccAcqState::idle ) == "IDLE" );
        REQUIRE( wccAcqStateName( wccAcqState::tracking ) == "TRACKING" );
        REQUIRE( wccAcqStateName( wccAcqState::failed ) == "FAILED" );
    }

    SECTION( "a new controller starts idle" )
    {
        wccCtrlTester app;

        REQUIRE( app.state() == wccAcqState::idle );
    }
}

/// Verify configuration parsing, including the checks that must reject a config.
/**
 * \ingroup wccCtrl_unit_test
 */
TEST_CASE( "wccCtrl parses its configuration and rejects unusable ones", "[wccCtrl]" )
{
    // clang-format off
    #ifdef WCCCTRL_TEST_DOXYGEN_REF
    wccCtrl::loadConfigImpl;
    wccCtrl::sensorByName;
    #endif
    // clang-format on

    SECTION( "sensors are built and matched tolerantly by name" )
    {
        wccCtrlTester app;

        const std::string path = writeCtrlConfig(
            "/tmp/wccCtrl_test_config.conf", "IMX-18,HWK-09",
            "[IMX-18]\nindi_device=nsv18\npixel_size=3.76\nfull_w=9576\nfull_h=6388\n"
            "field_x=-849.953\nfield_y=-68.782\n"
            "[HWK-09]\nindi_device=hwk09\nshmim_in=hwk09custom\npixel_size=4.6\n"
            "full_w=4096\nfull_h=2300\nfield_x=1029.51\nfield_y=59.31\n" );

        REQUIRE( app.testLoadConfigFile( path ) == 0 );
        REQUIRE( app.nSensors() == 2 );

        // The input stream defaults to the simulator's output name and is overridable.
        REQUIRE( app.sensorStream( 0 ) == "nsv18sim" );
        REQUIRE( app.sensorStream( 1 ) == "hwk09custom" );

        // The visit file spells the same hardware both HWK and HAWK.
        REQUIRE( app.findSensor( "IMX-18" ) == 0 );
        REQUIRE( app.findSensor( "HWK-09" ) == 1 );
        REQUIRE( app.findSensor( "HAWK-09" ) == 1 );
        REQUIRE( app.findSensor( "IMX-99" ) == -1 );

        std::remove( path.c_str() );
    }

    SECTION( "a missing telescope device is rejected" )
    {
        // Without a telescope there is nothing to send offsets to, so this is fatal
        // rather than something to discover mid-sequence.
        wccCtrlTester app;

        const std::string path = "/tmp/wccCtrl_test_config_notel.conf";
        {
            std::ofstream fout( path );
            fout << "[visit]\ndevice=visitsim\n";
            fout << "[catalog]\npath=/dev/null\n";
            fout << "[acq]\nsensors=A\n";
            fout << "[A]\nindi_device=a\npixel_size=3.76\nfull_w=512\nfull_h=512\n";
        }

        REQUIRE( app.testLoadConfigFile( path ) < 0 );

        std::remove( path.c_str() );
    }

    SECTION( "a missing visit device is rejected" )
    {
        // wccCtrl reads the visit from visitCtrl over INDI, so without that device
        // there is nothing to read and the sequence could never start.
        wccCtrlTester app;

        const std::string path = "/tmp/wccCtrl_test_config_novisit.conf";
        {
            std::ofstream fout( path );
            fout << "[telescope]\ndevice=telsim\n";
            fout << "[catalog]\npath=/dev/null\n";
            fout << "[acq]\nsensors=A\n";
            fout << "[A]\nindi_device=a\npixel_size=3.76\nfull_w=512\nfull_h=512\n";
        }

        REQUIRE( app.testLoadConfigFile( path ) < 0 );

        std::remove( path.c_str() );
    }

    SECTION( "the fast loop period is clamped to the INDI rate limit" )
    {
        wccCtrlTester app;

        const std::string path = writeCtrlConfig(
            "/tmp/wccCtrl_test_config_rate.conf", "A",
            "[A]\nindi_device=a\npixel_size=3.76\nfull_w=512\nfull_h=512\n" );

        // Append a period well below the limit.
        {
            std::ofstream fout( path, std::ios::app );
            fout << "[track]\nperiod=0.02\n";
        }

        REQUIRE( app.testLoadConfigFile( path ) == 0 );

        // Silently running the loop faster than 1 Hz would overload the INDI server,
        // so the requested period is raised rather than honoured.
        REQUIRE( app.trackPeriod() == Approx( MagAOX::wcc::indiMinPeriod ).epsilon( 1e-12 ) );

        std::remove( path.c_str() );
    }

    SECTION( "an empty sensor list is rejected" )
    {
        wccCtrlTester app;

        const std::string path = "/tmp/wccCtrl_test_config_nosensors.conf";
        {
            std::ofstream fout( path );
            fout << "[telescope]\ndevice=telsim\n";
            fout << "[catalog]\npath=/dev/null\n";
        }

        REQUIRE( app.testLoadConfigFile( path ) < 0 );

        std::remove( path.c_str() );
    }
}

/// Verify guide and roll star selection degrades to a lower rank rather than failing.
/**
 * \ingroup wccCtrl_unit_test
 */
TEST_CASE( "wccCtrl selects the best usable guide and roll star pair", "[wccCtrl]" )
{
    // clang-format off
    #ifdef WCCCTRL_TEST_DOXYGEN_REF
    wccCtrl::resolveStars;
    wccCtrl::pixelToFieldShift;
    #endif
    // clang-format on

    // Configure an array holding only IMX-19 and HWK-08.
    wccCtrlTester app;

    const std::string path = writeCtrlConfig(
        "/tmp/wccCtrl_test_select.conf", "IMX-19,HWK-08",
        "[IMX-19]\nindi_device=nsv19\npixel_size=3.76\nfull_w=9576\nfull_h=6388\n"
        "field_x=-666.282\nfield_y=-109.31\n"
        "[HWK-08]\nindi_device=hwk08\npixel_size=4.6\nfull_w=4096\nfull_h=2300\n"
        "field_x=786.51\nfield_y=102.092\n" );

    REQUIRE( app.testLoadConfigFile( path ) == 0 );

    SECTION( "a star pair on configured sensors resolves to sensor indices" )
    {
        // visitCtrl has already chosen the rank, so wccCtrl only has to find the
        // sensors. HAWK-08 must match the configured HWK-08.
        MagAOX::wcc::visitSelection sel;
        sel.m_ra = 10.0;
        sel.m_dec = 20.0;
        sel.m_rank = 2;
        sel.m_guide.m_rank = 2;
        sel.m_guide.m_sensor = "IMX-19";
        sel.m_guide.m_ra = 10.2;
        sel.m_guide.m_dec = 20.2;
        sel.m_roll.m_rank = 2;
        sel.m_roll.m_sensor = "HAWK-08";
        sel.m_roll.m_ra = 9.8;
        sel.m_roll.m_dec = 19.8;

        REQUIRE( app.testResolveStars( sel ) == 0 );
        REQUIRE( app.guideSensor() == 0 );
        REQUIRE( app.rollSensor() == 1 );
        REQUIRE( app.guideStar().m_sensor == "IMX-19" );
        REQUIRE( app.rollStar().m_sensor == "HAWK-08" );
    }

    SECTION( "a star on an unconfigured sensor is an error" )
    {
        // The array holds only IMX-19 and HWK-08, so this pair cannot be used and
        // the operator has to move visitCtrl's select_rank to one that fits.
        MagAOX::wcc::visitSelection sel;
        sel.m_ra = 10.0;
        sel.m_dec = 20.0;
        sel.m_guide.m_sensor = "IMX-99";
        sel.m_roll.m_sensor = "HWK-08";

        REQUIRE( app.testResolveStars( sel ) < 0 );

        // Also when only the roll star is missing.
        sel.m_guide.m_sensor = "IMX-19";
        sel.m_roll.m_sensor = "IMX-98";

        REQUIRE( app.testResolveStars( sel ) < 0 );
    }

    SECTION( "a pixel displacement converts to a field shift at the plate scale" )
    {
        app.focalPlane().setPointing( 10.0, 20.0, 0.0 );

        double fx = 0, fy = 0;
        const double cx = app.focalPlane().sensor( 0 ).centerX();
        const double cy = app.focalPlane().sensor( 0 ).centerY();

        // With no mounting rotation, 100 pixels along x is 100 plate scales in
        // field angle and nothing along y.
        app.testPixelToFieldShift( 0, cx, cy, 100.0, 0.0, fx, fy );

        const double k = app.focalPlane().arcsecPerPixel( app.focalPlane().sensor( 0 ) );
        REQUIRE( fx == Approx( 100.0 * k ).epsilon( 1e-9 ) );
        REQUIRE( fy == Approx( 0.0 ).margin( 1e-12 ) );

        app.testPixelToFieldShift( 0, cx, cy, 0.0, -50.0, fx, fy );
        REQUIRE( fx == Approx( 0.0 ).margin( 1e-12 ) );
        REQUIRE( fy == Approx( -50.0 * k ).epsilon( 1e-9 ) );
    }

    std::remove( path.c_str() );
}

} // namespace wccCtrlTest

} // namespace libXWCTest
