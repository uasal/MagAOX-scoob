/** \file flipperCtrl_test.cpp
 * \brief Catch2 tests for the flipperCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * \ingroup flipperCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../flipperCtrl.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup flipperCtrl_unit_test flipperCtrl Unit Tests
 * \brief Unit tests for the flipperCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `flipperCtrl` unit tests.
/** \ingroup flipperCtrl_unit_test
 */
namespace flipperCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Directory the test harness points the telemetry logger at.
const char *telemPath = "/tmp/flipperCtrl_test_telem";

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

/// Test harness exposing flipperCtrl internals.
class flipperCtrl_test : public flipperCtrl
{
  public:
    /// Construct a harness with the given device name, and set up the position property as appStartup() does.
    explicit flipperCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        static_cast<void>( m_tel.logPath( telemPath ) );
        createStandardIndiSelectionSw( m_indiP_position, "presetName", { "in", "out" } );
    }

    /// Run setupConfig(), read the given config file, and run loadConfig().
    void configure( const std::string &file /**< [in] path of the config file to read */ )
    {
        setupConfig();
        config.readConfig( file );
        loadConfig();
        static_cast<void>( m_tel.logPath( telemPath ) );
    }

    /// Get the configured "in" position.
    int inPos() const
    {
        return m_inPos;
    }

    /// Get the configured "out" position.
    int outPos() const
    {
        return m_outPos;
    }

    /// Get the current position.
    int pos() const
    {
        return m_pos;
    }

    /// Set the current position.
    void pos( int p /**< [in] the new current position */ )
    {
        m_pos = p;
    }

    /// Get the target position.
    int tgt() const
    {
        return m_tgt;
    }

    /// Set the target position.
    void tgt( int t /**< [in] the new target position */ )
    {
        m_tgt = t;
    }

    /// Get the shutdown flag.
    int shutdown() const
    {
        return m_shutdown;
    }

    /// Get the power management flag.
    bool powerMgtEnabled() const
    {
        return m_powerMgtEnabled;
    }
};

/// A fake flipper device on the far end of a socket pair.
/** The app writes to and reads from appFd(), the fake answers on the other end.
 */
class fakeFlipper
{
  public:
    /// Create the socket pair.
    fakeFlipper()
    {
        m_ok = ( socketpair( AF_UNIX, SOCK_STREAM, 0, m_fds ) == 0 );
    }

    /// Join the responder thread and close the sockets.
    ~fakeFlipper()
    {
        join();
        if( m_ok )
        {
            close( m_fds[0] );
            close( m_fds[1] );
        }
    }

    /// Whether the socket pair was created.
    bool ok() const
    {
        return m_ok;
    }

    /// The file descriptor for the app to use.
    int appFd() const
    {
        return m_fds[0];
    }

    /// Start a thread which answers nCommands 6-byte commands with a 20-byte status, byte 16 set to posByte.
    void respond( int  nCommands, /**< [in] the number of commands to answer */
                  char posByte /**< [in] the value of byte 16 of the status response */ )
    {
        m_thread = std::thread(
            [this, nCommands, posByte]()
            {
                for( int n = 0; n < nCommands; ++n )
                {
                    std::string cmd = readBytes( 6, 2000 );
                    m_received += cmd;
                    if( cmd.size() < 6 )
                    {
                        return;
                    }

                    std::string resp( 20, '\0' );
                    resp[16] = posByte;
                    if( write( m_fds[1], resp.data(), resp.size() ) < 0 )
                    {
                        return;
                    }
                }
            } );
    }

    /// Wait for the responder thread to finish.
    void join()
    {
        if( m_thread.joinable() )
        {
            m_thread.join();
        }
    }

    /// Read up to nBytes of what the app sent, waiting at most timeoutMs for each chunk.
    std::string readBytes( size_t nBytes, /**< [in] the maximum number of bytes to read */
                           int    timeoutMs /**< [in] the per-chunk poll timeout in msec */ )
    {
        std::string out;
        while( out.size() < nBytes )
        {
            pollfd pfd;
            pfd.fd      = m_fds[1];
            pfd.events  = POLLIN;
            pfd.revents = 0;
            if( poll( &pfd, 1, timeoutMs ) <= 0 )
            {
                break;
            }

            char    buf[64];
            ssize_t rv = read( m_fds[1], buf, std::min( sizeof( buf ), nBytes - out.size() ) );
            if( rv <= 0 )
            {
                break;
            }
            out.append( buf, rv );
        }
        return out;
    }

    /// The commands received by the responder thread.  Only valid after join().
    const std::string &received() const
    {
        return m_received;
    }

  private:
    int         m_fds[2]{ -1, -1 }; ///< The socket pair: [0] is the app end, [1] is the fake device end.
    bool        m_ok{ false };      ///< True if the socket pair was created.
    std::thread m_thread;           ///< The responder thread.
    std::string m_received;         ///< Commands received by the responder thread.
};

/// Build the expected 6-byte flipper command.
std::string flipperCmd( unsigned char b0, /**< [in] first byte (message id low byte) */
                        unsigned char b3 /**< [in] fourth byte (parameter 2) */ )
{
    std::string cmd( 6, '\0' );
    cmd[0] = static_cast<char>( b0 );
    cmd[1] = 0x04;
    cmd[2] = 0x00;
    cmd[3] = static_cast<char>( b3 );
    cmd[4] = 0x50;
    cmd[5] = 0x01;
    return cmd;
}

/// \endcond

/// Verify flipperCtrl construction defaults.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl construction defaults", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl();
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );

    REQUIRE( app.inPos() == 1 );
    REQUIRE( app.outPos() == 2 );
    REQUIRE( app.pos() == 1 );
    REQUIRE( app.tgt() == 0 );
    REQUIRE( app.powerMgtEnabled() == true );
    REQUIRE( app.m_indiP_position.getName() == "presetName" );
    REQUIRE( app.m_indiP_position.getDevice() == "flipper" );
    REQUIRE( app.m_indiP_position.find( "in" ) );
    REQUIRE( app.m_indiP_position.find( "out" ) );
}

/// Verify flipperCtrl configuration defaults.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl configuration defaults", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::setupConfig();
    flipperCtrl::loadConfig();
    flipperCtrl::loadConfigImpl(config);
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );

    mx::app::writeConfigFile( "/tmp/flipperCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.configure( "/tmp/flipperCtrl_test_defaults.conf" );
    std::remove( "/tmp/flipperCtrl_test_defaults.conf" );

    REQUIRE( app.shutdown() == 0 );
    REQUIRE( app.m_baudRate == B115200 );
    REQUIRE( app.inPos() == 1 );
    REQUIRE( app.outPos() == 2 );
    REQUIRE( app.m_readTimeout == 1000 );
    REQUIRE( app.m_writeTimeout == 1000 );
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
}

/// Verify flipperCtrl configuration overrides, including reversed positions.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl configuration overrides", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::setupConfig();
    flipperCtrl::loadConfig();
    flipperCtrl::loadConfigImpl(config);
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );

    mx::app::writeConfigFile(
        "/tmp/flipperCtrl_test_override.conf",
        { "flipper", "usb", "usb", "usb", "usb", "device", "device", "telemeter" },
        { "reverse", "idVendor", "idProduct", "serial", "baud", "readTimeout", "writeTimeout", "maxInterval" },
        { "true", "ffff", "fffe", "SN123", "9600", "250", "300", "5.5" } );
    app.configure( "/tmp/flipperCtrl_test_override.conf" );
    std::remove( "/tmp/flipperCtrl_test_override.conf" );

    REQUIRE( app.shutdown() == 0 );
    REQUIRE( app.inPos() == 2 );
    REQUIRE( app.outPos() == 1 );
    REQUIRE( app.m_idVendor == "ffff" );
    REQUIRE( app.m_idProduct == "fffe" );
    REQUIRE( app.m_serial == "SN123" );
    REQUIRE( app.m_baudRate == B9600 );
    REQUIRE( app.m_readTimeout == 250 );
    REQUIRE( app.m_writeTimeout == 300 );
    REQUIRE( app.m_maxInterval == Approx( 5.5 ) );
}

/// Verify moveTo() sends the correct move command bytes and rejects invalid positions.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl moveTo sends the move command", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::moveTo(1);
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );
    fakeFlipper      dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    SECTION( "position 1" )
    {
        REQUIRE( app.moveTo( 1 ) == 0 );
        REQUIRE( dev.readBytes( 6, 500 ) == flipperCmd( 0x6A, 0x01 ) );
    }

    SECTION( "position 2" )
    {
        REQUIRE( app.moveTo( 2 ) == 0 );
        REQUIRE( dev.readBytes( 6, 500 ) == flipperCmd( 0x6A, 0x02 ) );
    }

    SECTION( "invalid positions send nothing" )
    {
        REQUIRE( app.moveTo( 0 ) == -1 );
        REQUIRE( app.moveTo( 3 ) == -1 );
        REQUIRE( app.moveTo( -1 ) == -1 );
        REQUIRE( dev.readBytes( 6, 100 ).empty() );
    }

    app.m_fileDescrip = 0;
}

/// Verify getPos() sends the status request and maps the status byte to a position.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl getPos maps the status response", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::getPos();
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );
    fakeFlipper      dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    SECTION( "status byte 1 is position 1" )
    {
        app.pos( 2 );
        dev.respond( 1, 1 );
        REQUIRE( app.getPos() == 0 );
        dev.join();
        REQUIRE( app.pos() == 1 );
        REQUIRE( dev.received() == flipperCmd( 0x80, 0x00 ) );
    }

    SECTION( "status byte 2 is position 2" )
    {
        app.pos( 1 );
        dev.respond( 1, 2 );
        REQUIRE( app.getPos() == 0 );
        dev.join();
        REQUIRE( app.pos() == 2 );
    }

    SECTION( "any status byte other than 1 is position 2" )
    {
        app.pos( 1 );
        dev.respond( 1, 0 );
        REQUIRE( app.getPos() == 0 );
        dev.join();
        REQUIRE( app.pos() == 2 );
    }

    app.m_fileDescrip = 0;
}

/// Verify recordStage() only records telemetry on change or when forced.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl recordStage records on change or force", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::recordStage(true);
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );

    app.pos( 1 );
    app.tgt( 1 );

    // A forced record always happens and sets the change-detection state.
    MagAOX::logger::telem_stage::lastRecord = { 0, 0 };
    REQUIRE( app.recordStage( true ) == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );

    // No change: nothing recorded.
    MagAOX::logger::telem_stage::lastRecord = { 0, 0 };
    REQUIRE( app.recordStage( false ) == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec == 0 );

    // Moving state changes: recorded.
    app.tgt( 2 );
    REQUIRE( app.recordStage( false ) == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );

    // Position changes: recorded.
    MagAOX::logger::telem_stage::lastRecord = { 0, 0 };
    app.pos( 2 );
    REQUIRE( app.recordStage( false ) == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );

    // Forced with no change: recorded.
    MagAOX::logger::telem_stage::lastRecord = { 0, 0 };
    REQUIRE( app.recordStage( true ) == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );
}

/// Verify recordTelem() forces a record and checkRecordTimes() records only when the interval has elapsed.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl telemetry interval handling", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::recordTelem(nullptr);
    flipperCtrl::checkRecordTimes();
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );

    MagAOX::logger::telem_stage::lastRecord = { 0, 0 };
    REQUIRE( app.recordTelem( nullptr ) == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );

    // Stale last record: checkRecordTimes records.
    MagAOX::logger::telem_stage::lastRecord = { 0, 0 };
    REQUIRE( app.checkRecordTimes() == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec != 0 );

    // Fresh last record: checkRecordTimes does not record.
    timespec fresh = MagAOX::logger::telem_stage::lastRecord;
    REQUIRE( app.checkRecordTimes() == 0 );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_sec == fresh.tv_sec );
    REQUIRE( MagAOX::logger::telem_stage::lastRecord.tv_nsec == fresh.tv_nsec );
}

/// Verify the position new-callback rejects the wrong property and ignores requests with no switch on.
/**
 * Requests that turn a switch on call `m_indiDriver->sendSetProperty()` unconditionally, which requires a running
 * INDI driver, so they are not exercised here.
 *
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl position new callback", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::newCallBack_m_indiP_position(pcf::IndiProperty());
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );
    app.tgt( 0 );

    SECTION( "wrong name is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "flipper" );
        ip.setName( "wrong" );
        ip.add( pcf::IndiElement( "in", pcf::IndiElement::On ) );
        REQUIRE( app.newCallBack_m_indiP_position( ip ) == -1 );
        REQUIRE( app.tgt() == 0 );
    }

    SECTION( "no elements is a no-op" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "flipper" );
        ip.setName( "presetName" );
        REQUIRE( app.newCallBack_m_indiP_position( ip ) == 0 );
        REQUIRE( app.tgt() == 0 );
    }

    SECTION( "all switches off is a no-op" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "flipper" );
        ip.setName( "presetName" );
        ip.add( pcf::IndiElement( "in", pcf::IndiElement::Off ) );
        ip.add( pcf::IndiElement( "out", pcf::IndiElement::Off ) );
        REQUIRE( app.newCallBack_m_indiP_position( ip ) == 0 );
        REQUIRE( app.tgt() == 0 );
    }

    SECTION( "the device is not checked" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Switch );
        ip.setDevice( "other" );
        ip.setName( "presetName" );
        REQUIRE( app.newCallBack_m_indiP_position( ip ) == 0 );
    }
}

/// Verify appLogic() moves from POWERON to NODEVICE when the USB device is not present.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl appLogic with no USB device", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::appLogic();
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );

    // IDs which match no real device.
    app.m_idVendor  = "fffe";
    app.m_idProduct = "fffd";
    app.m_serial    = "flipperCtrl_test";

    SECTION( "POWERON goes to NODEVICE" )
    {
        app.state( stateCodes::POWERON );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
    }

    SECTION( "NOTCONNECTED with a failed connect goes back to NODEVICE" )
    {
        app.state( stateCodes::NOTCONNECTED );
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_fileDescrip == 0 );
    }
}

/// Verify appLogic() reads the position when CONNECTED and READY.
/**
 * The telemetry thread is not running in the test, so `telemeter::appLogic()` sets FAILURE and `m_shutdown` at the
 * end of the READY branch.
 *
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl appLogic reads the position", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::appLogic();
    flipperCtrl::getPos();
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );
    fakeFlipper      dev;
    REQUIRE( dev.ok() );
    app.m_fileDescrip = dev.appFd();

    SECTION( "CONNECTED sets the target to the current position" )
    {
        app.pos( 1 );
        app.tgt( 0 );
        app.state( stateCodes::CONNECTED );

        // One status request in CONNECTED, one in the READY branch that follows.
        dev.respond( 2, 2 );
        REQUIRE( app.appLogic() == 0 );
        dev.join();

        REQUIRE( app.pos() == 2 );
        REQUIRE( app.tgt() == 2 );
        REQUIRE( dev.received() == flipperCmd( 0x80, 0x00 ) + flipperCmd( 0x80, 0x00 ) );
        REQUIRE( app.shutdown() == 1 );
    }

    SECTION( "READY updates the current position" )
    {
        app.pos( 2 );
        app.tgt( 1 );
        app.state( stateCodes::READY );

        dev.respond( 1, 1 );
        REQUIRE( app.appLogic() == 0 );
        dev.join();

        REQUIRE( app.pos() == 1 );
        REQUIRE( app.tgt() == 1 );
        REQUIRE( dev.received() == flipperCmd( 0x80, 0x00 ) );
    }

    app.m_fileDescrip = 0;
}

/// Verify appShutdown() succeeds.
/**
 * \ingroup flipperCtrl_unit_test
 */
TEST_CASE( "flipperCtrl appShutdown", "[flipperCtrl]" )
{
    // clang-format off
    #ifdef FLIPPERCTRL_TEST_DOXYGEN_REF
    flipperCtrl::appShutdown();
    #endif
    // clang-format on

    flipperCtrl_test app( "flipper" );
    REQUIRE( app.appShutdown() == 0 );
}

} // namespace flipperCtrlTest

} // namespace libXWCTest
