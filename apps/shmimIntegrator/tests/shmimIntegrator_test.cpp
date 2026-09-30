/** \file shmimIntegrator_test.cpp
 * \brief Catch2 tests for the shmimIntegrator app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup shmimIntegrator_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <semaphore.h>
#include <string>
#include <vector>

#include "../shmimIntegrator.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace MagAOX
{
namespace app
{

/// Test harness exposing shmimIntegrator internals (declared a friend by the app).
class shmimIntegrator_test : public shmimIntegrator
{
  public:
    /// Construct a harness with the given device name, initializing the semaphore and the INDI properties.
    explicit shmimIntegrator_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // normally done in appStartup()
        sem_init( &m_smSemaphore, 0, 0 );

        createStandardIndiNumber<unsigned>( m_indiP_nAverage, "nAverage", 1, 1000000, 1, "%u" );
        createStandardIndiNumber<float>( m_indiP_avgTime, "avgTime", 0, 1000000, 0, "%0.1f" );
        createStandardIndiNumber<unsigned>( m_indiP_nUpdate, "nUpdate", 1, 1000000, 1, "%u" );
        createStandardIndiToggleSw( m_indiP_startAveraging, "start" );

        m_indiP_fpsSource.setDevice( "camsci1" );
        m_indiP_fpsSource.setName( "fps" );

        m_indiP_stateSource.setDevice( "stagesrc" );
        m_indiP_stateSource.setName( "state_string" );

        // normally set by the framegrabber from the float output type
        frameGrabberT::m_typeSize = sizeof( float );
    }

    /// Destroy the harness, releasing the semaphore.
    ~shmimIntegrator_test() noexcept
    {
        sem_destroy( &m_smSemaphore );
    }

    using shmimIntegrator::m_accumImages;
    using shmimIntegrator::m_avgImage;
    using shmimIntegrator::m_avgTime;
    using shmimIntegrator::m_continuous;
    using shmimIntegrator::m_currImage;
    using shmimIntegrator::m_dark2Image;
    using shmimIntegrator::m_dark2Set;
    using shmimIntegrator::m_dark2Valid;
    using shmimIntegrator::m_darkImage;
    using shmimIntegrator::m_darkSet;
    using shmimIntegrator::m_darkValid;
    using shmimIntegrator::m_fileSaveDir;
    using shmimIntegrator::m_fileSaver;
    using shmimIntegrator::m_fps;
    using shmimIntegrator::m_fpsSource;
    using shmimIntegrator::m_imageValid;
    using shmimIntegrator::m_nAverage;
    using shmimIntegrator::m_nAverageDefault;
    using shmimIntegrator::m_nprocessed;
    using shmimIntegrator::m_nUpdate;
    using shmimIntegrator::m_running;
    using shmimIntegrator::m_sinceUpdate;
    using shmimIntegrator::m_stateSource;
    using shmimIntegrator::m_stateString;
    using shmimIntegrator::m_stateStringChanged;
    using shmimIntegrator::m_stateStringValid;
    using shmimIntegrator::m_stateStringValidOnStart;
    using shmimIntegrator::m_updated;

    using shmimIntegrator::allocate;
    using shmimIntegrator::processImage;

    using shmimIntegrator::acquireAndCheckValid;
    using shmimIntegrator::configureAcquisition;
    using shmimIntegrator::findMatchingDark;
    using shmimIntegrator::fps;
    using shmimIntegrator::loadImageIntoStream;
    using shmimIntegrator::reconfig;
    using shmimIntegrator::startAcquisition;

    using shmimIntegrator::newCallBack_m_indiP_avgTime;
    using shmimIntegrator::newCallBack_m_indiP_nAverage;
    using shmimIntegrator::newCallBack_m_indiP_nUpdate;
    using shmimIntegrator::newCallBack_m_indiP_startAveraging;
    using shmimIntegrator::setCallBack_m_indiP_fpsSource;
    using shmimIntegrator::setCallBack_m_indiP_stateSource;

    /// Register the configuration options.
    void setupConfigForTest()
    {
        setupConfig();
    }

    /// Read a config file and run loadConfigImpl(), returning its result.
    int loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        return loadConfigImpl( config );
    }

    /// Set the main input stream geometry as the shmimMonitor would.
    void setImageStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        shmimMonitorT::m_width    = w;
        shmimMonitorT::m_height   = h;
        shmimMonitorT::m_dataType = dt;
    }

    /// Set the first dark stream geometry as the shmimMonitor would.
    void setDarkStream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        darkMonitorT::m_width    = w;
        darkMonitorT::m_height   = h;
        darkMonitorT::m_dataType = dt;
    }

    /// Set the second dark stream geometry as the shmimMonitor would.
    void setDark2Stream( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */, uint8_t dt /**< [in] type */ )
    {
        dark2MonitorT::m_width    = w;
        dark2MonitorT::m_height   = h;
        dark2MonitorT::m_dataType = dt;
    }

    /// Try to take the semaphore without blocking; returns 0 if a post was pending.
    int trySem()
    {
        return sem_trywait( &m_smSemaphore );
    }

    /// Post the semaphore.
    void postSem()
    {
        sem_post( &m_smSemaphore );
    }

    /// Main shmimMonitor restart flag.
    bool &smRestart()
    {
        return shmimMonitorT::m_restart;
    }

    /// Main shmimMonitor shmim name.
    std::string smShmimName()
    {
        return shmimMonitorT::m_shmimName;
    }

    /// Dark shmimMonitor shmim name.
    std::string darkShmimName()
    {
        return darkMonitorT::m_shmimName;
    }

    /// Second dark shmimMonitor shmim name.
    std::string dark2ShmimName()
    {
        return dark2MonitorT::m_shmimName;
    }

    /// Dark shmimMonitor get-existing-first flag.
    bool darkGetExistingFirst()
    {
        return darkMonitorT::m_getExistingFirst;
    }

    /// Second dark shmimMonitor get-existing-first flag.
    bool dark2GetExistingFirst()
    {
        return dark2MonitorT::m_getExistingFirst;
    }

    /// frameGrabber reconfig flag.
    bool &fgReconfig()
    {
        return frameGrabberT::m_reconfig;
    }

    /// frameGrabber shmim name.
    std::string fgShmimName()
    {
        return frameGrabberT::m_shmimName;
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

    /// Telemeter maximum interval.
    double telMaxInterval()
    {
        return telemeterT::m_maxInterval;
    }
};

} // namespace app
} // namespace MagAOX
/// \endcond

namespace libXWCTest
{

/** \defgroup shmimIntegrator_unit_test shmimIntegrator Unit Tests
 * \brief Unit tests for the shmimIntegrator application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `shmimIntegrator` unit tests.
/** \ingroup shmimIntegrator_unit_test
 */
namespace shmimIntegratorTest
{

/// Build a number property with a single element.
static pcf::IndiProperty makeNumberProperty( const std::string &device /**< [in] property device */,
                                             const std::string &name /**< [in] property name */,
                                             const std::string &el /**< [in] element name, "" for none */,
                                             double             val /**< [in] element value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    if( el != "" )
    {
        ip.add( pcf::IndiElement( el, val ) );
    }
    return ip;
}

/// Build a text property with the given elements.
static pcf::IndiProperty makeTextProperty( const std::string              &device /**< [in] property device */,
                                           const std::string              &name /**< [in] property name */,
                                           const std::vector<std::string> &els /**< [in] element names */,
                                           const std::vector<std::string> &vals /**< [in] element values */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Text );
    ip.setDevice( device );
    ip.setName( name );
    for( size_t n = 0; n < els.size(); ++n )
    {
        ip.add( pcf::IndiElement( els[n], vals[n] ) );
    }
    return ip;
}

/// Build a toggle switch property.
static pcf::IndiProperty makeToggleProperty( const std::string &device /**< [in] property device */,
                                             const std::string &name /**< [in] property name */,
                                             bool               withToggle /**< [in] whether to add the toggle */,
                                             bool               on /**< [in] toggle state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    if( withToggle )
    {
        ip.add( pcf::IndiElement( "toggle" ) );
        ip["toggle"].setSwitchState( on ? pcf::IndiElement::On : pcf::IndiElement::Off );
    }
    return ip;
}

/// Removes a temporary directory and its contents when it goes out of scope.
struct tempDirGuard
{
    std::string m_dir; ///< The directory to remove.

    /// Construct the guard for a directory.
    explicit tempDirGuard( const std::string &dir /**< [in] directory to remove on destruction */ ) : m_dir( dir )
    {
    }

    /// Remove the directory and its contents.
    ~tempDirGuard()
    {
        std::error_code ec;
        std::filesystem::remove_all( m_dir, ec );
    }
};

/// Fill a frame with a constant value.
static std::vector<float> constFrame( size_t n /**< [in] number of pixels */, float val /**< [in] pixel value */ )
{
    return std::vector<float>( n, val );
}

/// Verify default configuration values.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator configuration defaults", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::shmimIntegrator();
    shmimIntegrator::setupConfig();
    shmimIntegrator::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    shmimIntegrator_test app( "camsci1-avg" );

    // set by the constructor
    CHECK( app.darkGetExistingFirst() == true );
    CHECK( app.dark2GetExistingFirst() == false );

    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/shmimIntegrator_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( "/tmp/shmimIntegrator_test_defaults.conf" ) == 0 );

    CHECK( app.m_nAverageDefault == 10 );
    CHECK( app.m_nAverage == 10 );
    CHECK( app.m_fpsSource == "" );
    CHECK( app.m_avgTime == 0 );
    CHECK( app.m_nUpdate == 0 );
    CHECK( app.m_continuous == true );
    CHECK( app.m_running == true );
    CHECK( app.m_stateSource == "" );
    CHECK( app.m_fileSaver == false );

    CHECK( app.smShmimName() == "camsci1-avg" );
    CHECK( app.darkShmimName() == "camsci1-avg" );
    CHECK( app.dark2ShmimName() == "camsci1-avg" );
    CHECK( app.fgShmimName() == "camsci1-avg" );
    CHECK( app.telMaxInterval() == Approx( 10.0 ) );

    std::remove( "/tmp/shmimIntegrator_test_defaults.conf" );
}

/// Verify configuration overrides for the integrator and stream settings.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator configuration overrides", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::setupConfig();
    shmimIntegrator::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    shmimIntegrator_test app( "camsci1-avg" );
    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/shmimIntegrator_test_override.conf",
                              { "integrator",
                                "integrator",
                                "integrator",
                                "integrator",
                                "integrator",
                                "integrator",
                                "integrator",
                                "integrator",
                                "shmimMonitor",
                                "darkShmim",
                                "dark2Shmim",
                                "framegrabber" },
                              { "nAverage",
                                "fpsSource",
                                "avgTime",
                                "nUpdate",
                                "continuous",
                                "running",
                                "stateSource",
                                "fileSaver",
                                "shmimName",
                                "shmimName",
                                "shmimName",
                                "shmimName" },
                              { "25",
                                "camsci1",
                                "2.5",
                                "5",
                                "false",
                                "false",
                                "stagesrc",
                                "true",
                                "camsci1",
                                "camsci1_dark",
                                "camsci1_dark2",
                                "camsci1_avg" } );

    REQUIRE( app.loadConfigFromFile( "/tmp/shmimIntegrator_test_override.conf" ) == 0 );

    CHECK( app.m_nAverageDefault == 25 );
    CHECK( app.m_nAverage == 25 );
    CHECK( app.m_fpsSource == "camsci1" );
    CHECK( app.m_avgTime == Approx( 2.5 ) );
    CHECK( app.m_nUpdate == 5 );
    CHECK( app.m_continuous == false );
    CHECK( app.m_running == false );
    CHECK( app.m_stateSource == "stagesrc" );
    CHECK( app.m_fileSaver == true );

    CHECK( app.smShmimName() == "camsci1" );
    CHECK( app.darkShmimName() == "camsci1_dark" );
    CHECK( app.dark2ShmimName() == "camsci1_dark2" );
    CHECK( app.fgShmimName() == "camsci1_avg" );

    std::remove( "/tmp/shmimIntegrator_test_override.conf" );
}

/// Verify the main-stream allocate() sizes the buffers and selects the averaging length.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator main image allocate", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::allocate(const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "simple average uses a 1x1x1 accumulator" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 4, 3, IMAGESTRUCT_UINT16 );
        app.m_nprocessed  = 5;
        app.m_currImage   = 2;
        app.m_sinceUpdate = 3;
        app.fgReconfig()  = false;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        CHECK( app.m_accumImages.rows() == 1 );
        CHECK( app.m_accumImages.cols() == 1 );
        CHECK( app.m_accumImages.planes() == 1 );
        CHECK( app.m_avgImage.rows() == 4 );
        CHECK( app.m_avgImage.cols() == 3 );
        CHECK( app.m_nprocessed == 0 );
        CHECK( app.m_currImage == 0 );
        CHECK( app.m_sinceUpdate == 0 );
        CHECK( app.m_nAverage == 10 );
        CHECK( app.fgReconfig() == true );
    }

    SECTION( "moving average allocates an nAverage-deep zeroed cube" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 4, 3, IMAGESTRUCT_FLOAT );
        app.m_nAverage = 6;
        app.m_nUpdate  = 2;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        CHECK( app.m_accumImages.rows() == 4 );
        CHECK( app.m_accumImages.cols() == 3 );
        CHECK( app.m_accumImages.planes() == 6 );
        CHECK( app.m_accumImages.image( 5 ).sum() == 0 );
    }

    SECTION( "time-based averaging sets nAverage from avgTime*fps" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_UINT16 );
        app.m_avgTime = 0.5;
        app.m_fps     = 100;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        CHECK( app.m_nAverage == 50 );
    }

    SECTION( "time-based averaging never drops below one frame" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_UINT16 );
        app.m_avgTime = 0.001;
        app.m_fps     = 100;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        CHECK( app.m_nAverage == 1 );
    }

    SECTION( "time-based averaging without an fps yet reverts to the default" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_UINT16 );
        app.m_avgTime         = 1.0;
        app.m_fps             = 0;
        app.m_nAverageDefault = 7;
        app.m_nAverage        = 33;

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        CHECK( app.m_nAverage == 7 );
    }

    SECTION( "bad data type returns -1" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_COMPLEX_FLOAT );

        REQUIRE( app.allocate( dev::shmimT() ) == -1 );
    }

    SECTION( "dark validity follows the dark geometry" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 4, 3, IMAGESTRUCT_UINT16 );
        app.setDarkStream( 4, 3, IMAGESTRUCT_FLOAT );
        app.setDark2Stream( 8, 8, IMAGESTRUCT_FLOAT );

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        CHECK( app.m_darkValid == true );
        CHECK( app.m_dark2Valid == false );
    }
}

/// Verify the dark allocate()/processImage() pairs load both darks.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator dark streams allocate and process", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::allocate(const darkShmimT &);
    shmimIntegrator::processImage(void *, const darkShmimT &);
    shmimIntegrator::allocate(const dark2ShmimT &);
    shmimIntegrator::processImage(void *, const dark2ShmimT &);
    #endif
    // clang-format on

    SECTION( "uint16 dark is converted and marked set" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 3, 2, IMAGESTRUCT_UINT16 );
        app.setDarkStream( 3, 2, IMAGESTRUCT_UINT16 );

        REQUIRE( app.allocate( darkShmimT() ) == 0 );
        CHECK( app.m_darkValid == true );
        CHECK( app.m_darkSet == false );
        CHECK( app.m_darkImage.rows() == 3 );
        CHECK( app.m_darkImage.cols() == 2 );
        CHECK( app.m_darkImage.sum() == 0 );

        std::vector<uint16_t> dark = { 1, 2, 3, 4, 5, 6 };
        REQUIRE( app.processImage( dark.data(), darkShmimT() ) == 0 );

        CHECK( app.m_darkSet == true );
        CHECK( app.m_darkImage( 0, 0 ) == Approx( 1 ) );
        CHECK( app.m_darkImage( 2, 0 ) == Approx( 3 ) );
        CHECK( app.m_darkImage( 0, 1 ) == Approx( 4 ) );
        CHECK( app.m_darkImage( 2, 1 ) == Approx( 6 ) );
    }

    SECTION( "second dark with mismatched geometry is not valid" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 3, 2, IMAGESTRUCT_UINT16 );
        app.setDark2Stream( 5, 5, IMAGESTRUCT_FLOAT );

        REQUIRE( app.allocate( dark2ShmimT() ) == 0 );
        CHECK( app.m_dark2Valid == false );
        CHECK( app.m_dark2Image.rows() == 5 );
        CHECK( app.m_dark2Image.cols() == 5 );

        std::vector<float> dark( 25, 1.5f );
        REQUIRE( app.processImage( dark.data(), dark2ShmimT() ) == 0 );
        CHECK( app.m_dark2Set == true );
        CHECK( app.m_dark2Image.sum() == Approx( 37.5 ) );
    }

    SECTION( "bad dark data types return -1 and clear the dark state" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_UINT16 );
        app.setDarkStream( 2, 2, IMAGESTRUCT_COMPLEX_FLOAT );
        app.setDark2Stream( 2, 2, IMAGESTRUCT_COMPLEX_DOUBLE );
        app.m_darkSet    = true;
        app.m_darkValid  = true;
        app.m_dark2Set   = true;
        app.m_dark2Valid = true;

        REQUIRE( app.allocate( darkShmimT() ) == -1 );
        CHECK( app.m_darkSet == false );
        CHECK( app.m_darkValid == false );

        REQUIRE( app.allocate( dark2ShmimT() ) == -1 );
        CHECK( app.m_dark2Set == false );
        CHECK( app.m_dark2Valid == false );
    }
}

/// Verify the simple (block) average of nAverage frames and the handoff to the framegrabber.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator simple average", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::processImage(void *, const dev::shmimT &);
    shmimIntegrator::acquireAndCheckValid();
    shmimIntegrator::loadImageIntoStream(void *);
    #endif
    // clang-format on

    shmimIntegrator_test app( "avg" );
    app.setImageStream( 3, 2, IMAGESTRUCT_UINT16 );
    app.m_nAverage = 3;
    REQUIRE( app.allocate( dev::shmimT() ) == 0 );

    std::vector<uint16_t> f1 = { 1, 2, 3, 4, 5, 6 };
    std::vector<uint16_t> f2 = { 2, 4, 6, 8, 10, 12 };
    std::vector<uint16_t> f3 = { 3, 6, 9, 12, 15, 18 };

    REQUIRE( app.processImage( f1.data(), dev::shmimT() ) == 0 );
    REQUIRE( app.processImage( f2.data(), dev::shmimT() ) == 0 );
    CHECK( app.m_updated == false );
    CHECK( app.m_sinceUpdate == 2 );
    CHECK( app.trySem() != 0 );

    REQUIRE( app.processImage( f3.data(), dev::shmimT() ) == 0 );
    CHECK( app.m_updated == true );
    CHECK( app.m_sinceUpdate == 0 );
    CHECK( app.m_running == true ); // continuous

    for( size_t n = 0; n < f1.size(); ++n )
    {
        CHECK( app.m_avgImage.data()[n] == Approx( 2.0 * f1[n] ) );
    }

    // the framegrabber is released and sees a valid frame
    REQUIRE( app.acquireAndCheckValid() == 0 );
    CHECK( app.fgTimestamp().tv_sec > 0 );

    // frames arriving before the framegrabber copies the average are ignored
    std::vector<uint16_t> big( 6, 1000 );
    REQUIRE( app.processImage( big.data(), dev::shmimT() ) == 0 );
    CHECK( app.m_sinceUpdate == 0 );
    CHECK( app.m_avgImage( 0, 0 ) == Approx( 2.0 ) );

    std::vector<float> dest( 6, 0.0f );
    REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
    CHECK( app.m_updated == false );
    for( size_t n = 0; n < f1.size(); ++n )
    {
        CHECK( dest[n] == Approx( 2.0 * f1[n] ) );
    }

    // the next block starts from zero
    REQUIRE( app.processImage( big.data(), dev::shmimT() ) == 0 );
    CHECK( app.m_sinceUpdate == 1 );
    CHECK( app.m_avgImage( 0, 0 ) == Approx( 1000.0 ) );
}

/// Verify dark subtraction in the simple average for each dark combination.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator simple average dark subtraction", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    const size_t npix = 4;

    // Load darks into the app as the dark shmimMonitors would.
    auto setup = [npix]( shmimIntegrator_test &app, bool useDark, bool useDark2 )
    {
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.setDarkStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.setDark2Stream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage = 2;

        REQUIRE( app.allocate( darkShmimT() ) == 0 );
        REQUIRE( app.allocate( dark2ShmimT() ) == 0 );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        std::vector<float> d1 = constFrame( npix, 1.0f );
        std::vector<float> d2 = constFrame( npix, 3.0f );
        if( useDark )
        {
            REQUIRE( app.processImage( d1.data(), darkShmimT() ) == 0 );
        }
        if( useDark2 )
        {
            REQUIRE( app.processImage( d2.data(), dark2ShmimT() ) == 0 );
        }
    };

    std::vector<float> fr = constFrame( npix, 10.0f );

    SECTION( "no dark" )
    {
        shmimIntegrator_test app( "avg" );
        setup( app, false, false );
        app.processImage( fr.data(), dev::shmimT() );
        app.processImage( fr.data(), dev::shmimT() );
        REQUIRE( app.m_updated );
        CHECK( app.m_avgImage( 1, 1 ) == Approx( 10.0 ) );
    }

    SECTION( "first dark only" )
    {
        shmimIntegrator_test app( "avg" );
        setup( app, true, false );
        app.processImage( fr.data(), dev::shmimT() );
        app.processImage( fr.data(), dev::shmimT() );
        REQUIRE( app.m_updated );
        CHECK( app.m_avgImage( 1, 1 ) == Approx( 9.0 ) );
    }

    SECTION( "second dark only" )
    {
        shmimIntegrator_test app( "avg" );
        setup( app, false, true );
        app.processImage( fr.data(), dev::shmimT() );
        app.processImage( fr.data(), dev::shmimT() );
        REQUIRE( app.m_updated );
        CHECK( app.m_avgImage( 1, 1 ) == Approx( 7.0 ) );
    }

    SECTION( "both darks" )
    {
        shmimIntegrator_test app( "avg" );
        setup( app, true, true );
        app.processImage( fr.data(), dev::shmimT() );
        app.processImage( fr.data(), dev::shmimT() );
        REQUIRE( app.m_updated );
        CHECK( app.m_avgImage( 0, 0 ) == Approx( 6.0 ) );
        CHECK( app.m_avgImage( 1, 1 ) == Approx( 6.0 ) );
    }

    SECTION( "a set but invalid dark is not subtracted" )
    {
        shmimIntegrator_test app( "avg" );
        setup( app, true, false );
        app.m_darkValid = false;
        app.processImage( fr.data(), dev::shmimT() );
        app.processImage( fr.data(), dev::shmimT() );
        REQUIRE( app.m_updated );
        CHECK( app.m_avgImage( 1, 1 ) == Approx( 10.0 ) );
    }
}

/// Verify the moving average (0 < nUpdate) over the accumulator cube.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator moving average", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::processImage(void *, const dev::shmimT &);
    shmimIntegrator::loadImageIntoStream(void *);
    #endif
    // clang-format on

    const size_t npix = 4;

    SECTION( "update every frame after burn-in" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage = 3;
        app.m_nUpdate  = 1;
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        std::vector<float> f1 = constFrame( npix, 1.0f );
        std::vector<float> f2 = constFrame( npix, 2.0f );
        std::vector<float> f3 = constFrame( npix, 3.0f );
        std::vector<float> f4 = constFrame( npix, 4.0f );

        REQUIRE( app.processImage( f1.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.processImage( f2.data(), dev::shmimT() ) == 0 );
        CHECK( app.m_updated == false ); // still burning in
        CHECK( app.m_nprocessed == 2 );

        REQUIRE( app.processImage( f3.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_updated == true );
        CHECK( app.m_currImage == 0 ); // wrapped
        CHECK( app.m_avgImage( 0, 0 ) == Approx( 2.0 ) );
        CHECK( app.trySem() == 0 );

        std::vector<float> dest( npix, 0.0f );
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        CHECK( dest[3] == Approx( 2.0 ) );
        CHECK( app.m_updated == false );

        // frame 4 replaces frame 1: (4+2+3)/3
        REQUIRE( app.processImage( f4.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_updated == true );
        CHECK( app.m_avgImage( 1, 0 ) == Approx( 3.0 ) );
        CHECK( app.m_currImage == 1 );
    }

    SECTION( "update every second frame" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage = 3;
        app.m_nUpdate  = 2;
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        for( float v : { 1.0f, 2.0f, 3.0f } )
        {
            std::vector<float> f = constFrame( npix, v );
            REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        }
        CHECK( app.m_updated == false );
        CHECK( app.m_sinceUpdate == 1 );

        std::vector<float> f4 = constFrame( npix, 4.0f );
        REQUIRE( app.processImage( f4.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_updated == true );
        CHECK( app.m_sinceUpdate == 0 );
        CHECK( app.m_avgImage( 0, 1 ) == Approx( 3.0 ) );
    }

    SECTION( "first dark is subtracted" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.setDarkStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage = 2;
        app.m_nUpdate  = 1;
        REQUIRE( app.allocate( darkShmimT() ) == 0 );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        std::vector<float> dark = constFrame( npix, 0.5f );
        REQUIRE( app.processImage( dark.data(), darkShmimT() ) == 0 );

        std::vector<float> f = constFrame( npix, 5.0f );
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_updated == true );
        CHECK( app.m_avgImage( 1, 1 ) == Approx( 4.5 ) );
    }

    SECTION( "update skipped while the framegrabber is behind" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage = 2;
        app.m_nUpdate  = 1;
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        std::vector<float> f1 = constFrame( npix, 1.0f );
        std::vector<float> f9 = constFrame( npix, 9.0f );
        REQUIRE( app.processImage( f1.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.processImage( f1.data(), dev::shmimT() ) == 0 );
        REQUIRE( app.m_updated == true );
        REQUIRE( app.trySem() == 0 );

        REQUIRE( app.processImage( f9.data(), dev::shmimT() ) == 0 );
        CHECK( app.m_avgImage( 0, 0 ) == Approx( 1.0 ) ); // not recomputed
        CHECK( app.trySem() != 0 );                       // not posted again
    }
}

/// Verify processImage() does nothing while stopped, and a triggered (non-continuous) average stops itself.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator triggered and stopped averaging", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    const size_t npix = 4;

    SECTION( "not running ignores frames" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage = 1;
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        app.m_running = false;

        std::vector<float> f = constFrame( npix, 1.0f );
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        CHECK( app.m_sinceUpdate == 0 );
        CHECK( app.m_updated == false );
        CHECK( app.trySem() != 0 );
    }

    SECTION( "non-continuous average stops after one block" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage   = 2;
        app.m_continuous = false;
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        std::vector<float> f = constFrame( npix, 4.0f );
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        CHECK( app.m_running == true );
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        CHECK( app.m_running == false );
        CHECK( app.m_updated == true );
        CHECK( app.m_avgImage( 0, 0 ) == Approx( 4.0 ) );
    }

    SECTION( "file saver with an invalid state string does not save" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage                = 1;
        app.m_continuous              = false;
        app.m_fileSaver               = true;
        app.m_stateStringValid        = false;
        app.m_stateStringValidOnStart = false;
        app.m_imageValid              = true;
        app.m_fileSaveDir             = "/tmp/shmimIntegrator_test_nonexistent_dir";
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        std::vector<float> f = constFrame( npix, 4.0f );
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        CHECK( app.m_running == false );
        CHECK( app.m_imageValid == false );
    }

    SECTION( "file saver with a state change during acquisition does not save" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 2, 2, IMAGESTRUCT_FLOAT );
        app.m_nAverage                = 1;
        app.m_continuous              = false;
        app.m_fileSaver               = true;
        app.m_stateStringValid        = true;
        app.m_stateStringValidOnStart = true;
        app.m_stateStringChanged      = true;
        app.m_imageValid              = true;
        app.m_fileSaveDir             = "/tmp/shmimIntegrator_test_nonexistent_dir";
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        std::vector<float> f = constFrame( npix, 4.0f );
        REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
        CHECK( app.m_imageValid == false );
    }
}

/// Verify the file saver writes a dark and findMatchingDark() reloads the newest match for the state string.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator file saver and findMatchingDark", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::processImage(void *, const dev::shmimT &);
    shmimIntegrator::findMatchingDark();
    #endif
    // clang-format on

    char dirTemplate[] = "/tmp/shmimIntegrator_test_XXXXXX";
    REQUIRE( mkdtemp( dirTemplate ) != nullptr );
    std::string  saveDir = dirTemplate;
    tempDirGuard guard( saveDir );

    SECTION( "save then reload a matching dark" )
    {
        {
            shmimIntegrator_test app( "avg" );
            app.setImageStream( 3, 2, IMAGESTRUCT_FLOAT );
            app.m_nAverage                = 1;
            app.m_continuous              = false;
            app.m_fileSaver               = true;
            app.m_stateString             = "stateA";
            app.m_stateStringValid        = true;
            app.m_stateStringValidOnStart = true;
            app.m_stateStringChanged      = false;
            app.m_fileSaveDir             = saveDir;
            REQUIRE( app.allocate( dev::shmimT() ) == 0 );

            std::vector<float> f = { 1, 2, 3, 4, 5, 6 };
            REQUIRE( app.processImage( f.data(), dev::shmimT() ) == 0 );
            CHECK( app.m_running == false );
            CHECK( app.m_imageValid == true );
        }

        std::vector<std::string> fnames;
        REQUIRE( mx::ioutils::getFileNames( fnames, saveDir, "avg_stateA__T", "", ".fits" ) == mx::error_t::noerror );
        REQUIRE( fnames.size() == 1 );

        shmimIntegrator_test app( "avg" );
        app.setImageStream( 3, 2, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        app.m_running            = false;
        app.m_fileSaveDir        = saveDir;
        app.m_stateString        = "stateA";
        app.m_stateStringChanged = true;
        app.m_updated            = false;
        app.m_avgImage.setZero();

        REQUIRE( app.findMatchingDark() == 0 );
        CHECK( app.m_imageValid == true );
        CHECK( app.m_stateStringChanged == false );
        CHECK( app.m_updated == true );
        CHECK( app.trySem() == 0 );
        CHECK( app.m_avgImage( 0, 0 ) == Approx( 1.0 ) );
        CHECK( app.m_avgImage( 2, 1 ) == Approx( 6.0 ) );
    }

    SECTION( "no matching state string invalidates the image" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 3, 2, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        app.m_running            = false;
        app.m_fileSaveDir        = saveDir;
        app.m_stateString        = "stateB";
        app.m_stateStringChanged = true;
        app.m_imageValid         = true;

        REQUIRE( app.findMatchingDark() == 0 );
        CHECK( app.m_imageValid == false );
        CHECK( app.m_stateStringChanged == false );
        CHECK( app.trySem() != 0 );
    }

    SECTION( "matching dark with the wrong size is rejected and retried later" )
    {
        mx::improc::eigenImage<float> im( 4, 4 );
        im.setConstant( 2.0f );
        mx::fits::fitsFile<float> ff;
        ff.write( saveDir + "/avg_stateC__T20260101000000000000000.fits", im );

        shmimIntegrator_test app( "avg" );
        app.setImageStream( 3, 2, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );
        app.m_running            = false;
        app.m_fileSaveDir        = saveDir;
        app.m_stateString        = "stateC";
        app.m_stateStringChanged = false;
        app.m_imageValid         = true;

        REQUIRE( app.findMatchingDark() == 0 );
        CHECK( app.m_imageValid == false );
        CHECK( app.m_stateStringChanged == true );
        CHECK( app.trySem() != 0 );
    }

    SECTION( "missing save directory returns -1" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_fileSaveDir = saveDir + "/does_not_exist";

        REQUIRE( app.findMatchingDark() == -1 );
    }
}

/// Verify configureAcquisition(), fps() and the trivial framegrabber hooks.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator framegrabber interface", "[shmimIntegrator]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::configureAcquisition();
    shmimIntegrator::startAcquisition();
    shmimIntegrator::reconfig();
    shmimIntegrator::fps();
    shmimIntegrator::acquireAndCheckValid();
    #endif
    // clang-format on

    SECTION( "configureAcquisition copies the input geometry with float output" )
    {
        shmimIntegrator_test app( "avg" );
        app.setImageStream( 16, 8, IMAGESTRUCT_UINT16 );

        REQUIRE( app.configureAcquisition() == 0 );
        CHECK( app.fgWidth() == 16 );
        CHECK( app.fgHeight() == 8 );
        CHECK( app.fgDataType() == _DATATYPE_FLOAT );
        CHECK( app.startAcquisition() == 0 );
        CHECK( app.reconfig() == 0 );
    }

    SECTION( "configureAcquisition without an input stream returns -1 (after a 1 s wait)" )
    {
        shmimIntegrator_test app( "avg" );
        REQUIRE( app.configureAcquisition() == -1 );
    }

    SECTION( "fps is the source fps divided by nAverage" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_fps      = 100;
        app.m_nAverage = 10;
        CHECK( app.fps() == Approx( 10.0 ) );

        app.m_running    = false;
        app.m_continuous = true;
        CHECK( app.fps() == Approx( 10.0 ) );

        app.m_continuous = false;
        CHECK( app.fps() == Approx( 1.0 ) );

        app.m_running = true;
        app.m_fps     = 0;
        CHECK( app.fps() == Approx( 1.0 ) );
    }

    SECTION( "a posted semaphore without an update is not a valid frame" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_updated = false;
        app.postSem();
        CHECK( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "a posted update is a valid frame" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_updated = true;
        app.postSem();
        CHECK( app.acquireAndCheckValid() == 0 );
    }
}

/// Verify the nAverage, avgTime and nUpdate NEW callbacks.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator number NEW callbacks", "[shmimIntegrator][indi]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::newCallBack_m_indiP_nAverage(const pcf::IndiProperty &);
    shmimIntegrator::newCallBack_m_indiP_avgTime(const pcf::IndiProperty &);
    shmimIntegrator::newCallBack_m_indiP_nUpdate(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong name, wrong device and missing elements are rejected" )
    {
        shmimIntegrator_test app( "avg" );

        CHECK( app.newCallBack_m_indiP_nAverage( makeNumberProperty( "avg", "wrong", "target", 5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_nAverage( makeNumberProperty( "wrong", "nAverage", "target", 5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_nAverage( makeNumberProperty( "avg", "nAverage", "", 0 ) ) == -1 );

        CHECK( app.newCallBack_m_indiP_avgTime( makeNumberProperty( "avg", "wrong", "target", 5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_avgTime( makeNumberProperty( "wrong", "avgTime", "target", 5 ) ) == -1 );

        CHECK( app.newCallBack_m_indiP_nUpdate( makeNumberProperty( "avg", "wrong", "target", 5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_nUpdate( makeNumberProperty( "wrong", "nUpdate", "target", 5 ) ) == -1 );

        CHECK( app.m_nAverage == 10 );
        CHECK( app.m_avgTime == 0 );
        CHECK( app.m_nUpdate == 0 );
        CHECK( app.smRestart() == false );
    }

    SECTION( "nAverage target sets the averaging length and restarts" )
    {
        shmimIntegrator_test app( "avg" );

        REQUIRE( app.newCallBack_m_indiP_nAverage( makeNumberProperty( "avg", "nAverage", "target", 25 ) ) == 0 );
        CHECK( app.m_nAverage == 25 );
        CHECK( app.m_avgTime == 0 );
        CHECK( app.smRestart() == true );
    }

    SECTION( "nAverage current is used when there is no target" )
    {
        shmimIntegrator_test app( "avg" );

        REQUIRE( app.newCallBack_m_indiP_nAverage( makeNumberProperty( "avg", "nAverage", "current", 12 ) ) == 0 );
        CHECK( app.m_nAverage == 12 );
    }

    SECTION( "nAverage with time-based averaging updates avgTime" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_avgTime = 1.0;
        app.m_fps     = 50;

        REQUIRE( app.newCallBack_m_indiP_nAverage( makeNumberProperty( "avg", "nAverage", "target", 25 ) ) == 0 );
        CHECK( app.m_nAverage == 25 );
        CHECK( app.m_avgTime == Approx( 0.5 ) );
    }

    SECTION( "avgTime target sets the averaging time and restarts" )
    {
        shmimIntegrator_test app( "avg" );

        REQUIRE( app.newCallBack_m_indiP_avgTime( makeNumberProperty( "avg", "avgTime", "target", 2.5 ) ) == 0 );
        CHECK( app.m_avgTime == Approx( 2.5 ) );
        CHECK( app.smRestart() == true );
    }

    SECTION( "nUpdate target sets the update rate and restarts" )
    {
        shmimIntegrator_test app( "avg" );

        REQUIRE( app.newCallBack_m_indiP_nUpdate( makeNumberProperty( "avg", "nUpdate", "target", 3 ) ) == 0 );
        CHECK( app.m_nUpdate == 3 );
        CHECK( app.smRestart() == true );
    }
}

/// Verify the start-averaging toggle NEW callback.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator startAveraging NEW callback", "[shmimIntegrator][indi]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::newCallBack_m_indiP_startAveraging(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong name is rejected" )
    {
        shmimIntegrator_test app( "avg" );
        CHECK( app.newCallBack_m_indiP_startAveraging( makeToggleProperty( "avg", "wrong", true, false ) ) == -1 );
        CHECK( app.m_running == true );
    }

    SECTION( "missing toggle element is ignored" )
    {
        shmimIntegrator_test app( "avg" );
        CHECK( app.newCallBack_m_indiP_startAveraging( makeToggleProperty( "avg", "start", false, false ) ) == 0 );
        CHECK( app.m_running == true );
    }

    SECTION( "toggle off stops averaging" )
    {
        shmimIntegrator_test app( "avg" );
        REQUIRE( app.newCallBack_m_indiP_startAveraging( makeToggleProperty( "avg", "start", true, false ) ) == 0 );
        CHECK( app.m_running == false );
        CHECK( app.state() == stateCodes::READY );
    }

    SECTION( "toggle on starts averaging and latches the state-string validity" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_running                 = false;
        app.m_fileSaver               = true;
        app.m_continuous              = false;
        app.m_stateStringChanged      = true;
        app.m_stateStringValid        = true;
        app.m_stateStringValidOnStart = false;

        REQUIRE( app.newCallBack_m_indiP_startAveraging( makeToggleProperty( "avg", "start", true, true ) ) == 0 );
        CHECK( app.m_running == true );
        CHECK( app.state() == stateCodes::OPERATING );
        CHECK( app.m_stateStringChanged == false );
        CHECK( app.m_stateStringValidOnStart == true );
    }

    SECTION( "toggle on in continuous mode leaves the state-changed flag" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_running            = false;
        app.m_fileSaver          = true;
        app.m_continuous         = true;
        app.m_stateStringChanged = true;

        REQUIRE( app.newCallBack_m_indiP_startAveraging( makeToggleProperty( "avg", "start", true, true ) ) == 0 );
        CHECK( app.m_running == true );
        CHECK( app.m_stateStringChanged == true );
    }
}

/// Verify the fps-source and state-source SET callbacks.
/**
 * \ingroup shmimIntegrator_unit_test
 */
TEST_CASE( "shmimIntegrator SET callbacks", "[shmimIntegrator][indi]" )
{
    // clang-format off
    #ifdef SHMIMINTEGRATOR_TEST_DOXYGEN_REF
    shmimIntegrator::setCallBack_m_indiP_fpsSource(const pcf::IndiProperty &);
    shmimIntegrator::setCallBack_m_indiP_stateSource(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "fps source: wrong name rejected, missing current ignored" )
    {
        shmimIntegrator_test app( "avg" );
        CHECK( app.setCallBack_m_indiP_fpsSource( makeNumberProperty( "camsci1", "wrong", "current", 10 ) ) == -1 );
        CHECK( app.setCallBack_m_indiP_fpsSource( makeNumberProperty( "camsci1", "fps", "", 0 ) ) == 0 );
        CHECK( app.m_fps == 0 );
    }

    SECTION( "fps source: a new fps restarts and reconfigures" )
    {
        shmimIntegrator_test app( "avg" );
        app.fgReconfig() = false;

        REQUIRE( app.setCallBack_m_indiP_fpsSource( makeNumberProperty( "camsci1", "fps", "current", 200 ) ) == 0 );
        CHECK( app.m_fps == Approx( 200 ) );
        CHECK( app.smRestart() == true );
        CHECK( app.fgReconfig() == true );

        // the same fps again changes nothing
        app.smRestart()  = false;
        app.fgReconfig() = false;
        REQUIRE( app.setCallBack_m_indiP_fpsSource( makeNumberProperty( "camsci1", "fps", "current", 200 ) ) == 0 );
        CHECK( app.smRestart() == false );
        CHECK( app.fgReconfig() == false );
    }

    SECTION( "state source: wrong name rejected" )
    {
        shmimIntegrator_test app( "avg" );
        CHECK( app.setCallBack_m_indiP_stateSource(
                   makeTextProperty( "stagesrc", "wrong", { "current" }, { "stateA" } ) ) == -1 );
        CHECK( app.m_stateString == "" );
    }

    SECTION( "state source: validity changes are tracked" )
    {
        shmimIntegrator_test app( "avg" );

        REQUIRE( app.setCallBack_m_indiP_stateSource(
                     makeTextProperty( "stagesrc", "state_string", { "valid" }, { "yes" } ) ) == 0 );
        CHECK( app.m_stateStringValid == true );
        CHECK( app.m_stateStringChanged == true );

        app.m_stateStringChanged = false;
        REQUIRE( app.setCallBack_m_indiP_stateSource(
                     makeTextProperty( "stagesrc", "state_string", { "valid" }, { "yes" } ) ) == 0 );
        CHECK( app.m_stateStringChanged == false );

        REQUIRE( app.setCallBack_m_indiP_stateSource(
                     makeTextProperty( "stagesrc", "state_string", { "valid" }, { "no" } ) ) == 0 );
        CHECK( app.m_stateStringValid == false );
        CHECK( app.m_stateStringChanged == true );
    }

    SECTION( "state source: a new state string invalidates the image" )
    {
        shmimIntegrator_test app( "avg" );
        app.m_imageValid = true;

        REQUIRE( app.setCallBack_m_indiP_stateSource( makeTextProperty(
                     "stagesrc", "state_string", { "current", "valid" }, { "stateA", "yes" } ) ) == 0 );
        CHECK( app.m_stateString == "stateA" );
        CHECK( app.m_imageValid == false );
        CHECK( app.m_stateStringChanged == true );
        CHECK( app.m_stateStringValid == true );

        // the same state string again leaves the image validity alone
        app.m_imageValid         = true;
        app.m_stateStringChanged = false;
        REQUIRE( app.setCallBack_m_indiP_stateSource(
                     makeTextProperty( "stagesrc", "state_string", { "current" }, { "stateA" } ) ) == 0 );
        CHECK( app.m_imageValid == true );
        CHECK( app.m_stateStringChanged == false );
    }
}

} // namespace shmimIntegratorTest

} // namespace libXWCTest
