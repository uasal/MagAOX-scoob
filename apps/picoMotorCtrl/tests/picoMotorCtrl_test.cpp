/** \file picoMotorCtrl_test.cpp
 * \brief Catch2 tests for the picoMotorCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup picoMotorCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cerrno>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include "../picoMotorCtrl.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup picoMotorCtrl_unit_test picoMotorCtrl Unit Tests
 * \brief Unit tests for the picoMotorCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `picoMotorCtrl` unit tests.
/** \ingroup picoMotorCtrl_unit_test
 */
namespace picoMotorCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing the protected picoMotorCtrl and MagAOXApp state the tests need.
/** The channel map, device address and telnet connection are private in picoMotorCtrl (and the app declares no
 * test friend), so the tests observe them through the public interface only.
 */
class picoMotorCtrl_test : public picoMotorCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit picoMotorCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    /// Call loadConfigImpl() with the app's own configurator.
    /**
     * \returns the return value of loadConfigImpl()
     */
    int loadImpl()
    {
        return loadConfigImpl( config );
    }

    /// Read a config file into the app's configurator.
    /**
     * \returns the return value of readConfig()
     */
    int readConfigFile( const std::string &fname /**< [in] the config file name */ )
    {
        return config.readConfig( fname );
    }

    /// Get the power management flag.
    bool powerMgt() const
    {
        return m_powerMgtEnabled;
    }

    /// Get the shutdown flag.
    int shutdownFlag() const
    {
        return m_shutdown;
    }

    /// Set the system directory used for the channel count files.
    void sysPath( const std::string &path /**< [in] the system directory */ )
    {
        m_sysPath = path;
    }

    /// Get the configured telemetry maximum interval.
    double maxInterval() const
    {
        return m_maxInterval;
    }
};

/// Write a config file, read it into the app configurator, and remove the file.
/**
 * \returns the return value of readConfig()
 */
int readTestConfig( picoMotorCtrl_test             &app,      /**< [in,out] the app, with setupConfig() called */
                    const std::string              &tag,      /**< [in] a unique tag for the file name */
                    const std::vector<std::string> &sections, /**< [in] the config sections */
                    const std::vector<std::string> &keywords, /**< [in] the config keywords */
                    const std::vector<std::string> &values /**< [in] the config values */ )
{
    std::string fname = "/tmp/picoMotorCtrl_test_" + tag + "_" + std::to_string( ::getpid() ) + ".conf";
    mx::app::writeConfigFile( fname, sections, keywords, values );
    int rv = app.readConfigFile( fname );
    ::unlink( fname.c_str() );
    return rv;
}

/// Configure an app with two motors: `tip` (channel 1, presets `in`/`out`) and `tilt` (channel 2, address 2).
/**
 * \returns the return value of loadConfigImpl()
 */
int loadTwoMotors( picoMotorCtrl_test &app /**< [in,out] the app to configure */ )
{
    app.setupConfig();
    if( readTestConfig( app,
                        "twomotors",
                        { "device", "tip", "tip", "tip", "tilt", "tilt", "tilt" },
                        { "address", "channel", "names", "positions", "channel", "address", "type" },
                        { "192.168.0.10", "1", "in,out", "100,-200", "2", "2", "4" } ) != 0 )
    {
        return -100;
    }

    return app.loadImpl();
}

/// Build a switch property with the given elements switched on.
pcf::IndiProperty presetProp( const std::string              &name, /**< [in] the property (channel) name */
                              const std::vector<std::string> &on /**< [in] the elements to switch on */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "pico" );
    ip.setName( name );
    for( auto &el : on )
    {
        ip.add( pcf::IndiElement( el ) );
        ip[el].setSwitchState( pcf::IndiElement::On );
    }
    return ip;
}

/// Build a number property with a target element.
pcf::IndiProperty posProp( const std::string &name, /**< [in] the property name */
                           long               target /**< [in] the target counts */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "pico" );
    ip.setName( name );
    ip.add( pcf::IndiElement( "target" ) );
    ip["target"].set( target );
    return ip;
}

/// \endcond

/// Verify splitting of address-prefixed controller responses.
/**
 * \ingroup picoMotorCtrl_unit_test
 */
TEST_CASE( "picoMotorCtrl response splitting", "[picoMotorCtrl]" )
{
    // clang-format off
    #ifdef PICOMOTORCTRL_TEST_DOXYGEN_REF
    MagAOX::app::splitResponse( 0, std::string(), std::string() );
    #endif
    // clang-format on

    int         address = -1;
    std::string resp    = "unset";

    SECTION( "a response without an address is from address 1" )
    {
        REQUIRE( MagAOX::app::splitResponse( address, resp, "New_Focus 8742 v2.2" ) == 0 );
        REQUIRE( address == 1 );
        REQUIRE( resp == "New_Focus 8742 v2.2" );
    }

    SECTION( "an empty response is from address 1" )
    {
        REQUIRE( MagAOX::app::splitResponse( address, resp, "" ) == 0 );
        REQUIRE( address == 1 );
        REQUIRE( resp == "" );
    }

    SECTION( "a single-digit address" )
    {
        REQUIRE( MagAOX::app::splitResponse( address, resp, "2>1" ) == 0 );
        REQUIRE( address == 2 );
        REQUIRE( resp == "1" );
    }

    SECTION( "a two-digit address" )
    {
        REQUIRE( MagAOX::app::splitResponse( address, resp, "12>New_Focus 8742" ) == 0 );
        REQUIRE( address == 12 );
        REQUIRE( resp == "New_Focus 8742" );
    }

    SECTION( "a leading prompt with no address is an error" )
    {
        REQUIRE( MagAOX::app::splitResponse( address, resp, ">1" ) == -1 );
        REQUIRE( address == 0 );
        REQUIRE( resp == "" );
    }

    SECTION( "an address with no response is an error" )
    {
        REQUIRE( MagAOX::app::splitResponse( address, resp, "3>" ) == -2 );
        REQUIRE( address == 0 );
        REQUIRE( resp == "" );
    }

    SECTION( "a non-numeric address is an error" )
    {
        REQUIRE( MagAOX::app::splitResponse( address, resp, "ab>1" ) == -3 );
        REQUIRE( address == 0 );
        REQUIRE( resp == "" );
    }
}

/// Verify the constructor defaults and the trivial FSM hooks.
/**
 * \ingroup picoMotorCtrl_unit_test
 */
TEST_CASE( "picoMotorCtrl construction and trivial hooks", "[picoMotorCtrl]" )
{
    // clang-format off
    #ifdef PICOMOTORCTRL_TEST_DOXYGEN_REF
    picoMotorCtrl::picoMotorCtrl();
    picoMotorCtrl::onPowerOff();
    picoMotorCtrl::whilePowerOff();
    picoMotorCtrl::appShutdown();
    picoMotorCtrl::appLogic();
    #endif
    // clang-format on

    picoMotorCtrl_test app( "pico" );

    REQUIRE( app.powerMgt() == true );
    REQUIRE( app.m_readTimeout == 1000 );
    REQUIRE( app.m_writeTimeout == 1000 );

    REQUIRE( app.onPowerOff() == 0 );
    REQUIRE( app.whilePowerOff() == 0 );

    // With no connection states, appLogic has nothing to do.
    REQUIRE( app.state() == stateCodes::UNINITIALIZED );
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.state() == stateCodes::UNINITIALIZED );

    SECTION( "appShutdown with configured but unstarted channels" )
    {
        REQUIRE( loadTwoMotors( app ) == 0 );
        REQUIRE( app.appShutdown() == 0 );
    }
}

/// Verify configuration parsing of the device options and motor sections.
/**
 * \ingroup picoMotorCtrl_unit_test
 */
TEST_CASE( "picoMotorCtrl configuration", "[picoMotorCtrl]" )
{
    // clang-format off
    #ifdef PICOMOTORCTRL_TEST_DOXYGEN_REF
    picoMotorCtrl::setupConfig();
    picoMotorCtrl::loadConfigImpl( mx::app::appConfigurator() );
    picoMotorCtrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "no unused sections means no motors" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nomotors", { "device" }, { "address" }, { "192.168.0.10" } ) == 0 );
        REQUIRE( app.loadImpl() == PICOMOTORCTRL_E_NOMOTORS );
    }

    SECTION( "loadConfig shuts down when there are no motors" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nomotorsload", { "device" }, { "address" }, { "192.168.0.10" } ) == 0 );
        app.loadConfig();
        REQUIRE( app.shutdownFlag() != 0 );
    }

    SECTION( "an unused section without a channel is ignored" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nochannel", { "other" }, { "thing" }, { "1" } ) == 0 );
        REQUIRE( app.loadImpl() == 0 );
    }

    SECTION( "channel 0 is rejected" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "chan0", { "tip" }, { "channel" }, { "0" } ) == 0 );
        REQUIRE( app.loadImpl() == PICOMOTORCTRL_E_BADCHANNEL );
    }

    SECTION( "a channel above nChannels is rejected" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "chan5", { "tip" }, { "channel" }, { "5" } ) == 0 );
        REQUIRE( app.loadImpl() == PICOMOTORCTRL_E_BADCHANNEL );
    }

    SECTION( "nChannels raises the channel limit" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nchan8", { "device", "tip" }, { "nChannels", "channel" }, { "8", "5" } ) == 0 );
        REQUIRE( app.loadImpl() == 0 );
    }

    SECTION( "address 0 is rejected" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "addr0", { "tip", "tip" }, { "channel", "address" }, { "1", "0" } ) == 0 );
        REQUIRE( app.loadImpl() == PICOMOTORCTRL_E_BADCHANNEL );
    }

    SECTION( "motor type 0 is rejected" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "type0", { "tip", "tip" }, { "channel", "type" }, { "1", "0" } ) == 0 );
        REQUIRE( app.loadImpl() == PICOMOTORCTRL_E_BADCHANNEL );
    }

    SECTION( "a valid two-motor config with ioDevice and telemeter options" )
    {
        picoMotorCtrl_test app( "pico" );
        REQUIRE( loadTwoMotors( app ) == 0 );
        REQUIRE( app.maxInterval() == Approx( 10.0 ) );
    }

    SECTION( "loadConfig applies the ioDevice timeouts" )
    {
        picoMotorCtrl_test app( "pico" );
        app.setupConfig();
        REQUIRE( readTestConfig( app,
                                 "timeouts",
                                 { "tip", "device", "device", "telemeter" },
                                 { "channel", "readTimeout", "writeTimeout", "maxInterval" },
                                 { "1", "250", "300", "2.5" } ) == 0 );
        app.loadConfig();
        REQUIRE( app.shutdownFlag() == 0 );
        REQUIRE( app.m_readTimeout == 250 );
        REQUIRE( app.m_writeTimeout == 300 );
        REQUIRE( app.maxInterval() == Approx( 2.5 ) );
    }
}

/// Verify the relative position INDI callback rejects requests it cannot act on.
/**
 * \ingroup picoMotorCtrl_unit_test
 */
TEST_CASE( "picoMotorCtrl position INDI callback", "[picoMotorCtrl]" )
{
    // clang-format off
    #ifdef PICOMOTORCTRL_TEST_DOXYGEN_REF
    picoMotorCtrl::newCallBack_picopos( pcf::IndiProperty() );
    picoMotorCtrl::st_newCallBack_picopos( nullptr, pcf::IndiProperty() );
    #endif
    // clang-format on

    picoMotorCtrl_test app( "pico" );
    REQUIRE( loadTwoMotors( app ) == 0 );

    SECTION( "a property name without _pos is rejected" )
    {
        REQUIRE( app.newCallBack_picopos( posProp( "tip", 10 ) ) == -1 );
    }

    SECTION( "an unknown channel is rejected" )
    {
        REQUIRE( app.newCallBack_picopos( posProp( "focus_pos", 10 ) ) == -1 );
    }

    SECTION( "a known channel whose property was never created is rejected" )
    {
        // appStartup() creates the channel property; without it the target cannot be updated.
        REQUIRE( app.newCallBack_picopos( posProp( "tip_pos", 10 ) ) == -1 );
    }

    SECTION( "the static callback dispatches to the handler" )
    {
        REQUIRE( picoMotorCtrl::st_newCallBack_picopos( static_cast<picoMotorCtrl *>( &app ),
                                                        posProp( "focus_pos", 10 ) ) == -1 );
    }
}

/// Verify the preset INDI callback channel lookup and preset selection checks.
/**
 * \ingroup picoMotorCtrl_unit_test
 */
TEST_CASE( "picoMotorCtrl preset INDI callback", "[picoMotorCtrl]" )
{
    // clang-format off
    #ifdef PICOMOTORCTRL_TEST_DOXYGEN_REF
    picoMotorCtrl::newCallBack_presetName( pcf::IndiProperty() );
    picoMotorCtrl::st_newCallBack_presetName( nullptr, pcf::IndiProperty() );
    #endif
    // clang-format on

    picoMotorCtrl_test app( "pico" );
    REQUIRE( loadTwoMotors( app ) == 0 );

    SECTION( "an unknown channel is rejected" )
    {
        REQUIRE( app.newCallBack_presetName( presetProp( "focus", { "in" } ) ) == -1 );
        REQUIRE( picoMotorCtrl::st_newCallBack_presetName( static_cast<picoMotorCtrl *>( &app ),
                                                           presetProp( "focus", { "in" } ) ) == -1 );
    }

    SECTION( "selecting more than one configured preset is rejected" )
    {
        REQUIRE( app.newCallBack_presetName( presetProp( "tip", { "in", "out" } ) ) == -1 );
    }

    SECTION( "a single configured preset reaches the channel property" )
    {
        // The preset was found for the configured channel, and the callback goes on to set the target of the
        // channel property.  That property is only created in appStartup(), so the element lookup throws here.
        REQUIRE_THROWS_AS( app.newCallBack_presetName( presetProp( "tip", { "out" } ) ), std::runtime_error );
    }
}

/// Verify the channel count files in the system directory.
/**
 * \ingroup picoMotorCtrl_unit_test
 */
TEST_CASE( "picoMotorCtrl channel count files", "[picoMotorCtrl]" )
{
    // clang-format off
    #ifdef PICOMOTORCTRL_TEST_DOXYGEN_REF
    picoMotorCtrl::readChannelCounts( std::string() );
    picoMotorCtrl::writeChannelCounts( std::string(), 0 );
    #endif
    // clang-format on

    std::string name = "picoMotorCtrl_test_counts_" + std::to_string( ::getpid() );
    std::string dir  = "/tmp/" + name;

    picoMotorCtrl_test app( name );
    app.sysPath( "/tmp" );

    SECTION( "a missing file reads as 0" )
    {
        REQUIRE( app.readChannelCounts( "tip" ) == 0 );
    }

    SECTION( "writing to a missing directory fails" )
    {
        REQUIRE( app.writeChannelCounts( "tip", 5 ) == -1 );
    }

    SECTION( "counts round trip through the file" )
    {
        REQUIRE( ( ::mkdir( dir.c_str(), 0700 ) == 0 || errno == EEXIST ) );

        REQUIRE( app.writeChannelCounts( "tip", 12345 ) == 0 );
        REQUIRE( app.readChannelCounts( "tip" ) == 12345 );

        REQUIRE( app.writeChannelCounts( "tilt", -678 ) == 0 );
        REQUIRE( app.readChannelCounts( "tilt" ) == -678 );

        // Overwrite rather than append.
        REQUIRE( app.writeChannelCounts( "tip", 7 ) == 0 );
        REQUIRE( app.readChannelCounts( "tip" ) == 7 );

        std::ifstream fin( dir + "/tip" );
        std::string   contents;
        std::getline( fin, contents );
        REQUIRE( contents == "7" );

        ::unlink( ( dir + "/tip" ).c_str() );
        ::unlink( ( dir + "/tilt" ).c_str() );
        ::rmdir( dir.c_str() );
    }
}

/// Verify telemetry recording of the channel counts.
/**
 * \ingroup picoMotorCtrl_unit_test
 */
TEST_CASE( "picoMotorCtrl telemetry", "[picoMotorCtrl]" )
{
    // clang-format off
    #ifdef PICOMOTORCTRL_TEST_DOXYGEN_REF
    picoMotorCtrl::recordPico( true );
    picoMotorCtrl::recordTelem( static_cast<const telem_pico *>( nullptr ) );
    picoMotorCtrl::checkRecordTimes();
    #endif
    // clang-format on

    // Every app here uses the default 4 channels, which sizes the static count record in recordPico().
    picoMotorCtrl_test app( "pico" );
    REQUIRE( loadTwoMotors( app ) == 0 );

    SECTION( "a forced record is always made, then unchanged counts are not re-recorded" )
    {
        telem_pico::lastRecord = { 0, 0 };
        REQUIRE( app.recordPico( true ) == 0 );
        REQUIRE( telem_pico::lastRecord.tv_sec > 0 );

        telem_pico::lastRecord = { 0, 0 };
        REQUIRE( app.recordPico() == 0 );
        REQUIRE( telem_pico::lastRecord.tv_sec == 0 );
    }

    SECTION( "recordTelem forces a record" )
    {
        REQUIRE( app.recordPico( true ) == 0 );
        telem_pico::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const telem_pico *>( nullptr ) ) == 0 );
        REQUIRE( telem_pico::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records stale telemetry" )
    {
        telem_pico::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_pico::lastRecord.tv_sec > 0 );

        timespec last = telem_pico::lastRecord;
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( telem_pico::lastRecord.tv_sec == last.tv_sec );
        REQUIRE( telem_pico::lastRecord.tv_nsec == last.tv_nsec );
    }
}

} // namespace picoMotorCtrlTest

} // namespace libXWCTest
