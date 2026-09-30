/** \file rhusbMon_test.cpp
 * \brief Catch2 tests for the rhusbMon app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup rhusbMon_files
 */

#include "../../../tests/testXWC.hpp"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../rhusbMon.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup rhusbMon_unit_test rhusbMon Unit Tests
 * \brief Unit tests for the rhusbMon application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `rhusbMon` unit tests.
/** \ingroup rhusbMon_unit_test
 */
namespace rhusbMonTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing rhusbMon internals.
class rhusbMon_test : public rhusbMon
{
  public:
    /// Construct a harness with the given device name.
    explicit rhusbMon_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    using rhusbMon::m_alertHumid;
    using rhusbMon::m_alertTemp;
    using rhusbMon::m_emergHumid;
    using rhusbMon::m_emergTemp;
    using rhusbMon::m_indiP_rh;
    using rhusbMon::m_indiP_temp;
    using rhusbMon::m_rh;
    using rhusbMon::m_temp;
    using rhusbMon::m_warnHumid;
    using rhusbMon::m_warnTemp;
    using rhusbMon::recordRH;

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Create the temperature and humidity properties as `appStartup()` does.
    /** `appStartup()` itself is not called because it starts the telemetry thread and searches udev for the probe.
     */
    void setupProperties()
    {
        createROIndiNumber( m_indiP_temp, "temperature", "Temperature [C]" );
        MagAOX::app::indi::addNumberElement<float>( m_indiP_temp, "current", -20., 120., 0, "%0.1f" );
        m_indiP_temp["current"] = -999;

        createROIndiNumber( m_indiP_rh, "humidity", "Relative Humidity [%]" );
        MagAOX::app::indi::addNumberElement<float>( m_indiP_rh, "current", 0., 100., 0, "%0.1f" );
        m_indiP_rh["current"] = -999;
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

/// A fake RH-USB probe on one end of a socket pair, answering the `C` and `H` commands.
/** The app's file descriptor is set to the other end of the pair.  An empty response means the command is
 * not answered, which causes a read timeout in the app.
 */
class fakeProbe
{
  public:
    /// Create the socket pair and start the probe thread.
    fakeProbe( const std::string &cResponse, /**< [in] the reply to the `C` (temperature) command */
               const std::string &hResponse  /**< [in] the reply to the `H` (humidity) command */
               )
        : m_cResponse( cResponse ), m_hResponse( hResponse )
    {
        if( socketpair( AF_UNIX, SOCK_STREAM, 0, m_fds ) != 0 )
        {
            m_fds[0] = -1;
            m_fds[1] = -1;
            return;
        }

        m_thread = std::thread( &fakeProbe::run, this );
    }

    /// Stop the probe thread and close the socket pair.
    ~fakeProbe()
    {
        m_stop = true;

        if( m_thread.joinable() )
        {
            m_thread.join();
        }

        if( m_fds[0] >= 0 )
        {
            close( m_fds[0] );
        }

        if( m_fds[1] >= 0 )
        {
            close( m_fds[1] );
        }
    }

    /// Get the file descriptor for the app's end of the socket pair.
    /**
     * \returns the app's file descriptor, or -1 if the socket pair could not be created
     */
    int fd() const
    {
        return m_fds[0];
    }

  private:
    /// Answer commands until stopped.
    void run()
    {
        struct pollfd pfd;
        pfd.fd     = m_fds[1];
        pfd.events = POLLIN;

        while( !m_stop )
        {
            int rv = poll( &pfd, 1, 10 );
            if( rv < 0 )
            {
                return;
            }

            if( rv == 0 )
            {
                continue;
            }

            char    buff[64];
            ssize_t nrd = read( m_fds[1], buff, sizeof( buff ) );
            if( nrd <= 0 )
            {
                return;
            }

            for( ssize_t n = 0; n < nrd; ++n )
            {
                if( buff[n] == 'C' )
                {
                    reply( m_cResponse );
                }
                else if( buff[n] == 'H' )
                {
                    reply( m_hResponse );
                }
            }
        }
    }

    /// Write a reply to the app, unless it is empty.
    void reply( const std::string &response /**< [in] the reply to write */ )
    {
        if( response.size() > 0 )
        {
            ssize_t nwr = write( m_fds[1], response.data(), response.size() );
            static_cast<void>( nwr );
        }
    }

    std::string m_cResponse; ///< The reply to the `C` command.

    std::string m_hResponse; ///< The reply to the `H` command.

    int m_fds[2]{ -1, -1 }; ///< The socket pair: [0] is used by the app, [1] by the probe.

    std::atomic<bool> m_stop{ false }; ///< Flag telling the probe thread to exit.

    std::thread m_thread; ///< The probe thread.
};
/// \endcond

/// Verify the rhusbMon configuration defaults.
/**
 * \ingroup rhusbMon_unit_test
 */
TEST_CASE( "rhusbMon configuration defaults", "[rhusbMon]" )
{
    // clang-format off
    #ifdef RHUSBMON_TEST_DOXYGEN_REF
    rhusbMon::rhusbMon();
    rhusbMon::setupConfig();
    rhusbMon::loadConfig();
    rhusbMon::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    rhusbMon_test app( "rhusb" );

    mx::app::writeConfigFile( "/tmp/rhusbMon_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.configure( "/tmp/rhusbMon_test_defaults.conf" );

    REQUIRE( app.m_warnTemp == 30.0f );
    REQUIRE( app.m_alertTemp == 35.0f );
    REQUIRE( app.m_emergTemp == 40.0f );
    REQUIRE( app.m_warnHumid == 18.0f );
    REQUIRE( app.m_alertHumid == 20.0f );
    REQUIRE( app.m_emergHumid == 22.0f );

    // set by the constructor
    REQUIRE( app.m_readTimeout == 2000u );
    REQUIRE( app.m_writeTimeout == 1000u );

    REQUIRE( app.m_maxInterval == 10.0 );
    REQUIRE( app.m_idVendor == "" );

    REQUIRE( app.m_temp == -999.0f );
    REQUIRE( app.m_rh == -999.0f );

    std::remove( "/tmp/rhusbMon_test_defaults.conf" );
}

/// Verify the rhusbMon configuration overrides, including the base class sections.
/**
 * \ingroup rhusbMon_unit_test
 */
TEST_CASE( "rhusbMon configuration overrides", "[rhusbMon]" )
{
    // clang-format off
    #ifdef RHUSBMON_TEST_DOXYGEN_REF
    rhusbMon::setupConfig();
    rhusbMon::loadConfig();
    #endif
    // clang-format on

    rhusbMon_test app( "rhusb" );

    // usb.baud is left unset so that usbDevice::loadConfig() does not search udev
    mx::app::writeConfigFile(
        "/tmp/rhusbMon_test_overrides.conf",
        { "temp", "temp", "temp", "humid", "humid", "humid", "device", "device", "telemeter", "usb", "usb", "usb" },
        { "warning",
          "alert",
          "emergency",
          "warning",
          "alert",
          "emergency",
          "readTimeout",
          "writeTimeout",
          "maxInterval",
          "idVendor",
          "idProduct",
          "serial" },
        { "25", "28.5", "31", "10", "12", "14.5", "500", "250", "5", "0403", "6001", "RH1234" } );
    app.configure( "/tmp/rhusbMon_test_overrides.conf" );

    REQUIRE( app.m_warnTemp == Approx( 25.0 ) );
    REQUIRE( app.m_alertTemp == Approx( 28.5 ) );
    REQUIRE( app.m_emergTemp == Approx( 31.0 ) );
    REQUIRE( app.m_warnHumid == Approx( 10.0 ) );
    REQUIRE( app.m_alertHumid == Approx( 12.0 ) );
    REQUIRE( app.m_emergHumid == Approx( 14.5 ) );

    REQUIRE( app.m_readTimeout == 500u );
    REQUIRE( app.m_writeTimeout == 250u );

    REQUIRE( app.m_maxInterval == Approx( 5.0 ) );

    REQUIRE( app.m_idVendor == "0403" );
    REQUIRE( app.m_idProduct == "6001" );
    REQUIRE( app.m_serial == "RH1234" );

    std::remove( "/tmp/rhusbMon_test_overrides.conf" );
}

/// Verify `readProbe()` issues the commands and parses the temperature and humidity replies.
/**
 * \ingroup rhusbMon_unit_test
 */
TEST_CASE( "rhusbMon readProbe parses the probe replies", "[rhusbMon]" )
{
    // clang-format off
    #ifdef RHUSBMON_TEST_DOXYGEN_REF
    rhusbMon::readProbe();
    #endif
    // clang-format on

    rhusbMon_test app( "rhusb" );
    app.m_readTimeout  = 1000;
    app.m_writeTimeout = 1000;

    SECTION( "positive temperature" )
    {
        fakeProbe probe( "23.4 C\r\n>", "45.6 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == 0 );

        REQUIRE( app.m_temp == Approx( 23.4 ) );
        REQUIRE( app.m_rh == Approx( 45.6 ) );
    }

    SECTION( "negative temperature" )
    {
        fakeProbe probe( "-5.5 C\r\n>", "0.2 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == 0 );

        REQUIRE( app.m_temp == Approx( -5.5 ) );
        REQUIRE( app.m_rh == Approx( 0.2 ) );
    }
}

/// Verify `readProbe()` reports I/O and parsing errors.
/**
 * \ingroup rhusbMon_unit_test
 */
TEST_CASE( "rhusbMon readProbe error handling", "[rhusbMon]" )
{
    // clang-format off
    #ifdef RHUSBMON_TEST_DOXYGEN_REF
    rhusbMon::readProbe();
    #endif
    // clang-format on

    rhusbMon_test app( "rhusb" );
    app.m_readTimeout  = 200;
    app.m_writeTimeout = 200;

    SECTION( "write failure" )
    {
        // poll() ignores a negative descriptor, so the write poll times out
        app.m_fileDescrip  = -1;
        app.m_writeTimeout = 20;

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_temp == -999.0f );
        REQUIRE( app.m_rh == -999.0f );
    }

    SECTION( "no temperature reply" )
    {
        fakeProbe probe( "", "45.6 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_temp == -999.0f );
    }

    SECTION( "temperature reply without a value" )
    {
        fakeProbe probe( " C\r\n>", "45.6 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_temp == -999.0f );
        REQUIRE( app.m_rh == -999.0f );
    }

    SECTION( "temperature reply not starting with a digit" )
    {
        fakeProbe probe( "A C\r\n>", "45.6 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_temp == -999.0f );
    }

    SECTION( "temperature reply without the unit" )
    {
        fakeProbe probe( "20.0\r\n>", "45.6 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_temp == -999.0f );
    }

    SECTION( "no humidity reply" )
    {
        fakeProbe probe( "21.0 C\r\n>", "" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_temp == Approx( 21.0 ) );
        REQUIRE( app.m_rh == -999.0f );
    }

    SECTION( "negative humidity reply" )
    {
        fakeProbe probe( "21.0 C\r\n>", "-1.0 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_temp == Approx( 21.0 ) );
        REQUIRE( app.m_rh == -999.0f );
    }

    SECTION( "humidity reply without a value" )
    {
        fakeProbe probe( "21.0 C\r\n>", " %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_rh == -999.0f );
    }

    SECTION( "humidity reply without the unit" )
    {
        fakeProbe probe( "21.0 C\r\n>", "40.0\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        REQUIRE( app.readProbe() == -1 );
        REQUIRE( app.m_rh == -999.0f );
    }
}

/// Verify `appLogic()` reads the probe when connected and handles read failures.
/**
 * With no telemetry thread running in the unit tests, the `telemeter` step of `appLogic()` sets `FAILURE`
 * and `m_shutdown` after the probe has been read.
 *
 * \ingroup rhusbMon_unit_test
 */
TEST_CASE( "rhusbMon appLogic reads the probe", "[rhusbMon]" )
{
    // clang-format off
    #ifdef RHUSBMON_TEST_DOXYGEN_REF
    rhusbMon::appLogic();
    rhusbMon::appShutdown();
    #endif
    // clang-format on

    rhusbMon_test app( "rhusb" );
    app.setupProperties();
    app.m_readTimeout  = 1000;
    app.m_writeTimeout = 1000;

    SECTION( "a successful read updates the values" )
    {
        fakeProbe probe( "19.5 C\r\n>", "12.5 %RH\r\n>" );
        REQUIRE( probe.fd() >= 0 );
        app.m_fileDescrip = probe.fd();

        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );

        REQUIRE( app.m_temp == Approx( 19.5 ) );
        REQUIRE( app.m_rh == Approx( 12.5 ) );

        // the humidity property is reset directly to force a new timestamp
        REQUIRE( app.m_indiP_rh["current"].get<float>() == Approx( -999.0 ) );

        // the telemetry thread is not running
        REQUIRE( app.state() == stateCodes::FAILURE );
        REQUIRE( app.shutdownFlag() == 1 );
    }

    SECTION( "a failed read sets ERROR" )
    {
        app.m_fileDescrip  = -1;
        app.m_writeTimeout = 20;

        app.state( stateCodes::OPERATING );

        REQUIRE( app.appLogic() == 0 );

        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.shutdownFlag() == 0 );
        REQUIRE( app.m_temp == -999.0f );
    }

    REQUIRE( app.appShutdown() == 0 );
}

/// Verify `appLogic()` evaluates the warning, alert and emergency limits without reading the probe.
/**
 * In a state other than the connect and read states, `appLogic()` only checks the current values against the limits.
 *
 * \ingroup rhusbMon_unit_test
 */
TEST_CASE( "rhusbMon appLogic checks the limits", "[rhusbMon]" )
{
    // clang-format off
    #ifdef RHUSBMON_TEST_DOXYGEN_REF
    rhusbMon::appLogic();
    #endif
    // clang-format on

    rhusbMon_test app( "rhusb" );
    app.setupProperties();

    app.state( stateCodes::READY );

    // normal, warning, alert and emergency levels for both temperature and humidity
    float temps[]  = { 20.0f, 31.0f, 36.0f, 41.0f };
    float humids[] = { 10.0f, 19.0f, 21.0f, 23.0f };

    for( size_t n = 0; n < 4; ++n )
    {
        app.m_temp = temps[n];
        app.m_rh   = humids[n];

        REQUIRE( app.appLogic() == 0 );

        // the values are not changed by the limit checks
        REQUIRE( app.m_temp == temps[n] );
        REQUIRE( app.m_rh == humids[n] );
    }
}

/// Verify the telemetry recording of temperature and humidity.
/**
 * \ingroup rhusbMon_unit_test
 */
TEST_CASE( "rhusbMon telemetry recording", "[rhusbMon]" )
{
    // clang-format off
    #ifdef RHUSBMON_TEST_DOXYGEN_REF
    rhusbMon::checkRecordTimes();
    rhusbMon::recordTelem( nullptr );
    rhusbMon::recordRH( false );
    #endif
    // clang-format on

    rhusbMon_test app( "rhusb" );

    app.m_temp = 21.0f;
    app.m_rh   = 11.0f;

    SECTION( "recordTelem forces a record" )
    {
        MagAOX::logger::telem_rhusb::lastRecord = { 0, 0 };

        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_rhusb *>( nullptr ) ) == 0 );

        REQUIRE( MagAOX::logger::telem_rhusb::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordRH only records changed values unless forced" )
    {
        // synchronize the last recorded values
        REQUIRE( app.recordRH( true ) == 0 );

        MagAOX::logger::telem_rhusb::lastRecord = { 0, 0 };

        REQUIRE( app.recordRH( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_rhusb::lastRecord.tv_sec == 0 );

        app.m_temp = 21.5f;
        REQUIRE( app.recordRH( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_rhusb::lastRecord.tv_sec > 0 );

        MagAOX::logger::telem_rhusb::lastRecord = { 0, 0 };
        app.m_rh                                = 11.5f;
        REQUIRE( app.recordRH( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_rhusb::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records only after the maximum interval" )
    {
        MagAOX::logger::telem_rhusb::lastRecord = { 0, 0 };

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_rhusb::lastRecord.tv_sec > 0 );

        // a record one second ago is within the 10 second maximum interval
        timespec recent;
        clock_gettime( CLOCK_REALTIME, &recent );
        recent.tv_sec -= 1;
        MagAOX::logger::telem_rhusb::lastRecord = recent;

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_rhusb::lastRecord.tv_sec == recent.tv_sec );
        REQUIRE( MagAOX::logger::telem_rhusb::lastRecord.tv_nsec == recent.tv_nsec );
    }
}

} // namespace rhusbMonTest

} // namespace libXWCTest
