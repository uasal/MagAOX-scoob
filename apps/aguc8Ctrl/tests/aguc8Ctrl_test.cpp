/** \file aguc8Ctrl_test.cpp
 * \brief Catch2 tests for the aguc8Ctrl app.
 * \author Claude Code
 *
 * \ingroup aguc8Ctrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// aguc8Ctrl::readChannelCounts() and writeChannelCounts() use `sysPath` as if it were a data member, but in
// MagAOXApp it is the accessor function `sysPath()` (the data member is `m_sysPath`), so the app header does not
// compile as-is.  libMagAOX is included first (so its own declaration of `sysPath()` is unaffected), and then the
// app header is included with `sysPath` mapped onto the protected member `m_sysPath`.
#include "../../../libMagAOX/libMagAOX.hpp"

#define sysPath m_sysPath
#include "../aguc8Ctrl.hpp"
#undef sysPath

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup aguc8Ctrl_unit_test aguc8Ctrl Unit Tests
 * \brief Unit tests for the aguc8Ctrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `aguc8Ctrl` unit tests.
/** \ingroup aguc8Ctrl_unit_test
 */
namespace aguc8CtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness exposing aguc8Ctrl internals.
class aguc8Ctrl_test : public aguc8Ctrl
{
  public:
    /// Construct a harness with the given device name.
    explicit aguc8Ctrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
    }

    /// Access the application configurator.
    mx::app::appConfigurator &appConfig()
    {
        return config;
    }

    /// Set the system directory used for the channel counts files.
    void setSysPath( const std::string &path /**< [in] the new system path */ )
    {
        m_sysPath = path;
    }

    /// Get the read timeout loaded by the dev::ioDevice base.
    unsigned ioReadTimeout()
    {
        return MagAOX::app::dev::ioDevice::m_readTimeout;
    }

    /// Get the write timeout loaded by the dev::ioDevice base.
    unsigned ioWriteTimeout()
    {
        return MagAOX::app::dev::ioDevice::m_writeTimeout;
    }
};

/// A fake AG-UC8 serial port built on a socket pair.
/** The app side of the pair is handed to the app as `m_fileDescrip`.  A responder thread reads CRLF-terminated
 * commands from the other side, records them, and answers the ones found in the reply table.
 */
class fakeAguc8Port
{
  public:
    /// Create the socket pair.
    fakeAguc8Port()
    {
        if( ::socketpair( AF_UNIX, SOCK_STREAM, 0, m_fds ) < 0 )
        {
            m_fds[0] = -1;
            m_fds[1] = -1;
        }
    }

    /// Stop the responder and close the sockets.
    ~fakeAguc8Port()
    {
        stop();

        if( m_fds[0] >= 0 )
        {
            ::close( m_fds[0] );
        }

        if( m_fds[1] >= 0 )
        {
            ::close( m_fds[1] );
        }
    }

    /// Get the file descriptor to give to the app.
    int appFd() const
    {
        return m_fds[0];
    }

    /// Add a canned reply, which is sent with a trailing CRLF.  Call before start().
    void reply( const std::string &command, /**< [in] the command line, without CRLF */
                const std::string &response /**< [in] the response, without CRLF */ )
    {
        m_replies[command] = response;
    }

    /// Write unsolicited bytes to the app side.
    void push( const std::string &bytes /**< [in] the raw bytes to send */ )
    {
        ssize_t rv = ::write( m_fds[1], bytes.data(), bytes.size() );
        static_cast<void>( rv );
    }

    /// Start the responder thread.
    void start()
    {
        m_thread = std::thread( &fakeAguc8Port::serve, this );
    }

    /// Stop the responder thread.
    void stop()
    {
        if( m_thread.joinable() )
        {
            m_stop = true;
            m_thread.join();
        }
    }

    /// Get the commands received so far.  Call after stop().
    const std::vector<std::string> &commands() const
    {
        return m_commands;
    }

  private:
    /// Responder thread loop.
    void serve()
    {
        std::string buffer;

        while( !m_stop )
        {
            pollfd pfd;
            pfd.fd      = m_fds[1];
            pfd.events  = POLLIN;
            pfd.revents = 0;

            int rv = ::poll( &pfd, 1, 20 );

            if( rv <= 0 )
            {
                continue;
            }

            char    buf[256];
            ssize_t nrd = ::read( m_fds[1], buf, sizeof( buf ) );

            if( nrd <= 0 )
            {
                break;
            }

            buffer.append( buf, nrd );

            size_t pos;
            while( ( pos = buffer.find( "\r\n" ) ) != std::string::npos )
            {
                std::string line = buffer.substr( 0, pos );
                buffer.erase( 0, pos + 2 );

                m_commands.push_back( line );

                auto it = m_replies.find( line );
                if( it != m_replies.end() )
                {
                    std::string out = it->second + "\r\n";
                    ssize_t     nwr = ::write( m_fds[1], out.data(), out.size() );
                    static_cast<void>( nwr );
                }
            }
        }
    }

    int m_fds[2]{ -1, -1 }; ///< The socket pair: [0] is the app side, [1] is the device side.

    std::map<std::string, std::string> m_replies; ///< Canned replies keyed by command line.

    std::vector<std::string> m_commands; ///< Commands received, in order, written only by the responder thread.

    std::atomic<bool> m_stop{ false }; ///< Flag telling the responder thread to exit.

    std::thread m_thread; ///< The responder thread.
};

/// A temporary system directory for the channel counts files, removed on destruction.
struct tmpSysDir
{
    std::string m_path; ///< The system path given to the app.

    std::string m_devDir; ///< The per-device directory, `m_path/<configName>`.

    /// Create the directory tree.
    tmpSysDir( const std::string &tag, /**< [in] a unique tag for this test */
               const std::string &configName /**< [in] the app configuration name */ )
    {
        m_path   = "/tmp/aguc8Ctrl_test_" + tag + "_" + std::to_string( ::getpid() );
        m_devDir = m_path + "/" + configName;
        std::filesystem::create_directories( m_devDir );
    }

    /// Remove the directory tree.
    ~tmpSysDir()
    {
        std::error_code ec;
        std::filesystem::remove_all( m_path, ec );
    }
};

/// Write a config file, read it into the app configurator, and remove the file.
/**
 * \returns the return value of readConfig()
 */
int readTestConfig( aguc8Ctrl_test                 &app,      /**< [in/out] the app, with setupConfig() called */
                    const std::string              &tag,      /**< [in] a unique tag for the file name */
                    const std::vector<std::string> &sections, /**< [in] the config sections */
                    const std::vector<std::string> &keywords, /**< [in] the config keywords */
                    const std::vector<std::string> &values /**< [in] the config values */ )
{
    std::string fname = "/tmp/aguc8Ctrl_test_" + tag + "_" + std::to_string( ::getpid() ) + ".conf";
    mx::app::writeConfigFile( fname, sections, keywords, values );
    int rv = app.appConfig().readConfig( fname );
    ::unlink( fname.c_str() );
    return rv;
}

/// Build a number property with a target element.
pcf::IndiProperty numberProp( const std::string &name, /**< [in] the property name */
                              int                target /**< [in] the target value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "aguc8test" );
    ip.setName( name );
    ip.add( pcf::IndiElement( "target" ) );
    ip["target"].set( target );
    return ip;
}

/// \endcond

/// Verify that loadConfigImpl() reports a configuration without motor sections.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl loadConfigImpl requires motor sections", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::setupConfig();
    aguc8Ctrl::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    SECTION( "only registered keys gives AGUC8CTRL_E_NOMOTORS" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nomotors", { "device" }, { "nChannels" }, { "4" } ) == 0 );

        REQUIRE( app.loadConfigImpl( app.appConfig() ) == AGUC8CTRL_E_NOMOTORS );
    }

    SECTION( "unused sections without a channel keyword are skipped" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nochannel", { "none" }, { "nada" }, { "0" } ) == 0 );

        REQUIRE( app.loadConfigImpl( app.appConfig() ) == 0 );
    }
}

/// Verify the channel number validation in loadConfigImpl(), including the device.nChannels override.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl loadConfigImpl validates channel numbers", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    SECTION( "channel 0 is rejected" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "ch0", { "pupil" }, { "channel" }, { "0" } ) == 0 );

        REQUIRE( app.loadConfigImpl( app.appConfig() ) == AGUC8CTRL_E_BADCHANNEL );
    }

    SECTION( "channel above the default 8 channels is rejected" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "ch9", { "pupil" }, { "channel" }, { "9" } ) == 0 );

        REQUIRE( app.loadConfigImpl( app.appConfig() ) == AGUC8CTRL_E_BADCHANNEL );
    }

    SECTION( "device.nChannels raises the limit" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "ch9n10", { "device", "pupil" }, { "nChannels", "channel" }, { "10", "9" } ) ==
                 0 );

        REQUIRE( app.loadConfigImpl( app.appConfig() ) == 0 );
    }

    SECTION( "device.nChannels lowers the limit" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "ch3n2", { "device", "pupil" }, { "nChannels", "channel" }, { "2", "3" } ) == 0 );

        REQUIRE( app.loadConfigImpl( app.appConfig() ) == AGUC8CTRL_E_BADCHANNEL );
    }

    SECTION( "channels at both ends of the range are accepted" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "chends", { "a", "b" }, { "channel", "channel" }, { "1", "8" } ) == 0 );

        REQUIRE( app.loadConfigImpl( app.appConfig() ) == 0 );
    }
}

/// Verify that the full loadConfig() handles the USB, ioDevice, and motor configuration.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl loadConfig sets shutdown on configuration errors", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::loadConfig();
    #endif
    // clang-format on

    SECTION( "a valid configuration loads without shutdown" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE(
            readTestConfig( app,
                            "full",
                            { "usb", "usb", "usb", "usb", "device", "device", "pupil" },
                            { "idVendor", "idProduct", "serial", "baud", "readTimeout", "writeTimeout", "channel" },
                            { "fff0", "fff1", "NOSUCHSERIAL", "9600", "250", "350", "1" } ) == 0 );

        app.loadConfig();

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_idVendor == "fff0" );
        REQUIRE( app.m_idProduct == "fff1" );
        REQUIRE( app.m_serial == "NOSUCHSERIAL" );
        REQUIRE( app.m_baudRate == B9600 );

        // These are loaded into the dev::ioDevice base.  Note that aguc8Ctrl's own private m_readTimeout and
        // m_writeTimeout, which are what the serial I/O actually uses, shadow these and stay at 1000 ms.
        REQUIRE( app.ioReadTimeout() == 250 );
        REQUIRE( app.ioWriteTimeout() == 350 );
    }

    SECTION( "a missing baud rate is a fatal USB configuration error" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app, "nobaud", { "pupil" }, { "channel" }, { "1" } ) == 0 );

        app.loadConfig();

        REQUIRE( app.shutdown() == 1 );
    }

    SECTION( "a bad motor channel sets shutdown" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.setupConfig();
        REQUIRE( readTestConfig( app,
                                 "badch",
                                 { "usb", "usb", "usb", "pupil" },
                                 { "idVendor", "idProduct", "baud", "channel" },
                                 { "fff0", "fff1", "9600", "12" } ) == 0 );

        app.loadConfig();

        REQUIRE( app.shutdown() != 0 );
    }
}

/// Verify the step size callback rejects bad property names and finds configured channels.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl newCallBack_stepSize channel lookup", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::newCallBack_stepSize( pcf::IndiProperty() );
    aguc8Ctrl::st_newCallBack_stepSize( nullptr, pcf::IndiProperty() );
    #endif
    // clang-format on

    aguc8Ctrl_test app( "aguc8test" );
    app.setupConfig();
    REQUIRE( readTestConfig( app,
                             "stepcb",
                             { "pupil", "pupil", "pupil", "focus" },
                             { "channel", "hwchannel", "hwaxis", "channel" },
                             { "1", "2", "1", "2" } ) == 0 );
    REQUIRE( app.loadConfigImpl( app.appConfig() ) == 0 );

    SECTION( "a property name without _stepSize is rejected" )
    {
        pcf::IndiProperty ip = numberProp( "pupil_pos", 10 );
        REQUIRE( app.newCallBack_stepSize( ip ) == -1 );
    }

    SECTION( "an unknown channel is rejected" )
    {
        pcf::IndiProperty ip = numberProp( "other_stepSize", 10 );
        REQUIRE( aguc8Ctrl::st_newCallBack_stepSize( static_cast<aguc8Ctrl *>( &app ), ip ) == -1 );
    }

    SECTION( "configured channels are found" )
    {
        // A configured channel gets past the lookup to the unchecked `ipRecv["target"]` access, which throws for a
        // property without a target element.  An unknown channel returns -1 before that.
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "aguc8test" );
        ip.setName( "pupil_stepSize" );
        REQUIRE_THROWS_AS( app.newCallBack_stepSize( ip ), std::runtime_error );

        ip.setName( "focus_stepSize" );
        REQUIRE_THROWS_AS( app.newCallBack_stepSize( ip ), std::runtime_error );
    }

    SECTION( "before appStartup the local property does not match" )
    {
        // indiTargetUpdate() fails because the channel's m_indiP_stepSize is only created in appStartup().
        pcf::IndiProperty ip = numberProp( "pupil_stepSize", 20 );
        REQUIRE( app.newCallBack_stepSize( ip ) == -1 );
    }
}

/// Verify the relative position callback rejects bad property names and unknown channels.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl newCallBack_pos validation", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::newCallBack_pos( pcf::IndiProperty() );
    aguc8Ctrl::st_newCallBack_pos( nullptr, pcf::IndiProperty() );
    #endif
    // clang-format on

    aguc8Ctrl_test app( "aguc8test" );
    app.setupConfig();
    REQUIRE( readTestConfig( app, "poscb", { "pupil", "pupil" }, { "channel", "hwaxis" }, { "1", "1" } ) == 0 );
    REQUIRE( app.loadConfigImpl( app.appConfig() ) == 0 );

    SECTION( "a property name without _pos is rejected" )
    {
        REQUIRE( app.newCallBack_pos( numberProp( "pupil_stepSize", 100 ) ) == -1 );
    }

    SECTION( "an unknown channel is rejected" )
    {
        REQUIRE( aguc8Ctrl::st_newCallBack_pos( static_cast<aguc8Ctrl *>( &app ), numberProp( "other_pos", 100 ) ) ==
                 -1 );
    }

    SECTION( "before appStartup the local property does not match" )
    {
        REQUIRE( app.newCallBack_pos( numberProp( "pupil_pos", 100 ) ) == -1 );
    }
}

/// Verify the preset callback channel lookup and multiple-selection rejection.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl newCallBack_presetName selection handling", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::newCallBack_presetName( pcf::IndiProperty() );
    aguc8Ctrl::st_newCallBack_presetName( nullptr, pcf::IndiProperty() );
    #endif
    // clang-format on

    aguc8Ctrl_test app( "aguc8test" );
    app.setupConfig();
    REQUIRE( readTestConfig( app,
                             "presetcb",
                             { "stage", "stage", "stage", "stage" },
                             { "channel", "hwaxis", "names", "positions" },
                             { "3", "1", "in,out", "100,200" } ) == 0 );
    REQUIRE( app.loadConfigImpl( app.appConfig() ) == 0 );

    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "aguc8test" );
    ip.add( pcf::IndiElement( "in" ) );
    ip.add( pcf::IndiElement( "out" ) );

    SECTION( "an unknown channel is rejected" )
    {
        ip.setName( "other" );
        ip["in"].setSwitchState( pcf::IndiElement::On );
        ip["out"].setSwitchState( pcf::IndiElement::Off );

        REQUIRE( aguc8Ctrl::st_newCallBack_presetName( static_cast<aguc8Ctrl *>( &app ), ip ) == -1 );
    }

    SECTION( "selecting more than one preset is rejected" )
    {
        ip.setName( "stage" );
        ip["in"].setSwitchState( pcf::IndiElement::On );
        ip["out"].setSwitchState( pcf::IndiElement::On );

        REQUIRE( app.newCallBack_presetName( ip ) == -1 );
    }

    SECTION( "a single selection reaches the target update" )
    {
        // The configured preset names were parsed (a single "On" passes the multiple-selection check).  Before
        // appStartup() the channel's m_property has no "target" element, so the unchecked element access throws.
        ip.setName( "stage" );
        ip["in"].setSwitchState( pcf::IndiElement::Off );
        ip["out"].setSwitchState( pcf::IndiElement::On );

        REQUIRE_THROWS_AS( app.newCallBack_presetName( ip ), std::runtime_error );
    }
}

/// Verify the channel counts are written to and read back from the system directory.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl channel counts persistence", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::readChannelCounts( std::string() );
    aguc8Ctrl::writeChannelCounts( std::string(), 0 );
    #endif
    // clang-format on

    aguc8Ctrl_test app( "aguc8test" );
    tmpSysDir      sysDir( "counts", "aguc8test" );
    app.setSysPath( sysDir.m_path );

    SECTION( "a missing counts file initializes to 0" )
    {
        REQUIRE( app.readChannelCounts( "pupil" ) == 0 );
    }

    SECTION( "counts round trip through the file" )
    {
        REQUIRE( app.writeChannelCounts( "pupil", 12345 ) == 0 );
        REQUIRE( std::filesystem::exists( sysDir.m_devDir + "/pupil" ) );
        REQUIRE( app.readChannelCounts( "pupil" ) == 12345 );

        REQUIRE( app.writeChannelCounts( "pupil", -678 ) == 0 );
        REQUIRE( app.readChannelCounts( "pupil" ) == -678 );

        std::ifstream fin( sysDir.m_devDir + "/pupil" );
        std::string   contents;
        fin >> contents;
        REQUIRE( contents == "-678" );
    }

    SECTION( "channels are stored in separate files" )
    {
        REQUIRE( app.writeChannelCounts( "pupil", 10 ) == 0 );
        REQUIRE( app.writeChannelCounts( "focus", 20 ) == 0 );
        REQUIRE( app.readChannelCounts( "pupil" ) == 10 );
        REQUIRE( app.readChannelCounts( "focus" ) == 20 );
    }

    SECTION( "an unwritable directory is an error" )
    {
        app.setSysPath( sysDir.m_path + "/does/not/exist" );
        REQUIRE( app.writeChannelCounts( "pupil", 5 ) == -1 );
    }
}

/// Verify the serial query, write-with-error-check, and read helpers on a fake port.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl serial I/O helpers", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::writeQuery( std::string(), std::string() );
    aguc8Ctrl::writeReadError( std::string(), std::string() );
    aguc8Ctrl::Read( std::string() );
    #endif
    // clang-format on

    SECTION( "writeQuery sends the command with CRLF and returns the reply" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        fakeAguc8Port  port;
        port.reply( "1TS", "1TS0" );
        port.start();
        app.m_fileDescrip = port.appFd();

        std::string resp;
        REQUIRE( app.writeQuery( "1TS", resp ) == TTY_E_NOERROR );
        REQUIRE( resp == "1TS0\r\n" );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "1TS" } );
    }

    SECTION( "writeReadError sends the command then TE and returns the TE reply" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        fakeAguc8Port  port;
        port.reply( "TE", "TE0" );
        port.start();
        app.m_fileDescrip = port.appFd();

        std::string resp;
        REQUIRE( app.writeReadError( "MR", resp ) == TTY_E_NOERROR );
        REQUIRE( resp == "TE0\r\n" );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "MR", "TE" } );
    }

    SECTION( "writeReadError reports a read timeout when TE is not answered" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        fakeAguc8Port  port;
        port.start();
        app.m_fileDescrip = port.appFd();

        std::string resp;
        REQUIRE( app.writeReadError( "MR", resp ) == TTY_E_TIMEOUTONREADPOLL );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "MR", "TE" } );
    }

    SECTION( "Read returns pending bytes up to CRLF" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        fakeAguc8Port  port;
        app.m_fileDescrip = port.appFd();
        port.push( "CC2\r\n" );

        std::string resp;
        REQUIRE( app.Read( resp ) == TTY_E_NOERROR );
        REQUIRE( resp == "CC2\r\n" );
    }
}

/// Verify the command sequence sent by setStepSize().
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl setStepSize command sequence", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::setStepSize( 0, 0, 0, true );
    #endif
    // clang-format on

    aguc8Ctrl_test app( "aguc8test" );
    fakeAguc8Port  port;
    port.reply( "TE", "TE0" );
    port.start();
    app.m_fileDescrip = port.appFd();

    REQUIRE( app.setStepSize( 25, 3, 2, true ) == 0 );

    port.stop();
    REQUIRE( port.commands() == std::vector<std::string>{ "CC3", "TE", "2SU+25", "TE", "2SU-25", "TE" } );
}

/// Verify the appLogic() power-on and connection steps without a USB device.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl appLogic connection states", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::appLogic();
    aguc8Ctrl::onPowerOff();
    aguc8Ctrl::whilePowerOff();
    #endif
    // clang-format on

    SECTION( "INITIALIZED advances to NOTCONNECTED when the device cannot be opened" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        app.state( stateCodes::INITIALIZED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );

        // and stays there
        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "CONNECTED sets remote mode and goes READY" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        fakeAguc8Port  port;
        port.reply( "TE", "TE0" );
        port.start();
        app.m_fileDescrip = port.appFd();
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::READY );

        port.stop();
        REQUIRE( port.commands() == std::vector<std::string>{ "MR", "TE" } );
    }

    SECTION( "CONNECTED goes to ERROR when the controller does not answer" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        fakeAguc8Port  port;
        port.start();
        app.m_fileDescrip = port.appFd();
        app.state( stateCodes::CONNECTED );

        REQUIRE( app.appLogic() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "the power-off hooks do nothing" )
    {
        aguc8Ctrl_test app( "aguc8test" );
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( app.whilePowerOff() == 0 );
    }
}

/// Verify the READY-state appLogic() queries each axis and stores the channel counts.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl appLogic READY polls motion status", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::appLogic();
    aguc8Ctrl::writeChannelCounts( std::string(), 0 );
    #endif
    // clang-format on

    aguc8Ctrl_test app( "aguc8test" );
    app.setupConfig();
    REQUIRE( readTestConfig( app,
                             "ready",
                             { "pupil", "pupil", "focus", "focus" },
                             { "channel", "hwaxis", "channel", "hwaxis" },
                             { "1", "1", "2", "2" } ) == 0 );
    REQUIRE( app.loadConfigImpl( app.appConfig() ) == 0 );

    tmpSysDir sysDir( "ready", "aguc8test" );
    app.setSysPath( sysDir.m_path );

    fakeAguc8Port port;
    port.reply( "1TS", "1TS0" );
    port.reply( "2TS", "2TS0" );
    port.start();
    app.m_fileDescrip = port.appFd();

    app.state( stateCodes::READY );

    REQUIRE( app.appLogic() == 0 );

    port.stop();

    // The channel map is ordered by name, so focus (axis 2) is polled before pupil (axis 1).
    REQUIRE( port.commands() == std::vector<std::string>{ "2TS", "1TS" } );

    // The current counts (0, since appStartup() did not read any) are stored for each channel.
    REQUIRE( app.readChannelCounts( "pupil" ) == 0 );
    REQUIRE( app.readChannelCounts( "focus" ) == 0 );
    REQUIRE( std::filesystem::exists( sysDir.m_devDir + "/pupil" ) );
    REQUIRE( std::filesystem::exists( sysDir.m_devDir + "/focus" ) );

    // Without appStartup() the telemetry log thread is not running, so the telemeter logic fails the app.
    REQUIRE( app.state() == stateCodes::FAILURE );
    REQUIRE( app.shutdown() == 1 );
}

/// Verify the telemetry recording hooks.
/**
 * \ingroup aguc8Ctrl_unit_test
 */
TEST_CASE( "aguc8Ctrl telemetry recording", "[aguc8Ctrl]" )
{
    // clang-format off
    #ifdef AGUC8CTRL_TEST_DOXYGEN_REF
    aguc8Ctrl::recordAGUC8( true );
    aguc8Ctrl::recordTelem( nullptr );
    aguc8Ctrl::checkRecordTimes();
    #endif
    // clang-format on

    aguc8Ctrl_test app( "aguc8test" );

    SECTION( "a forced record updates the telemetry timestamp" )
    {
        telem_pico::lastRecord = { 0, 0 };
        REQUIRE( app.recordAGUC8( true ) == 0 );
        REQUIRE( telem_pico::lastRecord.tv_sec > 0 );
    }

    SECTION( "recordTelem forces a record" )
    {
        telem_pico::lastRecord = { 0, 0 };
        REQUIRE( app.recordTelem( static_cast<const telem_pico *>( nullptr ) ) == 0 );
        REQUIRE( telem_pico::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records only after the interval has elapsed" )
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

} // namespace aguc8CtrlTest

} // namespace libXWCTest
