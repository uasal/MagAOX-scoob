/** \file cacaoInterface_test.cpp
 * \brief Catch2 tests for the cacaoInterface app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup cacaoInterface_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../cacaoInterface.hpp"

// Included after the app header so the callback bodies stay live.  The device/name validation
// macros still work because every callback returns 0 for a property with no elements.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup cacaoInterface_unit_test cacaoInterface Unit Tests
 * \brief Unit tests for the cacaoInterface application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `cacaoInterface` unit tests.
/** \ingroup cacaoInterface_unit_test
 */
namespace cacaoInterfaceTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Path of the regular file standing in for the CACAO fpsCTRL FIFO.
const std::string fifoPath = "/tmp/cacaoInterface_test_fpsCTRL.fifo";

/// Read the whole content of a file.
/**
 * \returns the file content, or an empty string if it cannot be opened
 */
std::string readFile( const std::string &path /**< [in] the file to read */ )
{
    std::ifstream     fin( path );
    std::stringstream ss;
    ss << fin.rdbuf();
    return ss.str();
}

/// Create (or truncate) a file and write the given content.
void writeFile( const std::string &path, /**< [in] the file to write */
                const std::string &content /**< [in] the content to write */ )
{
    std::ofstream fout( path, std::ios::trunc );
    fout << content;
}

/// Test harness exposing cacaoInterface internals.
class cacaoInterface_test : public cacaoInterface
{
  public:
    /// Construct a harness with the given device name.
    explicit cacaoInterface_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_loopState, loop_state );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_loopGain, loop_gain );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_loopZero, loop_zero );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_multCoeff, loop_multcoeff );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_maxLim, loop_max_limit );
    }

    using cacaoInterface::m_fpsFifo;
    using cacaoInterface::m_gain;
    using cacaoInterface::m_gain_target;
    using cacaoInterface::m_loopName;
    using cacaoInterface::m_loopNumber;
    using cacaoInterface::m_loopProcesses;
    using cacaoInterface::m_loopProcesses_stat;
    using cacaoInterface::m_loopState;
    using cacaoInterface::m_maxLim;
    using cacaoInterface::m_maxLim_target;
    using cacaoInterface::m_multCoeff;
    using cacaoInterface::m_multCoeff_target;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Point the app at a fresh, empty stand-in FIFO file and set the loop identity.
    void useFakeFifo( const std::string &loopNumber /**< [in] loop number, X in aolX */ )
    {
        m_loopNumber = loopNumber;
        m_loopName   = "cacaoInterfaceTest";
        m_fpsFifo    = fifoPath;
        writeFile( fifoPath, "" );
    }

    /// Get the path of the reply file that `getFPSValStr()`/`getFPSValNum()` wait for.
    /**
     * \returns the reply file path under `/dev/shm`
     */
    std::string outFile( const std::string &fps, /**< [in] the FPS name */
                         const std::string &param /**< [in] the FPS parameter */ )
    {
        return "/dev/shm/" + m_loopName + "_out_" + fps + "-" + m_loopNumber + "." + param;
    }
};

/// Build a single-element Number request.
/**
 * \returns the INDI property
 */
pcf::IndiProperty numberRequest( const std::string &device, /**< [in] INDI device name */
                                 const std::string &name,   /**< [in] INDI property name */
                                 const std::string &el,     /**< [in] element name */
                                 const std::string &value   /**< [in] element value as text */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el ) );
    ip[el].setValue( value );
    return ip;
}

/// Build a single-element Switch request.
/**
 * \returns the INDI property
 */
pcf::IndiProperty switchRequest( const std::string                &device, /**< [in] INDI device name */
                                 const std::string                &name,   /**< [in] INDI property name */
                                 const std::string                &el,     /**< [in] element name */
                                 pcf::IndiElement::SwitchStateType state   /**< [in] switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}
/// \endcond

/// Verify the cacaoInterface INDI callback validators accept only the expected properties.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface INDI callbacks validate device and property names", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::newCallBack_m_indiP_loopState( pcf::IndiProperty() );
    cacaoInterface::newCallBack_m_indiP_loopGain( pcf::IndiProperty() );
    cacaoInterface::newCallBack_m_indiP_loopZero( pcf::IndiProperty() );
    cacaoInterface::newCallBack_m_indiP_multCoeff( pcf::IndiProperty() );
    cacaoInterface::newCallBack_m_indiP_maxLim( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_ARBNEW_CALLBACK( cacaoInterface, newCallBack_m_indiP_loopState, loop_state );
    XWCTEST_INDI_ARBNEW_CALLBACK( cacaoInterface, newCallBack_m_indiP_loopGain, loop_gain );
    XWCTEST_INDI_ARBNEW_CALLBACK( cacaoInterface, newCallBack_m_indiP_loopZero, loop_zero );
    XWCTEST_INDI_ARBNEW_CALLBACK( cacaoInterface, newCallBack_m_indiP_multCoeff, loop_multcoeff );
    XWCTEST_INDI_ARBNEW_CALLBACK( cacaoInterface, newCallBack_m_indiP_maxLim, loop_max_limit );
}

/// Verify the cacaoInterface configuration defaults and overrides.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface configuration", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::setupConfig();
    cacaoInterface::loadConfig();
    cacaoInterface::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/cacaoInterface_test_config.conf";

    SECTION( "defaults" )
    {
        mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

        cacaoInterface_test app( "cacaoif" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_loopNumber == "" );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
        REQUIRE( app.m_tel.m_logLevel == logPrio::LOG_TELEM );

        // Keep telemetry records out of the queue so nothing is written at destruction.
        app.m_tel.m_logLevel = logPrio::LOG_INFO;
    }

    SECTION( "overrides" )
    {
        mx::app::writeConfigFile( fname, { "loop", "telemeter" }, { "number", "maxInterval" }, { "3", "25" } );

        cacaoInterface_test app( "cacaoif" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_loopNumber == "3" );
        REQUIRE( app.m_maxInterval == Approx( 25.0 ) );

        app.m_tel.m_logLevel = logPrio::LOG_INFO;
    }

    std::remove( fname.c_str() );
}

/// Verify `appStartup()` refuses to start without a loop number.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface appStartup requires a loop number", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::appStartup();
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.m_loopNumber = "";

    REQUIRE( app.appStartup() == -1 );
}

/// Verify `setFPSVal()` writes the CACAO `setval` command to the fpsCTRL FIFO.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface setFPSVal writes setval commands", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::setFPSVal( std::string(), std::string(), std::string() );
    cacaoInterface::setFPSVal<int>( std::string(), std::string(), 0 );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "7" );

    SECTION( "string value" )
    {
        REQUIRE( app.setFPSVal( "mfilt", "loopON", std::string( "ON" ) ) == 0 );
        REQUIRE( readFile( fifoPath ) == "setval mfilt-7.loopON ON\n" );
    }

    SECTION( "numeric values are formatted with std::to_string" )
    {
        REQUIRE( app.setFPSVal( "mfilt", "loopgain", 0.5f ) == 0 );
        REQUIRE( readFile( fifoPath ) == "setval mfilt-7.loopgain 0.500000\n" );

        writeFile( fifoPath, "" );
        REQUIRE( app.setFPSVal( "mfilt", "count", 3 ) == 0 );
        REQUIRE( readFile( fifoPath ) == "setval mfilt-7.count 3\n" );
    }

    SECTION( "a missing FIFO is an error" )
    {
        app.m_fpsFifo = "/tmp/cacaoInterface_test_no_such.fifo";
        std::remove( app.m_fpsFifo.c_str() );
        REQUIRE( app.setFPSVal( "mfilt", "loopON", std::string( "ON" ) ) == -1 );
    }

    std::remove( fifoPath.c_str() );
}

/// Verify the gain, mult. coefficient and limit setters send their targets.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface gain, mult and limit setters send targets", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::setGain();
    cacaoInterface::setMultCoeff();
    cacaoInterface::setMaxLim();
    cacaoInterface::recordLoopGain( true );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "1" );

    app.m_gain_target = 0.25;
    REQUIRE( app.setGain() == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-1.loopgain 0.250000\n" );

    writeFile( fifoPath, "" );
    app.m_multCoeff_target = 0.99;
    REQUIRE( app.setMultCoeff() == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-1.loopmult 0.990000\n" );

    writeFile( fifoPath, "" );
    app.m_maxLim_target = 1.5;
    REQUIRE( app.setMaxLim() == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-1.looplimit 1.500000\n" );

    // Without a FIFO every setter fails.
    app.m_fpsFifo = "/tmp/cacaoInterface_test_no_such.fifo";
    std::remove( app.m_fpsFifo.c_str() );
    REQUIRE( app.setGain() == -1 );
    REQUIRE( app.setMultCoeff() == -1 );
    REQUIRE( app.setMaxLim() == -1 );

    std::remove( fifoPath.c_str() );
}

/// Verify the loop on/off/zero commands.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface loop on, off and zero commands", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::loopOn();
    cacaoInterface::loopOff();
    cacaoInterface::loopZero();
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "2" );

    REQUIRE( app.loopOn() == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-2.loopON ON\n" );

    writeFile( fifoPath, "" );
    app.m_gain = 0; // logs loop_open
    REQUIRE( app.loopOff() == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-2.loopON OFF\n" );

    writeFile( fifoPath, "" );
    app.m_gain = 0.3; // logs loop_paused
    REQUIRE( app.loopOff() == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-2.loopON OFF\n" );

    writeFile( fifoPath, "" );
    REQUIRE( app.loopZero() == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-2.loopZERO ON\n" );

    app.m_fpsFifo = "/tmp/cacaoInterface_test_no_such.fifo";
    std::remove( app.m_fpsFifo.c_str() );
    REQUIRE( app.loopOn() == -1 );
    REQUIRE( app.loopOff() == -1 );
    REQUIRE( app.loopZero() == -1 );

    std::remove( fifoPath.c_str() );
}

/// Verify `getFPSValStr()` and `getFPSValNum()` parse the value from the CACAO reply file.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface getFPSVal parses CACAO replies", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::getFPSValStr( std::string(), std::string() );
    cacaoInterface::getFPSValNum( std::string(), std::string() );
    #endif
    // clang-format on

    REQUIRE( std::filesystem::is_directory( "/dev/shm" ) );

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "5" );

    SECTION( "string value keeps everything after the last separating space" )
    {
        const std::string out = app.outFile( "mfilt", "loopON" );
        writeFile( out, "mfilt-5.loopON   OFF\n" );

        // The trailing newline is not stripped from the returned value.
        REQUIRE( app.getFPSValStr( "mfilt", "loopON" ) == "OFF\n" );
        REQUIRE( readFile( fifoPath ) == "fwrval mfilt-5.loopON " + out + "\n" );
        REQUIRE_FALSE( std::filesystem::exists( out ) );
    }

    SECTION( "numeric value keeps the leading space and converts with stof" )
    {
        const std::string out = app.outFile( "mfilt", "loopgain" );
        writeFile( out, "mfilt-5.loopgain 0.35\n" );

        std::string ans = app.getFPSValNum( "mfilt", "loopgain" );
        REQUIRE( ans == " 0.35\n" );
        REQUIRE( std::stof( ans ) == Approx( 0.35 ) );
        REQUIRE( readFile( fifoPath ) == "fwrval mfilt-5.loopgain " + out + "\n" );
        REQUIRE_FALSE( std::filesystem::exists( out ) );
    }

    SECTION( "an empty reply gives an empty string" )
    {
        const std::string out = app.outFile( "mfilt", "loopmult" );
        writeFile( out, "  \n" );
        REQUIRE( app.getFPSValStr( "mfilt", "loopmult" ) == "" );

        writeFile( out, "\n" );
        REQUIRE( app.getFPSValNum( "mfilt", "loopmult" ) == "" );
    }

    SECTION( "a reply without a separating space is a format error" )
    {
        const std::string out = app.outFile( "mfilt", "looplimit" );
        writeFile( out, "nospace\n" );
        REQUIRE( app.getFPSValStr( "mfilt", "looplimit" ) == "" );

        writeFile( out, "nospace\n" );
        REQUIRE( app.getFPSValNum( "mfilt", "looplimit" ) == "" );
    }

    SECTION( "a missing FIFO returns immediately with an empty string" )
    {
        app.m_fpsFifo = "/tmp/cacaoInterface_test_no_such.fifo";
        std::remove( app.m_fpsFifo.c_str() );
        REQUIRE( app.getFPSValStr( "mfilt", "loopON" ) == "" );
        REQUIRE( app.getFPSValNum( "mfilt", "loopgain" ) == "" );
    }

    std::remove( fifoPath.c_str() );
}

/// Verify the loop state toggle turns the loop on and off.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface loop_state callback turns the loop on and off", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::newCallBack_m_indiP_loopState( pcf::IndiProperty() );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "4" );

    REQUIRE( app.newCallBack_m_indiP_loopState(
                 switchRequest( "cacaoif", "loop_state", "toggle", pcf::IndiElement::On ) ) == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-4.loopON ON\n" );

    writeFile( fifoPath, "" );
    REQUIRE( app.newCallBack_m_indiP_loopState(
                 switchRequest( "cacaoif", "loop_state", "toggle", pcf::IndiElement::Off ) ) == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-4.loopON OFF\n" );

    // An unknown switch state falls through to an error without writing.
    writeFile( fifoPath, "" );
    REQUIRE( app.newCallBack_m_indiP_loopState(
                 switchRequest( "cacaoif", "loop_state", "toggle", pcf::IndiElement::UnknownSwitchState ) ) == -1 );
    REQUIRE( readFile( fifoPath ) == "" );

    // A request without a toggle element is ignored.
    REQUIRE( app.newCallBack_m_indiP_loopState(
                 switchRequest( "cacaoif", "loop_state", "other", pcf::IndiElement::On ) ) == 0 );
    REQUIRE( readFile( fifoPath ) == "" );

    std::remove( fifoPath.c_str() );
}

/// Verify the loop zero request zeroes the loop only when switched on.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface loop_zero callback zeroes the loop", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::newCallBack_m_indiP_loopZero( pcf::IndiProperty() );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "4" );

    REQUIRE( app.newCallBack_m_indiP_loopZero(
                 switchRequest( "cacaoif", "loop_zero", "request", pcf::IndiElement::Off ) ) == 0 );
    REQUIRE( readFile( fifoPath ) == "" );

    REQUIRE( app.newCallBack_m_indiP_loopZero(
                 switchRequest( "cacaoif", "loop_zero", "request", pcf::IndiElement::On ) ) == 0 );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-4.loopZERO ON\n" );

    std::remove( fifoPath.c_str() );
}

/// Verify the gain callback takes the target (or current) value and sends it.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface loop_gain callback sets the gain target", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::newCallBack_m_indiP_loopGain( pcf::IndiProperty() );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "6" );

    SECTION( "target element" )
    {
        REQUIRE( app.newCallBack_m_indiP_loopGain( numberRequest( "cacaoif", "loop_gain", "target", "0.4" ) ) == 0 );
        REQUIRE( app.m_gain_target == Approx( 0.4 ) );
        REQUIRE( readFile( fifoPath ) == "setval mfilt-6.loopgain 0.400000\n" );
    }

    SECTION( "target takes precedence over current" )
    {
        pcf::IndiProperty ip = numberRequest( "cacaoif", "loop_gain", "current", "0.1" );
        ip.add( pcf::IndiElement( "target" ) );
        ip["target"].setValue( std::string( "0.2" ) );

        REQUIRE( app.newCallBack_m_indiP_loopGain( ip ) == 0 );
        REQUIRE( app.m_gain_target == Approx( 0.2 ) );
    }

    SECTION( "current is used when there is no target" )
    {
        REQUIRE( app.newCallBack_m_indiP_loopGain( numberRequest( "cacaoif", "loop_gain", "current", "0.75" ) ) == 0 );
        REQUIRE( app.m_gain_target == Approx( 0.75 ) );
        REQUIRE( readFile( fifoPath ) == "setval mfilt-6.loopgain 0.750000\n" );
    }

    SECTION( "no usable value is ignored" )
    {
        app.m_gain_target = 0.9;
        REQUIRE( app.newCallBack_m_indiP_loopGain( numberRequest( "cacaoif", "loop_gain", "other", "0.3" ) ) == 0 );
        REQUIRE( app.m_gain_target == Approx( 0.9 ) );
        REQUIRE( readFile( fifoPath ) == "" );
    }

    SECTION( "a failed send is reported" )
    {
        app.m_fpsFifo = "/tmp/cacaoInterface_test_no_such.fifo";
        std::remove( app.m_fpsFifo.c_str() );
        REQUIRE( app.newCallBack_m_indiP_loopGain( numberRequest( "cacaoif", "loop_gain", "target", "0.4" ) ) == -1 );
        REQUIRE( app.m_gain_target == Approx( 0.4 ) );
    }

    std::remove( fifoPath.c_str() );
}

/// Verify the mult. coefficient callback sets and sends its target.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface loop_multcoeff callback sets the mult. coefficient target", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::newCallBack_m_indiP_multCoeff( pcf::IndiProperty() );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "6" );

    REQUIRE( app.newCallBack_m_indiP_multCoeff( numberRequest( "cacaoif", "loop_multcoeff", "target", "0.995" ) ) ==
             0 );
    REQUIRE( app.m_multCoeff_target == Approx( 0.995 ) );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-6.loopmult 0.995000\n" );

    writeFile( fifoPath, "" );
    REQUIRE( app.newCallBack_m_indiP_multCoeff( numberRequest( "cacaoif", "loop_multcoeff", "current", "0.5" ) ) == 0 );
    REQUIRE( app.m_multCoeff_target == Approx( 0.5 ) );

    writeFile( fifoPath, "" );
    REQUIRE( app.newCallBack_m_indiP_multCoeff( numberRequest( "cacaoif", "loop_multcoeff", "other", "0.1" ) ) == 0 );
    REQUIRE( app.m_multCoeff_target == Approx( 0.5 ) );
    REQUIRE( readFile( fifoPath ) == "" );

    std::remove( fifoPath.c_str() );
}

/// Verify the max. limit callback sets and sends its target.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface loop_max_limit callback sets the limit target", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::newCallBack_m_indiP_maxLim( pcf::IndiProperty() );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.useFakeFifo( "6" );

    REQUIRE( app.newCallBack_m_indiP_maxLim( numberRequest( "cacaoif", "loop_max_limit", "target", "2.5" ) ) == 0 );
    REQUIRE( app.m_maxLim_target == Approx( 2.5 ) );
    REQUIRE( readFile( fifoPath ) == "setval mfilt-6.looplimit 2.500000\n" );

    writeFile( fifoPath, "" );
    REQUIRE( app.newCallBack_m_indiP_maxLim( numberRequest( "cacaoif", "loop_max_limit", "current", "0.125" ) ) == 0 );
    REQUIRE( app.m_maxLim_target == Approx( 0.125 ) );

    writeFile( fifoPath, "" );
    REQUIRE( app.newCallBack_m_indiP_maxLim( numberRequest( "cacaoif", "loop_max_limit", "other", "9" ) ) == 0 );
    REQUIRE( app.m_maxLim_target == Approx( 0.125 ) );
    REQUIRE( readFile( fifoPath ) == "" );

    std::remove( fifoPath.c_str() );
}

/// Verify `checkLoopProcesses()` mirrors the status-file value.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface checkLoopProcesses mirrors the status flag", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::checkLoopProcesses();
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );

    app.m_loopProcesses_stat = true;
    REQUIRE( app.checkLoopProcesses() == 0 );
    REQUIRE( app.m_loopProcesses );

    app.m_loopProcesses_stat = false;
    REQUIRE( app.checkLoopProcesses() == 0 );
    REQUIRE_FALSE( app.m_loopProcesses );
}

/// Verify `getAOCalib()` succeeds without changes when no calibration source exists.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface getAOCalib tolerates a missing calibration source", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::getAOCalib();
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );
    app.m_loopNumber = "cacaoInterfaceTestNoSuchLoop";
    app.m_loopName   = "unchanged";
    app.m_fpsFifo    = "";

    REQUIRE_FALSE( std::filesystem::exists( "/milk/shm/aolcacaoInterfaceTestNoSuchLoop_calib_source.txt" ) );
    REQUIRE( app.getAOCalib() == 0 );
    REQUIRE( app.m_loopName == "unchanged" );
    REQUIRE( app.m_fpsFifo == "" );
}

/// Verify the telemetry hooks succeed.
/**
 * \ingroup cacaoInterface_unit_test
 */
TEST_CASE( "cacaoInterface telemetry hooks succeed", "[cacaoInterface]" )
{
    // clang-format off
    #ifdef CACAOINTERFACE_TEST_DOXYGEN_REF
    cacaoInterface::checkRecordTimes();
    cacaoInterface::recordTelem( nullptr );
    cacaoInterface::recordLoopGain( false );
    #endif
    // clang-format on

    cacaoInterface_test app( "cacaoif" );

    app.m_loopState = 2;
    app.m_gain      = 0.5;
    REQUIRE( app.recordLoopGain() == 0 );
    REQUIRE( app.recordLoopGain() == 0 );
    REQUIRE( app.recordTelem( nullptr ) == 0 );
    REQUIRE( app.checkRecordTimes() == 0 );
}

} // namespace cacaoInterfaceTest

} // namespace libXWCTest
