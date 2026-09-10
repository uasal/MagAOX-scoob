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
    /// Run loadConfigImpl against a configurator built by a test.
    int testLoadConfig( mx::app::appConfigurator &cfg )
    {
        return loadConfigImpl( cfg );
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

    /// Install a visit directly, bypassing the file, then resolve its stars.
    int testSelectStars( const MagAOX::wcc::visitFile &vf )
    {
        m_visit = vf;
        m_visitLoaded = true;
        return selectStars();
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
        mx::app::appConfigurator cfg;
        app.setupConfig();

        const std::string path = writeCtrlConfig(
            "/tmp/wccCtrl_test_config.conf", "IMX-18,HWK-09",
            "[IMX-18]\nindi_device=nsv18\npixel_size=3.76\nfull_w=9576\nfull_h=6388\n"
            "field_x=-849.953\nfield_y=-68.782\n"
            "[HWK-09]\nindi_device=hwk09\nshmim_in=hwk09custom\npixel_size=4.6\n"
            "full_w=4096\nfull_h=2300\nfield_x=1029.51\nfield_y=59.31\n" );

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) == 0 );
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
        mx::app::appConfigurator cfg;
        app.setupConfig();

        const std::string path = "/tmp/wccCtrl_test_config_notel.conf";
        {
            std::ofstream fout( path );
            fout << "[catalog]\npath=/dev/null\n";
            fout << "[acq]\nsensors=A\n";
            fout << "[A]\nindi_device=a\npixel_size=3.76\nfull_w=512\nfull_h=512\n";
        }

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) < 0 );

        std::remove( path.c_str() );
    }

    SECTION( "an empty sensor list is rejected" )
    {
        wccCtrlTester app;
        mx::app::appConfigurator cfg;
        app.setupConfig();

        const std::string path = "/tmp/wccCtrl_test_config_nosensors.conf";
        {
            std::ofstream fout( path );
            fout << "[telescope]\ndevice=telsim\n";
            fout << "[catalog]\npath=/dev/null\n";
        }

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) < 0 );

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
    wccCtrl::selectStars;
    wccCtrl::pixelToFieldShift;
    #endif
    // clang-format on

    // Configure an array holding only IMX-19 and HWK-08.
    wccCtrlTester app;
    mx::app::appConfigurator cfg;
    app.setupConfig();

    const std::string path = writeCtrlConfig(
        "/tmp/wccCtrl_test_select.conf", "IMX-19,HWK-08",
        "[IMX-19]\nindi_device=nsv19\npixel_size=3.76\nfull_w=9576\nfull_h=6388\n"
        "field_x=-666.282\nfield_y=-109.31\n"
        "[HWK-08]\nindi_device=hwk08\npixel_size=4.6\nfull_w=4096\nfull_h=2300\n"
        "field_x=786.51\nfield_y=102.092\n" );

    cfg.readConfig( path );
    REQUIRE( app.testLoadConfig( cfg ) == 0 );

    SECTION( "rank 1 is skipped when its sensor is not configured" )
    {
        // Rank 1 sits on IMX-18, which this array does not have. Rank 2 is on
        // IMX-19, which it does, so rank 2 must be chosen rather than failing.
        const std::string doc = R"({
          "RA_PROP":10.0,"DEC_PROP":20.0,
          "GUIDE_STAR":[
            {"RANK":1,"SENSOR":"IMX-18","RA":10.1,"DEC":20.1,"X_WCC":-849.9,"Y_WCC":-68.8},
            {"RANK":2,"SENSOR":"IMX-19","RA":10.2,"DEC":20.2,"X_WCC":-666.3,"Y_WCC":-109.3}],
          "ROLL_STAR":[
            {"RANK":1,"SENSOR":"HAWK-09","RA":9.9,"DEC":19.9,"X_WCC":1029.5,"Y_WCC":59.3},
            {"RANK":2,"SENSOR":"HAWK-08","RA":9.8,"DEC":19.8,"X_WCC":786.5,"Y_WCC":102.1}]
        })";

        MagAOX::wcc::jsonParser p;
        MagAOX::wcc::jsonValue root;
        REQUIRE( p.parse( doc, root ) == 0 );

        MagAOX::wcc::visitFile vf;
        std::string err;
        REQUIRE( vf.loadJSON( root, err ) == 0 );

        REQUIRE( app.testSelectStars( vf ) == 0 );
        REQUIRE( app.guideSensor() == 0 );
        REQUIRE( app.guideStar().m_rank == 2 );
        REQUIRE( app.guideStar().m_sensor == "IMX-19" );

        // The roll star of the same rank is preferred, and HAWK-08 must match the
        // configured HWK-08.
        REQUIRE( app.rollSensor() == 1 );
        REQUIRE( app.rollStar().m_sensor == "HAWK-08" );
    }

    SECTION( "no usable pair is an error" )
    {
        const std::string doc = R"({
          "RA_PROP":10.0,"DEC_PROP":20.0,
          "GUIDE_STAR":[{"RANK":1,"SENSOR":"IMX-99","RA":10.1,"DEC":20.1,"X_WCC":0,"Y_WCC":0}],
          "ROLL_STAR":[{"RANK":1,"SENSOR":"IMX-98","RA":9.9,"DEC":19.9,"X_WCC":0,"Y_WCC":0}]
        })";

        MagAOX::wcc::jsonParser p;
        MagAOX::wcc::jsonValue root;
        REQUIRE( p.parse( doc, root ) == 0 );

        MagAOX::wcc::visitFile vf;
        std::string err;
        REQUIRE( vf.loadJSON( root, err ) == 0 );

        REQUIRE( app.testSelectStars( vf ) < 0 );
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
