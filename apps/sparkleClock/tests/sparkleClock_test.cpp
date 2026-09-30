/** \file sparkleClock_test.cpp
 * \brief Catch2 tests for the sparkleClock app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup sparkleClock_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../sparkleClock.hpp"

// Included after the app header so that the INDI callback bodies stay live.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace MagAOX
{
namespace app
{

/// Test harness exposing sparkleClock internals (declared a friend by the app).
class sparkleClock_test : public sparkleClock
{
  public:
    /// The telemeter base type.
    typedef dev::telemeter<sparkleClock> telemeterT;

    /// Metadata backing the in-memory DM channel stream.
    IMAGE_METADATA m_testMd;

    /// Pixel data backing the in-memory DM channel stream.
    std::vector<float> m_testData;

    /// Construct a harness with the given device name, setting up the INDI properties and an in-memory DM stream.
    explicit sparkleClock_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        createStandardIndiNumber<float>( m_indiP_delay, "delay", 0, 0, 1, "%f" );
        createStandardIndiNumber<float>( m_indiP_separation_1, "separation_1", 2, 24, 100, "%f" );
        createStandardIndiNumber<float>( m_indiP_separation_2, "separation_2", 2, 24, 100, "%f" );
        createStandardIndiNumber<float>( m_indiP_angle, "angle", 0, 0, 100, "%f" );
        createStandardIndiNumber<float>( m_indiP_amp, "amp", -1, 0, 1, "%f" );
        createStandardIndiNumber<float>( m_indiP_frequency, "frequency", 0, 0, 10000, "%f" );
        createStandardIndiNumber<float>( m_indiP_interval, "interval", 0, 0, 10000, "%f" );
        createStandardIndiNumber<int>( m_indiP_dwell, "dwell", 1, 100, 1, "%d" );
        createStandardIndiNumber<int>( m_indiP_single, "single", -1, 3, 1, "%d" );
        createStandardIndiToggleSw( m_indiP_cross, "cross" );
        createStandardIndiToggleSw( m_indiP_trigger, "trigger" );
        createStandardIndiToggleSw( m_indiP_modulating, "modulating" );
        createStandardIndiRequestSw( m_indiP_zero, "zero" );

        // An in-memory DM stream with no semaphores.
        std::memset( &m_testMd, 0, sizeof( m_testMd ) );
        m_testData.assign( 4, 0.0f );
        std::memset( &m_imageStream, 0, sizeof( m_imageStream ) );
        m_imageStream.md        = &m_testMd;
        m_imageStream.array.raw = m_testData.data();
        m_imageStream.semlog    = nullptr;
    }

    /// Set the DM geometry used for the shapes and writes.
    void setGeometry( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        m_width    = w;
        m_height   = h;
        m_typeSize = sizeof( float );
        m_testData.assign( w * h, 0.0f );
        m_imageStream.array.raw = m_testData.data();
    }

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

    /// Fill the in-memory DM stream data, keeping the stream pointer valid.
    void setStreamData( const std::vector<float> &data /**< [in] new stream contents */ )
    {
        m_testData              = data;
        m_imageStream.array.raw = m_testData.data();
    }

    /// Telemeter maximum interval.
    double telMaxInterval()
    {
        return telemeterT::m_maxInterval;
    }

    /// Telemeter log name.
    std::string telLogName()
    {
        return telemeterT::m_tel.logName();
    }

    using sparkleClock::m_amp;
    using sparkleClock::m_angle;
    using sparkleClock::m_angleOffset;
    using sparkleClock::m_cross;
    using sparkleClock::m_dataType;
    using sparkleClock::m_dmChannelName;
    using sparkleClock::m_dmName;
    using sparkleClock::m_dmTriggerChannel;
    using sparkleClock::m_dwell;
    using sparkleClock::m_frequency;
    using sparkleClock::m_height;
    using sparkleClock::m_imageStream;
    using sparkleClock::m_interval;
    using sparkleClock::m_modThreadCpuset;
    using sparkleClock::m_modThreadPrio;
    using sparkleClock::m_modulating;
    using sparkleClock::m_restartSp;
    using sparkleClock::m_separation_1;
    using sparkleClock::m_separation_2;
    using sparkleClock::m_shapes;
    using sparkleClock::m_shutdown;
    using sparkleClock::m_single;
    using sparkleClock::m_sparkleClockInterval;
    using sparkleClock::m_trigger;
    using sparkleClock::m_triggerDelay;
    using sparkleClock::m_triggerSemaphore;
    using sparkleClock::m_typeSize;
    using sparkleClock::m_width;

    using sparkleClock::generateSparkleClock;
};

} // namespace app
} // namespace MagAOX
/// \endcond

namespace libXWCTest
{

/** \defgroup sparkleClock_unit_test sparkleClock Unit Tests
 * \brief Unit tests for the sparkleClock application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `sparkleClock` unit tests.
/** \ingroup sparkleClock_unit_test
 */
namespace sparkleClockTest
{

/// Name of the debug file generateSparkleClock() writes.
static const char *c_specksFile = "/tmp/specks.fits";

/// Build a number property with optional current and target elements.
static pcf::IndiProperty makeNumberProperty( const std::string &device /**< [in] property device */,
                                             const std::string &name /**< [in] property name */,
                                             bool               withCurrent /**< [in] add a current element */,
                                             double             current /**< [in] current value */,
                                             bool               withTarget /**< [in] add a target element */,
                                             double             target /**< [in] target value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    if( withCurrent )
    {
        ip.add( pcf::IndiElement( "current", current ) );
    }
    if( withTarget )
    {
        ip.add( pcf::IndiElement( "target", target ) );
    }
    return ip;
}

/// Build a number property with only a target element.
static pcf::IndiProperty makeTarget( const std::string &device /**< [in] property device */,
                                     const std::string &name /**< [in] property name */,
                                     double             target /**< [in] target value */ )
{
    return makeNumberProperty( device, name, false, 0, true, target );
}

/// Build a switch property with an optional single element.
static pcf::IndiProperty makeSwitch( const std::string &device /**< [in] property device */,
                                     const std::string &name /**< [in] property name */,
                                     const std::string &el /**< [in] element name, "" for none */,
                                     bool               on /**< [in] element state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    if( el != "" )
    {
        ip.add( pcf::IndiElement( el ) );
        ip[el].setSwitchState( on ? pcf::IndiElement::On : pcf::IndiElement::Off );
    }
    return ip;
}

/// Evaluate the cosine Fourier mode used by mx::sigproc::makeFourierMode at pixel (i,j) of a DxD image.
static double cosMode( double m /**< [in] u spatial frequency */,
                       double n /**< [in] v spatial frequency */,
                       int    i /**< [in] first pixel index */,
                       int    j /**< [in] second pixel index */,
                       int    D /**< [in] image size */ )
{
    double c = 0.5 * ( D - 1.0 );
    return cos( 2 * M_PI / D * ( m * ( i - c ) + n * ( j - c ) ) );
}

/// Verify default configuration values.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock configuration defaults", "[sparkleClock]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::sparkleClock();
    sparkleClock::setupConfig();
    sparkleClock::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    sparkleClock_test app( "sparkleclock" );
    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/sparkleClock_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    REQUIRE( app.loadConfigFromFile( "/tmp/sparkleClock_test_defaults.conf" ) == 0 );

    CHECK( app.m_dmChannelName == "" );
    CHECK( app.m_dmName == "" );
    CHECK( app.m_dmTriggerChannel == "" );
    CHECK( app.m_triggerSemaphore == 9 );
    CHECK( app.m_trigger == true );
    CHECK( app.m_triggerDelay == 0 );
    CHECK( app.m_separation_1 == Approx( 10.0 ) );
    CHECK( app.m_separation_2 == Approx( 20.0 ) );
    CHECK( app.m_angle == Approx( 0.0 ) );
    CHECK( app.m_angleOffset == Approx( 28.0 ) );
    CHECK( app.m_amp == Approx( 0.01 ) );
    CHECK( app.m_cross == true );
    CHECK( app.m_frequency == Approx( 2000 ) );
    CHECK( app.m_sparkleClockInterval == Approx( 1.0 ) );
    CHECK( app.m_dwell == 1 );
    CHECK( app.m_single == -1 );
    CHECK( app.m_modThreadPrio == 60 );
    CHECK( app.m_modThreadCpuset == "" );
    CHECK( app.telMaxInterval() == Approx( 10.0 ) );
    CHECK( app.telLogName() == "sparkleclock" );

    std::remove( "/tmp/sparkleClock_test_defaults.conf" );
}

/// Verify configuration overrides for the DM, trigger, pattern and modulator settings.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock configuration overrides", "[sparkleClock]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::setupConfig();
    sparkleClock::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    sparkleClock_test app( "sparkleclock" );
    app.setupConfigForTest();

    mx::app::writeConfigFile(
        "/tmp/sparkleClock_test_override.conf",
        { "dm", "dm", "dm", "dm", "dm", "dm", "dm", "dm", "dm", "dm", "dm", "modulator", "modulator" },
        { "channelName",
          "triggerChannel",
          "triggerSemaphore",
          "trigger",
          "triggerDelay",
          "angle",
          "angleOffset",
          "amp",
          "cross",
          "frequency",
          "dwell",
          "threadPrio",
          "cpuset" },
        { "dm02disp04", "camsci1", "5", "false", "250", "45", "30", "0.05", "false", "1000", "3", "50", "mod_cpus" } );

    REQUIRE( app.loadConfigFromFile( "/tmp/sparkleClock_test_override.conf" ) == 0 );

    CHECK( app.m_dmChannelName == "dm02disp04" );
    CHECK( app.m_dmName == "dm02disp04" ); // defaults to the channel name
    CHECK( app.m_dmTriggerChannel == "camsci1" );
    CHECK( app.m_triggerSemaphore == 5 );
    CHECK( app.m_trigger == false );
    CHECK( app.m_triggerDelay == Approx( 250 ) );
    CHECK( app.m_angle == Approx( 45 ) );
    CHECK( app.m_angleOffset == Approx( 30 ) );
    CHECK( app.m_amp == Approx( 0.05 ) );
    CHECK( app.m_cross == false );
    CHECK( app.m_frequency == Approx( 1000 ) );
    CHECK( app.m_dwell == 3 );
    CHECK( app.m_modThreadPrio == 50 );
    CHECK( app.m_modThreadCpuset == "mod_cpus" );

    std::remove( "/tmp/sparkleClock_test_override.conf" );
}

/// Verify generateSparkleClock() sizes the shape cube from the interval and frequency.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock generateSparkleClock frame count", "[sparkleClock]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::generateSparkleClock();
    #endif
    // clang-format on

    SECTION( "evenly divisible frame count" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( 8, 8 );
        app.m_sparkleClockInterval = 1.0;
        app.m_frequency            = 16;

        REQUIRE( app.generateSparkleClock() == 0 );
        CHECK( app.m_shapes.rows() == 8 );
        CHECK( app.m_shapes.cols() == 8 );
        CHECK( app.m_shapes.planes() == 16 );
    }

    SECTION( "frame count is rounded down to a multiple of 4" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( 8, 8 );
        app.m_sparkleClockInterval = 1.0;
        app.m_frequency            = 10; // 10 frames, rounded to 8

        REQUIRE( app.generateSparkleClock() == 0 );
        CHECK( app.m_shapes.planes() == 8 );
    }

    SECTION( "longer interval gives more frames" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( 8, 8 );
        app.m_sparkleClockInterval = 2.0;
        app.m_frequency            = 16;

        REQUIRE( app.generateSparkleClock() == 0 );
        CHECK( app.m_shapes.planes() == 32 );
    }

    std::remove( c_specksFile );
}

/// Verify the first sparkle-clock frame is the amplitude-scaled Fourier speckle pattern.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock generateSparkleClock speckle pattern", "[sparkleClock]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::generateSparkleClock();
    #endif
    // clang-format on

    const int D = 8;

    SECTION( "single speckle pair along u at zero angle" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( D, D );
        app.m_sparkleClockInterval = 1.0;
        app.m_frequency            = 8;
        app.m_separation_1         = 2;
        app.m_angle                = 0;
        app.m_angleOffset          = 0;
        app.m_amp                  = 0.1;
        app.m_cross                = false;

        REQUIRE( app.generateSparkleClock() == 0 );

        for( int i = 0; i < D; ++i )
        {
            for( int j = 0; j < D; ++j )
            {
                CHECK( app.m_shapes.image( 0 )( i, j ) == Approx( 0.1 * cosMode( 2, 0, i, j, D ) ).margin( 1e-5 ) );
            }
        }
    }

    SECTION( "cross adds the speckle pair rotated by 90 degrees" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( D, D );
        app.m_sparkleClockInterval = 1.0;
        app.m_frequency            = 8;
        app.m_separation_1         = 2;
        app.m_angle                = 0;
        app.m_angleOffset          = 0;
        app.m_amp                  = 0.1;
        app.m_cross                = true;

        REQUIRE( app.generateSparkleClock() == 0 );

        for( int i = 0; i < D; ++i )
        {
            for( int j = 0; j < D; ++j )
            {
                double expect = 0.1 * ( cosMode( 2, 0, i, j, D ) + cosMode( 0, 2, i, j, D ) );
                CHECK( app.m_shapes.image( 0 )( i, j ) == Approx( expect ).margin( 1e-5 ) );
            }
        }
    }

    SECTION( "angle rotates the speckle clockwise relative to the offset" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( D, D );
        app.m_sparkleClockInterval = 1.0;
        app.m_frequency            = 8;
        app.m_separation_1         = 2;
        app.m_angle                = 90; // -90 deg rotation: m = 0, n = -2
        app.m_angleOffset          = 0;
        app.m_amp                  = 0.1;
        app.m_cross                = false;

        REQUIRE( app.generateSparkleClock() == 0 );

        for( int i = 0; i < D; ++i )
        {
            for( int j = 0; j < D; ++j )
            {
                CHECK( app.m_shapes.image( 0 )( i, j ) == Approx( 0.1 * cosMode( 0, -2, i, j, D ) ).margin( 1e-4 ) );
            }
        }
    }

    SECTION( "angle offset cancels an equal angle" )
    {
        mx::improc::eigenImage<float> ref;

        {
            sparkleClock_test app( "sparkleclock" );
            app.setGeometry( D, D );
            app.m_sparkleClockInterval = 1.0;
            app.m_frequency            = 8;
            app.m_angle                = 0;
            app.m_angleOffset          = 0;
            REQUIRE( app.generateSparkleClock() == 0 );
            ref = app.m_shapes.image( 0 );
        }

        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( D, D );
        app.m_sparkleClockInterval = 1.0;
        app.m_frequency            = 8;
        app.m_angle                = 28;
        app.m_angleOffset          = 28;
        REQUIRE( app.generateSparkleClock() == 0 );

        for( int i = 0; i < D; ++i )
        {
            for( int j = 0; j < D; ++j )
            {
                CHECK( app.m_shapes.image( 0 )( i, j ) == Approx( ref( i, j ) ).margin( 1e-5 ) );
            }
        }
    }

    SECTION( "amplitude scales the pattern linearly" )
    {
        mx::improc::eigenImage<float> ref;

        {
            sparkleClock_test app( "sparkleclock" );
            app.setGeometry( D, D );
            app.m_sparkleClockInterval = 1.0;
            app.m_frequency            = 8;
            app.m_amp                  = 0.01;
            REQUIRE( app.generateSparkleClock() == 0 );
            ref = app.m_shapes.image( 0 );
        }

        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( D, D );
        app.m_sparkleClockInterval = 1.0;
        app.m_frequency            = 8;
        app.m_amp                  = 0.03;
        REQUIRE( app.generateSparkleClock() == 0 );

        for( int i = 0; i < D; ++i )
        {
            for( int j = 0; j < D; ++j )
            {
                CHECK( app.m_shapes.image( 0 )( i, j ) == Approx( 3.0 * ref( i, j ) ).margin( 1e-6 ) );
            }
        }
    }

    std::remove( c_specksFile );
}

/// Verify appLogic() reads the connected DM stream geometry and rejects non-float streams.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock appLogic connected stream checks", "[sparkleClock]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::appLogic();
    #endif
    // clang-format on

    SECTION( "float stream sets the geometry and goes READY" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.m_testMd.datatype = _DATATYPE_FLOAT;
        app.m_testMd.size[0]  = 12;
        app.m_testMd.size[1]  = 10;
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );
        CHECK( app.m_width == 12 );
        CHECK( app.m_height == 10 );
        CHECK( app.m_dataType == _DATATYPE_FLOAT );
        CHECK( app.m_typeSize == sizeof( float ) );

        // no telemetry thread is running, which the telemeter treats as fatal
        CHECK( app.state() == stateCodes::FAILURE );
        CHECK( app.m_shutdown == 1 );
    }

    SECTION( "non-float stream is a critical error" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.m_testMd.datatype = _DATATYPE_UINT16;
        app.m_testMd.size[0]  = 12;
        app.m_testMd.size[1]  = 10;
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == -1 );
        CHECK( app.state() == stateCodes::CONNECTED );
    }
}

/// Verify the NEW callbacks that validate device and name (cross, dwell, single) with the standard checks.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock INDI callback validation", "[sparkleClock][indi]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::newCallBack_m_indiP_cross(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_dwell(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( sparkleClock, cross );
    XWCTEST_INDI_NEW_CALLBACK( sparkleClock, dwell );
    XWCTEST_INDI_NEW_CALLBACK( sparkleClock, single );
}

/// Verify the name-only validation of the remaining NEW callbacks.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock INDI callbacks reject the wrong property name", "[sparkleClock][indi]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::newCallBack_m_indiP_trigger(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_delay(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_separation_1(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_separation_2(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_angle(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_amp(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_frequency(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_interval(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_modulating(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_zero(const pcf::IndiProperty &);
    #endif
    // clang-format on

    sparkleClock_test app( "sparkleclock" );

    CHECK( app.newCallBack_m_indiP_trigger( makeSwitch( "sparkleclock", "wrong", "toggle", true ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_delay( makeTarget( "sparkleclock", "wrong", 1 ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_separation_1( makeTarget( "sparkleclock", "wrong", 1 ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_separation_2( makeTarget( "sparkleclock", "wrong", 1 ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_angle( makeTarget( "sparkleclock", "wrong", 1 ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_amp( makeTarget( "sparkleclock", "wrong", 1 ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_frequency( makeTarget( "sparkleclock", "wrong", 1 ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_interval( makeTarget( "sparkleclock", "wrong", 1 ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_modulating( makeSwitch( "sparkleclock", "wrong", "toggle", true ) ) == -1 );
    CHECK( app.newCallBack_m_indiP_zero( makeSwitch( "sparkleclock", "wrong", "request", true ) ) == -1 );

    CHECK( app.m_restartSp == false );
    CHECK( app.m_trigger == true );
    CHECK( app.m_modulating == false );
}

/// Verify the float pattern-parameter NEW callbacks (delay, separations, angle, amp, frequency, interval).
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock pattern parameter callbacks", "[sparkleClock][indi]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::newCallBack_m_indiP_delay(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_separation_1(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_separation_2(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_angle(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_amp(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_frequency(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_interval(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "delay: current, target precedence and missing value" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_delay( makeNumberProperty( "sparkleclock", "delay", false, 0, false, 0 ) ) ==
                 0 );
        CHECK( app.m_triggerDelay == 0 );
        CHECK( app.m_restartSp == false );

        REQUIRE( app.newCallBack_m_indiP_delay( makeNumberProperty( "sparkleclock", "delay", true, 100, false, 0 ) ) ==
                 0 );
        CHECK( app.m_triggerDelay == Approx( 100 ) );
        CHECK( app.m_restartSp == true );

        REQUIRE( app.newCallBack_m_indiP_delay( makeNumberProperty( "sparkleclock", "delay", true, 100, true, 375 ) ) ==
                 0 );
        CHECK( app.m_triggerDelay == Approx( 375 ) );
    }

    SECTION( "separations" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_separation_1( makeTarget( "sparkleclock", "separation_1", 12.5 ) ) == 0 );
        CHECK( app.m_separation_1 == Approx( 12.5 ) );
        CHECK( app.m_separation_2 == Approx( 20.0 ) );
        CHECK( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_separation_2( makeTarget( "sparkleclock", "separation_2", 18 ) ) == 0 );
        CHECK( app.m_separation_2 == Approx( 18 ) );
        CHECK( app.m_separation_1 == Approx( 12.5 ) );
        CHECK( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_separation_1(
                     makeNumberProperty( "sparkleclock", "separation_1", false, 0, false, 0 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_separation_2(
                     makeNumberProperty( "sparkleclock", "separation_2", false, 0, false, 0 ) ) == 0 );
        CHECK( app.m_separation_1 == Approx( 12.5 ) );
        CHECK( app.m_separation_2 == Approx( 18 ) );
        CHECK( app.m_restartSp == false );
    }

    SECTION( "angle and amplitude, including negative values" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_angle( makeTarget( "sparkleclock", "angle", -30 ) ) == 0 );
        CHECK( app.m_angle == Approx( -30 ) );
        CHECK( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_amp( makeTarget( "sparkleclock", "amp", -0.02 ) ) == 0 );
        CHECK( app.m_amp == Approx( -0.02 ) );
        CHECK( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_angle( makeNumberProperty( "sparkleclock", "angle", false, 0, false, 0 ) ) ==
                 0 );
        REQUIRE( app.newCallBack_m_indiP_amp( makeNumberProperty( "sparkleclock", "amp", false, 0, false, 0 ) ) == 0 );
        CHECK( app.m_angle == Approx( -30 ) );
        CHECK( app.m_amp == Approx( -0.02 ) );
        CHECK( app.m_restartSp == false );
    }

    SECTION( "frequency rejects negative and missing values" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_frequency( makeTarget( "sparkleclock", "frequency", 1500 ) ) == 0 );
        CHECK( app.m_frequency == Approx( 1500 ) );
        CHECK( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_frequency( makeTarget( "sparkleclock", "frequency", -5 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_frequency(
                     makeNumberProperty( "sparkleclock", "frequency", false, 0, false, 0 ) ) == 0 );
        CHECK( app.m_frequency == Approx( 1500 ) );
        CHECK( app.m_restartSp == false );
    }

    SECTION( "interval sets the sparkle clock interval and rejects negative values" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_interval( makeTarget( "sparkleclock", "interval", 0.25 ) ) == 0 );
        CHECK( app.m_sparkleClockInterval == Approx( 0.25 ) );
        CHECK( app.m_interval == Approx( 1.0 ) ); // separate, unused-by-callback member
        CHECK( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_interval( makeTarget( "sparkleclock", "interval", -1 ) ) == 0 );
        CHECK( app.m_sparkleClockInterval == Approx( 0.25 ) );
        CHECK( app.m_restartSp == false );
    }
}

/// Verify the integer dwell and single NEW callbacks, including range checks.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock dwell and single callbacks", "[sparkleClock][indi]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::newCallBack_m_indiP_dwell(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "dwell" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_dwell( makeTarget( "sparkleclock", "dwell", 4 ) ) == 0 );
        CHECK( app.m_dwell == 4 );
        CHECK( app.m_restartSp == true );

        app.m_restartSp = false;
        REQUIRE( app.newCallBack_m_indiP_dwell( makeTarget( "sparkleclock", "dwell", 0 ) ) == 0 );
        CHECK( app.m_dwell == 4 );
        CHECK( app.m_restartSp == false );
    }

    SECTION( "single accepts -1 to 3 and does not restart" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_single( makeTarget( "sparkleclock", "single", 2 ) ) == 0 );
        CHECK( app.m_single == 2 );

        REQUIRE( app.newCallBack_m_indiP_single( makeTarget( "sparkleclock", "single", -1 ) ) == 0 );
        CHECK( app.m_single == -1 );

        REQUIRE( app.newCallBack_m_indiP_single( makeTarget( "sparkleclock", "single", 4 ) ) == 0 );
        CHECK( app.m_single == -1 );

        REQUIRE( app.newCallBack_m_indiP_single( makeTarget( "sparkleclock", "single", -2 ) ) == 0 );
        CHECK( app.m_single == -1 );

        CHECK( app.m_restartSp == false );
    }
}

/// Verify the trigger, cross and modulating toggle NEW callbacks.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock toggle callbacks", "[sparkleClock][indi]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::newCallBack_m_indiP_trigger(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_cross(const pcf::IndiProperty &);
    sparkleClock::newCallBack_m_indiP_modulating(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "trigger" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_trigger( makeSwitch( "sparkleclock", "trigger", "", false ) ) == 0 );
        CHECK( app.m_trigger == true );
        CHECK( app.m_restartSp == false );

        REQUIRE( app.newCallBack_m_indiP_trigger( makeSwitch( "sparkleclock", "trigger", "toggle", false ) ) == 0 );
        CHECK( app.m_trigger == false );
        CHECK( app.m_restartSp == true );

        REQUIRE( app.newCallBack_m_indiP_trigger( makeSwitch( "sparkleclock", "trigger", "toggle", true ) ) == 0 );
        CHECK( app.m_trigger == true );
    }

    SECTION( "cross" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_cross( makeSwitch( "sparkleclock", "cross", "toggle", false ) ) == 0 );
        CHECK( app.m_cross == false );
        CHECK( app.m_restartSp == true );

        REQUIRE( app.newCallBack_m_indiP_cross( makeSwitch( "sparkleclock", "cross", "toggle", true ) ) == 0 );
        CHECK( app.m_cross == true );
    }

    SECTION( "modulating does not request a pattern restart" )
    {
        sparkleClock_test app( "sparkleclock" );

        REQUIRE( app.newCallBack_m_indiP_modulating( makeSwitch( "sparkleclock", "modulating", "", true ) ) == 0 );
        CHECK( app.m_modulating == false );

        REQUIRE( app.newCallBack_m_indiP_modulating( makeSwitch( "sparkleclock", "modulating", "toggle", true ) ) ==
                 0 );
        CHECK( app.m_modulating == true );

        REQUIRE( app.newCallBack_m_indiP_modulating( makeSwitch( "sparkleclock", "modulating", "toggle", false ) ) ==
                 0 );
        CHECK( app.m_modulating == false );
        CHECK( app.m_restartSp == false );
    }
}

/// Verify the zero request writes zeros to the DM channel, unless modulating.
/**
 * \ingroup sparkleClock_unit_test
 */
TEST_CASE( "sparkleClock zero callback", "[sparkleClock][indi]" )
{
    // clang-format off
    #ifdef SPARKLECLOCK_TEST_DOXYGEN_REF
    sparkleClock::newCallBack_m_indiP_zero(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "request zeroes the stream and bumps the counter" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( 2, 2 );
        app.setStreamData( { 1.0f, -2.0f, 3.0f, 4.0f } );
        app.m_testMd.cnt0 = 7;

        REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "sparkleclock", "zero", "request", true ) ) == 0 );
        for( float v : app.m_testData )
        {
            CHECK( v == 0.0f );
        }
        CHECK( app.m_testMd.cnt0 == 8 );
        CHECK( app.m_testMd.write == 0 );
        CHECK( app.m_testMd.writetime.tv_sec > 0 );
    }

    SECTION( "no request element or request off does nothing" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( 2, 2 );
        app.setStreamData( std::vector<float>( 4, 5.0f ) );

        REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "sparkleclock", "zero", "", true ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "sparkleclock", "zero", "request", false ) ) == 0 );
        CHECK( app.m_testData[0] == 5.0f );
        CHECK( app.m_testMd.cnt0 == 0 );
    }

    SECTION( "zero is refused while modulating" )
    {
        sparkleClock_test app( "sparkleclock" );
        app.setGeometry( 2, 2 );
        app.setStreamData( std::vector<float>( 4, 5.0f ) );
        app.m_modulating = true;

        REQUIRE( app.newCallBack_m_indiP_zero( makeSwitch( "sparkleclock", "zero", "request", true ) ) == 0 );
        CHECK( app.m_testData[3] == 5.0f );
        CHECK( app.m_testMd.cnt0 == 0 );
    }
}

} // namespace sparkleClockTest

} // namespace libXWCTest
