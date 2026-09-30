/** \file t2wOffloader_test.cpp
 * \brief Catch2 tests for the t2wOffloader app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * The callback bodies are live in this translation unit (testMacrosINDI.hpp is not included).  The
 * device/name validation tests are in t2wOffloader_indi_test.cpp.
 *
 * \ingroup t2wOffloader_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "../t2wOffloader.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup t2wOffloader_unit_test t2wOffloader Unit Tests
 * \brief Unit tests for the t2wOffloader application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `t2wOffloader` unit tests.
/** \ingroup t2wOffloader_unit_test
 */
namespace t2wOffloaderTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing t2wOffloader internals.
class t2wOffloader_test : public t2wOffloader
{
  public:
    /// Metadata for the in-memory woofer DM stream (no shared memory, no semaphores).
    IMAGE_METADATA m_testDmMd;

    /// Pixel buffer for the in-memory woofer DM stream.
    std::vector<float> m_testDmData;

    /// Construct a harness with the given device name.
    explicit t2wOffloader_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        memset( &m_testDmMd, 0, sizeof( m_testDmMd ) );
        memset( &m_dmStream, 0, sizeof( m_dmStream ) );
    }

    using t2wOffloader::m_actLim;
    using t2wOffloader::m_dmChannel;
    using t2wOffloader::m_dmHeight;
    using t2wOffloader::m_dmStream;
    using t2wOffloader::m_dmWidth;
    using t2wOffloader::m_effFPS;
    using t2wOffloader::m_fps;
    using t2wOffloader::m_fpsSource;
    using t2wOffloader::m_gain;
    using t2wOffloader::m_leak;
    using t2wOffloader::m_loopNumber;
    using t2wOffloader::m_maxModes;
    using t2wOffloader::m_navg;
    using t2wOffloader::m_navgSource;
    using t2wOffloader::m_numModes;
    using t2wOffloader::m_offloading;
    using t2wOffloader::m_tweeterMaskFile;
    using t2wOffloader::m_tweeterModeFile;
    using t2wOffloader::m_twRespM;
    using t2wOffloader::m_twRespMPath;
    using t2wOffloader::m_woofer;
    using t2wOffloader::m_wooferMaskFile;

    using t2wOffloader::m_indiP_actLim;
    using t2wOffloader::m_indiP_fps;
    using t2wOffloader::m_indiP_fpsSource;
    using t2wOffloader::m_indiP_gain;
    using t2wOffloader::m_indiP_leak;
    using t2wOffloader::m_indiP_navgSource;
    using t2wOffloader::m_indiP_numModes;
    using t2wOffloader::m_indiP_offloadToggle;
    using t2wOffloader::m_indiP_zero;

    using t2wOffloader::newCallBack_m_indiP_actLim;
    using t2wOffloader::newCallBack_m_indiP_gain;
    using t2wOffloader::newCallBack_m_indiP_leak;
    using t2wOffloader::newCallBack_m_indiP_numModes;
    using t2wOffloader::newCallBack_m_indiP_offloadToggle;
    using t2wOffloader::newCallBack_m_indiP_zero;
    using t2wOffloader::setCallBack_m_indiP_fpsSource;
    using t2wOffloader::setCallBack_m_indiP_navgSource;

    using t2wOffloader::checkRecordTimes;
    using t2wOffloader::recordLoopGain;
    using t2wOffloader::recordOffloading;
    using t2wOffloader::recordTelem;

    using dev::shmimMonitor<t2wOffloader>::m_height;
    using dev::shmimMonitor<t2wOffloader>::m_shmimName;
    using dev::shmimMonitor<t2wOffloader>::m_typeSize;
    using dev::shmimMonitor<t2wOffloader>::m_width;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();

        // Keep telemetry records out of the queue so nothing is written at destruction.
        m_tel.m_logLevel = logPrio::LOG_INFO;
    }

    /// Create the INDI properties the way `appStartup()` does, without reading mode files or starting threads.
    void setupProperties()
    {
        createStandardIndiNumber<float>( m_indiP_gain, "gain", 0, 1, 0, "%0.2f" );
        m_indiP_gain["current"] = m_gain;
        m_indiP_gain["target"]  = m_gain;

        createStandardIndiNumber<float>( m_indiP_leak, "leak", 0, 1, 0, "%0.2f" );
        m_indiP_leak["current"] = m_leak;
        m_indiP_leak["target"]  = m_leak;

        createStandardIndiNumber<float>( m_indiP_actLim, "actLim", 0, 8, 0, "%0.2f" );
        m_indiP_actLim["current"] = m_actLim;
        m_indiP_actLim["target"]  = m_actLim;

        createStandardIndiRequestSw( m_indiP_zero, "zero", "zero loop" );

        createStandardIndiNumber<int>( m_indiP_numModes, "numModes", 0, 97, 0, "%d" );
        m_indiP_numModes["current"] = m_numModes;
        m_indiP_numModes["target"]  = m_numModes;

        createStandardIndiToggleSw( m_indiP_offloadToggle, "offload" );

        m_indiP_fpsSource.setDevice( m_fpsSource );
        m_indiP_fpsSource.setName( "fps" );

        m_indiP_navgSource.setDevice( m_navgSource );
        m_indiP_navgSource.setName( "nAverage" );

        createROIndiNumber( m_indiP_fps, "fps" );
        m_indiP_fps.add( pcf::IndiElement( "current" ) );
    }

    /// Set up actuator offloading from a 2x1 tweeter to a 2x2 woofer, with an in-memory DM stream.
    /** The response matrix maps tweeter (t0, t1) to woofer (t0, t1, t0 + t1, 10 t0 + 10 t1).
     */
    void setupActuatorOffload()
    {
        m_width    = 2;
        m_height   = 1;
        m_typeSize = sizeof( float );

        m_twRespM.resize( 4, 2 );
        m_twRespM << 1, 0, 0, 1, 1, 1, 10, 10;

        m_dmWidth  = 2;
        m_dmHeight = 2;
        m_woofer.resize( 2, 2 );
        m_woofer.setZero();

        m_testDmData.assign( 4, -100.0f );
        m_dmStream.md        = &m_testDmMd;
        m_dmStream.array.raw = m_testDmData.data();
        m_dmStream.semlog    = nullptr;

        m_numModes   = 0;
        m_offloading = true;
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

/// Build a Switch property with a single element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty switchProp( const std::string                &device, /**< [in] INDI device name */
                              const std::string                &name,   /**< [in] INDI property name */
                              const std::string                &el,     /**< [in] element name */
                              pcf::IndiElement::SwitchStateType state   /**< [in] the switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// Check whether a timespec is zero.
/**
 * \returns true if both the seconds and nanoseconds are zero
 */
bool isZero( const timespec &ts /**< [in] the time to check */ )
{
    return ( ts.tv_sec == 0 && ts.tv_nsec == 0 );
}

/// \endcond

/// Verify the t2wOffloader configuration defaults and overrides.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader configuration", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::setupConfig();
    t2wOffloader::loadConfig();
    t2wOffloader::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/t2wOffloader_test_config.conf";

    SECTION( "defaults" )
    {
        mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

        t2wOffloader_test app( "t2w" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_fpsSource == "camwfs" );
        REQUIRE( app.m_navgSource == "dmtweeter-avg" );
        REQUIRE( app.m_twRespMPath == "" );
        REQUIRE( app.m_dmChannel == "" );
        REQUIRE( app.m_gain == Approx( 0.1f ) );
        REQUIRE( app.m_leak == 0.0f );
        REQUIRE( app.m_actLim == Approx( 7.0f ) );
        REQUIRE( app.m_tweeterModeFile == "" );
        REQUIRE( app.m_tweeterMaskFile == "" );
        REQUIRE( app.m_wooferMaskFile == "" );
        REQUIRE( app.m_maxModes == 50u );
        REQUIRE( app.m_numModes == 0u );
        REQUIRE( app.m_offloading == false );
        REQUIRE( app.m_shmimName == "t2w" );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
    }

    SECTION( "overrides" )
    {
        mx::app::writeConfigFile( fname,
                                  { "integrator",
                                    "integrator",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "offload",
                                    "shmimMonitor" },
                                  { "fpsSource",
                                    "navgSource",
                                    "respMPath",
                                    "channel",
                                    "gain",
                                    "leak",
                                    "actLim",
                                    "tweeterModes",
                                    "tweeterMask",
                                    "wooferMask",
                                    "maxModes",
                                    "numModes",
                                    "startupOffloading",
                                    "shmimName" },
                                  { "camlowfs",
                                    "dmtweeter-avg2",
                                    "/tmp/respM.fits",
                                    "dm00disp03",
                                    "0.25",
                                    "0.05",
                                    "3.5",
                                    "/tmp/tmodes.fits",
                                    "/tmp/tmask.fits",
                                    "/tmp/wmask.fits",
                                    "20",
                                    "12",
                                    "true",
                                    "dmtweeter-avg" } );

        t2wOffloader_test app( "t2w" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_fpsSource == "camlowfs" );
        REQUIRE( app.m_navgSource == "dmtweeter-avg2" );
        REQUIRE( app.m_twRespMPath == "/tmp/respM.fits" );
        REQUIRE( app.m_dmChannel == "dm00disp03" );
        REQUIRE( app.m_gain == Approx( 0.25f ) );
        REQUIRE( app.m_leak == Approx( 0.05f ) );
        REQUIRE( app.m_actLim == Approx( 3.5f ) );
        REQUIRE( app.m_tweeterModeFile == "/tmp/tmodes.fits" );
        REQUIRE( app.m_tweeterMaskFile == "/tmp/tmask.fits" );
        REQUIRE( app.m_wooferMaskFile == "/tmp/wmask.fits" );
        REQUIRE( app.m_maxModes == 20u );
        REQUIRE( app.m_numModes == 12u );
        REQUIRE( app.m_offloading == true );
        REQUIRE( app.m_shmimName == "dmtweeter-avg" );
    }

    SECTION( "startupOffloading false" )
    {
        mx::app::writeConfigFile( fname, { "offload" }, { "startupOffloading" }, { "false" } );

        t2wOffloader_test app( "t2w" );
        app.configure( fname );

        REQUIRE( app.m_offloading == false );
    }

    std::remove( fname.c_str() );
}

/// Verify `updateFPS()` computes the effective offload rate from the loop FPS and the averaging length.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader updateFPS", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::updateFPS();
    #endif
    // clang-format on

    t2wOffloader_test app( "t2w" );
    app.setupProperties();

    app.m_fps  = 1000.0f;
    app.m_navg = 0;
    REQUIRE( app.updateFPS() == 0 );
    REQUIRE( app.m_effFPS == 0.0f );

    app.m_navg = 10;
    REQUIRE( app.updateFPS() == 0 );
    REQUIRE( app.m_effFPS == Approx( 100.0f ) );

    app.m_navg = 4;
    REQUIRE( app.updateFPS() == 0 );
    REQUIRE( app.m_effFPS == Approx( 250.0f ) );

    app.m_fps = 0.0f;
    REQUIRE( app.updateFPS() == 0 );
    REQUIRE( app.m_effFPS == 0.0f );
}

/// Verify `processImage()` in actuator mode applies the response matrix, gain, leak and actuator limit.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader processImage actuator offloading", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::processImage( nullptr, dev::shmimT() );
    #endif
    // clang-format on

    float tweeter[2] = { 2.0f, 4.0f };

    SECTION( "not offloading does nothing" )
    {
        t2wOffloader_test app( "t2w" );
        app.setupActuatorOffload();
        app.m_offloading = false;

        REQUIRE( app.processImage( tweeter, dev::shmimT() ) == 0 );
        REQUIRE( ( app.m_woofer == 0.0f ).all() );
        REQUIRE( app.m_testDmMd.cnt0 == 0 );
        REQUIRE( app.m_testDmData[0] == -100.0f );
    }

    SECTION( "gain, leak and clamping over two frames" )
    {
        t2wOffloader_test app( "t2w" );
        app.setupActuatorOffload();
        app.m_gain   = 0.5f;
        app.m_leak   = 0.1f;
        app.m_actLim = 7.0f;

        REQUIRE( app.processImage( tweeter, dev::shmimT() ) == 0 );

        // delta = (2, 4, 6, 60); woofer = 0.5 * delta, with the last actuator clamped to 7.
        REQUIRE( app.m_woofer( 0, 0 ) == Approx( 1.0f ) );
        REQUIRE( app.m_woofer( 1, 0 ) == Approx( 2.0f ) );
        REQUIRE( app.m_woofer( 0, 1 ) == Approx( 3.0f ) );
        REQUIRE( app.m_woofer( 1, 1 ) == Approx( 7.0f ) );

        // The command was written to the DM stream and the counter updated.
        REQUIRE( app.m_testDmMd.cnt0 == 1 );
        REQUIRE( app.m_testDmMd.write == 0 );
        REQUIRE( app.m_testDmData[0] == Approx( 1.0f ) );
        REQUIRE( app.m_testDmData[1] == Approx( 2.0f ) );
        REQUIRE( app.m_testDmData[2] == Approx( 3.0f ) );
        REQUIRE( app.m_testDmData[3] == Approx( 7.0f ) );

        // Second frame integrates with the leak: 0.5 * delta + 0.9 * previous.
        REQUIRE( app.processImage( tweeter, dev::shmimT() ) == 0 );
        REQUIRE( app.m_woofer( 0, 0 ) == Approx( 1.0f + 0.9f * 1.0f ) );
        REQUIRE( app.m_woofer( 1, 0 ) == Approx( 2.0f + 0.9f * 2.0f ) );
        REQUIRE( app.m_woofer( 0, 1 ) == Approx( 3.0f + 0.9f * 3.0f ) );
        REQUIRE( app.m_woofer( 1, 1 ) == Approx( 7.0f ) );
        REQUIRE( app.m_testDmMd.cnt0 == 2 );
        REQUIRE( app.m_testDmData[0] == Approx( 1.9f ) );
    }

    SECTION( "negative commands are clamped to minus the limit" )
    {
        t2wOffloader_test app( "t2w" );
        app.setupActuatorOffload();
        app.m_gain   = 1.0f;
        app.m_leak   = 0.0f;
        app.m_actLim = 5.0f;

        float negTweeter[2] = { -2.0f, -4.0f };
        REQUIRE( app.processImage( negTweeter, dev::shmimT() ) == 0 );

        REQUIRE( app.m_woofer( 0, 0 ) == Approx( -2.0f ) );
        REQUIRE( app.m_woofer( 1, 0 ) == Approx( -4.0f ) );
        REQUIRE( app.m_woofer( 0, 1 ) == Approx( -5.0f ) );
        REQUIRE( app.m_woofer( 1, 1 ) == Approx( -5.0f ) );
        REQUIRE( app.m_testDmData[3] == Approx( -5.0f ) );
    }

    SECTION( "full leak discards the previous command" )
    {
        t2wOffloader_test app( "t2w" );
        app.setupActuatorOffload();
        app.m_gain = 0.25f;
        app.m_leak = 1.0f;
        app.m_woofer.setConstant( 3.0f );

        REQUIRE( app.processImage( tweeter, dev::shmimT() ) == 0 );
        REQUIRE( app.m_woofer( 0, 0 ) == Approx( 0.5f ) );
        REQUIRE( app.m_woofer( 1, 0 ) == Approx( 1.0f ) );
        REQUIRE( app.m_woofer( 0, 1 ) == Approx( 1.5f ) );
    }

    SECTION( "a stuck write flag times out without writing" )
    {
        t2wOffloader_test app( "t2w" );
        app.setupActuatorOffload();
        app.m_testDmMd.write = 1;

        REQUIRE( app.processImage( tweeter, dev::shmimT() ) == 0 );
        REQUIRE( app.m_testDmMd.cnt0 == 0 );
        REQUIRE( app.m_testDmData[0] == -100.0f );
        REQUIRE( ( app.m_woofer == 0.0f ).all() );
    }
}

/// Verify the gain, leak and actuator limit callbacks.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader gain, leak and actLim callbacks", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::newCallBack_m_indiP_gain( pcf::IndiProperty() );
    t2wOffloader::newCallBack_m_indiP_leak( pcf::IndiProperty() );
    t2wOffloader::newCallBack_m_indiP_actLim( pcf::IndiProperty() );
    #endif
    // clang-format on

    t2wOffloader_test app( "t2w" );
    app.setupProperties();

    SECTION( "gain" )
    {
        REQUIRE( app.newCallBack_m_indiP_gain( numberProp( "t2w", "gain", "target", 0.3f ) ) == 0 );
        REQUIRE( app.m_gain == Approx( 0.3f ) );

        REQUIRE( app.newCallBack_m_indiP_gain( numberProp( "t2w", "gain", "current", 0.2f ) ) == 0 );
        REQUIRE( app.m_gain == Approx( 0.2f ) );

        REQUIRE( app.newCallBack_m_indiP_gain( numberProp( "t2w", "gain", "other", 0.9f ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_gain( numberProp( "other", "gain", "target", 0.9f ) ) == -1 );
        REQUIRE( app.m_gain == Approx( 0.2f ) );
    }

    SECTION( "leak" )
    {
        REQUIRE( app.newCallBack_m_indiP_leak( numberProp( "t2w", "leak", "target", 0.05f ) ) == 0 );
        REQUIRE( app.m_leak == Approx( 0.05f ) );

        REQUIRE( app.newCallBack_m_indiP_leak( numberProp( "t2w", "gain", "target", 0.5f ) ) == -1 );
        REQUIRE( app.m_leak == Approx( 0.05f ) );
    }

    SECTION( "actLim" )
    {
        REQUIRE( app.newCallBack_m_indiP_actLim( numberProp( "t2w", "actLim", "target", 4.5f ) ) == 0 );
        REQUIRE( app.m_actLim == Approx( 4.5f ) );

        REQUIRE( app.newCallBack_m_indiP_actLim( numberProp( "t2w", "actLim", "other", 1.0f ) ) == -1 );
        REQUIRE( app.m_actLim == Approx( 4.5f ) );
    }
}

/// Verify the numModes callback sets the mode count and clamps it to `maxModes`.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader numModes callback", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::newCallBack_m_indiP_numModes( pcf::IndiProperty() );
    #endif
    // clang-format on

    t2wOffloader_test app( "t2w" );
    app.setupProperties();
    app.m_maxModes = 50;

    REQUIRE( app.newCallBack_m_indiP_numModes( numberProp( "t2w", "numModes", "target", 12 ) ) == 0 );
    REQUIRE( app.m_numModes == 12u );

    REQUIRE( app.newCallBack_m_indiP_numModes( numberProp( "t2w", "numModes", "target", 80 ) ) == 0 );
    REQUIRE( app.m_numModes == 50u );

    REQUIRE( app.newCallBack_m_indiP_numModes( numberProp( "t2w", "numModes", "target", 0 ) ) == 0 );
    REQUIRE( app.m_numModes == 0u );

    REQUIRE( app.newCallBack_m_indiP_numModes( numberProp( "t2w", "numModes", "other", 5 ) ) == -1 );
    REQUIRE( app.m_numModes == 0u );
}

/// Verify the offload toggle callback starts and stops offloading, zeroing the woofer on start.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader offload toggle callback", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::newCallBack_m_indiP_offloadToggle( pcf::IndiProperty() );
    #endif
    // clang-format on

    t2wOffloader_test app( "t2w" );
    app.setupProperties();
    app.m_woofer.resize( 2, 2 );
    app.m_woofer.setConstant( 1.0f );

    // Starting zeroes the woofer.
    REQUIRE( app.newCallBack_m_indiP_offloadToggle( switchProp( "t2w", "offload", "toggle", pcf::IndiElement::On ) ) ==
             0 );
    REQUIRE( app.m_offloading == true );
    REQUIRE( ( app.m_woofer == 0.0f ).all() );

    // Already offloading: no change and no re-zeroing.
    app.m_woofer.setConstant( 2.0f );
    REQUIRE( app.newCallBack_m_indiP_offloadToggle( switchProp( "t2w", "offload", "toggle", pcf::IndiElement::On ) ) ==
             0 );
    REQUIRE( app.m_offloading == true );
    REQUIRE( ( app.m_woofer == 2.0f ).all() );

    // Stopping keeps the woofer shape.
    REQUIRE( app.newCallBack_m_indiP_offloadToggle( switchProp( "t2w", "offload", "toggle", pcf::IndiElement::Off ) ) ==
             0 );
    REQUIRE( app.m_offloading == false );
    REQUIRE( ( app.m_woofer == 2.0f ).all() );

    // Stopping again does nothing.
    REQUIRE( app.newCallBack_m_indiP_offloadToggle( switchProp( "t2w", "offload", "toggle", pcf::IndiElement::Off ) ) ==
             0 );
    REQUIRE( app.m_offloading == false );

    // Wrong device is rejected.
    REQUIRE( app.newCallBack_m_indiP_offloadToggle(
                 switchProp( "other", "offload", "toggle", pcf::IndiElement::On ) ) == -1 );
    REQUIRE( app.m_offloading == false );
}

/// Verify the zero callback ignores a request that is not On.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader zero callback", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::newCallBack_m_indiP_zero( pcf::IndiProperty() );
    #endif
    // clang-format on

    t2wOffloader_test app( "t2w" );
    app.setupProperties();

    REQUIRE( app.newCallBack_m_indiP_zero( switchProp( "t2w", "zero", "request", pcf::IndiElement::Off ) ) == 0 );
    REQUIRE( app.newCallBack_m_indiP_zero( switchProp( "other", "zero", "request", pcf::IndiElement::On ) ) == -1 );
}

/// Verify the fps and navg source set callbacks update the effective offload rate.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader fps and navg source callbacks", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::setCallBack_m_indiP_fpsSource( pcf::IndiProperty() );
    t2wOffloader::setCallBack_m_indiP_navgSource( pcf::IndiProperty() );
    #endif
    // clang-format on

    t2wOffloader_test app( "t2w" );
    app.setupProperties();

    REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "current", 2000.0f ) ) == 0 );
    REQUIRE( app.m_fps == Approx( 2000.0f ) );
    REQUIRE( app.m_effFPS == 0.0f ); // navg is still 0

    REQUIRE( app.setCallBack_m_indiP_navgSource( numberProp( "dmtweeter-avg", "nAverage", "current", 20 ) ) == 0 );
    REQUIRE( app.m_navg == 20u );
    REQUIRE( app.m_effFPS == Approx( 100.0f ) );

    REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "current", 1000.0f ) ) == 0 );
    REQUIRE( app.m_effFPS == Approx( 50.0f ) );

    // Properties without a current element are ignored.
    REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "target", 10.0f ) ) == 0 );
    REQUIRE( app.setCallBack_m_indiP_navgSource( numberProp( "dmtweeter-avg", "nAverage", "target", 1 ) ) == 0 );
    REQUIRE( app.m_fps == Approx( 1000.0f ) );
    REQUIRE( app.m_navg == 20u );

    // Wrong devices are rejected.
    REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camother", "fps", "current", 10.0f ) ) == -1 );
    REQUIRE( app.setCallBack_m_indiP_navgSource( numberProp( "other", "nAverage", "current", 1 ) ) == -1 );
    REQUIRE( app.m_fps == Approx( 1000.0f ) );
    REQUIRE( app.m_navg == 20u );
}

/// Verify the loop gain and offloading telemetry records change detection and the telemeter hooks.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader telemetry", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::recordLoopGain( false );
    t2wOffloader::recordOffloading( false );
    t2wOffloader::recordTelem( static_cast<const telem_loopgain *>( nullptr ) );
    t2wOffloader::recordTelem( static_cast<const telem_offloading *>( nullptr ) );
    t2wOffloader::checkRecordTimes();
    #endif
    // clang-format on

    t2wOffloader_test app( "t2w" );
    app.setupProperties();

    SECTION( "loop gain records on change" )
    {
        // Synchronize the recorded state with the app.
        REQUIRE( app.recordLoopGain( true ) == 0 );

        MagAOX::logger::telem_loopgain::lastRecord = { 0, 0 };
        REQUIRE( app.recordLoopGain() == 0 );
        REQUIRE( isZero( MagAOX::logger::telem_loopgain::lastRecord ) );

        app.m_gain = app.m_gain + 0.1f;
        REQUIRE( app.recordLoopGain() == 0 );
        REQUIRE_FALSE( isZero( MagAOX::logger::telem_loopgain::lastRecord ) );

        MagAOX::logger::telem_loopgain::lastRecord = { 0, 0 };
        app.m_offloading                           = !app.m_offloading;
        REQUIRE( app.recordLoopGain() == 0 );
        REQUIRE_FALSE( isZero( MagAOX::logger::telem_loopgain::lastRecord ) );

        MagAOX::logger::telem_loopgain::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_loopgain *>( nullptr ) ) == 0 );
        REQUIRE_FALSE( isZero( MagAOX::logger::telem_loopgain::lastRecord ) );
    }

    SECTION( "offloading records on change" )
    {
        // With zero effective FPS the only changes are in the mode count and the averaging length.
        app.m_effFPS = 0;
        REQUIRE( app.recordOffloading( true ) == 0 );

        MagAOX::logger::telem_offloading::lastRecord = { 0, 0 };
        REQUIRE( app.recordOffloading() == 0 );
        REQUIRE( isZero( MagAOX::logger::telem_offloading::lastRecord ) );

        app.m_numModes = app.m_numModes + 3;
        REQUIRE( app.recordOffloading() == 0 );
        REQUIRE_FALSE( isZero( MagAOX::logger::telem_offloading::lastRecord ) );

        MagAOX::logger::telem_offloading::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_offloading *>( nullptr ) ) == 0 );
        REQUIRE_FALSE( isZero( MagAOX::logger::telem_offloading::lastRecord ) );
    }

    SECTION( "checkRecordTimes records both types after the max interval" )
    {
        MagAOX::logger::telem_loopgain::lastRecord   = { 0, 0 };
        MagAOX::logger::telem_offloading::lastRecord = { 0, 0 };

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE_FALSE( isZero( MagAOX::logger::telem_loopgain::lastRecord ) );
        REQUIRE_FALSE( isZero( MagAOX::logger::telem_offloading::lastRecord ) );
    }
}

} // namespace t2wOffloaderTest

} // namespace libXWCTest
