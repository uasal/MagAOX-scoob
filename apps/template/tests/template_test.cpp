/** \file template_test.cpp
 * \brief Catch2 tests for the template app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * This is the starting point for the unit tests of a new app.  Copy it with the rest of the `template`
 * directory and replace `template`/`TEMPLATE` with the new app name (see README.md).
 *
 * \ingroup template_files
 */

#include "../../../tests/testXWC.hpp"

// For INDI callback validation tests, include this before the app header so that
// INDI_VALIDATE_CALLBACK_PROPS returns 0 after a successful device/name check:
// #include "../../../tests/testMacrosINDI.hpp"

#include <cstdio>
#include <string>

#include "../template.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup template_unit_test template Unit Tests
 * \brief Unit tests for the template application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `template` unit tests.
/** \ingroup template_unit_test
 */
namespace templateTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing template internals.
/** The INDI test macros in `tests/testMacrosINDI.hpp` require a harness named `<app>_test` that is
 * constructible from a device name.
 */
class template_test : public MagAOX::app::template
{
  public:
    /// Construct a harness with the given device name.
    explicit template_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // Set the device and name of each NEW property here, e.g.
        // XWCTEST_SETUP_INDI_NEW_PROP( myProp );
    }

    // Expose protected members for the tests with using declarations, e.g.
    // using MagAOX::app::template::m_myParam;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Run `loadConfigImpl()` on the app's configurator.
    /**
     * \returns the value returned by `loadConfigImpl()`
     */
    int loadConfigImplOnConfig()
    {
        return loadConfigImpl( config );
    }

    /// Get the shutdown flag.
    /**
     * \returns the current value of `m_shutdown`
     */
    int shutdownFlag()
    {
        return m_shutdown;
    }
};
/// \endcond

/// Verify the template configuration defaults.
/**
 * \ingroup template_unit_test
 */
TEST_CASE( "template configuration defaults", "[template]" )
{
    // clang-format off
    #ifdef TEMPLATE_TEST_DOXYGEN_REF
    template::setupConfig();
    template::loadConfig();
    template::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    template_test app( "template" );

    mx::app::writeConfigFile( "/tmp/template_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.configure( "/tmp/template_test_defaults.conf" );

    REQUIRE( app.loadConfigImplOnConfig() == 0 );
    REQUIRE( app.shutdownFlag() == 0 );

    // Check the default of each configurable parameter here, e.g.
    // REQUIRE( app.m_myParam == 1.0 );

    std::remove( "/tmp/template_test_defaults.conf" );
}

/// Verify the template configuration overrides.
/**
 * \ingroup template_unit_test
 */
TEST_CASE( "template configuration overrides", "[template]" )
{
    // clang-format off
    #ifdef TEMPLATE_TEST_DOXYGEN_REF
    template::setupConfig();
    template::loadConfig();
    #endif
    // clang-format on

    template_test app( "template" );

    // One section/keyword/value per configurable parameter, e.g. { "mySection" }, { "myParam" }, { "2.5" }
    mx::app::writeConfigFile( "/tmp/template_test_overrides.conf", { "template" }, { "example" }, { "1" } );
    app.configure( "/tmp/template_test_overrides.conf" );

    REQUIRE( app.loadConfigImplOnConfig() == 0 );
    REQUIRE( app.shutdownFlag() == 0 );

    // Check that each parameter took the value in the file here, e.g.
    // REQUIRE( app.m_myParam == Approx( 2.5 ) );

    std::remove( "/tmp/template_test_overrides.conf" );
}

/// Verify the template INDI device name and the application lifecycle functions.
/**
 * \ingroup template_unit_test
 */
TEST_CASE( "template INDI device and lifecycle", "[template]" )
{
    // clang-format off
    #ifdef TEMPLATE_TEST_DOXYGEN_REF
    template::appStartup();
    template::appLogic();
    template::appShutdown();
    #endif
    // clang-format on

    template_test app( "right" );

    REQUIRE( app.configName() == "right" );

    REQUIRE( app.appStartup() == 0 );
    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.appShutdown() == 0 );
}

// Add INDI callback validation tests once the app has NEW properties, e.g.
// (with testMacrosINDI.hpp included before the app header):
//
// SCENARIO( "template INDI callbacks", "[template]" )
// {
//     XWCTEST_INDI_NEW_CALLBACK( template, myProp );
// }

} // namespace templateTest

} // namespace libXWCTest
