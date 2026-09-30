/** \file pwfsSlopeCalc_test.cpp
 * \brief Catch2 tests for the pwfsSlopeCalc app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup pwfsSlopeCalc_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <semaphore.h>
#include <string>
#include <vector>

#include "../pwfsSlopeCalc.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup pwfsSlopeCalc_unit_test pwfsSlopeCalc Unit Tests
 * \brief Unit tests for the pwfsSlopeCalc application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `pwfsSlopeCalc` unit tests.
/** \ingroup pwfsSlopeCalc_unit_test
 */
namespace pwfsSlopeCalcTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing pwfsSlopeCalc internals.
class pwfsSlopeCalc_test : public pwfsSlopeCalc
{
  public:
    /// The main image shmimMonitor base type.
    typedef dev::shmimMonitor<pwfsSlopeCalc> imMonitorT;

    /// The dark image shmimMonitor base type.
    typedef dev::shmimMonitor<pwfsSlopeCalc, darkShmimT> darkMonitorBaseT;

    /// The frameGrabber base type.
    typedef dev::frameGrabber<pwfsSlopeCalc> fgBaseT;

    /// Construct a harness with the given device name.
    explicit pwfsSlopeCalc_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        sem_init( &m_smSemaphore, 0, 0 );

        m_indiP_quad1.setDevice( "fitter" );
        m_indiP_quad1.setName( "quadrant1" );
        m_indiP_quad2.setDevice( "fitter" );
        m_indiP_quad2.setName( "quadrant2" );
        m_indiP_quad3.setDevice( "fitter" );
        m_indiP_quad3.setName( "quadrant3" );
        m_indiP_quad4.setDevice( "fitter" );
        m_indiP_quad4.setName( "quadrant4" );

        setPupils( 0, 0, 0, 0, 0, 0, 0, 0 );
    }

    /// Destroy the harness, releasing the semaphore.
    ~pwfsSlopeCalc_test() noexcept
    {
        sem_destroy( &m_smSemaphore );
    }

    /// Register the configuration options.
    void setupConfigForTest()
    {
        setupConfig();
    }

    /// Read a config file and run loadConfig().
    void loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        loadConfig();
    }

    /// Set all four pupil centers.
    void setPupils( float cx1 /**< [in] pupil 1 x center */,
                    float cy1 /**< [in] pupil 1 y center */,
                    float cx2 /**< [in] pupil 2 x center */,
                    float cy2 /**< [in] pupil 2 y center */,
                    float cx3 /**< [in] pupil 3 x center */,
                    float cy3 /**< [in] pupil 3 y center */,
                    float cx4 /**< [in] pupil 4 x center */,
                    float cy4 /**< [in] pupil 4 y center */ )
    {
        m_pupil_cx_1 = cx1;
        m_pupil_cy_1 = cy1;
        m_pupil_cx_2 = cx2;
        m_pupil_cy_2 = cy2;
        m_pupil_cx_3 = cx3;
        m_pupil_cy_3 = cy3;
        m_pupil_cx_4 = cx4;
        m_pupil_cy_4 = cy4;
    }

    /// Set the pupil diameter, buffer, and number of pupils.
    void setGeometry( int D /**< [in] pupil diameter */,
                      int buffer /**< [in] pupil edge buffer */,
                      int numPupils /**< [in] number of pupils (3 or 4) */ )
    {
        m_pupil_D      = D;
        m_pupil_buffer = buffer;
        m_numPupils    = numPupils;
    }

    /// Set the main image stream geometry as the shmimMonitor would.
    void setImageStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        imMonitorT::m_width    = w;
        imMonitorT::m_height   = h;
        imMonitorT::m_dataType = dt;
    }

    /// Set the dark stream geometry as the shmimMonitor would.
    void setDarkStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        darkMonitorBaseT::m_width    = w;
        darkMonitorBaseT::m_height   = h;
        darkMonitorBaseT::m_dataType = dt;
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
    int processDarkForTest( void *im /**< [in] dark frame data */ )
    {
        return processImage( im, darkShmimT() );
    }

    /// Call configureAcquisition().
    int configureAcquisitionForTest()
    {
        return configureAcquisition();
    }

    /// Call startAcquisition().
    int startAcquisitionForTest()
    {
        return startAcquisition();
    }

    /// Call acquireAndCheckValid().
    int acquireAndCheckValidForTest()
    {
        return acquireAndCheckValid();
    }

    /// Call loadImageIntoStream().
    int loadImageIntoStreamForTest( void *dest /**< [out] destination slopes buffer */ )
    {
        return loadImageIntoStream( dest );
    }

    /// Call reconfig().
    int reconfigForTest()
    {
        return reconfig();
    }

    /// Set the current source pointer as processImage() would.
    void setCurrSrc( void *src /**< [in] current frame */ )
    {
        m_curr_src = src;
    }

    /// Current source pointer.
    void *currSrc()
    {
        return m_curr_src;
    }

    /// Try to wait on the semaphore without blocking.
    int trySem()
    {
        return sem_trywait( &m_smSemaphore );
    }

    /// Post to the semaphore.
    void postSem()
    {
        sem_post( &m_smSemaphore );
    }

    /// frameGrabber reconfig flag.
    bool &reconfigFlag()
    {
        return m_reconfig;
    }

    /// frameGrabber width.
    uint32_t fgWidth()
    {
        return fgBaseT::m_width;
    }

    /// frameGrabber height.
    uint32_t fgHeight()
    {
        return fgBaseT::m_height;
    }

    /// frameGrabber data type.
    uint8_t fgDataType()
    {
        return fgBaseT::m_dataType;
    }

    /// frameGrabber shmim name.
    std::string fgShmimName()
    {
        return fgBaseT::m_shmimName;
    }

    /// frameGrabber current image timestamp.
    timespec fgTimestamp()
    {
        return fgBaseT::m_currImageTimestamp;
    }

    /// Main shmimMonitor shmim name.
    std::string imShmimName()
    {
        return imMonitorT::m_shmimName;
    }

    /// Dark shmimMonitor shmim name.
    std::string darkShmimName()
    {
        return darkMonitorBaseT::m_shmimName;
    }

    /// Dark shmimMonitor get-existing-first flag.
    bool darkGetExistingFirst()
    {
        return darkMonitorBaseT::m_getExistingFirst;
    }

    /// Dark image.
    mx::improc::eigenImage<float> &darkImage()
    {
        return m_darkImage;
    }

    /// Dark-set flag.
    bool &darkSet()
    {
        return m_darkSet;
    }

    /// Dark pixel getter.
    bool darkPixgetSet()
    {
        return ( dark_pixget != nullptr );
    }

    /// Fitter device name.
    std::string fitter()
    {
        return m_fitter;
    }

    /// Number of pupils.
    int numPupils()
    {
        return m_numPupils;
    }

    /// Pupil diameter.
    int pupilD()
    {
        return m_pupil_D;
    }

    /// Pupil buffer.
    int pupilBuffer()
    {
        return m_pupil_buffer;
    }

    /// Quadrant size.
    int quadSize()
    {
        return m_quadSize;
    }

    /// Pupil center x coordinate for pupil \p n (1-4).
    float cx( int n /**< [in] pupil number */ )
    {
        switch( n )
        {
        case 1:
            return m_pupil_cx_1;
        case 2:
            return m_pupil_cx_2;
        case 3:
            return m_pupil_cx_3;
        default:
            return m_pupil_cx_4;
        }
    }

    /// Pupil center y coordinate for pupil \p n (1-4).
    float cy( int n /**< [in] pupil number */ )
    {
        switch( n )
        {
        case 1:
            return m_pupil_cy_1;
        case 2:
            return m_pupil_cy_2;
        case 3:
            return m_pupil_cy_3;
        default:
            return m_pupil_cy_4;
        }
    }

    /// Fitter-reported pupil diameter for pupil \p n (1-4).
    float fitD( int n /**< [in] pupil number */ )
    {
        switch( n )
        {
        case 1:
            return m_pupil_D_1;
        case 2:
            return m_pupil_D_2;
        case 3:
            return m_pupil_D_3;
        default:
            return m_pupil_D_4;
        }
    }

    /// Quadrant starting x coordinate for pupil \p n (1-4).
    int sx( int n /**< [in] pupil number */ )
    {
        switch( n )
        {
        case 1:
            return m_pupil_sx_1;
        case 2:
            return m_pupil_sx_2;
        case 3:
            return m_pupil_sx_3;
        default:
            return m_pupil_sx_4;
        }
    }

    /// Quadrant starting y coordinate for pupil \p n (1-4).
    int sy( int n /**< [in] pupil number */ )
    {
        switch( n )
        {
        case 1:
            return m_pupil_sy_1;
        case 2:
            return m_pupil_sy_2;
        case 3:
            return m_pupil_sy_3;
        default:
            return m_pupil_sy_4;
        }
    }
};
/// \endcond

/// Fill one quadrant of a column-major uint16 image with a constant value.
static void fillQuad( std::vector<uint16_t> &im /**< [in,out] image, column-major, width \p w */,
                      uint32_t               w /**< [in] image width */,
                      int                    sx /**< [in] quadrant starting x */,
                      int                    sy /**< [in] quadrant starting y */,
                      int                    q /**< [in] quadrant size */,
                      uint16_t               val /**< [in] value to fill */ )
{
    for( int rr = 0; rr < q; ++rr )
    {
        for( int cc = 0; cc < q; ++cc )
        {
            im[( sx + rr ) + ( sy + cc ) * w] = val;
        }
    }
}

/// Build a 4-pupil 8x8 harness with D=2, buffer=1 (quadrant size 4) and non-overlapping quadrants.
static void setupFourPupil( pwfsSlopeCalc_test &app /**< [in,out] the harness to configure */ )
{
    app.setGeometry( 2, 1, 4 );
    app.setPupils( 2, 2, 6, 2, 2, 6, 6, 6 );
    app.setImageStream( 8, 8, _DATATYPE_UINT16 );
    app.allocateImage();
    app.configureAcquisitionForTest();
}

/// Verify default configuration values.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc configuration defaults", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::pwfsSlopeCalc();
    pwfsSlopeCalc::setupConfig();
    pwfsSlopeCalc::loadConfig();
    pwfsSlopeCalc::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    pwfsSlopeCalc_test app( "pwfsslopes" );

    REQUIRE( app.darkGetExistingFirst() == true );

    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/pwfsSlopeCalc_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.loadConfigFromFile( "/tmp/pwfsSlopeCalc_test_defaults.conf" );

    CHECK( app.fitter() == "" );
    CHECK( app.numPupils() == 4 );
    CHECK( app.pupilD() == 56 );
    CHECK( app.pupilBuffer() == 1 );
    CHECK( app.imShmimName() == "pwfsslopes" );
    CHECK( app.darkShmimName() == "pwfsslopes" );
    CHECK( app.fgShmimName() == "pwfsslopes" );

    std::remove( "/tmp/pwfsSlopeCalc_test_defaults.conf" );
}

/// Verify configuration overrides for the pupil geometry and streams.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc configuration overrides", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::setupConfig();
    pwfsSlopeCalc::loadConfig();
    pwfsSlopeCalc::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    pwfsSlopeCalc_test app( "pwfsslopes" );
    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/pwfsSlopeCalc_test_override.conf",
                              { "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "pupil",
                                "shmimMonitor",
                                "darkShmim",
                                "framegrabber" },
                              { "fitter",
                                "numPupils",
                                "D",
                                "buffer",
                                "cx_1",
                                "cy_1",
                                "cx_2",
                                "cy_2",
                                "cx_3",
                                "cy_3",
                                "cx_4",
                                "cy_4",
                                "shmimName",
                                "shmimName",
                                "shmimName" },
                              { "camwfs-fit",
                                "3",
                                "40",
                                "2",
                                "30.5",
                                "31.5",
                                "90",
                                "32",
                                "33",
                                "91",
                                "92",
                                "93",
                                "camwfs",
                                "camwfs_dark",
                                "camwfs_slopes" } );

    app.loadConfigFromFile( "/tmp/pwfsSlopeCalc_test_override.conf" );

    CHECK( app.fitter() == "camwfs-fit" );
    CHECK( app.numPupils() == 3 );
    CHECK( app.pupilD() == 40 );
    CHECK( app.pupilBuffer() == 2 );
    CHECK( app.cx( 1 ) == Approx( 30.5 ) );
    CHECK( app.cy( 1 ) == Approx( 31.5 ) );
    CHECK( app.cx( 2 ) == Approx( 90 ) );
    CHECK( app.cy( 2 ) == Approx( 32 ) );
    CHECK( app.cx( 3 ) == Approx( 33 ) );
    CHECK( app.cy( 3 ) == Approx( 91 ) );
    CHECK( app.cx( 4 ) == Approx( 92 ) );
    CHECK( app.cy( 4 ) == Approx( 93 ) );
    CHECK( app.imShmimName() == "camwfs" );
    CHECK( app.darkShmimName() == "camwfs_dark" );
    CHECK( app.fgShmimName() == "camwfs_slopes" );

    std::remove( "/tmp/pwfsSlopeCalc_test_override.conf" );
}

/// Verify configureAcquisition() computes the quadrant geometry and output frame size.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc configureAcquisition quadrant geometry", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::configureAcquisition();
    pwfsSlopeCalc::startAcquisition();
    pwfsSlopeCalc::reconfig();
    #endif
    // clang-format on

    SECTION( "no input stream yet returns -1" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        // image stream geometry is all zeros
        REQUIRE( app.configureAcquisitionForTest() == -1 );
    }

    SECTION( "quadrant starts and frame size from pupil geometry" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setGeometry( 56, 1, 4 );
        app.setPupils( 30, 31, 90, 32, 33, 91, 92, 93 );
        app.setImageStream( 120, 120, _DATATYPE_UINT16 );

        REQUIRE( app.configureAcquisitionForTest() == 0 );

        CHECK( app.quadSize() == 58 );
        CHECK( app.sx( 1 ) == 1 );
        CHECK( app.sy( 1 ) == 2 );
        CHECK( app.sx( 2 ) == 61 );
        CHECK( app.sy( 2 ) == 3 );
        CHECK( app.sx( 3 ) == 4 );
        CHECK( app.sy( 3 ) == 62 );
        CHECK( app.sx( 4 ) == 63 );
        CHECK( app.sy( 4 ) == 64 );

        CHECK( app.fgWidth() == 58 );
        CHECK( app.fgHeight() == 116 );
        CHECK( app.fgDataType() == _DATATYPE_FLOAT );

        CHECK( app.startAcquisitionForTest() == 0 );
        CHECK( app.reconfigForTest() == 0 );
    }

    SECTION( "buffer enlarges the quadrant" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setGeometry( 10, 3, 4 );
        app.setPupils( 20, 20, 40, 20, 20, 40, 40, 40 );
        app.setImageStream( 60, 60, _DATATYPE_UINT16 );

        REQUIRE( app.configureAcquisitionForTest() == 0 );
        CHECK( app.quadSize() == 16 );
        CHECK( app.sx( 1 ) == 12 );
        CHECK( app.sy( 4 ) == 32 );
        CHECK( app.fgWidth() == 16 );
        CHECK( app.fgHeight() == 32 );
    }
}

/// Verify the main image allocate() resets the dark and flags a reconfiguration.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc main image allocate", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::allocate(const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "dark size mismatch resizes and zeros the dark" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setImageStream( 8, 6, _DATATYPE_UINT16 );
        app.darkSet()      = true;
        app.reconfigFlag() = false;

        REQUIRE( app.allocateImage() == 0 );

        CHECK( app.darkImage().rows() == 8 );
        CHECK( app.darkImage().cols() == 6 );
        CHECK( app.darkImage().sum() == 0 );
        CHECK( app.darkSet() == false );
        CHECK( app.reconfigFlag() == true );
    }

    SECTION( "matching dark size keeps the dark" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setImageStream( 4, 4, _DATATYPE_UINT16 );
        app.setDarkStream( 4, 4, _DATATYPE_FLOAT );
        app.darkImage().resize( 4, 4 );
        app.darkImage().setConstant( 2.0f );
        app.darkSet()      = true;
        app.reconfigFlag() = false;

        REQUIRE( app.allocateImage() == 0 );

        CHECK( app.darkImage().sum() == Approx( 32.0 ) );
        CHECK( app.darkSet() == true );
        CHECK( app.reconfigFlag() == true );
    }
}

/// Verify the dark allocate() and processImage() load the dark image.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc dark stream allocate and process", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::allocate(const darkShmimT &);
    pwfsSlopeCalc::processImage(void *, const darkShmimT &);
    #endif
    // clang-format on

    SECTION( "uint16 dark is converted to float" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setDarkStream( 3, 2, _DATATYPE_UINT16 );
        app.darkSet() = true;

        REQUIRE( app.allocateDark() == 0 );
        CHECK( app.darkSet() == false );
        CHECK( app.darkPixgetSet() );
        CHECK( app.darkImage().rows() == 3 );
        CHECK( app.darkImage().cols() == 2 );

        std::vector<uint16_t> dark = { 1, 2, 3, 4, 5, 6 };
        REQUIRE( app.processDarkForTest( dark.data() ) == 0 );

        CHECK( app.darkSet() == true );
        CHECK( app.darkImage()( 0, 0 ) == Approx( 1 ) );
        CHECK( app.darkImage()( 2, 0 ) == Approx( 3 ) );
        CHECK( app.darkImage()( 0, 1 ) == Approx( 4 ) );
        CHECK( app.darkImage()( 2, 1 ) == Approx( 6 ) );
    }

    SECTION( "float dark is copied" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setDarkStream( 2, 2, _DATATYPE_FLOAT );

        REQUIRE( app.allocateDark() == 0 );

        std::vector<float> dark = { 0.5f, 1.5f, 2.5f, 3.5f };
        REQUIRE( app.processDarkForTest( dark.data() ) == 0 );
        CHECK( app.darkImage()( 1, 1 ) == Approx( 3.5 ) );
        CHECK( app.darkImage().sum() == Approx( 8.0 ) );
    }

    SECTION( "unsupported data type fails" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setDarkStream( 2, 2, 250 );

        REQUIRE( app.allocateDark() == -1 );
        CHECK_FALSE( app.darkPixgetSet() );
    }
}

/// Verify processImage() and acquireAndCheckValid() hand frames to the framegrabber through the semaphore.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc frame handoff via semaphore", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::processImage(void *, const dev::shmimT &);
    pwfsSlopeCalc::acquireAndCheckValid();
    #endif
    // clang-format on

    SECTION( "processImage stores the source and posts" )
    {
        pwfsSlopeCalc_test    app( "pwfsslopes" );
        std::vector<uint16_t> im( 16, 0 );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        CHECK( app.currSrc() == static_cast<void *>( im.data() ) );
        CHECK( app.trySem() == 0 );
        CHECK( app.trySem() != 0 );
    }

    SECTION( "acquireAndCheckValid returns 0 when a frame is posted" )
    {
        pwfsSlopeCalc_test    app( "pwfsslopes" );
        std::vector<uint16_t> im( 16, 0 );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.acquireAndCheckValidForTest() == 0 );

        timespec ts = app.fgTimestamp();
        CHECK( ts.tv_sec > 0 );
    }

    SECTION( "acquireAndCheckValid times out with no frame" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );

        // Waits ~1 second on the semaphore before timing out.
        REQUIRE( app.acquireAndCheckValidForTest() == 1 );
    }
}

/// Verify 4-pupil slope calculation on synthetic constant quadrants.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc 4-pupil slopes", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::loadImageIntoStream(void *);
    #endif
    // clang-format on

    SECTION( "constant quadrants, no dark" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        setupFourPupil( app );
        REQUIRE( app.quadSize() == 4 );

        std::vector<uint16_t> im( 64, 0 );
        fillQuad( im, 8, app.sx( 1 ), app.sy( 1 ), 4, 4 );
        fillQuad( im, 8, app.sx( 2 ), app.sy( 2 ), 4, 3 );
        fillQuad( im, 8, app.sx( 3 ), app.sy( 3 ), 4, 2 );
        fillQuad( im, 8, app.sx( 4 ), app.sy( 4 ), 4, 1 );
        app.setCurrSrc( im.data() );

        std::vector<float> slopes( app.fgWidth() * app.fgHeight(), -99 );
        REQUIRE( slopes.size() == 32 );
        REQUIRE( app.loadImageIntoStreamForTest( slopes.data() ) == 0 );

        // x = ((I1+I3)-(I2+I4))/<I>, y = ((I1+I2)-(I3+I4))/<I>, <I> = 10
        for( int rr = 0; rr < 4; ++rr )
        {
            for( int cc = 0; cc < 4; ++cc )
            {
                CHECK( slopes[rr + cc * 4] == Approx( 0.2 ) );
                CHECK( slopes[rr + ( cc + 4 ) * 4] == Approx( 0.4 ) );
            }
        }
    }

    SECTION( "dark is subtracted before the slopes" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        setupFourPupil( app );
        app.darkImage().setConstant( 1.0f );

        std::vector<uint16_t> im( 64, 0 );
        fillQuad( im, 8, app.sx( 1 ), app.sy( 1 ), 4, 4 );
        fillQuad( im, 8, app.sx( 2 ), app.sy( 2 ), 4, 3 );
        fillQuad( im, 8, app.sx( 3 ), app.sy( 3 ), 4, 2 );
        fillQuad( im, 8, app.sx( 4 ), app.sy( 4 ), 4, 1 );
        app.setCurrSrc( im.data() );

        std::vector<float> slopes( 32, -99 );
        REQUIRE( app.loadImageIntoStreamForTest( slopes.data() ) == 0 );

        // dark-subtracted: 3,2,1,0, <I> = 6
        CHECK( slopes[0] == Approx( 2.0 / 6.0 ) );
        CHECK( slopes[16] == Approx( 4.0 / 6.0 ) );
        CHECK( slopes[15] == Approx( 2.0 / 6.0 ) );
        CHECK( slopes[31] == Approx( 4.0 / 6.0 ) );
    }

    SECTION( "equal quadrants give zero slopes" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        setupFourPupil( app );

        std::vector<uint16_t> im( 64, 100 );
        app.setCurrSrc( im.data() );

        std::vector<float> slopes( 32, -99 );
        REQUIRE( app.loadImageIntoStreamForTest( slopes.data() ) == 0 );

        for( size_t n = 0; n < slopes.size(); ++n )
        {
            CHECK( slopes[n] == Approx( 0 ).margin( 1e-6 ) );
        }
    }

    SECTION( "a single bright pixel maps to the matching slope pixel" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        setupFourPupil( app );

        std::vector<uint16_t> im( 64, 1 );
        // quadrant 1 pixel (rr=1, cc=2)
        im[( app.sx( 1 ) + 1 ) + ( app.sy( 1 ) + 2 ) * 8] = 5;
        app.setCurrSrc( im.data() );

        std::vector<float> slopes( 32, -99 );
        REQUIRE( app.loadImageIntoStreamForTest( slopes.data() ) == 0 );

        // <I> = (15*4 + 8)/16 = 4.25
        double norm = 4.25;
        for( int rr = 0; rr < 4; ++rr )
        {
            for( int cc = 0; cc < 4; ++cc )
            {
                double expect = ( rr == 1 && cc == 2 ) ? 4.0 / norm : 0.0;
                CHECK( slopes[rr + cc * 4] == Approx( expect ).margin( 1e-6 ) );
                CHECK( slopes[rr + ( cc + 4 ) * 4] == Approx( expect ).margin( 1e-6 ) );
            }
        }
    }
}

/// Verify 3-pupil slope calculation on synthetic constant quadrants.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc 3-pupil slopes", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::loadImageIntoStream(void *);
    #endif
    // clang-format on

    pwfsSlopeCalc_test app( "pwfsslopes" );
    app.setGeometry( 2, 1, 3 );
    app.setPupils( 2, 2, 6, 2, 2, 6, 6, 6 );
    app.setImageStream( 8, 8, _DATATYPE_UINT16 );
    app.allocateImage();
    REQUIRE( app.configureAcquisitionForTest() == 0 );

    // pupil 1 -> I2, pupil 2 -> I3, pupil 3 -> I1; pupil 4 unused
    std::vector<uint16_t> im( 64, 0 );
    fillQuad( im, 8, app.sx( 1 ), app.sy( 1 ), 4, 4 );
    fillQuad( im, 8, app.sx( 2 ), app.sy( 2 ), 4, 2 );
    fillQuad( im, 8, app.sx( 3 ), app.sy( 3 ), 4, 6 );
    fillQuad( im, 8, app.sx( 4 ), app.sy( 4 ), 4, 1000 );
    app.setCurrSrc( im.data() );

    std::vector<float> slopes( 32, -99 );
    REQUIRE( app.loadImageIntoStreamForTest( slopes.data() ) == 0 );

    // x = sqrt(3)/2 (I2-I3)/<I>, y = (I1 - (I2+I3)/2)/<I>, <I> = 12
    double xExp = std::sqrt( 3.0 ) / 2.0 * 2.0 / 12.0;
    double yExp = 3.0 / 12.0;
    for( int rr = 0; rr < 4; ++rr )
    {
        for( int cc = 0; cc < 4; ++cc )
        {
            CHECK( slopes[rr + cc * 4] == Approx( xExp ) );
            CHECK( slopes[rr + ( cc + 4 ) * 4] == Approx( yExp ) );
        }
    }
}

/// Verify the pupil fitter SET callbacks update the pupil centers and diameters.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc fitter quadrant callbacks", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::setCallBack_m_indiP_quad1(const pcf::IndiProperty &);
    pwfsSlopeCalc::setCallBack_m_indiP_quad2(const pcf::IndiProperty &);
    pwfsSlopeCalc::setCallBack_m_indiP_quad3(const pcf::IndiProperty &);
    pwfsSlopeCalc::setCallBack_m_indiP_quad4(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong property name is rejected with the quadrant-specific code" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        pcf::IndiProperty  ip( pcf::IndiProperty::Number );
        ip.setDevice( "fitter" );
        ip.setName( "wrong" );

        CHECK( app.setCallBack_m_indiP_quad1( ip ) == -1 );
        CHECK( app.setCallBack_m_indiP_quad2( ip ) == -2 );
        CHECK( app.setCallBack_m_indiP_quad3( ip ) == -3 );
        CHECK( app.setCallBack_m_indiP_quad4( ip ) == -4 );
    }

    for( int n = 1; n <= 4; ++n )
    {
        DYNAMIC_SECTION( "quadrant " << n << " updates x, y and D" )
        {
            pwfsSlopeCalc_test app( "pwfsslopes" );
            app.reconfigFlag() = false;

            pcf::IndiProperty ip( pcf::IndiProperty::Number );
            ip.setDevice( "fitter" );
            ip.setName( "quadrant" + std::to_string( n ) );
            ip.add( pcf::IndiElement( "set-x", 10.0f + n ) );
            ip.add( pcf::IndiElement( "set-y", 20.0f + n ) );
            ip.add( pcf::IndiElement( "set-D", 30.0f + n ) );

            int rv = 0;
            if( n == 1 )
            {
                rv = app.setCallBack_m_indiP_quad1( ip );
            }
            else if( n == 2 )
            {
                rv = app.setCallBack_m_indiP_quad2( ip );
            }
            else if( n == 3 )
            {
                rv = app.setCallBack_m_indiP_quad3( ip );
            }
            else
            {
                rv = app.setCallBack_m_indiP_quad4( ip );
            }

            REQUIRE( rv == 0 );
            CHECK( app.cx( n ) == Approx( 10.0 + n ) );
            CHECK( app.cy( n ) == Approx( 20.0 + n ) );
            CHECK( app.fitD( n ) == Approx( 30.0 + n ) );
            CHECK( app.reconfigFlag() == true );

            // other quadrants untouched
            int other = ( n % 4 ) + 1;
            CHECK( app.cx( other ) == Approx( 0 ) );
            CHECK( app.cy( other ) == Approx( 0 ) );
        }
    }

    SECTION( "unchanged values do not trigger a reconfig" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setPupils( 5, 6, 0, 0, 0, 0, 0, 0 );
        app.reconfigFlag() = false;

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fitter" );
        ip.setName( "quadrant1" );
        ip.add( pcf::IndiElement( "set-x", 5.0f ) );
        ip.add( pcf::IndiElement( "set-y", 6.0f ) );

        REQUIRE( app.setCallBack_m_indiP_quad1( ip ) == 0 );
        CHECK( app.reconfigFlag() == false );
        CHECK( app.cx( 1 ) == Approx( 5 ) );
        CHECK( app.cy( 1 ) == Approx( 6 ) );
    }

    SECTION( "missing elements are ignored" )
    {
        pwfsSlopeCalc_test app( "pwfsslopes" );
        app.setPupils( 5, 6, 0, 0, 0, 0, 0, 0 );
        app.reconfigFlag() = false;

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "fitter" );
        ip.setName( "quadrant1" );
        ip.add( pcf::IndiElement( "set-y", 7.0f ) );

        REQUIRE( app.setCallBack_m_indiP_quad1( ip ) == 0 );
        CHECK( app.cx( 1 ) == Approx( 5 ) );
        CHECK( app.cy( 1 ) == Approx( 7 ) );
        CHECK( app.reconfigFlag() == true );
    }
}

/// Verify the fixed fps() accessor.
/**
 * \ingroup pwfsSlopeCalc_unit_test
 */
TEST_CASE( "pwfsSlopeCalc fps", "[pwfsSlopeCalc]" )
{
    // clang-format off
    #ifdef PWFSSLOPECALC_TEST_DOXYGEN_REF
    pwfsSlopeCalc::fps();
    #endif
    // clang-format on

    pwfsSlopeCalc_test app( "pwfsslopes" );
    CHECK( app.fps() == Approx( 250 ) );
}

} // namespace pwfsSlopeCalcTest

} // namespace libXWCTest
