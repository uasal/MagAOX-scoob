/** \file ttmModulator_test.cpp
 * \brief Catch2 tests for the ttmModulator app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * The callback bodies are live in this translation unit (testMacrosINDI.hpp is not included).  The device/name
 * validation tests are in ttmModulator_indi_test.cpp.
 */

#include "../../../tests/testXWC.hpp"

#include <fcntl.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../ttmModulator.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup ttmModulator_unit_test ttmModulator Unit Tests
 * \brief Unit tests for the ttmModulator application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `ttmModulator` unit tests.
/** \ingroup ttmModulator_unit_test
 */
namespace ttmModulatorTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

class fakeFxnGen;

/// Test harness exposing ttmModulator internals.
class ttmModulator_test : public ttmModulator
{
  public:
    /// The fake function generator installed as the INDI driver, owned (and deleted) by MagAOXApp.
    fakeFxnGen *m_fxngen{ nullptr };

    /// Construct a harness with the given device name.
    explicit ttmModulator_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    using ttmModulator::m_calC1Amps;
    using ttmModulator::m_calC2Amps;
    using ttmModulator::m_calC2Phse;
    using ttmModulator::m_calFreqs;
    using ttmModulator::m_calRadius;
    using ttmModulator::m_maxFreq;
    using ttmModulator::m_maxVolt;
    using ttmModulator::m_modDFreq;
    using ttmModulator::m_modDVolts;
    using ttmModulator::m_modFreq;
    using ttmModulator::m_modFreqRequested;
    using ttmModulator::m_modRad;
    using ttmModulator::m_modRadRequested;
    using ttmModulator::m_modState;
    using ttmModulator::m_modStateRequested;
    using ttmModulator::m_rotAngle;
    using ttmModulator::m_rotParity;
    using ttmModulator::m_setDVolts;
    using ttmModulator::m_setVoltage_1;
    using ttmModulator::m_setVoltage_2;

    using ttmModulator::m_C1freq;
    using ttmModulator::m_C1ofst;
    using ttmModulator::m_C1outp;
    using ttmModulator::m_C1phse;
    using ttmModulator::m_C1volts;
    using ttmModulator::m_C2freq;
    using ttmModulator::m_C2ofst;
    using ttmModulator::m_C2outp;
    using ttmModulator::m_C2phse;
    using ttmModulator::m_C2volts;

    using ttmModulator::m_indiP_C1freq;
    using ttmModulator::m_indiP_C1ofst;
    using ttmModulator::m_indiP_C1outp;
    using ttmModulator::m_indiP_C1phse;
    using ttmModulator::m_indiP_C1volts;
    using ttmModulator::m_indiP_C2freq;
    using ttmModulator::m_indiP_C2ofst;
    using ttmModulator::m_indiP_C2outp;
    using ttmModulator::m_indiP_C2phse;
    using ttmModulator::m_indiP_C2volts;
    using ttmModulator::m_indiP_modFrequency;
    using ttmModulator::m_indiP_modRadius;
    using ttmModulator::m_indiP_modState;
    using ttmModulator::m_indiP_offset;
    using ttmModulator::m_indiP_offset12;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Create the INDI properties the way `appStartup()` does, without registering them.
    void setupProperties()
    {
        newProp( m_indiP_modState, m_configName, "modState", { "current", "target" } );
        newProp( m_indiP_modFrequency, m_configName, "modFrequency", { "current", "target" } );
        newProp( m_indiP_modRadius, m_configName, "modRadius", { "current", "target" } );
        newProp( m_indiP_offset12, m_configName, "offset12", { "dC1", "dC2" } );
        newProp( m_indiP_offset, m_configName, "offset", { "x", "y" } );

        newProp( m_indiP_C1outp, "fxngenmodwfs", "C1outp", { "value" } );
        newProp( m_indiP_C1freq, "fxngenmodwfs", "C1freq", { "current", "target" } );
        newProp( m_indiP_C1volts, "fxngenmodwfs", "C1amp", { "current", "target" } );
        newProp( m_indiP_C1ofst, "fxngenmodwfs", "C1ofst", { "value" } );
        newProp( m_indiP_C1phse, "fxngenmodwfs", "C1phse", { "value" } );

        newProp( m_indiP_C2outp, "fxngenmodwfs", "C2outp", { "value" } );
        newProp( m_indiP_C2freq, "fxngenmodwfs", "C2freq", { "current", "target" } );
        newProp( m_indiP_C2volts, "fxngenmodwfs", "C2amp", { "current", "target" } );
        newProp( m_indiP_C2ofst, "fxngenmodwfs", "C2ofst", { "value" } );
        newProp( m_indiP_C2phse, "fxngenmodwfs", "C2phse", { "value" } );
    }

    /// Set both function generator channels to the same parameters.
    void setChannels( int    outp,  /**< [in] output state (0 off, 1 on) */
                      double freq,  /**< [in] frequency [Hz] */
                      double volts, /**< [in] amplitude [V] */
                      double ofst,  /**< [in] DC offset [V] */
                      double phse   /**< [in] phase [deg] */
    )
    {
        m_C1outp  = outp;
        m_C1freq  = freq;
        m_C1volts = volts;
        m_C1ofst  = ofst;
        m_C1phse  = phse;

        m_C2outp  = outp;
        m_C2freq  = freq;
        m_C2volts = volts;
        m_C2ofst  = ofst;
        m_C2phse  = phse;
    }

    /// Put the function generator in the state `restTTM()` leaves it in.
    void setRested()
    {
        setChannels( 0, 0.0, 0.002, 0.001, 0.0 );
    }

    /// Put the function generator in the state `setTTM()` leaves it in (with 1 V set voltages).
    void setSet()
    {
        setChannels( 1, 0.0, 0.002, 1.0, 0.0 );
    }

    /// Use 1 V set voltages with a 1 V step, so `setTTM()` has no ramp steps and does not sleep.
    void useFastSetting()
    {
        m_setVoltage_1 = 1.0;
        m_setVoltage_2 = 1.0;
        m_setDVolts    = 1.0;
    }

    /// Install a fake function generator as the INDI driver.
    void installFxnGen();

  protected:
    /// Set up a Number property with the given device, name and elements.
    void newProp( pcf::IndiProperty              &prop,   /**< [out] the property to set up */
                  const std::string              &device, /**< [in] the device name */
                  const std::string              &name,   /**< [in] the property name */
                  const std::vector<std::string> &els     /**< [in] the element names */
    )
    {
        prop = pcf::IndiProperty( pcf::IndiProperty::Number );
        prop.setDevice( device );
        prop.setName( name );
        for( auto &el : els )
        {
            prop.add( pcf::IndiElement( el ) );
        }
    }
};

/// Fake function generator: an INDI driver whose `sendNewProperty()` applies each request immediately.
/** Each request is recorded, and then answered by calling the matching ttmModulator set callback with the value
 * the fxngen would report: amplitudes below 0.002 V report 0.002 V, and offsets below 0.001 V report 0.001 V.
 * Output fds point at /dev/null so INDI set-property messages are discarded.
 */
class fakeFxnGen : public indiDriver<MagAOXApp<true>>
{
  public:
    /// The app being driven.
    ttmModulator_test *m_app{ nullptr };

    /// If true, `sendNewProperty()` fails without applying the request.
    bool m_fail{ false };

    /// Every property sent, in order.
    std::vector<pcf::IndiProperty> m_sent;

    /// Construct the fake for the given app.
    explicit fakeFxnGen( ttmModulator_test *app /**< [in] the app being driven */ )
        : indiDriver<MagAOXApp<true>>( app, "fakeFxnGen", "0", "0" ), m_app( app )
    {
        int fd = open( "/dev/null", O_WRONLY );
        if( fd >= 0 )
        {
            setOutputFd( fd );
        }
    }

    /// Record and apply a new-property request.
    /**
     * \returns 0 on success, -1 if `m_fail` is set or the property is unknown
     */
    virtual int sendNewProperty( const pcf::IndiProperty &ip /**< [in] the property sent */ )
    {
        m_sent.push_back( ip );

        if( m_fail )
        {
            return -1;
        }

        pcf::IndiProperty  resp = ip;
        const std::string &name = ip.getName();

        if( name == "C1freq" || name == "C2freq" )
        {
            resp["current"].set( ip["target"].get<double>() );
        }
        else if( name == "C1amp" || name == "C2amp" )
        {
            double v = ip["target"].get<double>();
            if( v < 0.002 )
            {
                v = 0.002;
            }
            resp["current"].set( v );
        }
        else if( name == "C1ofst" || name == "C2ofst" )
        {
            double v = ip["value"].get<double>();
            if( v < 0.001 )
            {
                v = 0.001;
            }
            resp["value"].set( v );
        }

        if( name == "C1outp" )
            return m_app->setCallBack_m_indiP_C1outp( resp );
        if( name == "C1freq" )
            return m_app->setCallBack_m_indiP_C1freq( resp );
        if( name == "C1amp" )
            return m_app->setCallBack_m_indiP_C1volts( resp );
        if( name == "C1ofst" )
            return m_app->setCallBack_m_indiP_C1ofst( resp );
        if( name == "C1phse" )
            return m_app->setCallBack_m_indiP_C1phse( resp );
        if( name == "C2outp" )
            return m_app->setCallBack_m_indiP_C2outp( resp );
        if( name == "C2freq" )
            return m_app->setCallBack_m_indiP_C2freq( resp );
        if( name == "C2amp" )
            return m_app->setCallBack_m_indiP_C2volts( resp );
        if( name == "C2ofst" )
            return m_app->setCallBack_m_indiP_C2ofst( resp );
        if( name == "C2phse" )
            return m_app->setCallBack_m_indiP_C2phse( resp );

        return -1;
    }

    /// Count the requests sent for a property.
    /**
     * \returns the number of requests for `name`
     */
    size_t count( const std::string &name /**< [in] the property name */ ) const
    {
        size_t n = 0;
        for( auto &ip : m_sent )
        {
            if( ip.getName() == name )
            {
                ++n;
            }
        }
        return n;
    }

    /// Get the value of an element in the last request sent for a property.
    /**
     * \returns the element value as a string, or "" if no request was sent
     */
    std::string last( const std::string &name, /**< [in] the property name */
                      const std::string &el    /**< [in] the element name */
    ) const
    {
        for( auto it = m_sent.rbegin(); it != m_sent.rend(); ++it )
        {
            if( it->getName() == name )
            {
                return ( *it )[el].getValue();
            }
        }
        return "";
    }
};

void ttmModulator_test::installFxnGen()
{
    m_fxngen     = new fakeFxnGen( this );
    m_indiDriver = m_fxngen;
}

/// Build a Number property with a single element.
/**
 * \returns the INDI property
 */
template <typename T>
pcf::IndiProperty numberProp( const std::string &device, /**< [in] INDI device name */
                              const std::string &name,   /**< [in] INDI property name */
                              const std::string &el,     /**< [in] element name */
                              const T           &value   /**< [in] element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el ) );
    ip[el].set( value );
    return ip;
}

/// Build a Number property with two elements.
/**
 * \returns the INDI property
 */
pcf::IndiProperty numberProp2( const std::string &device, /**< [in] INDI device name */
                               const std::string &name,   /**< [in] INDI property name */
                               const std::string &el1,    /**< [in] first element name */
                               double             value1, /**< [in] first element value */
                               const std::string &el2,    /**< [in] second element name */
                               double             value2  /**< [in] second element value */
)
{
    pcf::IndiProperty ip = numberProp( device, name, el1, value1 );
    ip.add( pcf::IndiElement( el2 ) );
    ip[el2].set( value2 );
    return ip;
}

/// Build a Text property with a single element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty textProp( const std::string &device, /**< [in] INDI device name */
                            const std::string &name,   /**< [in] INDI property name */
                            const std::string &el,     /**< [in] element name */
                            const std::string &value   /**< [in] element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Text );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, value ) );
    return ip;
}

/// \endcond

/// Verify the ttmModulator configuration defaults and overrides.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator configuration", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::setupConfig();
    ttmModulator::loadConfig();
    #endif
    // clang-format on

    const std::string fname = "/tmp/ttmModulator_test_config.conf";

    SECTION( "defaults" )
    {
        mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

        ttmModulator_test app( "ttmmod" );
        app.configure( fname );

        REQUIRE( app.m_maxFreq == Approx( 3000.0 ) );
        REQUIRE( app.m_maxVolt == Approx( 1.2801 ) );
        REQUIRE( app.m_setVoltage_1 == Approx( 5.0 ) );
        REQUIRE( app.m_setVoltage_2 == Approx( 5.0 ) );
        REQUIRE( app.m_setDVolts == Approx( 1.0 ) );
        REQUIRE( app.m_modDFreq == Approx( 500.0 ) );
        REQUIRE( app.m_modDVolts == Approx( 0.5 ) );
        REQUIRE( app.m_rotAngle == 0 );
        REQUIRE( app.m_rotParity == 1 );
        REQUIRE( app.m_modState == MODSTATE_UNKNOWN );
        REQUIRE( app.m_modStateRequested == MODSTATE_UNKNOWN );
        REQUIRE( app.m_calFreqs.size() == app.m_calC1Amps.size() );
        REQUIRE( app.m_calFreqs.size() == app.m_calC2Amps.size() );
        REQUIRE( app.m_calFreqs.size() == app.m_calC2Phse.size() );
    }

    SECTION( "overrides, with the rotation angle converted to radians" )
    {
        mx::app::writeConfigFile(
            fname,
            { "limits", "cal", "cal", "cal", "cal", "cal", "cal", "cal" },
            { "maxfreq", "setv1", "setv2", "setDvolts", "modDfreq", "modDvolts", "rotAngle", "rotParity" },
            { "2000", "4.5", "5.5", "0.5", "250", "0.25", "90", "-1" } );

        ttmModulator_test app( "ttmmod" );
        app.configure( fname );

        REQUIRE( app.m_maxFreq == Approx( 2000.0 ) );
        REQUIRE( app.m_setVoltage_1 == Approx( 4.5 ) );
        REQUIRE( app.m_setVoltage_2 == Approx( 5.5 ) );
        REQUIRE( app.m_setDVolts == Approx( 0.5 ) );
        REQUIRE( app.m_modDFreq == Approx( 250.0 ) );
        REQUIRE( app.m_modDVolts == Approx( 0.25 ) );
        REQUIRE( app.m_rotAngle == Approx( 90.0 * 3.14159 / 180.0 ) );
        REQUIRE( app.m_rotParity == -1 );
    }

    SECTION( "rotParity is normalized to +/-1" )
    {
        mx::app::writeConfigFile( fname, { "cal" }, { "rotParity" }, { "-3.5" } );
        {
            ttmModulator_test app( "ttmmod" );
            app.configure( fname );
            REQUIRE( app.m_rotParity == -1 );
        }

        mx::app::writeConfigFile( fname, { "cal" }, { "rotParity" }, { "7" } );
        {
            ttmModulator_test app( "ttmmod" );
            app.configure( fname );
            REQUIRE( app.m_rotParity == 1 );
        }

        mx::app::writeConfigFile( fname, { "cal" }, { "rotParity" }, { "0" } );
        {
            ttmModulator_test app( "ttmmod" );
            app.configure( fname );
            REQUIRE( app.m_rotParity == 1 );
        }
    }

    remove( fname.c_str() );
}

/// Verify the modulator state calculated from the function generator parameters.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator calcState", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::calcState();
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );

    SECTION( "unknown outputs are rest" )
    {
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );
    }

    SECTION( "either output off is rest" )
    {
        app.setChannels( 1, 100, 0.5, 5, 0 );
        app.m_C2outp = 0;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );

        app.m_C2outp = 1;
        app.m_C1outp = 0;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );
    }

    SECTION( "outputs on with no modulation and zero phase is set" )
    {
        app.setChannels( 1, 0, 0.002, 5, 0 );
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_SET );

        // Zero frequency with a non-zero amplitude, or minimum amplitude with a non-zero frequency, is also set
        app.m_C1volts = 0.5;
        app.m_C2freq  = 100;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_SET );
    }

    SECTION( "no modulation but non-zero phase is midset" )
    {
        app.setChannels( 1, 0, 0.002, 5, 0 );
        app.m_C2phse = 74;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MIDSET );
    }

    SECTION( "different channel frequencies are midset" )
    {
        app.setChannels( 1, 100, 0.2, 5, 0 );
        app.m_C2freq = 200;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MIDSET );

        // Only one channel modulating
        app.m_C2freq  = 0;
        app.m_C2volts = 0.002;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MIDSET );
    }

    SECTION( "modulating at a calibration frequency" )
    {
        app.setChannels( 1, 100, 0.155, 5, 0 );
        app.m_C2phse = 74;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modFreq == Approx( 100.0 ) );
        REQUIRE( app.m_modRad == Approx( 0.155 / 0.310 ) );
    }

    SECTION( "modulating between calibration frequencies interpolates" )
    {
        // 375 Hz is half way between 250 Hz (0.317 V) and 500 Hz (0.327 V)
        app.setChannels( 1, 375, 0.161, 5, 0 );
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modFreq == Approx( 375.0 ) );
        REQUIRE( app.m_modRad == Approx( 0.161 / 0.322 ) );
    }

    SECTION( "modulating below the first calibration frequency uses the first point" )
    {
        app.setChannels( 1, 50, 0.31, 5, 0 );
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modRad == Approx( 1.0 ) );
    }

    SECTION( "modulating at the last calibration frequency" )
    {
        app.setChannels( 1, 3000, 0.435, 5, 0 );
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modRad == Approx( 1.0 ) );
    }

    SECTION( "the radius scales with the calibration radius" )
    {
        app.m_calRadius = 2.0;
        app.setChannels( 1, 100, 0.155, 5, 0 );
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modRad == Approx( 2.0 * 0.155 / 0.310 ) );
    }
}

/// Verify the waitValue helpers.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator waitValue", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    waitValue( 0, 0 );
    waitValue( 0.0, 0.0, 1e-6 );
    nanoSleep( 0 );
    #endif
    // clang-format on

    SECTION( "exact match" )
    {
        int    i = 3;
        double d = 1.5;

        REQUIRE( MagAOX::app::waitValue( i, 3 ) == 0 );
        REQUIRE( MagAOX::app::waitValue( d, 1.5 ) == 0 );

        // A mismatch times out (1 ms timeout, 0.1 ms pauses)
        REQUIRE( MagAOX::app::waitValue( i, 4, 1000000UL, 100000UL ) == -1 );
        REQUIRE( MagAOX::app::waitValue( d, 1.5000001, 1000000UL, 100000UL ) == -1 );
    }

    SECTION( "with tolerance" )
    {
        double d = 0.002;

        REQUIRE( MagAOX::app::waitValue( d, 0.0020005, 1e-6, 1000000UL, 100000UL ) == 0 );
        REQUIRE( MagAOX::app::waitValue( d, 0.0020005, 1e-3, 1000000UL, 100000UL ) == 0 );
        REQUIRE( MagAOX::app::waitValue( d, 0.0020005, 1e-7, 1000000UL, 100000UL ) == -1 );
        REQUIRE( MagAOX::app::waitValue( d, 0.001, 1e-6, 1000000UL, 100000UL ) == -1 );
    }
}

/// Verify the new-property callbacks for the requested state, radius and frequency.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator modulation request callbacks", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::newCallBack_m_indiP_modState( pcf::IndiProperty() );
    ttmModulator::newCallBack_m_indiP_modRadius( pcf::IndiProperty() );
    ttmModulator::newCallBack_m_indiP_modFrequency( pcf::IndiProperty() );
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );
    app.setupProperties();

    SECTION( "modState" )
    {
        REQUIRE( app.newCallBack_m_indiP_modState( numberProp( "ttmmod", "modState", "target", 3 ) ) == 0 );
        REQUIRE( app.m_modStateRequested == MODSTATE_SET );

        // current is used if there is no target
        REQUIRE( app.newCallBack_m_indiP_modState( numberProp( "ttmmod", "modState", "current", 4 ) ) == 0 );
        REQUIRE( app.m_modStateRequested == MODSTATE_MODULATING );

        REQUIRE( app.newCallBack_m_indiP_modState( numberProp( "ttmmod", "modState", "other", 1 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_modState( numberProp( "wrong", "modState", "target", 1 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_modState( numberProp( "ttmmod", "wrong", "target", 1 ) ) == -1 );
        REQUIRE( app.m_modStateRequested == MODSTATE_MODULATING );
    }

    SECTION( "modRadius accepts only positive values" )
    {
        REQUIRE( app.newCallBack_m_indiP_modRadius( numberProp( "ttmmod", "modRadius", "target", 1.5 ) ) == 0 );
        REQUIRE( app.m_modRadRequested == Approx( 1.5 ) );

        REQUIRE( app.newCallBack_m_indiP_modRadius( numberProp( "ttmmod", "modRadius", "target", 0.0 ) ) == 0 );
        REQUIRE( app.m_modRadRequested == Approx( 1.5 ) );

        REQUIRE( app.newCallBack_m_indiP_modRadius( numberProp( "ttmmod", "modRadius", "target", -2.0 ) ) == 0 );
        REQUIRE( app.m_modRadRequested == Approx( 1.5 ) );

        REQUIRE( app.newCallBack_m_indiP_modRadius( numberProp( "ttmmod", "modRadius", "other", 3.0 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_modRadius( numberProp( "wrong", "modRadius", "target", 3.0 ) ) == -1 );
        REQUIRE( app.m_modRadRequested == Approx( 1.5 ) );
    }

    SECTION( "modFrequency accepts only positive values" )
    {
        REQUIRE( app.newCallBack_m_indiP_modFrequency( numberProp( "ttmmod", "modFrequency", "target", 1000.0 ) ) ==
                 0 );
        REQUIRE( app.m_modFreqRequested == Approx( 1000.0 ) );

        REQUIRE( app.newCallBack_m_indiP_modFrequency( numberProp( "ttmmod", "modFrequency", "target", 0.0 ) ) == 0 );
        REQUIRE( app.m_modFreqRequested == Approx( 1000.0 ) );

        REQUIRE( app.newCallBack_m_indiP_modFrequency( numberProp( "ttmmod", "modFrequency", "other", 5.0 ) ) == -1 );
        REQUIRE( app.newCallBack_m_indiP_modFrequency( numberProp( "ttmmod", "wrong", "target", 5.0 ) ) == -1 );
        REQUIRE( app.m_modFreqRequested == Approx( 1000.0 ) );
    }
}

/// Verify the function generator set callbacks.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator function generator callbacks", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::setCallBack_m_indiP_C1outp( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1freq( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1volts( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1ofst( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1phse( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2outp( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2freq( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2volts( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2ofst( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2phse( pcf::IndiProperty() );
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );
    app.setupProperties();

    const std::string fg = "fxngenmodwfs";

    SECTION( "output states" )
    {
        REQUIRE( app.setCallBack_m_indiP_C1outp( textProp( fg, "C1outp", "value", "On" ) ) == 0 );
        REQUIRE( app.m_C1outp == 1 );
        REQUIRE( app.setCallBack_m_indiP_C1outp( textProp( fg, "C1outp", "value", "Off" ) ) == 0 );
        REQUIRE( app.m_C1outp == 0 );
        REQUIRE( app.setCallBack_m_indiP_C1outp( textProp( fg, "C1outp", "value", "Maybe" ) ) == 0 );
        REQUIRE( app.m_C1outp == -1 );

        REQUIRE( app.setCallBack_m_indiP_C2outp( textProp( fg, "C2outp", "value", "On" ) ) == 0 );
        REQUIRE( app.m_C2outp == 1 );
        REQUIRE( app.setCallBack_m_indiP_C2outp( textProp( fg, "C2outp", "value", "Off" ) ) == 0 );
        REQUIRE( app.m_C2outp == 0 );
        REQUIRE( app.setCallBack_m_indiP_C2outp( textProp( fg, "C2outp", "value", "" ) ) == 0 );
        REQUIRE( app.m_C2outp == -1 );

        // missing element
        REQUIRE( app.setCallBack_m_indiP_C1outp( textProp( fg, "C1outp", "other", "On" ) ) == -1 );
    }

    SECTION( "frequencies and amplitudes use current" )
    {
        REQUIRE( app.setCallBack_m_indiP_C1freq( numberProp( fg, "C1freq", "current", 250.0 ) ) == 0 );
        REQUIRE( app.m_C1freq == Approx( 250.0 ) );
        REQUIRE( app.setCallBack_m_indiP_C2freq( numberProp( fg, "C2freq", "current", 500.0 ) ) == 0 );
        REQUIRE( app.m_C2freq == Approx( 500.0 ) );

        REQUIRE( app.setCallBack_m_indiP_C1volts( numberProp( fg, "C1amp", "current", 0.25 ) ) == 0 );
        REQUIRE( app.m_C1volts == Approx( 0.25 ) );
        REQUIRE( app.setCallBack_m_indiP_C2volts( numberProp( fg, "C2amp", "current", 0.35 ) ) == 0 );
        REQUIRE( app.m_C2volts == Approx( 0.35 ) );

        // target only is not accepted
        REQUIRE( app.setCallBack_m_indiP_C1freq( numberProp( fg, "C1freq", "target", 100.0 ) ) == -1 );
        REQUIRE( app.setCallBack_m_indiP_C2volts( numberProp( fg, "C2amp", "target", 0.1 ) ) == -1 );
        REQUIRE( app.m_C1freq == Approx( 250.0 ) );
        REQUIRE( app.m_C2volts == Approx( 0.35 ) );
    }

    SECTION( "offsets and phases use value" )
    {
        REQUIRE( app.setCallBack_m_indiP_C1ofst( numberProp( fg, "C1ofst", "value", 4.5 ) ) == 0 );
        REQUIRE( app.m_C1ofst == Approx( 4.5 ) );
        REQUIRE( app.setCallBack_m_indiP_C2ofst( numberProp( fg, "C2ofst", "value", 5.5 ) ) == 0 );
        REQUIRE( app.m_C2ofst == Approx( 5.5 ) );

        REQUIRE( app.setCallBack_m_indiP_C1phse( numberProp( fg, "C1phse", "value", 10.0 ) ) == 0 );
        REQUIRE( app.m_C1phse == Approx( 10.0 ) );
        REQUIRE( app.setCallBack_m_indiP_C2phse( numberProp( fg, "C2phse", "value", 74.0 ) ) == 0 );
        REQUIRE( app.m_C2phse == Approx( 74.0 ) );

        REQUIRE( app.setCallBack_m_indiP_C1ofst( numberProp( fg, "C1ofst", "current", 1.0 ) ) == -1 );
        REQUIRE( app.setCallBack_m_indiP_C2phse( numberProp( fg, "C2phse", "current", 1.0 ) ) == -1 );
        REQUIRE( app.m_C1ofst == Approx( 4.5 ) );
        REQUIRE( app.m_C2phse == Approx( 74.0 ) );
    }

    SECTION( "wrong device or name is rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_C1freq( numberProp( "fxngenother", "C1freq", "current", 1.0 ) ) == -1 );
        REQUIRE( app.setCallBack_m_indiP_C1freq( numberProp( fg, "C2freq", "current", 1.0 ) ) == -1 );
        REQUIRE( app.m_C1freq == -1 );
    }
}

/// Verify the offset commands, including the rotation for x/y offsets.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator offsets", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::offset12( 0, 0 );
    ttmModulator::offsetXY( 0, 0 );
    ttmModulator::newCallBack_m_indiP_offset12( pcf::IndiProperty() );
    ttmModulator::newCallBack_m_indiP_offset( pcf::IndiProperty() );
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );
    app.setupProperties();
    app.setSet();
    app.m_C1ofst = 5.0;
    app.m_C2ofst = 5.0;

    SECTION( "without INDI the offsets fail" )
    {
        REQUIRE( app.offset12( 0.1, 0.2 ) == -1 );
        REQUIRE( app.offsetXY( 0.1, 0.2 ) == -1 );
        REQUIRE( app.m_C1ofst == 5.0 );
    }

    SECTION( "offset12 adds to each channel" )
    {
        app.installFxnGen();
        REQUIRE( app.offset12( 0.1, -0.2 ) == 0 );
        REQUIRE( app.m_C1ofst == Approx( 5.1 ) );
        REQUIRE( app.m_C2ofst == Approx( 4.8 ) );

        REQUIRE( app.newCallBack_m_indiP_offset12( numberProp2( "ttmmod", "offset12", "dC1", 0.5, "dC2", 0.25 ) ) ==
                 0 );
        REQUIRE( app.m_C1ofst == Approx( 5.6 ) );
        REQUIRE( app.m_C2ofst == Approx( 5.05 ) );

        // A missing element is a zero offset
        REQUIRE( app.newCallBack_m_indiP_offset12( numberProp( "ttmmod", "offset12", "dC2", 0.05 ) ) == 0 );
        REQUIRE( app.m_C1ofst == Approx( 5.6 ) );
        REQUIRE( app.m_C2ofst == Approx( 5.1 ) );

        REQUIRE( app.newCallBack_m_indiP_offset12( numberProp( "wrong", "offset12", "dC1", 0.5 ) ) == -1 );
        REQUIRE( app.m_C1ofst == Approx( 5.6 ) );
    }

    SECTION( "offsetXY with no rotation" )
    {
        app.installFxnGen();
        REQUIRE( app.offsetXY( 0.1, 0.2 ) == 0 );
        REQUIRE( app.m_C1ofst == Approx( 5.1 ) );
        REQUIRE( app.m_C2ofst == Approx( 5.2 ) );
    }

    SECTION( "offsetXY with a 90 degree rotation" )
    {
        app.installFxnGen();
        app.m_rotAngle = 90.0 * 3.14159 / 180.0;

        // x -> channel 2
        REQUIRE( app.newCallBack_m_indiP_offset( numberProp2( "ttmmod", "offset", "x", 0.1, "y", 0.0 ) ) == 0 );
        REQUIRE( app.m_C1ofst == Approx( 5.0 ).margin( 1e-5 ) );
        REQUIRE( app.m_C2ofst == Approx( 5.1 ).margin( 1e-5 ) );

        // y -> -channel 1
        REQUIRE( app.newCallBack_m_indiP_offset( numberProp( "ttmmod", "offset", "y", 0.2 ) ) == 0 );
        REQUIRE( app.m_C1ofst == Approx( 4.8 ).margin( 1e-5 ) );
        REQUIRE( app.m_C2ofst == Approx( 5.1 ).margin( 1e-5 ) );
    }

    SECTION( "offsetXY with negative parity" )
    {
        app.installFxnGen();
        app.m_rotParity = -1;
        REQUIRE( app.offsetXY( 0.1, 0.2 ) == 0 );
        REQUIRE( app.m_C1ofst == Approx( 5.1 ) );
        REQUIRE( app.m_C2ofst == Approx( 4.8 ) );
    }
}

/// Verify resting the TTM.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator restTTM", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::restTTM();
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );
    app.setupProperties();
    app.setChannels( 1, 1000, 0.3, 5, 0 );
    app.m_C2phse = 74;

    SECTION( "without INDI" )
    {
        REQUIRE( app.restTTM() == -1 );
    }

    SECTION( "send failure" )
    {
        app.installFxnGen();
        app.m_fxngen->m_fail = true;
        REQUIRE( app.restTTM() == -1 );
        REQUIRE( app.m_fxngen->m_sent.size() == 1u );
        REQUIRE( app.m_C1freq == Approx( 1000 ) );
    }

    SECTION( "rests from modulating" )
    {
        app.installFxnGen();
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );

        REQUIRE( app.restTTM() == 0 );

        REQUIRE( app.m_fxngen->m_sent.size() == 10u );
        REQUIRE( app.m_fxngen->last( "C1outp", "value" ) == "Off" );
        REQUIRE( app.m_fxngen->last( "C2outp", "value" ) == "Off" );

        REQUIRE( app.m_C1freq == 0 );
        REQUIRE( app.m_C2freq == 0 );
        REQUIRE( app.m_C1volts == Approx( 0.002 ) );
        REQUIRE( app.m_C2volts == Approx( 0.002 ) );
        REQUIRE( app.m_C1phse == 0 );
        REQUIRE( app.m_C2phse == 0 );
        REQUIRE( app.m_C1ofst == Approx( 0.001 ) );
        REQUIRE( app.m_C2ofst == Approx( 0.001 ) );
        REQUIRE( app.m_C1outp == 0 );
        REQUIRE( app.m_C2outp == 0 );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );
    }
}

/// Verify setting the TTM from the rest, set, midset and modulating states.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator setTTM", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::setTTM();
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );
    app.setupProperties();
    app.useFastSetting();
    app.installFxnGen();

    SECTION( "already set does nothing" )
    {
        app.setSet();
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_SET );

        REQUIRE( app.setTTM() == 0 );
        REQUIRE( app.m_fxngen->m_sent.size() == 0u );
    }

    SECTION( "from rest turns on the outputs and sets the offsets" )
    {
        app.setRested();
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );

        REQUIRE( app.setTTM() == 0 );
        REQUIRE( app.m_C1outp == 1 );
        REQUIRE( app.m_C2outp == 1 );
        REQUIRE( app.m_C1ofst == Approx( 1.0 ) );
        REQUIRE( app.m_C2ofst == Approx( 1.0 ) );
        REQUIRE( app.m_fxngen->count( "C1ofst" ) == 1u );
        REQUIRE( app.m_fxngen->count( "C2ofst" ) == 1u );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_SET );
    }

    SECTION( "refuses set voltages above 10 V" )
    {
        app.setRested();
        REQUIRE( app.calcState() == 0 );

        app.m_setVoltage_1 = 11;
        app.m_setDVolts    = 20;

        REQUIRE( app.setTTM() == -1 );
        REQUIRE( app.m_fxngen->count( "C1ofst" ) == 0u );
        REQUIRE( app.m_C1ofst == Approx( 0.001 ) );
    }

    SECTION( "from modulating stops the modulation" )
    {
        app.setChannels( 1, 1000, 0.3, 1.0, 0 );
        app.m_C2phse = 74;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );

        REQUIRE( app.setTTM() == 0 );
        REQUIRE( app.m_fxngen->m_sent.size() == 6u );
        REQUIRE( app.m_fxngen->count( "C1outp" ) == 0u );
        REQUIRE( app.m_C1freq == 0 );
        REQUIRE( app.m_C2freq == 0 );
        REQUIRE( app.m_C1volts == Approx( 0.002 ) );
        REQUIRE( app.m_C2phse == 0 );
        REQUIRE( app.m_modFreq == 0 );
        REQUIRE( app.m_modFreqRequested == 0 );
        REQUIRE( app.m_modRad == 0 );
        REQUIRE( app.m_modRadRequested == 0 );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_SET );
    }

    SECTION( "from midset rests first" )
    {
        app.setSet();
        app.m_C2phse = 74;
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MIDSET );

        REQUIRE( app.setTTM() == 0 ); // sleeps 1 second after resting
        REQUIRE( app.m_fxngen->count( "C1outp" ) == 2u );
        REQUIRE( app.m_fxngen->last( "C1outp", "value" ) == "On" );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_SET );
    }

    SECTION( "send failure" )
    {
        app.setRested();
        REQUIRE( app.calcState() == 0 );

        app.m_fxngen->m_fail = true;
        REQUIRE( app.setTTM() == -1 );
        REQUIRE( app.m_C1outp == 0 );
    }
}

/// Verify starting and changing modulation.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator modTTM", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::modTTM( 0, 0 );
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );
    app.setupProperties();
    app.useFastSetting();
    app.installFxnGen();

    SECTION( "negative requests are ignored" )
    {
        app.setSet();
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.modTTM( -1, 100 ) == 0 );
        REQUIRE( app.modTTM( 1, -100 ) == 0 );
        REQUIRE( app.m_fxngen->m_sent.size() == 0u );
    }

    SECTION( "from set at a calibration frequency" )
    {
        app.setSet();
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_SET );

        REQUIRE( app.modTTM( 0.3, 100 ) == 0 );

        REQUIRE( app.m_modRad == Approx( 0.3 ) );
        REQUIRE( app.m_modFreq == Approx( 100 ) );
        REQUIRE( app.m_C2phse == Approx( 74 ) );
        REQUIRE( app.m_C1freq == Approx( 100 ) );
        REQUIRE( app.m_C2freq == Approx( 100 ) );
        REQUIRE( app.m_C1volts == Approx( 0.310 * 0.3 ) );
        REQUIRE( app.m_C2volts == Approx( 0.313 * 0.3 ) );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modRad == Approx( 0.3 ) );

        // The current parameters again do nothing
        size_t nsent = app.m_fxngen->m_sent.size();
        REQUIRE( app.modTTM( app.m_modRad, app.m_modFreq ) == 0 );
        REQUIRE( app.m_fxngen->m_sent.size() == nsent );
    }

    SECTION( "from rest sets first" )
    {
        app.setRested();
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );

        REQUIRE( app.modTTM( 0.3, 100 ) == 0 );
        REQUIRE( app.m_C1outp == 1 );
        REQUIRE( app.m_C2outp == 1 );
        REQUIRE( app.m_C1ofst == Approx( 1.0 ) );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modFreq == Approx( 100 ) );
        REQUIRE( app.m_modRad == Approx( 0.3 ) );
    }

    SECTION( "between calibration frequencies, with a frequency ramp" )
    {
        app.setSet();
        REQUIRE( app.calcState() == 0 );

        REQUIRE( app.modTTM( 0.3, 375 ) == 0 ); // sleeps 1 second per frequency step

        // 100 Hz first, then 375 Hz
        REQUIRE( app.m_fxngen->count( "C1freq" ) == 2u );
        REQUIRE( app.m_C1freq == Approx( 375 ) );
        REQUIRE( app.m_C2freq == Approx( 375 ) );
        REQUIRE( app.m_C1volts == Approx( 0.322 * 0.3 ) );
        REQUIRE( app.m_C2volts == Approx( 0.322 * 0.3 ) );
        REQUIRE( app.m_C2phse == Approx( 74 ) );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modRad == Approx( 0.3 ) );
    }

    SECTION( "frequency and voltage limits" )
    {
        app.m_maxFreq = 100;
        app.m_maxVolt = 0.05;

        app.setSet();
        REQUIRE( app.calcState() == 0 );

        REQUIRE( app.modTTM( 10, 2000 ) == 0 );
        REQUIRE( app.m_C1freq == Approx( 100 ) );
        REQUIRE( app.m_C1volts == Approx( 0.05 ) );
        REQUIRE( app.m_C2volts == Approx( 0.05 ) );
        REQUIRE( app.m_modFreq == Approx( 100 ) );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modRad == Approx( 0.05 / 0.310 ) );
    }

    SECTION( "changing modulation stops and restarts" )
    {
        app.setSet();
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.modTTM( 0.3, 100 ) == 0 );
        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );

        REQUIRE( app.modTTM( 0.2, 100 ) == 0 );
        REQUIRE( app.m_C1volts == Approx( 0.310 * 0.2 ) );
        REQUIRE( app.m_modRad == Approx( 0.2 ) );

        REQUIRE( app.calcState() == 0 );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.m_modRad == Approx( 0.2 ) );
    }
}

/// Verify the appLogic state machine and its handling of requested states.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator appLogic", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::appLogic();
    ttmModulator::appShutdown();
    #endif
    // clang-format on

    ttmModulator_test app( "ttmmod" );
    app.setupProperties();
    app.useFastSetting();

    SECTION( "power off does nothing" )
    {
        app.state( stateCodes::POWEROFF );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_modState == MODSTATE_UNKNOWN );
        REQUIRE( app.state() == stateCodes::POWEROFF );
    }

    SECTION( "the state follows the function generator" )
    {
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );
        REQUIRE( app.state() == stateCodes::NOTHOMED );

        app.setSet();
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );

        app.m_C1phse = 5;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );

        app.setChannels( 1, 100, 0.155, 1.0, 0 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::OPERATING );
        REQUIRE( app.m_modRad == Approx( 0.5 ) );
    }

    SECTION( "a set request without INDI fails but appLogic continues" )
    {
        app.setRested();
        app.m_modStateRequested = MODSTATE_SET;

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_modStateRequested == MODSTATE_OFF );
        REQUIRE( app.m_modState == MODSTATE_REST );
        REQUIRE( app.state() == stateCodes::NOTHOMED );
    }

    SECTION( "requests are carried out" )
    {
        app.installFxnGen();
        app.setRested();

        // Set
        REQUIRE( app.newCallBack_m_indiP_modState( numberProp( "ttmmod", "modState", "target", MODSTATE_SET ) ) == 0 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_modStateRequested == MODSTATE_OFF );
        REQUIRE( app.m_modState == MODSTATE_SET );
        REQUIRE( app.state() == stateCodes::READY );

        // Modulating, but no radius or frequency requested yet
        app.m_modStateRequested = MODSTATE_MODULATING;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_modStateRequested == MODSTATE_OFF );
        REQUIRE( app.m_modState == MODSTATE_SET );
        REQUIRE( app.m_fxngen->count( "C1freq" ) == 0u );

        // Modulating
        REQUIRE( app.newCallBack_m_indiP_modRadius( numberProp( "ttmmod", "modRadius", "target", 0.3 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_modFrequency( numberProp( "ttmmod", "modFrequency", "target", 100.0 ) ) == 0 );
        REQUIRE( app.newCallBack_m_indiP_modState(
                     numberProp( "ttmmod", "modState", "target", MODSTATE_MODULATING ) ) == 0 );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_modStateRequested == MODSTATE_OFF );
        REQUIRE( app.m_modState == MODSTATE_MODULATING );
        REQUIRE( app.state() == stateCodes::OPERATING );
        REQUIRE( app.m_modFreq == Approx( 100 ) );
        REQUIRE( app.m_modRad == Approx( 0.3 ) );

        // Rest
        app.m_modStateRequested = MODSTATE_REST;
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.m_modState == MODSTATE_REST );
        REQUIRE( app.state() == stateCodes::NOTHOMED );
        REQUIRE( app.m_C1outp == 0 );
    }

    SECTION( "appShutdown" )
    {
        REQUIRE( app.appShutdown() == 0 );
    }
}

} // namespace ttmModulatorTest

} // namespace libXWCTest
