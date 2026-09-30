/** \file photonCounter_test.cpp
 * \brief Catch2 tests for the photonCounter app.
 * \author Claude Code
 *
 * \ingroup photonCounter_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <semaphore.h>
#include <string>
#include <vector>

#define protected public
#include "../photonCounter.hpp"
#undef protected

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup photonCounter_unit_test photonCounter Unit Tests
 * \brief Unit tests for the photonCounter application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `photonCounter` unit tests.
/** \ingroup photonCounter_unit_test
 */
namespace photonCounterTest
{

/// The shmimMonitor base of photonCounter, used to qualify members also present in the frameGrabber base.
typedef dev::shmimMonitor<photonCounter> smBaseT;

/// The frameGrabber base of photonCounter, used to qualify members also present in the shmimMonitor base.
typedef dev::frameGrabber<photonCounter> fgBaseT;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness which initializes the photonCounter state its constructor leaves unset.
class photonCounter_test : public photonCounter
{
  public:
    /// Construct a harness with the given device name.
    explicit photonCounter_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // The semaphore is normally initialized in appStartup()
        sem_init( &m_smSemaphore, 0, 0 );

        // photonCounter() leaves these uninitialized
        m_image_width                 = 0;
        m_image_height                = 0;
        m_quantile_cut                = 0.5;
        m_calibrate                   = false;
        m_calibrationSet              = false;
        calibration_steps             = 10;
        current_calibration_iteration = 0;
        m_stack_frames                = 1;
        m_stack_frames_index          = 0;

        // Names are normally set in appStartup()
        m_indiP_calibrateToggle.setDevice( device );
        m_indiP_calibrateToggle.setName( "calibrate" );
        m_indiP_calibrateSteps.setDevice( device );
        m_indiP_calibrateSteps.setName( "nFrames" );
        m_indiP_stackFrames.setDevice( device );
        m_indiP_stackFrames.setName( "stackNframes" );
        m_indiP_quantileCut.setDevice( device );
        m_indiP_quantileCut.setName( "quantile" );
    }

    /// Destroy the harness, releasing the semaphore.
    ~photonCounter_test() noexcept
    {
        sem_destroy( &m_smSemaphore );
    }

    /// Set up the configuration, read a config file, and load it.
    void loadTestConfig( const std::string &path /**< [in] the config file path */ )
    {
        setupConfig();
        config.readConfig( path );
        loadConfig();
    }

    /// Set the input stream geometry as if connected to a uint16 stream, and allocate.
    int connectStream( uint32_t N /**< [in] the (square) image size */ )
    {
        smBaseT::m_width    = N;
        smBaseT::m_height   = N;
        smBaseT::m_dataType = _DATATYPE_UINT16;

        return allocate( dev::shmimT() );
    }

    /// Get the current value of the frame semaphore.
    int semValue()
    {
        int val = -1;
        sem_getvalue( &m_smSemaphore, &val );
        return val;
    }
};
/// \endcond

/// Set pixel (r,c) of a column-major N x N uint16 frame, matching the Eigen::Map used by processImage().
void setPix( std::vector<uint16_t> &frame, /**< [in.out] the frame buffer, N*N pixels */
             uint32_t               N,     /**< [in] the image size */
             uint32_t               r,     /**< [in] the row */
             uint32_t               c,     /**< [in] the column */
             uint16_t               val    /**< [in] the pixel value */
)
{
    frame[r + c * N] = val;
}

/// Build a number property with a single element.
pcf::IndiProperty numberProp( const std::string &device, /**< [in] the device name */
                              const std::string &name,   /**< [in] the property name */
                              const std::string &el,     /**< [in] the element name */
                              float              val     /**< [in] the element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, val ) );
    return ip;
}

/// Build a switch property with a single element.
pcf::IndiProperty switchProp( const std::string                       &device, /**< [in] the device name */
                              const std::string                       &name,   /**< [in] the property name */
                              const std::string                       &el,     /**< [in] the element name */
                              const pcf::IndiElement::SwitchStateType &state   /**< [in] the switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// Run a 3x3, 4-frame calibration with quantile 0.5 on an allocated harness.
/** Pixel p = r + 3c of frame k has value 10p + {3,0,2,1}[k], so the dark (mean) is 10p + 1.5 and the
 * threshold (sorted index int(0.5*4) = 2) is 10p + 2.
 */
void runCalibration( photonCounter_test &app /**< [in.out] the harness, already allocated as 3x3 */ )
{
    const uint32_t N          = 3;
    const uint16_t offsets[4] = { 3, 0, 2, 1 };
    app.calibration_steps     = 4;
    app.m_quantile_cut        = 0.5;
    app.m_calibrationCube.resize( N, N, 4 );
    app.m_calibrationCube.setZero();
    app.m_calibrate = true;

    for( int k = 0; k < 4; ++k )
    {
        std::vector<uint16_t> frame( N * N, 0 );
        for( uint32_t c = 0; c < N; ++c )
        {
            for( uint32_t r = 0; r < N; ++r )
            {
                setPix( frame, N, r, c, 10 * ( r + c * N ) + offsets[k] );
            }
        }

        app.processImage( frame.data(), dev::shmimT() );
    }
}

/// Verify the photonCounter configuration defaults load from an empty config file.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter configuration defaults", "[photonCounter][config]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::setupConfig();
    photonCounter::loadConfig();
    photonCounter::loadConfigImpl(config);
    #endif
    // clang-format on

    photonCounter_test app( "pc" );

    const std::string path = "/tmp/photonCounter_test_defaults.conf";
    mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    // Unset parameters keep their pre-config values
    REQUIRE( app.m_stack_frames == 1 );
    REQUIRE( app.m_quantile_cut == Approx( 0.5 ) );
    REQUIRE( app.calibration_steps == 10 );

    // Both streams default to the config name
    REQUIRE( app.smBaseT::m_shmimName == "pc" );
    REQUIRE( app.fgBaseT::m_shmimName == "pc" );
    REQUIRE( app.m_circBuffLength == 1 );
    REQUIRE( app.smBaseT::m_getExistingFirst == false );
}

/// Verify photonCounter configuration overrides are loaded.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter configuration overrides", "[photonCounter][config]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::setupConfig();
    photonCounter::loadConfig();
    photonCounter::loadConfigImpl(config);
    #endif
    // clang-format on

    photonCounter_test app( "pc" );

    const std::string path = "/tmp/photonCounter_test_overrides.conf";
    mx::app::writeConfigFile(
        path,
        { "parameters", "parameters", "parameters", "shmimMonitor", "framegrabber", "framegrabber" },
        { "quantile", "Nstack", "Ncalibrate", "shmimName", "shmimName", "circBuffLength" },
        { "0.9", "5", "200", "camsci1", "camsci1_pc", "3" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_quantile_cut == Approx( 0.9 ) );
    REQUIRE( app.m_stack_frames == 5 );
    REQUIRE( app.calibration_steps == 200 );

    REQUIRE( app.smBaseT::m_shmimName == "camsci1" );
    REQUIRE( app.fgBaseT::m_shmimName == "camsci1_pc" );
    REQUIRE( app.m_circBuffLength == 3 );
}

/// Verify allocate() sizes the calibration cube and images from the input stream, and resets the state.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter allocate", "[photonCounter]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::allocate(dev::shmimT());
    #endif
    // clang-format on

    photonCounter_test app( "pc" );

    app.calibration_steps             = 6;
    app.m_calibrate                   = true;
    app.m_calibrationSet              = true;
    app.current_calibration_iteration = 3;
    app.m_stack_frames_index          = 2;

    REQUIRE( app.connectStream( 4 ) == 0 );

    REQUIRE( app.m_image_width == 4 );
    REQUIRE( app.m_image_height == 4 );

    REQUIRE( app.m_calibrationCube.rows() == 4 );
    REQUIRE( app.m_calibrationCube.cols() == 4 );
    REQUIRE( app.m_calibrationCube.planes() == 6 );
    REQUIRE( app.m_calibrationCube.image( 5 ).sum() == Approx( 0 ) );

    REQUIRE( app.m_thresholdImage.rows() == 4 );
    REQUIRE( app.m_thresholdImage.cols() == 4 );
    REQUIRE( app.m_thresholdImage.sum() == Approx( 0 ) );
    REQUIRE( app.m_dark_image.rows() == 4 );
    REQUIRE( app.m_dark_image.sum() == Approx( 0 ) );
    REQUIRE( app.m_photonCountedImage.rows() == 4 );
    REQUIRE( app.m_photonCountedImage.sum() == Approx( 0 ) );

    REQUIRE( app.m_calibrate == false );
    REQUIRE( app.m_calibrationSet == false );
    REQUIRE( app.current_calibration_iteration == 0 );
    REQUIRE( app.m_stack_frames_index == 0 );
}

/// Verify processImage() collects calibration frames and computes the dark and quantile threshold images.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter calibration", "[photonCounter][calibration]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::processImage(nullptr, dev::shmimT());
    #endif
    // clang-format on

    photonCounter_test app( "pc" );
    REQUIRE( app.connectStream( 3 ) == 0 );

    SECTION( "partial calibration keeps collecting" )
    {
        app.calibration_steps = 4;
        app.m_calibrationCube.resize( 3, 3, 4 );
        app.m_calibrationCube.setZero();
        app.m_calibrate = true;

        std::vector<uint16_t> frame( 9, 0 );
        setPix( frame, 3, 2, 1, 77 );
        setPix( frame, 3, 0, 2, 12 );

        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

        REQUIRE( app.current_calibration_iteration == 2 );
        REQUIRE( app.m_calibrate == true );
        REQUIRE( app.m_calibrationSet == false );

        // Frames are stored in the cube at (row, col)
        REQUIRE( app.m_calibrationCube.image( 0 )( 2, 1 ) == Approx( 77 ) );
        REQUIRE( app.m_calibrationCube.image( 1 )( 2, 1 ) == Approx( 77 ) );
        REQUIRE( app.m_calibrationCube.image( 1 )( 0, 2 ) == Approx( 12 ) );
        REQUIRE( app.m_calibrationCube.image( 2 )( 2, 1 ) == Approx( 0 ) );
    }

    SECTION( "complete calibration" )
    {
        runCalibration( app );

        REQUIRE( app.m_calibrate == false );
        REQUIRE( app.m_calibrationSet == true );
        REQUIRE( app.current_calibration_iteration == 0 );

        for( uint32_t c = 0; c < 3; ++c )
        {
            for( uint32_t r = 0; r < 3; ++r )
            {
                float p = 10.0 * ( r + c * 3 );
                REQUIRE( app.m_dark_image( r, c ) == Approx( p + 1.5 ) );
                REQUIRE( app.m_thresholdImage( r, c ) == Approx( p + 2 ) );
            }
        }

        // Calibration does not count photons or post the semaphore
        REQUIRE( app.m_photonCountedImage.sum() == Approx( 0 ) );
        REQUIRE( app.semValue() == 0 );
    }

    SECTION( "quantile 0 gives the minimum" )
    {
        app.calibration_steps = 3;
        app.m_quantile_cut    = 0;
        app.m_calibrationCube.resize( 3, 3, 3 );
        app.m_calibrationCube.setZero();
        app.m_calibrate = true;

        const uint16_t vals[3] = { 50, 20, 80 };
        for( int k = 0; k < 3; ++k )
        {
            std::vector<uint16_t> frame( 9, vals[k] );
            app.processImage( frame.data(), dev::shmimT() );
        }

        REQUIRE( app.m_calibrationSet == true );
        REQUIRE( app.m_thresholdImage( 1, 1 ) == Approx( 20 ) );
        REQUIRE( app.m_dark_image( 1, 1 ) == Approx( 50 ) );
    }

    SECTION( "a new calibration request clears the calibrated flag" )
    {
        runCalibration( app );
        REQUIRE( app.m_calibrationSet == true );

        app.m_calibrate = true;
        std::vector<uint16_t> frame( 9, 0 );
        app.processImage( frame.data(), dev::shmimT() );

        REQUIRE( app.m_calibrationSet == false );
        REQUIRE( app.current_calibration_iteration == 1 );
    }
}

/// Verify processImage() counts pixels above threshold and posts the semaphore after Nstack frames.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter photon counting", "[photonCounter][counting]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::processImage(nullptr, dev::shmimT());
    #endif
    // clang-format on

    photonCounter_test app( "pc" );
    REQUIRE( app.connectStream( 3 ) == 0 );

    SECTION( "no calibration means no counting" )
    {
        std::vector<uint16_t> frame( 9, 1000 );
        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

        REQUIRE( app.m_photonCountedImage.sum() == Approx( 0 ) );
        REQUIRE( app.m_stack_frames_index == 0 );
        REQUIRE( app.semValue() == 0 );
    }

    SECTION( "counts strictly above threshold and stacks frames" )
    {
        runCalibration( app ); // threshold(r,c) = 10*(r+3c) + 2
        app.m_stack_frames = 3;

        // Frame 1: pixel (0,0) above (3 > 2), pixel (1,0) equal to threshold (12), pixel (2,2) above (83 > 82)
        std::vector<uint16_t> f1( 9, 0 );
        setPix( f1, 3, 0, 0, 3 );
        setPix( f1, 3, 1, 0, 12 );
        setPix( f1, 3, 2, 2, 83 );

        REQUIRE( app.processImage( f1.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_stack_frames_index == 1 );
        REQUIRE( app.m_photonCountedImage( 0, 0 ) == Approx( 1 ) );
        REQUIRE( app.m_photonCountedImage( 1, 0 ) == Approx( 0 ) );
        REQUIRE( app.m_photonCountedImage( 2, 2 ) == Approx( 1 ) );
        REQUIRE( app.m_photonCountedImage.sum() == Approx( 2 ) );
        REQUIRE( app.semValue() == 0 );

        // Frame 2: all pixels far above
        std::vector<uint16_t> f2( 9, 1000 );
        REQUIRE( app.processImage( f2.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_stack_frames_index == 2 );
        REQUIRE( app.m_photonCountedImage( 0, 0 ) == Approx( 2 ) );
        REQUIRE( app.m_photonCountedImage( 0, 1 ) == Approx( 1 ) );
        REQUIRE( app.m_photonCountedImage.sum() == Approx( 11 ) );
        REQUIRE( app.semValue() == 0 );

        // Frame 3: nothing above, completes the stack
        std::vector<uint16_t> f3( 9, 0 );
        REQUIRE( app.processImage( f3.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_stack_frames_index == 0 );
        REQUIRE( app.m_photonCountedImage.sum() == Approx( 11 ) );
        REQUIRE( app.semValue() == 1 );

        // Calibration is untouched by counting
        REQUIRE( app.m_calibrationSet == true );
        REQUIRE( app.m_thresholdImage( 2, 2 ) == Approx( 82 ) );
    }

    SECTION( "Nstack of 1 posts every frame" )
    {
        runCalibration( app );
        app.m_stack_frames = 1;

        std::vector<uint16_t> f( 9, 1000 );
        app.processImage( f.data(), dev::shmimT() );
        app.processImage( f.data(), dev::shmimT() );

        REQUIRE( app.m_stack_frames_index == 0 );
        REQUIRE( app.semValue() == 2 );
        REQUIRE( app.m_photonCountedImage( 1, 1 ) == Approx( 2 ) );
    }
}

/// Verify loadImageIntoStream() copies the photon-counted image as float and resets it.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter loadImageIntoStream", "[photonCounter][framegrabber]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    photonCounter_test app( "pc" );
    REQUIRE( app.connectStream( 3 ) == 0 );
    REQUIRE( app.configureAcquisition() == 0 );

    for( uint32_t c = 0; c < 3; ++c )
    {
        for( uint32_t r = 0; r < 3; ++r )
        {
            app.m_photonCountedImage( r, c ) = r + 3 * c;
        }
    }

    std::vector<float> dest( 9, -1 );
    REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

    // Column-major output, same layout as the counted image
    for( size_t n = 0; n < 9; ++n )
    {
        REQUIRE( dest[n] == Approx( n ) );
    }

    REQUIRE( app.m_photonCountedImage.sum() == Approx( 0 ) );
}

/// Verify configureAcquisition() waits for the input stream, then copies its size with float output.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter configureAcquisition", "[photonCounter][framegrabber]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::configureAcquisition();
    photonCounter::startAcquisition();
    photonCounter::reconfig();
    photonCounter::fps();
    #endif
    // clang-format on

    photonCounter_test app( "pc" );

    SECTION( "no stream connected" )
    {
        // Sleeps 1 s
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.fgBaseT::m_width == 0 );
        REQUIRE( app.fgBaseT::m_height == 0 );
    }

    SECTION( "stream connected" )
    {
        app.smBaseT::m_width    = 64;
        app.smBaseT::m_height   = 32;
        app.smBaseT::m_dataType = _DATATYPE_UINT16;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.fgBaseT::m_width == 64 );
        REQUIRE( app.fgBaseT::m_height == 32 );
        REQUIRE( app.fgBaseT::m_dataType == _DATATYPE_FLOAT );
    }

    SECTION( "trivial hooks" )
    {
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( app.fps() == Approx( 250 ) );
    }
}

/// Verify acquireAndCheckValid() returns a frame when the semaphore is posted, and times out otherwise.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter acquireAndCheckValid", "[photonCounter][framegrabber]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::acquireAndCheckValid();
    #endif
    // clang-format on

    photonCounter_test app( "pc" );

    SECTION( "frame ready" )
    {
        sem_post( &app.m_smSemaphore );
        app.m_currImageTimestamp = { 0, 0 };

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( app.m_currImageTimestamp.tv_sec > 0 );
        REQUIRE( app.semValue() == 0 );
    }

    SECTION( "timeout" )
    {
        // Waits 1 s
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }
}

/// Verify the calibrate request callback starts a calibration.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter calibrate INDI callback", "[photonCounter][indi]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::newCallBack_m_indiP_calibrateToggle(pcf::IndiProperty());
    #endif
    // clang-format on

    photonCounter_test app( "pc" );
    app.m_calibrationSet = true;

    SECTION( "wrong name" )
    {
        REQUIRE( app.newCallBack_m_indiP_calibrateToggle(
                     switchProp( "pc", "wrong", "request", pcf::IndiElement::On ) ) == -1 );
        REQUIRE( app.m_calibrate == false );
    }

    SECTION( "no request element" )
    {
        REQUIRE( app.newCallBack_m_indiP_calibrateToggle(
                     switchProp( "pc", "calibrate", "other", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_calibrate == false );
        REQUIRE( app.m_calibrationSet == true );
    }

    SECTION( "request off does nothing" )
    {
        REQUIRE( app.newCallBack_m_indiP_calibrateToggle(
                     switchProp( "pc", "calibrate", "request", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_calibrate == false );
        REQUIRE( app.m_calibrationSet == true );
    }

    SECTION( "request on starts calibration" )
    {
        REQUIRE( app.newCallBack_m_indiP_calibrateToggle(
                     switchProp( "pc", "calibrate", "request", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_calibrate == true );
        REQUIRE( app.m_calibrationSet == false );
    }
}

/// Verify the quantile callback sets the threshold quantile from target or current.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter quantile INDI callback", "[photonCounter][indi]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::newCallBack_m_indiP_quantileCut(pcf::IndiProperty());
    #endif
    // clang-format on

    photonCounter_test app( "pc" );
    app.m_quantile_cut = 0.5;

    SECTION( "wrong name" )
    {
        REQUIRE( app.newCallBack_m_indiP_quantileCut( numberProp( "pc", "wrong", "target", 0.25 ) ) == -1 );
        REQUIRE( app.m_quantile_cut == Approx( 0.5 ) );
    }

    SECTION( "target" )
    {
        REQUIRE( app.newCallBack_m_indiP_quantileCut( numberProp( "pc", "quantile", "target", 0.25 ) ) == 0 );
        REQUIRE( app.m_quantile_cut == Approx( 0.25 ) );
    }

    SECTION( "current" )
    {
        REQUIRE( app.newCallBack_m_indiP_quantileCut( numberProp( "pc", "quantile", "current", 0.75 ) ) == 0 );
        REQUIRE( app.m_quantile_cut == Approx( 0.75 ) );
    }

    SECTION( "no value" )
    {
        REQUIRE( app.newCallBack_m_indiP_quantileCut( numberProp( "pc", "quantile", "other", 0.75 ) ) == 0 );
        REQUIRE( app.m_quantile_cut == Approx( 0.5 ) );
    }
}

/// Verify the nFrames callback sets the calibration length and reallocates the calibration cube.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter nFrames INDI callback", "[photonCounter][indi]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::newCallBack_m_indiP_calibrateSteps(pcf::IndiProperty());
    #endif
    // clang-format on

    photonCounter_test app( "pc" );
    app.calibration_steps = 10;
    REQUIRE( app.connectStream( 3 ) == 0 );
    REQUIRE( app.m_calibrationCube.planes() == 10 );

    SECTION( "wrong name" )
    {
        REQUIRE( app.newCallBack_m_indiP_calibrateSteps( numberProp( "pc", "wrong", "target", 7 ) ) == -1 );
        REQUIRE( app.calibration_steps == 10 );
    }

    SECTION( "target" )
    {
        app.m_calibrationCube.image( 0 )( 1, 1 ) = 5;

        REQUIRE( app.newCallBack_m_indiP_calibrateSteps( numberProp( "pc", "nFrames", "target", 7 ) ) == 0 );
        REQUIRE( app.calibration_steps == 7 );
        REQUIRE( app.m_calibrationCube.rows() == 3 );
        REQUIRE( app.m_calibrationCube.cols() == 3 );
        REQUIRE( app.m_calibrationCube.planes() == 7 );
        REQUIRE( app.m_calibrationCube.image( 0 )( 1, 1 ) == Approx( 0 ) );
    }

    SECTION( "current" )
    {
        REQUIRE( app.newCallBack_m_indiP_calibrateSteps( numberProp( "pc", "nFrames", "current", 20 ) ) == 0 );
        REQUIRE( app.calibration_steps == 20 );
        REQUIRE( app.m_calibrationCube.planes() == 20 );
    }

    SECTION( "no value" )
    {
        REQUIRE( app.newCallBack_m_indiP_calibrateSteps( numberProp( "pc", "nFrames", "other", 20 ) ) == 0 );
        REQUIRE( app.calibration_steps == 10 );
        REQUIRE( app.m_calibrationCube.planes() == 10 );
    }
}

/// Verify the stackNframes callback sets the number of frames to stack.
/**
 * \ingroup photonCounter_unit_test
 */
TEST_CASE( "photonCounter stackNframes INDI callback", "[photonCounter][indi]" )
{
    // clang-format off
    #ifdef PHOTONCOUNTER_TEST_DOXYGEN_REF
    photonCounter::newCallBack_m_indiP_stackFrames(pcf::IndiProperty());
    #endif
    // clang-format on

    photonCounter_test app( "pc" );
    app.m_stack_frames = 1;

    SECTION( "wrong name" )
    {
        REQUIRE( app.newCallBack_m_indiP_stackFrames( numberProp( "pc", "wrong", "target", 4 ) ) == -1 );
        REQUIRE( app.m_stack_frames == 1 );
    }

    SECTION( "target" )
    {
        REQUIRE( app.newCallBack_m_indiP_stackFrames( numberProp( "pc", "stackNframes", "target", 4 ) ) == 0 );
        REQUIRE( app.m_stack_frames == 4 );
    }

    SECTION( "current" )
    {
        REQUIRE( app.newCallBack_m_indiP_stackFrames( numberProp( "pc", "stackNframes", "current", 8 ) ) == 0 );
        REQUIRE( app.m_stack_frames == 8 );
    }

    SECTION( "no value" )
    {
        REQUIRE( app.newCallBack_m_indiP_stackFrames( numberProp( "pc", "stackNframes", "other", 8 ) ) == 0 );
        REQUIRE( app.m_stack_frames == 1 );
    }

    SECTION( "the device is not checked" )
    {
        REQUIRE( app.newCallBack_m_indiP_stackFrames( numberProp( "other", "stackNframes", "target", 6 ) ) == 0 );
        REQUIRE( app.m_stack_frames == 6 );
    }
}

} // namespace photonCounterTest

} // namespace libXWCTest
