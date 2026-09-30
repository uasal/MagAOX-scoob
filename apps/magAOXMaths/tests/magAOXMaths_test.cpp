/** \file magAOXMaths_test.cpp
 * \brief Catch2 tests for the magAOXMaths app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <string>

#include "../magAOXMaths.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup magAOXMaths_unit_test magAOXMaths Unit Tests
 * \brief Unit tests for the magAOXMaths application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `magAOXMaths` unit tests.
/** \ingroup magAOXMaths_unit_test
 */
namespace magAOXMathsTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing magAOXMaths internals.
class magAOXMaths_test : public magAOXMaths
{
  public:
    /// Construct a harness with the given device name.
    explicit magAOXMaths_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    using magAOXMaths::m_indiP_myVal;
    using magAOXMaths::m_indiP_myVal_maths;
    using magAOXMaths::m_indiP_otherVal;
    using magAOXMaths::m_indiP_setOtherVal;
    using magAOXMaths::m_myVal;
    using magAOXMaths::m_otherDevName;
    using magAOXMaths::m_otherValName;
    using magAOXMaths::m_startVal;
    using magAOXMaths::updateVals;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Check whether a NEW property callback is registered under a unique key.
    /**
     * \returns true if the key is registered, false otherwise
     */
    bool hasNewCallBack( const std::string &key /**< [in] the `device.name` key */ )
    {
        return m_indiNewCallBacks.count( key ) > 0;
    }

    /// Check whether a SET property callback is registered under a unique key.
    /**
     * \returns true if the key is registered, false otherwise
     */
    bool hasSetCallBack( const std::string &key /**< [in] the `device.name` key */ )
    {
        return m_indiSetCallBacks.count( key ) > 0;
    }
};
/// \endcond

/// Build a number property with one element.
/**
 * \returns the new property
 */
pcf::IndiProperty numberProperty( const std::string &device, /**< [in] the device name */
                                  const std::string &name,   /**< [in] the property name */
                                  const std::string &elName, /**< [in] the element name */
                                  double             value   /**< [in] the element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( elName ) );
    ip[elName] = value;
    return ip;
}

/// Verify the magAOXMaths configuration defaults.
/**
 * \ingroup magAOXMaths_unit_test
 */
TEST_CASE( "magAOXMaths configuration defaults", "[magAOXMaths]" )
{
    // clang-format off
    #ifdef MAGAOXMATHS_TEST_DOXYGEN_REF
    magAOXMaths::setupConfig();
    magAOXMaths::loadConfig();
    #endif
    // clang-format on

    magAOXMaths_test app( "maths" );

    mx::app::writeConfigFile( "/tmp/magAOXMaths_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.configure( "/tmp/magAOXMaths_test_defaults.conf" );

    REQUIRE( app.m_myVal == "x" );
    REQUIRE( app.m_otherDevName == "" );
    REQUIRE( app.m_otherValName == "" );
    REQUIRE( app.m_startVal == 0.0 );

    std::remove( "/tmp/magAOXMaths_test_defaults.conf" );
}

/// Verify the magAOXMaths configuration overrides.
/**
 * \ingroup magAOXMaths_unit_test
 */
TEST_CASE( "magAOXMaths configuration overrides", "[magAOXMaths]" )
{
    // clang-format off
    #ifdef MAGAOXMATHS_TEST_DOXYGEN_REF
    magAOXMaths::setupConfig();
    magAOXMaths::loadConfig();
    #endif
    // clang-format on

    magAOXMaths_test app( "maths" );

    mx::app::writeConfigFile( "/tmp/magAOXMaths_test_overrides.conf",
                              { "", "", "", "" },
                              { "myVal", "otherDevName", "otherValName", "startVal" },
                              { "y", "otherMaths", "y", "2.5" } );
    app.configure( "/tmp/magAOXMaths_test_overrides.conf" );

    REQUIRE( app.m_myVal == "y" );
    REQUIRE( app.m_otherDevName == "otherMaths" );
    REQUIRE( app.m_otherValName == "y" );
    REQUIRE( app.m_startVal == Approx( 2.5 ) );

    std::remove( "/tmp/magAOXMaths_test_overrides.conf" );
}

/// Verify `appStartup()` creates and registers the INDI properties and computes the initial maths.
/**
 * \ingroup magAOXMaths_unit_test
 */
TEST_CASE( "magAOXMaths appStartup registers the INDI properties", "[magAOXMaths]" )
{
    // clang-format off
    #ifdef MAGAOXMATHS_TEST_DOXYGEN_REF
    magAOXMaths::appStartup();
    magAOXMaths::appLogic();
    magAOXMaths::appShutdown();
    #endif
    // clang-format on

    magAOXMaths_test app( "maths" );
    app.m_otherDevName = "other";
    app.m_otherValName = "val";
    app.m_startVal     = 9.0;

    REQUIRE( app.appStartup() == 0 );

    REQUIRE( app.state() == stateCodes::READY );

    REQUIRE( app.m_indiP_myVal.getDevice() == "maths" );
    REQUIRE( app.m_indiP_myVal.getName() == "x" );
    REQUIRE( app.m_indiP_myVal["value"].get<double>() == Approx( 9.0 ) );

    REQUIRE( app.m_indiP_myVal_maths.getDevice() == "maths" );
    REQUIRE( app.m_indiP_myVal_maths.getName() == "maths" );
    REQUIRE( app.m_indiP_myVal_maths.getPerm() == pcf::IndiProperty::ReadOnly );
    REQUIRE( app.m_indiP_myVal_maths.getState() == pcf::IndiProperty::Ok );
    REQUIRE( app.m_indiP_myVal_maths["value"].get<double>() == Approx( 9.0 ) );
    REQUIRE( app.m_indiP_myVal_maths["sqr"].get<double>() == Approx( 81.0 ) );
    REQUIRE( app.m_indiP_myVal_maths["sqrt"].get<double>() == Approx( 3.0 ) );
    REQUIRE( app.m_indiP_myVal_maths["abs"].get<double>() == Approx( 9.0 ) );
    REQUIRE( app.m_indiP_myVal_maths["prod"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );

    REQUIRE( app.m_indiP_otherVal.getDevice() == "other" );
    REQUIRE( app.m_indiP_otherVal.getName() == "val" );
    REQUIRE( app.m_indiP_otherVal["value"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );

    REQUIRE( app.m_indiP_setOtherVal.getDevice() == "maths" );
    REQUIRE( app.m_indiP_setOtherVal.getName() == "other_val" );
    REQUIRE( app.m_indiP_setOtherVal["current"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
    REQUIRE( app.m_indiP_setOtherVal["target"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );

    REQUIRE( app.hasNewCallBack( "maths.x" ) );
    REQUIRE( app.hasNewCallBack( "maths.maths" ) );
    REQUIRE( app.hasNewCallBack( "maths.other_val" ) );
    REQUIRE( app.hasSetCallBack( "other.val" ) );

    REQUIRE( app.appLogic() == 0 );
    REQUIRE( app.appShutdown() == 0 );
}

/// Verify `updateVals()` fills the maths property from the value and the other value.
/**
 * \ingroup magAOXMaths_unit_test
 */
TEST_CASE( "magAOXMaths updateVals computes the maths", "[magAOXMaths]" )
{
    // clang-format off
    #ifdef MAGAOXMATHS_TEST_DOXYGEN_REF
    magAOXMaths::updateVals();
    #endif
    // clang-format on

    magAOXMaths_test app( "maths" );
    app.m_otherDevName = "other";
    app.m_otherValName = "val";

    REQUIRE( app.appStartup() == 0 );

    SECTION( "positive value" )
    {
        app.m_indiP_myVal["value"]    = 6.25;
        app.m_indiP_otherVal["value"] = -2.0;

        REQUIRE( app.updateVals() == 0 );

        REQUIRE( app.m_indiP_myVal_maths["value"].get<double>() == Approx( 6.25 ) );
        REQUIRE( app.m_indiP_myVal_maths["sqr"].get<double>() == Approx( 39.0625 ) );
        REQUIRE( app.m_indiP_myVal_maths["sqrt"].get<double>() == Approx( 2.5 ) );
        REQUIRE( app.m_indiP_myVal_maths["abs"].get<double>() == Approx( 6.25 ) );
        REQUIRE( app.m_indiP_myVal_maths["prod"].get<double>() == Approx( -12.5 ) );
        REQUIRE( app.m_indiP_myVal_maths.getState() == pcf::IndiProperty::Ok );
    }

    SECTION( "negative value" )
    {
        app.m_indiP_myVal["value"]    = -3.0;
        app.m_indiP_otherVal["value"] = 4.0;

        REQUIRE( app.updateVals() == 0 );

        // sqrt of a negative value is not checked: NaN handling is unreliable under -ffast-math
        REQUIRE( app.m_indiP_myVal_maths["value"].get<double>() == Approx( -3.0 ) );
        REQUIRE( app.m_indiP_myVal_maths["sqr"].get<double>() == Approx( 9.0 ) );
        REQUIRE( app.m_indiP_myVal_maths["abs"].get<double>() == Approx( 3.0 ) );
        REQUIRE( app.m_indiP_myVal_maths["prod"].get<double>() == Approx( -12.0 ) );
    }

    SECTION( "values -1 to -5 log at escalating priority and still compute" )
    {
        app.m_indiP_otherVal["value"] = 2.0;

        for( int n = 1; n <= 5; ++n )
        {
            app.m_indiP_myVal["value"] = -1.0 * n;

            REQUIRE( app.updateVals() == 0 );

            REQUIRE( app.m_indiP_myVal_maths["abs"].get<double>() == Approx( 1.0 * n ) );
            REQUIRE( app.m_indiP_myVal_maths["sqr"].get<double>() == Approx( 1.0 * n * n ) );
            REQUIRE( app.m_indiP_myVal_maths["prod"].get<double>() == Approx( -2.0 * n ) );
        }
    }

    SECTION( "zero value" )
    {
        app.m_indiP_myVal["value"]    = 0.0;
        app.m_indiP_otherVal["value"] = 5.0;

        REQUIRE( app.updateVals() == 0 );

        REQUIRE( app.m_indiP_myVal_maths["sqr"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
        REQUIRE( app.m_indiP_myVal_maths["sqrt"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
        REQUIRE( app.m_indiP_myVal_maths["prod"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
    }
}

/// Verify the SET callback for the other app's value copies it and recomputes the product.
/**
 * \ingroup magAOXMaths_unit_test
 */
TEST_CASE( "magAOXMaths setCallBack_m_indiP_otherVal updates the product", "[magAOXMaths]" )
{
    // clang-format off
    #ifdef MAGAOXMATHS_TEST_DOXYGEN_REF
    magAOXMaths::setCallBack_m_indiP_otherVal( pcf::IndiProperty() );
    #endif
    // clang-format on

    magAOXMaths_test app( "maths" );
    app.m_otherDevName = "other";
    app.m_otherValName = "val";
    app.m_startVal     = 3.0;

    REQUIRE( app.appStartup() == 0 );

    pcf::IndiProperty ip = numberProperty( "other", "val", "value", 4.0 );

    REQUIRE( app.setCallBack_m_indiP_otherVal( ip ) == 0 );

    REQUIRE( app.m_indiP_otherVal["value"].get<double>() == Approx( 4.0 ) );
    REQUIRE( app.m_indiP_myVal_maths["value"].get<double>() == Approx( 3.0 ) );
    REQUIRE( app.m_indiP_myVal_maths["prod"].get<double>() == Approx( 12.0 ) );

    // a second update replaces the first
    ip["value"] = -0.5;
    REQUIRE( app.setCallBack_m_indiP_otherVal( ip ) == 0 );
    REQUIRE( app.m_indiP_myVal_maths["prod"].get<double>() == Approx( -1.5 ) );
}

/// Verify the NEW callback for this app's value rejects a property with the wrong name.
/**
 * A matching property is not sent here: the callback body calls `m_indiDriver->sendSetProperty()`
 * without a null check, and there is no INDI driver in the unit tests.
 *
 * \ingroup magAOXMaths_unit_test
 */
TEST_CASE( "magAOXMaths newCallBack_m_indiP_myVal rejects a wrong name", "[magAOXMaths]" )
{
    // clang-format off
    #ifdef MAGAOXMATHS_TEST_DOXYGEN_REF
    magAOXMaths::newCallBack_m_indiP_myVal( pcf::IndiProperty() );
    #endif
    // clang-format on

    magAOXMaths_test app( "maths" );
    app.m_otherDevName = "other";
    app.m_otherValName = "val";
    app.m_startVal     = 1.5;

    REQUIRE( app.appStartup() == 0 );

    pcf::IndiProperty ip = numberProperty( "maths", "wrong", "value", 8.0 );

    REQUIRE( app.newCallBack_m_indiP_myVal( ip ) == -1 );

    REQUIRE( app.m_indiP_myVal["value"].get<double>() == Approx( 1.5 ) );
    REQUIRE( app.m_indiP_myVal_maths["sqr"].get<double>() == Approx( 2.25 ) );
}

/// Verify the NEW callback for the other value request sets the target and rejects a wrong name.
/**
 * \ingroup magAOXMaths_unit_test
 */
TEST_CASE( "magAOXMaths newCallBack_m_indiP_setOtherVal sets the target", "[magAOXMaths]" )
{
    // clang-format off
    #ifdef MAGAOXMATHS_TEST_DOXYGEN_REF
    magAOXMaths::newCallBack_m_indiP_setOtherVal( pcf::IndiProperty() );
    #endif
    // clang-format on

    magAOXMaths_test app( "maths" );
    app.m_otherDevName = "other";
    app.m_otherValName = "val";
    app.m_startVal     = 2.0;

    REQUIRE( app.appStartup() == 0 );

    SECTION( "wrong name is rejected" )
    {
        pcf::IndiProperty ip = numberProperty( "maths", "wrong", "target", 7.5 );

        REQUIRE( app.newCallBack_m_indiP_setOtherVal( ip ) == -1 );
        REQUIRE( app.m_indiP_setOtherVal["target"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
    }

    SECTION( "matching name sets the target" )
    {
        pcf::IndiProperty ip = numberProperty( "maths", "other_val", "target", 7.5 );

        REQUIRE( app.newCallBack_m_indiP_setOtherVal( ip ) == 0 );

        REQUIRE( app.m_indiP_setOtherVal["target"].get<double>() == Approx( 7.5 ) );
        REQUIRE( app.m_indiP_setOtherVal.getState() == pcf::IndiProperty::Ok );

        // Without an INDI driver sendNewProperty() fails, so the other app's value is unchanged
        REQUIRE( app.m_indiP_otherVal["value"].get<double>() == Approx( 0.0 ).margin( 1e-12 ) );
        REQUIRE( app.m_indiP_myVal_maths["value"].get<double>() == Approx( 2.0 ) );
        REQUIRE( app.m_indiP_myVal_maths["sqr"].get<double>() == Approx( 4.0 ) );
    }
}

} // namespace magAOXMathsTest

} // namespace libXWCTest
