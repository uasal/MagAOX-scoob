/** \file dmPokeCenter_test.cpp
 * \brief Catch2 tests for the dmPokeCenter app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup dmPokeCenter_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "../dmPokeCenter.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup dmPokeCenter_unit_test dmPokeCenter Unit Tests
 * \brief Unit tests for the dmPokeCenter application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `dmPokeCenter` unit tests.
/** \ingroup dmPokeCenter_unit_test
 */
namespace dmPokeCenterTest
{

/// The WFS camera shmimMonitor base of dmPokeCenter.
typedef dev::shmimMonitor<dmPokeCenter, ::wfsShmimT> wfsMonitorT;

/// Directory used as `MILK_SHM_DIR` for the shared-memory tests.
constexpr const char *c_shmDir = "/tmp/dmPokeCenter_test_shm";

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing dmPokeCenter internals.
class dmPokeCenter_test : public dmPokeCenter
{
  public:
    /// Construct a harness with the given device name.
    /** Sets the INDI property keys, adds the measurement elements, and initializes the semaphores
     * that `appStartup()` would normally initialize.
     */
    explicit dmPokeCenter_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        m_indiP_poke_amp.setDevice( device );
        m_indiP_poke_amp.setName( "poke_amp" );
        m_indiP_nPupilImages.setDevice( device );
        m_indiP_nPupilImages.setName( "nPupilImages" );
        m_indiP_nPokeImages.setDevice( device );
        m_indiP_nPokeImages.setName( "nPokeImages" );
        m_indiP_single.setDevice( device );
        m_indiP_single.setName( "single" );
        m_indiP_continuous.setDevice( device );
        m_indiP_continuous.setName( "continuous" );
        m_indiP_stop.setDevice( device );
        m_indiP_stop.setName( "stop" );
        m_indiP_wfsFps.setDevice( "camwfs" );
        m_indiP_wfsFps.setName( "fps" );
        m_indiP_shutter.setDevice( "camwfs" );
        m_indiP_shutter.setName( "shutter" );

        m_indiP_deltaPos.add( pcf::IndiElement( "delta_x", 0.0 ) );
        m_indiP_deltaPos.add( pcf::IndiElement( "delta_y", 0.0 ) );
        m_indiP_deltaPos.add( pcf::IndiElement( "counter", 0 ) );

        sem_init( &m_wfsSemaphore, 0, 0 );
        sem_init( &m_imageSemaphore, 0, 0 );
    }

    /// Destroy the semaphores.
    ~dmPokeCenter_test() noexcept
    {
        sem_destroy( &m_wfsSemaphore );
        sem_destroy( &m_imageSemaphore );
    }

    using dmPokeCenter::m_continuous;
    using dmPokeCenter::m_counter;
    using dmPokeCenter::m_dmChan;
    using dmPokeCenter::m_dmImage;
    using dmPokeCenter::m_dmSleep;
    using dmPokeCenter::m_dmStream;
    using dmPokeCenter::m_imageSemWait;
    using dmPokeCenter::m_imageSemWait_nsec;
    using dmPokeCenter::m_imageSemWait_sec;
    using dmPokeCenter::m_indiP_deltaPos;
    using dmPokeCenter::m_measuring;
    using dmPokeCenter::m_nDarks;
    using dmPokeCenter::m_nPokeImages;
    using dmPokeCenter::m_nPupilImages;
    using dmPokeCenter::m_poke_amp;
    using dmPokeCenter::m_poke_x;
    using dmPokeCenter::m_poke_y;
    using dmPokeCenter::m_pokeBlockW;
    using dmPokeCenter::m_pokeFWHMGuess;
    using dmPokeCenter::m_pokeImage;
    using dmPokeCenter::m_pokePositions;
    using dmPokeCenter::m_pokeX;
    using dmPokeCenter::m_pokeY;
    using dmPokeCenter::m_pupilCutBuff;
    using dmPokeCenter::m_pupilImage;
    using dmPokeCenter::m_pupilMag;
    using dmPokeCenter::m_pupilMedThresh;
    using dmPokeCenter::m_pupilPixels;
    using dmPokeCenter::m_pupilX;
    using dmPokeCenter::m_pupilY;
    using dmPokeCenter::m_rawImage;
    using dmPokeCenter::m_shutdown;
    using dmPokeCenter::m_shutter;
    using dmPokeCenter::m_single;
    using dmPokeCenter::m_smoothWidth;
    using dmPokeCenter::m_stopMeasurement;
    using dmPokeCenter::m_wfsCamDevName;
    using dmPokeCenter::m_wfsDark;
    using dmPokeCenter::m_wfsFps;
    using dmPokeCenter::m_wfsSemWait;
    using dmPokeCenter::m_wfsSemWait_nsec;
    using dmPokeCenter::m_wfsSemWait_sec;

    using dmPokeCenter::analyzeSensor;
    using dmPokeCenter::checkRecordTimes;
    using dmPokeCenter::fitPokes;
    using dmPokeCenter::fitPupil;
    using dmPokeCenter::recordPokeCenter;
    using dmPokeCenter::recordTelem;
    using dmPokeCenter::runSensor;

    using dmPokeCenter::newCallBack_m_indiP_continuous;
    using dmPokeCenter::newCallBack_m_indiP_nPokeImages;
    using dmPokeCenter::newCallBack_m_indiP_nPupilImages;
    using dmPokeCenter::newCallBack_m_indiP_poke_amp;
    using dmPokeCenter::newCallBack_m_indiP_single;
    using dmPokeCenter::newCallBack_m_indiP_stop;
    using dmPokeCenter::setCallBack_m_indiP_shutter;
    using dmPokeCenter::setCallBack_m_indiP_wfsFps;

    /// Setup the configurator and load the given config file.
    void loadConfigFile( const std::string &file /**< [in] config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
    }

    /// Set the WFS camera stream geometry as the shmimMonitor would.
    void setWfsGeometry( uint32_t width,  /**< [in] image width */
                         uint32_t height, /**< [in] image height */
                         uint8_t  dataType /**< [in] ImageStreamIO data type code */ )
    {
        wfsMonitorT::m_width    = width;
        wfsMonitorT::m_height   = height;
        wfsMonitorT::m_dataType = dataType;
    }

    /// Get the configured shmim name of the WFS camera monitor.
    /** \returns the shmim name */
    std::string wfsShmimName()
    {
        return wfsMonitorT::m_shmimName;
    }

    /// Create the pupil and poke image streams directly (normally done by `allocate()`).
    void createAnalysisImages( uint32_t width, /**< [in] image width */
                               uint32_t height /**< [in] image height */ )
    {
        m_pupilImage.create( m_configName + "_pupil", width, height );
        m_pokeImage.create( m_configName + "_poke", width, height );
    }

    /// Get the current value of the image semaphore.
    /** \returns the semaphore value */
    int imageSemValue()
    {
        int val = -1;
        sem_getvalue( &m_imageSemaphore, &val );
        return val;
    }

    /// Get the current value of the WFS start semaphore.
    /** \returns the semaphore value */
    int wfsSemValue()
    {
        int val = -1;
        sem_getvalue( &m_wfsSemaphore, &val );
        return val;
    }
};
/// \endcond

/// Point ImageStreamIO at a private shared-memory directory.
void useTestShmDir()
{
    std::filesystem::create_directories( c_shmDir );
    setenv( "MILK_SHM_DIR", c_shmDir, 1 );
}

/// Remove the debugging FITS files that dmPokeCenter writes to /tmp.
void removeDebugFiles()
{
    for( const char *f : { "/tmp/fullMask.fits",
                           "/tmp/pupilMagnified.fits",
                           "/tmp/magMask.fits",
                           "/tmp/magEdge.fits",
                           "/tmp/sm.fits",
                           "/tmp/pupilImage.fits",
                           "/tmp/poke.fits" } )
    {
        std::filesystem::remove( f );
    }
}

/// Fill an image with a uniform disk on a zero background.
void fillDisk( mx::improc::eigenMap<float> &im,  /**< [out] the image */
               float                        x0,  /**< [in] center along rows */
               float                        y0,  /**< [in] center along columns */
               float                        rad, /**< [in] disk radius */
               float                        value /**< [in] value inside the disk */ )
{
    for( int r = 0; r < im.rows(); ++r )
    {
        for( int c = 0; c < im.cols(); ++c )
        {
            float dr   = r - x0;
            float dc   = c - y0;
            im( r, c ) = ( dr * dr + dc * dc <= rad * rad ) ? value : 0.0f;
        }
    }
}

/// Add a symmetric Gaussian to an image.
void addGaussian( mx::improc::eigenMap<float> &im,    /**< [in/out] the image */
                  float                        x0,    /**< [in] center along rows */
                  float                        y0,    /**< [in] center along columns */
                  float                        sigma, /**< [in] Gaussian width */
                  float                        peak /**< [in] Gaussian peak */ )
{
    for( int r = 0; r < im.rows(); ++r )
    {
        for( int c = 0; c < im.cols(); ++c )
        {
            float dr = r - x0;
            float dc = c - y0;
            im( r, c ) += peak * std::exp( -( dr * dr + dc * dc ) / ( 2 * sigma * sigma ) );
        }
    }
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

/// Verify the default dmPokeCenter configuration with a minimal poke specification.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter configuration defaults", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::setupConfig();
    dmPokeCenter::loadConfig();
    dmPokeCenter::loadConfigImpl(config);
    #endif
    // clang-format on

    dmPokeCenter_test app( "dmpc" );

    mx::app::writeConfigFile(
        "/tmp/dmPokeCenter_test_defaults.conf", { "pokecen", "pokecen" }, { "pokeX", "pokeY" }, { "3", "4" } );
    app.loadConfigFile( "/tmp/dmPokeCenter_test_defaults.conf" );

    REQUIRE( app.m_shutdown == 0 );

    REQUIRE( app.wfsShmimName() == "dmpc" );
    REQUIRE( app.m_wfsCamDevName == "dmpc" );

    REQUIRE( app.m_wfsSemWait == Approx( 1.5 ) );
    REQUIRE( app.m_wfsSemWait_sec == 1 );
    REQUIRE( app.m_wfsSemWait_nsec == 500000000 );
    REQUIRE( app.m_imageSemWait == Approx( 0.5 ) );
    REQUIRE( app.m_imageSemWait_sec == 0 );
    REQUIRE( app.m_imageSemWait_nsec == 500000000 );

    REQUIRE( app.m_dmChan == "" );
    REQUIRE( app.m_poke_x == std::vector<int>( { 3 } ) );
    REQUIRE( app.m_poke_y == std::vector<int>( { 4 } ) );
    REQUIRE( app.m_poke_amp == Approx( 0.0 ) );
    REQUIRE( app.m_dmSleep == Approx( 10000.0 ) );
    REQUIRE( app.m_nDarks == 5 );
    REQUIRE( app.m_nPokeImages == 5 );
    REQUIRE( app.m_nPupilImages == 20 );
    REQUIRE( app.m_pupilPixels == 68600 );
    REQUIRE( app.m_pupilCutBuff == 20 );
    REQUIRE( app.m_pupilMag == Approx( 10.0 ) );
    REQUIRE( app.m_pupilMedThresh == Approx( 0.9 ) );
    REQUIRE( app.m_pokeBlockW == 64 );
    REQUIRE( app.m_pokeFWHMGuess == 2 );
    REQUIRE( app.m_smoothWidth == Approx( 3.0 ) );
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );

    std::filesystem::remove( "/tmp/dmPokeCenter_test_defaults.conf" );
}

/// Verify dmPokeCenter configuration overrides.
/**
 * The `wfscam.camDevName`, `wfscam.loopSemWait` and `wfscam.imageSemWait` keys are registered with the
 * config-file section `wfs` (not `wfscam`), so they are written under `[wfs]` here.
 *
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter configuration overrides", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::setupConfig();
    dmPokeCenter::loadConfig();
    dmPokeCenter::loadConfigImpl(config);
    #endif
    // clang-format on

    dmPokeCenter_test app( "dmpc" );

    mx::app::writeConfigFile( "/tmp/dmPokeCenter_test_overrides.conf",
                              { "wfscam",
                                "wfs",
                                "wfs",
                                "wfs",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "pokecen",
                                "telemeter" },
                              { "shmimName",
                                "camDevName",
                                "loopSemWait",
                                "imageSemWait",
                                "dmChannel",
                                "pokeX",
                                "pokeY",
                                "pokeAmp",
                                "dmSleep",
                                "nPokeImages",
                                "nPupilImages",
                                "pupilPixels",
                                "pupilCutBuff",
                                "pupilMag",
                                "pupilMedThresh",
                                "pokeBlockW",
                                "pokeFWHMGuess",
                                "maxInterval" },
                              { "camwfs",
                                "camwfsdev",
                                "2.25",
                                "0.75",
                                "dm01disp06",
                                "10,20",
                                "11,21",
                                "0.05",
                                "2500",
                                "3",
                                "7",
                                "1000",
                                "4",
                                "5",
                                "0.5",
                                "16",
                                "3",
                                "4" } );
    app.loadConfigFile( "/tmp/dmPokeCenter_test_overrides.conf" );

    REQUIRE( app.m_shutdown == 0 );

    REQUIRE( app.wfsShmimName() == "camwfs" );
    REQUIRE( app.m_wfsCamDevName == "camwfsdev" );
    REQUIRE( app.m_wfsSemWait == Approx( 2.25 ) );
    REQUIRE( app.m_wfsSemWait_sec == 2 );
    REQUIRE( app.m_wfsSemWait_nsec == 250000000 );
    REQUIRE( app.m_imageSemWait == Approx( 0.75 ) );
    REQUIRE( app.m_imageSemWait_sec == 0 );
    REQUIRE( app.m_imageSemWait_nsec == 750000000 );

    REQUIRE( app.m_dmChan == "dm01disp06" );
    REQUIRE( app.m_poke_x == std::vector<int>( { 10, 20 } ) );
    REQUIRE( app.m_poke_y == std::vector<int>( { 11, 21 } ) );
    REQUIRE( app.m_poke_amp == Approx( 0.05 ) );
    REQUIRE( app.m_dmSleep == Approx( 2500.0 ) );
    REQUIRE( app.m_nPokeImages == 3 );
    REQUIRE( app.m_nPupilImages == 7 );
    REQUIRE( app.m_pupilPixels == 1000 );
    REQUIRE( app.m_pupilCutBuff == 4 );
    REQUIRE( app.m_pupilMag == Approx( 5.0 ) );
    REQUIRE( app.m_pupilMedThresh == Approx( 0.5 ) );
    REQUIRE( app.m_pokeBlockW == 16 );
    REQUIRE( app.m_pokeFWHMGuess == 3 );
    REQUIRE( app.m_maxInterval == Approx( 4.0 ) );

    std::filesystem::remove( "/tmp/dmPokeCenter_test_overrides.conf" );
}

/// Verify an invalid poke specification makes loadConfig request shutdown.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter rejects an invalid poke specification", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::loadConfig();
    dmPokeCenter::loadConfigImpl(config);
    #endif
    // clang-format on

    SECTION( "no pokes" )
    {
        dmPokeCenter_test app( "dmpc" );

        mx::app::writeConfigFile( "/tmp/dmPokeCenter_test_nopoke.conf", { "none" }, { "nada" }, { "0" } );
        app.loadConfigFile( "/tmp/dmPokeCenter_test_nopoke.conf" );

        REQUIRE( app.m_shutdown == 1 );

        std::filesystem::remove( "/tmp/dmPokeCenter_test_nopoke.conf" );
    }

    SECTION( "mismatched x and y" )
    {
        dmPokeCenter_test app( "dmpc" );

        mx::app::writeConfigFile(
            "/tmp/dmPokeCenter_test_mismatch.conf", { "pokecen", "pokecen" }, { "pokeX", "pokeY" }, { "1", "4,5" } );
        app.loadConfigFile( "/tmp/dmPokeCenter_test_mismatch.conf" );

        REQUIRE( app.m_shutdown == 1 );

        std::filesystem::remove( "/tmp/dmPokeCenter_test_mismatch.conf" );
    }
}

/// Verify the numeric and camera-status INDI callbacks.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter numeric and camera INDI callbacks", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::newCallBack_m_indiP_poke_amp(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_nPupilImages(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_nPokeImages(const pcf::IndiProperty &);
    dmPokeCenter::setCallBack_m_indiP_wfsFps(const pcf::IndiProperty &);
    dmPokeCenter::setCallBack_m_indiP_shutter(const pcf::IndiProperty &);
    #endif
    // clang-format on

    using sw = pcf::IndiElement;

    SECTION( "poke_amp target" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_poke_amp( makeNumber( "dmpc", "poke_amp", "target", 0.125 ) ) == 0 );
        REQUIRE( app.m_poke_amp == Approx( 0.125 ) );
    }

    SECTION( "poke_amp without target or current is rejected" )
    {
        dmPokeCenter_test app( "dmpc" );
        app.m_poke_amp = 0.5;
        REQUIRE( app.newCallBack_m_indiP_poke_amp( makeNumber( "dmpc", "poke_amp", "value", 0.1 ) ) == -1 );
        REQUIRE( app.m_poke_amp == Approx( 0.5 ) );
    }

    SECTION( "nPupilImages current" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_nPupilImages( makeNumber( "dmpc", "nPupilImages", "current", 40 ) ) == 0 );
        REQUIRE( app.m_nPupilImages == 40 );
    }

    SECTION( "nPokeImages target" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_nPokeImages( makeNumber( "dmpc", "nPokeImages", "target", 9 ) ) == 0 );
        REQUIRE( app.m_nPokeImages == 9 );
    }

    SECTION( "nPokeImages for another device is rejected" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_nPokeImages( makeNumber( "other", "nPokeImages", "target", 9 ) ) == -1 );
        REQUIRE( app.m_nPokeImages == 5 );
    }

    SECTION( "WFS fps" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.setCallBack_m_indiP_wfsFps( makeNumber( "camwfs", "fps", "target", 100.0 ) ) == 0 );
        REQUIRE( app.m_wfsFps == Approx( -1.0 ) );
        REQUIRE( app.setCallBack_m_indiP_wfsFps( makeNumber( "camwfs", "fps", "current", 2000.0 ) ) == 0 );
        REQUIRE( app.m_wfsFps == Approx( 2000.0 ) );
    }

    SECTION( "shutter toggle" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.m_shutter == -1 );

        REQUIRE( app.setCallBack_m_indiP_shutter( makeSwitch( "camwfs", "shutter", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_shutter == 1 );

        REQUIRE( app.setCallBack_m_indiP_shutter( makeSwitch( "camwfs", "shutter", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_shutter == 0 );
    }

    SECTION( "shutter without toggle is rejected" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.setCallBack_m_indiP_shutter( makeSwitch( "camwfs", "shutter", "request", sw::On ) ) == -1 );
        REQUIRE( app.m_shutter == -1 );
    }
}

/// Verify the single, continuous and stop INDI callbacks drive the measurement state.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter measurement control INDI callbacks", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_continuous(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_stop(const pcf::IndiProperty &);
    #endif
    // clang-format on

    using sw = pcf::IndiElement;

    SECTION( "single on starts a single measurement" )
    {
        dmPokeCenter_test app( "dmpc" );
        app.m_continuous = true;
        REQUIRE( app.newCallBack_m_indiP_single( makeSwitch( "dmpc", "single", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_single == true );
        REQUIRE( app.m_continuous == false );
        REQUIRE( app.wfsSemValue() == 1 );
    }

    SECTION( "single on while measuring is ignored" )
    {
        dmPokeCenter_test app( "dmpc" );
        app.m_measuring = 1;
        REQUIRE( app.newCallBack_m_indiP_single( makeSwitch( "dmpc", "single", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_single == false );
        REQUIRE( app.wfsSemValue() == 0 );
    }

    SECTION( "single without toggle is rejected" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_single( makeSwitch( "dmpc", "single", "request", sw::On ) ) == -1 );
    }

    SECTION( "continuous on starts continuous measurements" )
    {
        dmPokeCenter_test app( "dmpc" );
        app.m_single = true;
        REQUIRE( app.newCallBack_m_indiP_continuous( makeSwitch( "dmpc", "continuous", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_continuous == true );
        REQUIRE( app.m_single == false );
        REQUIRE( app.wfsSemValue() == 1 );
    }

    SECTION( "continuous off while measuring requests a stop" )
    {
        dmPokeCenter_test app( "dmpc" );
        app.m_measuring = 2;
        REQUIRE( app.newCallBack_m_indiP_continuous( makeSwitch( "dmpc", "continuous", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == true );
    }

    SECTION( "continuous off while idle does nothing" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_continuous( makeSwitch( "dmpc", "continuous", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == false );
    }

    SECTION( "stop while measuring requests a stop" )
    {
        dmPokeCenter_test app( "dmpc" );
        app.m_measuring = 1;
        REQUIRE( app.newCallBack_m_indiP_stop( makeSwitch( "dmpc", "stop", "request", sw::On ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == true );
    }

    SECTION( "stop while idle does nothing" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_stop( makeSwitch( "dmpc", "stop", "request", sw::On ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == false );
    }

    SECTION( "stop without request is rejected" )
    {
        dmPokeCenter_test app( "dmpc" );
        REQUIRE( app.newCallBack_m_indiP_stop( makeSwitch( "dmpc", "stop", "toggle", sw::On ) ) == -1 );
    }
}

/// Verify the WFS stream allocation and frame copy.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter allocate and processImage", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::allocate(const wfsShmimT &);
    dmPokeCenter::processImage(void *, const wfsShmimT &);
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "missing DM channel fails" )
    {
        dmPokeCenter_test app( "dmpc" );
        app.m_dmChan = "dmpc_nodm";
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_FLOAT );

        REQUIRE( app.allocate( ::wfsShmimT() ) == -1 );
        REQUIRE( app.m_rawImage.valid() );
        REQUIRE( app.m_wfsDark.valid() );
        REQUIRE( app.m_pupilImage.valid() );
        REQUIRE_FALSE( app.m_pokeImage.valid() );
    }

    SECTION( "allocation sizes the images and frames are converted to float" )
    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpc_dm", 4, 5 );

        dmPokeCenter_test app( "dmpc" );
        app.m_dmChan = "dmpc_dm";
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_INT16 );

        REQUIRE( app.allocate( ::wfsShmimT() ) == 0 );
        REQUIRE( app.m_rawImage.rows() == 3 );
        REQUIRE( app.m_rawImage.cols() == 2 );
        REQUIRE( app.m_wfsDark.rows() == 3 );
        REQUIRE( app.m_wfsDark.cols() == 2 );
        REQUIRE( app.m_pupilImage.rows() == 3 );
        REQUIRE( app.m_pupilImage.cols() == 2 );
        REQUIRE( app.m_pokeImage.rows() == 3 );
        REQUIRE( app.m_pokeImage.cols() == 2 );
        REQUIRE( app.m_dmImage.rows() == 4 );
        REQUIRE( app.m_dmImage.cols() == 5 );

        std::vector<int16_t> frame = { -3, 0, 7, 100, -200, 32000 };
        REQUIRE( app.imageSemValue() == 0 );
        REQUIRE( app.processImage( frame.data(), ::wfsShmimT() ) == 0 );
        REQUIRE( app.imageSemValue() == 1 );

        for( size_t n = 0; n < frame.size(); ++n )
        {
            REQUIRE( app.m_rawImage.data()[n] == Approx( static_cast<float>( frame[n] ) ) );
        }
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify runSensor stops when the camera shutter cannot be commanded.
/**
 * There is no INDI driver in unit tests, so `sendNewStandardIndiToggle()` fails.  On the first run
 * (shut for the dark) this is reported as an error.  On later runs (open for the pupil) the failure is
 * logged but `runSensor()` returns 0 without measuring.
 *
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter runSensor without shutter control", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::runSensor(true);
    #endif
    // clang-format on

    useTestShmDir();

    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpc_dm", 4, 4 );

        dmPokeCenter_test app( "dmpc" );
        app.m_dmChan = "dmpc_dm";
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocate( ::wfsShmimT() ) == 0 );

        app.m_wfsDark().setConstant( 2.0 );
        app.m_pupilImage().setConstant( 5.0 );

        SECTION( "first run fails to shut the shutter" )
        {
            REQUIRE( app.runSensor( true ) == -1 );
            REQUIRE( app.m_wfsDark().minCoeff() == Approx( 2.0 ) );
        }

        SECTION( "later run fails to open the shutter but returns 0" )
        {
            REQUIRE( app.runSensor( false ) == 0 );
            REQUIRE( app.m_pupilImage().minCoeff() == Approx( 5.0 ) );
            REQUIRE( app.m_counter == 0 );
        }
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify fitPupil finds the center of a synthetic circular pupil.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter fitPupil", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::fitPupil();
    #endif
    // clang-format on

    useTestShmDir();

    {
        dmPokeCenter_test app( "dmpc" );
        app.createAnalysisImages( 64, 64 );
        fillDisk( app.m_pupilImage(), 30, 33, 15, 1000 );

        app.m_pupilPixels = 600;
        app.m_pupilMag    = 4;

        SECTION( "pupil center is found" )
        {
            app.m_pupilCutBuff = 5;
            REQUIRE( app.fitPupil() == 0 );
            REQUIRE( app.m_pupilX == Approx( 30.0 ).margin( 1.0 ) );
            REQUIRE( app.m_pupilY == Approx( 33.0 ).margin( 1.0 ) );
        }

        SECTION( "a cut buffer extending past the image is rejected" )
        {
            app.m_pupilCutBuff = 20;
            REQUIRE( app.fitPupil() == -1 );
        }
    }

    removeDebugFiles();
    std::filesystem::remove_all( c_shmDir );
}

/// Verify fitPokes locates each poke and their average.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter fitPokes", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::fitPokes();
    #endif
    // clang-format on

    useTestShmDir();

    {
        dmPokeCenter_test app( "dmpc" );
        app.createAnalysisImages( 64, 64 );
        app.m_pokeImage().setZero();
        addGaussian( app.m_pokeImage(), 20, 22, 1.5, 1000 );
        addGaussian( app.m_pokeImage(), 44, 40, 1.5, 800 );

        app.m_poke_x     = { 0, 1 };
        app.m_poke_y     = { 0, 1 };
        app.m_pokeBlockW = 16;
        app.m_pokePositions.assign( 6, 0.0f );

        REQUIRE( app.fitPokes() == 0 );

        // The brightest poke is found first.
        REQUIRE( app.m_pokePositions[0] == Approx( 20.0 ).margin( 0.1 ) );
        REQUIRE( app.m_pokePositions[1] == Approx( 22.0 ).margin( 0.1 ) );
        REQUIRE( app.m_pokePositions[2] == Approx( 44.0 ).margin( 0.1 ) );
        REQUIRE( app.m_pokePositions[3] == Approx( 40.0 ).margin( 0.1 ) );
        REQUIRE( app.m_pokeX == Approx( 32.0 ).margin( 0.1 ) );
        REQUIRE( app.m_pokeY == Approx( 31.0 ).margin( 0.1 ) );
        REQUIRE( app.m_pokePositions[4] == Approx( app.m_pokeX ) );
        REQUIRE( app.m_pokePositions[5] == Approx( app.m_pokeY ) );
    }

    removeDebugFiles();
    std::filesystem::remove_all( c_shmDir );
}

/// Verify analyzeSensor reports the pupil minus average-poke position and counts measurements.
/**
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter analyzeSensor", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::analyzeSensor();
    #endif
    // clang-format on

    useTestShmDir();

    {
        dmPokeCenter_test app( "dmpc" );
        app.createAnalysisImages( 64, 64 );
        fillDisk( app.m_pupilImage(), 30, 33, 15, 1000 );
        app.m_pokeImage().setZero();
        addGaussian( app.m_pokeImage(), 24, 28, 1.5, 1000 );
        addGaussian( app.m_pokeImage(), 40, 34, 1.5, 800 );

        app.m_pupilPixels  = 600;
        app.m_pupilMag     = 4;
        app.m_pupilCutBuff = 5;
        app.m_poke_x       = { 0, 1 };
        app.m_poke_y       = { 0, 1 };
        app.m_pokeBlockW   = 16;
        app.m_pokePositions.assign( 6, 0.0f );

        SECTION( "a measurement is made" )
        {
            REQUIRE( app.analyzeSensor() == 0 );
            REQUIRE( app.m_counter == 1 );
            REQUIRE( app.m_pokeX == Approx( 32.0 ).margin( 0.1 ) );
            REQUIRE( app.m_pokeY == Approx( 31.0 ).margin( 0.1 ) );
            REQUIRE( app.m_indiP_deltaPos["delta_x"].get<float>() == Approx( app.m_pupilX - app.m_pokeX ) );
            REQUIRE( app.m_indiP_deltaPos["delta_y"].get<float>() == Approx( app.m_pupilY - app.m_pokeY ) );
            REQUIRE( app.m_indiP_deltaPos["delta_x"].get<float>() == Approx( -2.0 ).margin( 1.0 ) );
            REQUIRE( app.m_indiP_deltaPos["delta_y"].get<float>() == Approx( 2.0 ).margin( 1.0 ) );

            REQUIRE( app.analyzeSensor() == 0 );
            REQUIRE( app.m_counter == 2 );
        }

        SECTION( "a stop request skips the analysis" )
        {
            app.m_stopMeasurement = true;
            REQUIRE( app.analyzeSensor() == 0 );
            REQUIRE( app.m_counter == 0 );
            REQUIRE( app.m_pupilX == 0 );
        }

        SECTION( "shutdown skips the analysis" )
        {
            app.m_shutdown = 1;
            REQUIRE( app.analyzeSensor() == 0 );
            REQUIRE( app.m_counter == 0 );
        }

        SECTION( "a pupil fit failure is reported" )
        {
            app.m_pupilCutBuff = 20;
            REQUIRE( app.analyzeSensor() == -1 );
            REQUIRE( app.m_counter == 0 );
        }
    }

    removeDebugFiles();
    std::filesystem::remove_all( c_shmDir );
}

/// Verify the poke-center telemetry is recorded on change, when forced, and when the interval elapses.
/**
 * Recording is detected through `telem_pokecenter::lastRecord`, which `telemeter::telem()` updates.
 *
 * \ingroup dmPokeCenter_unit_test
 */
TEST_CASE( "dmPokeCenter poke center telemetry", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::recordPokeCenter(false);
    dmPokeCenter::recordTelem(nullptr);
    dmPokeCenter::checkRecordTimes();
    #endif
    // clang-format on

    using MagAOX::logger::telem_pokecenter;

    dmPokeCenter_test app( "dmpc" );
    app.m_maxInterval = 10.0;
    app.m_pokePositions.assign( 4, 1.0f );

    // Synchronize the internal last-recorded values.
    REQUIRE( app.recordPokeCenter( true ) == 0 );

    SECTION( "unchanged values are not recorded" )
    {
        telem_pokecenter::lastRecord = { 0, 0 };
        REQUIRE( app.recordPokeCenter( false ) == 0 );
        REQUIRE( telem_pokecenter::lastRecord.tv_sec == 0 );
    }

    SECTION( "a pupil change is recorded" )
    {
        telem_pokecenter::lastRecord = { 0, 0 };
        app.m_pupilX                 = 12.5;
        REQUIRE( app.recordPokeCenter( false ) == 0 );
        REQUIRE( telem_pokecenter::lastRecord.tv_sec > 0 );
    }

    SECTION( "a poke position change is recorded" )
    {
        telem_pokecenter::lastRecord = { 0, 0 };
        app.m_pokePositions[3]       = 2.0f;
        REQUIRE( app.recordPokeCenter( false ) == 0 );
        REQUIRE( telem_pokecenter::lastRecord.tv_sec > 0 );
    }

    SECTION( "a measuring change is recorded" )
    {
        telem_pokecenter::lastRecord = { 0, 0 };
        app.m_measuring              = 1;
        REQUIRE( app.recordPokeCenter( false ) == 0 );
        REQUIRE( telem_pokecenter::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordTelem always records" )
    {
        telem_pokecenter::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( telem_pokecenter::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records after the interval" )
    {
        telem_pokecenter::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_pokecenter::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes skips a recent record" )
    {
        timespec now;
        clock_gettime( CLOCK_REALTIME, &now );
        telem_pokecenter::lastRecord = now;

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_pokecenter::lastRecord.tv_sec == now.tv_sec );
        REQUIRE( telem_pokecenter::lastRecord.tv_nsec == now.tv_nsec );
    }
}

} // namespace dmPokeCenterTest

} // namespace libXWCTest
