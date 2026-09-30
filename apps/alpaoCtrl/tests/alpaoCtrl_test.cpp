/** \file alpaoCtrl_test.cpp
 * \brief Catch2 tests for the alpaoCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup alpaoCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../alpaoCtrl.hpp"

// Included after the app header so the dev::dm callback bodies stay live.  These callbacks check the
// property key directly and return 0 for a property with no elements.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Fake state for the ALPAO SDK stubs declared in `stubs/asdkWrapper.h`.
struct asdkStubState
{
    asdkDM m_dm{ 1 }; ///< The fake DM handle returned by asdkInit().

    bool        m_initNull{ false }; ///< If true asdkInit() returns nullptr.
    UInt        m_initError{ 0 };    ///< Error code set by asdkInit().
    int         m_initCalls{ 0 };    ///< Number of asdkInit() calls.
    std::string m_initSerial;        ///< Serial number passed to the last asdkInit().

    Scalar      m_nbAct{ 0 };             ///< Value returned by asdkGet() for "NbOfActuator".
    COMPL_STAT  m_getRet{ acs::SUCCESS }; ///< Return value of asdkGet().
    int         m_getCalls{ 0 };          ///< Number of asdkGet() calls.
    std::string m_getCommand;             ///< Parameter name passed to the last asdkGet().

    COMPL_STAT          m_sendRet{ acs::SUCCESS }; ///< Return value of asdkSend().
    int                 m_sendCalls{ 0 };          ///< Number of asdkSend() calls.
    size_t              m_sendLen{ 0 };            ///< Number of values captured from each asdkSend().
    std::vector<Scalar> m_lastSend;                ///< Values passed to the last asdkSend().

    UInt m_resetError{ 0 }; ///< Error code set by asdkReset().
    int  m_resetCalls{ 0 }; ///< Number of asdkReset() calls.

    UInt m_releaseError{ 0 }; ///< Error code set by asdkRelease().
    int  m_releaseCalls{ 0 }; ///< Number of asdkRelease() calls.

    UInt m_lastError{ 0 }; ///< The current "last error" reported by asdkGetLastError().
};

/// The global ALPAO stub state.
asdkStubState g_asdk;

/// Reset the ALPAO stub state to defaults.
void resetAsdkStub()
{
    g_asdk = asdkStubState();
}

asdkDM *asdkInit( acs::CStrConst serialName )
{
    ++g_asdk.m_initCalls;
    g_asdk.m_initSerial = serialName;
    g_asdk.m_lastError  = g_asdk.m_initError;

    if( g_asdk.m_initNull )
    {
        return nullptr;
    }

    return &g_asdk.m_dm;
}

COMPL_STAT asdkRelease( asdkDM *pDm )
{
    static_cast<void>( pDm );
    ++g_asdk.m_releaseCalls;
    g_asdk.m_lastError = g_asdk.m_releaseError;
    return ( g_asdk.m_releaseError ? acs::FAILURE : acs::SUCCESS );
}

COMPL_STAT asdkSend( asdkDM *pDm, const Scalar *value )
{
    static_cast<void>( pDm );
    ++g_asdk.m_sendCalls;
    g_asdk.m_lastSend.assign( value, value + g_asdk.m_sendLen );
    return g_asdk.m_sendRet;
}

COMPL_STAT asdkReset( asdkDM *pDm )
{
    static_cast<void>( pDm );
    ++g_asdk.m_resetCalls;
    g_asdk.m_lastError = g_asdk.m_resetError;
    return ( g_asdk.m_resetError ? acs::FAILURE : acs::SUCCESS );
}

COMPL_STAT asdkGet( asdkDM *pDm, acs::CStrConst command, Scalar *value )
{
    static_cast<void>( pDm );
    ++g_asdk.m_getCalls;
    g_asdk.m_getCommand = command;
    *value              = g_asdk.m_nbAct;
    return g_asdk.m_getRet;
}

COMPL_STAT asdkGetLastError( UInt *errorNo, acs::Char *errMsg, size_t len )
{
    *errorNo = g_asdk.m_lastError;

    if( errMsg != nullptr && len > 0 )
    {
        snprintf( errMsg, len, "stub error %u", static_cast<unsigned>( g_asdk.m_lastError ) );
    }

    return acs::SUCCESS;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup alpaoCtrl_unit_test alpaoCtrl Unit Tests
 * \brief Unit tests for the alpaoCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `alpaoCtrl` unit tests.
/** \ingroup alpaoCtrl_unit_test
 */
namespace alpaoCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Scratch directory for the calibration files and shared memory used by these tests.
const std::string g_tmpDir = "/tmp/alpaoCtrl_test";

/// Test harness exposing alpaoCtrl internals.
class alpaoCtrl_test : public alpaoCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit alpaoCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        m_calibDir   = g_tmpDir + "/calib";

        createStandardIndiRequestSw( m_indiP_init, "initDM" );
        createStandardIndiRequestSw( m_indiP_zero, "zeroDM" );
        createStandardIndiRequestSw( m_indiP_release, "releaseDM" );
        createStandardIndiRequestSw( m_indiP_zeroAll, "zeroAll" );
    }

    using alpaoCtrl::m_actuator_mapping;
    using alpaoCtrl::m_calibDir;
    using alpaoCtrl::m_calibPath;
    using alpaoCtrl::m_calibRelDir;
    using alpaoCtrl::m_dm;
    using alpaoCtrl::m_dminputs;
    using alpaoCtrl::m_flatPath;
    using alpaoCtrl::m_indiP_init;
    using alpaoCtrl::m_indiP_release;
    using alpaoCtrl::m_indiP_zero;
    using alpaoCtrl::m_instSatMap;
    using alpaoCtrl::m_max_stroke;
    using alpaoCtrl::m_nbAct;
    using alpaoCtrl::m_nsat;
    using alpaoCtrl::m_outputShape;
    using alpaoCtrl::m_satThresh;
    using alpaoCtrl::m_serialNumber;
    using alpaoCtrl::m_shutdown;
    using alpaoCtrl::m_testPath;
    using alpaoCtrl::m_volume_factor;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Put the app in the state of an initialized DM without calling initDM().
    /** Allocates the command vector and actuator mapping as initDM() would.  Also sets m_shutdown so that
     * releaseDM() does not signal the (never started) shmim monitor thread.
     */
    void fakeInit( const std::vector<int> &mapping,     /**< [in] the actuator mapping */
                   Scalar                  maxStroke,   /**< [in] the maximum stroke */
                   Scalar                  volumeFactor /**< [in] the volume factor */
    )
    {
        m_dm            = &g_asdk.m_dm;
        m_nbAct         = mapping.size();
        m_max_stroke    = maxStroke;
        m_volume_factor = volumeFactor;

        if( m_dminputs )
        {
            free( m_dminputs );
        }
        m_dminputs = (Scalar *)calloc( m_nbAct, sizeof( Scalar ) );

        if( m_actuator_mapping )
        {
            free( m_actuator_mapping );
        }
        m_actuator_mapping = (int *)malloc( m_nbAct * sizeof( int ) );
        for( size_t n = 0; n < mapping.size(); ++n )
        {
            m_actuator_mapping[n] = mapping[n];
        }

        g_asdk.m_sendLen = m_nbAct;

        m_shutdown = 1;
    }
};

/// Write a two-line ALPAO user calibration file.
void writeCalibFile( const std::string &dir,         /**< [in] the calibration directory */
                     const std::string &ser,         /**< [in] the lower-case serial number */
                     double             maxStroke,   /**< [in] the maximum stroke */
                     double             volumeFactor /**< [in] the volume factor */
)
{
    std::filesystem::create_directories( dir );
    std::ofstream fout( dir + "/" + ser + "_userconfig.txt" );
    fout << maxStroke << " # max stroke\n";
    fout << volumeFactor << " # volume factor\n";
}

/// Point MILK_SHM_DIR at the test scratch directory so shared memory stays under /tmp.
void setupShmDir()
{
    std::filesystem::create_directories( g_tmpDir + "/shm" );
    setenv( "MILK_SHM_DIR", ( g_tmpDir + "/shm" ).c_str(), 1 );
}

/// \endcond

/// Verify alpaoCtrl configuration defaults.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl configuration defaults", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::setupConfig();
    alpaoCtrl::loadConfig();
    alpaoCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    resetAsdkStub();

    mx::app::writeConfigFile( "/tmp/alpaoCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );

    alpaoCtrl_test app( "alpaoCtrl" );
    app.configure( "/tmp/alpaoCtrl_test_defaults.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serialNumber == "" );
    REQUIRE( app.m_satThresh == 100 );
    REQUIRE( app.m_calibRelDir == "dm/alpao_" );
    REQUIRE( app.calibPath() == g_tmpDir + "/calib/dm/alpao_" );
    REQUIRE( app.flatPath() == g_tmpDir + "/calib/dm/alpao_/flats" );
    REQUIRE( app.testPath() == g_tmpDir + "/calib/dm/alpao_/tests" );
    REQUIRE( app.shmimName() == "" );

    // No SDK calls are made during configuration
    REQUIRE( g_asdk.m_initCalls == 0 );

    std::remove( "/tmp/alpaoCtrl_test_defaults.conf" );
}

/// Verify alpaoCtrl configuration overrides.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl configuration overrides", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::setupConfig();
    alpaoCtrl::loadConfig();
    alpaoCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    resetAsdkStub();

    SECTION( "serial number sets the lower-case calibration directory" )
    {
        mx::app::writeConfigFile( "/tmp/alpaoCtrl_test_over.conf",
                                  { "dm", "dm", "dm" },
                                  { "serialNumber", "satThresh", "shmimName" },
                                  { "BAX150", "25", "dm00disp" } );

        alpaoCtrl_test app( "alpaoCtrl" );
        app.configure( "/tmp/alpaoCtrl_test_over.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_serialNumber == "BAX150" );
        REQUIRE( app.m_satThresh == 25 );
        REQUIRE( app.m_calibRelDir == "dm/alpao_bax150" );
        REQUIRE( app.calibPath() == g_tmpDir + "/calib/dm/alpao_bax150" );
        REQUIRE( app.shmimName() == "dm00disp" );
        REQUIRE( app.shmimFlat() == "dm00disp00" );
        REQUIRE( app.shmimTest() == "dm00disp02" );
        REQUIRE( app.shmimSat() == "dm00dispST" );
        REQUIRE( app.shmimShape() == "dm00disp_shape" );
    }

    SECTION( "an explicit calibPath overrides the serial-number directory" )
    {
        mx::app::writeConfigFile( "/tmp/alpaoCtrl_test_over.conf",
                                  { "dm", "dm" },
                                  { "serialNumber", "calibPath" },
                                  { "BAX150", "/tmp/alpaoCtrl_test_other" } );

        alpaoCtrl_test app( "alpaoCtrl" );
        app.configure( "/tmp/alpaoCtrl_test_over.conf" );

        REQUIRE( app.m_calibRelDir == "dm/alpao_bax150" );
        REQUIRE( app.calibPath() == "/tmp/alpaoCtrl_test_other" );
        REQUIRE( app.flatPath() == "/tmp/alpaoCtrl_test_other/flats" );
    }

    std::remove( "/tmp/alpaoCtrl_test_over.conf" );
}

/// Verify parsing of the ALPAO user calibration file.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl parse_calibration_file", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::parse_calibration_file();
    #endif
    // clang-format on

    resetAsdkStub();

    const std::string dir = g_tmpDir + "/parse";

    alpaoCtrl_test app( "alpaoCtrl" );
    app.m_serialNumber = "BAX999";
    app.m_calibPath    = dir;

    SECTION( "a valid file sets the stroke and volume factor" )
    {
        writeCalibFile( dir, "bax999", 3.5, 1.25 );

        REQUIRE( app.parse_calibration_file() == 0 );
        REQUIRE( app.m_max_stroke == Approx( 3.5 ) );
        REQUIRE( app.m_volume_factor == Approx( 1.25 ) );
    }

    SECTION( "the file name uses the lower-case serial number" )
    {
        std::filesystem::remove_all( dir );
        writeCalibFile( dir, "BAX999", 3.5, 1.25 ); // upper-case name is not found

        REQUIRE( app.parse_calibration_file() == -1 );
        REQUIRE( app.m_max_stroke == 0 );
        REQUIRE( app.m_volume_factor == 0 );
    }

    SECTION( "a missing file is an error" )
    {
        std::filesystem::remove_all( dir );

        REQUIRE( app.parse_calibration_file() == -1 );
        REQUIRE( app.m_max_stroke == 0 );
        REQUIRE( app.m_volume_factor == 0 );
    }

    std::filesystem::remove_all( dir );
}

/// Verify appStartup() fails when the calibration is missing or invalid.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl appStartup calibration checks", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::appStartup();
    #endif
    // clang-format on

    resetAsdkStub();

    const std::string dir = g_tmpDir + "/startup";

    alpaoCtrl_test app( "alpaoCtrl" );
    app.m_serialNumber = "BAX1";
    app.m_calibPath    = dir;

    SECTION( "missing calibration file" )
    {
        std::filesystem::remove_all( dir );
        REQUIRE( app.appStartup() == -1 );
    }

    SECTION( "zero max stroke" )
    {
        writeCalibFile( dir, "bax1", 0.0, 1.0 );
        REQUIRE( app.appStartup() == -1 );
    }

    SECTION( "zero volume factor" )
    {
        writeCalibFile( dir, "bax1", 2.0, 0.0 );
        REQUIRE( app.appStartup() == -1 );
    }

    // No DM access happens on these failures
    REQUIRE( g_asdk.m_initCalls == 0 );

    std::filesystem::remove_all( dir );
}

/// Verify reading the actuator mapping from a FITS file.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl get_actuator_mapping", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::get_actuator_mapping();
    #endif
    // clang-format on

    resetAsdkStub();

    const std::string dir = g_tmpDir + "/mapping";
    std::filesystem::create_directories( dir );

    alpaoCtrl_test app( "alpaoCtrl" );
    app.m_serialNumber = "BAX2";
    app.m_calibPath    = dir;
    app.fakeInit( { -1, -1, -1, -1 }, 1.0, 1.0 );

    SECTION( "active pixels are mapped to linear indices, last FITS row first" )
    {
        // 3x3 map with 4 active actuators at column-major linear indices 0, 2, 4, and 6
        mx::improc::eigenImage<int> im( 3, 3 );
        im.setZero();
        im( 0, 0 ) = 1;
        im( 2, 0 ) = 1;
        im( 1, 1 ) = 1;
        im( 0, 2 ) = 1;

        mx::fits::fitsFile<int> ff;
        bool written = ( ff.write( dir + "/bax2_actuator_mapping.fits", im ) == mx::error_t::noerror );
        REQUIRE( written );

        REQUIRE( app.get_actuator_mapping() == 0 );

        REQUIRE( app.m_actuator_mapping[0] == 6 );
        REQUIRE( app.m_actuator_mapping[1] == 4 );
        REQUIRE( app.m_actuator_mapping[2] == 0 );
        REQUIRE( app.m_actuator_mapping[3] == 2 );
    }

    SECTION( "a missing file leaves the mapping unchanged (and still returns 0)" )
    {
        std::filesystem::remove( dir + "/bax2_actuator_mapping.fits" );

        REQUIRE( app.get_actuator_mapping() == 0 );

        for( int n = 0; n < 4; ++n )
        {
            REQUIRE( app.m_actuator_mapping[n] == -1 );
        }
    }

    app.m_dm = nullptr;
    std::filesystem::remove_all( dir );
}

/// Verify initDM() drives the SDK correctly on success.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl initDM success", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::initDM();
    #endif
    // clang-format on

    resetAsdkStub();
    g_asdk.m_nbAct   = 97;
    g_asdk.m_sendLen = 97;

    alpaoCtrl_test app( "alpaoCtrl" );
    app.m_serialNumber = "bax150";
    app.m_calibPath    = g_tmpDir + "/nonexistent";
    app.m_shutdown     = 1;

    REQUIRE( app.initDM() == 0 );

    // The serial number is passed upper-case
    REQUIRE( g_asdk.m_initCalls == 1 );
    REQUIRE( g_asdk.m_initSerial == "BAX150" );

    // The number of actuators is queried
    REQUIRE( g_asdk.m_getCalls == 1 );
    REQUIRE( g_asdk.m_getCommand == "NbOfActuator" );
    REQUIRE( app.m_nbAct == 97 );

    // Buffers are allocated
    REQUIRE( app.m_dm == &g_asdk.m_dm );
    REQUIRE( app.m_dminputs != nullptr );
    REQUIRE( app.m_actuator_mapping != nullptr );

    // The DM is zeroed
    REQUIRE( g_asdk.m_sendCalls == 1 );
    REQUIRE( g_asdk.m_lastSend.size() == 97 );
    for( size_t n = 0; n < g_asdk.m_lastSend.size(); ++n )
    {
        REQUIRE( g_asdk.m_lastSend[n] == 0 );
    }

    REQUIRE( app.state() == stateCodes::READY );

    SECTION( "a second initDM is a no-op" )
    {
        REQUIRE( app.initDM() == 0 );
        REQUIRE( g_asdk.m_initCalls == 1 );
        REQUIRE( g_asdk.m_sendCalls == 1 );
    }

    app.m_dm = nullptr; // avoid any release attempts
}

/// Verify initDM() error handling.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl initDM failures", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::initDM();
    #endif
    // clang-format on

    resetAsdkStub();
    g_asdk.m_nbAct   = 10;
    g_asdk.m_sendLen = 10;

    alpaoCtrl_test app( "alpaoCtrl" );
    app.m_serialNumber = "BAX3";
    app.m_calibPath    = g_tmpDir + "/nonexistent";
    app.m_shutdown     = 1;

    SECTION( "SDK error after init" )
    {
        g_asdk.m_initError = 7;
        REQUIRE( app.initDM() == -1 );
        REQUIRE( app.m_dm == nullptr );
        REQUIRE( g_asdk.m_getCalls == 0 );
    }

    SECTION( "null handle without an SDK error" )
    {
        g_asdk.m_initNull = true;
        REQUIRE( app.initDM() == -1 );
        REQUIRE( app.m_dm == nullptr );
        REQUIRE( g_asdk.m_getCalls == 0 );
    }

    SECTION( "failure getting the number of actuators" )
    {
        g_asdk.m_getRet = acs::FAILURE;
        REQUIRE( app.initDM() == -1 );
        REQUIRE( g_asdk.m_sendCalls == 0 );
    }

    SECTION( "zero actuators reported makes zeroing fail" )
    {
        g_asdk.m_nbAct = 0;
        REQUIRE( app.initDM() == -1 );
        REQUIRE( g_asdk.m_sendCalls == 0 );
    }

    SECTION( "failure zeroing the DM" )
    {
        g_asdk.m_sendRet = acs::FAILURE;
        REQUIRE( app.initDM() == -1 );
        REQUIRE( g_asdk.m_sendCalls == 1 );
    }

    REQUIRE( app.state() != stateCodes::READY );

    app.m_dm = nullptr;
}

/// Verify zeroDM().
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl zeroDM", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::zeroDM();
    #endif
    // clang-format on

    resetAsdkStub();

    alpaoCtrl_test app( "alpaoCtrl" );

    SECTION( "not initialized" )
    {
        REQUIRE( app.zeroDM() == -1 );
        REQUIRE( g_asdk.m_sendCalls == 0 );
    }

    SECTION( "no actuators" )
    {
        app.m_dm = &g_asdk.m_dm;
        REQUIRE( app.zeroDM() == -1 );
        REQUIRE( g_asdk.m_sendCalls == 0 );
    }

    SECTION( "success sends all zeros" )
    {
        app.fakeInit( { 0, 1, 2, 3, 4 }, 1.0, 1.0 );
        app.m_dminputs[2] = 0.5; // the pre-allocated command vector is not what is sent

        REQUIRE( app.zeroDM() == 0 );
        REQUIRE( g_asdk.m_sendCalls == 1 );
        REQUIRE( g_asdk.m_lastSend.size() == 5 );
        for( size_t n = 0; n < 5; ++n )
        {
            REQUIRE( g_asdk.m_lastSend[n] == 0 );
        }
    }

    SECTION( "SDK send failure" )
    {
        app.fakeInit( { 0, 1, 2 }, 1.0, 1.0 );
        g_asdk.m_sendRet = acs::FAILURE;

        REQUIRE( app.zeroDM() == -1 );
        REQUIRE( g_asdk.m_sendCalls == 1 );
    }

    app.m_dm = nullptr;
}

/// Verify commandDM() scaling, mean removal, mapping, clipping, and saturation bookkeeping.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl commandDM", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::commandDM(void *);
    #endif
    // clang-format on

    resetAsdkStub();
    setupShmDir();

    {
        alpaoCtrl_test app( "alpaoCtrl" );

        // 2x2 DM grid, 3 actuators at linear indices 3, 0, and 2.  Gain scale = 2/4 = 0.5.
        app.fakeInit( { 3, 0, 2 }, 4.0, 2.0 );

        app.m_outputShape.create( "alpaoCtrlTest_shape", 2, 2 );
        app.m_outputShape().setZero();
        app.m_instSatMap.resize( 2, 2 );
        app.m_instSatMap.setZero();

        SECTION( "in-range command" )
        {
            // inputs: 3->1.5, 0->0.5, 2->-0.5; mean 0.5; after mean removal: 1, 0, -1
            float src[4] = { 1.0f, 9.0f, -1.0f, 3.0f };

            REQUIRE( app.commandDM( src ) == 0 );

            REQUIRE( g_asdk.m_sendCalls == 1 );
            REQUIRE( g_asdk.m_lastSend.size() == 3 );
            REQUIRE( g_asdk.m_lastSend[0] == Approx( 1.0 ) );
            REQUIRE( g_asdk.m_lastSend[1] == Approx( 0.0 ).margin( 1e-12 ) );
            REQUIRE( g_asdk.m_lastSend[2] == Approx( -1.0 ) );

            // exactly +/-1 is not counted as saturated
            REQUIRE( app.m_nsat == 0 );

            // output shape is in physical units, mapped back to the grid
            REQUIRE( app.m_outputShape[3] == Approx( 2.0 ) );
            REQUIRE( app.m_outputShape[0] == Approx( 0.0 ).margin( 1e-6 ) );
            REQUIRE( app.m_outputShape[2] == Approx( -2.0 ) );
            REQUIRE( app.m_outputShape[1] == 0 ); // not an actuator

            // but +/-1 is flagged in the instantaneous saturation map
            REQUIRE( app.m_instSatMap.data()[3] == 1 );
            REQUIRE( app.m_instSatMap.data()[0] == 0 );
            REQUIRE( app.m_instSatMap.data()[2] == 1 );
            REQUIRE( app.m_instSatMap.data()[1] == 0 );
        }

        SECTION( "saturating command is clipped" )
        {
            // inputs: 3->4, 0->2, 2->-3; mean 1; after mean removal: 3, 1, -4 -> clipped 1, 1, -1
            float src[4] = { 4.0f, 0.0f, -6.0f, 8.0f };

            REQUIRE( app.commandDM( src ) == 0 );

            REQUIRE( g_asdk.m_lastSend[0] == Approx( 1.0 ) );
            REQUIRE( g_asdk.m_lastSend[1] == Approx( 1.0 ) );
            REQUIRE( g_asdk.m_lastSend[2] == Approx( -1.0 ) );

            REQUIRE( app.m_nsat == 2 );

            REQUIRE( app.m_outputShape[3] == Approx( 2.0 ) );
            REQUIRE( app.m_outputShape[0] == Approx( 2.0 ) );
            REQUIRE( app.m_outputShape[2] == Approx( -2.0 ) );

            REQUIRE( app.m_instSatMap.data()[3] == 1 );
            REQUIRE( app.m_instSatMap.data()[0] == 1 );
            REQUIRE( app.m_instSatMap.data()[2] == 1 );

            // saturation counts accumulate across commands
            REQUIRE( app.commandDM( src ) == 0 );
            REQUIRE( app.m_nsat == 4 );
        }

        SECTION( "a piston-only command is removed" )
        {
            float src[4] = { 0.7f, 0.7f, 0.7f, 0.7f };

            REQUIRE( app.commandDM( src ) == 0 );
            for( size_t n = 0; n < 3; ++n )
            {
                REQUIRE( g_asdk.m_lastSend[n] == Approx( 0.0 ).margin( 1e-7 ) );
            }
            REQUIRE( app.m_nsat == 0 );
        }

        SECTION( "an SDK send failure is returned" )
        {
            float src[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

            g_asdk.m_sendRet = acs::FAILURE;
            REQUIRE( app.commandDM( src ) == acs::FAILURE );
            REQUIRE( g_asdk.m_sendCalls == 1 );
        }

        app.m_dm = nullptr;
    }

    std::filesystem::remove_all( g_tmpDir + "/shm" );
}

/// Verify releaseDM() resets and releases the DM, and handles SDK errors.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl releaseDM", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::releaseDM();
    #endif
    // clang-format on

    resetAsdkStub();

    alpaoCtrl_test app( "alpaoCtrl" );

    SECTION( "not initialized is a no-op" )
    {
        REQUIRE( app.releaseDM() == 0 );
        REQUIRE( g_asdk.m_sendCalls == 0 );
        REQUIRE( g_asdk.m_resetCalls == 0 );
        REQUIRE( g_asdk.m_releaseCalls == 0 );
    }

    SECTION( "success zeroes, resets, and releases" )
    {
        app.fakeInit( { 0, 1, 2 }, 1.0, 1.0 );

        REQUIRE( app.releaseDM() == 0 );
        REQUIRE( g_asdk.m_sendCalls == 1 );
        REQUIRE( g_asdk.m_lastSend.size() == 3 );
        REQUIRE( g_asdk.m_resetCalls == 1 );
        REQUIRE( g_asdk.m_releaseCalls == 1 );
        REQUIRE( app.m_dm == nullptr );
    }

    SECTION( "reset error leaves the DM handle" )
    {
        app.fakeInit( { 0, 1, 2 }, 1.0, 1.0 );
        g_asdk.m_resetError = 3;

        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( g_asdk.m_resetCalls == 1 );
        REQUIRE( g_asdk.m_releaseCalls == 0 );
        REQUIRE( app.m_dm != nullptr );
    }

    SECTION( "release error leaves the DM handle" )
    {
        app.fakeInit( { 0, 1, 2 }, 1.0, 1.0 );
        g_asdk.m_releaseError = 4;

        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( g_asdk.m_resetCalls == 1 );
        REQUIRE( g_asdk.m_releaseCalls == 1 );
        REQUIRE( app.m_dm != nullptr );
    }

    SECTION( "zeroing failure aborts the release" )
    {
        app.fakeInit( { 0, 1, 2 }, 1.0, 1.0 );
        g_asdk.m_sendRet = acs::FAILURE;

        REQUIRE( app.releaseDM() == -1 );
        REQUIRE( g_asdk.m_resetCalls == 0 );
        REQUIRE( g_asdk.m_releaseCalls == 0 );
    }

    app.m_dm = nullptr;
}

/// Verify appShutdown() and onPowerOff() release an initialized DM.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl appShutdown and onPowerOff", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::appShutdown();
    alpaoCtrl::onPowerOff();
    alpaoCtrl::whilePowerOff();
    #endif
    // clang-format on

    resetAsdkStub();

    alpaoCtrl_test app( "alpaoCtrl" );

    SECTION( "appShutdown with no DM" )
    {
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_asdk.m_releaseCalls == 0 );
    }

    SECTION( "appShutdown releases the DM" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_asdk.m_releaseCalls == 1 );
        REQUIRE( app.m_dm == nullptr );
    }

    SECTION( "onPowerOff releases the DM and sets NOTHOMED" )
    {
        app.fakeInit( { 0, 1 }, 1.0, 1.0 );
        app.state( stateCodes::READY );

        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_asdk.m_releaseCalls == 1 );
        REQUIRE( app.m_dm == nullptr );
        REQUIRE( app.state() == stateCodes::NOTHOMED );
    }

    SECTION( "whilePowerOff with no flat or test directories" )
    {
        REQUIRE( app.whilePowerOff() == 0 );
    }

    app.m_dm = nullptr;
}

/// Verify the dev::dm INDI callbacks validate the property device and name.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl INDI callback validation", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::newCallBack_init(const pcf::IndiProperty &);
    alpaoCtrl::newCallBack_zero(const pcf::IndiProperty &);
    alpaoCtrl::newCallBack_release(const pcf::IndiProperty &);
    alpaoCtrl::newCallBack_zeroAll(const pcf::IndiProperty &);
    #endif
    // clang-format on

    resetAsdkStub();

    XWCTEST_INDI_ARBNEW_CALLBACK( alpaoCtrl, newCallBack_init, initDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( alpaoCtrl, newCallBack_zero, zeroDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( alpaoCtrl, newCallBack_release, releaseDM );
    XWCTEST_INDI_ARBNEW_CALLBACK( alpaoCtrl, newCallBack_zeroAll, zeroAll );

    // None of the empty requests reach the SDK
    REQUIRE( g_asdk.m_initCalls == 0 );
    REQUIRE( g_asdk.m_sendCalls == 0 );
}

/// Verify the dev::dm INDI callbacks drive the alpaoCtrl DM interface.
/**
 * \ingroup alpaoCtrl_unit_test
 */
TEST_CASE( "alpaoCtrl INDI requests", "[alpaoCtrl]" )
{
    // clang-format off
    #ifdef ALPAOCTRL_TEST_DOXYGEN_REF
    alpaoCtrl::newCallBack_init(const pcf::IndiProperty &);
    alpaoCtrl::newCallBack_zero(const pcf::IndiProperty &);
    alpaoCtrl::initDM();
    alpaoCtrl::zeroDM();
    #endif
    // clang-format on

    resetAsdkStub();
    g_asdk.m_nbAct   = 12;
    g_asdk.m_sendLen = 12;

    alpaoCtrl_test app( "alpaoCtrl" );
    app.m_serialNumber = "BAX4";
    app.m_calibPath    = g_tmpDir + "/nonexistent";
    app.m_shutdown     = 1;

    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "alpaoCtrl" );

    SECTION( "init request in the wrong state is rejected" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.state( stateCodes::READY );
        REQUIRE( app.newCallBack_init( ip ) == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_asdk.m_initCalls == 0 );
    }

    SECTION( "init request from NOTHOMED initializes the DM" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.state( stateCodes::NOTHOMED );
        REQUIRE( app.newCallBack_init( ip ) == 0 );
        REQUIRE( g_asdk.m_initCalls == 1 );
        REQUIRE( app.m_nbAct == 12 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "init request with an SDK failure sets ERROR" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        g_asdk.m_initError = 9;
        app.state( stateCodes::NOTHOMED );
        REQUIRE( app.newCallBack_init( ip ) == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "init request switched off does nothing" )
    {
        ip.setName( "initDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::Off ) );

        app.state( stateCodes::NOTHOMED );
        REQUIRE( app.newCallBack_init( ip ) == 0 );
        REQUIRE( g_asdk.m_initCalls == 0 );
    }

    SECTION( "zero request zeroes an initialized DM" )
    {
        ip.setName( "zeroDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        app.fakeInit( { 0, 1, 2, 3 }, 1.0, 1.0 );
        REQUIRE( app.newCallBack_zero( ip ) == 0 );
        REQUIRE( g_asdk.m_sendCalls == 1 );
        REQUIRE( g_asdk.m_lastSend.size() == 4 );
    }

    SECTION( "zero request on an uninitialized DM fails" )
    {
        ip.setName( "zeroDM" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_zero( ip ) == -1 );
        REQUIRE( g_asdk.m_sendCalls == 0 );
    }

    app.m_dm = nullptr;
}

} // namespace alpaoCtrlTest

} // namespace libXWCTest
