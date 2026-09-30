/** \file trippLitePDU_test.cpp
 * \brief Catch2 tests for the trippLitePDU app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup trippLitePDU_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

// Build the app against its simulator (trippLitePDU_simulator.hpp) instead of a telnet connection.
#define XWC_SIM_MODE

#include "../trippLitePDU.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup trippLitePDU_unit_test trippLitePDU Unit Tests
 * \brief Unit tests for the trippLitePDU application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `trippLitePDU` unit tests.
/** \ingroup trippLitePDU_unit_test
 */
namespace trippLitePDUTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing trippLitePDU internals.
class trippLitePDU_test : public trippLitePDU
{
  public:
    /// Construct a harness with the given device name.
    explicit trippLitePDU_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
    }

    /// The device address.
    using trippLitePDU::m_deviceAddr;

    /// The device port.
    using trippLitePDU::m_devicePort;

    /// The login username.
    using trippLitePDU::m_deviceUsername;

    /// The login password file.
    using trippLitePDU::m_devicePassFile;

    /// The power alert interface version.
    using trippLitePDU::m_deviceVersion;

    /// Low-frequency warning threshold.
    using trippLitePDU::m_freqLowWarn;

    /// High-frequency warning threshold.
    using trippLitePDU::m_freqHighWarn;

    /// Low-frequency alert threshold.
    using trippLitePDU::m_freqLowAlert;

    /// High-frequency alert threshold.
    using trippLitePDU::m_freqHighAlert;

    /// Low-frequency emergency threshold.
    using trippLitePDU::m_freqLowEmerg;

    /// High-frequency emergency threshold.
    using trippLitePDU::m_freqHighEmerg;

    /// Low-voltage warning threshold.
    using trippLitePDU::m_voltLowWarn;

    /// High-voltage warning threshold.
    using trippLitePDU::m_voltHighWarn;

    /// Low-voltage alert threshold.
    using trippLitePDU::m_voltLowAlert;

    /// High-voltage alert threshold.
    using trippLitePDU::m_voltHighAlert;

    /// Low-voltage emergency threshold.
    using trippLitePDU::m_voltLowEmerg;

    /// High-voltage emergency threshold.
    using trippLitePDU::m_voltHighEmerg;

    /// High-current warning threshold.
    using trippLitePDU::m_currWarn;

    /// High-current alert threshold.
    using trippLitePDU::m_currAlert;

    /// High-current emergency threshold.
    using trippLitePDU::m_currEmerg;

    /// The device status string.
    using trippLitePDU::m_status;

    /// The parsed line frequency.
    using trippLitePDU::m_frequency;

    /// The parsed line voltage.
    using trippLitePDU::m_voltage;

    /// The parsed load current.
    using trippLitePDU::m_current;

    /// The status INDI property.
    using trippLitePDU::m_indiP_status;

    /// The load INDI property.
    using trippLitePDU::m_indiP_load;

    /// The registered new-property callbacks.
    using trippLitePDU::m_indiNewCallBacks;

    /// The main loop pause.
    using trippLitePDU::m_loopPause;
};

/// \endcond

/// Build a devstatus response in the format of the PDU (and the simulator).
/** The first line is the echoed command, which the parser skips.
 *
 * \returns the response string
 */
std::string statusString( const std::string &outlets, /**< [in] the "Outlets On" value, e.g. "1 3" or "NONE" */
                          const std::string &voltage   = "120.0", /**< [in] [optional] the input voltage */
                          const std::string &frequency = "60.0",  /**< [in] [optional] the input frequency */
                          const std::string &current   = "4.00",  /**< [in] [optional] the output current */
                          const std::string &eol       = "\n" /**< [in] [optional] the line ending */ )
{
    std::string s = "devstatus" + eol;
    s += "-------------------------------------------------------------------------------" + eol;
    s += "01: PDUMH20NET2LX 'Device0062'" + eol;
    s += "--------------------------------------------------------------------------------" + eol;
    s += "Device Type:                    PDU" + eol;
    s += "Device Status:                  WARNING        !" + eol;
    s += eol;
    s += "Input Voltage:                  " + voltage + " V    " + eol;
    s += "Input Frequency:                " + frequency + " Hz       " + eol;
    s += "Low Transfer Voltage:           70.0 V          " + eol;
    s += eol;
    s += "Output Current:                 " + current + " A - Total  " + eol;
    s += "Output Voltage:                 120.0 V" + eol;
    s += "Output Frequency:               60.0 Hz" + eol;
    s += eol;
    s += "Outlets On:                     " + outlets + eol;
    s += "$> ";
    return s;
}

/// Parse a response consisting of an echo line followed by one body line.
/**
 * \returns the return value of parsePDUStatus
 */
int parseLine( trippLitePDU_test &app, /**< [in] the app to parse with */
               const std::string &line /**< [in] the body line */ )
{
    std::string s = "devstatus\n" + line + "\n";
    return app.parsePDUStatus( s );
}

/// Write the standard config file used by the channel tests.
/** Channel `lamp` is outlet 3 (index 2); channel `camera` is outlets 1 and 2 (indices 0 and 1).
 */
void writeStdConfig( const std::string &file /**< [in] the config file path */ )
{
    mx::app::writeConfigFile( file,
                              { "device", "device", "lamp", "camera", "camera", "camera" },
                              { "address", "port", "outlet", "outlets", "onOrder", "offOrder" },
                              { "192.168.1.5", "23", "3", "1,2", "1,0", "0,1" } );
}

/// Verify the constructor defaults.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU construction", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::trippLitePDU();
    #endif
    // clang-format on

    trippLitePDU_test app( "pdu" );

    REQUIRE( app.m_firstOne == true );
    REQUIRE( app.m_stateDelay == Approx( 5 ) );
    REQUIRE( app.m_loopPause == 2000000000 );
    REQUIRE( app.m_outletStates.size() == 8 );
    for( size_t n = 0; n < 8; ++n )
    {
        REQUIRE( app.m_outletStates[n] == OUTLET_STATE_UNKNOWN );
    }
    REQUIRE( app.m_frequency == 0 );
    REQUIRE( app.m_voltage == 0 );
    REQUIRE( app.m_current == 0 );
}

/// Verify configuration defaults and overrides.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU configuration", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::setupConfig();
    trippLitePDU::loadConfig();
    #endif
    // clang-format on

    SECTION( "defaults" )
    {
        std::string file = "/tmp/trippLitePDU_test_defaults.conf";
        mx::app::writeConfigFile( file, { "none" }, { "nada" }, { "0" } );

        trippLitePDU_test app( "pdu" );
        app.configure( file );

        REQUIRE( app.m_deviceAddr == "" );
        REQUIRE( app.m_devicePort == "" );
        REQUIRE( app.m_deviceUsername == "" );
        REQUIRE( app.m_devicePassFile == "" );
        REQUIRE( app.m_deviceVersion == 0 );
        REQUIRE( app.m_readTimeout == 1000 );
        REQUIRE( app.m_writeTimeout == 1000 );

        REQUIRE( app.m_freqLowWarn == Approx( 59 ) );
        REQUIRE( app.m_freqHighWarn == Approx( 61 ) );
        REQUIRE( app.m_freqLowAlert == Approx( 58 ) );
        REQUIRE( app.m_freqHighAlert == Approx( 62 ) );
        REQUIRE( app.m_freqLowEmerg == Approx( 57 ) );
        REQUIRE( app.m_freqHighEmerg == Approx( 63 ) );
        REQUIRE( app.m_voltLowWarn == Approx( 105 ) );
        REQUIRE( app.m_voltHighWarn == Approx( 125 ) );
        REQUIRE( app.m_voltLowAlert == Approx( 101 ) );
        REQUIRE( app.m_voltHighAlert == Approx( 126 ) );
        REQUIRE( app.m_voltLowEmerg == Approx( 99 ) );
        REQUIRE( app.m_voltHighEmerg == Approx( 128 ) );
        REQUIRE( app.m_currWarn == Approx( 15 ) );
        REQUIRE( app.m_currAlert == Approx( 16 ) );
        REQUIRE( app.m_currEmerg == Approx( 20 ) );

        REQUIRE( app.numChannels() == 0 );

        remove( file.c_str() );
    }

    SECTION( "overrides" )
    {
        std::string file = "/tmp/trippLitePDU_test_overrides.conf";
        mx::app::writeConfigFile(
            file,
            { "device", "device", "device", "device", "device", "device", "device", "limits",
              "limits", "limits", "limits", "limits", "limits", "limits", "limits", "limits",
              "limits", "limits", "limits", "limits", "limits", "limits", "pump" },
            { "address",       "port",          "username",      "passfile",      "powerAlertVersion",
              "readTimeout",   "writeTimeout",  "freqLowWarn",   "freqHighWarn",  "freqLowAlert",
              "freqHighAlert", "freqLowEmerg",  "freqHighEmerg", "voltLowWarn",   "voltHighWarn",
              "voltLowAlert",  "voltHighAlert", "voltLowEmerg",  "voltHighEmerg", "currWarn",
              "currAlert",     "currEmerg",     "outlets" },
            { "192.168.1.5", "23",  "admin", "pdu.pass", "1",   "500", "250", "49.5", "50.5", "49", "51", "48",
              "52",          "110", "120",   "108",      "122", "100", "130", "10",   "12",   "14", "4,5" } );

        trippLitePDU_test app( "pdu" );
        app.configure( file );

        REQUIRE( app.m_deviceAddr == "192.168.1.5" );
        REQUIRE( app.m_devicePort == "23" );
        REQUIRE( app.m_deviceUsername == "admin" );
        REQUIRE( app.m_devicePassFile == "pdu.pass" );
        REQUIRE( app.m_deviceVersion == 1 );
        REQUIRE( app.m_readTimeout == 500 );
        REQUIRE( app.m_writeTimeout == 250 );

        REQUIRE( app.m_freqLowWarn == Approx( 49.5 ) );
        REQUIRE( app.m_freqHighWarn == Approx( 50.5 ) );
        REQUIRE( app.m_freqLowAlert == Approx( 49 ) );
        REQUIRE( app.m_freqHighAlert == Approx( 51 ) );
        REQUIRE( app.m_freqLowEmerg == Approx( 48 ) );
        REQUIRE( app.m_freqHighEmerg == Approx( 52 ) );
        REQUIRE( app.m_voltLowWarn == Approx( 110 ) );
        REQUIRE( app.m_voltHighWarn == Approx( 120 ) );
        REQUIRE( app.m_voltLowAlert == Approx( 108 ) );
        REQUIRE( app.m_voltHighAlert == Approx( 122 ) );
        REQUIRE( app.m_voltLowEmerg == Approx( 100 ) );
        REQUIRE( app.m_voltHighEmerg == Approx( 130 ) );
        REQUIRE( app.m_currWarn == Approx( 10 ) );
        REQUIRE( app.m_currAlert == Approx( 12 ) );
        REQUIRE( app.m_currEmerg == Approx( 14 ) );

        // 1-based outlets in the config
        REQUIRE( app.numChannels() == 1 );
        REQUIRE( app.channelOutlets( "pump" ) == std::vector<size_t>( { 3, 4 } ) );

        remove( file.c_str() );
    }
}

/// Verify parsing of a complete devstatus response.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU parsePDUStatus full response", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::parsePDUStatus(s);
    #endif
    // clang-format on

    trippLitePDU_test app( "pdu" );

    SECTION( "no outlets on" )
    {
        std::string s = statusString( "NONE", "118.5", "59.9", "3.25" );
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        REQUIRE( app.m_voltage == Approx( 118.5 ) );
        REQUIRE( app.m_frequency == Approx( 59.9 ) );
        REQUIRE( app.m_current == Approx( 3.25 ) );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == OUTLET_STATE_OFF );
        }
    }

    SECTION( "some outlets on" )
    {
        std::string s = statusString( "1 3 8" );
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        REQUIRE( app.m_voltage == Approx( 120.0 ) );
        REQUIRE( app.m_frequency == Approx( 60.0 ) );
        REQUIRE( app.m_current == Approx( 4.0 ) );

        std::vector<int> expected = { OUTLET_STATE_ON,
                                      OUTLET_STATE_OFF,
                                      OUTLET_STATE_ON,
                                      OUTLET_STATE_OFF,
                                      OUTLET_STATE_OFF,
                                      OUTLET_STATE_OFF,
                                      OUTLET_STATE_OFF,
                                      OUTLET_STATE_ON };
        REQUIRE( app.m_outletStates == expected );
    }

    SECTION( "all outlets on" )
    {
        std::string s = statusString( "1 2 3 4 5 6 7 8" );
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == OUTLET_STATE_ON );
        }
    }

    SECTION( "CRLF line endings" )
    {
        std::string s = statusString( "2 4", "121.0", "60.1", "0.50", "\r\n" );
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        REQUIRE( app.m_voltage == Approx( 121.0 ) );
        REQUIRE( app.m_frequency == Approx( 60.1 ) );
        REQUIRE( app.m_current == Approx( 0.5 ) );
        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_OFF );
        REQUIRE( app.outletState( 1 ) == OUTLET_STATE_ON );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_OFF );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_ON );
    }

    SECTION( "outlets not listed are turned off, and out of range numbers ignored" )
    {
        for( int n = 0; n < 8; ++n )
        {
            app.m_outletStates[n] = OUTLET_STATE_ON;
        }

        std::string s = statusString( "0 2  9 12" );
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( app.outletState( n ) == ( n == 1 ? OUTLET_STATE_ON : OUTLET_STATE_OFF ) );
        }
    }

    SECTION( "a response without an outlets line leaves outlet states unchanged" )
    {
        app.m_outletStates[4] = OUTLET_STATE_ON;
        std::string s         = "devstatus\nInput Voltage:   119.0 V\nInput Frequency:  60.0 Hz\n";
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        REQUIRE( app.m_voltage == Approx( 119.0 ) );
        REQUIRE( app.outletState( 4 ) == OUTLET_STATE_ON );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_UNKNOWN );
    }

    SECTION( "the first line is always skipped" )
    {
        std::string s = "Xyz: this would be an error\nInput Voltage:   110.0 V\n";
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        REQUIRE( app.m_voltage == Approx( 110.0 ) );
    }

    SECTION( "an empty response is accepted" )
    {
        std::string s;
        REQUIRE( app.parsePDUStatus( s ) == 0 );
        REQUIRE( app.m_voltage == 0 );
    }

    SECTION( "skipped header lines" )
    {
        std::string s =
            "devstatus\n---\n01: PDU\n Leading space\nDevice Status: FAULT\nLow Transfer Voltage: 1 V\n$> \n";
        REQUIRE( app.parsePDUStatus( s ) == 0 );
    }
}

/// Verify the parse error codes of parsePDUStatus.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU parsePDUStatus errors", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::parsePDUStatus(s);
    #endif
    // clang-format on

    trippLitePDU_test app( "pdu" );

    // Input Voltage
    REQUIRE( parseLine( app, "Input Voltage:120.0V" ) == -1 );
    REQUIRE( parseLine( app, "Input Voltage:     " ) == -2 );
    REQUIRE( parseLine( app, "Input Voltage: 120.0" ) == -3 );

    // Input Frequency
    REQUIRE( parseLine( app, "Input Frequency:60.0Hz" ) == -4 );
    REQUIRE( parseLine( app, "Input Frequency:     " ) == -5 );
    REQUIRE( parseLine( app, "Input Frequency: 60.0" ) == -6 );

    // Other Input lines
    REQUIRE( parseLine( app, "Input Current: 3.0 A" ) == -1 );

    // Output Current
    REQUIRE( parseLine( app, "Output Current:4.00A" ) == -7 );
    REQUIRE( parseLine( app, "Output Current:     " ) == -8 );
    REQUIRE( parseLine( app, "Output Current: 4.00" ) == -9 );

    // Outlets On
    REQUIRE( parseLine( app, "Outlets On:" ) == -10 );
    REQUIRE( parseLine( app, "Outlets On:    " ) == -11 );

    // Other Output lines
    REQUIRE( parseLine( app, "Output Power:   100 W" ) == -12 );

    // Unknown lines
    REQUIRE( parseLine( app, "Xyz: 1" ) == -13 );
    REQUIRE( parseLine( app, "Model: PDU" ) == -13 );

    // Values parsed before an error are kept, later ones are not
    app.m_voltage   = 0;
    app.m_frequency = 0;
    std::string s   = "devstatus\nInput Voltage:   117.0 V\nXyz\nInput Frequency: 61.0 Hz\n";
    REQUIRE( app.parsePDUStatus( s ) == -13 );
    REQUIRE( app.m_voltage == Approx( 117.0 ) );
    REQUIRE( app.m_frequency == 0 );
}

/// Verify the simulator-backed device interface.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU device interface in simulation", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::devConnect();
    trippLitePDU::devLogin();
    trippLitePDU::devPostLogin();
    trippLitePDU::devStatus(s);
    #endif
    // clang-format on

    trippLitePDU_test app( "pdu" );

    REQUIRE( app.devConnect() == 0 );
    REQUIRE( app.devLogin() == 0 );
    app.devPostLogin();

    std::string s;
    REQUIRE( app.devStatus( s ) == 0 );
    REQUIRE( s.find( "Outlets On:" ) != std::string::npos );
    REQUIRE( s.find( "NONE" ) != std::string::npos );

    // the simulator output parses
    REQUIRE( app.parsePDUStatus( s ) == 0 );
    REQUIRE( app.m_voltage == Approx( 120 ) );
    REQUIRE( app.m_frequency == Approx( 60 ) );
    REQUIRE( app.m_current == Approx( 4 ) );
    for( int n = 0; n < 8; ++n )
    {
        REQUIRE( app.outletState( n ) == OUTLET_STATE_OFF );
    }

    app.m_simulator.m_outlets[1] = 1;
    app.m_simulator.m_outlets[6] = 1;
    app.m_simulator.m_voltage    = 108.3;
    app.m_simulator.m_frequency  = 59.2;
    app.m_simulator.m_current    = 12.75;
    REQUIRE( app.devStatus( s ) == 0 );
    REQUIRE( app.parsePDUStatus( s ) == 0 );
    REQUIRE( app.m_voltage == Approx( 108.3 ) );
    REQUIRE( app.m_frequency == Approx( 59.2 ) );
    REQUIRE( app.m_current == Approx( 12.75 ) );
    for( int n = 0; n < 8; ++n )
    {
        REQUIRE( app.outletState( n ) == ( n == 1 || n == 6 ? OUTLET_STATE_ON : OUTLET_STATE_OFF ) );
    }
}

/// Verify turnOutletOn/turnOutletOff and updateOutletState(s) against the simulator.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU outlet control in simulation", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::turnOutletOn(0);
    trippLitePDU::turnOutletOff(0);
    trippLitePDU::updateOutletState(0);
    trippLitePDU::updateOutletStates();
    #endif
    // clang-format on

    trippLitePDU_test app( "pdu" );

    SECTION( "on and off" )
    {
        REQUIRE( app.turnOutletOn( 2 ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 1 );

        // the stored state is only updated by reading the device
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_UNKNOWN );
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_ON );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_OFF );

        REQUIRE( app.turnOutletOff( 2 ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 0 );

        // updating one outlet updates all of them
        REQUIRE( app.turnOutletOn( 7 ) == 0 );
        REQUIRE( app.updateOutletState( 0 ) == 0 );
        REQUIRE( app.outletState( 2 ) == OUTLET_STATE_OFF );
        REQUIRE( app.outletState( 7 ) == OUTLET_STATE_ON );
    }

    SECTION( "invalid outlets are rejected" )
    {
        REQUIRE( app.turnOutletOn( 8 ) == -1 );
        REQUIRE( app.turnOutletOff( 8 ) == -1 );
        REQUIRE( app.turnOutletOn( -1 ) == -1 );
        REQUIRE( app.turnOutletOff( -1 ) == -1 );
    }

    SECTION( "the INDI mutex is released" )
    {
        REQUIRE( app.turnOutletOn( 0 ) == 0 );
        REQUIRE( app.turnOutletOn( 9 ) == -1 );
        std::unique_lock<std::mutex> lock( app.m_indiMutex, std::try_to_lock );
        REQUIRE( lock.owns_lock() );
    }

    SECTION( "the parsed load is updated" )
    {
        app.m_simulator.m_voltage = 115.0;
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.m_voltage == Approx( 115.0 ) );
    }
}

/// Verify channel control through the outletController against the simulator.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU channel control in simulation", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    dev::outletController<trippLitePDU>::turnChannelOn("");
    dev::outletController<trippLitePDU>::turnChannelOff("");
    dev::outletController<trippLitePDU>::channelState("");
    #endif
    // clang-format on

    std::string file = "/tmp/trippLitePDU_test_channels.conf";
    writeStdConfig( file );

    trippLitePDU_test app( "pdu" );
    app.configure( file );

    REQUIRE( app.numChannels() == 2 );
    REQUIRE( app.channelOutlets( "lamp" ) == std::vector<size_t>( { 2 } ) );
    REQUIRE( app.channelOutlets( "camera" ) == std::vector<size_t>( { 0, 1 } ) );

    SECTION( "turning channels on and off" )
    {
        app.m_stateDelay = 0;

        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_OFF );

        REQUIRE( app.turnChannelOn( "camera" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[0] == 1 );
        REQUIRE( app.m_simulator.m_outlets[1] == 1 );
        REQUIRE( app.m_simulator.m_outlets[2] == 0 );

        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_ON );
        REQUIRE( app.channelState( "lamp" ) == OUTLET_STATE_OFF );

        REQUIRE( app.turnChannelOn( "lamp" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 1 );

        REQUIRE( app.turnChannelOff( "camera" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[0] == 0 );
        REQUIRE( app.m_simulator.m_outlets[1] == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 1 );

        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_OFF );
        REQUIRE( app.channelState( "lamp" ) == OUTLET_STATE_ON );
    }

    SECTION( "intermediate channels" )
    {
        app.m_stateDelay             = 0;
        app.m_simulator.m_outlets[1] = 1;
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "camera" ) == OUTLET_STATE_INTERMEDIATE );

        REQUIRE( app.turnChannelOn( "camera" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[0] == 1 );
        REQUIRE( app.m_simulator.m_outlets[1] == 1 );
    }

    SECTION( "the default 5 s state delay blocks a quick second change" )
    {
        REQUIRE( app.m_stateDelay == Approx( 5 ) );

        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.turnChannelOn( "lamp" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 1 );
        REQUIRE( app.updateOutletStates() == 0 );
        REQUIRE( app.channelState( "lamp" ) == OUTLET_STATE_ON );

        // returns success, but nothing is done
        REQUIRE( app.turnChannelOff( "lamp" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 1 );

        // the delay is per channel
        REQUIRE( app.turnChannelOn( "camera" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[0] == 1 );

        app.m_stateDelay = 0;
        REQUIRE( app.turnChannelOff( "lamp" ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 0 );
    }

    remove( file.c_str() );
}

/// Verify the outletController channel new callback against the simulator.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU newCallBack_channels in simulation", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    dev::outletController<trippLitePDU>::newCallBack_channels(pcf::IndiProperty());
    #endif
    // clang-format on

    std::string file = "/tmp/trippLitePDU_test_newcb.conf";
    writeStdConfig( file );

    trippLitePDU_test app( "pdu" );
    app.configure( file );
    app.m_stateDelay = 0;
    REQUIRE( app.updateOutletStates() == 0 );

    pcf::IndiProperty ip( pcf::IndiProperty::Text );
    ip.setDevice( "pdu" );
    ip.setName( "lamp" );
    ip.add( pcf::IndiElement( "target", std::string( "On" ) ) );

    SECTION( "rejected when not READY" )
    {
        REQUIRE( app.newCallBack_channels( ip ) == -1 );
        REQUIRE( app.m_simulator.m_outlets[2] == 0 );
    }

    SECTION( "READY" )
    {
        app.state( stateCodes::READY );

        REQUIRE( app.newCallBack_channels( ip ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 1 );
        REQUIRE( app.updateOutletStates() == 0 );

        ip["target"].set( std::string( "off" ) );
        REQUIRE( app.newCallBack_channels( ip ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 0 );
        REQUIRE( app.updateOutletStates() == 0 );

        // unrecognized targets do nothing
        ip["target"].set( std::string( "toggle" ) );
        REQUIRE( app.newCallBack_channels( ip ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[2] == 0 );
    }

    SECTION( "state is used if there is no target" )
    {
        app.state( stateCodes::READY );

        pcf::IndiProperty ips( pcf::IndiProperty::Text );
        ips.setDevice( "pdu" );
        ips.setName( "camera" );
        ips.add( pcf::IndiElement( "state", std::string( "ON" ) ) );

        REQUIRE( app.newCallBack_channels( ips ) == 0 );
        REQUIRE( app.m_simulator.m_outlets[0] == 1 );
        REQUIRE( app.m_simulator.m_outlets[1] == 1 );
    }

    remove( file.c_str() );
}

/// Verify appStartup registers the load, status and outletController properties.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU appStartup", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::appStartup();
    #endif
    // clang-format on

    std::string file = "/tmp/trippLitePDU_test_startup.conf";
    writeStdConfig( file );

    trippLitePDU_test app( "pdu" );
    app.configure( file );

    REQUIRE( app.appStartup() == 0 );
    REQUIRE( app.state() == stateCodes::NOTCONNECTED );

    REQUIRE( app.m_indiP_status.getName() == "status" );
    REQUIRE( app.m_indiP_status.getDevice() == "pdu" );
    REQUIRE( app.m_indiP_status.find( "value" ) );

    REQUIRE( app.m_indiP_load.getName() == "load" );
    REQUIRE( app.m_indiP_load.find( "frequency" ) );
    REQUIRE( app.m_indiP_load.find( "voltage" ) );
    REQUIRE( app.m_indiP_load.find( "current" ) );

    REQUIRE( app.m_indiNewCallBacks.count( "pdu.status" ) == 1 );
    REQUIRE( app.m_indiNewCallBacks.count( "pdu.load" ) == 1 );
    REQUIRE( app.m_indiNewCallBacks.count( "pdu.outlet" ) == 1 );
    REQUIRE( app.m_indiNewCallBacks.count( "pdu.lamp" ) == 1 );
    REQUIRE( app.m_indiNewCallBacks.count( "pdu.camera" ) == 1 );
    REQUIRE( app.m_indiNewCallBacks.count( "pdu.stateTimes" ) == 1 );
    REQUIRE( app.m_indiNewCallBacks.count( "pdu.channelOutlets" ) == 1 );

    REQUIRE( app.m_indiP_outletStates.getNumElements() == 8 );
    REQUIRE( app.m_indiP_outletStates.find( "1" ) );
    REQUIRE( app.m_indiP_outletStates.find( "8" ) );
    REQUIRE( app.m_indiP_chOutlets["camera"].get() == "0,1" );
    REQUIRE( app.m_indiP_chOutlets["lamp"].get() == "2" );

    remove( file.c_str() );
}

/// Verify the appLogic state machine with the simulator.
/**
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU appLogic in simulation", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::appLogic();
    trippLitePDU::appShutdown();
    #endif
    // clang-format on

    std::string file = "/tmp/trippLitePDU_test_logic.conf";
    writeStdConfig( file );

    trippLitePDU_test app( "pdu" );
    app.configure( file );
    REQUIRE( app.appStartup() == 0 );

    SECTION( "NOTCONNECTED connects, logs in and reads the status in one pass" )
    {
        app.m_simulator.m_outlets[3] = 1;
        app.m_simulator.m_voltage    = 119.5;

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( app.m_voltage == Approx( 119.5 ) );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_ON );
        REQUIRE( app.outletState( 0 ) == OUTLET_STATE_OFF );

        // READY keeps polling
        app.m_simulator.m_outlets[3] = 0;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( app.outletState( 3 ) == OUTLET_STATE_OFF );
    }

    SECTION( "READY with the INDI mutex held skips the poll" )
    {
        app.state( stateCodes::READY );
        app.m_simulator.m_outlets[5] = 1;

        { // mutex scope
            std::lock_guard<std::mutex> guard( app.m_indiMutex );
            REQUIRE( app.appLogic() == 0 );
        }
        REQUIRE( app.outletState( 5 ) == OUTLET_STATE_UNKNOWN );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.outletState( 5 ) == OUTLET_STATE_ON );
    }

    SECTION( "an unhandled state is a failure" )
    {
        app.state( stateCodes::POWERON );
        REQUIRE( app.appLogic() == -1 );
        REQUIRE( app.state() == stateCodes::FAILURE );
    }

    SECTION( "appShutdown does nothing" )
    {
        REQUIRE( app.appShutdown() == 0 );
    }

    remove( file.c_str() );
}

/// Verify updateAlarmsAndWarnings runs for values in every threshold band.
/** The function only logs, so this checks that every branch can be reached without error.
 *
 * \ingroup trippLitePDU_unit_test
 */
TEST_CASE( "trippLitePDU updateAlarmsAndWarnings", "[trippLitePDU]" )
{
    // clang-format off
    #ifdef TRIPPLITEPDU_TEST_DOXYGEN_REF
    trippLitePDU::updateAlarmsAndWarnings();
    #endif
    // clang-format on

    trippLitePDU_test app( "pdu" );

    std::vector<float> freqs = { 56, 64, 57.5, 62.5, 58.5, 61.5, 60 };
    std::vector<float> volts = { 98, 129, 100, 127, 103, 125.5, 115 };
    std::vector<float> currs = { 21, 17, 15.5, 1 };

    for( size_t n = 0; n < freqs.size(); ++n )
    {
        app.m_frequency = freqs[n];
        app.m_voltage   = volts[n];
        app.m_current   = currs[n % currs.size()];
        REQUIRE_NOTHROW( app.updateAlarmsAndWarnings() );
    }
}

} // namespace trippLitePDUTest

} // namespace libXWCTest
