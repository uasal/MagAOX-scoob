/** \file bmcCtrl_test.cpp
 * \brief Catch2 tests for the bmcCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup bmcCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../bmcCtrl.hpp"

// Included after the app header so the dev::dm callback bodies stay live.  These callbacks check the
// property key directly and return 0 for a property with no elements.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Fake state for the BMC SDK stubs declared in `stubs/BMCApi.h` and `stubs/BMC_PCIeApi.h`.
struct bmcStubState
{
    BMCRC       m_openRet{ NO_ERR }; ///< Return value of BMCOpen().
    uint32_t    m_actCount{ 0 };     ///< Actuator count set in the DM handle by BMCOpen().
    int         m_openCalls{ 0 };    ///< Number of BMCOpen() calls.
    std::string m_openSerial;        ///< Serial number passed to the last BMCOpen().

    BMCRC            m_highResRet{ NO_ERR }; ///< Return value of BMC_PCIeEnableHighRes().
    std::vector<int> m_highResArgs;          ///< The enable arguments passed to BMC_PCIeEnableHighRes().

    BMCRC m_loadMapRet{ NO_ERR };     ///< Return value of BMCLoadMap().
    int   m_loadMapCalls{ 0 };        ///< Number of BMCLoadMap() calls.
    bool  m_loadMapPathNull{ false }; ///< Whether the last BMCLoadMap() map path was nullptr.

    BMCRC               m_setRet{ NO_ERR };    ///< Return value of BMCSetArray().
    int                 m_setCalls{ 0 };       ///< Number of BMCSetArray() calls.
    size_t              m_setLen{ 0 };         ///< Number of values captured from each BMCSetArray().
    std::vector<double> m_lastSet;             ///< Values passed to the last BMCSetArray().
    bool                m_setLutNull{ false }; ///< Whether the last BMCSetArray() map lookup table was nullptr.

    BMCRC m_clearRet{ NO_ERR }; ///< Return value of BMCClearArray().
    int   m_clearCalls{ 0 };    ///< Number of BMCClearArray() calls.

    BMCRC m_closeRet{ NO_ERR }; ///< Return value of BMCClose().
    int   m_closeCalls{ 0 };    ///< Number of BMCClose() calls.
};

/// The global BMC stub state.
bmcStubState g_bmc;

/// Reset the BMC stub state to defaults.
void resetBmcStub()
{
    g_bmc = bmcStubState();
}

const char *BMCErrorString( BMCRC err )
{
    return ( err == NO_ERR ) ? "no error" : "stub error";
}

BMCRC BMCOpen( DM *dm, const char *serialNumber )
{
    ++g_bmc.m_openCalls;
    g_bmc.m_openSerial = serialNumber;

    if( g_bmc.m_openRet == NO_ERR )
    {
        dm->ActCount = g_bmc.m_actCount;
        dm->m_id     = 1;
    }

    return g_bmc.m_openRet;
}

BMCRC BMC_PCIeEnableHighRes( DM *dm, int enable )
{
    static_cast<void>( dm );
    g_bmc.m_highResArgs.push_back( enable );
    return g_bmc.m_highResRet;
}

BMCRC BMCLoadMap( DM *dm, const char *mapPath, uint32_t *mapLut )
{
    static_cast<void>( dm );
    static_cast<void>( mapLut );
    ++g_bmc.m_loadMapCalls;
    g_bmc.m_loadMapPathNull = ( mapPath == nullptr );
    return g_bmc.m_loadMapRet;
}

BMCRC BMCSetArray( DM *dm, const double *array, const uint32_t *mapLut )
{
    static_cast<void>( dm );
    ++g_bmc.m_setCalls;
    g_bmc.m_lastSet.assign( array, array + g_bmc.m_setLen );
    g_bmc.m_setLutNull = ( mapLut == nullptr );
    return g_bmc.m_setRet;
}

BMCRC BMCClearArray( DM *dm )
{
    static_cast<void>( dm );
    ++g_bmc.m_clearCalls;
    return g_bmc.m_clearRet;
}

BMCRC BMCClose( DM *dm )
{
    static_cast<void>( dm );
    ++g_bmc.m_closeCalls;
    return g_bmc.m_closeRet;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup bmcCtrl_unit_test bmcCtrl Unit Tests
 * \brief Unit tests for the bmcCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `bmcCtrl` unit tests.
/** \ingroup bmcCtrl_unit_test
 */
namespace bmcCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Scratch directory for the calibration files and shared memory used by these tests.
const std::string g_tmpDir = "/tmp/bmcCtrl_test";

/// Test harness exposing bmcCtrl internals.
class bmcCtrl_test : public bmcCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit bmcCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        m_calibDir   = g_tmpDir + "/calib";

        createStandardIndiRequestSw( m_indiP_init, "initDM" );
        createStandardIndiRequestSw( m_indiP_zero, "zeroDM" );
        createStandardIndiRequestSw( m_indiP_release, "releaseDM" );
        createStandardIndiRequestSw( m_indiP_zeroAll, "zeroAll" );
    }

    using bmcCtrl::m_act_gain;
    using bmcCtrl::m_actuator_mapping;
    using bmcCtrl::m_calibDir;
    using bmcCtrl::m_calibPath;
    using bmcCtrl::m_calibRelDir;
    using bmcCtrl::m_dm;
    using bmcCtrl::m_dminputs;
    using bmcCtrl::m_dmopen;
    using bmcCtrl::m_instSatMap;
    using bmcCtrl::m_nbAct;
    using bmcCtrl::m_nsat;
    using bmcCtrl::m_outputShape;
    using bmcCtrl::m_powerMgtEnabled;
    using bmcCtrl::m_satThresh;
    using bmcCtrl::m_serialNumber;
    using bmcCtrl::m_shutdown;
    using bmcCtrl::m_volume_factor;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Put the app in the state of an opened DM without calling initDM().
    /** Allocates the command vector and actuator mapping as initDM() would.  Also sets m_shutdown so that
     * releaseDM() does not signal the (never started) shmim monitor thread.
     */
    void fakeInit( const std::vector<int> &mapping,     /**< [in] the actuator mapping, -1 for ignored actuators */
                   double                  actGain,     /**< [in] the actuator gain */
                   double                  volumeFactor /**< [in] the volume factor */
    )
    {
        m_dmopen        = true;
        m_dm.ActCount   = mapping.size();
        m_dm.m_id       = 1;
        m_nbAct         = mapping.size();
        m_act_gain      = actGain;
        m_volume_factor = volumeFactor;

        if( m_dminputs )
        {
            free( m_dminputs );
        }
        m_dminputs = (double *)calloc( m_nbAct, sizeof( double ) );

        if( m_actuator_mapping )
        {
            free( m_actuator_mapping );
        }
        m_actuator_mapping = (int *)malloc( m_nbAct * sizeof( int ) );
        for( size_t n = 0; n < mapping.size(); ++n )
        {
            m_actuator_mapping[n] = mapping[n];
        }

        g_bmc.m_setLen = m_nbAct;

        m_shutdown = 1;
    }
};

/// Write a two-line BMC user calibration file.
void writeCalibFile( const std::string &dir,         /**< [in] the calibration directory */
                     double             actGain,     /**< [in] the actuator gain */
                     double             volumeFactor /**< [in] the volume factor */
)
{
    std::filesystem::create_directories( dir );
    std::ofstream fout( dir + "/bmc_2k_userconfig.txt" );
    fout << actGain << " # actuator gain\n";
    fout << volumeFactor << " # volume factor\n";
}

/// Point MILK_SHM_DIR at the test scratch directory so shared memory stays under /tmp.
void setupShmDir()
{
    std::filesystem::create_directories( g_tmpDir + "/shm" );
    setenv( "MILK_SHM_DIR", ( g_tmpDir + "/shm" ).c_str(), 1 );
}

/// \endcond

/// Verify bmcCtrl construction and configuration defaults.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl configuration defaults", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::bmcCtrl();
    bmcCtrl::setupConfig();
    bmcCtrl::loadConfig();
    bmcCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    resetBmcStub();

    mx::app::writeConfigFile( "/tmp/bmcCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );

    bmcCtrl_test app( "bmcCtrl" );

    // Power management is enabled by the constructor
    REQUIRE( app.m_powerMgtEnabled == true );

    app.configure( "/tmp/bmcCtrl_test_defaults.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serialNumber == "" );
    REQUIRE( app.m_satThresh == 100000 );
    REQUIRE( app.m_calibRelDir == "" );
    REQUIRE( app.calibPath() == g_tmpDir + "/calib/" );
    REQUIRE( app.shmimName() == "" );
    REQUIRE( app.m_dmopen == false );

    REQUIRE( g_bmc.m_openCalls == 0 );

    std::remove( "/tmp/bmcCtrl_test_defaults.conf" );
}

/// Verify bmcCtrl configuration overrides.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl configuration overrides", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::setupConfig();
    bmcCtrl::loadConfig();
    bmcCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    resetBmcStub();

    mx::app::writeConfigFile( "/tmp/bmcCtrl_test_over.conf",
                              { "dm", "dm", "dm", "dm" },
                              { "serialNumber", "calibRelDir", "satThresh", "shmimName" },
                              { "27BW007_081", "dm/bmc_2k", "500", "dm01disp" } );

    bmcCtrl_test app( "bmcCtrl" );
    app.configure( "/tmp/bmcCtrl_test_over.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serialNumber == "27BW007_081" );
    REQUIRE( app.m_satThresh == 500 );
    REQUIRE( app.m_calibRelDir == "dm/bmc_2k" );
    REQUIRE( app.calibPath() == g_tmpDir + "/calib/dm/bmc_2k" );
    REQUIRE( app.flatPath() == g_tmpDir + "/calib/dm/bmc_2k/flats" );
    REQUIRE( app.testPath() == g_tmpDir + "/calib/dm/bmc_2k/tests" );
    REQUIRE( app.shmimName() == "dm01disp" );
    REQUIRE( app.shmimFlat() == "dm01disp00" );
    REQUIRE( app.shmimSatPerc() == "dm01dispSP" );
    REQUIRE( app.shmimDelta() == "dm01disp_delta" );

    std::remove( "/tmp/bmcCtrl_test_over.conf" );
}

/// Verify parsing of the BMC user calibration file and the appStartup() calibration checks.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl calibration file and appStartup checks", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::parse_calibration_file();
    bmcCtrl::appStartup();
    #endif
    // clang-format on

    resetBmcStub();

    const std::string dir = g_tmpDir + "/parse";
    std::filesystem::remove_all( dir );

    bmcCtrl_test app( "bmcCtrl" );
    app.m_calibPath = dir;

    SECTION( "a valid file sets the actuator gain and volume factor" )
    {
        writeCalibFile( dir, 2.5, 0.75 );

        REQUIRE( app.parse_calibration_file() == 0 );
        REQUIRE( app.m_act_gain == Approx( 2.5 ) );
        REQUIRE( app.m_volume_factor == Approx( 0.75 ) );
    }

    SECTION( "a missing file is an error" )
    {
        REQUIRE( app.parse_calibration_file() == -1 );
        REQUIRE( app.m_act_gain == 0 );
        REQUIRE( app.appStartup() == -1 );
    }

    SECTION( "appStartup rejects a zero actuator gain" )
    {
        writeCalibFile( dir, 0.0, 0.75 );
        REQUIRE( app.appStartup() == -1 );
    }

    SECTION( "appStartup rejects a zero volume factor" )
    {
        writeCalibFile( dir, 2.5, 0.0 );
        REQUIRE( app.appStartup() == -1 );
    }

    REQUIRE( g_bmc.m_openCalls == 0 );

    std::filesystem::remove_all( dir );
}

/// Verify reading the actuator mapping from a FITS file.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl get_actuator_mapping", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::get_actuator_mapping();
    #endif
    // clang-format on

    resetBmcStub();

    const std::string dir = g_tmpDir + "/mapping";
    std::filesystem::create_directories( dir );

    bmcCtrl_test app( "bmcCtrl" );
    app.m_calibPath = dir;
    app.fakeInit( { -1, -1, -1, -1 }, 1.0, 1.0 );

    SECTION( "pixel values are 1-based actuator numbers mapped to linear grid indices" )
    {
        // 3x3 map: actuator 2 at linear index 0, actuator 1 at 2, actuator 3 at 7; actuator 4 absent
        mx::improc::eigenImage<int> im( 3, 3 );
        im.setZero();
        im( 0, 0 ) = 2;
        im( 2, 0 ) = 1;
        im( 1, 2 ) = 3;

        mx::fits::fitsFile<int> ff;
        bool written = ( ff.write( dir + "/bmc_2k_actuator_mapping.fits", im ) == mx::error_t::noerror );
        REQUIRE( written );

        REQUIRE( app.get_actuator_mapping() == 0 );

        REQUIRE( app.m_actuator_mapping[0] == 2 );
        REQUIRE( app.m_actuator_mapping[1] == 0 );
        REQUIRE( app.m_actuator_mapping[2] == 7 );
        REQUIRE( app.m_actuator_mapping[3] == -1 );
    }

    SECTION( "a missing file leaves the mapping unchanged (and still returns 0)" )
    {
        std::filesystem::remove( dir + "/bmc_2k_actuator_mapping.fits" );

        REQUIRE( app.get_actuator_mapping() == 0 );

        for( int n = 0; n < 4; ++n )
        {
            REQUIRE( app.m_actuator_mapping[n] == -1 );
        }
    }

    app.m_dmopen = false;
    std::filesystem::remove_all( dir );
}

/// Verify initDM() drives the SDK correctly on success.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl initDM success", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::initDM();
    #endif
    // clang-format on

    resetBmcStub();
    g_bmc.m_actCount = 50;
    g_bmc.m_setLen   = 50;

    bmcCtrl_test app( "bmcCtrl" );
    app.m_serialNumber = "27bw007#081";
    app.m_calibPath    = g_tmpDir + "/nonexistent";
    app.m_shutdown     = 1;

    SECTION( "all SDK calls succeed" )
    {
        REQUIRE( app.initDM() == 0 );

        // Serial number is upper-cased
        REQUIRE( g_bmc.m_openCalls == 1 );
        REQUIRE( g_bmc.m_openSerial == "27BW007#081" );
        REQUIRE( app.m_dmopen == true );

        // High resolution mode is enabled
        REQUIRE( g_bmc.m_highResArgs.size() == 1 );
        REQUIRE( g_bmc.m_highResArgs[0] == 1 );

        // Actuator count comes from the handle
        REQUIRE( app.m_nbAct == 50 );

        // The default map is loaded
        REQUIRE( g_bmc.m_loadMapCalls == 1 );
        REQUIRE( g_bmc.m_loadMapPathNull == true );

        // The DM is zeroed without a lookup table
        REQUIRE( g_bmc.m_setCalls == 1 );
        REQUIRE( g_bmc.m_setLutNull == true );
        REQUIRE( g_bmc.m_lastSet.size() == 50 );
        for( size_t n = 0; n < 50; ++n )
        {
            REQUIRE( g_bmc.m_lastSet[n] == 0 );
        }

        // Mapping is allocated and initialized to "ignored" when there is no mapping file
        REQUIRE( app.m_dminputs != nullptr );
        REQUIRE( app.m_actuator_mapping != nullptr );
        for( uint32_t n = 0; n < app.m_nbAct; ++n )
        {
            REQUIRE( app.m_actuator_mapping[n] == -1 );
        }

        REQUIRE( app.state() == stateCodes::READY );

        // A second initDM is refused
        REQUIRE( app.initDM() == -1 );
        REQUIRE( g_bmc.m_openCalls == 1 );
    }

    SECTION( "a high resolution failure is not fatal" )
    {
        g_bmc.m_highResRet = ERR_UNKNOWN;

        REQUIRE( app.initDM() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    app.m_dmopen = false;
}

/// Verify initDM() error handling.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl initDM failures", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::initDM();
    #endif
    // clang-format on

    resetBmcStub();
    g_bmc.m_actCount = 8;
    g_bmc.m_setLen   = 8;

    bmcCtrl_test app( "bmcCtrl" );
    app.m_serialNumber = "ser";
    app.m_calibPath    = g_tmpDir + "/nonexistent";
    app.m_shutdown     = 1;

    SECTION( "open failure" )
    {
        g_bmc.m_openRet = ERR_NO_HW;

        REQUIRE( app.initDM() == -1 );
        REQUIRE( app.m_dmopen == false );
        REQUIRE( g_bmc.m_loadMapCalls == 0 );
        REQUIRE( g_bmc.m_setCalls == 0 );
    }

    SECTION( "map load failure" )
    {
        g_bmc.m_loadMapRet = ERR_UNKNOWN;

        REQUIRE( app.initDM() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_bmc.m_setCalls == 0 );
        REQUIRE( app.m_dm.ActCount == 0 ); // handle is reset
    }

    SECTION( "zeroing failure" )
    {
        g_bmc.m_setRet = ERR_UNKNOWN;

        REQUIRE( app.initDM() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_bmc.m_setCalls == 1 );
    }

    SECTION( "zero actuators makes zeroing fail" )
    {
        g_bmc.m_actCount = 0;

        REQUIRE( app.initDM() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_bmc.m_setCalls == 0 );
    }

    app.m_dmopen = false;
}

/// Verify zeroDM().
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl zeroDM", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::zeroDM();
    #endif
    // clang-format on

    resetBmcStub();

    bmcCtrl_test app( "bmcCtrl" );

    SECTION( "not open" )
    {
        REQUIRE( app.zeroDM() == -1 );
        REQUIRE( g_bmc.m_setCalls == 0 );
    }

    SECTION( "no actuators" )
    {
        app.m_dmopen = true;
        REQUIRE( app.zeroDM() == -1 );
        REQUIRE( g_bmc.m_setCalls == 0 );
    }

    SECTION( "success sends all zeros" )
    {
        app.fakeInit( { 0, 1, 2 }, 1.0, 1.0 );
        app.m_dminputs[1] = 0.3; // the pre-allocated command vector is not what is sent

        REQUIRE( app.zeroDM() == 0 );
        REQUIRE( g_bmc.m_setCalls == 1 );
        REQUIRE( g_bmc.m_lastSet.size() == 3 );
        for( size_t n = 0; n < 3; ++n )
        {
            REQUIRE( g_bmc.m_lastSet[n] == 0 );
        }
    }

    SECTION( "SDK failure" )
    {
        app.fakeInit( { 0, 1, 2 }, 1.0, 1.0 );
        g_bmc.m_setRet = ERR_UNKNOWN;

        REQUIRE( app.zeroDM() == -1 );
    }

    app.m_dmopen = false;
}

/// Verify commandDM() scaling, square-root voltage conversion, clamping, mapping, and saturation bookkeeping.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl commandDM", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::commandDM(void *);
    #endif
    // clang-format on

    resetBmcStub();
    setupShmDir();

    {
        bmcCtrl_test app( "bmcCtrl" );

        // 2x2 grid; actuator 2 is addressable but ignored.  Gain scale = 1/4 = 0.25, inverse = 4.
        app.fakeInit( { 0, 3, -1, 2 }, 4.0, 1.0 );

        app.m_outputShape.create( "bmcCtrlTest_shape", 2, 2 );
        app.m_outputShape().setZero();
        app.m_instSatMap.resize( 2, 2 );
        app.m_instSatMap.setZero();

        SECTION( "in-range, over-range, under-range, and ignored actuators" )
        {
            // idx0: 1*0.25 -> sqrt = 0.5; idx1: 16*0.25 = 4 -> clamp 1; idx2: ignored; idx3: -2*0.25 < 0 -> 0
            float src[4] = { 1.0f, 7.0f, -2.0f, 16.0f };

            REQUIRE( app.commandDM( src ) == 0 );

            REQUIRE( g_bmc.m_setCalls == 1 );
            REQUIRE( g_bmc.m_setLutNull == true );
            REQUIRE( g_bmc.m_lastSet.size() == 4 );
            REQUIRE( g_bmc.m_lastSet[0] == Approx( 0.5 ) );
            REQUIRE( g_bmc.m_lastSet[1] == Approx( 1.0 ) );
            REQUIRE( g_bmc.m_lastSet[2] == 0 );
            REQUIRE( g_bmc.m_lastSet[3] == 0 );

            // Output shape: the input where in range, the maximum where clamped high, 0 where clamped low
            REQUIRE( app.m_outputShape[0] == Approx( 1.0 ) );
            REQUIRE( app.m_outputShape[3] == Approx( 4.0 ) );
            REQUIRE( app.m_outputShape[2] == 0 );
            REQUIRE( app.m_outputShape[1] == 0 ); // not mapped

            // Saturation: clamped actuators are counted and flagged, ignored ones are skipped
            REQUIRE( app.m_nsat == 2 );
            REQUIRE( app.m_instSatMap.data()[0] == 0 );
            REQUIRE( app.m_instSatMap.data()[3] == 1 );
            REQUIRE( app.m_instSatMap.data()[2] == 1 );
            REQUIRE( app.m_instSatMap.data()[1] == 0 );
        }

        SECTION( "the voltage is the square root of the fractional displacement" )
        {
            float src[4] = { 0.36f, 0.0f, 0.0f, 2.56f };

            REQUIRE( app.commandDM( src ) == 0 );

            REQUIRE( g_bmc.m_lastSet[0] == Approx( std::sqrt( 0.36 * 0.25 ) ).epsilon( 1e-6 ) );
            REQUIRE( g_bmc.m_lastSet[1] == Approx( std::sqrt( 2.56 * 0.25 ) ).epsilon( 1e-6 ) );

            // A zero command (actuator at index 2) counts as saturated at the low end
            REQUIRE( g_bmc.m_lastSet[3] == 0 );
            REQUIRE( app.m_instSatMap.data()[2] == 1 );
            REQUIRE( app.m_nsat == 1 );
        }

        SECTION( "an SDK failure returns -1 before the saturation map is updated" )
        {
            float src[4] = { 100.0f, 100.0f, 100.0f, 100.0f };

            g_bmc.m_setRet = ERR_UNKNOWN;
            REQUIRE( app.commandDM( src ) == -1 );
            REQUIRE( app.m_nsat == 0 );
            REQUIRE( app.m_instSatMap.data()[0] == 0 );
        }

        app.m_dmopen = false;
    }

    std::filesystem::remove_all( g_tmpDir + "/shm" );
}

/// Verify releaseDM() zeroes, clears, and closes the DM, and handles SDK errors.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl releaseDM", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::releaseDM();
    #endif
    // clang-format on

    resetBmcStub();

    bmcCtrl_test app( "bmcCtrl" );

    SECTION( "not open is a no-op" )
    {
        REQUIRE( app.releaseDM() == 0 );
        REQUIRE( g_bmc.m_setCalls == 0 );
        REQUIRE( g_bmc.m_closeCalls == 0 );
    }

    SECTION( "success" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );
        app.state( stateCodes::READY );

        REQUIRE( app.releaseDM() == 0 );
        REQUIRE( g_bmc.m_setCalls == 1 );
        REQUIRE( g_bmc.m_clearCalls == 1 );
        REQUIRE( g_bmc.m_highResArgs.size() == 1 );
        REQUIRE( g_bmc.m_highResArgs[0] == 0 );
        REQUIRE( g_bmc.m_closeCalls == 1 );
        REQUIRE( app.m_dmopen == false );
        REQUIRE( app.m_dm.m_id == 0 );
        REQUIRE( app.state() == stateCodes::NOTHOMED );
    }

    SECTION( "clear failure" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );
        g_bmc.m_clearRet = ERR_UNKNOWN;

        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_bmc.m_closeCalls == 0 );
        REQUIRE( app.m_dmopen == true );
    }

    SECTION( "close failure" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );
        g_bmc.m_closeRet = ERR_UNKNOWN;

        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_dmopen == true );
    }

    SECTION( "zeroing failure" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );
        g_bmc.m_setRet = ERR_UNKNOWN;

        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_bmc.m_clearCalls == 0 );
    }

    app.m_dmopen = false;
}

/// Verify appShutdown() and onPowerOff() release an open DM.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl appShutdown and onPowerOff", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::appShutdown();
    bmcCtrl::onPowerOff();
    bmcCtrl::whilePowerOff();
    #endif
    // clang-format on

    resetBmcStub();

    bmcCtrl_test app( "bmcCtrl" );

    SECTION( "appShutdown with no DM" )
    {
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_bmc.m_closeCalls == 0 );
    }

    SECTION( "appShutdown closes the DM" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_bmc.m_closeCalls == 1 );
        REQUIRE( app.m_dmopen == false );
    }

    SECTION( "onPowerOff closes the DM" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );
        app.state( stateCodes::POWEROFF );

        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_bmc.m_closeCalls == 1 );
        REQUIRE( app.m_dmopen == false );
    }

    SECTION( "whilePowerOff with no flat or test directories" )
    {
        REQUIRE( app.whilePowerOff() == 0 );
    }

    app.m_dmopen = false;
}

/// Verify the dev::dm INDI callbacks validate the property device and name.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl INDI callback validation", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::newCallBack_init(const pcf::IndiProperty &);
    bmcCtrl::newCallBack_zero(const pcf::IndiProperty &);
    bmcCtrl::newCallBack_release(const pcf::IndiProperty &);
    bmcCtrl::newCallBack_zeroAll(const pcf::IndiProperty &);
    #endif
    // clang-format on

    resetBmcStub();

    XWCTEST_INDI_ARBNEW_CALLBACK( bmcCtrl, newCallBack_init, initDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( bmcCtrl, newCallBack_zero, zeroDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( bmcCtrl, newCallBack_release, releaseDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( bmcCtrl, newCallBack_zeroAll, zeroAll );

    REQUIRE( g_bmc.m_openCalls == 0 );
    REQUIRE( g_bmc.m_setCalls == 0 );
}

/// Verify the dev::dm INDI callbacks drive the bmcCtrl DM interface.
/**
 * \ingroup bmcCtrl_unit_test
 */
TEST_CASE( "bmcCtrl INDI requests", "[bmcCtrl]" )
{
    // clang-format off
    #ifdef BMCCTRL_TEST_DOXYGEN_REF
    bmcCtrl::newCallBack_init(const pcf::IndiProperty &);
    bmcCtrl::newCallBack_zero(const pcf::IndiProperty &);
    bmcCtrl::initDM();
    bmcCtrl::zeroDM();
    #endif
    // clang-format on

    resetBmcStub();
    g_bmc.m_actCount = 6;
    g_bmc.m_setLen   = 6;

    bmcCtrl_test app( "bmcCtrl" );
    app.m_serialNumber = "ser";
    app.m_calibPath    = g_tmpDir + "/nonexistent";
    app.m_shutdown     = 1;

    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "bmcCtrl" );

    SECTION( "init request in the wrong state is rejected" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.state( stateCodes::POWERON );
        REQUIRE( app.newCallBack_init( ip ) == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_bmc.m_openCalls == 0 );
    }

    SECTION( "init request from NOTHOMED opens the DM" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.state( stateCodes::NOTHOMED );
        REQUIRE( app.newCallBack_init( ip ) == 0 );
        REQUIRE( g_bmc.m_openCalls == 1 );
        REQUIRE( app.m_dmopen == true );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "init request with an open failure sets ERROR" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        g_bmc.m_openRet = ERR_NO_HW;
        app.state( stateCodes::NOTHOMED );
        REQUIRE( app.newCallBack_init( ip ) == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "zero request zeroes an open DM" )
    {
        ip.setName( "zeroDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.fakeInit( { 0, 1, 2, 3, 4, 5 }, 1.0, 1.0 );
        REQUIRE( app.newCallBack_zero( ip ) == 0 );
        REQUIRE( g_bmc.m_setCalls == 1 );
    }

    SECTION( "zero request switched off does nothing" )
    {
        ip.setName( "zeroDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::Off ) );

        app.fakeInit( { 0, 1, 2, 3, 4, 5 }, 1.0, 1.0 );
        REQUIRE( app.newCallBack_zero( ip ) == 0 );
        REQUIRE( g_bmc.m_setCalls == 0 );
    }

    app.m_dmopen = false;
}

} // namespace bmcCtrlTest

} // namespace libXWCTest
