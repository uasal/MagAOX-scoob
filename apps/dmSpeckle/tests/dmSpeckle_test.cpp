/** \file dmSpeckle_test.cpp
 * \brief Catch2 tests for the dmSpeckle app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup dmSpeckle_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <mx/improc/milkImage.hpp>

#include "../dmSpeckle.hpp"

// Included after the app header so the callback bodies stay live.  The device/name validation
// macros are only used for the callbacks that check the full device.name key and return 0 for a
// property with no elements.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup dmSpeckle_unit_test dmSpeckle Unit Tests
 * \brief Unit tests for the dmSpeckle application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `dmSpeckle` unit tests.
/** \ingroup dmSpeckle_unit_test
 */
namespace dmSpeckleTest
{

/// Directory used as `MILK_SHM_DIR` for the shared-memory tests.
constexpr const char *c_shmDir = "/tmp/dmSpeckle_test_shm";

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing dmSpeckle internals.
class dmSpeckle_test : public dmSpeckle
{
  public:
    /// Construct a harness with the given device name.
    explicit dmSpeckle_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( trigger );
        XWCTEST_SETUP_INDI_NEW_PROP( delay );
        XWCTEST_SETUP_INDI_NEW_PROP( separation );
        XWCTEST_SETUP_INDI_NEW_PROP( angle );
        XWCTEST_SETUP_INDI_NEW_PROP( amp );
        XWCTEST_SETUP_INDI_NEW_PROP( cross );
        XWCTEST_SETUP_INDI_NEW_PROP( frequency );
        XWCTEST_SETUP_INDI_NEW_PROP( dwell );
        XWCTEST_SETUP_INDI_NEW_PROP( single );
        XWCTEST_SETUP_INDI_NEW_PROP( modulating );
        XWCTEST_SETUP_INDI_NEW_PROP( zero );
    }

    using dmSpeckle::m_amp;
    using dmSpeckle::m_angle;
    using dmSpeckle::m_angleOffset;
    using dmSpeckle::m_cross;
    using dmSpeckle::m_dataType;
    using dmSpeckle::m_dmChannelName;
    using dmSpeckle::m_dmName;
    using dmSpeckle::m_dmTriggerChannel;
    using dmSpeckle::m_dwell;
    using dmSpeckle::m_fileName;
    using dmSpeckle::m_frequency;
    using dmSpeckle::m_height;
    using dmSpeckle::m_modThreadCpuset;
    using dmSpeckle::m_modThreadInit;
    using dmSpeckle::m_modThreadPrio;
    using dmSpeckle::m_modulating;
    using dmSpeckle::m_opened;
    using dmSpeckle::m_opMode;
    using dmSpeckle::m_restartSp;
    using dmSpeckle::m_separation;
    using dmSpeckle::m_shapes;
    using dmSpeckle::m_shutdown;
    using dmSpeckle::m_single;
    using dmSpeckle::m_trigger;
    using dmSpeckle::m_triggerDelay;
    using dmSpeckle::m_triggerSemaphore;
    using dmSpeckle::m_typeSize;
    using dmSpeckle::m_width;

    using dmSpeckle::generateSpeckles;

    /// Setup the configurator and load the given config file.
    void loadConfigFile( const std::string &file /**< [in] config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
    }

    /// Run the modulator thread body in the calling thread.
    void runModulator()
    {
        modThreadExec();
    }

    /// Close the DM channel image opened by `appLogic()`.
    void closeStream()
    {
        ImageStreamIO_closeIm( &m_imageStream );
    }

    /// Get the cnt0 counter of the DM channel opened by `appLogic()`.
    /** \returns the counter */
    uint64_t streamCnt0()
    {
        return m_imageStream.md->cnt0;
    }
};
/// \endcond

/// Point ImageStreamIO at a private shared-memory directory.
void useTestShmDir()
{
    std::filesystem::create_directories( c_shmDir );
    setenv( "MILK_SHM_DIR", c_shmDir, 1 );
}

/// Compute the Fourier mode used by `mx::sigproc::makeFourierMode` for a square image.
/** \returns cos (p=+1) or sin (p=-1) of \f$ 2\pi (m u + n v)/N \f$ at pixel (i,j), with u,v measured from the center
 */
float fourierMode( int   i, /**< [in] first pixel index */
                   int   j, /**< [in] second pixel index */
                   int   N, /**< [in] image size */
                   float m, /**< [in] spatial frequency along the first index */
                   float n, /**< [in] spatial frequency along the second index */
                   int   p /**< [in] +1 for cosine, -1 for sine */ )
{
    float u   = i - 0.5 * ( N - 1.0 );
    float v   = j - 0.5 * ( N - 1.0 );
    float arg = 2 * M_PI / N * ( m * u + n * v );
    return ( p == 1 ) ? std::cos( arg ) : std::sin( arg );
}

/// Build a numeric INDI property with a single element.
/** \returns the new property */
pcf::IndiProperty makeNumber( const std::string &device, /**< [in] device name */
                              const std::string &name,   /**< [in] property name */
                              const std::string &el,     /**< [in] element name */
                              double             val /**< [in] element value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, val ) );
    return ip;
}

/// Build a switch INDI property with a single element.
/** \returns the new property */
pcf::IndiProperty makeSwitch( const std::string                &device, /**< [in] device name */
                              const std::string                &name,   /**< [in] property name */
                              const std::string                &el,     /**< [in] element name */
                              pcf::IndiElement::SwitchStateType state /**< [in] switch state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// Verify the default dmSpeckle configuration.
/**
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle configuration defaults", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::setupConfig();
    dmSpeckle::loadConfig();
    dmSpeckle::loadConfigImpl(config);
    #endif
    // clang-format on

    dmSpeckle_test app( "dmspeck" );

    mx::app::writeConfigFile( "/tmp/dmSpeckle_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.loadConfigFile( "/tmp/dmSpeckle_test_defaults.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_dmChannelName == "" );
    REQUIRE( app.m_dmName == "" );
    REQUIRE( app.m_dmTriggerChannel == "" );
    REQUIRE( app.m_triggerSemaphore == 9 );
    REQUIRE( app.m_trigger == true );
    REQUIRE( app.m_triggerDelay == Approx( 0.0 ) );
    REQUIRE( app.m_opMode == SPARKLE );
    REQUIRE( app.m_fileName == "" );
    REQUIRE( app.m_separation == Approx( 15.0 ) );
    REQUIRE( app.m_angle == Approx( 0.0 ) );
    REQUIRE( app.m_angleOffset == Approx( 28.0 ) );
    REQUIRE( app.m_amp == Approx( 0.01 ) );
    REQUIRE( app.m_cross == true );
    REQUIRE( app.m_frequency == Approx( 2000.0 ) );
    REQUIRE( app.m_dwell == 1 );
    REQUIRE( app.m_single == -1 );
    REQUIRE( app.m_modThreadPrio == 60 );
    REQUIRE( app.m_modThreadCpuset == "" );

    std::filesystem::remove( "/tmp/dmSpeckle_test_defaults.conf" );
}

/// Verify dmSpeckle configuration overrides.
/**
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle configuration overrides", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::setupConfig();
    dmSpeckle::loadConfig();
    dmSpeckle::loadConfigImpl(config);
    #endif
    // clang-format on

    SECTION( "all keys set, arbcube mode" )
    {
        dmSpeckle_test app( "dmspeck" );

        mx::app::writeConfigFile( "/tmp/dmSpeckle_test_overrides.conf",
                                  { "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "dm",
                                    "modulator",
                                    "modulator",
                                    "modulator",
                                    "modulator" },
                                  { "channelName",
                                    "triggerChannel",
                                    "triggerSemaphore",
                                    "trigger",
                                    "triggerDelay",
                                    "separation",
                                    "angle",
                                    "angleOffset",
                                    "amp",
                                    "cross",
                                    "frequency",
                                    "dwell",
                                    "threadPrio",
                                    "cpuset",
                                    "opMode",
                                    "fileName" },
                                  { "dm02disp05",
                                    "camsci1",
                                    "4",
                                    "false",
                                    "375",
                                    "12.5",
                                    "45",
                                    "10",
                                    "0.02",
                                    "false",
                                    "500",
                                    "3",
                                    "70",
                                    "/rt",
                                    "arbcube",
                                    "/tmp/cube.fits" } );
        app.loadConfigFile( "/tmp/dmSpeckle_test_overrides.conf" );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_dmChannelName == "dm02disp05" );
        REQUIRE( app.m_dmName == "dm02disp05" );
        REQUIRE( app.m_dmTriggerChannel == "camsci1" );
        REQUIRE( app.m_triggerSemaphore == 4 );
        REQUIRE( app.m_trigger == false );
        REQUIRE( app.m_triggerDelay == Approx( 375.0 ) );
        REQUIRE( app.m_separation == Approx( 12.5 ) );
        REQUIRE( app.m_angle == Approx( 45.0 ) );
        REQUIRE( app.m_angleOffset == Approx( 10.0 ) );
        REQUIRE( app.m_amp == Approx( 0.02 ) );
        REQUIRE( app.m_cross == false );
        REQUIRE( app.m_frequency == Approx( 500.0 ) );
        REQUIRE( app.m_dwell == 3 );
        REQUIRE( app.m_modThreadPrio == 70 );
        REQUIRE( app.m_modThreadCpuset == "/rt" );
        REQUIRE( app.m_opMode == ARBCUBE );
        REQUIRE( app.m_fileName == "/tmp/cube.fits" );

        std::filesystem::remove( "/tmp/dmSpeckle_test_overrides.conf" );
    }

    SECTION( "an unknown opMode selects sparkle" )
    {
        dmSpeckle_test app( "dmspeck" );

        mx::app::writeConfigFile( "/tmp/dmSpeckle_test_opmode.conf", { "modulator" }, { "opMode" }, { "glitter" } );
        app.loadConfigFile( "/tmp/dmSpeckle_test_opmode.conf" );

        REQUIRE( app.m_opMode == SPARKLE );

        std::filesystem::remove( "/tmp/dmSpeckle_test_opmode.conf" );
    }
}

/// Verify the sparkle speckle pattern: plane signs, Fourier mode type, angle convention and cross speckles.
/**
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle generateSpeckles sparkle mode", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::generateSpeckles();
    #endif
    // clang-format on

    constexpr int N = 16;

    dmSpeckle_test app( "dmspeck" );
    app.m_width       = N;
    app.m_height      = N;
    app.m_opMode      = SPARKLE;
    app.m_separation  = 3;
    app.m_angleOffset = 28;
    app.m_amp         = 0.5;

    SECTION( "angle equal to the offset gives speckles along the first axis" )
    {
        app.m_angle = 28;
        app.m_cross = false;

        REQUIRE( app.generateSpeckles() == 0 );
        REQUIRE( app.m_shapes.rows() == N );
        REQUIRE( app.m_shapes.cols() == N );
        REQUIRE( app.m_shapes.planes() == 4 );

        for( int i = 0; i < N; ++i )
        {
            for( int j = 0; j < N; ++j )
            {
                float c = 0.5 * fourierMode( i, j, N, 3, 0, 1 );
                float s = 0.5 * fourierMode( i, j, N, 3, 0, -1 );
                REQUIRE( app.m_shapes.image( 0 )( i, j ) == Approx( c ).margin( 1e-5 ) );
                REQUIRE( app.m_shapes.image( 1 )( i, j ) == Approx( -c ).margin( 1e-5 ) );
                REQUIRE( app.m_shapes.image( 2 )( i, j ) == Approx( s ).margin( 1e-5 ) );
                REQUIRE( app.m_shapes.image( 3 )( i, j ) == Approx( -s ).margin( 1e-5 ) );
            }
        }
    }

    SECTION( "a 90 degree rotation moves the speckles to the second axis" )
    {
        app.m_angle = 28 - 90;
        app.m_cross = false;

        REQUIRE( app.generateSpeckles() == 0 );

        for( int i = 0; i < N; ++i )
        {
            for( int j = 0; j < N; ++j )
            {
                REQUIRE( app.m_shapes.image( 0 )( i, j ) ==
                         Approx( 0.5 * fourierMode( i, j, N, 0, 3, 1 ) ).margin( 1e-4 ) );
                REQUIRE( app.m_shapes.image( 2 )( i, j ) ==
                         Approx( 0.5 * fourierMode( i, j, N, 0, 3, -1 ) ).margin( 1e-4 ) );
            }
        }
    }

    SECTION( "cross adds the speckles rotated by 90 degrees" )
    {
        app.m_angle = 28;
        app.m_cross = true;

        REQUIRE( app.generateSpeckles() == 0 );

        for( int i = 0; i < N; ++i )
        {
            for( int j = 0; j < N; ++j )
            {
                float c = 0.5 * ( fourierMode( i, j, N, 3, 0, 1 ) + fourierMode( i, j, N, 0, 3, 1 ) );
                float s = 0.5 * ( fourierMode( i, j, N, 3, 0, -1 ) + fourierMode( i, j, N, 0, 3, -1 ) );
                REQUIRE( app.m_shapes.image( 0 )( i, j ) == Approx( c ).margin( 1e-5 ) );
                REQUIRE( app.m_shapes.image( 1 )( i, j ) == Approx( -c ).margin( 1e-5 ) );
                REQUIRE( app.m_shapes.image( 2 )( i, j ) == Approx( s ).margin( 1e-5 ) );
                REQUIRE( app.m_shapes.image( 3 )( i, j ) == Approx( -s ).margin( 1e-5 ) );
            }
        }
    }

    SECTION( "single keeps only the selected plane" )
    {
        app.m_angle  = 28;
        app.m_cross  = false;
        app.m_single = 2;

        REQUIRE( app.generateSpeckles() == 0 );

        REQUIRE( app.m_shapes.image( 0 ).abs().maxCoeff() == Approx( 0.0 ) );
        REQUIRE( app.m_shapes.image( 1 ).abs().maxCoeff() == Approx( 0.0 ) );
        REQUIRE( app.m_shapes.image( 3 ).abs().maxCoeff() == Approx( 0.0 ) );
        REQUIRE( app.m_shapes.image( 2 )( 5, 7 ) == Approx( 0.5 * fourierMode( 5, 7, N, 3, 0, -1 ) ).margin( 1e-5 ) );
    }

    std::filesystem::remove( "/tmp/specks.fits" );
}

/// Verify the arbitrary-cube mode loads and scales the shape cube, and rejects bad files.
/**
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle generateSpeckles arbcube mode", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::generateSpeckles();
    #endif
    // clang-format on

    const std::string cubeFile = "/tmp/dmSpeckle_test_cube.fits";

    mx::improc::eigenCube<float> cube( 6, 5, 3 );
    for( int p = 0; p < 3; ++p )
    {
        for( int r = 0; r < 6; ++r )
        {
            for( int c = 0; c < 5; ++c )
            {
                cube.image( p )( r, c ) = 100.0f * p + 10.0f * r + c;
            }
        }
    }
    std::filesystem::remove( cubeFile );
    mx::fits::fitsFile<float> ff;
    REQUIRE( ff.write( cubeFile, cube ) == mx::error_t::noerror );

    dmSpeckle_test app( "dmspeck" );
    app.m_opMode = ARBCUBE;
    app.m_amp    = 0.5;

    SECTION( "matching cube is scaled by the amplitude" )
    {
        app.m_fileName = cubeFile;
        app.m_width    = 6;
        app.m_height   = 5;

        REQUIRE( app.generateSpeckles() == 0 );
        REQUIRE( app.m_shapes.planes() == 3 );
        REQUIRE( app.m_shapes.image( 0 )( 0, 0 ) == Approx( 0.0 ) );
        REQUIRE( app.m_shapes.image( 1 )( 2, 3 ) == Approx( 0.5 * 123.0 ) );
        REQUIRE( app.m_shapes.image( 2 )( 5, 4 ) == Approx( 0.5 * 254.0 ) );
    }

    SECTION( "wrong size is rejected" )
    {
        app.m_fileName = cubeFile;
        app.m_width    = 5;
        app.m_height   = 6;

        REQUIRE( app.generateSpeckles() == -1 );
    }

    SECTION( "missing file is rejected" )
    {
        app.m_fileName = "/tmp/dmSpeckle_test_no_such_cube.fits";
        app.m_width    = 6;
        app.m_height   = 5;

        REQUIRE( app.generateSpeckles() == -1 );
    }

    std::filesystem::remove( cubeFile );
}

/// Verify appLogic connects to a float DM channel and rejects unusable channels.
/**
 * There is no telemetry thread in unit tests, so the telemeter step at the end of every appLogic call
 * sets the FAILURE state and requests shutdown.
 *
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle appLogic DM channel connection", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::appLogic();
    dmSpeckle::appShutdown();
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "missing channel" )
    {
        dmSpeckle_test app( "dmspeck" );
        app.m_dmChannelName = "dmspeck_missing";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_opened == false );
        REQUIRE( app.m_width == 0 );
        REQUIRE( app.state() == stateCodes::FAILURE );
        REQUIRE( app.m_shutdown == 1 );
        REQUIRE( app.appShutdown() == 0 );
    }

    SECTION( "float channel connects" )
    {
        mx::improc::milkImage<float> chan;
        chan.create( "dmspeck_dm", 16, 12 );

        dmSpeckle_test app( "dmspeck" );
        app.m_dmChannelName = "dmspeck_dm";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_opened == true );
        REQUIRE( app.m_width == 16 );
        REQUIRE( app.m_height == 12 );
        REQUIRE( app.m_dataType == _DATATYPE_FLOAT );
        REQUIRE( app.m_typeSize == sizeof( float ) );

        app.closeStream();
    }

    SECTION( "double channel is rejected" )
    {
        mx::improc::milkImage<double> chan;
        chan.create( "dmspeck_dbl", 16, 12 );

        dmSpeckle_test app( "dmspeck" );
        app.m_dmChannelName = "dmspeck_dbl";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == -1 );
        REQUIRE( app.state() == stateCodes::CONNECTED );

        app.closeStream();
    }

    SECTION( "trigger channel with too few semaphores blocks the connection" )
    {
        mx::improc::milkImage<float> chan;
        chan.create( "dmspeck_dm", 16, 12 );

        IMAGE    trig{};
        uint32_t imsize[3] = { 4, 4, 1 };
        REQUIRE(
            ImageStreamIO_createIm_gpu(
                &trig, "dmspeck_trig", 3, imsize, _DATATYPE_FLOAT, -1, 1, 2, 0, CIRCULAR_BUFFER | ZAXIS_TEMPORAL, 0 ) ==
            IMAGESTREAMIO_SUCCESS );

        dmSpeckle_test app( "dmspeck" );
        app.m_dmChannelName    = "dmspeck_dm";
        app.m_dmTriggerChannel = "dmspeck_trig";
        app.state( stateCodes::NOTCONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_opened == false );
        REQUIRE( app.m_width == 0 );

        app.closeStream();
        ImageStreamIO_destroyIm( &trig );
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify the numeric INDI callbacks set the speckle parameters and flag a restart.
/**
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle numeric INDI callbacks", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::newCallBack_m_indiP_delay(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_separation(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_angle(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_amp(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_frequency(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_dwell(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "delay" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_delay( makeNumber( "dmspeck", "delay", "target", 250 ) ) == 0 );
        REQUIRE( app.m_triggerDelay == Approx( 250.0 ) );
        REQUIRE( app.m_restartSp == true );
    }

    SECTION( "delay with no value is ignored" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_delay( makeNumber( "dmspeck", "delay", "value", 250 ) ) == 0 );
        REQUIRE( app.m_triggerDelay == Approx( 0.0 ) );
        REQUIRE( app.m_restartSp == false );
    }

    SECTION( "separation from current" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_separation( makeNumber( "dmspeck", "separation", "current", 7.5 ) ) == 0 );
        REQUIRE( app.m_separation == Approx( 7.5 ) );
        REQUIRE( app.m_restartSp == true );
    }

    SECTION( "angle" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_angle( makeNumber( "dmspeck", "angle", "target", -30 ) ) == 0 );
        REQUIRE( app.m_angle == Approx( -30.0 ) );
        REQUIRE( app.m_restartSp == true );
    }

    SECTION( "amp" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_amp( makeNumber( "dmspeck", "amp", "target", 0.05 ) ) == 0 );
        REQUIRE( app.m_amp == Approx( 0.05 ) );
        REQUIRE( app.m_restartSp == true );
    }

    SECTION( "frequency" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_frequency( makeNumber( "dmspeck", "frequency", "target", 1000 ) ) == 0 );
        REQUIRE( app.m_frequency == Approx( 1000.0 ) );
        REQUIRE( app.m_restartSp == true );
    }

    SECTION( "negative frequency is ignored" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_frequency( makeNumber( "dmspeck", "frequency", "target", -5 ) ) == 0 );
        REQUIRE( app.m_frequency == Approx( 2000.0 ) );
        REQUIRE( app.m_restartSp == false );
    }

    SECTION( "dwell" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_dwell( makeNumber( "dmspeck", "dwell", "target", 4 ) ) == 0 );
        REQUIRE( app.m_dwell == 4 );
        REQUIRE( app.m_restartSp == true );
    }

    SECTION( "zero dwell is ignored" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_dwell( makeNumber( "dmspeck", "dwell", "target", 0 ) ) == 0 );
        REQUIRE( app.m_dwell == 1 );
        REQUIRE( app.m_restartSp == false );
    }

    SECTION( "single in range" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_single( makeNumber( "dmspeck", "single", "target", 3 ) ) == 0 );
        REQUIRE( app.m_single == 3 );
        REQUIRE( app.newCallBack_m_indiP_single( makeNumber( "dmspeck", "single", "target", -1 ) ) == 0 );
        REQUIRE( app.m_single == -1 );
    }

    SECTION( "single out of range is ignored" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_single( makeNumber( "dmspeck", "single", "target", 4 ) ) == 0 );
        REQUIRE( app.m_single == -1 );
        REQUIRE( app.newCallBack_m_indiP_single( makeNumber( "dmspeck", "single", "target", -2 ) ) == 0 );
        REQUIRE( app.m_single == -1 );
    }

    SECTION( "wrong property names are rejected" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_delay( makeNumber( "dmspeck", "amp", "target", 1 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_separation( makeNumber( "dmspeck", "angle", "target", 1 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_angle( makeNumber( "dmspeck", "separation", "target", 1 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_amp( makeNumber( "dmspeck", "delay", "target", 1 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_frequency( makeNumber( "dmspeck", "dwell", "target", 1 ) ) == -1 );
        REQUIRE( app.m_triggerDelay == Approx( 0.0 ) );
        REQUIRE( app.m_separation == Approx( 15.0 ) );
        REQUIRE( app.m_angle == Approx( 0.0 ) );
        REQUIRE( app.m_amp == Approx( 0.01 ) );
        REQUIRE( app.m_frequency == Approx( 2000.0 ) );
        REQUIRE( app.m_restartSp == false );
    }
}

/// Verify the switch INDI callbacks for trigger, cross, modulating and zero.
/**
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle switch INDI callbacks", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::newCallBack_m_indiP_trigger(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_cross(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_modulating(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_zero(const pcf::IndiProperty &);
    #endif
    // clang-format on

    using sw = pcf::IndiElement;

    SECTION( "trigger off and on" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_trigger( makeSwitch( "dmspeck", "trigger", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_trigger == false );
        REQUIRE( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_trigger( makeSwitch( "dmspeck", "trigger", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_trigger == true );
        REQUIRE( app.m_restartSp == true );
    }

    SECTION( "trigger without toggle is ignored" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_trigger( makeSwitch( "dmspeck", "trigger", "request", sw::Off ) ) == 0 );
        REQUIRE( app.m_trigger == true );
        REQUIRE( app.m_restartSp == false );
    }

    SECTION( "cross off and on" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_cross( makeSwitch( "dmspeck", "cross", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_cross == false );
        REQUIRE( app.m_restartSp == true );
        REQUIRE( app.newCallBack_m_indiP_cross( makeSwitch( "dmspeck", "cross", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_cross == true );
    }

    SECTION( "modulating on and off" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_modulating( makeSwitch( "dmspeck", "modulating", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_modulating == true );
        REQUIRE( app.newCallBack_m_indiP_modulating( makeSwitch( "dmspeck", "modulating", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_modulating == false );
    }

    SECTION( "wrong property names are rejected" )
    {
        dmSpeckle_test app( "dmspeck" );
        REQUIRE( app.newCallBack_m_indiP_trigger( makeSwitch( "dmspeck", "cross", "toggle", sw::Off ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_modulating( makeSwitch( "dmspeck", "trigger", "toggle", sw::On ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "dmspeck", "modulating", "request", sw::On ) ) == -1 );
        REQUIRE( app.m_trigger == true );
        REQUIRE( app.m_modulating == false );
    }

    SECTION( "zero is refused while modulating" )
    {
        dmSpeckle_test app( "dmspeck" );
        app.m_modulating = true;
        REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "dmspeck", "zero", "request", sw::On ) ) == 0 );
    }

    SECTION( "zero clears the DM channel" )
    {
        useTestShmDir();

        {
            mx::improc::milkImage<float> chan;
            chan.create( "dmspeck_dm", 8, 8 );
            chan().setConstant( 1.0 );

            dmSpeckle_test app( "dmspeck" );
            app.m_dmChannelName = "dmspeck_dm";
            app.state( stateCodes::NOTCONNECTED );
            REQUIRE( app.appLogic() == 0 );
            REQUIRE( app.m_opened == true );

            uint64_t cnt0 = app.streamCnt0();

            REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "dmspeck", "zero", "request", sw::Off ) ) == 0 );
            REQUIRE( chan().minCoeff() == Approx( 1.0 ) );

            REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "dmspeck", "zero", "request", sw::On ) ) == 0 );
            REQUIRE( chan().abs().maxCoeff() == Approx( 0.0 ) );
            REQUIRE( app.streamCnt0() == cnt0 + 1 );

            app.closeStream();
        }

        std::filesystem::remove_all( c_shmDir );
    }
}

/// Verify the device/name validation of the callbacks that check the full device.name key.
/**
 * The other callbacks compare only the property name, so they accept a request addressed to another device.
 *
 * \ingroup dmSpeckle_unit_test
 */
SCENARIO( "dmSpeckle INDI callbacks validate device and property names", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::newCallBack_m_indiP_cross(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_dwell(const pcf::IndiProperty &);
    dmSpeckle::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( dmSpeckle, cross );
    XWCTEST_INDI_NEW_CALLBACK( dmSpeckle, dwell );
    XWCTEST_INDI_NEW_CALLBACK( dmSpeckle, single );

    GIVEN( "A name-only callback" )
    {
        WHEN( "the device differs" )
        {
            dmSpeckle_test app( "right" );
            REQUIRE( app.newCallBack_m_indiP_amp( makeNumber( "wrong", "amp", "target", 0.2 ) ) == 0 );
            REQUIRE( app.m_amp == Approx( 0.2 ) );
        }
    }
}

/// Verify the modulator thread writes the speckle pattern to the DM and zeroes it when stopped.
/**
 * The modulator body is run in a separate thread in free-running (untriggered) mode at 1 kHz.
 *
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle modulator writes and then zeroes the DM", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::modThreadExec();
    #endif
    // clang-format on

    useTestShmDir();

    {
        mx::improc::milkImage<float> chan;
        chan.create( "dmspeck_dm", 16, 16 );
        chan().setZero();

        dmSpeckle_test app( "dmspeck" );
        app.m_dmChannelName = "dmspeck_dm";
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_opened == true );

        // Undo the telemetry-thread failure from appLogic so the modulator can run.
        app.m_shutdown = 0;
        app.state( stateCodes::READY );

        app.m_modThreadInit = false;
        app.m_separation    = 3;
        app.m_amp           = 0.5;
        app.m_frequency     = 1000;
        app.m_trigger       = true; // no trigger channel, so the modulator must switch this off
        app.m_modulating    = true;

        uint64_t cnt0 = app.streamCnt0();

        std::thread mod( [&]() { app.runModulator(); } );

        bool sawPattern = false;
        for( int n = 0; n < 400 && !sawPattern; ++n )
        {
            mx::sys::milliSleep( 5 );
            if( chan().abs().maxCoeff() > 0.1 )
            {
                sawPattern = true;
            }
        }

        app.m_modulating = false;
        mx::sys::milliSleep( 100 );
        app.m_shutdown = 1;
        mod.join();

        REQUIRE( sawPattern );
        REQUIRE( app.m_trigger == false );
        REQUIRE( app.streamCnt0() > cnt0 + 1 );
        REQUIRE( chan().abs().maxCoeff() == Approx( 0.0 ) );
        REQUIRE( app.m_shapes.planes() == 4 );

        app.closeStream();
    }

    std::filesystem::remove( "/tmp/specks.fits" );
    std::filesystem::remove_all( c_shmDir );
}

/// Verify the speckle telemetry is recorded on change, when forced, and when the interval elapses.
/**
 * Recording is detected through `telem_dmspeck::lastRecord`, which `telemeter::telem()` updates.
 *
 * \ingroup dmSpeckle_unit_test
 */
TEST_CASE( "dmSpeckle speckle telemetry", "[dmSpeckle]" )
{
    // clang-format off
    #ifdef DMSPECKLE_TEST_DOXYGEN_REF
    dmSpeckle::recordDmSpeck(false);
    dmSpeckle::recordTelem(nullptr);
    dmSpeckle::checkRecordTimes();
    #endif
    // clang-format on

    using MagAOX::logger::telem_dmspeck;

    dmSpeckle_test app( "dmspeck" );
    app.m_maxInterval = 10.0;

    // Synchronize the internal last-recorded values.
    REQUIRE( app.recordDmSpeck( true ) == 0 );

    SECTION( "unchanged values are not recorded" )
    {
        telem_dmspeck::lastRecord = { 0, 0 };
        REQUIRE( app.recordDmSpeck( false ) == 0 );
        REQUIRE( telem_dmspeck::lastRecord.tv_sec == 0 );
    }

    SECTION( "an amplitude change is recorded" )
    {
        telem_dmspeck::lastRecord = { 0, 0 };
        app.m_amp                 = 0.03;
        REQUIRE( app.recordDmSpeck( false ) == 0 );
        REQUIRE( telem_dmspeck::lastRecord.tv_sec > 0 );
    }

    SECTION( "a modulating change is recorded" )
    {
        telem_dmspeck::lastRecord = { 0, 0 };
        app.m_modulating          = true;
        REQUIRE( app.recordDmSpeck( false ) == 0 );
        REQUIRE( telem_dmspeck::lastRecord.tv_sec > 0 );
    }

    SECTION( "a cross change is recorded" )
    {
        telem_dmspeck::lastRecord = { 0, 0 };
        app.m_cross               = false;
        REQUIRE( app.recordDmSpeck( false ) == 0 );
        REQUIRE( telem_dmspeck::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordTelem always records" )
    {
        telem_dmspeck::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( telem_dmspeck::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records after the interval" )
    {
        telem_dmspeck::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_dmspeck::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes skips a recent record" )
    {
        timespec now;
        clock_gettime( CLOCK_REALTIME, &now );
        telem_dmspeck::lastRecord = now;

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_dmspeck::lastRecord.tv_sec == now.tv_sec );
        REQUIRE( telem_dmspeck::lastRecord.tv_nsec == now.tv_nsec );
    }
}

} // namespace dmSpeckleTest

} // namespace libXWCTest
