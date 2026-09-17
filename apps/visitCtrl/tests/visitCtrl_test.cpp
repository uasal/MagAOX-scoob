/** \file visitCtrl_test.cpp
 * \brief Catch2 tests for the visitCtrl app.
 * \author Adam Schilperoort
 *
 * \ingroup visitCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <fstream>

#include "../visitCtrl.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup visitCtrl_unit_test visitCtrl Unit Tests
 * \brief Unit tests for the visitCtrl application.
 *
 * The JSON reader and the visit schema are covered by \ref wccCommon_unit_test.
 * What is tested here is the loader itself: that a file becomes a published
 * selection, that rank selection picks the pair asked for, and that a bad file
 * leaves the app in a reportable error state rather than publishing something
 * stale.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `visitCtrl` unit tests.
/** \ingroup visitCtrl_unit_test
 */
namespace visitCtrlTest
{

/// Expose the protected surface of visitCtrl for testing.
/** \cond
 */
class visitCtrlTester : public visitCtrl
{
  public:
    /// Register the config targets, read a file, and load it.
    int testLoadConfigFile( const std::string &path )
    {
        setupConfig();
        config.readConfig( path );
        return loadConfigImpl( config );
    }

    /// Set the visit path directly, as the INDI callback would.
    void setPath( const std::string &p )
    {
        m_visitPath = p;
    }

    /// Set the rank to select, as the INDI callback would.
    void setRank( int r )
    {
        m_selectRank = r;
    }

    /// Read the visit file and resolve the selection.
    int testLoad()
    {
        return loadVisitFile();
    }

    /// The published selection.
    const MagAOX::wcc::visitSelection &selection()
    {
        return m_selection;
    }

    /// The loader state.
    std::string loaderState()
    {
        return m_state;
    }

    /// The loader message.
    std::string loaderMessage()
    {
        return m_message;
    }
};
/** \endcond
 */

/// Write a minimal visit file with two ranked star pairs.
/** \returns the path written
 *
 * \ingroup visitCtrl_unit_test
 */
inline std::string writeVisit( const std::string &path /**< [in] where to write */ )
{
    std::ofstream fout( path );

    fout << R"({
      "PROGMID":"072226","TELESCOP":"LAZULI","OBSID":"001","VISITID":"001",
      "TARGET":"TEST FIELD",
      "RA_PROP":192.317,"DEC_PROP":26.84316038,"ROLLPA":12.5,
      "TARGET_ACQUISITION":{
        "CONFIG_SENSORS":["IMX-18","HWK-09"],
        "TA_EXPTIME_SEC_SLOANR":[1.5],"TA_FRAME_RATE_HZ":4,
        "TA_GUIDE_TOL_PIX":40,"TA_ROLL_TOL_PIX":30,"TA_MAX_ITERATIONS":4
      },
      "GUIDE_STAR":[
        {"RANK":1,"ID":"G1","SENSOR":"IMX-18","RA":192.1,"DEC":26.8,
         "X_WCC":-640.0,"Y_WCC":-68.8,"MAG":14.2,
         "TARGET_X_PIX":4788,"TARGET_Y_PIX":3194,
         "EXP_TIME_FG":0.01,"FRAME_RATE_FG":100,"ROI_W_FG":128,"ROI_H_FG":128,
         "CATALOG_NAME":"gsc31_north.csv"},
        {"RANK":2,"ID":"G2","SENSOR":"IMX-19","RA":192.13,"DEC":26.81,
         "X_WCC":-493.0,"Y_WCC":-109.3,"MAG":15.1}],
      "ROLL_STAR":[
        {"RANK":1,"ID":"R1","SENSOR":"HWK-09","RA":192.63,"DEC":26.86,
         "X_WCC":1029.51,"Y_WCC":59.31,"MAG":13.8},
        {"RANK":2,"ID":"R2","SENSOR":"HWK-08","RA":192.55,"DEC":26.87,
         "X_WCC":786.51,"Y_WCC":102.09,"MAG":14.4}],
      "TRACKING":{"ROI_W":64,"ROI_H":64,"FRAME_RATE":250,"EXPTIME":0.002,
                  "CENTROID_DEVICE_GUIDE":"cguide","CENTROID_DEVICE_ROLL":"croll",
                  "LOOP_GAIN":0.45,"ROLL_GAIN":0.2}
    })";

    return path;
}

/// Verify visitCtrl constructs and reads its configuration.
/**
 * \ingroup visitCtrl_unit_test
 */
TEST_CASE( "visitCtrl constructs and reads configuration", "[visitCtrl]" )
{
    // clang-format off
    #ifdef VISITCTRL_TEST_DOXYGEN_REF
    visitCtrl();
    visitCtrl::setupConfig;
    visitCtrl::appStartup;
    visitCtrl::appLogic;
    visitCtrl::appShutdown;
    #endif
    // clang-format on

    SECTION( "default construction succeeds" )
    {
        visitCtrl app;

        REQUIRE( true );
    }

    SECTION( "configuration is read and a negative rank is normalized" )
    {
        visitCtrlTester app;

        const std::string path = "/tmp/visitCtrl_test_config.conf";
        {
            std::ofstream fout( path );
            fout << "[visit]\npath=/tmp/nope.json\nload_at_startup=true\nselect_rank=-3\n";
        }

        REQUIRE( app.testLoadConfigFile( path ) == 0 );

        // A negative rank has no meaning; it becomes 0, which means "best available".
        REQUIRE( app.selection().m_valid == false );

        std::remove( path.c_str() );
    }
}

/// Verify a visit file becomes a published selection.
/**
 * \ingroup visitCtrl_unit_test
 */
TEST_CASE( "visitCtrl publishes a selection from a visit file", "[visitCtrl]" )
{
    // clang-format off
    #ifdef VISITCTRL_TEST_DOXYGEN_REF
    visitCtrl::loadVisitFile;
    visitCtrl::applySelection;
    #endif
    // clang-format on

    const std::string vpath = writeVisit( "/tmp/visitCtrl_test_visit.json" );

    SECTION( "rank 0 selects the best available pair" )
    {
        visitCtrlTester app;
        app.setPath( vpath );
        app.setRank( 0 );

        REQUIRE( app.testLoad() == 0 );
        REQUIRE( app.loaderState() == MagAOX::wcc::visitIndi::stateLoaded );

        const MagAOX::wcc::visitSelection &sel = app.selection();
        REQUIRE( sel.m_valid );
        REQUIRE( sel.m_targetName == "TEST FIELD" );
        REQUIRE( sel.m_ra == Approx( 192.317 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_dec == Approx( 26.84316038 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_rollPA == Approx( 12.5 ).epsilon( 1e-12 ) );

        // Rank 1 is the best available, and its roll star is the rank 1 one.
        REQUIRE( sel.m_rank == 1 );
        REQUIRE( sel.m_guide.m_id == "G1" );
        REQUIRE( sel.m_guide.m_sensor == "IMX-18" );
        REQUIRE( sel.m_guide.m_fieldX == Approx( -640.0 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_guide.m_mag == Approx( 14.2 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_guide.m_targetX == Approx( 4788.0 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_roll.m_id == "R1" );
        REQUIRE( sel.m_roll.m_sensor == "HWK-09" );

        // Acquisition and tracking parameters come through.
        REQUIRE( sel.m_taExpTime == Approx( 1.5 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_taFrameRate == Approx( 4.0 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_guideTolPix == Approx( 40.0 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_rollTolPix == Approx( 30.0 ).epsilon( 1e-12 ) );
        REQUIRE( sel.m_maxIterations == 4 );
        REQUIRE( sel.m_tracking.m_roiW == 64 );
        REQUIRE( sel.m_tracking.m_centroidGuide == "cguide" );
        REQUIRE( sel.m_tracking.m_loopGain == Approx( 0.45 ).epsilon( 1e-12 ) );

        // The sensor list round trips through the comma separated wire form.
        REQUIRE( sel.m_configSensors.size() == 2 );
        REQUIRE( sel.configSensorList() == "IMX-18,HWK-09" );
    }

    SECTION( "a specific rank can be selected" )
    {
        visitCtrlTester app;
        app.setPath( vpath );
        app.setRank( 2 );

        REQUIRE( app.testLoad() == 0 );

        const MagAOX::wcc::visitSelection &sel = app.selection();
        REQUIRE( sel.m_rank == 2 );
        REQUIRE( sel.m_guide.m_id == "G2" );
        REQUIRE( sel.m_guide.m_sensor == "IMX-19" );
        REQUIRE( sel.m_roll.m_id == "R2" );
        REQUIRE( sel.m_roll.m_sensor == "HWK-08" );
    }

    SECTION( "a rank the visit does not have is an error" )
    {
        visitCtrlTester app;
        app.setPath( vpath );
        app.setRank( 9 );

        REQUIRE( app.testLoad() < 0 );
        REQUIRE( app.loaderState() == MagAOX::wcc::visitIndi::stateError );
        REQUIRE_FALSE( app.loaderMessage().empty() );
        REQUIRE_FALSE( app.selection().m_valid );
    }

    SECTION( "a missing file leaves an error state and publishes nothing" )
    {
        visitCtrlTester app;
        app.setPath( "/tmp/visitCtrl_test_does_not_exist.json" );

        REQUIRE( app.testLoad() < 0 );
        REQUIRE( app.loaderState() == MagAOX::wcc::visitIndi::stateError );
        REQUIRE_FALSE( app.selection().m_valid );
    }

    SECTION( "an empty path is an error" )
    {
        visitCtrlTester app;
        app.setPath( "" );

        REQUIRE( app.testLoad() < 0 );
        REQUIRE( app.loaderState() == MagAOX::wcc::visitIndi::stateError );
    }

    SECTION( "a malformed file is an error and does not publish a partial selection" )
    {
        const std::string bad = "/tmp/visitCtrl_test_bad.json";
        {
            std::ofstream fout( bad );
            fout << "{ \"RA_PROP\": 1.0, ";
        }

        visitCtrlTester app;
        app.setPath( bad );

        REQUIRE( app.testLoad() < 0 );
        REQUIRE( app.loaderState() == MagAOX::wcc::visitIndi::stateError );
        REQUIRE_FALSE( app.selection().m_valid );

        std::remove( bad.c_str() );
    }

    std::remove( vpath.c_str() );
}

/// Verify the sensor list wire form round trips.
/**
 * \ingroup visitCtrl_unit_test
 */
TEST_CASE( "visitSelection sensor list round trips through the wire form", "[visitCtrl]" )
{
    // clang-format off
    #ifdef VISITCTRL_TEST_DOXYGEN_REF
    MagAOX::wcc::visitSelection::configSensorList;
    MagAOX::wcc::visitSelection::setConfigSensorList;
    #endif
    // clang-format on

    MagAOX::wcc::visitSelection sel;

    sel.m_configSensors = { "IMX-18", "HWK-09", "IMX-21" };
    REQUIRE( sel.configSensorList() == "IMX-18,HWK-09,IMX-21" );

    MagAOX::wcc::visitSelection back;
    back.setConfigSensorList( sel.configSensorList() );
    REQUIRE( back.m_configSensors == sel.m_configSensors );

    SECTION( "whitespace and empty entries are tolerated" )
    {
        MagAOX::wcc::visitSelection s;
        s.setConfigSensorList( " IMX-18 , ,HWK-09,, " );

        REQUIRE( s.m_configSensors.size() == 2 );
        REQUIRE( s.m_configSensors[0] == "IMX-18" );
        REQUIRE( s.m_configSensors[1] == "HWK-09" );
    }

    SECTION( "an empty list yields no sensors" )
    {
        MagAOX::wcc::visitSelection s;
        s.setConfigSensorList( "" );

        REQUIRE( s.m_configSensors.empty() );
        REQUIRE( s.configSensorList().empty() );
    }
}

} // namespace visitCtrlTest

} // namespace libXWCTest
