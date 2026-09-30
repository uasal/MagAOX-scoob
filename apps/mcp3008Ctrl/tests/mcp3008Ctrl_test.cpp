/** \file mcp3008Ctrl_test.cpp
 * \brief Catch2 tests for the mcp3008Ctrl app.
 * \author Claude Code
 *
 * \ingroup mcp3008Ctrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

#define protected public
#include "../mcp3008Ctrl.hpp"
#undef protected

// Compile the real MCP3008 driver into this translation unit.  Its `#include <lgpio.h>` resolves to
// `stubs/lgpio.h`, whose functions are defined below.
#include "../dependencies/MCP3008.cpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Fake lgpio SPI state driven by the mcp3008Ctrl unit tests.
struct lgpioStubState
{
    int m_openReturn{ 7 };       ///< Value returned by lgSpiOpen (the handle, or a negative error).
    int m_closeReturn{ 0 };      ///< Value returned by lgSpiClose.
    int m_xferReturn{ -1 };      ///< Override for the lgSpiXfer return value; -1 (the default) returns `count`.
    int m_openCalls{ 0 };        ///< Number of lgSpiOpen calls.
    int m_closeCalls{ 0 };       ///< Number of lgSpiClose calls.
    int m_xferCalls{ 0 };        ///< Number of lgSpiXfer calls.
    int m_openDev{ -1 };         ///< Device number passed to the last lgSpiOpen.
    int m_openChan{ -1 };        ///< Chip-select channel passed to the last lgSpiOpen.
    int m_openBaud{ -1 };        ///< Baud rate passed to the last lgSpiOpen.
    int m_openFlags{ -1 };       ///< Flags passed to the last lgSpiOpen.
    int m_lastCloseHandle{ -1 }; ///< Handle passed to the last lgSpiClose.
    int m_lastXferHandle{ -2 };  ///< Handle passed to the last lgSpiXfer.
    int m_lastXferCount{ 0 };    ///< Byte count passed to the last lgSpiXfer.

    std::vector<std::uint8_t> m_lastTx; ///< Bytes transmitted by the last lgSpiXfer.

    std::vector<int> m_readOrder; ///< Channels decoded from each transmitted control byte, in order.

    std::vector<unsigned short> m_channelValues; ///< 10-bit values returned per channel (missing channels read 0).
};

/// Access the shared lgpio stub state.
lgpioStubState &stubState()
{
    static lgpioStubState state;
    return state;
}

/// Reset the shared lgpio stub state to its defaults.
void resetStubState()
{
    stubState() = lgpioStubState();
}

int lgSpiOpen( int spiDev, int spiChan, int spiBaud, int spiFlags )
{
    ++stubState().m_openCalls;
    stubState().m_openDev   = spiDev;
    stubState().m_openChan  = spiChan;
    stubState().m_openBaud  = spiBaud;
    stubState().m_openFlags = spiFlags;
    return stubState().m_openReturn;
}

int lgSpiClose( int handle )
{
    ++stubState().m_closeCalls;
    stubState().m_lastCloseHandle = handle;
    return stubState().m_closeReturn;
}

int lgSpiXfer( int handle, const char *txBuf, char *rxBuf, int count )
{
    ++stubState().m_xferCalls;
    stubState().m_lastXferHandle = handle;
    stubState().m_lastXferCount  = count;

    stubState().m_lastTx.assign( reinterpret_cast<const std::uint8_t *>( txBuf ),
                                 reinterpret_cast<const std::uint8_t *>( txBuf ) + count );

    int channel = 0;
    if( count >= 2 )
    {
        channel = ( static_cast<std::uint8_t>( txBuf[1] ) >> 4 ) & 0x07;
    }
    stubState().m_readOrder.push_back( channel );

    unsigned short value = 0;
    if( static_cast<size_t>( channel ) < stubState().m_channelValues.size() )
    {
        value = stubState().m_channelValues[static_cast<size_t>( channel )];
    }

    // Fill the ignored bits with ones so that the driver's masking is exercised.
    if( count >= 3 )
    {
        rxBuf[0] = static_cast<char>( 0xFF );
        rxBuf[1] = static_cast<char>( 0xFC | ( ( value >> 8 ) & 0x03 ) );
        rxBuf[2] = static_cast<char>( value & 0xFF );
    }

    if( stubState().m_xferReturn != -1 )
    {
        return stubState().m_xferReturn;
    }

    return count;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup mcp3008Ctrl_unit_test mcp3008Ctrl Unit Tests
 * \brief Unit tests for the mcp3008Ctrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `mcp3008Ctrl` unit tests.
/** \ingroup mcp3008Ctrl_unit_test
 */
namespace mcp3008CtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing mcp3008Ctrl INDI setup helpers.
class mcp3008Ctrl_test : public mcp3008Ctrl
{
  public:
    /// Construct a harness with the given device name.
    explicit mcp3008Ctrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    /// Initialize the local fps INDI property as appStartup would.
    void setupFpsProperty()
    {
        m_indiP_fps = pcf::IndiProperty( pcf::IndiProperty::Number );
        m_indiP_fps.setDevice( m_configName );
        m_indiP_fps.setName( "fps" );
        m_indiP_fps.add( pcf::IndiElement( "current" ) );
        m_indiP_fps["current"].setValue( m_fps );
        m_indiP_fps.add( pcf::IndiElement( "target" ) );
        m_indiP_fps["target"].setValue( m_fps );
    }

    /// Initialize the external fps-source INDI property as appStartup would.
    void setupFpsSourceProperty( const std::string &device /**< [in] the fps source device */,
                                 const std::string &property /**< [in] the fps source property */,
                                 const std::string &element /**< [in] the fps source element */ )
    {
        m_fpsDevice   = device;
        m_fpsProperty = property;
        m_fpsElement  = element;

        m_indiP_fpsSource = pcf::IndiProperty( pcf::IndiProperty::Number );
        m_indiP_fpsSource.setDevice( m_fpsDevice );
        m_indiP_fpsSource.setName( m_fpsProperty );
    }
};

/// Build a number property with optional `current` and `target` elements.
pcf::IndiProperty makeNumberProp( const std::string &device /**< [in] property device */,
                                  const std::string &name /**< [in] property name */,
                                  bool               addCurrent /**< [in] whether to add a `current` element */,
                                  double             current /**< [in] value of the `current` element */,
                                  bool               addTarget /**< [in] whether to add a `target` element */,
                                  double             target /**< [in] value of the `target` element */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );

    if( addCurrent )
    {
        ip.add( pcf::IndiElement( "current" ) );
        ip["current"].setValue( current );
    }

    if( addTarget )
    {
        ip.add( pcf::IndiElement( "target" ) );
        ip["target"].setValue( target );
    }

    return ip;
}
/// \endcond

/// Verify construction defaults for the acquisition timing members.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl construction defaults", "[mcp3008Ctrl]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::mcp3008Ctrl();
    mcp3008Ctrl::fps();
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );

    REQUIRE( app.m_numChannels == 8 );
    REQUIRE( app.m_fps == Approx( 2000.0f ) );
    REQUIRE( app.fps() == Approx( 2000.0f ) );
    REQUIRE( app.m_trigger == Approx( 1e9f / 2000.0f ) );
    REQUIRE( app.nano_sec_target == Approx( 1e9f / 2000.0f ) );
    REQUIRE( app.m_gain == Approx( 0.1f ) );
    REQUIRE( app.m_values.empty() );
}

/// Verify configuration defaults when no options are supplied.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl configuration defaults", "[mcp3008Ctrl][config]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::setupConfig();
    mcp3008Ctrl::loadConfigImpl( config );
    mcp3008Ctrl::loadConfig();
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );

    app.setupConfig();

    const char *fname = "/tmp/mcp3008Ctrl_test_defaults.conf";
    mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );
    app.config.readConfig( fname );

    REQUIRE( app.loadConfigImpl( app.config ) == 0 );

    REQUIRE( app.m_numChannels == 8 );
    REQUIRE( app.m_fpsDevice == "" );
    REQUIRE( app.m_fpsProperty == "fps" );
    REQUIRE( app.m_fpsElement == "current" );
    REQUIRE( app.m_fpsTol == Approx( 0.0f ) );

    // frameGrabber defaults: the shmim name follows the configuration name.
    REQUIRE( app.m_shmimName == "mcp3008Ctrl_test" );
    REQUIRE( app.m_circBuffLength == 1 );

    // telemeter default
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );

    ::remove( fname );
}

/// Verify configuration overrides for the app, frameGrabber and telemeter options.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl configuration overrides", "[mcp3008Ctrl][config]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::setupConfig();
    mcp3008Ctrl::loadConfig();
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );

    app.setupConfig();

    const char *fname = "/tmp/mcp3008Ctrl_test_overrides.conf";
    mx::app::writeConfigFile(
        fname,
        { "fps", "fps", "fps", "fps", "accel", "framegrabber", "framegrabber", "telemeter" },
        { "device", "property", "element", "tol", "numChannels", "shmimName", "circBuffLength", "maxInterval" },
        { "camwfs", "framerate", "target", "0.5", "3", "accelStream", "50", "2.5" } );
    app.config.readConfig( fname );

    app.loadConfig();

    REQUIRE( app.m_fpsDevice == "camwfs" );
    REQUIRE( app.m_fpsProperty == "framerate" );
    REQUIRE( app.m_fpsElement == "target" );
    REQUIRE( app.m_fpsTol == Approx( 0.5f ) );
    REQUIRE( app.m_numChannels == 3 );
    REQUIRE( app.m_shmimName == "accelStream" );
    REQUIRE( app.m_circBuffLength == 50 );
    REQUIRE( app.m_maxInterval == Approx( 2.5 ) );

    ::remove( fname );
}

/// Verify the MCP3008 driver opens and closes the SPI device through lgpio.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "MCP3008 driver connect and disconnect", "[mcp3008Ctrl][MCP3008]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    MCP3008Lib::MCP3008::MCP3008();
    MCP3008Lib::MCP3008::connect();
    MCP3008Lib::MCP3008::disconnect();
    #endif
    // clang-format on

    resetStubState();

    SECTION( "defaults are passed to lgSpiOpen and connect is idempotent" )
    {
        MCP3008Lib::MCP3008 adc;

        adc.connect();
        REQUIRE( stubState().m_openCalls == 1 );
        REQUIRE( stubState().m_openDev == MCP3008Lib::MCP3008::DEFAULT_SPI_DEV );
        REQUIRE( stubState().m_openChan == MCP3008Lib::MCP3008::DEFAULT_SPI_CHANNEL );
        REQUIRE( stubState().m_openBaud == MCP3008Lib::MCP3008::DEFAULT_SPI_BAUD );
        REQUIRE( stubState().m_openBaud == 1350000 );
        REQUIRE( stubState().m_openFlags == MCP3008Lib::MCP3008::DEFAULT_SPI_FLAGS );

        adc.connect();
        REQUIRE( stubState().m_openCalls == 1 );

        adc.disconnect();
        REQUIRE( stubState().m_closeCalls == 1 );
        REQUIRE( stubState().m_lastCloseHandle == 7 );

        // Already disconnected, so no second close
        adc.disconnect();
        REQUIRE( stubState().m_closeCalls == 1 );
    }

    SECTION( "custom constructor arguments are forwarded" )
    {
        MCP3008Lib::MCP3008 adc( 1, 2, MCP3008Lib::MCP3008::SPI_5V_BAUD, 3 );

        adc.connect();
        REQUIRE( stubState().m_openDev == 1 );
        REQUIRE( stubState().m_openChan == 2 );
        REQUIRE( stubState().m_openBaud == 3600000 );
        REQUIRE( stubState().m_openFlags == 3 );
    }

    SECTION( "a failed open throws and leaves the driver disconnected" )
    {
        stubState().m_openReturn = -5;

        MCP3008Lib::MCP3008 adc;
        REQUIRE_THROWS_AS( adc.connect(), std::runtime_error );

        adc.disconnect();
        REQUIRE( stubState().m_closeCalls == 0 );
    }

    SECTION( "a failed close throws" )
    {
        MCP3008Lib::MCP3008 adc;
        adc.connect();

        stubState().m_closeReturn = -1;
        REQUIRE_THROWS_AS( adc.disconnect(), std::runtime_error );

        // The destructor retries the close but swallows the exception.
        stubState().m_closeReturn = 0;
    }

    SECTION( "the destructor closes an open handle" )
    {
        {
            MCP3008Lib::MCP3008 adc;
            adc.connect();
        }
        REQUIRE( stubState().m_closeCalls == 1 );
        REQUIRE( stubState().m_lastCloseHandle == 7 );
    }
}

/// Verify the MCP3008 read command encoding and response decoding.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "MCP3008 driver read encodes the command and decodes 10 bits", "[mcp3008Ctrl][MCP3008]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    MCP3008Lib::MCP3008::read( 0, MCP3008Lib::Mode::SINGLE );
    #endif
    // clang-format on

    resetStubState();
    stubState().m_channelValues = { 0, 1, 255, 256, 511, 768, 1023, 0x2AA };

    MCP3008Lib::MCP3008 adc;
    adc.connect();

    SECTION( "single-ended reads on each channel" )
    {
        for( int ch = 0; ch < 8; ++ch )
        {
            unsigned short v = adc.read( static_cast<std::uint8_t>( ch ) );

            REQUIRE( v == stubState().m_channelValues[ch] );
            REQUIRE( stubState().m_lastXferHandle == 7 );
            REQUIRE( stubState().m_lastXferCount == 3 );
            REQUIRE( stubState().m_lastTx.size() == 3 );
            REQUIRE( stubState().m_lastTx[0] == 0x01 );
            REQUIRE( stubState().m_lastTx[1] == static_cast<std::uint8_t>( 0x80 | ( ch << 4 ) ) );
            REQUIRE( stubState().m_lastTx[2] == 0x00 );
        }
    }

    SECTION( "differential mode clears the SGL/DIFF bit" )
    {
        unsigned short v = adc.read( 5, MCP3008Lib::Mode::DIFFERENTIAL );
        REQUIRE( v == 768 );
        REQUIRE( stubState().m_lastTx[1] == 0x50 );
    }

    SECTION( "channel numbers are masked to three bits" )
    {
        unsigned short v = adc.read( 9 );
        REQUIRE( stubState().m_lastTx[1] == 0x90 );
        REQUIRE( v == stubState().m_channelValues[1] );
    }

    SECTION( "a short transfer throws" )
    {
        stubState().m_xferReturn = 2;
        REQUIRE_THROWS_AS( adc.read( 0 ), std::runtime_error );
    }
}

/// Verify configureAcquisition sizes the frame to the channel count.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl configureAcquisition sets the frame geometry", "[mcp3008Ctrl][framegrabber]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::configureAcquisition();
    mcp3008Ctrl::reconfig();
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );

    SECTION( "default eight channels" )
    {
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_values.size() == 8 );
        REQUIRE( app.m_width == 8 );
        REQUIRE( app.m_height == 1 );
        REQUIRE( app.m_dataType == _DATATYPE_UINT16 );
    }

    SECTION( "configured channel count" )
    {
        app.m_numChannels = 3;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_values.size() == 3 );
        REQUIRE( app.m_width == 3 );
        REQUIRE( app.m_height == 1 );
    }

    REQUIRE( app.reconfig() == 0 );
}

/// Verify startAcquisition records the acquisition start time.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl startAcquisition resets the start time", "[mcp3008Ctrl][framegrabber]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::startAcquisition();
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );

    auto before = std::chrono::high_resolution_clock::now();
    REQUIRE( app.startAcquisition() == 0 );
    auto after = std::chrono::high_resolution_clock::now();

    REQUIRE( app.m_time_start >= before );
    REQUIRE( app.m_time_start <= after );
}

/// Verify acquireAndCheckValid reads every channel and adjusts the trigger.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl acquireAndCheckValid reads all channels", "[mcp3008Ctrl][framegrabber]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::acquireAndCheckValid();
    #endif
    // clang-format on

    resetStubState();
    stubState().m_channelValues = { 100, 200, 1023, 512 };

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );
    app.m_numChannels = 4;
    REQUIRE( app.configureAcquisition() == 0 );

    SECTION( "an elapsed trigger interval reads the ADC and updates the trigger" )
    {
        // Start one second in the past so the trigger has already elapsed.
        auto start       = std::chrono::high_resolution_clock::now() - std::chrono::seconds( 1 );
        app.m_time_start = start;
        app.m_trigger    = 1e6f;

        REQUIRE( app.acquireAndCheckValid() == 0 );

        REQUIRE( stubState().m_xferCalls == 4 );
        REQUIRE( stubState().m_readOrder == std::vector<int>( { 0, 1, 2, 3 } ) );
        REQUIRE( app.m_values == std::vector<uint16_t>( { 100, 200, 1023, 512 } ) );

        // The start time was reset to the read time.
        REQUIRE( app.m_time_start > start );

        // m_trigger = 1e6 - 0.1*(elapsed - 5e5) with elapsed >= 1 s, and far less than a minute.
        REQUIRE( app.m_trigger < 1e6f - 0.1f * ( 1e9f - 5e5f ) + 1000.0f );
        REQUIRE( app.m_trigger > 1e6f - 0.1f * ( 60e9f - 5e5f ) );
    }

    SECTION( "shutdown returns without reading" )
    {
        app.m_shutdown   = 1;
        app.m_time_start = std::chrono::high_resolution_clock::now() - std::chrono::seconds( 1 );

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( stubState().m_xferCalls == 0 );
        REQUIRE( app.m_values == std::vector<uint16_t>( { 0, 0, 0, 0 } ) );
    }

    SECTION( "a pending reconfig returns without reading" )
    {
        app.m_reconfig   = true;
        app.m_time_start = std::chrono::high_resolution_clock::now() - std::chrono::seconds( 1 );

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( stubState().m_xferCalls == 0 );
    }

    SECTION( "an SPI transfer failure propagates as an exception" )
    {
        stubState().m_xferReturn = 0;
        app.m_time_start         = std::chrono::high_resolution_clock::now() - std::chrono::seconds( 1 );

        REQUIRE_THROWS_AS( app.acquireAndCheckValid(), std::runtime_error );
    }
}

/// Verify loadImageIntoStream copies the channel values into the destination buffer.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl loadImageIntoStream copies channel values", "[mcp3008Ctrl][framegrabber]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::loadImageIntoStream( nullptr );
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );
    app.m_numChannels = 3;
    REQUIRE( app.configureAcquisition() == 0 );

    app.m_values = { 1, 1023, 42 };

    std::vector<uint16_t> dest( 4, 0xFFFF );
    REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

    REQUIRE( dest[0] == 1 );
    REQUIRE( dest[1] == 1023 );
    REQUIRE( dest[2] == 42 );
    REQUIRE( dest[3] == 0xFFFF ); // nothing written past the channel count
}

/// Verify the fps new-property callback updates the target fps and trigger timing.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl fps callback", "[mcp3008Ctrl][indi]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::newCallBack_m_indiP_fps( pcf::IndiProperty() );
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );
    app.setupFpsProperty();

    SECTION( "a target request updates fps, trigger and target time" )
    {
        REQUIRE( app.newCallBack_m_indiP_fps( makeNumberProp( "mcp3008Ctrl_test", "fps", true, 0, true, 500 ) ) == 0 );
        REQUIRE( app.m_fps == Approx( 500.0f ) );
        REQUIRE( app.fps() == Approx( 500.0f ) );
        REQUIRE( app.m_trigger == Approx( 2e6f ) );
        REQUIRE( app.nano_sec_target == Approx( 2e6f ) );
    }

    SECTION( "a current-only request is used as the target" )
    {
        REQUIRE( app.newCallBack_m_indiP_fps( makeNumberProp( "mcp3008Ctrl_test", "fps", true, 250, false, 0 ) ) == 0 );
        REQUIRE( app.m_fps == Approx( 250.0f ) );
        REQUIRE( app.m_trigger == Approx( 4e6f ) );
        REQUIRE( app.nano_sec_target == Approx( 4e6f ) );
    }

    SECTION( "wrong property name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_fps( makeNumberProp( "mcp3008Ctrl_test", "wrong", true, 0, true, 500 ) ) ==
                 -1 );
        REQUIRE( app.m_fps == Approx( 2000.0f ) );
    }

    SECTION( "wrong device is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_fps( makeNumberProp( "wrong", "fps", true, 0, true, 500 ) ) == -1 );
        REQUIRE( app.m_fps == Approx( 2000.0f ) );
    }

    SECTION( "a property without current or target is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_fps( makeNumberProp( "mcp3008Ctrl_test", "fps", false, 0, false, 0 ) ) == -1 );
        REQUIRE( app.m_fps == Approx( 2000.0f ) );
        REQUIRE( app.m_trigger == Approx( 5e5f ) );
    }
}

/// Verify the fps-source set-property callback tracks an external fps.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl fps source callback", "[mcp3008Ctrl][indi]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::setCallBack_m_indiP_fpsSource( pcf::IndiProperty() );
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );

    SECTION( "the default current element sets fps" )
    {
        app.setupFpsSourceProperty( "camwfs", "fps", "current" );

        REQUIRE( app.setCallBack_m_indiP_fpsSource( makeNumberProp( "camwfs", "fps", true, 1000, false, 0 ) ) == 0 );
        REQUIRE( app.m_fps == Approx( 1000.0f ) );
        REQUIRE( app.m_trigger == Approx( 1e6f ) );
        REQUIRE( app.nano_sec_target == Approx( 1e6f ) );
    }

    SECTION( "a configured element is used" )
    {
        app.setupFpsSourceProperty( "camwfs", "framerate", "target" );

        REQUIRE( app.setCallBack_m_indiP_fpsSource( makeNumberProp( "camwfs", "framerate", true, 100, true, 400 ) ) ==
                 0 );
        REQUIRE( app.m_fps == Approx( 400.0f ) );
        REQUIRE( app.m_trigger == Approx( 2.5e6f ) );
    }

    SECTION( "a missing element is ignored" )
    {
        app.setupFpsSourceProperty( "camwfs", "fps", "current" );

        REQUIRE( app.setCallBack_m_indiP_fpsSource( makeNumberProp( "camwfs", "fps", false, 0, true, 400 ) ) == 0 );
        REQUIRE( app.m_fps == Approx( 2000.0f ) );
        REQUIRE( app.m_trigger == Approx( 5e5f ) );
    }

    SECTION( "wrong device or name is rejected" )
    {
        app.setupFpsSourceProperty( "camwfs", "fps", "current" );

        REQUIRE( app.setCallBack_m_indiP_fpsSource( makeNumberProp( "wrong", "fps", true, 1000, false, 0 ) ) == -1 );
        REQUIRE( app.setCallBack_m_indiP_fpsSource( makeNumberProp( "camwfs", "wrong", true, 1000, false, 0 ) ) == -1 );
        REQUIRE( app.m_fps == Approx( 2000.0f ) );
    }
}

/// Verify the telemetry hooks record frame-grabber timings when due.
/**
 * \ingroup mcp3008Ctrl_unit_test
 */
TEST_CASE( "mcp3008Ctrl telemetry records fgtimings", "[mcp3008Ctrl][telem]" )
{
    // clang-format off
    #ifdef MCP3008CTRL_TEST_DOXYGEN_REF
    mcp3008Ctrl::checkRecordTimes();
    mcp3008Ctrl::recordTelem( nullptr );
    #endif
    // clang-format on

    mcp3008Ctrl_test app( "mcp3008Ctrl_test" );

    SECTION( "recordTelem always records" )
    {
        MagAOX::logger::telem_fgtimings::lastRecord = timespec{ 0, 0 };

        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_fgtimings *>( nullptr ) ) == 0 );
        REQUIRE( MagAOX::logger::telem_fgtimings::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records a stale entry" )
    {
        MagAOX::logger::telem_fgtimings::lastRecord = timespec{ 0, 0 };

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_fgtimings::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes skips a recent entry" )
    {
        timespec now;
        clock_gettime( CLOCK_REALTIME, &now );
        MagAOX::logger::telem_fgtimings::lastRecord = now;

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_fgtimings::lastRecord.tv_sec == now.tv_sec );
        REQUIRE( MagAOX::logger::telem_fgtimings::lastRecord.tv_nsec == now.tv_nsec );
    }
}

} // namespace mcp3008CtrlTest

} // namespace libXWCTest
