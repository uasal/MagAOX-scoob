/** \file usbtempMon_test.cpp
 * \brief Catch2 tests for the usbtempMon app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup usbtempMon_files
 */

#include "../../../tests/testXWC.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

#include "../usbtempMon.hpp"

// Direct include of the DS18B20 driver (normally built as usbtemp.o) so this test stays a single translation unit.
extern "C"
{
#include "../usbtemp.c"
}

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup usbtempMon_unit_test usbtempMon Unit Tests
 * \brief Unit tests for the usbtempMon application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `usbtempMon` unit tests.
/** \ingroup usbtempMon_unit_test
 */
namespace usbtempMonTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Directory the test harness points the telemetry logger at.
const char *telemPath = "/tmp/usbtempMon_test_telem";

/// Removes the telemetry directory at program exit (telemetry is written when each app is destroyed).
struct telemCleanup
{
    /// Remove the telemetry directory.
    ~telemCleanup()
    {
        std::error_code ec;
        std::filesystem::remove_all( telemPath, ec );
    }
};

/// The cleanup object.
telemCleanup s_telemCleanup;

/// Test harness exposing usbtempMon internals.
class usbtempMon_test : public usbtempMon
{
  public:
    /// Construct a harness with the given device name.
    explicit usbtempMon_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        static_cast<void>( m_tel.logPath( telemPath ) );
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
        static_cast<void>( m_tel.logPath( telemPath ) );
    }

    /// Run setupConfig(), read the given config file, and return the result of loadConfigImpl().
    int configureImpl( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        return loadConfigImpl( config );
    }

    /// Get the warning temperature.
    float warnTemp() const
    {
        return m_warnTemp;
    }

    /// Get the alert temperature.
    float alertTemp() const
    {
        return m_alertTemp;
    }

    /// Get the emergency temperature.
    float emergTemp() const
    {
        return m_emergTemp;
    }

    /// Get the number of configured probes.
    size_t nProbes() const
    {
        return m_probes.size();
    }

    /// Get a probe's location.
    std::string probeLocation( size_t n /**< [in] probe index */ ) const
    {
        return m_probes[n].m_location;
    }

    /// Get a probe's serial number.
    std::string probeSerial( size_t n /**< [in] probe index */ ) const
    {
        return m_probes[n].m_serial;
    }

    /// Get a probe's file descriptor.
    int probeFd( size_t n /**< [in] probe index */ ) const
    {
        return m_probes[n].m_fd;
    }

    /// Get a probe's last temperature.
    float probeTemperature( size_t n /**< [in] probe index */ ) const
    {
        return m_probes[n].m_temperature;
    }

    /// Set a probe's temperature.
    void probeTemperature( size_t n, /**< [in] probe index */
                           float  t /**< [in] the new temperature */ )
    {
        m_probes[n].m_temperature = t;
    }

    /// Add a probe.
    void addProbe( const std::string &location, /**< [in] the probe location */
                   const std::string &serial,   /**< [in] the probe ROM serial string */
                   int                fd,       /**< [in] the probe file descriptor, 0 if not connected */
                   float              temp /**< [in] the initial temperature */ )
    {
        m_probes.emplace_back();
        m_probes.back().m_location    = location;
        m_probes.back().m_serial      = serial;
        m_probes.back().m_fd          = fd;
        m_probes.back().m_temperature = temp;
    }

    /// Get the last recorded temperatures.
    const std::vector<float> &lastTemps() const
    {
        return m_lastTemps;
    }

    /// Call the protected recordTemps().
    int doRecordTemps( bool force /**< [in] whether to force the record */ )
    {
        return recordTemps( force );
    }

    /// Get the shutdown flag.
    int shutdown() const
    {
        return m_shutdown;
    }
};

/// A fake DS18B20 1-wire temperature sensor behind a USB-serial adapter, on the master side of a pseudo-terminal.
/** The app opens the pty slave with `DS18B20_open()`.  The fake answers the reset pulse (0xF0) with a presence
 * byte, echoes write slots, and answers read slots after the Read ROM (0x33) and Read Scratchpad (0xCC 0xBE) commands.
 */
class fakeDS18B20
{
  public:
    /// Create the pseudo-terminal and default ROM and scratchpad contents (25.0625 C, 12-bit).
    fakeDS18B20()
    {
        m_master = posix_openpt( O_RDWR | O_NOCTTY );
        if( m_master >= 0 && grantpt( m_master ) == 0 && unlockpt( m_master ) == 0 )
        {
            char *name = ptsname( m_master );
            if( name )
            {
                m_slaveName = name;
            }
        }

        m_rom    = { 0x28, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0 };
        m_rom[7] = lsb_crc8( m_rom.data(), 7, DS18X20_GENERATOR );

        m_sp    = { 0x91, 0x01, 0x4b, 0x46, 0x7f, 0xff, 0x0f, 0x10, 0 };
        m_sp[8] = lsb_crc8( m_sp.data(), 8, DS18X20_GENERATOR );
    }

    /// Stop the responder and close the master side.
    ~fakeDS18B20()
    {
        stop();
        if( m_master >= 0 )
        {
            close( m_master );
        }
    }

    /// Whether the pseudo-terminal was created.
    bool ok() const
    {
        return m_master >= 0 && !m_slaveName.empty();
    }

    /// The pty slave device path, for `DS18B20_open()`.
    const std::string &slaveName() const
    {
        return m_slaveName;
    }

    /// Set the raw 16-bit temperature in the scratchpad (1/16 C units) and update the CRC.
    void rawTemperature( uint16_t raw /**< [in] the raw temperature word */ )
    {
        m_sp[0] = raw & 0xff;
        m_sp[1] = ( raw >> 8 ) & 0xff;
        m_sp[8] = lsb_crc8( m_sp.data(), 8, DS18X20_GENERATOR );
    }

    /// Set the scratchpad configuration byte and update the CRC.
    void configByte( unsigned char cfg /**< [in] the configuration register value */ )
    {
        m_sp[4] = cfg;
        m_sp[8] = lsb_crc8( m_sp.data(), 8, DS18X20_GENERATOR );
    }

    /// Corrupt the scratchpad CRC.
    void corruptScratchpadCRC()
    {
        m_sp[8] ^= 0xff;
    }

    /// Corrupt the ROM CRC.
    void corruptRomCRC()
    {
        m_rom[7] ^= 0xff;
    }

    /// Set whether the sensor answers the reset pulse.
    void present( bool p /**< [in] true if the sensor is present */ )
    {
        m_present = p;
    }

    /// The ROM contents.
    const std::vector<unsigned char> &rom() const
    {
        return m_rom;
    }

    /// Start the responder thread.  Call after the slave is opened, and after setting the contents.
    void start()
    {
        m_stop   = false;
        m_thread = std::thread( [this]() { run(); } );
    }

    /// Stop the responder thread.
    void stop()
    {
        m_stop = true;
        if( m_thread.joinable() )
        {
            m_thread.join();
        }
    }

    /// Bytes written by the host in non-read phases (commands and data).  Only valid after stop().
    const std::vector<unsigned char> &commands() const
    {
        return m_commands;
    }

    /// Number of reset pulses received.  Only valid after stop().
    int resets() const
    {
        return m_resets;
    }

  private:
    /// Protocol phase of the fake sensor.
    enum class phaseT
    {
        other,    ///< Echo write slots, record bytes.
        romCmd,   ///< Expecting a ROM command after a reset.
        funcCmd,  ///< Expecting a function command after Skip ROM.
        readData, ///< Answering read slots from m_readData.
    };

    /// Write one reply byte to the host.
    void reply( unsigned char b /**< [in] the byte to send */ )
    {
        if( write( m_master, &b, 1 ) < 0 )
        {
            return;
        }
    }

    /// The responder loop.
    void run()
    {
        phaseT                            phase = phaseT::other;
        int                               slot  = 0;
        unsigned char                     cur   = 0;
        const std::vector<unsigned char> *data  = nullptr;
        size_t                            idx   = 0;

        while( !m_stop )
        {
            pollfd pfd;
            pfd.fd      = m_master;
            pfd.events  = POLLIN;
            pfd.revents = 0;
            if( poll( &pfd, 1, 10 ) <= 0 )
            {
                continue;
            }

            unsigned char buf[64];
            ssize_t       rv = read( m_master, buf, sizeof( buf ) );
            if( rv <= 0 )
            {
                std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
                continue;
            }

            for( ssize_t k = 0; k < rv; ++k )
            {
                unsigned char b = buf[k];

                if( b == 0xf0 ) // reset pulse
                {
                    ++m_resets;
                    slot  = 0;
                    cur   = 0;
                    phase = phaseT::romCmd;
                    reply( m_present ? 0xe0 : 0xf0 );
                    continue;
                }

                if( phase == phaseT::readData )
                {
                    reply( ( ( ( *data )[idx] >> slot ) & 0x01 ) ? 0xff : 0x00 );
                }
                else
                {
                    reply( b );
                    if( b & 0x01 )
                    {
                        cur |= ( 1 << slot );
                    }
                }

                ++slot;
                if( slot < 8 )
                {
                    continue;
                }

                slot = 0;
                if( phase == phaseT::readData )
                {
                    ++idx;
                    if( idx >= data->size() )
                    {
                        phase = phaseT::other;
                    }
                    continue;
                }

                m_commands.push_back( cur );
                if( phase == phaseT::romCmd )
                {
                    if( cur == 0x33 )
                    {
                        phase = phaseT::readData;
                        data  = &m_rom;
                        idx   = 0;
                    }
                    else if( cur == 0xcc )
                    {
                        phase = phaseT::funcCmd;
                    }
                    else
                    {
                        phase = phaseT::other;
                    }
                }
                else if( phase == phaseT::funcCmd )
                {
                    if( cur == 0xbe )
                    {
                        phase = phaseT::readData;
                        data  = &m_sp;
                        idx   = 0;
                    }
                    else
                    {
                        phase = phaseT::other;
                    }
                }
                cur = 0;
            }
        }
    }

    int                        m_master{ -1 };    ///< The pty master file descriptor.
    std::string                m_slaveName;       ///< The pty slave device path.
    std::vector<unsigned char> m_rom;             ///< The ROM contents returned by Read ROM.
    std::vector<unsigned char> m_sp;              ///< The scratchpad contents returned by Read Scratchpad.
    bool                       m_present{ true }; ///< Whether the sensor answers the reset pulse.
    std::atomic<bool>          m_stop{ false };   ///< Flag to stop the responder thread.
    std::thread                m_thread;          ///< The responder thread.
    std::vector<unsigned char> m_commands;        ///< Bytes written by the host outside of read phases.
    int                        m_resets{ 0 };     ///< Number of reset pulses received.
};

/// Format a ROM as the lower-case hex string checkConnections() compares against the configured serial.
std::string romString( const std::vector<unsigned char> &rom /**< [in] the 8 ROM bytes */ )
{
    char romstr[2 * DS18X20_ROM_SIZE + 1];
    for( size_t i = 0; i < DS18X20_ROM_SIZE; i++ )
    {
        snprintf( romstr + 2 * i, 3, "%02x", rom[i] );
    }
    return std::string( romstr );
}

/// \endcond

/// Verify usbtempMon construction defaults.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon construction defaults", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    usbtempMon();
    #endif
    // clang-format on

    usbtempMon_test app( "usbtemp" );

    REQUIRE( app.warnTemp() == Approx( 40 ) );
    REQUIRE( app.alertTemp() == Approx( 50 ) );
    REQUIRE( app.emergTemp() == Approx( 55 ) );
    REQUIRE( app.nProbes() == 0 );
    REQUIRE( app.lastTemps().empty() );
    REQUIRE( app.appShutdown() == 0 );
}

/// Verify usbtempMon configuration of thresholds, USB IDs, and probe sections.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon configuration", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    usbtempMon::setupConfig();
    usbtempMon::loadConfig();
    usbtempMon::loadConfigImpl(config);
    #endif
    // clang-format on

    SECTION( "defaults with one probe" )
    {
        usbtempMon_test app( "usbtemp" );

        mx::app::writeConfigFile(
            "/tmp/usbtempMon_test_defaults.conf", { "rack" }, { "serial" }, { "28A1B2C3D4E5F601" } );
        app.configure( "/tmp/usbtempMon_test_defaults.conf" );
        std::remove( "/tmp/usbtempMon_test_defaults.conf" );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.warnTemp() == Approx( 40 ) );
        REQUIRE( app.alertTemp() == Approx( 50 ) );
        REQUIRE( app.emergTemp() == Approx( 55 ) );
        REQUIRE( app.m_idVendor == "067b" );
        REQUIRE( app.m_idProduct == "2303" );
        REQUIRE( app.m_maxInterval == Approx( 10.0 ) );

        REQUIRE( app.nProbes() == 1 );
        REQUIRE( app.probeSerial( 0 ) == "28a1b2c3d4e5f601" ); // lower-cased
        REQUIRE( app.probeLocation( 0 ) == "rack" );           // defaults to the section name
        REQUIRE( app.probeFd( 0 ) == 0 );
    }

    SECTION( "overrides and several probes sorted by location" )
    {
        usbtempMon_test app( "usbtemp" );

        mx::app::writeConfigFile(
            "/tmp/usbtempMon_test_override.conf",
            { "temp", "temp", "temp", "usb", "usb", "telemeter", "p1", "p1", "p2", "p2", "p3", "nolocation" },
            { "warning",
              "alert",
              "emergency",
              "idVendor",
              "idProduct",
              "maxInterval",
              "serial",
              "location",
              "serial",
              "location",
              "location",
              "serial" },
            { "30", "35", "45", "fffe", "fffd", "7", "AA01", "zeta", "BB02", "alpha", "ignored", "CC03" } );
        app.configure( "/tmp/usbtempMon_test_override.conf" );
        std::remove( "/tmp/usbtempMon_test_override.conf" );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.warnTemp() == Approx( 30 ) );
        REQUIRE( app.alertTemp() == Approx( 35 ) );
        REQUIRE( app.emergTemp() == Approx( 45 ) );
        REQUIRE( app.m_idVendor == "fffe" );
        REQUIRE( app.m_idProduct == "fffd" );
        REQUIRE( app.m_maxInterval == Approx( 7.0 ) );

        // p3 has no serial so it is not a probe.
        REQUIRE( app.nProbes() == 3 );
        REQUIRE( app.probeLocation( 0 ) == "alpha" );
        REQUIRE( app.probeSerial( 0 ) == "bb02" );
        REQUIRE( app.probeLocation( 1 ) == "nolocation" );
        REQUIRE( app.probeSerial( 1 ) == "cc03" );
        REQUIRE( app.probeLocation( 2 ) == "zeta" );
        REQUIRE( app.probeSerial( 2 ) == "aa01" );
    }

    SECTION( "no probe sections is fatal" )
    {
        usbtempMon_test app( "usbtemp" );

        mx::app::writeConfigFile( "/tmp/usbtempMon_test_noprobes.conf", { "temp" }, { "warning" }, { "33" } );
        REQUIRE( app.configureImpl( "/tmp/usbtempMon_test_noprobes.conf" ) == -1 );
        std::remove( "/tmp/usbtempMon_test_noprobes.conf" );

        REQUIRE( app.shutdown() == 1 );
        REQUIRE( app.nProbes() == 0 );
        REQUIRE( app.warnTemp() == Approx( 33 ) );
    }
}

/// Verify the Dallas/Maxim CRC-8 used for the DS18B20 ROM and scratchpad.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon DS18B20 CRC-8", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    lsb_crc8(data, len, DS18X20_GENERATOR);
    #endif
    // clang-format on

    // Maxim application note 27 example ROM.
    unsigned char rom[8] = { 0x02, 0x1c, 0xb8, 0x01, 0x00, 0x00, 0x00, 0xa2 };
    REQUIRE( lsb_crc8( rom, 7, DS18X20_GENERATOR ) == 0xa2 );
    REQUIRE( lsb_crc8( rom, 8, DS18X20_GENERATOR ) == 0x00 );

    // CRC-8/MAXIM check value.
    unsigned char check[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    REQUIRE( lsb_crc8( check, 9, DS18X20_GENERATOR ) == 0xa1 );

    REQUIRE( lsb_crc8( rom, 0, DS18X20_GENERATOR ) == 0x00 );
}

/// Verify DS18B20_open() failure and error messages.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon DS18B20 open errors", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    DS18B20_open("");
    DS18B20_errmsg();
    is_fd_valid(0);
    #endif
    // clang-format on

    int fd = DS18B20_open( "/tmp/usbtempMon_test_does_not_exist" );
    REQUIRE( fd == -1 );
    REQUIRE_FALSE( is_fd_valid( fd ) );
    REQUIRE( std::string( DS18B20_errmsg() ) == "Error, serial port does not exist!" );

    REQUIRE_FALSE( is_fd_valid( 0 ) );
    REQUIRE( is_fd_valid( 3 ) );
}

/// Verify DS18B20_rom() reads and CRC-checks the sensor ROM, formatted as checkConnections() does.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon DS18B20 read ROM", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    DS18B20_rom(fd, rom);
    usbtempMon::checkConnections();
    #endif
    // clang-format on

    fakeDS18B20 dev;
    REQUIRE( dev.ok() );

    SECTION( "valid ROM" )
    {
        int fd = DS18B20_open( dev.slaveName().c_str() );
        REQUIRE( is_fd_valid( fd ) );
        dev.start();

        unsigned char rom[DS18X20_ROM_SIZE];
        REQUIRE( DS18B20_rom( fd, rom ) == 0 );
        dev.stop();
        DS18B20_close( fd );

        std::vector<unsigned char> vrom( rom, rom + DS18X20_ROM_SIZE );
        REQUIRE( vrom == dev.rom() );
        REQUIRE( romString( vrom ).substr( 0, 14 ) == "28a1b2c3d4e5f6" );
        REQUIRE( dev.resets() == 1 );
        REQUIRE( dev.commands() == std::vector<unsigned char>{ 0x33 } );
    }

    SECTION( "bad ROM CRC" )
    {
        dev.corruptRomCRC();
        int fd = DS18B20_open( dev.slaveName().c_str() );
        REQUIRE( is_fd_valid( fd ) );
        dev.start();

        unsigned char rom[DS18X20_ROM_SIZE];
        REQUIRE( DS18B20_rom( fd, rom ) == -1 );
        dev.stop();
        DS18B20_close( fd );

        REQUIRE( std::string( DS18B20_errmsg() ) == "Error, sensor CRC mismatch!" );
    }

    SECTION( "no sensor" )
    {
        dev.present( false );
        int fd = DS18B20_open( dev.slaveName().c_str() );
        REQUIRE( is_fd_valid( fd ) );
        dev.start();

        unsigned char rom[DS18X20_ROM_SIZE];
        REQUIRE( DS18B20_rom( fd, rom ) == -1 );
        dev.stop();
        DS18B20_close( fd );

        REQUIRE( std::string( DS18B20_errmsg() ) == "Error, sensor not found!" );
    }
}

/// Verify DS18B20_acquire() converts the scratchpad temperature word to degrees C.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon DS18B20 temperature conversion", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    DS18B20_acquire(fd, &temperature);
    #endif
    // clang-format on

    auto [raw, expected] = GENERATE( table<uint16_t, float>(
        { { 0x0191, 25.0625f }, { 0x07d0, 125.0f }, { 0x0550, 85.0f }, { 0x0008, 0.5f }, { 0x0000, 0.0f } } ) );

    fakeDS18B20 dev;
    REQUIRE( dev.ok() );
    dev.rawTemperature( raw );

    int fd = DS18B20_open( dev.slaveName().c_str() );
    REQUIRE( is_fd_valid( fd ) );
    dev.start();

    float temperature = -1e37;
    REQUIRE( DS18B20_acquire( fd, &temperature ) == 0 );
    dev.stop();
    DS18B20_close( fd );

    REQUIRE( temperature == Approx( expected ) );
    REQUIRE( dev.commands() == std::vector<unsigned char>{ 0xcc, 0xbe } );
}

/// Verify DS18B20_acquire() error handling for missing sensors, bad CRC, and a bad configuration byte.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon DS18B20 acquire errors", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    DS18B20_acquire(fd, &temperature);
    #endif
    // clang-format on

    fakeDS18B20 dev;
    REQUIRE( dev.ok() );

    std::string msg;

    SECTION( "no sensor" )
    {
        dev.present( false );
        msg = "Error, sensor not found!";
    }

    SECTION( "bad scratchpad CRC" )
    {
        dev.corruptScratchpadCRC();
        msg = "Error, sensor CRC mismatch!";
    }

    SECTION( "configuration byte is not a DS18B20" )
    {
        dev.configByte( 0x00 );
        msg = "Error, sensor not found!";
    }

    int fd = DS18B20_open( dev.slaveName().c_str() );
    REQUIRE( is_fd_valid( fd ) );
    dev.start();

    float temperature = -1e37;
    REQUIRE( DS18B20_acquire( fd, &temperature ) == -1 );
    dev.stop();
    DS18B20_close( fd );

    REQUIRE( temperature == Approx( -1e37 ) );
    REQUIRE( std::string( DS18B20_errmsg() ) == msg );
}

/// Verify DS18B20_measure() and DS18B20_setprecision() send the expected 1-wire commands.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon DS18B20 measure and precision commands", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    DS18B20_measure(fd);
    DS18B20_setprecision(fd, 9);
    #endif
    // clang-format on

    fakeDS18B20 dev;
    REQUIRE( dev.ok() );

    int fd = DS18B20_open( dev.slaveName().c_str() );
    REQUIRE( is_fd_valid( fd ) );
    dev.start();

    SECTION( "measure starts a conversion" )
    {
        REQUIRE( DS18B20_measure( fd ) == 0 );
        dev.stop();
        REQUIRE( dev.resets() == 1 );
        REQUIRE( dev.commands() == std::vector<unsigned char>{ 0xcc, 0x44 } );
    }

    SECTION( "setprecision to the current precision only reads" )
    {
        REQUIRE( DS18B20_setprecision( fd, 12 ) == 0 );
        dev.stop();
        REQUIRE( dev.commands() == std::vector<unsigned char>{ 0xcc, 0xbe } );
    }

    SECTION( "setprecision to 9 bits writes and saves the configuration" )
    {
        REQUIRE( DS18B20_setprecision( fd, 9 ) == 0 );
        dev.stop();
        REQUIRE( dev.resets() == 3 );
        REQUIRE( dev.commands() == std::vector<unsigned char>{ 0xcc, 0xbe, 0xcc, 0x4e, 0x4b, 0x46, 0x1f, 0xcc, 0x48 } );
    }

    DS18B20_close( fd );
}

/// Verify recordTemps() records only on change or when forced, and the telemeter interval hooks.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon telemetry recording", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    usbtempMon::recordTemps(true);
    usbtempMon::recordTelem(nullptr);
    usbtempMon::checkRecordTimes();
    #endif
    // clang-format on

    usbtempMon_test app( "usbtemp" );
    app.addProbe( "a", "aa", 0, 20.0 );
    app.addProbe( "b", "bb", 0, 21.0 );

    SECTION( "recordTemps change detection" )
    {
        MagAOX::logger::telem_temps::lastRecord = { 0, 0 };
        REQUIRE( app.doRecordTemps( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec != 0 ); // first call always differs
        REQUIRE( app.lastTemps().size() == 2 );
        REQUIRE( app.lastTemps()[0] == Approx( 20.0 ) );
        REQUIRE( app.lastTemps()[1] == Approx( 21.0 ) );

        MagAOX::logger::telem_temps::lastRecord = { 0, 0 };
        REQUIRE( app.doRecordTemps( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec == 0 );

        app.probeTemperature( 1, 22.5 );
        REQUIRE( app.doRecordTemps( false ) == 0 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec != 0 );
        REQUIRE( app.lastTemps()[1] == Approx( 22.5 ) );

        MagAOX::logger::telem_temps::lastRecord = { 0, 0 };
        REQUIRE( app.doRecordTemps( true ) == 0 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec != 0 );
    }

    SECTION( "adding a probe resizes the last temperatures" )
    {
        REQUIRE( app.doRecordTemps( true ) == 0 );
        app.addProbe( "c", "cc", 0, 23.0 );
        MagAOX::logger::telem_temps::lastRecord = { 0, 0 };
        REQUIRE( app.doRecordTemps( false ) == 0 );
        REQUIRE( app.lastTemps().size() == 3 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec != 0 );
    }

    SECTION( "recordTelem forces a record" )
    {
        REQUIRE( app.doRecordTemps( true ) == 0 );
        MagAOX::logger::telem_temps::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( nullptr ) == 0 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec != 0 );
    }

    SECTION( "checkRecordTimes records only when stale" )
    {
        MagAOX::logger::telem_temps::lastRecord = { 0, 0 };
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec != 0 );

        timespec fresh = MagAOX::logger::telem_temps::lastRecord;
        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_sec == fresh.tv_sec );
        REQUIRE( MagAOX::logger::telem_temps::lastRecord.tv_nsec == fresh.tv_nsec );
    }
}

/// Verify checkConnections() sets NOTCONNECTED when no matching USB serial adapters exist.
/**
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon checkConnections with no adapters", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    usbtempMon::checkConnections();
    #endif
    // clang-format on

    usbtempMon_test app( "usbtemp" );

    // IDs which match no real device.
    app.m_idVendor  = "fffe";
    app.m_idProduct = "fffd";
    app.addProbe( "a", "aa", 0, 20.0 );

    app.state( stateCodes::CONNECTED );
    REQUIRE( app.checkConnections() == 0 );
    REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    REQUIRE( app.probeFd( 0 ) == 0 );
}

/// Verify appLogic() reads connected probes, restarts their conversion, and drops probes that fail.
/**
 * The telemetry thread is not running in the test, so `telemeter::appLogic()` sets FAILURE at the end of each call.
 *
 * \ingroup usbtempMon_unit_test
 */
TEST_CASE( "usbtempMon appLogic reads the probes", "[usbtempMon]" )
{
    // clang-format off
    #ifdef USBTEMPMON_TEST_DOXYGEN_REF
    usbtempMon::appLogic();
    #endif
    // clang-format on

    usbtempMon_test app( "usbtemp" );

    // IDs which match no real device, so checkConnections() never opens anything.
    app.m_idVendor  = "fffe";
    app.m_idProduct = "fffd";

    fakeDS18B20 dev;
    REQUIRE( dev.ok() );

    SECTION( "a good probe is read and a new conversion started" )
    {
        dev.rawTemperature( 0x0191 );
        int fd = DS18B20_open( dev.slaveName().c_str() );
        REQUIRE( is_fd_valid( fd ) );
        dev.start();

        app.addProbe( "rack", romString( dev.rom() ), fd, 0.0 );
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );
        dev.stop();

        REQUIRE( app.probeFd( 0 ) == fd );
        REQUIRE( app.probeTemperature( 0 ) == Approx( 25.0625 ) );
        REQUIRE( app.lastTemps().size() == 1 );
        REQUIRE( app.lastTemps()[0] == Approx( 25.0625 ) );
        REQUIRE( dev.commands() == std::vector<unsigned char>{ 0xcc, 0xbe, 0xcc, 0x44 } );
        REQUIRE( app.shutdown() == 1 ); // from telemeter::appLogic, no telemetry thread

        DS18B20_close( fd );
    }

    SECTION( "a failing probe is closed and marked disconnected" )
    {
        dev.present( false );
        int fd = DS18B20_open( dev.slaveName().c_str() );
        REQUIRE( is_fd_valid( fd ) );
        dev.start();

        app.addProbe( "rack", romString( dev.rom() ), fd, 12.0 );
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );
        dev.stop();

        REQUIRE( app.probeFd( 0 ) == 0 );
        REQUIRE( app.probeTemperature( 0 ) == Approx( 12.0 ) );
    }

    SECTION( "an unconnected probe triggers checkConnections" )
    {
        app.addProbe( "rack", "00", 0, 12.0 );
        app.state( stateCodes::OPERATING );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.probeFd( 0 ) == 0 );
        REQUIRE( app.shutdown() == 1 );
    }
}

} // namespace usbtempMonTest

} // namespace libXWCTest
