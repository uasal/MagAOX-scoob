/** \file modalFilter_test.cpp
 * \brief Catch2 tests for the modalFilter app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup modalFilter_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <semaphore.h>
#include <string>
#include <vector>

#include "../modalFilter.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace MagAOX
{
namespace app
{

/// Test harness exposing modalFilter internals.
/** This is the befriended `MagAOX::app::modalFilter_test`, which is needed because modalFilter inherits its
 * shmimMonitor, frameGrabber and telemeter bases privately.
 */
class modalFilter_test : public modalFilter
{
  public:
    /// Construct a harness with the given device name.
    explicit modalFilter_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        sem_init( &m_filtSem, 0, 0 );

        m_indiP_fpsSource.setDevice( "camwfs" );
        m_indiP_fpsSource.setName( "fps" );

        m_indiP_loop.setDevice( device );
        m_indiP_loop.setName( "loop_state" );
        m_indiP_gain.setDevice( device );
        m_indiP_gain.setName( "loop_gain" );
        m_indiP_mult.setDevice( device );
        m_indiP_mult.setName( "loop_multcoeff" );
        m_indiP_pcGain.setDevice( device );
        m_indiP_pcGain.setName( "loop_pcgain" );
        m_indiP_pcMult.setDevice( device );
        m_indiP_pcMult.setName( "loop_pcmultcoeff" );
        m_indiP_pcOn.setDevice( device );
        m_indiP_pcOn.setName( "loop_pcon" );
    }

    /// Destroy the harness, releasing the semaphore.
    ~modalFilter_test() noexcept
    {
        sem_destroy( &m_filtSem );
    }

    /// Register the configuration options.
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

    /// Set the gain-factor stream geometry and call its allocate().
    int allocGainFact( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        gainFactShmimMonitorT::m_width  = w;
        gainFactShmimMonitorT::m_height = h;
        return allocate( gainFactShmimT() );
    }

    /// Set the mult-factor stream geometry and call its allocate().
    int allocMultFact( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        multFactShmimMonitorT::m_width  = w;
        multFactShmimMonitorT::m_height = h;
        return allocate( multFactShmimT() );
    }

    /// Set the PC gain-factor stream geometry and call its allocate().
    int allocPcGainFact( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        pcGainFactShmimMonitorT::m_width  = w;
        pcGainFactShmimMonitorT::m_height = h;
        return allocate( pcGainFactShmimT() );
    }

    /// Set the PC mult-factor stream geometry and call its allocate().
    int allocPcMultFact( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        pcMultFactShmimMonitorT::m_width  = w;
        pcMultFactShmimMonitorT::m_height = h;
        return allocate( pcMultFactShmimT() );
    }

    /// Set the a-coefficient stream geometry and call its allocate().
    int allocACoeff( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        acoeffShmimMonitorT::m_width  = w;
        acoeffShmimMonitorT::m_height = h;
        return allocate( acoeffShmimT() );
    }

    /// Set the b-coefficient stream geometry and call its allocate().
    int allocBCoeff( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        bcoeffShmimMonitorT::m_width  = w;
        bcoeffShmimMonitorT::m_height = h;
        return allocate( bcoeffShmimT() );
    }

    /// Set the modeval stream geometry and call its allocate().
    int allocModeval( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        modevalShmimMonitorT::m_width  = w;
        modevalShmimMonitorT::m_height = h;
        return allocate( modevalShmimT() );
    }

    /// Set the modeval stream geometry without allocating.
    void setModevalGeometry( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        modevalShmimMonitorT::m_width  = w;
        modevalShmimMonitorT::m_height = h;
    }

    /// Process a gain-factor frame.
    int procGainFact( std::vector<float> &v /**< [in] frame */ )
    {
        return processImage( v.data(), gainFactShmimT() );
    }

    /// Process a mult-factor frame.
    int procMultFact( std::vector<float> &v /**< [in] frame */ )
    {
        return processImage( v.data(), multFactShmimT() );
    }

    /// Process a PC gain-factor frame.
    int procPcGainFact( std::vector<float> &v /**< [in] frame */ )
    {
        return processImage( v.data(), pcGainFactShmimT() );
    }

    /// Process a PC mult-factor frame.
    int procPcMultFact( std::vector<float> &v /**< [in] frame */ )
    {
        return processImage( v.data(), pcMultFactShmimT() );
    }

    /// Process an a-coefficient frame.
    int procACoeff( std::vector<float> &v /**< [in] frame */ )
    {
        return processImage( v.data(), acoeffShmimT() );
    }

    /// Process a b-coefficient frame.
    int procBCoeff( std::vector<float> &v /**< [in] frame */ )
    {
        return processImage( v.data(), bcoeffShmimT() );
    }

    /// Process a WFS modeval frame.
    int procModeval( std::vector<float> &v /**< [in] frame */ )
    {
        return processImage( v.data(), modevalShmimT() );
    }

    /// Try to wait on the filter semaphore without blocking.
    int trySem()
    {
        return sem_trywait( &m_filtSem );
    }

    /// Set the modeval circular buffer length.
    void setCBLength( int32_t len /**< [in] circular buffer length */ )
    {
        m_modevalCBLength = len;
    }

    /// Set the loop state and integrator parameters.
    void setLoop( bool  loop /**< [in] loop closed flag */,
                  float gain /**< [in] global gain */,
                  float mc /**< [in] global mult. coeff */ )
    {
        m_loop = loop;
        m_gain = gain;
        m_mc   = mc;
    }

    /// Set the PC state and parameters.
    void setPc( bool on /**< [in] PC on flag */, float gain /**< [in] PC gain */, float mc /**< [in] PC mult. coeff */ )
    {
        m_pcOn   = on;
        m_pcGain = gain;
        m_pcMc   = mc;
    }

    /// Set the shutdown flag.
    void setShutdown( int sd /**< [in] shutdown flag */ )
    {
        m_shutdown = sd;
    }

    /// Set the fps tolerance.
    void setFpsTol( float tol /**< [in] fps tolerance */ )
    {
        m_fpsTol = tol;
    }

    /// Call configureAcquisition().
    int configureAcquisitionForTest()
    {
        return configureAcquisition();
    }

    /// Call acquireAndCheckValid().
    int acquireAndCheckValidForTest()
    {
        return acquireAndCheckValid();
    }

    /// Call loadImageIntoStream().
    int loadImageIntoStreamForTest( void *dest /**< [out] destination buffer */ )
    {
        return loadImageIntoStream( dest );
    }

    /// Call checkSizes().
    void checkSizesForTest()
    {
        checkSizes();
    }

    /// Latest DM command entry.
    std::vector<float> latestDM()
    {
        return m_modevalDM[-1];
    }

    /// Number of DM command entries.
    int32_t dmSize()
    {
        return m_modevalDM.size();
    }

    /// Number of WFS modeval entries.
    int32_t wfsSize()
    {
        return m_modevalWFS.size();
    }

    /// Latest WFS modeval entry.
    std::vector<float> latestWFS()
    {
        return m_modevalWFS[-1];
    }

    /// WFS modeval circular buffer max entries.
    int32_t wfsMaxEntries()
    {
        return m_modevalWFS.maxEntries();
    }

    /// DM command circular buffer max entries.
    int32_t dmMaxEntries()
    {
        return m_modevalDM.maxEntries();
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

    /// frameGrabber reconfig flag.
    bool &fgReconfig()
    {
        return frameGrabberT::m_reconfig;
    }

    /// frameGrabber current image timestamp.
    timespec fgTimestamp()
    {
        return frameGrabberT::m_currImageTimestamp;
    }

    /// Acquisition time of the latest WFS modevals.
    timespec atime()
    {
        return m_atime;
    }

    /// frameGrabber shmim name.
    std::string fgShmimName()
    {
        return frameGrabberT::m_shmimName;
    }

    /// Gain-factor shmim name.
    std::string gainFactShmimName()
    {
        return gainFactShmimMonitorT::m_shmimName;
    }

    /// Mult-factor shmim name.
    std::string multFactShmimName()
    {
        return multFactShmimMonitorT::m_shmimName;
    }

    /// PC gain-factor shmim name.
    std::string pcGainFactShmimName()
    {
        return pcGainFactShmimMonitorT::m_shmimName;
    }

    /// PC mult-factor shmim name.
    std::string pcMultFactShmimName()
    {
        return pcMultFactShmimMonitorT::m_shmimName;
    }

    /// a-coefficient shmim name.
    std::string acoeffShmimName()
    {
        return acoeffShmimMonitorT::m_shmimName;
    }

    /// b-coefficient shmim name.
    std::string bcoeffShmimName()
    {
        return bcoeffShmimMonitorT::m_shmimName;
    }

    /// Modeval shmim name.
    std::string modevalShmimName()
    {
        return modevalShmimMonitorT::m_shmimName;
    }

    /// True if every shmimMonitor has getExistingFirst set.
    bool allGetExistingFirst()
    {
        return gainFactShmimMonitorT::m_getExistingFirst && multFactShmimMonitorT::m_getExistingFirst &&
               pcGainFactShmimMonitorT::m_getExistingFirst && pcMultFactShmimMonitorT::m_getExistingFirst &&
               acoeffShmimMonitorT::m_getExistingFirst && bcoeffShmimMonitorT::m_getExistingFirst &&
               modevalShmimMonitorT::m_getExistingFirst;
    }

    /// FPS source device.
    std::string fpsDevice()
    {
        return m_fpsDevice;
    }

    /// FPS source property.
    std::string fpsProperty()
    {
        return m_fpsProperty;
    }

    /// FPS source element.
    std::string fpsElement()
    {
        return m_fpsElement;
    }

    /// FPS tolerance.
    float fpsTol()
    {
        return m_fpsTol;
    }

    /// Loop number.
    int loopNum()
    {
        return m_loopNum;
    }

    /// Loop name.
    std::string loopName()
    {
        return m_loopName;
    }

    /// Loop closed flag.
    bool loop()
    {
        return m_loop;
    }

    /// Global gain.
    float gain()
    {
        return m_gain;
    }

    /// Global mult. coeff.
    float mc()
    {
        return m_mc;
    }

    /// PC on flag.
    bool pcOn()
    {
        return m_pcOn;
    }

    /// PC gain.
    float pcGain()
    {
        return m_pcGain;
    }

    /// PC mult. coeff.
    float pcMc()
    {
        return m_pcMc;
    }

    /// Gain factors.
    std::vector<float> &gainfacts()
    {
        return m_gainfacts;
    }

    /// Mult factors.
    std::vector<float> &multfacts()
    {
        return m_multfacts;
    }

    /// PC gain factors.
    std::vector<float> &pcGainfacts()
    {
        return m_pcGainfacts;
    }

    /// PC mult factors.
    std::vector<float> &pcMultfacts()
    {
        return m_pcMultfacts;
    }

    /// Number of a coefficients per mode.
    std::vector<int> &Na()
    {
        return m_Na;
    }

    /// Number of b coefficients per mode.
    std::vector<int> &Nb()
    {
        return m_Nb;
    }

    /// a coefficients.
    eigenImage<float> &as()
    {
        return m_as;
    }

    /// b coefficients.
    eigenImage<float> &bs()
    {
        return m_bs;
    }

    /// Modeval size.
    uint32_t modevalSz()
    {
        return m_modevalSz;
    }

    /// Integrator sizes-match flag.
    bool sizesMatch()
    {
        return m_sizesMatch;
    }

    /// PC sizes-match flag.
    bool pcSizesMatch()
    {
        return m_pcSizesMatch;
    }
};

} // namespace app
} // namespace MagAOX
/// \endcond

namespace libXWCTest
{

/** \defgroup modalFilter_unit_test modalFilter Unit Tests
 * \brief Unit tests for the modalFilter application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `modalFilter` unit tests.
/** \ingroup modalFilter_unit_test
 */
namespace modalFilterTest
{

/// Allocate a consistent N-mode integrator setup (gain/mult factors and modevals).
static void setupIntegrator( modalFilter_test &app /**< [in,out] the harness */,
                             uint32_t          N /**< [in] number of modes */,
                             int32_t           cbLen /**< [in] circular buffer length */ )
{
    app.setCBLength( cbLen );
    app.allocGainFact( N, 1 );
    app.allocMultFact( N, 1 );
    app.allocModeval( N, 1 );
}

/// Build a number property with a target element.
static pcf::IndiProperty targetProp( const std::string &device /**< [in] device name */,
                                     const std::string &name /**< [in] property name */,
                                     float              target /**< [in] target value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( "target", target ) );
    return ip;
}

/// Build a switch property with a toggle element.
static pcf::IndiProperty toggleProp( const std::string &device /**< [in] device name */,
                                     const std::string &name /**< [in] property name */,
                                     bool               on /**< [in] toggle state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( "toggle", on ? pcf::IndiElement::On : pcf::IndiElement::Off ) );
    return ip;
}

/// Verify default configuration values and derived shmim names.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter configuration defaults", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::modalFilter();
    modalFilter::setupConfig();
    modalFilter::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    modalFilter_test app( "modalfilter" );

    CHECK( app.allGetExistingFirst() );

    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/modalFilter_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( "/tmp/modalFilter_test_defaults.conf" ) == 0 );

    CHECK( app.fpsDevice() == "" );
    CHECK( app.fpsProperty() == "fps" );
    CHECK( app.fpsElement() == "current" );
    CHECK( app.fpsTol() == 0 );
    CHECK( app.loopNum() == 1 );
    CHECK( app.loopName() == "ho" );

    CHECK( app.gainFactShmimName() == "aol1_mgainfact" );
    CHECK( app.multFactShmimName() == "aol1_mmultfact" );
    CHECK( app.pcGainFactShmimName() == "aol1_mpcgainfact" );
    CHECK( app.pcMultFactShmimName() == "aol1_mpcmultfact" );
    CHECK( app.acoeffShmimName() == "aol1_acoeff" );
    CHECK( app.bcoeffShmimName() == "aol1_bcoeff" );
    CHECK( app.modevalShmimName() == "aol1_modevalWFS" );
    CHECK( app.fgShmimName() == "aol1_modevalDM" );

    // runtime defaults
    CHECK( app.loop() == false );
    CHECK( app.gain() == 0 );
    CHECK( app.mc() == 1 );
    CHECK( app.pcOn() == false );
    CHECK( app.pcGain() == 0 );
    CHECK( app.pcMc() == 1 );

    std::remove( "/tmp/modalFilter_test_defaults.conf" );
}

/// Verify configuration overrides, including loop-number-derived and explicit shmim names.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter configuration overrides", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::setupConfig();
    modalFilter::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    modalFilter_test app( "modalfilter" );
    app.setupConfigForTest();

    mx::app::writeConfigFile(
        "/tmp/modalFilter_test_override.conf",
        { "circBuff", "circBuff", "circBuff", "circBuff", "loop", "loop", "bcoeffShmim", "framegrabber" },
        { "fpsDevice", "fpsProperty", "fpsElement", "fpsTol", "number", "name", "shmimName", "shmimName" },
        { "camlowfs", "framerate", "target", "2.5", "3", "lo", "my_bcoeffs", "my_modevalDM" } );

    REQUIRE( app.loadConfigFromFile( "/tmp/modalFilter_test_override.conf" ) == 0 );

    CHECK( app.fpsDevice() == "camlowfs" );
    CHECK( app.fpsProperty() == "framerate" );
    CHECK( app.fpsElement() == "target" );
    CHECK( app.fpsTol() == Approx( 2.5 ) );
    CHECK( app.loopNum() == 3 );
    CHECK( app.loopName() == "lo" );

    CHECK( app.gainFactShmimName() == "aol3_mgainfact" );
    CHECK( app.multFactShmimName() == "aol3_mmultfact" );
    CHECK( app.pcGainFactShmimName() == "aol3_mpcgainfact" );
    CHECK( app.pcMultFactShmimName() == "aol3_mpcmultfact" );
    CHECK( app.acoeffShmimName() == "aol3_acoeff" );
    CHECK( app.bcoeffShmimName() == "my_bcoeffs" );
    CHECK( app.modevalShmimName() == "aol3_modevalWFS" );
    CHECK( app.fgShmimName() == "my_modevalDM" );

    std::remove( "/tmp/modalFilter_test_override.conf" );
}

/// Verify the gain and mult factor streams are allocated and loaded.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter gain and mult factor streams", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::allocate(const gainFactShmimT &);
    modalFilter::processImage(void *, const gainFactShmimT &);
    modalFilter::allocate(const multFactShmimT &);
    modalFilter::processImage(void *, const multFactShmimT &);
    modalFilter::allocate(const pcGainFactShmimT &);
    modalFilter::processImage(void *, const pcGainFactShmimT &);
    modalFilter::allocate(const pcMultFactShmimT &);
    modalFilter::processImage(void *, const pcMultFactShmimT &);
    #endif
    // clang-format on

    SECTION( "height other than 1 is rejected" )
    {
        modalFilter_test app( "modalfilter" );
        CHECK( app.allocGainFact( 3, 2 ) == -1 );
        CHECK( app.allocMultFact( 3, 2 ) == -1 );
        CHECK( app.allocPcGainFact( 3, 2 ) == -1 );
        CHECK( app.allocPcMultFact( 3, 2 ) == -1 );
        CHECK( app.gainfacts().size() == 0 );
        CHECK( app.multfacts().size() == 0 );
        CHECK( app.pcGainfacts().size() == 0 );
        CHECK( app.pcMultfacts().size() == 0 );
    }

    SECTION( "vectors are resized and loaded" )
    {
        modalFilter_test app( "modalfilter" );
        REQUIRE( app.allocGainFact( 3, 1 ) == 0 );
        REQUIRE( app.allocMultFact( 3, 1 ) == 0 );
        REQUIRE( app.allocPcGainFact( 3, 1 ) == 0 );
        REQUIRE( app.allocPcMultFact( 3, 1 ) == 0 );

        std::vector<float> g   = { 1.0f, 0.5f, 0.25f };
        std::vector<float> m   = { 0.99f, 0.98f, 0.97f };
        std::vector<float> pcg = { 2.0f, 3.0f, 4.0f };
        std::vector<float> pcm = { 0.1f, 0.2f, 0.3f };

        REQUIRE( app.procGainFact( g ) == 0 );
        REQUIRE( app.procMultFact( m ) == 0 );
        REQUIRE( app.procPcGainFact( pcg ) == 0 );
        REQUIRE( app.procPcMultFact( pcm ) == 0 );

        CHECK( app.gainfacts() == g );
        CHECK( app.multfacts() == m );
        CHECK( app.pcGainfacts() == pcg );
        CHECK( app.pcMultfacts() == pcm );
    }
}

/// Verify the PC a/b coefficient streams are parsed into counts and coefficient matrices.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter PC coefficient streams", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::allocate(const acoeffShmimT &);
    modalFilter::processImage(void *, const acoeffShmimT &);
    modalFilter::allocate(const bcoeffShmimT &);
    modalFilter::processImage(void *, const bcoeffShmimT &);
    #endif
    // clang-format on

    modalFilter_test app( "modalfilter" );

    // 2 modes, each column is [N, c0, c1, c2]
    REQUIRE( app.allocACoeff( 4, 2 ) == 0 );
    CHECK( app.Na().size() == 2 );
    CHECK( app.as().rows() == 3 );
    CHECK( app.as().cols() == 2 );

    std::vector<float> ac = { 3, 0.1f, 0.2f, 0.3f, 2, 0.4f, 0.5f, 0 };
    REQUIRE( app.procACoeff( ac ) == 0 );
    CHECK( app.Na()[0] == 3 );
    CHECK( app.Na()[1] == 2 );
    CHECK( app.as()( 0, 0 ) == Approx( 0.1 ) );
    CHECK( app.as()( 2, 0 ) == Approx( 0.3 ) );
    CHECK( app.as()( 0, 1 ) == Approx( 0.4 ) );
    CHECK( app.as()( 1, 1 ) == Approx( 0.5 ) );

    REQUIRE( app.allocBCoeff( 3, 2 ) == 0 );
    CHECK( app.Nb().size() == 2 );
    CHECK( app.bs().rows() == 2 );
    CHECK( app.bs().cols() == 2 );

    std::vector<float> bc = { 1, 0.9f, 0, 2, 0.7f, 0.6f };
    REQUIRE( app.procBCoeff( bc ) == 0 );
    CHECK( app.Nb()[0] == 1 );
    CHECK( app.Nb()[1] == 2 );
    CHECK( app.bs()( 0, 0 ) == Approx( 0.9 ) );
    CHECK( app.bs()( 0, 1 ) == Approx( 0.7 ) );
    CHECK( app.bs()( 1, 1 ) == Approx( 0.6 ) );
}

/// Verify the modeval stream allocation and the size consistency checks.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter modeval allocation and checkSizes", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::allocate(const modevalShmimT &);
    modalFilter::checkSizes();
    #endif
    // clang-format on

    SECTION( "height other than 1 is rejected" )
    {
        modalFilter_test app( "modalfilter" );
        CHECK( app.allocModeval( 5, 2 ) == -1 );
        CHECK( app.modevalSz() == 0 );
    }

    SECTION( "modeval size and circular buffers are set" )
    {
        modalFilter_test app( "modalfilter" );
        app.setCBLength( 7 );
        REQUIRE( app.allocModeval( 5, 1 ) == 0 );
        CHECK( app.modevalSz() == 5 );
        CHECK( app.wfsMaxEntries() == 7 );
        CHECK( app.dmMaxEntries() == 7 );

        // no gain/mult facts yet
        CHECK( app.sizesMatch() == false );
    }

    SECTION( "integrator sizes match once all vectors agree" )
    {
        modalFilter_test app( "modalfilter" );
        app.allocModeval( 4, 1 );
        app.allocGainFact( 4, 1 );
        CHECK( app.sizesMatch() == false );
        app.allocMultFact( 4, 1 );
        CHECK( app.sizesMatch() == true );

        // a mismatched gain factor breaks it again
        app.allocGainFact( 3, 1 );
        CHECK( app.sizesMatch() == false );
    }

    SECTION( "PC sizes need coefficients and full circular buffers" )
    {
        modalFilter_test app( "modalfilter" );
        app.setCBLength( 2 );
        app.allocModeval( 2, 1 );
        app.allocGainFact( 2, 1 );
        app.allocMultFact( 2, 1 );
        app.allocACoeff( 2, 2 );
        app.allocBCoeff( 2, 2 );

        CHECK( app.sizesMatch() == true );
        // circular buffers are empty
        CHECK( app.pcSizesMatch() == false );

        std::vector<float> wfs = { 0, 0 };
        app.procModeval( wfs );
        app.procModeval( wfs );
        app.setLoop( false, 0, 1 );
        app.acquireAndCheckValidForTest();
        app.acquireAndCheckValidForTest();
        REQUIRE( app.wfsSize() == 2 );
        REQUIRE( app.dmSize() == 2 );

        app.checkSizesForTest();
        CHECK( app.pcSizesMatch() == true );
    }
}

/// Verify configureAcquisition() sizes the output frame from the modevals.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter configureAcquisition", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::configureAcquisition();
    modalFilter::startAcquisition();
    modalFilter::reconfig();
    modalFilter::fps();
    #endif
    // clang-format on

    SECTION( "valid modeval geometry" )
    {
        modalFilter_test app( "modalfilter" );
        app.setModevalGeometry( 12, 1 );
        REQUIRE( app.configureAcquisitionForTest() == 0 );
        CHECK( app.fgWidth() == 12 );
        CHECK( app.fgHeight() == 1 );
        CHECK( app.fgDataType() == _DATATYPE_FLOAT );

        CHECK( app.startAcquisition() == 0 );
        CHECK( app.reconfig() == 0 );
        CHECK( app.fps() == 0 );
    }

    SECTION( "shutdown breaks the wait for a modeval stream" )
    {
        modalFilter_test app( "modalfilter" );
        app.setShutdown( 1 );
        REQUIRE( app.configureAcquisitionForTest() == 0 );
        CHECK( app.fgWidth() == 0 );
    }
}

/// Verify WFS modevals are buffered and signalled.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter modeval processImage", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::processImage(void *, const modevalShmimT &);
    #endif
    // clang-format on

    modalFilter_test app( "modalfilter" );
    app.setCBLength( 3 );
    REQUIRE( app.allocModeval( 3, 1 ) == 0 );

    std::vector<float> wfs = { 1, 2, 3 };
    REQUIRE( app.procModeval( wfs ) == 0 );

    CHECK( app.wfsSize() == 1 );
    CHECK( app.latestWFS() == wfs );
    CHECK( app.trySem() == 0 );
    CHECK( app.trySem() != 0 );

    std::vector<float> wfs2 = { 4, 5, 6 };
    REQUIRE( app.procModeval( wfs2 ) == 0 );
    CHECK( app.wfsSize() == 2 );
    CHECK( app.latestWFS() == wfs2 );
}

/// Verify the integrator filter math applied in acquireAndCheckValid().
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter integrator filter", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::acquireAndCheckValid();
    modalFilter::loadImageIntoStream(void *);
    #endif
    // clang-format on

    SECTION( "mismatched sizes skip filtering" )
    {
        modalFilter_test app( "modalfilter" );
        app.setCBLength( 4 );
        app.allocModeval( 2, 1 );

        std::vector<float> wfs = { 1, 1 };
        app.procModeval( wfs );

        CHECK( app.acquireAndCheckValidForTest() == 1 );
        CHECK( app.dmSize() == 0 );
    }

    SECTION( "open loop writes zeros and does not publish" )
    {
        modalFilter_test app( "modalfilter" );
        setupIntegrator( app, 2, 4 );
        app.setLoop( false, 0.5, 0.9 );

        std::vector<float> wfs = { 1, 1 };
        app.procModeval( wfs );

        CHECK( app.acquireAndCheckValidForTest() == 1 );
        REQUIRE( app.dmSize() == 1 );
        std::vector<float> zeros = { 0, 0 };
        CHECK( app.latestDM() == zeros );
    }

    SECTION( "closed loop leaky integrator over several frames, including buffer wrap" )
    {
        modalFilter_test app( "modalfilter" );
        setupIntegrator( app, 2, 4 );

        std::vector<float> g = { 1.0f, 2.0f };
        std::vector<float> m = { 1.0f, 0.5f };
        app.procGainFact( g );
        app.procMultFact( m );

        // one open-loop frame to seed the DM buffer with zeros
        std::vector<float> wfs = { 0, 0 };
        app.setLoop( false, 0.5, 0.9 );
        app.procModeval( wfs );
        REQUIRE( app.acquireAndCheckValidForTest() == 1 );

        app.setLoop( true, 0.5, 0.9 );

        std::vector<double>             ref    = { 0, 0 };
        std::vector<std::vector<float>> inputs = {
            { 1, 1 }, { 2, -1 }, { -0.5f, 0.25f }, { 0, 0 }, { 3, 1 }, { 1, -2 }, { 0.5f, 0.5f }, { -1, 1 } };

        for( size_t k = 0; k < inputs.size(); ++k )
        {
            app.procModeval( inputs[k] );
            REQUIRE( app.acquireAndCheckValidForTest() == 0 );

            for( size_t mode = 0; mode < 2; ++mode )
            {
                ref[mode] = -0.5 * g[mode] * inputs[k][mode] + 0.9 * m[mode] * ref[mode];
            }

            std::vector<float> dm = app.latestDM();
            REQUIRE( dm.size() == 2 );
            CHECK( dm[0] == Approx( ref[0] ).margin( 1e-5 ) );
            CHECK( dm[1] == Approx( ref[1] ).margin( 1e-5 ) );

            std::vector<float> out( 2, -99 );
            REQUIRE( app.loadImageIntoStreamForTest( out.data() ) == 0 );
            CHECK( out == dm );
        }

        CHECK( app.dmSize() == 4 );

        // the output timestamp is the WFS acquisition time
        timespec ts = app.fgTimestamp();
        timespec at = app.atime();
        CHECK( ts.tv_sec == at.tv_sec );
        CHECK( ts.tv_nsec == at.tv_nsec );
    }

    SECTION( "first closed-loop steps have the expected values" )
    {
        modalFilter_test app( "modalfilter" );
        setupIntegrator( app, 2, 4 );

        std::vector<float> g = { 1.0f, 2.0f };
        std::vector<float> m = { 1.0f, 0.5f };
        app.procGainFact( g );
        app.procMultFact( m );

        std::vector<float> wfs = { 0, 0 };
        app.setLoop( false, 0.5, 0.9 );
        app.procModeval( wfs );
        app.acquireAndCheckValidForTest();

        app.setLoop( true, 0.5, 0.9 );

        std::vector<float> w1 = { 1, 1 };
        app.procModeval( w1 );
        REQUIRE( app.acquireAndCheckValidForTest() == 0 );
        CHECK( app.latestDM()[0] == Approx( -0.5 ) );
        CHECK( app.latestDM()[1] == Approx( -1.0 ) );

        std::vector<float> w2 = { 2, -1 };
        app.procModeval( w2 );
        REQUIRE( app.acquireAndCheckValidForTest() == 0 );
        CHECK( app.latestDM()[0] == Approx( -1.45 ) );
        CHECK( app.latestDM()[1] == Approx( 0.55 ) );
    }

    SECTION( "no frame posted times out" )
    {
        modalFilter_test app( "modalfilter" );
        setupIntegrator( app, 2, 4 );

        // Waits ~1 second on the semaphore.
        CHECK( app.acquireAndCheckValidForTest() == 1 );
    }
}

/// Verify the PC branch of the filter applied in acquireAndCheckValid().
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter PC filter branch", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::acquireAndCheckValid();
    #endif
    // clang-format on

    modalFilter_test app( "modalfilter" );
    setupIntegrator( app, 2, 4 );

    app.allocPcGainFact( 2, 1 );
    app.allocPcMultFact( 2, 1 );
    std::vector<float> pcg = { 1.0f, 4.0f };
    std::vector<float> pcm = { 1.0f, 0.5f };
    app.procPcGainFact( pcg );
    app.procPcMultFact( pcm );

    // one coefficient each so the (currently unused) ARMA sums stay in range
    app.allocACoeff( 2, 2 );
    std::vector<float> ac = { 1, 0.3f, 1, 0.3f };
    app.procACoeff( ac );
    app.allocBCoeff( 2, 2 );
    std::vector<float> bc = { 1, 0.2f, 1, 0.2f };
    app.procBCoeff( bc );

    std::vector<float> g = { 100.0f, 100.0f };
    std::vector<float> m = { 100.0f, 100.0f };
    app.procGainFact( g );
    app.procMultFact( m );

    // seed the DM buffer
    std::vector<float> wfs = { 0, 0 };
    app.setLoop( false, 0, 1 );
    app.procModeval( wfs );
    app.acquireAndCheckValidForTest();

    app.setLoop( true, 0.9f, 0.9f );
    app.setPc( true, 0.25f, 0.8f );

    std::vector<double>             ref    = { 0, 0 };
    std::vector<std::vector<float>> inputs = { { 1, 1 }, { 2, -1 }, { -1, 0.5f } };

    for( size_t k = 0; k < inputs.size(); ++k )
    {
        app.procModeval( inputs[k] );
        REQUIRE( app.acquireAndCheckValidForTest() == 0 );

        // The PC branch uses the pc gain/mult parameters, not the integrator ones.
        for( size_t mode = 0; mode < 2; ++mode )
        {
            ref[mode] = -0.25 * pcg[mode] * inputs[k][mode] + 0.8 * pcm[mode] * ref[mode];
        }

        std::vector<float> dm = app.latestDM();
        CHECK( dm[0] == Approx( ref[0] ).margin( 1e-5 ) );
        CHECK( dm[1] == Approx( ref[1] ).margin( 1e-5 ) );
    }
}

/// Verify the fps source SET callback.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter fps source callback", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::setCallBack_m_indiP_fpsSource(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong device or name is rejected" )
    {
        modalFilter_test  app( "modalfilter" );
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "wrong" );
        ip.setName( "fps" );
        CHECK( app.setCallBack_m_indiP_fpsSource( ip ) == -1 );

        ip.setDevice( "camwfs" );
        ip.setName( "wrong" );
        CHECK( app.setCallBack_m_indiP_fpsSource( ip ) == -1 );
    }

    SECTION( "missing element is ignored" )
    {
        modalFilter_test  app( "modalfilter" );
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "camwfs" );
        ip.setName( "fps" );
        ip.add( pcf::IndiElement( "target", 1000.0f ) );

        CHECK( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        CHECK( app.fps() == 0 );
    }

    SECTION( "fps update respects the tolerance" )
    {
        modalFilter_test app( "modalfilter" );
        app.fgReconfig() = false;

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "camwfs" );
        ip.setName( "fps" );
        ip.add( pcf::IndiElement( "current", 1000.0f ) );

        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        CHECK( app.fps() == Approx( 1000 ) );
        CHECK( app.fgReconfig() == true );

        app.fgReconfig() = false;
        app.setFpsTol( 5 );
        ip["current"].set( 1003.0f );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        CHECK( app.fps() == Approx( 1000 ) );
        CHECK( app.fgReconfig() == false );

        ip["current"].set( 1010.0f );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( ip ) == 0 );
        CHECK( app.fps() == Approx( 1010 ) );
        CHECK( app.fgReconfig() == true );
    }
}

/// Verify the loop and PC toggle NEW callbacks.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter loop and pcOn toggle callbacks", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::newCallBack_m_indiP_loop(const pcf::IndiProperty &);
    modalFilter::newCallBack_m_indiP_pcOn(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong device or name is rejected" )
    {
        modalFilter_test app( "modalfilter" );
        CHECK( app.newCallBack_m_indiP_loop( toggleProp( "wrong", "loop_state", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_loop( toggleProp( "modalfilter", "wrong", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_pcOn( toggleProp( "wrong", "loop_pcon", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_pcOn( toggleProp( "modalfilter", "wrong", true ) ) == -1 );
        CHECK( app.loop() == false );
        CHECK( app.pcOn() == false );
    }

    SECTION( "loop toggles on and off" )
    {
        modalFilter_test app( "modalfilter" );
        REQUIRE( app.newCallBack_m_indiP_loop( toggleProp( "modalfilter", "loop_state", true ) ) == 0 );
        CHECK( app.loop() == true );
        REQUIRE( app.newCallBack_m_indiP_loop( toggleProp( "modalfilter", "loop_state", false ) ) == 0 );
        CHECK( app.loop() == false );
    }

    SECTION( "pcOn toggles on and off" )
    {
        modalFilter_test app( "modalfilter" );
        REQUIRE( app.newCallBack_m_indiP_pcOn( toggleProp( "modalfilter", "loop_pcon", true ) ) == 0 );
        CHECK( app.pcOn() == true );
        REQUIRE( app.newCallBack_m_indiP_pcOn( toggleProp( "modalfilter", "loop_pcon", false ) ) == 0 );
        CHECK( app.pcOn() == false );
    }

    SECTION( "missing toggle element is ignored" )
    {
        modalFilter_test  app( "modalfilter" );
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "modalfilter" );
        ip.setName( "loop_state" );
        CHECK( app.newCallBack_m_indiP_loop( ip ) == 0 );
        CHECK( app.loop() == false );
    }
}

/// Verify the gain and mult coefficient NEW callbacks.
/**
 * \ingroup modalFilter_unit_test
 */
TEST_CASE( "modalFilter gain and mult callbacks", "[modalFilter]" )
{
    // clang-format off
    #ifdef MODALFILTER_TEST_DOXYGEN_REF
    modalFilter::newCallBack_m_indiP_gain(const pcf::IndiProperty &);
    modalFilter::newCallBack_m_indiP_mult(const pcf::IndiProperty &);
    modalFilter::newCallBack_m_indiP_pcGain(const pcf::IndiProperty &);
    modalFilter::newCallBack_m_indiP_pcMult(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong device or name is rejected" )
    {
        modalFilter_test app( "modalfilter" );
        CHECK( app.newCallBack_m_indiP_gain( targetProp( "wrong", "loop_gain", 0.5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_gain( targetProp( "modalfilter", "wrong", 0.5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_mult( targetProp( "wrong", "loop_multcoeff", 0.5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_pcGain( targetProp( "wrong", "loop_pcgain", 0.5 ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_pcMult( targetProp( "wrong", "loop_pcmultcoeff", 0.5 ) ) == -1 );
        CHECK( app.gain() == 0 );
        CHECK( app.mc() == 1 );
        CHECK( app.pcGain() == 0 );
        CHECK( app.pcMc() == 1 );
    }

    SECTION( "targets set the parameters" )
    {
        modalFilter_test app( "modalfilter" );
        REQUIRE( app.newCallBack_m_indiP_gain( targetProp( "modalfilter", "loop_gain", 0.35 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_mult( targetProp( "modalfilter", "loop_multcoeff", 0.99 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_pcGain( targetProp( "modalfilter", "loop_pcgain", 0.2 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_pcMult( targetProp( "modalfilter", "loop_pcmultcoeff", 0.95 ) ) == 0 );

        CHECK( app.gain() == Approx( 0.35 ) );
        CHECK( app.mc() == Approx( 0.99 ) );
        CHECK( app.pcGain() == Approx( 0.2 ) );
        CHECK( app.pcMc() == Approx( 0.95 ) );
    }

    SECTION( "current is used when there is no target" )
    {
        modalFilter_test  app( "modalfilter" );
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "modalfilter" );
        ip.setName( "loop_gain" );
        ip.add( pcf::IndiElement( "current", 0.6f ) );
        REQUIRE( app.newCallBack_m_indiP_gain( ip ) == 0 );
        CHECK( app.gain() == Approx( 0.6 ) );
    }

    SECTION( "no target or current is an error" )
    {
        modalFilter_test  app( "modalfilter" );
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "modalfilter" );
        ip.setName( "loop_gain" );
        ip.add( pcf::IndiElement( "other", 0.6f ) );
        CHECK( app.newCallBack_m_indiP_gain( ip ) == -1 );
        CHECK( app.gain() == 0 );
    }
}

} // namespace modalFilterTest

} // namespace libXWCTest
