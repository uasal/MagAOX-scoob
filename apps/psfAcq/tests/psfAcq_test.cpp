/** \file psfAcq_test.cpp
 * \brief Catch2 tests for the psfAcq app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup psfAcq_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "../psfAcq.hpp"

// Included after the app header so callback bodies stay live.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup psfAcq_unit_test psfAcq Unit Tests
 * \brief Unit tests for the psfAcq application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `psfAcq` unit tests.
/** \ingroup psfAcq_unit_test
 */
namespace psfAcqTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing psfAcq internals.
class psfAcq_test : public psfAcq
{
  public:
    /// Construct a harness with the given device name.
    explicit psfAcq_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_restartAcq, restart_acq );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_recordSeeing, record_seeing );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_acquire_star, acquire_star );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_seeing_star, seeing_star );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_flipAcqPresetName, flipacq, presetName );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_fpsSource, camacq, fps );
    }

    using psfAcq::newCallBack_m_indiP_acquire_star;
    using psfAcq::newCallBack_m_indiP_recordSeeing;
    using psfAcq::newCallBack_m_indiP_restartAcq;
    using psfAcq::newCallBack_m_indiP_seeing_star;
    using psfAcq::setCallBack_m_indiP_flipAcqPresetName;
    using psfAcq::setCallBack_m_indiP_fpsSource;

    using psfAcq::checkRecordTimes;
    using psfAcq::recordTelem;

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
    void setImageStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        shmimMonitorT::m_width    = w;
        shmimMonitorT::m_height   = h;
        shmimMonitorT::m_dataType = _DATATYPE_FLOAT;
    }

    /// Set up the dark stream geometry as the shmimMonitor would.
    void setDarkStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        darkShmimMonitorT::m_width    = w;
        darkShmimMonitorT::m_height   = h;
        darkShmimMonitorT::m_dataType = dt;
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
    int processDark( void *im /**< [in] dark frame */ )
    {
        return processImage( im, darkShmimT() );
    }

    /// Add a star directly to the tracked list.
    void addStar( std::size_t id /**< [in] star id */,
                  float       x /**< [in] x */,
                  float       y /**< [in] y */,
                  float       max /**< [in] peak */ )
    {
        Star s;
        s.id  = id;
        s.x   = x;
        s.y   = y;
        s.max = max;
        m_detectedStars.push_back( std::move( s ) );
    }

    /// Call relabelStarsByBrightness() holding the INDI mutex.
    void relabel()
    {
        { // mutex scope
            std::lock_guard<std::mutex> guard( m_indiMutex );
            relabelStarsByBrightness();
        }
    }

    /// Call removeStar() holding the INDI mutex.
    void remove( size_t index /**< [in] star index */ )
    {
        { // mutex scope
            std::lock_guard<std::mutex> guard( m_indiMutex );
            removeStar( index );
        }
    }

    /// Call resetAcq() holding the INDI mutex.
    void reset()
    {
        { // mutex scope
            std::lock_guard<std::mutex> guard( m_indiMutex );
            resetAcq();
        }
    }

    /// Whether a property with this unique key is registered.
    bool isRegistered( const std::string &key /**< [in] device.name key */ )
    {
        return m_indiNewCallBacks.count( key ) > 0;
    }

    /// Call emitStarTelemetry() with one sample.
    int emitOne( float x /**< [in] x */, float y /**< [in] y */ )
    {
        std::vector<starTelemSample> samples;
        samples.push_back( { x, y, 1, 2, 3 } );
        return emitStarTelemetry( samples );
    }

    /// Call emitStarTelemetry() with no samples.
    int emitNone()
    {
        return emitStarTelemetry( std::vector<starTelemSample>() );
    }

    /// Access m_detectedStars.
    std::vector<Star> &stars()
    {
        return m_detectedStars;
    }

    /// Access m_fpsSource.
    std::string &fpsSource()
    {
        return m_fpsSource;
    }

    /// Access m_max_loops.
    int &maxLoops()
    {
        return m_max_loops;
    }

    /// Access m_zero_area.
    int &zeroArea()
    {
        return m_zero_area;
    }

    /// Access m_threshold.
    float &threshold()
    {
        return m_threshold;
    }

    /// Access m_fwhm_threshold.
    float &fwhmThreshold()
    {
        return m_fwhm_threshold;
    }

    /// Access m_max_fwhm.
    float &maxFwhm()
    {
        return m_max_fwhm;
    }

    /// Access m_x_center.
    int &xCenter()
    {
        return m_x_center;
    }

    /// Access m_y_center.
    int &yCenter()
    {
        return m_y_center;
    }

    /// Access m_plate_scale.
    double plateScale() const
    {
        return m_plate_scale;
    }

    /// Access m_image.
    mx::improc::eigenImage<float> &image()
    {
        return m_image;
    }

    /// Access m_sm.
    mx::improc::eigenImage<float> &sm()
    {
        return m_sm;
    }

    /// Access m_dark.
    mx::improc::eigenImage<float> &dark()
    {
        return m_dark;
    }

    /// Access m_updated.
    bool &updated()
    {
        return m_updated;
    }

    /// Access m_num_stars.
    int &numStars()
    {
        return m_num_stars;
    }

    /// Access m_seeing.
    float &seeing()
    {
        return m_seeing;
    }

    /// Access m_seeing_star.
    int &seeingStar()
    {
        return m_seeing_star;
    }

    /// Access m_current_acq_star.
    int &currentAcqStar()
    {
        return m_current_acq_star;
    }

    /// Access m_acquire_star.
    int &acquireStar()
    {
        return m_acquire_star;
    }

    /// Access m_temp_acq_star.
    int &tempAcqStar()
    {
        return m_temp_acq_star;
    }

    /// Access m_acqQuitTime.
    double &acqQuitTime()
    {
        return m_acqQuitTime;
    }

    /// Access m_lastLoopExitTelemTime.
    double &lastLoopExitTelemTime()
    {
        return m_lastLoopExitTelemTime;
    }

    /// Access m_fps.
    float &fpsValue()
    {
        return m_fps;
    }

    /// Access m_flipAcqOutWasOn.
    bool &flipAcqOutWasOn()
    {
        return m_flipAcqOutWasOn;
    }

    /// Access m_flipAcqOutStateValid.
    bool &flipAcqOutStateValid()
    {
        return m_flipAcqOutStateValid;
    }

    /// Main shmimMonitor restart flag.
    bool &restart()
    {
        return shmimMonitorT::m_restart;
    }

    /// Main shmim name.
    std::string imShmimName()
    {
        return shmimMonitorT::m_shmimName;
    }

    /// Dark shmim name.
    std::string darkShmimName()
    {
        return darkShmimMonitorT::m_shmimName;
    }

    /// Dark getExistingFirst.
    bool darkGetExistingFirst()
    {
        return darkShmimMonitorT::m_getExistingFirst;
    }
};
/// \endcond

/// Image size used by the detection tests.
constexpr uint32_t c_imSize = 96;

/// Gaussian sigma of the synthetic stars.
constexpr float c_sigma = 2.5;

/// A synthetic star: center and peak amplitude.
struct synthStar
{
    float x;   ///< Row coordinate of the center.
    float y;   ///< Column coordinate of the center.
    float amp; ///< Peak amplitude.
};

/// Build a column-major `c_imSize` square image of Gaussian stars on Gaussian noise.
std::vector<float> starImage( const std::vector<synthStar> &stars, /**< [in] the stars to add */
                              float                         bkg,   /**< [in] constant background */
                              unsigned                      seed = 1234 /**< [in] noise seed */ )
{
    std::mt19937                    gen( seed );
    std::normal_distribution<float> noise( 0.0f, 1.0f );

    std::vector<float> im( c_imSize * c_imSize );
    for( uint32_t j = 0; j < c_imSize; ++j )
    {
        for( uint32_t i = 0; i < c_imSize; ++i )
        {
            float v = bkg + noise( gen );
            for( const auto &s : stars )
            {
                float r2 = ( i - s.x ) * ( i - s.x ) + ( j - s.y ) * ( j - s.y );
                v += s.amp * std::exp( -0.5f * r2 / ( c_sigma * c_sigma ) );
            }
            im[j * c_imSize + i] = v;
        }
    }
    return im;
}

/// Bright star used by the detection tests.
const synthStar c_starA{ 60.3, 40.6, 200 };

/// Medium star used by the detection tests.
const synthStar c_starB{ 30.2, 75.4, 100 };

/// Faint star used by the detection tests.
const synthStar c_starC{ 75.7, 70.1, 50 };

/// Expected FWHM of the synthetic stars.
const float c_fwhm = 2.0f * std::sqrt( 2.0f * std::log( 2.0f ) ) * c_sigma;

/// Allocate a harness for `c_imSize` float frames.
void setupStream( psfAcq_test &app /**< [in,out] the harness */ )
{
    app.setImageStream( c_imSize, c_imSize );
    REQUIRE( app.allocateImage() == 0 );
}

/// Verify psfAcq configuration defaults.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq configuration defaults", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::psfAcq();
    psfAcq::setupConfig();
    psfAcq::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    psfAcq_test app( "psfacq" );
    REQUIRE( app.darkGetExistingFirst() == true );

    app.setupConfigForTest();

    const std::string fname = "/tmp/psfAcq_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );

    REQUIRE( app.fpsSource() == "" );
    REQUIRE( app.maxLoops() == 5 );
    REQUIRE( app.zeroArea() == 8 );
    REQUIRE( app.threshold() == Approx( 7 ) );
    REQUIRE( app.fwhmThreshold() == Approx( 4 ) );
    REQUIRE( app.maxFwhm() == Approx( 40 ) );
    REQUIRE( app.xCenter() == 0 );
    REQUIRE( app.yCenter() == 0 );
    REQUIRE( app.plateScale() == Approx( 0.0795336 ) );
    REQUIRE( app.imShmimName() == "psfacq" );
    REQUIRE( app.darkShmimName() == "psfacq" );
    REQUIRE( app.darkGetExistingFirst() == true );

    std::remove( fname.c_str() );
}

/// Verify psfAcq configuration overrides.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq configuration overrides", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::setupConfig();
    psfAcq::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    psfAcq_test app( "psfacq" );
    app.setupConfigForTest();

    const std::string fname = "/tmp/psfAcq_test_overrides.conf";
    mx::app::writeConfigFile( fname,
                              { "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "fitter",
                                "acquisition",
                                "acquisition",
                                "shmimMonitor",
                                "darkShmim",
                                "darkShmim" },
                              { "fpsSource",
                                "max_loops",
                                "zero_area",
                                "threshold",
                                "fwhm_threshold",
                                "x_center",
                                "y_center",
                                "shmimName",
                                "shmimName",
                                "getExistingFirst" },
                              { "camacq", "3", "6", "9.5", "2.5", "256", "240", "camacq", "camacq_dark", "false" } );
    REQUIRE( app.loadConfigFromFile( fname ) == 0 );

    REQUIRE( app.fpsSource() == "camacq" );
    REQUIRE( app.maxLoops() == 3 );
    REQUIRE( app.zeroArea() == 6 );
    REQUIRE( app.threshold() == Approx( 9.5 ) );
    REQUIRE( app.fwhmThreshold() == Approx( 2.5 ) );
    REQUIRE( app.xCenter() == 256 );
    REQUIRE( app.yCenter() == 240 );
    REQUIRE( app.imShmimName() == "camacq" );
    REQUIRE( app.darkShmimName() == "camacq_dark" );
    REQUIRE( app.darkGetExistingFirst() == false );

    std::remove( fname.c_str() );
}

/// Verify the Star helper's property ownership and the distance helper.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq Star and calculateDistance helpers", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    Star::prop();
    Star::hasProp();
    Star::allocate();
    Star::deallocate();
    calculateDistance(float, float, float, float);
    #endif
    // clang-format on

    SECTION( "Star property lifecycle" )
    {
        Star s;
        REQUIRE( s.hasProp() == false );
        REQUIRE_THROWS_AS( s.prop(), std::runtime_error );

        s.allocate();
        REQUIRE( s.hasProp() == true );
        s.prop().setName( "star_0" );

        pcf::IndiProperty *p = &s.prop();
        s.allocate(); // no reallocation
        REQUIRE( &s.prop() == p );

        Star moved( std::move( s ) );
        REQUIRE( moved.hasProp() == true );
        REQUIRE( moved.prop().getName() == "star_0" );
        REQUIRE( &moved.prop() == p );

        moved.deallocate();
        REQUIRE( moved.hasProp() == false );
    }

    SECTION( "calculateDistance" )
    {
        REQUIRE( calculateDistance( 0, 0, 3, 4 ) == Approx( 5 ) );
        REQUIRE( calculateDistance( 1, 1, 1, 1 ) == Approx( 0 ).margin( 1e-7 ) );
        REQUIRE( calculateDistance( -2, 5, 4, -3 ) == Approx( 10 ) );
    }
}

/// Verify allocate() sizes the working images.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq allocate sizes the images", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::allocate(const dev::shmimT &);
    #endif
    // clang-format on

    psfAcq_test app( "psfacq" );
    app.updated() = true;
    app.setImageStream( 40, 50 );
    REQUIRE( app.allocateImage() == 0 );

    REQUIRE( app.image().rows() == 40 );
    REQUIRE( app.image().cols() == 50 );
    REQUIRE( app.image().sum() == 0 );
    REQUIRE( app.sm().rows() == 40 );
    REQUIRE( app.sm().cols() == 50 );
    REQUIRE( app.updated() == false );
}

/// Verify processImage() detects and fits a single star.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq processImage detects a single star", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::processImage(void *, const dev::shmimT &);
    psfAcq::relabelStarsByBrightness();
    psfAcq::registerStarProperty(Star &, std::size_t);
    #endif
    // clang-format on

    psfAcq_test app( "psfacq" );
    setupStream( app );

    std::vector<float> im = starImage( { c_starA }, 0 );
    REQUIRE( app.processImageForTest( im.data() ) == 0 );

    REQUIRE( app.updated() == true );
    REQUIRE( app.numStars() == 1 );
    REQUIRE( app.stars().size() == 1 );

    Star &s = app.stars()[0];
    REQUIRE( s.x == Approx( c_starA.x ).margin( 0.1 ) );
    REQUIRE( s.y == Approx( c_starA.y ).margin( 0.1 ) );
    REQUIRE( s.max == Approx( c_starA.amp ).epsilon( 0.03 ) );
    REQUIRE( s.fwhm == Approx( c_fwhm ).epsilon( 0.03 ) );
    REQUIRE( s.seeing == Approx( s.fwhm * app.plateScale() ) );
    REQUIRE( s.missedFrames == 0 );

    REQUIRE( app.seeing() == Approx( s.seeing ) );
    REQUIRE( app.seeingStar() == 0 );
    REQUIRE( app.currentAcqStar() == 0 );

    REQUIRE( s.hasProp() == true );
    REQUIRE( s.prop().getDevice() == "psfacq" );
    REQUIRE( s.prop().getName() == "star_0" );
    REQUIRE( s.prop()["x"].get<float>() == Approx( s.x ) );
    REQUIRE( s.prop()["y"].get<float>() == Approx( s.y ) );
    REQUIRE( s.prop()["peak"].get<float>() == Approx( s.max ) );
    REQUIRE( s.prop()["fwhm"].get<float>() == Approx( s.fwhm ) );
    REQUIRE( app.isRegistered( "psfacq.star_0" ) );
}

/// Verify processImage() detects multiple stars and orders them by brightness.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq processImage detects and ranks multiple stars", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::processImage(void *, const dev::shmimT &);
    psfAcq::relabelStarsByBrightness();
    #endif
    // clang-format on

    // Stars are listed faint-first so that ranking is exercised
    std::vector<float> im = starImage( { c_starC, c_starA, c_starB }, 0 );

    SECTION( "all stars within max_loops" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.numStars() == 3 );
        REQUIRE( app.stars().size() == 3 );

        REQUIRE( app.stars()[0].x == Approx( c_starA.x ).margin( 0.1 ) );
        REQUIRE( app.stars()[0].y == Approx( c_starA.y ).margin( 0.1 ) );
        REQUIRE( app.stars()[1].x == Approx( c_starB.x ).margin( 0.1 ) );
        REQUIRE( app.stars()[1].y == Approx( c_starB.y ).margin( 0.1 ) );
        REQUIRE( app.stars()[2].x == Approx( c_starC.x ).margin( 0.15 ) );
        REQUIRE( app.stars()[2].y == Approx( c_starC.y ).margin( 0.15 ) );

        REQUIRE( app.stars()[0].max > app.stars()[1].max );
        REQUIRE( app.stars()[1].max > app.stars()[2].max );

        for( size_t n = 0; n < 3; ++n )
        {
            REQUIRE( app.stars()[n].prop().getName() == "star_" + std::to_string( n ) );
            REQUIRE( app.isRegistered( "psfacq.star_" + std::to_string( n ) ) );
        }

        REQUIRE( app.seeing() == Approx( app.stars()[0].seeing ) );
    }

    SECTION( "max_loops limits the number of detections" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.maxLoops() = 2;

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.numStars() == 2 );
        REQUIRE( app.stars()[0].x == Approx( c_starA.x ).margin( 0.1 ) );
        REQUIRE( app.stars()[1].x == Approx( c_starB.x ).margin( 0.1 ) );
    }

    SECTION( "max_loops of zero detects nothing" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.maxLoops() = 0;

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.numStars() == 0 );
        REQUIRE( app.updated() == true );
        REQUIRE( app.seeingStar() == -1 );
    }

    SECTION( "a higher threshold rejects the faint star" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.threshold() = 75;

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.numStars() == 2 );
    }

    SECTION( "a large minimum fwhm rejects every star" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.fwhmThreshold() = 10;

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.numStars() == 0 );
    }
}

/// Verify stars are tracked across frames and dropped when they disappear.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq processImage tracks stars across frames", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::processImage(void *, const dev::shmimT &);
    psfAcq::removeStar(size_t);
    #endif
    // clang-format on

    psfAcq_test app( "psfacq" );
    setupStream( app );

    std::vector<float> im1 = starImage( { c_starA }, 0, 1 );
    REQUIRE( app.processImageForTest( im1.data() ) == 0 );
    REQUIRE( app.stars().size() == 1 );
    std::size_t id = app.stars()[0].id;

    // Move the star by a few pixels: the same tracked star is updated
    synthStar moved = c_starA;
    moved.x += 3;
    moved.y += 2;
    std::vector<float> im2 = starImage( { moved }, 0, 2 );
    REQUIRE( app.processImageForTest( im2.data() ) == 0 );
    REQUIRE( app.stars().size() == 1 );
    REQUIRE( app.stars()[0].id == id );
    REQUIRE( app.stars()[0].x == Approx( moved.x ).margin( 0.1 ) );
    REQUIRE( app.stars()[0].y == Approx( moved.y ).margin( 0.1 ) );

    // A far-away star is a new star, and the old one is dropped after one missed frame
    std::vector<float> im3 = starImage( { c_starB }, 0, 3 );
    REQUIRE( app.processImageForTest( im3.data() ) == 0 );
    REQUIRE( app.stars().size() == 1 );
    REQUIRE( app.stars()[0].id != id );
    REQUIRE( app.stars()[0].x == Approx( c_starB.x ).margin( 0.1 ) );
    REQUIRE( app.isRegistered( "psfacq.star_0" ) );

    // A frame with no stars drops everything
    std::vector<float> im4 = starImage( {}, 0, 4 );
    REQUIRE( app.processImageForTest( im4.data() ) == 0 );
    REQUIRE( app.stars().size() == 0 );
    REQUIRE( app.numStars() == 0 );
    REQUIRE( app.seeing() == 0 );
    REQUIRE( app.seeingStar() == -1 );
    REQUIRE( app.currentAcqStar() == -1 );
    REQUIRE( app.isRegistered( "psfacq.star_0" ) == false );
}

/// Verify processImage() early returns for unusable input.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq processImage rejects unusable input", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    std::vector<float> im = starImage( { c_starA }, 0 );

    SECTION( "null pointer" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        REQUIRE( app.processImageForTest( nullptr ) == -1 );
        REQUIRE( app.updated() == false );
    }

    SECTION( "not allocated" )
    {
        psfAcq_test app( "psfacq" );
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.updated() == false );
    }

    SECTION( "image smaller than the zero area" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.zeroArea() = 60;
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.updated() == false );
        REQUIRE( app.numStars() == 0 );
    }

    SECTION( "non-positive zero area" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.zeroArea() = 0;
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.updated() == false );
    }

    SECTION( "constant image has no noise estimate" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        std::vector<float> flat( c_imSize * c_imSize, 5.0f );
        REQUIRE( app.processImageForTest( flat.data() ) == 0 );
        REQUIRE( app.updated() == false );
        REQUIRE( app.image()( 10, 10 ) == Approx( 5 ) );
    }

    SECTION( "noise-only image has no detections" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        std::vector<float> noise = starImage( {}, 0 );
        REQUIRE( app.processImageForTest( noise.data() ) == 0 );
        REQUIRE( app.updated() == true );
        REQUIRE( app.numStars() == 0 );
    }

    SECTION( "acquisition pause skips frames" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.acqQuitTime() = mx::sys::get_curr_time();
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.updated() == false );
        REQUIRE( app.numStars() == 0 );
    }
}

/// Verify dark allocation, accumulation and subtraction.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq dark handling", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::allocate(const darkShmimT &);
    psfAcq::processImage(void *, const darkShmimT &);
    psfAcq::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "a non-float dark is rejected" )
    {
        psfAcq_test app( "psfacq" );
        app.setDarkStream( c_imSize, c_imSize, _DATATYPE_UINT16 );
        REQUIRE( app.allocateDark() == -1 );
    }

    SECTION( "a null dark pointer is rejected" )
    {
        psfAcq_test app( "psfacq" );
        app.setDarkStream( c_imSize, c_imSize, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocateDark() == 0 );
        REQUIRE( app.processDark( nullptr ) == -1 );
    }

    SECTION( "dark frames accumulate into the dark" )
    {
        psfAcq_test app( "psfacq" );
        app.setDarkStream( 4, 4, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocateDark() == 0 );
        REQUIRE( app.dark().rows() == 4 );
        REQUIRE( app.dark().sum() == 0 );

        std::vector<float> dk( 16, 2.0f );
        REQUIRE( app.processDark( dk.data() ) == 0 );
        REQUIRE( app.dark()( 1, 1 ) == Approx( 2 ) );

        // Current behavior: processImage(darkShmimT) adds to, rather than replaces, the dark
        REQUIRE( app.processDark( dk.data() ) == 0 );
        REQUIRE( app.dark()( 1, 1 ) == Approx( 4 ) );
    }

    SECTION( "a matching dark is subtracted before detection" )
    {
        psfAcq_test app( "psfacq" );
        app.setDarkStream( c_imSize, c_imSize, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocateDark() == 0 );
        std::vector<float> dk( c_imSize * c_imSize, 10.0f );
        REQUIRE( app.processDark( dk.data() ) == 0 );

        setupStream( app );
        std::vector<float> im  = starImage( { c_starA }, 10 );
        std::vector<float> ref = starImage( { c_starA }, 0 );
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        REQUIRE( app.image()( 3, 5 ) == Approx( ref[5 * c_imSize + 3] ).margin( 1e-4 ) );
        REQUIRE( app.numStars() == 1 );
        REQUIRE( app.stars()[0].max == Approx( c_starA.amp ).epsilon( 0.03 ) );
    }
}

/// Verify selecting a star to acquire resets the tracked stars and pauses acquisition.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq star acquisition", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::processImage(void *, const dev::shmimT &);
    psfAcq::resetAcq();
    #endif
    // clang-format on

    std::vector<float> im = starImage( { c_starA, c_starB }, 0 );

    SECTION( "a valid star index triggers acquisition" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );
        app.xCenter() = 48;
        app.yCenter() = 48;

        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.numStars() == 2 );

        app.acquireStar() = 1;
        // sendNewProperty fails without an INDI driver, but processImage still succeeds
        REQUIRE( app.processImageForTest( im.data() ) == 0 );

        REQUIRE( app.tempAcqStar() == 1 );
        REQUIRE( app.acquireStar() == -1 );
        REQUIRE( app.acqQuitTime() > 0 );
        REQUIRE( app.stars().size() == 0 );
        REQUIRE( app.numStars() == 0 );
        REQUIRE( app.seeing() == 0 );
        REQUIRE( app.seeingStar() == -1 );
        REQUIRE( app.isRegistered( "psfacq.star_0" ) == false );

        // The next frame is skipped during the pause
        app.updated() = false;
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.updated() == false );
        REQUIRE( app.numStars() == 0 );
    }

    SECTION( "an out of range star index is cleared" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );

        app.acquireStar() = 5;
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.acquireStar() == -1 );
        REQUIRE( app.numStars() == 2 );
        REQUIRE( app.acqQuitTime() == 0 );

        app.acquireStar() = -4;
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( app.acquireStar() == -1 );
        REQUIRE( app.numStars() == 2 );
    }
}

/// Verify brightness ranking, relabeling and removal of tracked stars.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq star bookkeeping", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::relabelStarsByBrightness();
    psfAcq::registerStarProperty(Star &, std::size_t);
    psfAcq::unregisterStarProperty(Star &);
    psfAcq::removeStar(size_t);
    psfAcq::resetAcq();
    #endif
    // clang-format on

    psfAcq_test app( "psfacq" );
    app.addStar( 0, 10, 10, 50 );
    app.addStar( 1, 20, 20, 150 );
    app.addStar( 2, 30, 30, 100 );
    app.addStar( 3, 40, 40, 100 ); // ties with id 2

    app.relabel();

    REQUIRE( app.stars().size() == 4 );
    REQUIRE( app.stars()[0].id == 1 );
    REQUIRE( app.stars()[1].id == 2 );
    REQUIRE( app.stars()[2].id == 3 );
    REQUIRE( app.stars()[3].id == 0 );

    for( size_t n = 0; n < 4; ++n )
    {
        REQUIRE( app.stars()[n].hasProp() );
        REQUIRE( app.stars()[n].prop().getName() == "star_" + std::to_string( n ) );
        REQUIRE( app.isRegistered( "psfacq.star_" + std::to_string( n ) ) );
    }
    REQUIRE( app.stars()[0].prop()["peak"].get<float>() == Approx( 150 ) );

    // Labels already match: properties are not recreated
    pcf::IndiProperty *p0 = &app.stars()[0].prop();
    app.relabel();
    REQUIRE( &app.stars()[0].prop() == p0 );

    // Out-of-range removal is a no-op
    app.remove( 10 );
    REQUIRE( app.stars().size() == 4 );

    // Removing the brightest star and relabeling shifts the labels down
    app.remove( 0 );
    REQUIRE( app.stars().size() == 3 );
    REQUIRE( app.isRegistered( "psfacq.star_0" ) == false );
    app.relabel();
    REQUIRE( app.stars()[0].id == 2 );
    REQUIRE( app.stars()[0].prop().getName() == "star_0" );
    REQUIRE( app.isRegistered( "psfacq.star_0" ) );
    REQUIRE( app.isRegistered( "psfacq.star_2" ) );
    REQUIRE( app.isRegistered( "psfacq.star_3" ) == false );

    // Reset removes everything
    app.numStars()   = 3;
    app.seeing()     = 1.5;
    app.seeingStar() = 0;
    app.reset();
    REQUIRE( app.stars().size() == 0 );
    REQUIRE( app.numStars() == 0 );
    REQUIRE( app.seeing() == 0 );
    REQUIRE( app.seeingStar() == -1 );
    REQUIRE( app.currentAcqStar() == -1 );
    REQUIRE( app.isRegistered( "psfacq.star_0" ) == false );
}

/// Verify the telem_psfacq message fields and the telemetry emission paths.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq telemetry", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    MagAOX::logger::telem_psfacq::messageT;
    psfAcq::emitStarTelemetry(const std::vector<starTelemSample> &);
    psfAcq::recordTelem(const telem_psfacq *);
    psfAcq::checkRecordTimes();
    #endif
    // clang-format on

    SECTION( "telem_psfacq message fields" )
    {
        MagAOX::logger::telem_psfacq::messageT msg( 2, 3, 10.5, 20.25, 150, 5.5, 0.44 );
        void                                  *buf = msg.builder.GetBufferPointer();

        REQUIRE( MagAOX::logger::telem_psfacq::star_no( buf ) == 2 );
        REQUIRE( MagAOX::logger::telem_psfacq::num_stars( buf ) == 3 );
        REQUIRE( MagAOX::logger::telem_psfacq::x_pos( buf ) == Approx( 10.5 ) );
        REQUIRE( MagAOX::logger::telem_psfacq::y_pos( buf ) == Approx( 20.25 ) );
        REQUIRE( MagAOX::logger::telem_psfacq::m_pix( buf ) == Approx( 150 ) );
        REQUIRE( MagAOX::logger::telem_psfacq::fwhm( buf ) == Approx( 5.5 ) );
        REQUIRE( MagAOX::logger::telem_psfacq::seeing( buf ) == Approx( 0.44 ) );

        std::string str = MagAOX::logger::telem_psfacq::msgString( buf, msg.builder.GetSize() );
        REQUIRE( str.find( "star_no: 2/3" ) != std::string::npos );
    }

    SECTION( "emitStarTelemetry and recordTelem" )
    {
        psfAcq_test app( "psfacq" );

        REQUIRE( app.checkRecordTimes() == 0 );

        MagAOX::logger::telem_psfacq::lastRecord = { 0, 0 };
        REQUIRE( app.emitNone() == 0 );
        REQUIRE( MagAOX::logger::telem_psfacq::lastRecord.tv_sec == 0 );

        REQUIRE( app.recordTelem( nullptr ) == 0 ); // no stars: nothing recorded
        REQUIRE( MagAOX::logger::telem_psfacq::lastRecord.tv_sec == 0 );

        REQUIRE( app.emitOne( 1, 2 ) == 0 );
        REQUIRE( MagAOX::logger::telem_psfacq::lastRecord.tv_sec > 0 );

        MagAOX::logger::telem_psfacq::lastRecord = { 0, 0 };
        app.addStar( 0, 10, 10, 50 );
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( MagAOX::logger::telem_psfacq::lastRecord.tv_sec > 0 );
    }

    SECTION( "processImage emits telemetry at most once per second" )
    {
        psfAcq_test app( "psfacq" );
        setupStream( app );

        std::vector<float> im = starImage( { c_starA }, 0 );

        MagAOX::logger::telem_psfacq::lastRecord = { 0, 0 };
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( MagAOX::logger::telem_psfacq::lastRecord.tv_sec > 0 );
        REQUIRE( app.lastLoopExitTelemTime() > 0 );

        MagAOX::logger::telem_psfacq::lastRecord = { 0, 0 };
        REQUIRE( app.processImageForTest( im.data() ) == 0 );
        REQUIRE( MagAOX::logger::telem_psfacq::lastRecord.tv_sec == 0 );
    }
}

/// Verify NEW callback device/name validation with the standard macros.
/**
 * \ingroup psfAcq_unit_test
 */
SCENARIO( "psfAcq INDI callbacks validate device and property names", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::newCallBack_m_indiP_restartAcq(const pcf::IndiProperty &);
    psfAcq::newCallBack_m_indiP_recordSeeing(const pcf::IndiProperty &);
    psfAcq::setCallBack_m_indiP_flipAcqPresetName(const pcf::IndiProperty &);
    #endif
    // clang-format on

    XWCTEST_INDI_ARBNEW_CALLBACK( psfAcq, newCallBack_m_indiP_restartAcq, restart_acq );
    XWCTEST_INDI_ARBNEW_CALLBACK( psfAcq, newCallBack_m_indiP_recordSeeing, record_seeing );
    XWCTEST_INDI_SET_CALLBACK( psfAcq, m_indiP_flipAcqPresetName, flipacq, presetName );
}

/// Verify the restart_acq and record_seeing switch callbacks.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq restart_acq and record_seeing callbacks", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::newCallBack_m_indiP_restartAcq(const pcf::IndiProperty &);
    psfAcq::newCallBack_m_indiP_recordSeeing(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "restart_acq" )
    {
        psfAcq_test app( "psfacq" );
        app.addStar( 0, 10, 10, 50 );
        app.relabel();
        app.numStars() = 1;

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "psfacq" );
        ip.setName( "restart_acq" );
        ip.add( pcf::IndiElement( "request", pcf::IndiElement::Off ) );

        REQUIRE( app.newCallBack_m_indiP_restartAcq( ip ) == 0 );
        REQUIRE( app.stars().size() == 1 );

        ip["request"].setSwitchState( pcf::IndiElement::UnknownSwitchState );
        REQUIRE( app.newCallBack_m_indiP_restartAcq( ip ) == -1 );
        REQUIRE( app.stars().size() == 1 );

        ip["request"].setSwitchState( pcf::IndiElement::On );
        REQUIRE( app.newCallBack_m_indiP_restartAcq( ip ) == 0 );
        REQUIRE( app.stars().size() == 0 );
        REQUIRE( app.numStars() == 0 );
        REQUIRE( app.isRegistered( "psfacq.star_0" ) == false );
    }

    SECTION( "record_seeing" )
    {
        psfAcq_test app( "psfacq" );

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "psfacq" );
        ip.setName( "record_seeing" );
        ip.add( pcf::IndiElement( "toggle", pcf::IndiElement::On ) );

        REQUIRE( app.newCallBack_m_indiP_recordSeeing( ip ) == 0 );

        ip["toggle"].setSwitchState( pcf::IndiElement::Off );
        REQUIRE( app.newCallBack_m_indiP_recordSeeing( ip ) == 0 );

        ip["toggle"].setSwitchState( pcf::IndiElement::UnknownSwitchState );
        REQUIRE( app.newCallBack_m_indiP_recordSeeing( ip ) == -1 );
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

/// Verify the acquire_star and seeing_star number callbacks.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq acquire_star and seeing_star callbacks", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::newCallBack_m_indiP_acquire_star(const pcf::IndiProperty &);
    psfAcq::newCallBack_m_indiP_seeing_star(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "acquire_star" )
    {
        psfAcq_test app( "psfacq" );

        REQUIRE( app.newCallBack_m_indiP_acquire_star( numberRequest( "psfacq", "wrong", 2 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_acquire_star( numberRequest( "wrong", "acquire_star", 2 ) ) == -1 );
        REQUIRE( app.acquireStar() == -1 );

        pcf::IndiProperty empty( pcf::IndiProperty::Number );
        empty.setDevice( "psfacq" );
        empty.setName( "acquire_star" );
        REQUIRE( app.newCallBack_m_indiP_acquire_star( empty ) == -1 );

        REQUIRE( app.newCallBack_m_indiP_acquire_star( numberRequest( "psfacq", "acquire_star", 2 ) ) == 0 );
        REQUIRE( app.acquireStar() == 2 );
    }

    SECTION( "seeing_star forces automatic selection" )
    {
        psfAcq_test app( "psfacq" );

        REQUIRE( app.newCallBack_m_indiP_seeing_star( numberRequest( "psfacq", "wrong", 2 ) ) == -1 );

        app.seeingStar() = 3;
        app.numStars()   = 0;
        REQUIRE( app.newCallBack_m_indiP_seeing_star( numberRequest( "psfacq", "seeing_star", 2 ) ) == 0 );
        REQUIRE( app.seeingStar() == -1 );

        app.numStars() = 2;
        REQUIRE( app.newCallBack_m_indiP_seeing_star( numberRequest( "psfacq", "seeing_star", 1 ) ) == 0 );
        REQUIRE( app.seeingStar() == 0 );
    }
}

/// Verify the fps source and flipacq preset SET callbacks.
/**
 * \ingroup psfAcq_unit_test
 */
TEST_CASE( "psfAcq fps source and flipacq set callbacks", "[psfAcq]" )
{
    // clang-format off
    #ifdef PSFACQ_TEST_DOXYGEN_REF
    psfAcq::setCallBack_m_indiP_fpsSource(const pcf::IndiProperty &);
    psfAcq::setCallBack_m_indiP_flipAcqPresetName(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "fps source" )
    {
        psfAcq_test app( "psfacq" );

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "camacq" );
        ip.setName( "wrong" );
        ip.add( pcf::IndiElement( "current", 30.0 ) );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == -1 );
        REQUIRE( app.fpsValue() == 0 );

        ip.setName( "fps" );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        REQUIRE( app.fpsValue() == Approx( 30 ) );
        REQUIRE( app.restart() == true );

        app.restart() = false;
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        REQUIRE( app.restart() == false );

        pcf::IndiProperty nocurrent( pcf::IndiProperty::Number );
        nocurrent.setDevice( "camacq" );
        nocurrent.setName( "fps" );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( nocurrent ) == 0 );
        REQUIRE( app.fpsValue() == Approx( 30 ) );
    }

    SECTION( "flipacq out On to Off restarts acquisition" )
    {
        psfAcq_test app( "psfacq" );
        app.addStar( 0, 10, 10, 50 );
        app.relabel();

        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "flipacq" );
        ip.setName( "presetName" );

        // no out element: ignored
        REQUIRE( app.setCallBack_m_indiP_flipAcqPresetName( ip ) == 0 );
        REQUIRE( app.flipAcqOutStateValid() == false );

        ip.add( pcf::IndiElement( "out", pcf::IndiElement::UnknownSwitchState ) );
        REQUIRE( app.setCallBack_m_indiP_flipAcqPresetName( ip ) == 0 );
        REQUIRE( app.flipAcqOutStateValid() == false );

        // first observation of Off does not reset
        ip["out"].setSwitchState( pcf::IndiElement::Off );
        REQUIRE( app.setCallBack_m_indiP_flipAcqPresetName( ip ) == 0 );
        REQUIRE( app.flipAcqOutStateValid() == true );
        REQUIRE( app.flipAcqOutWasOn() == false );
        REQUIRE( app.stars().size() == 1 );

        ip["out"].setSwitchState( pcf::IndiElement::On );
        REQUIRE( app.setCallBack_m_indiP_flipAcqPresetName( ip ) == 0 );
        REQUIRE( app.flipAcqOutWasOn() == true );
        REQUIRE( app.stars().size() == 1 );

        ip["out"].setSwitchState( pcf::IndiElement::Off );
        REQUIRE( app.setCallBack_m_indiP_flipAcqPresetName( ip ) == 0 );
        REQUIRE( app.flipAcqOutWasOn() == false );
        REQUIRE( app.stars().size() == 0 );
    }
}

} // namespace psfAcqTest

} // namespace libXWCTest
