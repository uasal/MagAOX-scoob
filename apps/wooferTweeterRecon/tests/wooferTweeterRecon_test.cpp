/** \file wooferTweeterRecon_test.cpp
 * \brief Catch2 tests for the wooferTweeterRecon app.
 * \author Claude Code
 *
 * The callback bodies are live in this translation unit (testMacrosINDI.hpp is not included).  The set
 * callbacks only check the property name, not the device, so device/name validation is tested explicitly.
 *
 * \ingroup wooferTweeterRecon_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../wooferTweeterRecon.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup wooferTweeterRecon_unit_test wooferTweeterRecon Unit Tests
 * \brief Unit tests for the wooferTweeterRecon application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `wooferTweeterRecon` unit tests.
/** \ingroup wooferTweeterRecon_unit_test
 */
namespace wooferTweeterReconTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// The woofer mode-value shmimMonitor base of wooferTweeterRecon.
typedef dev::shmimMonitor<wooferTweeterRecon, wooferModesShmimT> wooferSMT;

/// The tweeter mode-value shmimMonitor base of wooferTweeterRecon.
typedef dev::shmimMonitor<wooferTweeterRecon, tweeterModesShmimT> tweeterSMT;

/// The WFS mode-value shmimMonitor base of wooferTweeterRecon.
typedef dev::shmimMonitor<wooferTweeterRecon, wfsModesShmimT> wfsSMT;

/// Test harness exposing wooferTweeterRecon internals.
class wooferTweeterRecon_test : public wooferTweeterRecon
{
  public:
    /// Metadata for the in-memory woofer stream (only the timestamps are used).
    IMAGE_METADATA m_testWooferMd;

    /// Metadata for the in-memory tweeter stream (only the timestamps are used).
    IMAGE_METADATA m_testTweeterMd;

    /// Metadata for the in-memory WFS stream (only the timestamps are used).
    IMAGE_METADATA m_testWfsMd;

    /// Construct a harness with the given device name.
    explicit wooferTweeterRecon_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        memset( &m_testWooferMd, 0, sizeof( m_testWooferMd ) );
        memset( &m_testTweeterMd, 0, sizeof( m_testTweeterMd ) );
        memset( &m_testWfsMd, 0, sizeof( m_testWfsMd ) );

        wooferSMT::m_imageStream.md  = &m_testWooferMd;
        tweeterSMT::m_imageStream.md = &m_testTweeterMd;
        wfsSMT::m_imageStream.md     = &m_testWfsMd;
    }

    using wooferTweeterRecon::m_el;
    using wooferTweeterRecon::m_elSource;
    using wooferTweeterRecon::m_fps;
    using wooferTweeterRecon::m_fpsSource;
    using wooferTweeterRecon::m_invFps;
    using wooferTweeterRecon::m_lastr0;
    using wooferTweeterRecon::m_lastTweeterVal;
    using wooferTweeterRecon::m_lastWfsVal;
    using wooferTweeterRecon::m_lastWooferVal;
    using wooferTweeterRecon::m_modevalCircBuffLen;
    using wooferTweeterRecon::m_nloaded;
    using wooferTweeterRecon::m_nvals;
    using wooferTweeterRecon::m_opticalGain;
    using wooferTweeterRecon::m_outputVal;
    using wooferTweeterRecon::m_r0;
    using wooferTweeterRecon::m_sig;
    using wooferTweeterRecon::m_tweeterModesReady;
    using wooferTweeterRecon::m_tweeterOffset;
    using wooferTweeterRecon::m_tweeterVals;
    using wooferTweeterRecon::m_wfsModesReady;
    using wooferTweeterRecon::m_wfsOffset;
    using wooferTweeterRecon::m_wfsVals;
    using wooferTweeterRecon::m_wooferModesReady;
    using wooferTweeterRecon::m_wooferOffset;
    using wooferTweeterRecon::m_wooferVals;

    using wooferTweeterRecon::m_indiP_elSource;
    using wooferTweeterRecon::m_indiP_fps;
    using wooferTweeterRecon::m_indiP_fpsSource;

    using wooferTweeterRecon::setCallBack_m_indiP_elSource;
    using wooferTweeterRecon::setCallBack_m_indiP_fpsSource;

    /// Access the woofer stream width.
    /**
     * \returns a reference to the woofer shmimMonitor width
     */
    uint32_t &wooferWidth()
    {
        return wooferSMT::m_width;
    }

    /// Access the tweeter stream width.
    /**
     * \returns a reference to the tweeter shmimMonitor width
     */
    uint32_t &tweeterWidth()
    {
        return tweeterSMT::m_width;
    }

    /// Access the WFS stream width.
    /**
     * \returns a reference to the WFS shmimMonitor width
     */
    uint32_t &wfsWidth()
    {
        return wfsSMT::m_width;
    }

    /// Access the woofer shmimMonitor restart flag.
    /**
     * \returns a reference to the woofer restart flag
     */
    bool &wooferRestart()
    {
        return wooferSMT::m_restart;
    }

    /// Access the tweeter shmimMonitor restart flag.
    /**
     * \returns a reference to the tweeter restart flag
     */
    bool &tweeterRestart()
    {
        return tweeterSMT::m_restart;
    }

    /// Access the WFS shmimMonitor restart flag.
    /**
     * \returns a reference to the WFS restart flag
     */
    bool &wfsRestart()
    {
        return wfsSMT::m_restart;
    }

    /// Access the woofer shmimMonitor shmim name.
    /**
     * \returns the woofer shmim name
     */
    std::string wooferShmimName()
    {
        return wooferSMT::m_shmimName;
    }

    /// Access the tweeter shmimMonitor shmim name.
    /**
     * \returns the tweeter shmim name
     */
    std::string tweeterShmimName()
    {
        return tweeterSMT::m_shmimName;
    }

    /// Access the WFS shmimMonitor shmim name.
    /**
     * \returns the WFS shmim name
     */
    std::string wfsShmimName()
    {
        return wfsSMT::m_shmimName;
    }

    /// Access the woofer shmimMonitor getExistingFirst flag.
    /**
     * \returns the woofer getExistingFirst flag
     */
    bool wooferGetExistingFirst()
    {
        return wooferSMT::m_getExistingFirst;
    }

    /// Access the WFS shmimMonitor getExistingFirst flag.
    /**
     * \returns the WFS getExistingFirst flag
     */
    bool wfsGetExistingFirst()
    {
        return wfsSMT::m_getExistingFirst;
    }

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Set the property keys of the set-property sources the way `appStartup()` does.
    void setupProperties()
    {
        m_indiP_fpsSource.setDevice( m_fpsSource );
        m_indiP_fpsSource.setName( "fps" );

        m_indiP_elSource.setDevice( m_elSource );
        m_indiP_elSource.setName( "telpos" );

        createROIndiNumber( m_indiP_fps, "fps" );
        m_indiP_fps.add( pcf::IndiElement( "current" ) );
    }

    /// Allocate all three mode-value buffers in the order that succeeds.
    /**
     * \returns 0 if all three allocations report ready, -1 otherwise
     */
    int allocateAll( uint32_t circLen, /**< [in] length of the mode-value circular buffers */
                     uint32_t width    /**< [in] number of modes in each stream */
    )
    {
        m_modevalCircBuffLen = circLen;
        tweeterWidth()       = width;
        wfsWidth()           = width;
        wooferWidth()        = width;

        allocate( tweeterModesShmimT() );
        allocate( wfsModesShmimT() );
        allocate( wooferModesShmimT() );

        if( m_tweeterModesReady && m_wfsModesReady && m_wooferModesReady )
        {
            return 0;
        }

        return -1;
    }

    /// Feed a woofer frame with the given acquisition time through `processImage()`.
    /**
     * \returns the processImage return value
     */
    int feedWoofer( std::vector<float> vals, /**< [in] the mode values */
                    time_t             sec,  /**< [in] the acquisition time seconds */
                    long               nsec  /**< [in] the acquisition time nanoseconds */
    )
    {
        m_testWooferMd.atime.tv_sec  = sec;
        m_testWooferMd.atime.tv_nsec = nsec;
        return processImage( vals.data(), wooferModesShmimT() );
    }

    /// Feed a tweeter frame with the given acquisition time through `processImage()`.
    /**
     * \returns the processImage return value
     */
    int feedTweeter( std::vector<float> vals, /**< [in] the mode values */
                     time_t             sec,  /**< [in] the acquisition time seconds */
                     long               nsec  /**< [in] the acquisition time nanoseconds */
    )
    {
        m_testTweeterMd.atime.tv_sec  = sec;
        m_testTweeterMd.atime.tv_nsec = nsec;
        return processImage( vals.data(), tweeterModesShmimT() );
    }

    /// Feed a WFS frame with the given write time through `processImage()`.
    /**
     * \returns the processImage return value
     */
    int feedWfs( std::vector<float> vals, /**< [in] the mode values */
                 time_t             sec,  /**< [in] the write time seconds */
                 long               nsec  /**< [in] the write time nanoseconds */
    )
    {
        m_testWfsMd.writetime.tv_sec  = sec;
        m_testWfsMd.writetime.tv_nsec = nsec;
        return processImage( vals.data(), wfsModesShmimT() );
    }
};

/// Build a Number property with a single element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty numberProp( const std::string &device, /**< [in] INDI device name */
                              const std::string &name,   /**< [in] INDI property name */
                              const std::string &el,     /**< [in] element name */
                              float              value   /**< [in] element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, value ) );
    return ip;
}

/// The r0 estimate made by `recon()` from the variance of a reconstructed wavefront.
/**
 * \returns r0 in meters
 */
float expectedR0( float var /**< [in] the sum of squares of the reconstructed mode values */ )
{
    return pow( 1.0299 * pow( 6.5, 5. / 3. ) / ( 4 * var * pow( 2 * 3.14159 / 0.5, 2 ) ), 3. / 5. );
}

/// \endcond

/// Verify the wooferTweeterRecon configuration defaults and overrides.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon configuration", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::setupConfig();
    wooferTweeterRecon::loadConfig();
    wooferTweeterRecon::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/wooferTweeterRecon_test_config.conf";

    SECTION( "defaults" )
    {
        mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

        wooferTweeterRecon_test app( "wtrecon" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_fpsSource == "camwfs" );
        REQUIRE( app.m_elSource == "tcsi" );
        REQUIRE( app.m_wooferOffset == Approx( 500e-6 ) );
        REQUIRE( app.m_tweeterOffset == Approx( 50e-6 ) );
        REQUIRE( app.m_wfsOffset == Approx( -10e-6 ) );
        REQUIRE( app.m_modevalCircBuffLen == 5000u );
        REQUIRE( app.m_opticalGain == Approx( 0.8f ) );
        REQUIRE( app.m_el == Approx( 90.0f ) );

        REQUIRE( app.wooferShmimName() == "aol0_modevalDMf_mon" );
        REQUIRE( app.tweeterShmimName() == "aol1_modevalDMf_mon" );
        REQUIRE( app.wfsShmimName() == "aol1_modevalWFS" );
        REQUIRE( app.wooferGetExistingFirst() == true );
        REQUIRE( app.wfsGetExistingFirst() == true );
    }

    SECTION( "overrides" )
    {
        mx::app::writeConfigFile(
            fname,
            { "integrator", "woofer", "tweeter", "wfs", "wooferModes", "tweeterModes", "wfsModes", "wfsModes" },
            { "fpsSource", "offset", "offset", "offset", "shmimName", "shmimName", "shmimName", "getExistingFirst" },
            { "camlowfs", "0.001", "0.0002", "-0.00003", "woof_mv", "tweet_mv", "wfs_mv", "false" } );

        wooferTweeterRecon_test app( "wtrecon" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_fpsSource == "camlowfs" );
        REQUIRE( app.m_wooferOffset == Approx( 0.001 ) );
        REQUIRE( app.m_tweeterOffset == Approx( 0.0002 ) );
        REQUIRE( app.m_wfsOffset == Approx( -0.00003 ) );

        REQUIRE( app.wooferShmimName() == "woof_mv" );
        REQUIRE( app.tweeterShmimName() == "tweet_mv" );
        REQUIRE( app.wfsShmimName() == "wfs_mv" );
        REQUIRE( app.wooferGetExistingFirst() == true );
        REQUIRE( app.wfsGetExistingFirst() == false );
    }

    remove( fname.c_str() );
}

/// Verify the buffer allocation sequence and its size checks.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon allocate", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::allocate( wooferModesShmimT() );
    wooferTweeterRecon::allocate( tweeterModesShmimT() );
    wooferTweeterRecon::allocate( wfsModesShmimT() );
    #endif
    // clang-format on

    SECTION( "tweeter allocation sizes the buffers and restarts the other monitors" )
    {
        wooferTweeterRecon_test app( "wtrecon" );
        app.m_modevalCircBuffLen = 7;
        app.tweeterWidth()       = 4;

        REQUIRE( app.allocate( tweeterModesShmimT() ) == 0 );

        REQUIRE( app.m_tweeterModesReady == true );
        REQUIRE( app.wfsRestart() == true );
        REQUIRE( app.wooferRestart() == true );
        REQUIRE( app.m_tweeterVals.size() == 7u );
        for( auto &v : app.m_tweeterVals )
        {
            REQUIRE( v.vals.size() == 4u );
            REQUIRE( v.t == 0 );
            REQUIRE( v.reconstructed == false );
        }
        REQUIRE( app.m_r0.size() == 3600u * 30u );
        REQUIRE( app.m_sig.size() == 3600u * 30u );
        REQUIRE( app.m_outputVal.rows() == 4 );
        REQUIRE( app.m_outputVal.cols() == app.m_nvals );
    }

    SECTION( "full allocation sequence" )
    {
        wooferTweeterRecon_test app( "wtrecon" );

        REQUIRE( app.allocateAll( 6, 3 ) == 0 );
        REQUIRE( app.m_wfsVals.size() == 6u );
        REQUIRE( app.m_wooferVals.size() == 6u );
        REQUIRE( app.m_wfsVals[0].vals.size() == 3u );
        REQUIRE( app.m_wooferVals[5].vals.size() == 3u );
    }

    SECTION( "the woofer may have fewer modes than the wfs" )
    {
        wooferTweeterRecon_test app( "wtrecon" );
        app.m_modevalCircBuffLen = 4;
        app.tweeterWidth()       = 5;
        app.wfsWidth()           = 5;
        app.wooferWidth()        = 2;

        app.allocate( tweeterModesShmimT() );
        app.allocate( wfsModesShmimT() );
        REQUIRE( app.allocate( wooferModesShmimT() ) == 0 );
        REQUIRE( app.m_wooferModesReady == true );
        REQUIRE( app.m_wooferVals[0].vals.size() == 2u );
    }

    SECTION( "wfs allocation waits for a matching tweeter" )
    {
        wooferTweeterRecon_test app( "wtrecon" );
        app.m_modevalCircBuffLen = 4;
        app.tweeterWidth()       = 5;
        app.allocate( tweeterModesShmimT() );
        app.tweeterRestart() = false;

        app.wfsWidth()   = 6; // mismatch
        app.wfsRestart() = false;

        REQUIRE( app.allocate( wfsModesShmimT() ) == 0 ); // sleeps 1 second
        REQUIRE( app.m_wfsModesReady == false );
        REQUIRE( app.wfsRestart() == true );
        REQUIRE( app.tweeterRestart() == true );
        REQUIRE( app.m_wfsVals.size() == 0u );
    }

    SECTION( "woofer allocation waits for the wfs" )
    {
        wooferTweeterRecon_test app( "wtrecon" );
        app.wooferWidth()   = 2;
        app.wooferRestart() = false;
        app.wfsRestart()    = false;

        REQUIRE( app.allocate( wooferModesShmimT() ) == 0 ); // sleeps 1 second
        REQUIRE( app.m_wooferModesReady == false );
        REQUIRE( app.wooferRestart() == true );
        REQUIRE( app.wfsRestart() == false ); // wfs was not ready, so it is not restarted
        REQUIRE( app.m_wooferVals.size() == 0u );
    }
}

/// Verify that processImage stores the mode values and timestamps in the circular buffers.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon processImage timestamps and circular buffers", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::processImage( nullptr, wooferModesShmimT() );
    wooferTweeterRecon::processImage( nullptr, tweeterModesShmimT() );
    wooferTweeterRecon::processImage( nullptr, wfsModesShmimT() );
    #endif
    // clang-format on

    wooferTweeterRecon_test app( "wtrecon" );
    REQUIRE( app.allocateAll( 3, 2 ) == 0 );

    SECTION( "tweeter values use atime plus the tweeter offset and wrap around" )
    {
        REQUIRE( app.feedTweeter( { 1, 2 }, 100, 500000000 ) == 0 );
        REQUIRE( app.m_lastTweeterVal == 1u );
        REQUIRE( app.m_tweeterVals[1].t == Approx( 100.5 + 50e-6 ).epsilon( 1e-12 ) );
        REQUIRE( app.m_tweeterVals[1].vals[0] == 1.0f );
        REQUIRE( app.m_tweeterVals[1].vals[1] == 2.0f );
        REQUIRE( app.m_tweeterVals[1].reconstructed == false );

        app.feedTweeter( { 3, 4 }, 101, 0 );
        REQUIRE( app.m_lastTweeterVal == 2u );

        app.feedTweeter( { 5, 6 }, 102, 0 );
        REQUIRE( app.m_lastTweeterVal == 0u );
        REQUIRE( app.m_tweeterVals[0].vals[0] == 5.0f );
        REQUIRE( app.m_tweeterVals[0].t == Approx( 102 + 50e-6 ).epsilon( 1e-12 ) );
    }

    SECTION( "wfs values use writetime minus 1/fps plus the wfs offset" )
    {
        app.setupProperties();
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "current", 100.0f ) ) == 0 );
        REQUIRE( app.m_invFps == Approx( 0.01f ) );

        REQUIRE( app.feedWfs( { 7, 8 }, 200, 250000000 ) == 0 );
        REQUIRE( app.m_lastWfsVal == 1u );
        REQUIRE( app.m_wfsVals[1].t == Approx( 200.25 - 0.01 - 10e-6 ).epsilon( 1e-9 ) );
        REQUIRE( app.m_wfsVals[1].vals[0] == 7.0f );
        REQUIRE( app.m_wfsVals[1].vals[1] == 8.0f );
    }

    SECTION( "woofer values use atime plus the woofer offset" )
    {
        REQUIRE( app.feedWoofer( { 9, 10 }, 300, 0 ) == 0 );
        REQUIRE( app.m_lastWooferVal == 1u );
        REQUIRE( app.m_wooferVals[1].t == Approx( 300 + 500e-6 ).epsilon( 1e-12 ) );
        REQUIRE( app.m_wooferVals[1].vals[0] == 9.0f );
        REQUIRE( app.m_wooferVals[1].vals[1] == 10.0f );
        REQUIRE( app.m_wooferVals[1].reconstructed == false );
    }
}

/// Verify the pseudo-open-loop reconstruction from interpolated woofer and tweeter values.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon recon interpolates and combines", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::recon();
    wooferTweeterRecon::processImage( nullptr, wooferModesShmimT() );
    #endif
    // clang-format on

    wooferTweeterRecon_test app( "wtrecon" );
    REQUIRE( app.allocateAll( 8, 2 ) == 0 );

    app.m_wooferOffset  = 0;
    app.m_tweeterOffset = 0;
    app.m_wfsOffset     = 0;
    app.m_invFps        = 0;

    // Tweeter brackets the WFS time at t = 1 and t = 3
    app.feedTweeter( { 1, 2 }, 1, 0 );
    app.feedTweeter( { 3, 4 }, 3, 0 );

    // WFS measurement at t = 2
    app.feedWfs( { 0.8f, 1.6f }, 2, 0 );

    // First woofer value is earlier than the WFS, so nothing is reconstructed yet
    REQUIRE( app.feedWoofer( { 10, 20 }, 1, 0 ) == 0 );
    REQUIRE( app.m_wfsVals[1].reconstructed == false );
    REQUIRE( app.m_lastr0 == 0u );

    // Second woofer value brackets the WFS time, so the reconstruction happens
    REQUIRE( app.feedWoofer( { 30, 40 }, 3, 0 ) == 0 );
    REQUIRE( app.m_wfsVals[1].reconstructed == true );
    REQUIRE( app.m_lastr0 == 1u );
    REQUIRE( app.m_nloaded == 0 );

    // wval = {20, 30}, tval = {2, 3}, wfsval = {0.8, 1.6}/0.8 = {1, 2}
    // output = 0.04 * wval + tval + wfsval
    REQUIRE( app.m_outputVal( 0, 0 ) == Approx( 0.8 + 2 + 1 ) );
    REQUIRE( app.m_outputVal( 1, 0 ) == Approx( 1.2 + 3 + 2 ) );

    REQUIRE( app.m_sig[1] == Approx( 1.0 + 4.0 ) );

    float var = 3.8f * 3.8f + 6.2f * 6.2f;
    REQUIRE( app.m_r0[1] == Approx( expectedR0( var ) ).epsilon( 1e-4 ) );

    // A second recon does nothing because the latest WFS value is already reconstructed
    REQUIRE( app.recon() == 0 );
    REQUIRE( app.m_lastr0 == 1u );

    // A new WFS value at t = 2.5 is bracketed by the existing woofer (1, 3) and tweeter (1, 3) values
    app.feedWfs( { 0, 0 }, 2, 500000000 );
    REQUIRE( app.recon() == 0 );
    REQUIRE( app.m_wfsVals[2].reconstructed == true );
    REQUIRE( app.m_lastr0 == 2u );

    // wdt = tdt = 0.75: wval = {25, 35}, tval = {2.5, 3.5}, wfsval = 0
    REQUIRE( app.m_outputVal( 0, 0 ) == Approx( 1.0 + 2.5 ) );
    REQUIRE( app.m_outputVal( 1, 0 ) == Approx( 1.4 + 3.5 ) );
    REQUIRE( app.m_sig[2] == Approx( 0.0 ).margin( 1e-12 ) );
}

/// Verify that recon skips WFS values that are not bracketed by tweeter values.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon recon waits for the tweeter", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::recon();
    #endif
    // clang-format on

    wooferTweeterRecon_test app( "wtrecon" );
    REQUIRE( app.allocateAll( 8, 1 ) == 0 );

    app.m_wooferOffset  = 0;
    app.m_tweeterOffset = 0;
    app.m_wfsOffset     = 0;
    app.m_invFps        = 0;

    // Only one tweeter value, earlier than the WFS
    app.feedTweeter( { 1 }, 1, 0 );
    app.feedWfs( { 1 }, 2, 0 );
    app.feedWoofer( { 1 }, 1, 0 );
    REQUIRE( app.feedWoofer( { 2 }, 3, 0 ) == 0 );

    REQUIRE( app.m_wfsVals[1].reconstructed == false );
    REQUIRE( app.m_lastr0 == 0u );

    // Once a later tweeter value arrives, the next woofer frame triggers the reconstruction
    app.feedTweeter( { 3 }, 3, 0 );
    REQUIRE( app.feedWoofer( { 3 }, 4, 0 ) == 0 );
    REQUIRE( app.m_wfsVals[1].reconstructed == true );
    REQUIRE( app.m_lastr0 == 1u );

    // woofer brackets (1 -> 3) at 0.5 is 1.5, tweeter (1 -> 3) at 0.5 is 2, wfs 1/0.8
    REQUIRE( app.m_outputVal( 0, 0 ) == Approx( 0.04 * 1.5 + 2 + 1.25 ) );
}

/// Verify the fps set callback.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon fps source callback", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::setCallBack_m_indiP_fpsSource( pcf::IndiProperty() );
    #endif
    // clang-format on

    wooferTweeterRecon_test app( "wtrecon" );
    app.setupProperties();

    SECTION( "wrong property name is rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "wrong", "current", 100.0f ) ) == -1 );
        REQUIRE( app.m_fps == 0 );
    }

    SECTION( "missing current element is ignored" )
    {
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "target", 100.0f ) ) == 0 );
        REQUIRE( app.m_fps == 0 );
        REQUIRE( app.m_invFps == 0 );
    }

    SECTION( "current sets fps and its inverse" )
    {
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "current", 2000.0f ) ) == 0 );
        REQUIRE( app.m_fps == Approx( 2000.0f ) );
        REQUIRE( app.m_invFps == Approx( 0.0005f ) );

        // A non-positive fps sets the inverse to 0
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "current", 0.0f ) ) == 0 );
        REQUIRE( app.m_fps == 0 );
        REQUIRE( app.m_invFps == 0 );

        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "current", -5.0f ) ) == 0 );
        REQUIRE( app.m_fps == Approx( -5.0f ) );
        REQUIRE( app.m_invFps == 0 );
    }
}

/// Verify the elevation set callback.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon elevation source callback", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::setCallBack_m_indiP_elSource( pcf::IndiProperty() );
    #endif
    // clang-format on

    wooferTweeterRecon_test app( "wtrecon" );
    app.setupProperties();

    SECTION( "wrong property name is rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_elSource( numberProp( "tcsi", "wrong", "el", 45.0f ) ) == -1 );
        REQUIRE( app.m_el == Approx( 90.0f ) );
    }

    SECTION( "missing el element is ignored" )
    {
        REQUIRE( app.setCallBack_m_indiP_elSource( numberProp( "tcsi", "telpos", "az", 45.0f ) ) == 0 );
        REQUIRE( app.m_el == Approx( 90.0f ) );
    }

    SECTION( "el sets the elevation" )
    {
        REQUIRE( app.setCallBack_m_indiP_elSource( numberProp( "tcsi", "telpos", "el", 45.0f ) ) == 0 );
        REQUIRE( app.m_el == Approx( 45.0f ) );
    }
}

/// Verify that appShutdown succeeds when no monitor threads were started.
/**
 * \ingroup wooferTweeterRecon_unit_test
 */
TEST_CASE( "wooferTweeterRecon appShutdown without threads", "[wooferTweeterRecon]" )
{
    // clang-format off
    #ifdef WOOFERTWEETERRECON_TEST_DOXYGEN_REF
    wooferTweeterRecon::appShutdown();
    #endif
    // clang-format on

    wooferTweeterRecon_test app( "wtrecon" );
    REQUIRE( app.appShutdown() == 0 );
}

} // namespace wooferTweeterReconTest

} // namespace libXWCTest
