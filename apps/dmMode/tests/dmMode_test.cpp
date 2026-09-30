/** \file dmMode_test.cpp
 * \brief Catch2 tests for the dmMode app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup dmMode_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <mx/improc/milkImage.hpp>

#include "../dmMode.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup dmMode_unit_test dmMode Unit Tests
 * \brief Unit tests for the dmMode application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `dmMode` unit tests.
/** \ingroup dmMode_unit_test
 */
namespace dmModeTest
{

/// Directory used as `MILK_SHM_DIR` for the shared-memory DM channel tests.
constexpr const char *c_shmDir = "/tmp/dmMode_test_shm";

/// Number of modes used by every test that touches `recordDmModes()`.
/** `dmMode::recordDmModes()` sizes a function-local static vector on its first call, so all calls in this binary
 * must use the same number of modes.
 */
constexpr int c_nModes = 3;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing dmMode internals.
class dmMode_test : public dmMode
{
  public:
    using dmMode::m_amps;
    using dmMode::m_dataType;
    using dmMode::m_dmChannelName;
    using dmMode::m_dmName;
    using dmMode::m_elNames;
    using dmMode::m_height;
    using dmMode::m_indiP_currAmps;
    using dmMode::m_indiP_dm;
    using dmMode::m_indiP_tgtAmps;
    using dmMode::m_maxModes;
    using dmMode::m_modeCube;
    using dmMode::m_modes;
    using dmMode::m_opened;
    using dmMode::m_shape;
    using dmMode::m_shutdown;
    using dmMode::m_typeSize;
    using dmMode::m_width;

    /// Placeholder property used to force a registration collision in `appStartup()`.
    pcf::IndiProperty m_blocker;

    /// Construct a harness with the given device name.
    explicit dmMode_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        m_indiP_currAmps.setDevice( device );
        m_indiP_currAmps.setName( "current_amps" );

        m_indiP_tgtAmps.setDevice( device );
        m_indiP_tgtAmps.setName( "target_amps" );
    }

    /// Setup the configurator and load the given config file.
    void loadConfigFile( const std::string &file /**< [in] config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
    }

    /// Fill the mode cube, amplitudes, shape and element names as `appStartup()` would.
    /** Plane `p` has pixel values `100*p + 10*r + c`.
     */
    void setupModes( int rows, /**< [in] rows of each mode */
                     int cols, /**< [in] columns of each mode */
                     int planes /**< [in] number of modes */ )
    {
        m_modes.resize( rows, cols, planes );
        for( int p = 0; p < planes; ++p )
        {
            for( int r = 0; r < rows; ++r )
            {
                for( int c = 0; c < cols; ++c )
                {
                    m_modes.image( p )( r, c ) = 100.0f * p + 10.0f * r + c;
                }
            }
        }

        m_amps.assign( planes, 0.0f );
        m_shape.resize( rows, cols );
        m_shape.setZero();

        m_elNames.resize( planes );
        for( int n = 0; n < planes; ++n )
        {
            m_elNames[n] = mx::ioutils::convertToString<size_t, 4, '0'>( n );
        }
    }

    /// Register a placeholder `target_amps` property so the app's own registration of it fails.
    int blockTargetAmps()
    {
        return registerIndiPropertyNew( m_blocker,
                                        "target_amps",
                                        pcf::IndiProperty::Number,
                                        pcf::IndiProperty::ReadWrite,
                                        pcf::IndiProperty::Idle,
                                        nullptr );
    }

    /// Close the DM channel image opened by `appLogic()`.
    void closeStream()
    {
        ImageStreamIO_closeIm( &m_imageStream );
    }
};
/// \endcond

/// Write a synthetic mode cube to a FITS file.
/** Plane `p` has pixel values `100*p + 10*r + c`.
 *
 * \returns true if the file was written
 */
bool writeModeCube( const std::string &file, /**< [in] FITS file path */
                    int                rows, /**< [in] rows of each mode */
                    int                cols, /**< [in] columns of each mode */
                    int                planes /**< [in] number of modes */ )
{
    mx::improc::eigenCube<float> cube( rows, cols, planes );
    for( int p = 0; p < planes; ++p )
    {
        for( int r = 0; r < rows; ++r )
        {
            for( int c = 0; c < cols; ++c )
            {
                cube.image( p )( r, c ) = 100.0f * p + 10.0f * r + c;
            }
        }
    }

    std::filesystem::remove( file );

    mx::fits::fitsFile<float> ff;
    return ( ff.write( file, cube ) == mx::error_t::noerror );
}

/// Point ImageStreamIO at a private shared-memory directory.
void useTestShmDir()
{
    std::filesystem::create_directories( c_shmDir );
    setenv( "MILK_SHM_DIR", c_shmDir, 1 );
}

/// Build a numeric INDI property with the given elements.
/** \returns the new property
 */
pcf::IndiProperty makeAmpsProperty( const std::string              &device, /**< [in] device name */
                                    const std::string              &name,   /**< [in] property name */
                                    const std::vector<std::string> &els,    /**< [in] element names */
                                    const std::vector<float>       &vals /**< [in] element values */ )
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

/// Verify the default dmMode configuration.
/**
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode configuration defaults", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::setupConfig();
    dmMode::loadConfig();
    dmMode::loadConfigImpl(config);
    #endif
    // clang-format on

    dmMode_test app( "dmmode" );

    mx::app::writeConfigFile( "/tmp/dmMode_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.loadConfigFile( "/tmp/dmMode_test_defaults.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_modeCube == "" );
    REQUIRE( app.m_maxModes == 50 );
    REQUIRE( app.m_dmChannelName == "" );
    REQUIRE( app.m_dmName == "" );
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );

    std::filesystem::remove( "/tmp/dmMode_test_defaults.conf" );
}

/// Verify dmMode configuration overrides, and that the DM name defaults to the channel name.
/**
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode configuration overrides", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::setupConfig();
    dmMode::loadConfig();
    dmMode::loadConfigImpl(config);
    #endif
    // clang-format on

    SECTION( "all keys set" )
    {
        dmMode_test app( "dmmode" );

        mx::app::writeConfigFile( "/tmp/dmMode_test_overrides.conf",
                                  { "dm", "dm", "dm", "dm", "telemeter" },
                                  { "modeCube", "maxModes", "name", "channelName", "maxInterval" },
                                  { "/tmp/modes.fits", "7", "woofer", "dm00disp04", "2.5" } );
        app.loadConfigFile( "/tmp/dmMode_test_overrides.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_modeCube == "/tmp/modes.fits" );
        REQUIRE( app.m_maxModes == 7 );
        REQUIRE( app.m_dmChannelName == "dm00disp04" );
        REQUIRE( app.m_dmName == "woofer" );
        REQUIRE( app.m_maxInterval == Approx( 2.5 ) );

        std::filesystem::remove( "/tmp/dmMode_test_overrides.conf" );
    }

    SECTION( "name defaults to channel name" )
    {
        dmMode_test app( "dmmode" );

        mx::app::writeConfigFile(
            "/tmp/dmMode_test_name.conf", { "dm", "dm" }, { "channelName", "maxModes" }, { "dm01disp03", "-1" } );
        app.loadConfigFile( "/tmp/dmMode_test_name.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_dmChannelName == "dm01disp03" );
        REQUIRE( app.m_dmName == "dm01disp03" );
        REQUIRE( app.m_maxModes == -1 );

        std::filesystem::remove( "/tmp/dmMode_test_name.conf" );
    }
}

/// Verify appStartup fails when the mode cube cannot be read.
/**
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode appStartup fails on a missing mode cube", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::appStartup();
    #endif
    // clang-format on

    dmMode_test app( "dmmode" );

    app.m_modeCube = "/tmp/dmMode_test_does_not_exist.fits";
    std::filesystem::remove( app.m_modeCube );

    REQUIRE( app.appStartup() == -1 );
    REQUIRE( app.m_amps.size() == 0 );
    REQUIRE( app.state() != stateCodes::NOTCONNECTED );
}

/// Verify appStartup loads the mode cube, applies maxModes, and sets up the INDI properties.
/**
 * The `target_amps` registration is forced to fail (by pre-registering a property with the same key) so that
 * appStartup stops before it starts the telemetry thread.
 *
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode appStartup loads and truncates the mode cube", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::appStartup();
    #endif
    // clang-format on

    const std::string cubeFile = "/tmp/dmMode_test_cube.fits";
    REQUIRE( writeModeCube( cubeFile, 4, 3, 5 ) );

    SECTION( "maxModes smaller than the cube truncates" )
    {
        dmMode_test app( "dmmode" );
        app.m_modeCube      = cubeFile;
        app.m_maxModes      = 2;
        app.m_dmName        = "woofer";
        app.m_dmChannelName = "dm00disp04";

        REQUIRE( app.blockTargetAmps() == 0 );
        REQUIRE( app.appStartup() == -1 );

        REQUIRE( app.m_modes.rows() == 4 );
        REQUIRE( app.m_modes.cols() == 3 );
        REQUIRE( app.m_modes.planes() == 2 );
        for( int p = 0; p < 2; ++p )
        {
            REQUIRE( app.m_modes.image( p )( 0, 0 ) == Approx( 100.0 * p ) );
            REQUIRE( app.m_modes.image( p )( 3, 2 ) == Approx( 100.0 * p + 32.0 ) );
        }

        REQUIRE( app.m_amps.size() == 2 );
        REQUIRE( app.m_amps[0] == 0 );
        REQUIRE( app.m_amps[1] == 0 );
        REQUIRE( app.m_shape.rows() == 4 );
        REQUIRE( app.m_shape.cols() == 3 );

        REQUIRE( app.m_indiP_dm.getName() == "dm" );
        REQUIRE( app.m_indiP_dm.getDevice() == "dmmode" );
        REQUIRE( app.m_indiP_dm["name"].get() == "woofer" );
        REQUIRE( app.m_indiP_dm["channel"].get() == "dm00disp04" );
        REQUIRE( app.m_indiP_currAmps.getName() == "current_amps" );
    }

    SECTION( "maxModes <= 0 keeps all modes" )
    {
        dmMode_test app( "dmmode" );
        app.m_modeCube = cubeFile;
        app.m_maxModes = 0;

        REQUIRE( app.blockTargetAmps() == 0 );
        REQUIRE( app.appStartup() == -1 );

        REQUIRE( app.m_modes.planes() == 5 );
        REQUIRE( app.m_amps.size() == 5 );
        REQUIRE( app.m_modes.image( 4 )( 1, 1 ) == Approx( 411.0 ) );
    }

    SECTION( "maxModes larger than the cube keeps all modes" )
    {
        dmMode_test app( "dmmode" );
        app.m_modeCube = cubeFile;
        app.m_maxModes = 50;

        REQUIRE( app.blockTargetAmps() == 0 );
        REQUIRE( app.appStartup() == -1 );

        REQUIRE( app.m_modes.planes() == 5 );
        REQUIRE( app.m_amps.size() == 5 );
    }

    std::filesystem::remove( cubeFile );
}

/// Verify appLogic stays NOTCONNECTED while the DM channel does not exist.
/**
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode appLogic waits for the DM channel", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::appLogic();
    #endif
    // clang-format on

    useTestShmDir();

    dmMode_test app( "dmmode" );
    app.setupModes( 4, 3, c_nModes );
    app.m_dmChannelName = "dmMode_test_missing";
    app.state( stateCodes::NOTCONNECTED );

    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    REQUIRE( app.m_opened == false );

    std::filesystem::remove_all( c_shmDir );
}

/// Verify appLogic rejects DM channels that do not match the mode cube.
/**
 * Each case opens a real shared-memory channel under a private `MILK_SHM_DIR`, so the state goes to CONNECTED and
 * the checks in the CONNECTED branch return -1 before any command is sent.
 *
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode appLogic validates the DM channel", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::appLogic();
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "wrong data type" )
    {
        mx::improc::milkImage<double> chan;
        chan.create( "dmMode_test_dbl", 4, 3 );

        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_dmChannelName = "dmMode_test_dbl";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == -1 );
        REQUIRE( app.m_opened == true );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_dataType == _DATATYPE_DOUBLE );
        REQUIRE( app.m_typeSize == sizeof( double ) );

        app.closeStream();
    }

    SECTION( "wrong number of rows" )
    {
        mx::improc::milkImage<float> chan;
        chan.create( "dmMode_test_rows", 5, 3 );

        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_dmChannelName = "dmMode_test_rows";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == -1 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_width == 5 );
        REQUIRE( app.m_height == 3 );

        app.closeStream();
    }

    SECTION( "wrong number of columns" )
    {
        mx::improc::milkImage<float> chan;
        chan.create( "dmMode_test_cols", 4, 6 );

        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_dmChannelName = "dmMode_test_cols";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == -1 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_width == 4 );
        REQUIRE( app.m_height == 6 );

        app.closeStream();
    }

    SECTION( "too few semaphores" )
    {
        IMAGE    img{};
        uint32_t imsize[3] = { 4, 3, 1 };
        REQUIRE( ImageStreamIO_createIm_gpu( &img,
                                             "dmMode_test_nosem",
                                             3,
                                             imsize,
                                             _DATATYPE_FLOAT,
                                             -1,
                                             1,
                                             2,
                                             0,
                                             CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                             0 ) == IMAGESTREAMIO_SUCCESS );

        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_dmChannelName = "dmMode_test_nosem";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_opened == false );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );

        ImageStreamIO_destroyIm( &img );
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify appLogic in READY fails when the telemetry thread is not running.
/**
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode appLogic READY without telemetry thread", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::appLogic();
    dmMode::appShutdown();
    #endif
    // clang-format on

    dmMode_test app( "dmmode" );
    app.setupModes( 4, 3, c_nModes );
    app.state( stateCodes::READY );

    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.state() == stateCodes::FAILURE );
    REQUIRE( app.m_shutdown == 1 );

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify sendCommand does nothing when the DM channel is not open.
/**
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode sendCommand requires an open channel", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::sendCommand();
    #endif
    // clang-format on

    dmMode_test app( "dmmode" );
    app.setupModes( 4, 3, c_nModes );
    app.m_opened  = false;
    app.m_amps[1] = 2.0f;

    REQUIRE( app.sendCommand() == 0 );
    REQUIRE( app.m_shape.abs().sum() == Approx( 0.0 ) );
}

/// Verify the current_amps and target_amps INDI callbacks.
/**
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode amplitude INDI callbacks", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::newCallBack_m_indiP_currAmps(ip);
    dmMode::newCallBack_m_indiP_tgtAmps(ip);
    #endif
    // clang-format on

    SECTION( "current_amps wrong name is rejected" )
    {
        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_opened = false;

        pcf::IndiProperty ip = makeAmpsProperty( "dmmode", "wrong", { "0001" }, { 1.5f } );
        REQUIRE( app.newCallBack_m_indiP_currAmps( ip ) == -1 );
        REQUIRE( app.m_amps[1] == 0 );
    }

    SECTION( "target_amps wrong name is rejected" )
    {
        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_opened = false;

        pcf::IndiProperty ip = makeAmpsProperty( "dmmode", "current_amps", { "0001" }, { 1.5f } );
        REQUIRE( app.newCallBack_m_indiP_tgtAmps( ip ) == -1 );
        REQUIRE( app.m_amps[1] == 0 );
    }

    SECTION( "no matching elements leaves amplitudes unchanged" )
    {
        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_opened = false;

        pcf::IndiProperty ip = makeAmpsProperty( "dmmode", "target_amps", { "9999" }, { 1.5f } );
        REQUIRE( app.newCallBack_m_indiP_tgtAmps( ip ) == 0 );
        for( int n = 0; n < c_nModes; ++n )
        {
            REQUIRE( app.m_amps[n] == 0 );
        }
    }

    SECTION( "target_amps sets only the named modes" )
    {
        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_opened  = false;
        app.m_amps[0] = 0.25f;

        pcf::IndiProperty ip = makeAmpsProperty( "dmmode", "target_amps", { "0001", "0002" }, { 1.5f, -0.75f } );
        REQUIRE( app.newCallBack_m_indiP_tgtAmps( ip ) == 0 );
        REQUIRE( app.m_amps[0] == Approx( 0.25 ) );
        REQUIRE( app.m_amps[1] == Approx( 1.5 ) );
        REQUIRE( app.m_amps[2] == Approx( -0.75 ) );
    }

    SECTION( "current_amps sets the named modes" )
    {
        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_opened = false;

        pcf::IndiProperty ip = makeAmpsProperty( "dmmode", "current_amps", { "0000" }, { 3.0f } );
        REQUIRE( app.newCallBack_m_indiP_currAmps( ip ) == 0 );
        REQUIRE( app.m_amps[0] == Approx( 3.0 ) );
        REQUIRE( app.m_amps[1] == 0 );
    }

    SECTION( "device is not checked" )
    {
        dmMode_test app( "dmmode" );
        app.setupModes( 4, 3, c_nModes );
        app.m_opened = false;

        // The callbacks compare only the property name, so a request addressed to another device is accepted.
        pcf::IndiProperty ip = makeAmpsProperty( "otherdev", "target_amps", { "0002" }, { 2.0f } );
        REQUIRE( app.newCallBack_m_indiP_tgtAmps( ip ) == 0 );
        REQUIRE( app.m_amps[2] == Approx( 2.0 ) );
    }
}

/// Verify DM mode telemetry is recorded on change, when forced, and when the interval elapses.
/**
 * Recording is detected through `telem_dmmodes::lastRecord`, which `telemeter::telem()` updates.
 *
 * \ingroup dmMode_unit_test
 */
TEST_CASE( "dmMode telemetry recording", "[dmMode]" )
{
    // clang-format off
    #ifdef DMMODE_TEST_DOXYGEN_REF
    dmMode::recordDmModes();
    dmMode::recordTelem(nullptr);
    dmMode::checkRecordTimes();
    #endif
    // clang-format on

    using MagAOX::logger::telem_dmmodes;

    dmMode_test app( "dmmode" );
    app.setupModes( 4, 3, c_nModes );
    app.m_maxInterval = 10.0;

    // Synchronize the internal last-recorded amplitudes.
    REQUIRE( app.recordDmModes( true ) == 0 );

    SECTION( "unchanged amplitudes are not recorded" )
    {
        telem_dmmodes::lastRecord = { 0, 0 };
        REQUIRE( app.recordDmModes( false ) == 0 );
        REQUIRE( telem_dmmodes::lastRecord.tv_sec == 0 );
    }

    SECTION( "changed amplitudes are recorded" )
    {
        telem_dmmodes::lastRecord = { 0, 0 };
        app.m_amps[2]             = 0.5f;
        REQUIRE( app.recordDmModes( false ) == 0 );
        REQUIRE( telem_dmmodes::lastRecord.tv_sec > 0 );
    }

    SECTION( "force records unchanged amplitudes" )
    {
        telem_dmmodes::lastRecord = { 0, 0 };
        REQUIRE( app.recordDmModes( true ) == 0 );
        REQUIRE( telem_dmmodes::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordTelem always records" )
    {
        telem_dmmodes::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( telem_dmmodes::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records after the interval" )
    {
        telem_dmmodes::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_dmmodes::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes skips a recent record" )
    {
        timespec now;
        clock_gettime( CLOCK_REALTIME, &now );
        telem_dmmodes::lastRecord = now;

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_dmmodes::lastRecord.tv_sec == now.tv_sec );
        REQUIRE( telem_dmmodes::lastRecord.tv_nsec == now.tv_nsec );
    }
}

} // namespace dmModeTest

} // namespace libXWCTest
