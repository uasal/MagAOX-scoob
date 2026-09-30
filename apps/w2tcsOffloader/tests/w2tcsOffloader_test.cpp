/** \file w2tcsOffloader_test.cpp
 * \brief Catch2 tests for the w2tcsOffloader app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup w2tcsOffloader_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>
#include <limits>
#include <string>
#include <vector>

#include "../w2tcsOffloader.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup w2tcsOffloader_unit_test w2tcsOffloader Unit Tests
 * \brief Unit tests for the w2tcsOffloader application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `w2tcsOffloader` unit tests.
/** \ingroup w2tcsOffloader_unit_test
 */
namespace w2tcsOffloaderTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing w2tcsOffloader internals.
class w2tcsOffloader_test : public w2tcsOffloader
{
  public:
    /// Construct a harness with the given device name.
    explicit w2tcsOffloader_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    using w2tcsOffloader::m_elNames;
    using w2tcsOffloader::m_indiP_zCoeffs;
    using w2tcsOffloader::m_lastZCoeffs;
    using w2tcsOffloader::m_nModes;
    using w2tcsOffloader::m_norm;
    using w2tcsOffloader::m_wMask;
    using w2tcsOffloader::m_wMaskPath;
    using w2tcsOffloader::m_woofer;
    using w2tcsOffloader::m_wZModes;
    using w2tcsOffloader::m_wZModesPath;
    using w2tcsOffloader::m_zCoeffs;

    using dev::shmimMonitor<w2tcsOffloader>::m_height;
    using dev::shmimMonitor<w2tcsOffloader>::m_shmimName;
    using dev::shmimMonitor<w2tcsOffloader>::m_width;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();

        // Keep telemetry records out of the queue so nothing is written at destruction.
        m_tel.m_logLevel = logPrio::LOG_INFO;
    }

    /// Run `setupConfig()`, read a configuration file, and run `loadConfigImpl()` on the app configurator.
    /**
     * \returns the return value of `loadConfigImpl()`
     */
    int configureImpl( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        int rv = loadConfigImpl( config );

        m_tel.m_logLevel = logPrio::LOG_INFO;

        return rv;
    }

    /// Set up a 2x2 projection with three modes, as `appStartup()` would after reading the FITS files.
    /** The modes are: 0 = all ones, 1 = only pixel (0,0), 2 = all ones.  The mask excludes pixel (1,1),
     * so the normalization is 3.
     */
    void setupProjection( unsigned nModes /**< [in] the number of modes to offload */ )
    {
        m_width  = 2;
        m_height = 2;

        m_wZModes.resize( 2, 2, 3 );
        m_wZModes.image( 0 ).setConstant( 1.0f );
        m_wZModes.image( 1 ).setConstant( 0.0f );
        m_wZModes.image( 1 )( 0, 0 ) = 1.0f;
        m_wZModes.image( 2 ).setConstant( 1.0f );

        m_wMask.resize( 2, 2 );
        m_wMask.setConstant( 1.0f );
        m_wMask( 1, 1 ) = 0.0f;
        m_norm          = m_wMask.sum();

        m_nModes = nModes;

        m_zCoeffs.assign( 3, 0.0f );
        m_lastZCoeffs.assign( 3, std::numeric_limits<float>::max() );

        m_indiP_zCoeffs = pcf::IndiProperty( pcf::IndiProperty::Number );
        m_indiP_zCoeffs.setDevice( m_configName );
        m_indiP_zCoeffs.setName( "zCoeffs" );

        m_elNames.resize( 3 );
        for( size_t n = 0; n < 3; ++n )
        {
            m_elNames[n] = std::format( "{:02}", n );
            m_indiP_zCoeffs.add( pcf::IndiElement( m_elNames[n] ) );
            m_indiP_zCoeffs[m_elNames[n]] = 0;
        }
    }
};

/// Write a float FITS file, replacing any existing file.
/**
 * \returns true on success
 */
bool writeFits( const std::string        &fname, /**< [in] the FITS file path */
                const std::vector<float> &data,  /**< [in] the pixel data, column-major, d1*d2*d3 values */
                int                       d1,    /**< [in] the first dimension */
                int                       d2,    /**< [in] the second dimension */
                int                       d3     /**< [in] the third dimension (number of planes) */
)
{
    std::filesystem::remove( fname );
    mx::fits::fitsFile<float> ff;
    return ( ff.write( fname, data.data(), d1, d2, d3 ) == mx::error_t::noerror );
}

/// Check whether two timespecs are equal.
/**
 * \returns true if both the seconds and nanoseconds match
 */
bool sameTime( const timespec &a, /**< [in] the first time */
               const timespec &b /**< [in] the second time */ )
{
    return ( a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec );
}

/// \endcond

/// Verify the w2tcsOffloader configuration defaults and overrides.
/**
 * \ingroup w2tcsOffloader_unit_test
 */
TEST_CASE( "w2tcsOffloader configuration", "[w2tcsOffloader]" )
{
    // clang-format off
    #ifdef W2TCSOFFLOADER_TEST_DOXYGEN_REF
    w2tcsOffloader::setupConfig();
    w2tcsOffloader::loadConfig();
    w2tcsOffloader::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/w2tcsOffloader_test_config.conf";

    SECTION( "defaults" )
    {
        mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

        w2tcsOffloader_test app( "w2tcs" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_wZModesPath == "" );
        REQUIRE( app.m_wMaskPath == "" );
        REQUIRE( app.m_nModes == 5u );
        REQUIRE( app.m_shmimName == "w2tcs" );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
    }

    SECTION( "overrides" )
    {
        mx::app::writeConfigFile( fname,
                                  { "offload", "offload", "offload", "shmimMonitor", "telemeter" },
                                  { "wZModesPath", "wMaskPath", "nModes", "shmimName", "maxInterval" },
                                  { "/tmp/modes.fits", "/tmp/mask.fits", "3", "dm00disp", "2.5" } );

        w2tcsOffloader_test app( "w2tcs" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_wZModesPath == "/tmp/modes.fits" );
        REQUIRE( app.m_wMaskPath == "/tmp/mask.fits" );
        REQUIRE( app.m_nModes == 3u );
        REQUIRE( app.m_shmimName == "dm00disp" );
        REQUIRE( app.m_maxInterval == Approx( 2.5 ) );
    }

    SECTION( "loadConfigImpl returns success" )
    {
        mx::app::writeConfigFile( fname, { "offload" }, { "nModes" }, { "7" } );

        w2tcsOffloader_test app( "w2tcs" );
        REQUIRE( app.configureImpl( fname ) == 0 );

        REQUIRE( app.m_nModes == 7u );
    }

    std::remove( fname.c_str() );
}

/// Verify `appStartup()` fails cleanly when the mode cube or mask cannot be read, and clamps the mode count.
/**
 * \ingroup w2tcsOffloader_unit_test
 */
TEST_CASE( "w2tcsOffloader appStartup file handling", "[w2tcsOffloader]" )
{
    // clang-format off
    #ifdef W2TCSOFFLOADER_TEST_DOXYGEN_REF
    w2tcsOffloader::appStartup();
    #endif
    // clang-format on

    const std::string modesPath = "/tmp/w2tcsOffloader_test_modes.fits";
    const std::string maskPath  = "/tmp/w2tcsOffloader_test_mask_missing.fits";
    std::filesystem::remove( maskPath );

    SECTION( "missing mode cube" )
    {
        std::filesystem::remove( modesPath );

        w2tcsOffloader_test app( "w2tcs" );
        app.m_wZModesPath = modesPath;
        app.m_wMaskPath   = maskPath;

        REQUIRE( app.appStartup() == -1 );
        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_zCoeffs.empty() );
    }

    SECTION( "mode count is clamped to the cube and the missing mask is reported" )
    {
        REQUIRE( writeFits( modesPath, std::vector<float>( 2 * 2 * 3, 1.0f ), 2, 2, 3 ) );

        w2tcsOffloader_test app( "w2tcs" );
        app.m_wZModesPath = modesPath;
        app.m_wMaskPath   = maskPath;
        app.m_nModes      = 5;

        REQUIRE( app.appStartup() == -1 );
        REQUIRE( app.shutdown() == 0 );

        REQUIRE( app.m_wZModes.planes() == 3 );
        REQUIRE( app.m_nModes == 3u );
        REQUIRE( app.m_zCoeffs.size() == 3 );
        REQUIRE( app.m_lastZCoeffs.size() == 3 );
        for( size_t n = 0; n < 3; ++n )
        {
            REQUIRE( app.m_zCoeffs[n] == 0.0f );
            REQUIRE( app.m_lastZCoeffs[n] == std::numeric_limits<float>::max() );
        }
    }

    SECTION( "mode count smaller than the cube is kept" )
    {
        REQUIRE( writeFits( modesPath, std::vector<float>( 2 * 2 * 3, 1.0f ), 2, 2, 3 ) );

        w2tcsOffloader_test app( "w2tcs" );
        app.m_wZModesPath = modesPath;
        app.m_wMaskPath   = maskPath;
        app.m_nModes      = 2;

        REQUIRE( app.appStartup() == -1 );
        REQUIRE( app.m_nModes == 2u );
        REQUIRE( app.m_zCoeffs.size() == 3 );
    }

    SECTION( "more than 100 modes is a critical error" )
    {
        REQUIRE( writeFits( modesPath, std::vector<float>( 2 * 2 * 101, 1.0f ), 2, 2, 101 ) );

        w2tcsOffloader_test app( "w2tcs" );
        app.m_wZModesPath = modesPath;
        app.m_wMaskPath   = maskPath;
        app.m_nModes      = 200;

        REQUIRE( app.appStartup() == -1 );
        REQUIRE( app.shutdown() != 0 );
        REQUIRE( app.m_nModes == 101u );
        REQUIRE( app.m_zCoeffs.size() == 101 );
    }

    std::filesystem::remove( modesPath );
}

/// Verify `allocate()` sizes the woofer buffer to the stream and `appShutdown()` succeeds without threads.
/**
 * \ingroup w2tcsOffloader_unit_test
 */
TEST_CASE( "w2tcsOffloader allocate and shutdown", "[w2tcsOffloader]" )
{
    // clang-format off
    #ifdef W2TCSOFFLOADER_TEST_DOXYGEN_REF
    w2tcsOffloader::allocate( dev::shmimT() );
    w2tcsOffloader::appShutdown();
    #endif
    // clang-format on

    w2tcsOffloader_test app( "w2tcs" );
    app.m_width  = 4;
    app.m_height = 3;

    REQUIRE( app.allocate( dev::shmimT() ) == 0 );
    REQUIRE( app.m_woofer.rows() == 4 );
    REQUIRE( app.m_woofer.cols() == 3 );

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify `processImage()` projects the woofer onto the masked modes and zeroes modes beyond `nModes`.
/**
 * \ingroup w2tcsOffloader_unit_test
 */
TEST_CASE( "w2tcsOffloader processImage projects onto the modes", "[w2tcsOffloader]" )
{
    // clang-format off
    #ifdef W2TCSOFFLOADER_TEST_DOXYGEN_REF
    w2tcsOffloader::processImage( nullptr, dev::shmimT() );
    w2tcsOffloader::recordZCoeffs( false );
    #endif
    // clang-format on

    // Column-major 2x2 frame: (0,0)=1, (1,0)=2, (0,1)=3, (1,1)=4.  Pixel (1,1) is masked out.
    float frame[4] = { 1.0f, 2.0f, 3.0f, 4.0f };

    SECTION( "two of three modes offloaded" )
    {
        w2tcsOffloader_test app( "w2tcs" );
        app.setupProjection( 2 );
        REQUIRE( app.m_norm == Approx( 3.0f ) );

        REQUIRE( app.processImage( frame, dev::shmimT() ) == 0 );

        // The woofer copy matches the frame.
        REQUIRE( app.m_woofer.rows() == 2 );
        REQUIRE( app.m_woofer.cols() == 2 );
        REQUIRE( app.m_woofer( 0, 0 ) == 1.0f );
        REQUIRE( app.m_woofer( 1, 0 ) == 2.0f );
        REQUIRE( app.m_woofer( 0, 1 ) == 3.0f );
        REQUIRE( app.m_woofer( 1, 1 ) == 4.0f );

        REQUIRE( app.m_zCoeffs[0] == Approx( 2.0f ) );        //(1+2+3)/3
        REQUIRE( app.m_zCoeffs[1] == Approx( 1.0f / 3.0f ) ); // only pixel (0,0)
        REQUIRE( app.m_zCoeffs[2] == 0.0f );                  // beyond nModes

        REQUIRE( app.m_indiP_zCoeffs["00"].get<float>() == Approx( 2.0f ) );
        REQUIRE( app.m_indiP_zCoeffs["01"].get<float>() == Approx( 1.0f / 3.0f ) );
        REQUIRE( app.m_indiP_zCoeffs["02"].get<float>() == Approx( 0.0f ) );
        REQUIRE( app.m_indiP_zCoeffs.getState() == pcf::IndiProperty::Ok );

        // The changed coefficients were recorded for telemetry.
        REQUIRE( app.m_lastZCoeffs == app.m_zCoeffs );
    }

    SECTION( "all modes offloaded" )
    {
        w2tcsOffloader_test app( "w2tcs" );
        app.setupProjection( 3 );

        REQUIRE( app.processImage( frame, dev::shmimT() ) == 0 );

        REQUIRE( app.m_zCoeffs[0] == Approx( 2.0f ) );
        REQUIRE( app.m_zCoeffs[1] == Approx( 1.0f / 3.0f ) );
        REQUIRE( app.m_zCoeffs[2] == Approx( 2.0f ) );
        REQUIRE( app.m_indiP_zCoeffs["02"].get<float>() == Approx( 2.0f ) );
    }

    SECTION( "no modes offloaded zeroes everything" )
    {
        w2tcsOffloader_test app( "w2tcs" );
        app.setupProjection( 0 );
        app.m_zCoeffs.assign( 3, 5.0f );

        REQUIRE( app.processImage( frame, dev::shmimT() ) == 0 );

        for( size_t n = 0; n < 3; ++n )
        {
            REQUIRE( app.m_zCoeffs[n] == 0.0f );
            REQUIRE( app.m_indiP_zCoeffs[app.m_elNames[n]].get<float>() == Approx( 0.0f ) );
        }
    }

    SECTION( "reducing nModes between frames zeroes the dropped modes" )
    {
        w2tcsOffloader_test app( "w2tcs" );
        app.setupProjection( 3 );

        REQUIRE( app.processImage( frame, dev::shmimT() ) == 0 );
        REQUIRE( app.m_zCoeffs[2] == Approx( 2.0f ) );

        app.m_nModes = 1;
        REQUIRE( app.processImage( frame, dev::shmimT() ) == 0 );
        REQUIRE( app.m_zCoeffs[0] == Approx( 2.0f ) );
        REQUIRE( app.m_zCoeffs[1] == 0.0f );
        REQUIRE( app.m_zCoeffs[2] == 0.0f );
        REQUIRE( app.m_lastZCoeffs == app.m_zCoeffs );
    }

    SECTION( "the mask normalization scales the coefficients" )
    {
        w2tcsOffloader_test app( "w2tcs" );
        app.setupProjection( 1 );
        app.m_norm = 1.0f;

        REQUIRE( app.processImage( frame, dev::shmimT() ) == 0 );
        REQUIRE( app.m_zCoeffs[0] == Approx( 6.0f ) );
    }
}

/// Verify `recordZCoeffs()` records only on change unless forced, and tracks the last recorded values.
/**
 * \ingroup w2tcsOffloader_unit_test
 */
TEST_CASE( "w2tcsOffloader recordZCoeffs change detection", "[w2tcsOffloader]" )
{
    // clang-format off
    #ifdef W2TCSOFFLOADER_TEST_DOXYGEN_REF
    w2tcsOffloader::recordZCoeffs( false );
    #endif
    // clang-format on

    w2tcsOffloader_test app( "w2tcs" );

    const timespec zero{ 0, 0 };

    SECTION( "first record after startup always logs" )
    {
        app.m_zCoeffs     = { 0.0f, 0.0f };
        app.m_lastZCoeffs = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        MagAOX::logger::telem_w2tcsoffloader::lastRecord = zero;

        REQUIRE( app.recordZCoeffs() == 0 );
        REQUIRE( app.m_lastZCoeffs == app.m_zCoeffs );
        REQUIRE_FALSE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );
    }

    SECTION( "unchanged coefficients are not recorded unless forced" )
    {
        app.m_zCoeffs                                    = { 1.0f, -2.0f };
        app.m_lastZCoeffs                                = { 1.0f, -2.0f };
        MagAOX::logger::telem_w2tcsoffloader::lastRecord = zero;

        REQUIRE( app.recordZCoeffs() == 0 );
        REQUIRE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );

        REQUIRE( app.recordZCoeffs( true ) == 0 );
        REQUIRE_FALSE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );
        REQUIRE( app.m_lastZCoeffs == app.m_zCoeffs );
    }

    SECTION( "a change in any coefficient is recorded" )
    {
        app.m_zCoeffs     = { 1.0f, -2.0f, 3.0f };
        app.m_lastZCoeffs = { 1.0f, -2.0f, 3.0f };

        app.m_zCoeffs[2]                                 = 3.5f;
        MagAOX::logger::telem_w2tcsoffloader::lastRecord = zero;

        REQUIRE( app.recordZCoeffs() == 0 );
        REQUIRE_FALSE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );
        REQUIRE( app.m_lastZCoeffs[2] == 3.5f );
    }

    SECTION( "a size mismatch resizes the last-recorded vector and records" )
    {
        app.m_zCoeffs = { 0.5f, 0.25f, 0.125f };
        app.m_lastZCoeffs.clear();
        MagAOX::logger::telem_w2tcsoffloader::lastRecord = zero;

        REQUIRE( app.recordZCoeffs() == 0 );
        REQUIRE( app.m_lastZCoeffs.size() == 3 );
        REQUIRE( app.m_lastZCoeffs == app.m_zCoeffs );
        REQUIRE_FALSE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );
    }

    SECTION( "empty coefficients record only when forced" )
    {
        app.m_zCoeffs.clear();
        app.m_lastZCoeffs.clear();
        MagAOX::logger::telem_w2tcsoffloader::lastRecord = zero;

        REQUIRE( app.recordZCoeffs() == 0 );
        REQUIRE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );

        REQUIRE( app.recordZCoeffs( true ) == 0 );
        REQUIRE_FALSE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );
    }
}

/// Verify the telemeter hooks record on the max interval and via `recordTelem()`.
/**
 * \ingroup w2tcsOffloader_unit_test
 */
TEST_CASE( "w2tcsOffloader telemeter hooks", "[w2tcsOffloader]" )
{
    // clang-format off
    #ifdef W2TCSOFFLOADER_TEST_DOXYGEN_REF
    w2tcsOffloader::checkRecordTimes();
    w2tcsOffloader::recordTelem( nullptr );
    #endif
    // clang-format on

    w2tcsOffloader_test app( "w2tcs" );
    app.m_zCoeffs     = { 1.0f, 2.0f };
    app.m_lastZCoeffs = { 1.0f, 2.0f };

    const timespec zero{ 0, 0 };

    SECTION( "recordTelem forces a record" )
    {
        MagAOX::logger::telem_w2tcsoffloader::lastRecord = zero;

        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE_FALSE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, zero ) );
    }

    SECTION( "checkRecordTimes records once the max interval has elapsed" )
    {
        app.m_maxInterval                                = 10.0;
        MagAOX::logger::telem_w2tcsoffloader::lastRecord = zero;

        REQUIRE( app.checkRecordTimes() == 0 );

        timespec first = MagAOX::logger::telem_w2tcsoffloader::lastRecord;
        REQUIRE_FALSE( sameTime( first, zero ) );

        // Immediately after a record the interval has not elapsed, so nothing new is recorded.
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( sameTime( MagAOX::logger::telem_w2tcsoffloader::lastRecord, first ) );
    }
}

} // namespace w2tcsOffloaderTest

} // namespace libXWCTest
