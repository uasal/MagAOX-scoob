/** \file psfFit_test.cpp
 * \brief Catch2 tests for the psfFit app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup psfFit_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <semaphore.h>
#include <string>
#include <vector>

#include "../psfFit.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup psfFit_unit_test psfFit Unit Tests
 * \brief Unit tests for the psfFit application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `psfFit` unit tests.
/** \ingroup psfFit_unit_test
 */
namespace psfFitTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing psfFit internals.
class psfFit_test : public psfFit
{
  public:
    /// Construct a harness with the given device name.
    explicit psfFit_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        sem_init( &m_smSemaphore, 0, 0 );

        m_indiP_reset.setDevice( device );
        m_indiP_reset.setName( "reset" );
        m_indiP_statsTime.setDevice( device );
        m_indiP_statsTime.setName( "statsTime" );
        m_indiP_deltaPixThresh.setDevice( device );
        m_indiP_deltaPixThresh.setName( "deltaPixThresh" );
        m_indiP_sigmaMaxThreshUp.setDevice( device );
        m_indiP_sigmaMaxThreshUp.setName( "sigmaMaxThreshUp" );
        m_indiP_fractionMaxThreshDown.setDevice( device );
        m_indiP_fractionMaxThreshDown.setName( "fractionMaxThreshDown" );
        m_indiP_sigmaPixThresh.setDevice( device );
        m_indiP_sigmaPixThresh.setName( "sigmaPixThresh" );
        m_indiP_dx.setDevice( device );
        m_indiP_dx.setName( "dx" );
        m_indiP_dy.setDevice( device );
        m_indiP_dy.setName( "dy" );

        m_indiP_fpsSource.setDevice( "camtip" );
        m_indiP_fpsSource.setName( "fps" );
        m_indiP_shutter.setDevice( "camtip" );
        m_indiP_shutter.setName( "shutter" );
    }

    /// Destroy the harness, releasing the semaphore.
    ~psfFit_test() noexcept
    {
        sem_destroy( &m_smSemaphore );
    }

    using psfFit::newCallBack_m_indiP_deltaPixThresh;
    using psfFit::newCallBack_m_indiP_dx;
    using psfFit::newCallBack_m_indiP_dy;
    using psfFit::newCallBack_m_indiP_fractionMaxThreshDown;
    using psfFit::newCallBack_m_indiP_reset;
    using psfFit::newCallBack_m_indiP_sigmaMaxThreshUp;
    using psfFit::newCallBack_m_indiP_sigmaPixThresh;
    using psfFit::newCallBack_m_indiP_statsTime;
    using psfFit::setCallBack_m_indiP_fpsSource;
    using psfFit::setCallBack_m_indiP_shutter;

    /// Register the app configuration options.
    void setupConfigForTest()
    {
        setupConfig();
    }

    /// Read a config file and run loadConfigImpl().
    int loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        return loadConfigImpl( config );
    }

    /// Set up the main image stream geometry as the shmimMonitor would.
    void setImageStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        shmimMonitorT::m_width    = w;
        shmimMonitorT::m_height   = h;
        shmimMonitorT::m_dataType = dt;
    }

    /// Set up the dark stream geometry as the shmimMonitor would.
    void setDarkStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        darkShmimMonitorT::m_width    = w;
        darkShmimMonitorT::m_height   = h;
        darkShmimMonitorT::m_dataType = dt;
    }

    /// Set up the reference stream geometry as the shmimMonitor would.
    void setRefStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        refShmimMonitorT::m_width    = w;
        refShmimMonitorT::m_height   = h;
        refShmimMonitorT::m_dataType = dt;
    }

    /// Call the main image allocate().
    int allocateImage()
    {
        return allocate( dev::shmimT() );
    }

    /// Call the main image processImage().
    int processImageForTest( void *im /**< [in] frame data */ )
    {
        return processImage( im, dev::shmimT() );
    }

    /// Call the dark allocate().
    int allocateDark()
    {
        return allocate( darkShmimT() );
    }

    /// Call the dark processImage().
    int processDark( std::vector<float> &im /**< [in] dark frame */ )
    {
        return processImage( static_cast<void *>( im.data() ), darkShmimT() );
    }

    /// Call the reference allocate().
    int allocateRef()
    {
        return allocate( refShmimT() );
    }

    /// Call the reference processImage().
    int processRef( std::vector<float> &im /**< [in] reference frame */ )
    {
        return processImage( static_cast<void *>( im.data() ), refShmimT() );
    }

    /// Set the running statistics used by the quality checks.
    void setStats( float mnp /**< [in] mean max */,
                   float rmsp /**< [in] rms of max */,
                   float rmsx /**< [in] rms of x */,
                   float rmsy /**< [in] rms of y */ )
    {
        m_mnp  = mnp;
        m_rmsp = rmsp;
        m_rmsx = rmsx;
        m_rmsy = rmsy;
    }

    /// Set the previous coordinates.
    void setLast( float x /**< [in] last x */, float y /**< [in] last y */ )
    {
        m_last_x = x;
        m_last_y = y;
    }

    /// Wait on the fg semaphore without blocking.
    int trySem()
    {
        return sem_trywait( &m_smSemaphore );
    }

    /// Post to the fg semaphore.
    void postSem()
    {
        sem_post( &m_smSemaphore );
    }

    /// Main shmimMonitor restart flag.
    bool &restart()
    {
        return shmimMonitorT::m_restart;
    }

    /// frameGrabber reconfig flag.
    bool &reconfigFlag()
    {
        return frameGrabberT::m_reconfig;
    }

    /// frameGrabber shmim name.
    std::string fgShmimName()
    {
        return frameGrabberT::m_shmimName;
    }

    /// main shmimMonitor shmim name.
    std::string imShmimName()
    {
        return shmimMonitorT::m_shmimName;
    }

    /// dark shmimMonitor shmim name.
    std::string darkShmimName()
    {
        return darkShmimMonitorT::m_shmimName;
    }

    /// ref shmimMonitor shmim name.
    std::string refShmimName()
    {
        return refShmimMonitorT::m_shmimName;
    }

    /// frameGrabber width.
    uint32_t fgWidth()
    {
        return frameGrabberT::m_width;
    }

    /// frameGrabber height.
    uint32_t fgHeight()
    {
        return frameGrabberT::m_height;
    }

    /// frameGrabber data type.
    uint8_t fgDataType()
    {
        return frameGrabberT::m_dataType;
    }

    /// frameGrabber current image timestamp.
    timespec fgTimestamp()
    {
        return frameGrabberT::m_currImageTimestamp;
    }

    /// Access m_fpsDevice.
    std::string &fpsDevice()
    {
        return m_fpsDevice;
    }

    /// Access m_fpsProperty.
    std::string &fpsProperty()
    {
        return m_fpsProperty;
    }

    /// Access m_fpsElement.
    std::string &fpsElement()
    {
        return m_fpsElement;
    }

    /// Access m_fpsTol.
    float &fpsTol()
    {
        return m_fpsTol;
    }

    /// Access m_fps.
    float &fpsValue()
    {
        return m_fps;
    }

    /// Access m_shutterDevice.
    std::string &shutterDevice()
    {
        return m_shutterDevice;
    }

    /// Access m_shutterProperty.
    std::string &shutterProperty()
    {
        return m_shutterProperty;
    }

    /// Access m_shutterElement.
    std::string &shutterElement()
    {
        return m_shutterElement;
    }

    /// Access m_deltaPixThresh.
    float &deltaPixThresh()
    {
        return m_deltaPixThresh;
    }

    /// Access m_sigmaMaxThreshUp.
    float &sigmaMaxThreshUp()
    {
        return m_sigmaMaxThreshUp;
    }

    /// Access m_fractionMaxThreshDown.
    float &fractionMaxThreshDown()
    {
        return m_fractionMaxThreshDown;
    }

    /// Access m_sigmaPixThresh.
    float &sigmaPixThresh()
    {
        return m_sigmaPixThresh;
    }

    /// Access m_fitCircBuffMaxLength.
    uint16_t &fitCircBuffMaxLength()
    {
        return m_fitCircBuffMaxLength;
    }

    /// Access m_fitCircBuffMaxTime.
    float &fitCircBuffMaxTime()
    {
        return m_fitCircBuffMaxTime;
    }

    /// Access m_image.
    mx::improc::eigenImage<float> &image()
    {
        return m_image;
    }

    /// Access m_dark.
    mx::improc::eigenImage<float> &dark()
    {
        return m_dark;
    }

    /// Access m_ref.
    mx::improc::eigenImage<float> &ref()
    {
        return m_ref;
    }

    /// Access m_updated.
    bool &updated()
    {
        return m_updated;
    }

    /// Access m_skipped.
    bool &skipped()
    {
        return m_skipped;
    }

    /// Access m_shutter.
    bool &shutter()
    {
        return m_shutter;
    }

    /// Access m_x.
    float &x()
    {
        return m_x;
    }

    /// Access m_y.
    float &y()
    {
        return m_y;
    }

    /// Access m_last_x.
    float &lastX()
    {
        return m_last_x;
    }

    /// Access m_last_y.
    float &lastY()
    {
        return m_last_y;
    }

    /// Access m_dx.
    float &dx()
    {
        return m_dx;
    }

    /// Access m_dy.
    float &dy()
    {
        return m_dy;
    }

    /// Access m_mnp.
    float &mnp()
    {
        return m_mnp;
    }

    /// Access m_pcb.
    mx::sigproc::circularBufferIndex<float, cbIndexT> &pcb()
    {
        return m_pcb;
    }

    /// Access m_xcb.
    mx::sigproc::circularBufferIndex<float, cbIndexT> &xcb()
    {
        return m_xcb;
    }

    /// Access m_ycb.
    mx::sigproc::circularBufferIndex<float, cbIndexT> &ycb()
    {
        return m_ycb;
    }

    /// m_skipped_updating.
    uint64_t skippedUpdating() const
    {
        return m_skipped_updating;
    }

    /// m_skipped_DeltaFromMax.
    uint64_t skippedDeltaFromMax() const
    {
        return m_skipped_DeltaFromMax;
    }

    /// m_skipped_MaxRmsUp.
    uint64_t skippedMaxRmsUp() const
    {
        return m_skipped_MaxRmsUp;
    }

    /// m_skipped_MaxRmsDown.
    uint64_t skippedMaxRmsDown() const
    {
        return m_skipped_MaxRmsDown;
    }

    /// m_skipped_XRms.
    uint64_t skippedXRms() const
    {
        return m_skipped_XRms;
    }

    /// m_skipped_YRms.
    uint64_t skippedYRms() const
    {
        return m_skipped_YRms;
    }
};
/// \endcond

/// Build a sampled Gaussian PSF image, column-major with `w` rows and `h` columns.
std::vector<float> gaussImage( uint32_t w,     /**< [in] image width (rows) */
                               uint32_t h,     /**< [in] image height (columns) */
                               float    x0,    /**< [in] center row coordinate */
                               float    y0,    /**< [in] center column coordinate */
                               float    sigma, /**< [in] Gaussian width */
                               float    amp,   /**< [in] peak amplitude */
                               float    bkg    /**< [in] constant background */
)
{
    std::vector<float> im( w * h );
    for( uint32_t j = 0; j < h; ++j )
    {
        for( uint32_t i = 0; i < w; ++i )
        {
            float r2      = ( i - x0 ) * ( i - x0 ) + ( j - y0 ) * ( j - y0 );
            im[j * w + i] = bkg + amp * std::exp( -0.5 * r2 / ( sigma * sigma ) );
        }
    }
    return im;
}

/// Image size used by the fitting tests.
constexpr uint32_t c_imSize = 32;

/// Gaussian center x used by the fitting tests.
constexpr float c_x0 = 12.3;

/// Gaussian center y used by the fitting tests.
constexpr float c_y0 = 8.7;

/// Configure a harness with a 32x32 float stream, allocated with the given fps.
void setupFloatStream( psfFit_test &app, /**< [in,out] the harness */
                       float        fps /**< [in] the fps to allocate with */ )
{
    app.fpsValue() = fps;
    app.setImageStream( c_imSize, c_imSize, _DATATYPE_FLOAT );
    REQUIRE( app.allocateImage() == 0 );
}

/// Set running statistics so that the Gaussian test frame passes every quality check.
void setPassingStats( psfFit_test &app /**< [in,out] the harness */ )
{
    app.setStats( 100, 10, 1, 1 );
    app.setLast( 12, 9 );
}

/// Verify psfFit configuration defaults.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit configuration defaults", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::psfFit();
    psfFit::setupConfig();
    psfFit::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    psfFit_test app( "psffit" );
    app.setupConfigForTest();

    const std::string fname = "/tmp/psfFit_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );

    REQUIRE( app.fpsDevice() == "" );
    REQUIRE( app.fpsProperty() == "fps" );
    REQUIRE( app.fpsElement() == "current" );
    REQUIRE( app.fpsTol() == 0 );
    REQUIRE( app.fpsValue() == 0 );
    REQUIRE( app.shutterDevice() == "" );
    REQUIRE( app.shutterProperty() == "shutter" );
    REQUIRE( app.shutterElement() == "toggle" );
    REQUIRE( app.deltaPixThresh() == Approx( 8 ) );
    REQUIRE( app.sigmaMaxThreshUp() == Approx( 5 ) );
    REQUIRE( app.fractionMaxThreshDown() == Approx( 0.1 ) );
    REQUIRE( app.sigmaPixThresh() == Approx( 10 ) );
    REQUIRE( app.fitCircBuffMaxLength() == 50000 );
    REQUIRE( app.fitCircBuffMaxTime() == Approx( 5 ) );

    REQUIRE( app.imShmimName() == "psffit" );
    REQUIRE( app.darkShmimName() == "psffit" );
    REQUIRE( app.refShmimName() == "psffit" );
    REQUIRE( app.fgShmimName() == "psffit" );

    std::remove( fname.c_str() );
}

/// Verify psfFit configuration overrides.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit configuration overrides", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::setupConfig();
    psfFit::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    psfFit_test app( "psffit" );
    app.setupConfigForTest();

    const std::string fname = "/tmp/psfFit_test_overrides.conf";
    mx::app::writeConfigFile( fname,
                              { "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "shmimMonitor",
                                "darkShmim",
                                "refShmim",
                                "framegrabber" },
                              { "fpsDevice",
                                "fpsProperty",
                                "fpsElement",
                                "fpsTol",
                                "defaultFPS",
                                "shutterDevice",
                                "shutterProperty",
                                "shutterElement",
                                "deltaPixThresh",
                                "sigmaMaxThreshUp",
                                "fractionMaxThreshDown",
                                "sigmaPixThresh",
                                "shmimName",
                                "shmimName",
                                "shmimName",
                                "shmimName" },
                              { "camtip",
                                "framerate",
                                "value",
                                "0.5",
                                "1000",
                                "shuttip",
                                "state",
                                "onoff",
                                "4",
                                "3",
                                "0.25",
                                "6",
                                "camtip",
                                "camtip_dark",
                                "camtip_ref",
                                "camtip_fit" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );

    REQUIRE( app.fpsDevice() == "camtip" );
    REQUIRE( app.fpsProperty() == "framerate" );
    REQUIRE( app.fpsElement() == "value" );
    REQUIRE( app.fpsTol() == Approx( 0.5 ) );
    REQUIRE( app.fpsValue() == Approx( 1000 ) );
    REQUIRE( app.fps() == Approx( 1000 ) );
    REQUIRE( app.shutterDevice() == "shuttip" );
    REQUIRE( app.shutterProperty() == "state" );
    REQUIRE( app.shutterElement() == "onoff" );
    REQUIRE( app.deltaPixThresh() == Approx( 4 ) );
    REQUIRE( app.sigmaMaxThreshUp() == Approx( 3 ) );
    REQUIRE( app.fractionMaxThreshDown() == Approx( 0.25 ) );
    REQUIRE( app.sigmaPixThresh() == Approx( 6 ) );
    REQUIRE( app.imShmimName() == "camtip" );
    REQUIRE( app.darkShmimName() == "camtip_dark" );
    REQUIRE( app.refShmimName() == "camtip_ref" );
    REQUIRE( app.fgShmimName() == "camtip_fit" );

    std::remove( fname.c_str() );
}

/// Verify the main image allocate() sizes the image and fit circular buffers.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit allocate sizes the fit circular buffers", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::allocate(const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "zero fps disables the buffers" )
    {
        psfFit_test app( "psffit" );
        app.updated() = true;
        setupFloatStream( app, 0 );

        REQUIRE( app.image().rows() == c_imSize );
        REQUIRE( app.image().cols() == c_imSize );
        REQUIRE( app.image().sum() == 0 );
        REQUIRE( app.xcb().maxEntries() == 0 );
        REQUIRE( app.pcb().maxEntries() == 0 );
        REQUIRE( app.updated() == false );
    }

    SECTION( "buffer length is statsTime*fps + 1" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 100 );

        REQUIRE( app.xcb().maxEntries() == 501 );
        REQUIRE( app.ycb().maxEntries() == 501 );
        REQUIRE( app.pcb().maxEntries() == 501 );
    }

    SECTION( "buffer length is capped at the maximum length" )
    {
        psfFit_test app( "psffit" );
        app.fitCircBuffMaxLength() = 200;
        setupFloatStream( app, 100 );

        REQUIRE( app.xcb().maxEntries() == 200 );
    }

    SECTION( "buffer length has a minimum of 3" )
    {
        psfFit_test app( "psffit" );
        app.fitCircBuffMaxTime() = 0.01;
        setupFloatStream( app, 10 );

        REQUIRE( app.xcb().maxEntries() == 3 );
    }

    SECTION( "zero stats time disables the buffers" )
    {
        psfFit_test app( "psffit" );
        app.fitCircBuffMaxTime() = 0;
        setupFloatStream( app, 10 );

        REQUIRE( app.xcb().maxEntries() == 0 );
    }
}

/// Verify frames are used to fill the statistics buffers before statistics exist.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit processImage fills statistics before fitting", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "Gaussian centroid is buffered and the output is zero" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );

        std::vector<float> im = gaussImage( c_imSize, c_imSize, c_x0, c_y0, 2.0, 100, 0 );
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        REQUIRE( app.xcb().size() == 1 );
        REQUIRE( app.xcb()[0] == Approx( c_x0 ).margin( 1e-3 ) );
        REQUIRE( app.ycb()[0] == Approx( c_y0 ).margin( 1e-3 ) );
        REQUIRE( app.pcb()[0] == Approx( 100 * std::exp( -0.5 * ( 0.09 + 0.09 ) / 4.0 ) ).epsilon( 1e-4 ) );
        REQUIRE( app.lastX() == Approx( c_x0 ).margin( 1e-3 ) );
        REQUIRE( app.lastY() == Approx( c_y0 ).margin( 1e-3 ) );

        REQUIRE( app.skipped() == true );
        REQUIRE( app.updated() == true );
        REQUIRE( app.x() == 0 );
        REQUIRE( app.y() == 0 );

        // The framegrabber was signaled exactly once
        REQUIRE( app.trySem() == 0 );
        REQUIRE( app.trySem() == -1 );
    }

    SECTION( "skipped frames report the reference position" )
    {
        psfFit_test app( "psffit" );
        app.setRefStream( 2, 1, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocateRef() == 0 );
        std::vector<float> ref = { 15.5, 16.25 };
        REQUIRE( app.processRef( ref ) == 0 );
        REQUIRE( app.ref()( 0, 0 ) == Approx( 15.5 ) );
        REQUIRE( app.ref()( 1, 0 ) == Approx( 16.25 ) );

        setupFloatStream( app, 10 );

        std::vector<float> im = gaussImage( c_imSize, c_imSize, c_x0, c_y0, 2.0, 100, 0 );
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        REQUIRE( app.skipped() == true );
        REQUIRE( app.x() == Approx( 15.5 ) );
        REQUIRE( app.y() == Approx( 16.25 ) );
    }

    SECTION( "no buffering when the buffers are disabled" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 0 );

        std::vector<float> im = gaussImage( c_imSize, c_imSize, c_x0, c_y0, 2.0, 100, 0 );
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        REQUIRE( app.xcb().size() == 0 );
        REQUIRE( app.skipped() == true );
        REQUIRE( app.lastX() == Approx( c_x0 ).margin( 1e-3 ) );
    }
}

/// Verify a good frame is fit and reported once statistics exist.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit processImage fits a good Gaussian PSF", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::processImage(void *, const dev::shmimT &);
    psfFit::loadImageIntoStream(void *);
    #endif
    // clang-format on

    psfFit_test app( "psffit" );
    setupFloatStream( app, 10 );
    setPassingStats( app );

    std::vector<float> im = gaussImage( c_imSize, c_imSize, c_x0, c_y0, 2.0, 100, 0 );
    REQUIRE( app.processImageForTest( im.data() ) == 0 );

    REQUIRE( app.skipped() == false );
    REQUIRE( app.updated() == true );
    REQUIRE( app.x() == Approx( c_x0 ).margin( 1e-3 ) );
    REQUIRE( app.y() == Approx( c_y0 ).margin( 1e-3 ) );
    REQUIRE( app.lastX() == Approx( c_x0 ).margin( 1e-3 ) );
    REQUIRE( app.lastY() == Approx( c_y0 ).margin( 1e-3 ) );
    REQUIRE( app.xcb().size() == 1 );
    REQUIRE( app.skippedDeltaFromMax() == 0 );
    REQUIRE( app.skippedMaxRmsUp() == 0 );
    REQUIRE( app.skippedMaxRmsDown() == 0 );
    REQUIRE( app.skippedXRms() == 0 );
    REQUIRE( app.skippedYRms() == 0 );

    SECTION( "loadImageIntoStream applies the offsets to good frames" )
    {
        app.dx() = 0.5;
        app.dy() = -0.25;

        float dest[2] = { 0, 0 };
        REQUIRE( app.loadImageIntoStream( dest ) == 0 );
        REQUIRE( dest[0] == Approx( c_x0 - 0.5 ).margin( 1e-3 ) );
        REQUIRE( dest[1] == Approx( c_y0 + 0.25 ).margin( 1e-3 ) );
        REQUIRE( app.updated() == false );
    }

    SECTION( "a second frame before the framegrabber posts is counted as skipped-updating" )
    {
        std::vector<float> im2 = gaussImage( c_imSize, c_imSize, c_x0 + 1, c_y0, 2.0, 100, 0 );
        REQUIRE( app.processImageForTest( im2.data() ) == 0 );

        REQUIRE( app.skippedUpdating() == 1 );
        // the reported position is unchanged
        REQUIRE( app.x() == Approx( c_x0 ).margin( 1e-3 ) );
    }
}

/// Verify the frame quality checks reject bad frames.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit processImage quality checks", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    std::vector<float> im = gaussImage( c_imSize, c_imSize, c_x0, c_y0, 2.0, 100, 0 );

    SECTION( "closed shutter silently skips without buffering" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );
        setPassingStats( app );
        app.shutter() = true;

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.skipped() == true );
        REQUIRE( app.xcb().size() == 0 );
        REQUIRE( app.skippedDeltaFromMax() == 0 );
        REQUIRE( app.x() == 0 );
    }

    SECTION( "max pixel far from the center of light" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );
        setPassingStats( app );

        std::vector<float> two( c_imSize * c_imSize, 0.0f );
        two[2 * c_imSize + 2]   = 10;
        two[30 * c_imSize + 30] = 9;

        REQUIRE( app.processImageForTest( two.data() ) == 0 );
        REQUIRE( app.skippedDeltaFromMax() == 1 );
        REQUIRE( app.skipped() == true );
        REQUIRE( app.xcb().size() == 0 );
    }

    SECTION( "max pixel too far above the mean max" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );
        app.setStats( 50, 1, 1, 1 );
        app.setLast( 12, 9 );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.skippedMaxRmsUp() == 1 );
        REQUIRE( app.skipped() == true );
        REQUIRE( app.xcb().size() == 1 ); // still buffered
    }

    SECTION( "max pixel dropped too far below the mean max" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );
        app.setStats( 10000, 10, 1, 1 );
        app.setLast( 12, 9 );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.skippedMaxRmsDown() == 1 );
        REQUIRE( app.skippedMaxRmsUp() == 0 );
        REQUIRE( app.skipped() == true );
        REQUIRE( app.xcb().size() == 1 );
    }

    SECTION( "x jump too large" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );
        setPassingStats( app );
        app.setLast( 40, 9 );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.skippedXRms() == 1 );
        REQUIRE( app.skipped() == true );
        REQUIRE( app.lastX() == Approx( c_x0 ).margin( 1e-3 ) );
    }

    SECTION( "y jump too large" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );
        setPassingStats( app );
        app.setLast( 12, 40 );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.skippedYRms() == 1 );
        REQUIRE( app.skippedXRms() == 0 );
        REQUIRE( app.skipped() == true );
        REQUIRE( app.lastY() == Approx( c_y0 ).margin( 1e-3 ) );
    }
}

/// Verify dark subtraction and the uint16 input path.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit dark subtraction and data types", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::allocate(const darkShmimT &);
    psfFit::processImage(void *, const darkShmimT &);
    psfFit::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "a non-float dark is rejected" )
    {
        psfFit_test app( "psffit" );
        app.setDarkStream( c_imSize, c_imSize, _DATATYPE_UINT16 );
        REQUIRE( app.allocateDark() == -1 );
    }

    SECTION( "a matching float dark is subtracted" )
    {
        psfFit_test app( "psffit" );
        app.setDarkStream( c_imSize, c_imSize, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocateDark() == 0 );
        std::vector<float> dk( c_imSize * c_imSize, 10.0f );
        REQUIRE( app.processDark( dk ) == 0 );
        REQUIRE( app.dark()( 3, 4 ) == Approx( 10 ) );

        setupFloatStream( app, 10 );
        setPassingStats( app );

        std::vector<float> im = gaussImage( c_imSize, c_imSize, c_x0, c_y0, 2.0, 100, 10 );
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        REQUIRE( app.image()( 0, 0 ) == Approx( 0 ).margin( 1e-3 ) );
        REQUIRE( app.skipped() == false );
        REQUIRE( app.x() == Approx( c_x0 ).margin( 1e-3 ) );
        REQUIRE( app.y() == Approx( c_y0 ).margin( 1e-3 ) );
    }

    SECTION( "uint16 frames are converted, with and without a dark" )
    {
        psfFit_test app( "psffit" );
        app.fpsValue() = 10;
        app.setImageStream( c_imSize, c_imSize, _DATATYPE_UINT16 );
        REQUIRE( app.allocateImage() == 0 );
        setPassingStats( app );

        std::vector<float>    fim = gaussImage( c_imSize, c_imSize, 16, 20, 2.0, 1000, 0 );
        std::vector<uint16_t> im( fim.size() );
        for( size_t n = 0; n < fim.size(); ++n )
        {
            im[n] = static_cast<uint16_t>( std::lround( fim[n] ) );
        }
        app.setLast( 16, 20 );
        app.setStats( 1000, 10, 1, 1 );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.image()( 16, 20 ) == Approx( 1000 ) );
        REQUIRE( app.skipped() == false );
        REQUIRE( app.x() == Approx( 16 ).margin( 1e-2 ) );
        REQUIRE( app.y() == Approx( 20 ).margin( 1e-2 ) );
    }
}

/// Verify the frameGrabber interface implementation.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit frameGrabber interface", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::configureAcquisition();
    psfFit::startAcquisition();
    psfFit::acquireAndCheckValid();
    psfFit::loadImageIntoStream(void *);
    psfFit::reconfig();
    psfFit::fps();
    #endif
    // clang-format on

    SECTION( "configureAcquisition sets a 2x1 float output" )
    {
        psfFit_test app( "psffit" );
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.fgWidth() == 2 );
        REQUIRE( app.fgHeight() == 1 );
        REQUIRE( app.fgDataType() == _DATATYPE_FLOAT );
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( app.reconfig() == 0 );
    }

    SECTION( "fps reports the source fps" )
    {
        psfFit_test app( "psffit" );
        app.fpsValue() = 123.5;
        REQUIRE( app.fps() == Approx( 123.5 ) );
    }

    SECTION( "acquireAndCheckValid returns 0 after a processed frame" )
    {
        psfFit_test app( "psffit" );
        setupFloatStream( app, 10 );

        std::vector<float> im = gaussImage( c_imSize, c_imSize, c_x0, c_y0, 2.0, 100, 0 );
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( app.fgTimestamp().tv_sec > 0 );
    }

    SECTION( "acquireAndCheckValid returns 1 when signaled without an update" )
    {
        psfFit_test app( "psffit" );
        app.updated() = false;
        app.postSem();
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "loadImageIntoStream does not apply offsets to skipped frames" )
    {
        psfFit_test app( "psffit" );
        app.skipped() = true;
        app.updated() = true;
        app.x()       = 3.5;
        app.y()       = 4.5;
        app.dx()      = 1;
        app.dy()      = 1;

        float dest[2] = { 0, 0 };
        REQUIRE( app.loadImageIntoStream( dest ) == 0 );
        REQUIRE( dest[0] == Approx( 3.5 ) );
        REQUIRE( dest[1] == Approx( 4.5 ) );
        REQUIRE( app.updated() == false );
    }
}

/// Build a number property with a target element for a NEW callback.
pcf::IndiProperty numberRequest( const std::string &device, /**< [in] device name */
                                 const std::string &name,   /**< [in] property name */
                                 double             target /**< [in] target value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( "target", target ) );
    return ip;
}

/// Verify the NEW callbacks update the fitter settings.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit INDI new callbacks update settings", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::newCallBack_m_indiP_reset(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_statsTime(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_deltaPixThresh(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_sigmaMaxThreshUp(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_fractionMaxThreshDown(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_sigmaPixThresh(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_dx(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_dy(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "reset" )
    {
        psfFit_test app( "psffit" );

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "psffit" );
        ip.setName( "reset" );

        // wrong device
        pcf::IndiProperty bad = ip;
        bad.setDevice( "wrong" );
        REQUIRE( app.newCallBack_m_indiP_reset( bad ) == -1 );

        // no request element
        REQUIRE( app.newCallBack_m_indiP_reset( ip ) == -1 );

        ip.add( pcf::IndiElement( "request", pcf::IndiElement::Off ) );
        REQUIRE( app.newCallBack_m_indiP_reset( ip ) == 0 );
        REQUIRE( app.restart() == false );

        ip["request"].setSwitchState( pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_reset( ip ) == 0 );
        REQUIRE( app.restart() == true );
    }

    SECTION( "statsTime" )
    {
        psfFit_test app( "psffit" );

        REQUIRE( app.newCallBack_m_indiP_statsTime( numberRequest( "wrong", "statsTime", 2 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_statsTime( numberRequest( "psffit", "wrong", 2 ) ) == -1 );

        pcf::IndiProperty empty( pcf::IndiProperty::Number );
        empty.setDevice( "psffit" );
        empty.setName( "statsTime" );
        REQUIRE( app.newCallBack_m_indiP_statsTime( empty ) == -1 );
        REQUIRE( app.fitCircBuffMaxTime() == Approx( 5 ) );

        REQUIRE( app.newCallBack_m_indiP_statsTime( numberRequest( "psffit", "statsTime", 2.5 ) ) == 0 );
        REQUIRE( app.fitCircBuffMaxTime() == Approx( 2.5 ) );
        REQUIRE( app.restart() == true );
    }

    SECTION( "thresholds" )
    {
        psfFit_test app( "psffit" );

        REQUIRE( app.newCallBack_m_indiP_deltaPixThresh( numberRequest( "psffit", "deltaPixThresh", 3 ) ) == 0 );
        REQUIRE( app.deltaPixThresh() == Approx( 3 ) );

        REQUIRE( app.newCallBack_m_indiP_sigmaMaxThreshUp( numberRequest( "psffit", "sigmaMaxThreshUp", 7 ) ) == 0 );
        REQUIRE( app.sigmaMaxThreshUp() == Approx( 7 ) );

        REQUIRE( app.newCallBack_m_indiP_fractionMaxThreshDown(
                     numberRequest( "psffit", "fractionMaxThreshDown", 0.3 ) ) == 0 );
        REQUIRE( app.fractionMaxThreshDown() == Approx( 0.3 ) );

        REQUIRE( app.newCallBack_m_indiP_sigmaPixThresh( numberRequest( "psffit", "sigmaPixThresh", 12 ) ) == 0 );
        REQUIRE( app.sigmaPixThresh() == Approx( 12 ) );

        // mismatched requests leave values unchanged
        REQUIRE( app.newCallBack_m_indiP_deltaPixThresh( numberRequest( "wrong", "deltaPixThresh", 1 ) ) == -1 );
        REQUIRE( app.deltaPixThresh() == Approx( 3 ) );
        REQUIRE( app.newCallBack_m_indiP_sigmaPixThresh( numberRequest( "psffit", "wrong", 1 ) ) == -1 );
        REQUIRE( app.sigmaPixThresh() == Approx( 12 ) );
    }

    SECTION( "dx and dy" )
    {
        psfFit_test app( "psffit" );

        REQUIRE( app.newCallBack_m_indiP_dx( numberRequest( "psffit", "dx", -1.25 ) ) == 0 );
        REQUIRE( app.dx() == Approx( -1.25 ) );

        REQUIRE( app.newCallBack_m_indiP_dy( numberRequest( "psffit", "dy", 2.5 ) ) == 0 );
        REQUIRE( app.dy() == Approx( 2.5 ) );

        REQUIRE( app.newCallBack_m_indiP_dx( numberRequest( "psffit", "dy", 9 ) ) == -1 );
        REQUIRE( app.dx() == Approx( -1.25 ) );
    }
}

/// Verify the fps source SET callback.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit fps source set callback", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::setCallBack_m_indiP_fpsSource(const pcf::IndiProperty &);
    #endif
    // clang-format on

    psfFit_test app( "psffit" );
    app.fpsTol() = 0.5;

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "camtip" );
    ip.setName( "fps" );

    pcf::IndiProperty bad = ip;
    bad.setDevice( "wrong" );
    REQUIRE( app.setCallBack_m_indiP_fpsSource( bad ) == -1 );

    // no current element: ignored
    REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
    REQUIRE( app.fpsValue() == 0 );

    ip.add( pcf::IndiElement( "current", 1000.0 ) );
    REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
    REQUIRE( app.fpsValue() == Approx( 1000 ) );
    REQUIRE( app.restart() == true );
    REQUIRE( app.reconfigFlag() == true );

    // a change within tolerance is ignored
    app.restart()      = false;
    app.reconfigFlag() = false;
    ip["current"]      = 1000.25;
    REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
    REQUIRE( app.fpsValue() == Approx( 1000 ) );
    REQUIRE( app.restart() == false );
    REQUIRE( app.reconfigFlag() == false );

    // a change beyond tolerance is applied
    ip["current"] = 1001.0;
    REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
    REQUIRE( app.fpsValue() == Approx( 1001 ) );
    REQUIRE( app.restart() == true );
}

/// Verify the shutter SET callback.
/**
 * \ingroup psfFit_unit_test
 */
TEST_CASE( "psfFit shutter set callback", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::setCallBack_m_indiP_shutter(const pcf::IndiProperty &);
    #endif
    // clang-format on

    psfFit_test app( "psffit" );

    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "camtip" );
    ip.setName( "shutter" );

    pcf::IndiProperty bad = ip;
    bad.setName( "wrong" );
    REQUIRE( app.setCallBack_m_indiP_shutter( bad ) == -1 );

    // no toggle element: ignored
    REQUIRE( app.setCallBack_m_indiP_shutter( ip ) == 0 );
    REQUIRE( app.shutter() == false );

    ip.add( pcf::IndiElement( "toggle", pcf::IndiElement::Off ) );
    REQUIRE( app.setCallBack_m_indiP_shutter( ip ) == 0 );
    REQUIRE( app.shutter() == false );
    REQUIRE( app.restart() == false );

    ip["toggle"].setSwitchState( pcf::IndiElement::On );
    REQUIRE( app.setCallBack_m_indiP_shutter( ip ) == 0 );
    REQUIRE( app.shutter() == true );
    REQUIRE( app.restart() == true );

    app.restart() = false;
    ip["toggle"].setSwitchState( pcf::IndiElement::Off );
    REQUIRE( app.setCallBack_m_indiP_shutter( ip ) == 0 );
    REQUIRE( app.shutter() == false );
    REQUIRE( app.restart() == true );
}

} // namespace psfFitTest

} // namespace libXWCTest
