/** \file dmPokeXCorr_test.cpp
 * \brief Catch2 tests for the dmPokeXCorr app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup dmPokeXCorr_files
 */

#include "../../../tests/testXWC.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "../dmPokeXCorr.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup dmPokeXCorr_unit_test dmPokeXCorr Unit Tests
 * \brief Unit tests for the dmPokeXCorr application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `dmPokeXCorr` unit tests.
/** \ingroup dmPokeXCorr_unit_test
 */
namespace dmPokeXCorrTest
{

/// The dmPokeWFS base of dmPokeXCorr.
typedef dev::dmPokeWFS<dmPokeXCorr> pokeWFST;

/// The WFS camera shmimMonitor base of dmPokeXCorr.
typedef dev::shmimMonitor<dmPokeXCorr, pokeWFST::wfsShmimT> wfsMonitorT;

/// The WFS dark shmimMonitor base of dmPokeXCorr.
typedef dev::shmimMonitor<dmPokeXCorr, pokeWFST::darkShmimT> darkMonitorT;

/// The zonal response matrix shmimMonitor base of dmPokeXCorr.
typedef dev::shmimMonitor<dmPokeXCorr, zrespShmimT> zrespMonitorT;

/// Directory used as `MILK_SHM_DIR` for the shared-memory tests.
constexpr const char *c_shmDir = "/tmp/dmPokeXCorr_test_shm";

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing dmPokeXCorr internals.
class dmPokeXCorr_test : public dmPokeXCorr
{
  public:
    /// Construct a harness with the given device name.
    /** Sets the INDI property keys, adds the measurement elements, and initializes the semaphores
     * that `appStartup()` would normally initialize.
     */
    explicit dmPokeXCorr_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        m_indiP_poke_amp.setDevice( device );
        m_indiP_poke_amp.setName( "poke_amp" );
        m_indiP_nPokeImages.setDevice( device );
        m_indiP_nPokeImages.setName( "nPokeImages" );
        m_indiP_nPokeAverage.setDevice( device );
        m_indiP_nPokeAverage.setName( "nPokeAverage" );
        m_indiP_single.setDevice( device );
        m_indiP_single.setName( "single" );
        m_indiP_continuous.setDevice( device );
        m_indiP_continuous.setName( "continuous" );
        m_indiP_stop.setDevice( device );
        m_indiP_stop.setName( "stop" );
        m_indiP_wfsFps.setDevice( "camwfs" );
        m_indiP_wfsFps.setName( "fps" );

        m_indiP_measurement.add( pcf::IndiElement( "delta_x", 0.0 ) );
        m_indiP_measurement.add( pcf::IndiElement( "delta_y", 0.0 ) );
        m_indiP_measurement.add( pcf::IndiElement( "counter", 0 ) );

        sem_init( &m_wfsSemaphore, 0, 0 );
        sem_init( &m_imageSemaphore, 0, 0 );
    }

    /// Destroy the semaphores.
    ~dmPokeXCorr_test() noexcept
    {
        sem_destroy( &m_wfsSemaphore );
        sem_destroy( &m_imageSemaphore );
    }

    using pokeWFST::m_continuous;
    using pokeWFST::m_counter;
    using pokeWFST::m_darkImage;
    using pokeWFST::m_darkValid;
    using pokeWFST::m_deltaX;
    using pokeWFST::m_deltaY;
    using pokeWFST::m_dmChan;
    using pokeWFST::m_dmImage;
    using pokeWFST::m_dmSleep;
    using pokeWFST::m_dmStream;
    using pokeWFST::m_imageSemWait;
    using pokeWFST::m_imageSemWait_nsec;
    using pokeWFST::m_imageSemWait_sec;
    using pokeWFST::m_indiP_measurement;
    using pokeWFST::m_measuring;
    using pokeWFST::m_nPokeAverage;
    using pokeWFST::m_nPokeImages;
    using pokeWFST::m_poke_amp;
    using pokeWFST::m_poke_x;
    using pokeWFST::m_poke_y;
    using pokeWFST::m_pokeImage;
    using pokeWFST::m_pokeLocal;
    using pokeWFST::m_rawImage;
    using pokeWFST::m_single;
    using pokeWFST::m_stopMeasurement;
    using pokeWFST::m_wfsCamDevName;
    using pokeWFST::m_wfsFps;
    using pokeWFST::m_wfsSemWait;
    using pokeWFST::m_wfsSemWait_nsec;
    using pokeWFST::m_wfsSemWait_sec;

    using dmPokeXCorr::m_refIm;
    using dmPokeXCorr::m_shutdown;
    using dmPokeXCorr::m_xcorr;

    using dmPokeXCorr::checkRecordTimes;
    using pokeWFST::recordPokeLoop;
    using pokeWFST::recordTelem;
    using pokeWFST::updateMeasurement;

    using pokeWFST::newCallBack_m_indiP_continuous;
    using pokeWFST::newCallBack_m_indiP_nPokeAverage;
    using pokeWFST::newCallBack_m_indiP_nPokeImages;
    using pokeWFST::newCallBack_m_indiP_poke_amp;
    using pokeWFST::newCallBack_m_indiP_single;
    using pokeWFST::newCallBack_m_indiP_stop;
    using pokeWFST::setCallBack_m_indiP_wfsFps;

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

    /// Set the WFS dark stream geometry as the shmimMonitor would.
    void setDarkGeometry( uint32_t width,  /**< [in] image width */
                          uint32_t height, /**< [in] image height */
                          uint8_t  dataType /**< [in] ImageStreamIO data type code */ )
    {
        darkMonitorT::m_width    = width;
        darkMonitorT::m_height   = height;
        darkMonitorT::m_dataType = dataType;
    }

    /// Set the zonal response matrix stream geometry as the shmimMonitor would.
    void setZrespGeometry( uint32_t width,  /**< [in] image width */
                           uint32_t height, /**< [in] image height */
                           uint32_t depth /**< [in] number of planes */ )
    {
        zrespMonitorT::m_width    = width;
        zrespMonitorT::m_height   = height;
        zrespMonitorT::m_depth    = depth;
        zrespMonitorT::m_dataType = IMAGESTRUCT_FLOAT;
    }

    /// Get the configured shmim name of the WFS camera monitor.
    /** \returns the shmim name */
    std::string wfsShmimName()
    {
        return wfsMonitorT::m_shmimName;
    }

    /// Get the configured shmim name of the dark monitor.
    /** \returns the shmim name */
    std::string darkShmimName()
    {
        return darkMonitorT::m_shmimName;
    }

    /// Get the configured shmim name of the zonal response monitor.
    /** \returns the shmim name */
    std::string zrespShmimName()
    {
        return zrespMonitorT::m_shmimName;
    }

    /// Get the getExistingFirst flag of the WFS camera monitor.
    /** \returns the flag */
    bool wfsGetExistingFirst()
    {
        return wfsMonitorT::m_getExistingFirst;
    }

    /// Get the getExistingFirst flag of the dark monitor.
    /** \returns the flag */
    bool darkGetExistingFirst()
    {
        return darkMonitorT::m_getExistingFirst;
    }

    /// Get the getExistingFirst flag of the zonal response monitor.
    /** \returns the flag */
    bool zrespGetExistingFirst()
    {
        return zrespMonitorT::m_getExistingFirst;
    }

    /// Create the poke image stream directly (normally done by `allocate()`).
    void createPokeImage( uint32_t width, /**< [in] image width */
                          uint32_t height /**< [in] image height */ )
    {
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

/// Fill an image with a unit-height Gaussian.
void fillGaussian( mx::improc::eigenImage<float> &im, /**< [out] the image, already sized */
                   float                          x0, /**< [in] center along rows */
                   float                          y0, /**< [in] center along columns */
                   float                          sigma /**< [in] Gaussian width */ )
{
    for( int r = 0; r < im.rows(); ++r )
    {
        for( int c = 0; c < im.cols(); ++c )
        {
            float dx   = r - x0;
            float dy   = c - y0;
            im( r, c ) = std::exp( -( dx * dx + dy * dy ) / ( 2 * sigma * sigma ) );
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

/// Verify the dmPokeXCorr constructor sets the shmimMonitor getExistingFirst flags.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr constructor", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::dmPokeXCorr();
    #endif
    // clang-format on

    dmPokeXCorr_test app( "dmpxc" );

    REQUIRE( app.wfsGetExistingFirst() == false );
    REQUIRE( app.darkGetExistingFirst() == true );
    REQUIRE( app.zrespGetExistingFirst() == true );
    REQUIRE( app.m_measuring == 0 );
    REQUIRE( app.m_counter == 0 );
}

/// Verify the default dmPokeXCorr configuration with a minimal poke specification.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr configuration defaults", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::setupConfig();
    dmPokeXCorr::loadConfig();
    dmPokeXCorr::loadConfigImpl(config);
    dev::dmPokeWFS<dmPokeXCorr>::setupConfig(config);
    dev::dmPokeWFS<dmPokeXCorr>::loadConfig(config);
    #endif
    // clang-format on

    dmPokeXCorr_test app( "dmpxc" );

    mx::app::writeConfigFile(
        "/tmp/dmPokeXCorr_test_defaults.conf", { "pokecen", "pokecen" }, { "pokeX", "pokeY" }, { "3", "4" } );
    app.loadConfigFile( "/tmp/dmPokeXCorr_test_defaults.conf" );

    REQUIRE( app.m_shutdown == 0 );

    REQUIRE( app.wfsShmimName() == "dmpxc" );
    REQUIRE( app.darkShmimName() == "dmpxc" );
    REQUIRE( app.zrespShmimName() == "dmpxc" );
    REQUIRE( app.m_wfsCamDevName == "dmpxc" );

    REQUIRE( app.m_wfsSemWait == Approx( 1.5 ) );
    REQUIRE( app.m_wfsSemWait_sec == 1 );
    REQUIRE( app.m_wfsSemWait_nsec == 500000000 );
    REQUIRE( app.m_imageSemWait == Approx( 0.5 ) );
    REQUIRE( app.m_imageSemWait_sec == 0 );
    REQUIRE( app.m_imageSemWait_nsec == 500000000 );

    REQUIRE( app.m_dmChan == "" );
    REQUIRE( app.m_poke_x.size() == 1 );
    REQUIRE( app.m_poke_x[0] == 3 );
    REQUIRE( app.m_poke_y.size() == 1 );
    REQUIRE( app.m_poke_y[0] == 4 );
    REQUIRE( app.m_poke_amp == Approx( 0.0 ) );
    REQUIRE( app.m_dmSleep == Approx( 10000.0 ) );
    REQUIRE( app.m_nPokeImages == 5 );
    REQUIRE( app.m_nPokeAverage == 10 );
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );

    std::filesystem::remove( "/tmp/dmPokeXCorr_test_defaults.conf" );
}

/// Verify dmPokeXCorr configuration overrides.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr configuration overrides", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::setupConfig();
    dmPokeXCorr::loadConfig();
    dev::dmPokeWFS<dmPokeXCorr>::loadConfig(config);
    #endif
    // clang-format on

    dmPokeXCorr_test app( "dmpxc" );

    mx::app::writeConfigFile( "/tmp/dmPokeXCorr_test_overrides.conf",
                              { "wfscam",
                                "wfscam",
                                "wfscam",
                                "wfscam",
                                "wfsdark",
                                "zrespM",
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
                                "shmimName",
                                "shmimName",
                                "dmChannel",
                                "pokeX",
                                "pokeY",
                                "pokeAmp",
                                "dmSleep",
                                "nPokeImages",
                                "nPokeAverage",
                                "maxInterval" },
                              { "camwfs",
                                "camwfsdev",
                                "2.25",
                                "0.75",
                                "camwfs_dark",
                                "zrespM_dm01",
                                "dm01disp06",
                                "10,20",
                                "11,21",
                                "0.05",
                                "2500",
                                "3",
                                "7",
                                "4" } );
    app.loadConfigFile( "/tmp/dmPokeXCorr_test_overrides.conf" );

    REQUIRE( app.m_shutdown == 0 );

    REQUIRE( app.wfsShmimName() == "camwfs" );
    REQUIRE( app.darkShmimName() == "camwfs_dark" );
    REQUIRE( app.zrespShmimName() == "zrespM_dm01" );
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
    REQUIRE( app.m_nPokeAverage == 7 );
    REQUIRE( app.m_maxInterval == Approx( 4.0 ) );

    std::filesystem::remove( "/tmp/dmPokeXCorr_test_overrides.conf" );
}

/// Verify the camera device name defaults to the WFS shmim name.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr camera device name defaults to the WFS shmim name", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::loadConfig(config);
    #endif
    // clang-format on

    dmPokeXCorr_test app( "dmpxc" );

    mx::app::writeConfigFile( "/tmp/dmPokeXCorr_test_camdev.conf",
                              { "wfscam", "pokecen", "pokecen" },
                              { "shmimName", "pokeX", "pokeY" },
                              { "camlowfs", "1", "1" } );
    app.loadConfigFile( "/tmp/dmPokeXCorr_test_camdev.conf" );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_wfsCamDevName == "camlowfs" );

    std::filesystem::remove( "/tmp/dmPokeXCorr_test_camdev.conf" );
}

/// Verify an invalid poke specification makes loadConfig request shutdown.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr rejects an invalid poke specification", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::loadConfig();
    dev::dmPokeWFS<dmPokeXCorr>::loadConfig(config);
    #endif
    // clang-format on

    SECTION( "no pokes" )
    {
        dmPokeXCorr_test app( "dmpxc" );

        mx::app::writeConfigFile( "/tmp/dmPokeXCorr_test_nopoke.conf", { "none" }, { "nada" }, { "0" } );
        app.loadConfigFile( "/tmp/dmPokeXCorr_test_nopoke.conf" );

        REQUIRE( app.m_shutdown == 1 );

        std::filesystem::remove( "/tmp/dmPokeXCorr_test_nopoke.conf" );
    }

    SECTION( "mismatched x and y" )
    {
        dmPokeXCorr_test app( "dmpxc" );

        mx::app::writeConfigFile(
            "/tmp/dmPokeXCorr_test_mismatch.conf", { "pokecen", "pokecen" }, { "pokeX", "pokeY" }, { "1,2,3", "4,5" } );
        app.loadConfigFile( "/tmp/dmPokeXCorr_test_mismatch.conf" );

        REQUIRE( app.m_shutdown == 1 );

        std::filesystem::remove( "/tmp/dmPokeXCorr_test_mismatch.conf" );
    }
}

/// Verify updateMeasurement stores the deltas and sets the measurement property.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr updateMeasurement", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::updateMeasurement(0,0);
    #endif
    // clang-format on

    dmPokeXCorr_test app( "dmpxc" );

    REQUIRE( app.updateMeasurement( 0.25, -1.5 ) == 0 );
    REQUIRE( app.m_deltaX == Approx( 0.25 ) );
    REQUIRE( app.m_deltaY == Approx( -1.5 ) );
    REQUIRE( app.m_indiP_measurement["delta_x"].get<float>() == Approx( 0.25 ) );
    REQUIRE( app.m_indiP_measurement["delta_y"].get<float>() == Approx( -1.5 ) );
}

/// Verify the numeric INDI callbacks update the poke parameters.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr numeric INDI callbacks", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_poke_amp(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_nPokeImages(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_nPokeAverage(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::setCallBack_m_indiP_wfsFps(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "poke_amp target" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_poke_amp( makeNumber( "dmpxc", "poke_amp", "target", 0.125 ) ) == 0 );
        REQUIRE( app.m_poke_amp == Approx( 0.125 ) );
    }

    SECTION( "poke_amp current is used when there is no target" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_poke_amp( makeNumber( "dmpxc", "poke_amp", "current", -0.25 ) ) == 0 );
        REQUIRE( app.m_poke_amp == Approx( -0.25 ) );
    }

    SECTION( "poke_amp without target or current is rejected" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_poke_amp = 0.5;
        REQUIRE( app.newCallBack_m_indiP_poke_amp( makeNumber( "dmpxc", "poke_amp", "value", 0.1 ) ) == -1 );
        REQUIRE( app.m_poke_amp == Approx( 0.5 ) );
    }

    SECTION( "poke_amp for another device is rejected" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_poke_amp = 0.5;
        REQUIRE( app.newCallBack_m_indiP_poke_amp( makeNumber( "other", "poke_amp", "target", 0.1 ) ) == -1 );
        REQUIRE( app.m_poke_amp == Approx( 0.5 ) );
    }

    SECTION( "nPokeImages target" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_nPokeImages( makeNumber( "dmpxc", "nPokeImages", "target", 12 ) ) == 0 );
        REQUIRE( app.m_nPokeImages == 12 );
    }

    SECTION( "nPokeAverage target" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_nPokeAverage( makeNumber( "dmpxc", "nPokeAverage", "target", 3 ) ) == 0 );
        REQUIRE( app.m_nPokeAverage == 3 );
    }

    SECTION( "nPokeAverage for the wrong property is rejected" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_nPokeAverage( makeNumber( "dmpxc", "nPokeImages", "target", 3 ) ) == -1 );
        REQUIRE( app.m_nPokeAverage == 10 );
    }

    SECTION( "WFS fps with current" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.setCallBack_m_indiP_wfsFps( makeNumber( "camwfs", "fps", "current", 1500.0 ) ) == 0 );
        REQUIRE( app.m_wfsFps == Approx( 1500.0 ) );
    }

    SECTION( "WFS fps without current is ignored" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.setCallBack_m_indiP_wfsFps( makeNumber( "camwfs", "fps", "target", 1500.0 ) ) == 0 );
        REQUIRE( app.m_wfsFps == Approx( -1.0 ) );
    }
}

/// Verify the single, continuous and stop INDI callbacks drive the measurement state.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr measurement control INDI callbacks", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_continuous(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_stop(const pcf::IndiProperty &);
    #endif
    // clang-format on

    using sw = pcf::IndiElement;

    SECTION( "single on starts a single measurement" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_continuous = true;
        REQUIRE( app.newCallBack_m_indiP_single( makeSwitch( "dmpxc", "single", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_single == true );
        REQUIRE( app.m_continuous == false );
        REQUIRE( app.wfsSemValue() == 1 );
    }

    SECTION( "single on while measuring is ignored" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_measuring = 2;
        REQUIRE( app.newCallBack_m_indiP_single( makeSwitch( "dmpxc", "single", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_single == false );
        REQUIRE( app.wfsSemValue() == 0 );
    }

    SECTION( "single off does nothing" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_single( makeSwitch( "dmpxc", "single", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_single == false );
        REQUIRE( app.wfsSemValue() == 0 );
    }

    SECTION( "single without toggle is rejected" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_single( makeSwitch( "dmpxc", "single", "request", sw::On ) ) == -1 );
        REQUIRE( app.m_single == false );
    }

    SECTION( "continuous on starts continuous measurements" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_single = true;
        REQUIRE( app.newCallBack_m_indiP_continuous( makeSwitch( "dmpxc", "continuous", "toggle", sw::On ) ) == 0 );
        REQUIRE( app.m_continuous == true );
        REQUIRE( app.m_single == false );
        REQUIRE( app.wfsSemValue() == 1 );
    }

    SECTION( "continuous off while measuring requests a stop" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_measuring = 2;
        REQUIRE( app.newCallBack_m_indiP_continuous( makeSwitch( "dmpxc", "continuous", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == true );
    }

    SECTION( "continuous off while idle does nothing" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_continuous( makeSwitch( "dmpxc", "continuous", "toggle", sw::Off ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == false );
        REQUIRE( app.wfsSemValue() == 0 );
    }

    SECTION( "continuous without toggle is rejected" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_continuous( makeSwitch( "dmpxc", "continuous", "request", sw::On ) ) == -1 );
        REQUIRE( app.m_continuous == false );
    }

    SECTION( "stop while measuring requests a stop" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_measuring = 1;
        REQUIRE( app.newCallBack_m_indiP_stop( makeSwitch( "dmpxc", "stop", "request", sw::On ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == true );
    }

    SECTION( "stop while idle does nothing" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.newCallBack_m_indiP_stop( makeSwitch( "dmpxc", "stop", "request", sw::On ) ) == 0 );
        REQUIRE( app.m_stopMeasurement == false );
    }

    SECTION( "stop without request is rejected" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_measuring = 1;
        REQUIRE( app.newCallBack_m_indiP_stop( makeSwitch( "dmpxc", "stop", "toggle", sw::On ) ) == -1 );
        REQUIRE( app.m_stopMeasurement == false );
    }
}

/// Verify the dark stream allocation and dark frame copy.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr dark allocate and processImage", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::allocate(const darkShmimT &);
    dev::dmPokeWFS<dmPokeXCorr>::processImage(void *, const darkShmimT &);
    #endif
    // clang-format on

    SECTION( "matching dark geometry is valid and converts pixels to float" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_UINT16 );
        app.setDarkGeometry( 3, 2, IMAGESTRUCT_UINT16 );

        REQUIRE( app.allocate( pokeWFST::darkShmimT() ) == 0 );
        REQUIRE( app.m_darkValid == true );
        REQUIRE( app.m_darkImage.rows() == 3 );
        REQUIRE( app.m_darkImage.cols() == 2 );

        std::vector<uint16_t> dark = { 1, 2, 3, 4, 5, 60000 };
        REQUIRE( app.processImage( dark.data(), pokeWFST::darkShmimT() ) == 0 );
        for( size_t n = 0; n < dark.size(); ++n )
        {
            REQUIRE( app.m_darkImage.data()[n] == Approx( static_cast<float>( dark[n] ) ) );
        }
    }

    SECTION( "mismatched dark geometry is not valid" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_UINT16 );
        app.setDarkGeometry( 3, 3, IMAGESTRUCT_FLOAT );

        REQUIRE( app.allocate( pokeWFST::darkShmimT() ) == 0 );
        REQUIRE( app.m_darkValid == false );
        REQUIRE( app.m_darkImage.rows() == 3 );
        REQUIRE( app.m_darkImage.cols() == 3 );
    }
}

/// Verify the WFS stream allocation and the dark-subtracted frame copy.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr WFS allocate and processImage", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::allocate(const wfsShmimT &);
    dev::dmPokeWFS<dmPokeXCorr>::processImage(void *, const wfsShmimT &);
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "missing DM channel fails" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan = "dmpxc_nodm";
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_FLOAT );

        REQUIRE( app.allocate( pokeWFST::wfsShmimT() ) == -1 );
    }

    SECTION( "allocation sizes the images and frames are dark subtracted" )
    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpxc_dm", 4, 5 );

        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan = "dmpxc_dm";
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_UINT16 );
        app.setDarkGeometry( 3, 2, IMAGESTRUCT_UINT16 );

        REQUIRE( app.allocate( pokeWFST::wfsShmimT() ) == 0 );
        REQUIRE( app.m_darkValid == true );
        REQUIRE( app.m_rawImage.rows() == 3 );
        REQUIRE( app.m_rawImage.cols() == 2 );
        REQUIRE( app.m_pokeImage.valid() );
        REQUIRE( app.m_pokeImage.rows() == 3 );
        REQUIRE( app.m_pokeImage.cols() == 2 );
        REQUIRE( app.m_pokeLocal.rows() == 3 );
        REQUIRE( app.m_pokeLocal.cols() == 2 );
        REQUIRE( app.m_dmImage.rows() == 4 );
        REQUIRE( app.m_dmImage.cols() == 5 );
        REQUIRE( app.m_dmStream.passive() == true );

        REQUIRE( app.allocate( pokeWFST::darkShmimT() ) == 0 );
        std::vector<uint16_t> dark = { 1, 1, 2, 2, 3, 3 };
        REQUIRE( app.processImage( dark.data(), pokeWFST::darkShmimT() ) == 0 );

        std::vector<uint16_t> frame = { 10, 20, 30, 40, 50, 60 };
        REQUIRE( app.imageSemValue() == 0 );
        REQUIRE( app.processImage( frame.data(), pokeWFST::wfsShmimT() ) == 0 );
        REQUIRE( app.imageSemValue() == 1 );

        for( size_t n = 0; n < frame.size(); ++n )
        {
            REQUIRE( app.m_rawImage.data()[n] == Approx( static_cast<float>( frame[n] ) - dark[n] ) );
        }
    }

    SECTION( "frames are copied unchanged without a valid dark" )
    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpxc_dm", 4, 5 );

        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan = "dmpxc_dm";
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_FLOAT );

        REQUIRE( app.allocate( pokeWFST::wfsShmimT() ) == 0 );
        REQUIRE( app.m_darkValid == false );

        std::vector<float> frame = { 1.5, -2.5, 3.5, 4.5, 5.5, 6.5 };
        REQUIRE( app.processImage( frame.data(), pokeWFST::wfsShmimT() ) == 0 );
        REQUIRE( app.processImage( frame.data(), pokeWFST::wfsShmimT() ) == 0 );
        REQUIRE( app.imageSemValue() == 2 );

        for( size_t n = 0; n < frame.size(); ++n )
        {
            REQUIRE( app.m_rawImage.data()[n] == Approx( frame[n] ) );
        }
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify runSensor error and stop handling.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr runSensor error and stop handling", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::runSensor(true);
    dev::dmPokeWFS<dmPokeXCorr>::basicRunSensor();
    dev::dmPokeWFS<dmPokeXCorr>::basicTimedPoke(1.0);
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "no poke image" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        REQUIRE( app.runSensor( true ) == -1 );
    }

    SECTION( "a stop request zeroes the DM and leaves the poke image unchanged" )
    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpxc_dm", 4, 4 );
        dm().setConstant( 3.0 );

        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan   = "dmpxc_dm";
        app.m_poke_x   = { 1 };
        app.m_poke_y   = { 2 };
        app.m_poke_amp = 0.5;
        app.m_dmSleep  = 100;
        app.setWfsGeometry( 3, 2, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocate( pokeWFST::wfsShmimT() ) == 0 );

        app.m_pokeImage().setConstant( 7.0 );
        app.m_stopMeasurement = true;

        REQUIRE( app.runSensor( true ) == 0 );
        REQUIRE( dm().abs().maxCoeff() == Approx( 0.0 ) );
        REQUIRE( app.m_pokeImage().minCoeff() == Approx( 7.0 ) );
        REQUIRE( app.m_pokeImage().maxCoeff() == Approx( 7.0 ) );
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify runSensor pokes the DM and averages the +/- poke response.
/**
 * A feeder thread plays the WFS camera: each frame is a fixed response image scaled by the current
 * value of the poked DM actuator, delivered through `processImage()`. The measured poke image must then
 * equal the poke amplitude times the response, and the DM must be zeroed afterwards.
 *
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr runSensor measures the poke response", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::runSensor(true);
    dev::dmPokeWFS<dmPokeXCorr>::basicRunSensor();
    dev::dmPokeWFS<dmPokeXCorr>::basicTimedPoke(1.0);
    #endif
    // clang-format on

    useTestShmDir();

    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpxc_dm", 4, 4 );
        dm().setZero();

        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan       = "dmpxc_dm";
        app.m_poke_x       = { 1 };
        app.m_poke_y       = { 2 };
        app.m_poke_amp     = 0.5;
        app.m_dmSleep      = 2000;
        app.m_nPokeImages  = 2;
        app.m_nPokeAverage = 2;
        app.setWfsGeometry( 8, 6, IMAGESTRUCT_FLOAT );
        REQUIRE( app.allocate( pokeWFST::wfsShmimT() ) == 0 );

        mx::improc::eigenImage<float> resp( 8, 6 );
        for( int r = 0; r < resp.rows(); ++r )
        {
            for( int c = 0; c < resp.cols(); ++c )
            {
                resp( r, c ) = 1.0f + r + 10.0f * c;
            }
        }

        std::atomic<bool> done{ false };

        std::thread feeder(
            [&]()
            {
                mx::improc::eigenImage<float> frame( 8, 6 );
                auto                          t0 = std::chrono::steady_clock::now();
                while( !done )
                {
                    float act = dm()( 1, 2 );
                    frame     = act * resp;
                    app.processImage( frame.data(), pokeWFST::wfsShmimT() );
                    mx::sys::microSleep( 500 );

                    // Watchdog: stop the measurement instead of hanging if something goes wrong.
                    if( std::chrono::steady_clock::now() - t0 > std::chrono::seconds( 20 ) )
                    {
                        app.m_shutdown = 1;
                        break;
                    }
                }
            } );

        int rv = app.runSensor( true );

        done = true;
        feeder.join();

        REQUIRE( rv == 0 );
        REQUIRE( app.m_shutdown == 0 );

        for( int r = 0; r < resp.rows(); ++r )
        {
            for( int c = 0; c < resp.cols(); ++c )
            {
                REQUIRE( app.m_pokeImage()( r, c ) == Approx( 0.5 * resp( r, c ) ).epsilon( 1e-4 ) );
            }
        }

        REQUIRE( dm().abs().maxCoeff() == Approx( 0.0 ) );
        REQUIRE( app.m_dmImage.abs().maxCoeff() == Approx( 0.0 ) );
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify the zonal response matrix processing builds the reference image from the poked actuators.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr builds the reference from the zonal response matrix", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::allocate(const zrespShmimT &);
    dmPokeXCorr::processImage(void *, const zrespShmimT &);
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "missing DM channel fails" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan = "dmpxc_nodm";
        app.m_poke_x = { 1 };
        app.m_poke_y = { 2 };
        app.setZrespGeometry( 8, 8, 16 );
        REQUIRE( app.allocate( zrespShmimT() ) == 0 );

        std::vector<float> cube( 8 * 8 * 16, 0.0f );
        REQUIRE( app.processImage( cube.data(), zrespShmimT() ) == -1 );
    }

    SECTION( "the reference is the sum of the poked actuator responses" )
    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpxc_dm", 4, 4 );

        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan = "dmpxc_dm";
        app.m_poke_x = { 1, 2 };
        app.m_poke_y = { 2, 0 };
        app.setZrespGeometry( 32, 32, 16 );
        REQUIRE( app.allocate( zrespShmimT() ) == 0 );
        REQUIRE( app.m_refIm.rows() == 32 );
        REQUIRE( app.m_refIm.cols() == 32 );

        // The poked actuators are actno = y * dm.rows() + x = 2*4+1 = 9 and 0*4+2 = 2.  Planes 9 and 2 hold
        // G and 2G, and the x/y-swapped planes (6 and 8) hold other multiples, so only the right planes give 3G.
        mx::improc::eigenImage<float> gauss( 32, 32 );
        fillGaussian( gauss, 15.5, 15.5, 2.5 );

        mx::improc::eigenCube<float> zresp( 32, 32, 16 );
        zresp.setZero();
        zresp.image( 9 ) = gauss;
        zresp.image( 2 ) = 2.0f * gauss;
        zresp.image( 6 ) = 5.0f * gauss;
        zresp.image( 8 ) = 7.0f * gauss;

        REQUIRE( app.processImage( zresp.data(), zrespShmimT() ) == 0 );

        for( int r = 0; r < 32; r += 3 )
        {
            for( int c = 0; c < 32; c += 3 )
            {
                REQUIRE( app.m_refIm()( r, c ) == Approx( 3.0 * gauss( r, c ) ).margin( 1e-6 ) );
            }
        }
        REQUIRE( app.m_xcorr.refIm().rows() == 32 );
        REQUIRE( app.m_xcorr.refIm().cols() == 32 );
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify analyzeSensor measures the shift of the poke image relative to the reference.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr analyzeSensor measures the poke image shift", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dmPokeXCorr::analyzeSensor();
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "no reference" )
    {
        dmPokeXCorr_test app( "dmpxc" );
        app.createPokeImage( 32, 32 );
        REQUIRE( app.analyzeSensor() == -1 );
    }

    SECTION( "shifted Gaussian" )
    {
        mx::improc::milkImage<float> dm;
        dm.create( "dmpxc_dm", 4, 4 );

        dmPokeXCorr_test app( "dmpxc" );
        app.m_dmChan = "dmpxc_dm";
        app.m_poke_x = { 1 };
        app.m_poke_y = { 2 };
        app.setZrespGeometry( 32, 32, 16 );
        REQUIRE( app.allocate( zrespShmimT() ) == 0 );

        mx::improc::eigenCube<float> zresp( 32, 32, 16 );
        zresp.setZero();
        mx::improc::eigenImage<float> ref( 32, 32 );
        fillGaussian( ref, 15.5, 15.5, 2.5 );
        zresp.image( 9 ) = ref;

        REQUIRE( app.processImage( zresp.data(), zrespShmimT() ) == 0 );

        app.createPokeImage( 32, 32 );

        SECTION( "no shift" )
        {
            app.m_pokeImage = ref;

            REQUIRE( app.analyzeSensor() == 0 );
            REQUIRE( app.m_deltaX == Approx( 0.0 ).margin( 0.1 ) );
            REQUIRE( app.m_deltaY == Approx( 0.0 ).margin( 0.1 ) );
        }

        SECTION( "shift of (+2, -1)" )
        {
            mx::improc::eigenImage<float> im( 32, 32 );
            fillGaussian( im, 17.5, 14.5, 2.5 );
            app.m_pokeImage = im;

            REQUIRE( app.analyzeSensor() == 0 );
            REQUIRE( app.m_deltaX == Approx( 2.0 ).margin( 0.2 ) );
            REQUIRE( app.m_deltaY == Approx( -1.0 ).margin( 0.2 ) );
            REQUIRE( app.m_indiP_measurement["delta_x"].get<float>() == Approx( app.m_deltaX ) );
            REQUIRE( app.m_indiP_measurement["delta_y"].get<float>() == Approx( app.m_deltaY ) );
        }

        SECTION( "size mismatch" )
        {
            app.createPokeImage( 16, 16 );
            REQUIRE( app.analyzeSensor() == -1 );
        }
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify the poke loop telemetry is recorded on change, when forced, and when the interval elapses.
/**
 * Recording is detected through `telem_pokeloop::lastRecord`, which `telemeter::telem()` updates.
 *
 * \ingroup dmPokeXCorr_unit_test
 */
TEST_CASE( "dmPokeXCorr poke loop telemetry", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::recordPokeLoop(false);
    dev::dmPokeWFS<dmPokeXCorr>::recordTelem(nullptr);
    dmPokeXCorr::checkRecordTimes();
    #endif
    // clang-format on

    using MagAOX::logger::telem_pokeloop;

    dmPokeXCorr_test app( "dmpxc" );
    app.m_maxInterval = 10.0;

    // Synchronize the internal last-recorded values.
    REQUIRE( app.recordPokeLoop( true ) == 0 );

    SECTION( "unchanged values are not recorded" )
    {
        telem_pokeloop::lastRecord = { 0, 0 };
        REQUIRE( app.recordPokeLoop( false ) == 0 );
        REQUIRE( telem_pokeloop::lastRecord.tv_sec == 0 );
    }

    SECTION( "a counter change is recorded" )
    {
        telem_pokeloop::lastRecord = { 0, 0 };
        ++app.m_counter;
        REQUIRE( app.recordPokeLoop( false ) == 0 );
        REQUIRE( telem_pokeloop::lastRecord.tv_sec > 0 );
    }

    SECTION( "a delta change is recorded" )
    {
        telem_pokeloop::lastRecord = { 0, 0 };
        app.m_deltaY               = 0.3;
        REQUIRE( app.recordPokeLoop( false ) == 0 );
        REQUIRE( telem_pokeloop::lastRecord.tv_sec > 0 );
    }

    SECTION( "a measuring change is recorded" )
    {
        telem_pokeloop::lastRecord = { 0, 0 };
        app.m_measuring            = 2;
        REQUIRE( app.recordPokeLoop( false ) == 0 );
        REQUIRE( telem_pokeloop::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordTelem always records" )
    {
        telem_pokeloop::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( telem_pokeloop::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records after the interval" )
    {
        telem_pokeloop::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_pokeloop::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes skips a recent record" )
    {
        timespec now;
        clock_gettime( CLOCK_REALTIME, &now );
        telem_pokeloop::lastRecord = now;

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_pokeloop::lastRecord.tv_sec == now.tv_sec );
        REQUIRE( telem_pokeloop::lastRecord.tv_nsec == now.tv_nsec );
    }
}

} // namespace dmPokeXCorrTest

} // namespace libXWCTest
