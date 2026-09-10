/** \file wccSim_test.cpp
 * \brief Catch2 tests for the wccSim app.
 * \author Adam Schilperoort
 *
 * \ingroup wccSim_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdio>
#include <fstream>

#include "../wccSim.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup wccSim_unit_test wccSim Unit Tests
 * \brief Unit tests for the wccSim application.
 *
 * The simulation science itself is covered by \ref wccCommon_unit_test, which
 * pins it against astropy, prysm and the analytic Airy pattern. What is left to
 * test here is the application layer: configuration parsing, the sensor array it
 * builds, and the naming rules that connect a sensor to its INDI device and
 * output stream.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `wccSim` unit tests.
/** \ingroup wccSim_unit_test
 */
namespace wccSimTest
{

/// Expose the protected configuration surface of wccSim for testing.
/** \cond
 */
class wccSimTester : public wccSim
{
  public:
    /// Run loadConfigImpl against a configurator built by a test.
    int testLoadConfig( mx::app::appConfigurator &cfg )
    {
        return loadConfigImpl( cfg );
    }

    /// Number of sensors the configuration produced.
    size_t nSensors()
    {
        return m_sensors.size();
    }

    /// A sensor's logical name.
    std::string sensorName( size_t i )
    {
        return m_sensors[i]->m_name;
    }

    /// A sensor's INDI device.
    std::string sensorDevice( size_t i )
    {
        return m_sensors[i]->m_indiDevice;
    }

    /// A sensor's output stream name.
    std::string sensorStream( size_t i )
    {
        return m_sensors[i]->m_shmimOut;
    }

    /// A sensor's PSF bank key.
    std::string sensorBankKey( size_t i )
    {
        return m_sensors[i]->m_bankKey;
    }

    /// A sensor's RNG seed.
    uint64_t sensorSeed( size_t i )
    {
        return m_sensors[i]->m_seed;
    }

    /// The focal plane model the configuration built.
    const MagAOX::wcc::focalPlaneModel &focalPlane()
    {
        return m_focalPlane;
    }

    /// The parsed noise mode.
    MagAOX::wcc::noiseMode noise()
    {
        return m_noiseMode;
    }

    /// Expose the INDI property name sanitizer.
    static std::string safeName( const std::string &s )
    {
        return indiSafeName( s );
    }
};
/** \endcond
 */

/// Verify wccSim constructs and sanitizes INDI property names.
/**
 * \ingroup wccSim_unit_test
 */
TEST_CASE( "wccSim constructs and sanitizes sensor names for INDI", "[wccSim]" )
{
    // clang-format off
    #ifdef WCCSIM_TEST_DOXYGEN_REF
    wccSim();
    wccSim::setupConfig;
    wccSim::appStartup;
    wccSim::appLogic;
    wccSim::appShutdown;
    #endif
    // clang-format on

    SECTION( "default construction succeeds" )
    {
        wccSim app;

        REQUIRE( true );
    }

    SECTION( "sensor names become valid INDI property names" )
    {
        // INDI property names must not carry the hyphens the WCC sensor names use.
        REQUIRE( wccSimTester::safeName( "IMX-18" ) == "IMX_18" );
        REQUIRE( wccSimTester::safeName( "HWK-09" ) == "HWK_09" );
        REQUIRE( wccSimTester::safeName( "abc123" ) == "abc123" );
        REQUIRE( wccSimTester::safeName( "a.b c" ) == "a_b_c" );
        REQUIRE( wccSimTester::safeName( "" ).empty() );
    }
}

/// Verify the sensor array is built from configuration as documented.
/**
 * \ingroup wccSim_unit_test
 */
TEST_CASE( "wccSim builds its sensor array from configuration", "[wccSim]" )
{
    // clang-format off
    #ifdef WCCSIM_TEST_DOXYGEN_REF
    wccSim::loadSensorSection;
    wccSim::bankKey;
    #endif
    // clang-format on

    SECTION( "two sensors with different pitches need two PSF banks" )
    {
        wccSimTester app;
        mx::app::appConfigurator cfg;
        app.setupConfig();

        // Drive the configurator the way a config file would.
        std::vector<std::string> lines = { "[telescope]",
                                           "diameter=6.5",
                                           "f_number=12.0",
                                           "parity=-1",
                                           "[catalog]",
                                           "path=/dev/null",
                                           "[sim]",
                                           "sensors=IMX-18,HWK-09",
                                           "[IMX-18]",
                                           "indi_device=nsv18",
                                           "pixel_size=3.76",
                                           "full_w=9576",
                                           "full_h=6388",
                                           "field_x=-849.953",
                                           "field_y=-68.782",
                                           "[HWK-09]",
                                           "indi_device=hwk09",
                                           "shmim_out=hwk09custom",
                                           "pixel_size=4.6",
                                           "full_w=4096",
                                           "full_h=2300",
                                           "field_x=1029.51",
                                           "field_y=59.31",
                                           "rotation=23.5" };

        // appConfigurator reads an ini file, so write the lines to a temporary one.
        const std::string path = "/tmp/wccSim_test_config.conf";
        {
            std::ofstream fout( path );
            for( const std::string &l : lines )
            {
                fout << l << "\n";
            }
        }

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) == 0 );

        REQUIRE( app.nSensors() == 2 );
        REQUIRE( app.sensorName( 0 ) == "IMX-18" );
        REQUIRE( app.sensorName( 1 ) == "HWK-09" );
        REQUIRE( app.sensorDevice( 0 ) == "nsv18" );

        // The output stream defaults to the device name with a sim suffix, and is
        // overridable per sensor.
        REQUIRE( app.sensorStream( 0 ) == "nsv18sim" );
        REQUIRE( app.sensorStream( 1 ) == "hwk09custom" );

        // Different pixel pitches must not share a PSF bank.
        REQUIRE( app.sensorBankKey( 0 ) != app.sensorBankKey( 1 ) );

        // Each sensor gets its own noise seed so the detectors are independent.
        REQUIRE( app.sensorSeed( 0 ) != app.sensorSeed( 1 ) );

        // The geometry reached the focal plane model.
        REQUIRE( app.focalPlane().nSensors() == 2 );
        REQUIRE( app.focalPlane().sensor( 0 ).m_fieldX == Approx( -849.953 ).epsilon( 1e-12 ) );
        REQUIRE( app.focalPlane().sensor( 1 ).m_rotation == Approx( 23.5 ).epsilon( 1e-12 ) );
        REQUIRE( app.focalPlane().sensor( 1 ).m_pixelSize == Approx( 4.6 ).epsilon( 1e-12 ) );

        std::remove( path.c_str() );
    }

    SECTION( "identical sensors share one PSF bank" )
    {
        wccSimTester app;
        mx::app::appConfigurator cfg;
        app.setupConfig();

        const std::string path = "/tmp/wccSim_test_config_same.conf";
        {
            std::ofstream fout( path );
            fout << "[catalog]\npath=/dev/null\n";
            fout << "[sim]\nsensors=A,B\n";
            fout << "[A]\nindi_device=a\npixel_size=3.76\nfull_w=512\nfull_h=512\nfield_x=0\nfield_y=0\n";
            fout << "[B]\nindi_device=b\npixel_size=3.76\nfull_w=512\nfull_h=512\nfield_x=100\nfield_y=0\n";
        }

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) == 0 );

        // Sharing a bank is what keeps startup short across an array of identical
        // detectors, so it must not regress.
        REQUIRE( app.sensorBankKey( 0 ) == app.sensorBankKey( 1 ) );

        std::remove( path.c_str() );
    }

    SECTION( "a sensor with no indi_device is rejected" )
    {
        wccSimTester app;
        mx::app::appConfigurator cfg;
        app.setupConfig();

        const std::string path = "/tmp/wccSim_test_config_bad.conf";
        {
            std::ofstream fout( path );
            fout << "[catalog]\npath=/dev/null\n";
            fout << "[sim]\nsensors=A\n";
            fout << "[A]\npixel_size=3.76\nfull_w=512\nfull_h=512\n";
        }

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) < 0 );

        std::remove( path.c_str() );
    }

    SECTION( "an empty sensor list is rejected" )
    {
        wccSimTester app;
        mx::app::appConfigurator cfg;
        app.setupConfig();

        const std::string path = "/tmp/wccSim_test_config_empty.conf";
        {
            std::ofstream fout( path );
            fout << "[catalog]\npath=/dev/null\n";
        }

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) < 0 );

        std::remove( path.c_str() );
    }

    SECTION( "the noise mode is parsed from configuration" )
    {
        wccSimTester app;
        mx::app::appConfigurator cfg;
        app.setupConfig();

        const std::string path = "/tmp/wccSim_test_config_noise.conf";
        {
            std::ofstream fout( path );
            fout << "[catalog]\npath=/dev/null\n";
            fout << "[sim]\nsensors=A\nnoise_mode=read\n";
            fout << "[A]\nindi_device=a\npixel_size=3.76\nfull_w=512\nfull_h=512\n";
        }

        cfg.readConfig( path );

        REQUIRE( app.testLoadConfig( cfg ) == 0 );
        REQUIRE( app.noise() == MagAOX::wcc::noiseMode::read );

        std::remove( path.c_str() );
    }
}

} // namespace wccSimTest

} // namespace libXWCTest
