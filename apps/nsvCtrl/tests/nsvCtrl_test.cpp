/** \file nsvCtrl_test.cpp
 * \brief Catch2 tests for the nsvCtrl app.
 * \author Claude Code
 */

#include "../../../tests/testXWC.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#define protected public
#include "../nsvCtrl.hpp"
#undef protected

// Included after the app header so that the INDI callback bodies stay live.
#include "../../../tests/testMacrosINDI.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup nsvCtrl_unit_test nsvCtrl Unit Tests
 * \brief Unit tests for the nsvCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `nsvCtrl` unit tests.
/** \ingroup nsvCtrl_unit_test
 */
namespace nsvCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// A camera path that must not exist, so no V4L2 device is ever opened.
const char *const c_fakeCamPath = "/tmp/nsvCtrl_test_no_such_video_device";

/// Reset the global state of v4l2lib.hpp so no call touches a real file descriptor.
void resetV4l2Globals()
{
    ::fd              = -1; // never leave this at 0 (stdin)
    ::bufferCount     = 0;
    ::bufferSize      = 0;
    ::currentBufIndex = -1;
    ::stream_on       = false;
    ::buffers.clear();
    ::params = CameraParams{};
}

/// Fake `v4l2-ctl` and `i2ctransfer` executables placed first on PATH.
/** Each fake appends `<name> <args>` to a log file, prints `NSVCTRL_TEST_OUT` if it is non-empty, and exits
 * with `NSVCTRL_TEST_RC`.  The constructor installs them and the destructor restores PATH and removes them.
 */
class fakeTools
{
  public:
    /// Install the fake tools and prepend their directory to PATH.
    fakeTools()
    {
        m_dir = "/tmp/nsvCtrl_test_bin_" + std::to_string( getpid() );
        mkdir( m_dir.c_str(), 0700 );

        m_log = m_dir + "/calls.log";

        writeScript( "v4l2-ctl" );
        writeScript( "i2ctransfer" );

        const char *path = getenv( "PATH" );
        m_hadPath        = ( path != nullptr );
        if( path )
        {
            m_oldPath = path;
        }

        std::string newPath = m_dir + ":" + m_oldPath;
        setenv( "PATH", newPath.c_str(), 1 );
        setenv( "NSVCTRL_TEST_LOG", m_log.c_str(), 1 );

        output( "" );
        returnCode( 0 );
        clearLog();
    }

    /// Restore PATH and remove the fake tools.
    ~fakeTools()
    {
        if( m_hadPath )
        {
            setenv( "PATH", m_oldPath.c_str(), 1 );
        }
        else
        {
            unsetenv( "PATH" );
        }

        unsetenv( "NSVCTRL_TEST_LOG" );
        unsetenv( "NSVCTRL_TEST_OUT" );
        unsetenv( "NSVCTRL_TEST_RC" );

        ::remove( ( m_dir + "/v4l2-ctl" ).c_str() );
        ::remove( ( m_dir + "/i2ctransfer" ).c_str() );
        ::remove( m_log.c_str() );
        rmdir( m_dir.c_str() );
    }

    /// Set the text the fake tools print to stdout.
    void output( const std::string &out /**< [in] the text to print, empty for none */ )
    {
        setenv( "NSVCTRL_TEST_OUT", out.c_str(), 1 );
    }

    /// Set the exit code of the fake tools.
    void returnCode( int rc /**< [in] the exit code */ )
    {
        setenv( "NSVCTRL_TEST_RC", std::to_string( rc ).c_str(), 1 );
    }

    /// Remove all logged calls.
    void clearLog()
    {
        ::remove( m_log.c_str() );
    }

    /// Get the logged calls, one `<name> <args>` string per call.
    std::vector<std::string> calls()
    {
        std::vector<std::string> lines;
        std::ifstream            fin( m_log );
        std::string              line;
        while( std::getline( fin, line ) )
        {
            lines.push_back( line );
        }
        return lines;
    }

    /// Check that the shell resolves a tool name to the fake, so no real tool can run.
    bool resolvesToFake( const std::string &name /**< [in] the tool name */ )
    {
        std::string cmd  = "command -v " + name;
        FILE       *pipe = popen( cmd.c_str(), "r" );
        if( !pipe )
        {
            return false;
        }

        std::array<char, 512> buf;
        std::string           res;
        while( fgets( buf.data(), buf.size(), pipe ) != nullptr )
        {
            res += buf.data();
        }
        pclose( pipe );

        while( !res.empty() && ( res.back() == '\n' || res.back() == '\r' ) )
        {
            res.pop_back();
        }

        return res == m_dir + "/" + name;
    }

  protected:
    std::string m_dir;              ///< Directory holding the fake tools.
    std::string m_log;              ///< Path of the call log.
    std::string m_oldPath;          ///< PATH before installation.
    bool        m_hadPath{ false }; ///< Whether PATH was set before installation.

    /// Write one fake tool script.
    void writeScript( const std::string &name /**< [in] the tool name */ )
    {
        std::string   fname = m_dir + "/" + name;
        std::ofstream fout( fname );
        fout << "#!/bin/sh\n";
        fout << "echo \"${0##*/} $*\" >> \"$NSVCTRL_TEST_LOG\"\n";
        fout << "if [ -n \"$NSVCTRL_TEST_OUT\" ]; then\n";
        fout << "    echo \"$NSVCTRL_TEST_OUT\"\n";
        fout << "fi\n";
        fout << "exit ${NSVCTRL_TEST_RC:-0}\n";
        fout.close();
        chmod( fname.c_str(), 0700 );
    }
};

/// Test harness for nsvCtrl, sets the device name and the INDI properties created in appStartup.
class nsvCtrl_test : public nsvCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit nsvCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        resetV4l2Globals();

        m_camPath  = c_fakeCamPath;
        m_vCrop    = 0;
        m_bitDepth = 16;

        XWCTEST_SETUP_INDI_NEW_PROP( power );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_vCrop, vcropoffset );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_indiP_bitDepth, bitDepth );
    }

    /// D'tor, resets the v4l2lib globals which may point to test-owned memory.
    ~nsvCtrl_test()
    {
        resetV4l2Globals();
        ROIbuffers.clear();
    }
};

/// Build a number property with optional `current` and `target` elements.
pcf::IndiProperty makeNumberProp( const std::string &device /**< [in] property device */,
                                  const std::string &name /**< [in] property name */,
                                  bool               addCurrent /**< [in] whether to add a `current` element */,
                                  double             current /**< [in] value of the `current` element */,
                                  bool               addTarget /**< [in] whether to add a `target` element */,
                                  double             target /**< [in] value of the `target` element */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );

    if( addCurrent )
    {
        ip.add( pcf::IndiElement( "current" ) );
        ip["current"].setValue( current );
    }

    if( addTarget )
    {
        ip.add( pcf::IndiElement( "target" ) );
        ip["target"].setValue( target );
    }

    return ip;
}

/// Read the value written after `key:` in an nsvCam config file, or "<missing>".
std::string readCfgValue( const std::string &fname /**< [in] the config file */,
                          const std::string &key /**< [in] the key, without the colon */ )
{
    std::ifstream fin( fname );
    std::string   line;
    while( std::getline( fin, line ) )
    {
        if( line.rfind( key + ":", 0 ) == 0 )
        {
            std::string val = line.substr( key.size() + 1 );
            size_t      st  = val.find_first_not_of( ' ' );
            if( st == std::string::npos )
            {
                return "";
            }
            return val.substr( st );
        }
    }
    return "<missing>";
}

/// Check whether a file exists.
bool fileExists( const std::string &fname /**< [in] the file path */ )
{
    struct stat st;
    return stat( fname.c_str(), &st ) == 0;
}

/// \endcond

/// Verify the constructor defaults.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl construction defaults", "[nsvCtrl]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::nsvCtrl();
    nsvCtrl::fps();
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test" );

    REQUIRE( app.m_current_frame == 0 );
    REQUIRE( app.m_oldest_frame == 0 );
    REQUIRE( app.m_powerMgtEnabled == false );
    REQUIRE( app.m_powerOnWait == 0 );
    REQUIRE( app.m_maxEMGain == Approx( 360 ) );
    REQUIRE( app.m_emGainSet == Approx( 100 ) );
    REQUIRE( app.m_blacklevelSet == Approx( 10 ) );
    REQUIRE( app.m_maxBlacklevel == Approx( 65535 ) );
    REQUIRE( app.m_minBlacklevel == Approx( 0 ) );
    REQUIRE( app.m_maxExpTime == Approx( 3600000000.0 ) );
    REQUIRE( app.m_minExpTime == Approx( 69 ) );
    REQUIRE( app.m_powerCycles == 0 );
    REQUIRE( app.m_init == false );
    REQUIRE( app.m_poweredOn == false );
    REQUIRE( app.m_power == false );

    app.m_fps = 123;
    REQUIRE( app.fps() == Approx( 123 ) );
}

/// Verify configuration loading, including camera modes and the temporary config file.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl configuration", "[nsvCtrl][config]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::setupConfig();
    nsvCtrl::loadConfig();
    nsvCtrl::writeConfig();
    #endif
    // clang-format on

    const std::string cfgOut = "/tmp/nsvCam_nsvCtrl_test.cfg";

    SECTION( "options, camera mode and base-class settings" )
    {
        nsvCtrl_test app( "nsvCtrl_test" );
        app.setupConfig();

        const char *fname = "/tmp/nsvCtrl_test_config.conf";
        mx::app::writeConfigFile( fname,
                                  { "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "fullframe",
                                    "fullframe",
                                    "fullframe",
                                    "fullframe",
                                    "fullframe",
                                    "fullframe",
                                    "framegrabber",
                                    "telemeter" },
                                  { "camPath",
                                    "vcropoffset",
                                    "bitDepth",
                                    "power",
                                    "startupMode",
                                    "maxEMGain",
                                    "configFile",
                                    "centerX",
                                    "centerY",
                                    "sizeX",
                                    "sizeY",
                                    "maxFPS",
                                    "circBuffLength",
                                    "maxInterval" },
                                  { "/tmp/nsvCtrl_test_cfg_video",
                                    "200",
                                    "10",
                                    "true",
                                    "fullframe",
                                    "500",
                                    "none",
                                    "1000",
                                    "500",
                                    "2000",
                                    "1000",
                                    "60",
                                    "4",
                                    "3" } );
        app.config.readConfig( fname );

        app.loadConfig();

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_camPath == "/tmp/nsvCtrl_test_cfg_video" );
        REQUIRE( app.m_vCrop == 200 );
        REQUIRE( app.m_bitDepth == 10 );
        REQUIRE( app.m_power == true );
        REQUIRE( app.config.isSet( "camera.vcropoffset" ) );

        // maxEMGain is clamped to 360
        REQUIRE( app.m_maxEMGain == Approx( 360 ) );

        // startup mode
        REQUIRE( app.m_startupMode == "fullframe" );
        REQUIRE( app.m_modeName == "fullframe" );
        REQUIRE( app.m_nextMode == "fullframe" );
        REQUIRE( app.m_cameraModes.count( "fullframe" ) == 1 );

        // full frame and fps limits come from the mode
        REQUIRE( app.m_full_x == Approx( 1000 ) );
        REQUIRE( app.m_full_y == Approx( 500 ) );
        REQUIRE( app.m_full_w == 2000 );
        REQUIRE( app.m_full_h == 1000 );
        REQUIRE( app.m_maxFPS == Approx( 60 ) );
        REQUIRE( app.m_minFPS == Approx( 60 ) ); // note: minFPS is also set from the mode maxFPS

        // camera.full_* are not registered options, so stdCamera::loadConfig returns early and the
        // default ROI is left at 0.  nsvCtrl then copies that default into the current ROI.
        REQUIRE( app.m_currentROI.w == 0 );
        REQUIRE( app.m_currentROI.h == 0 );
        REQUIRE( app.m_currentROI.bin_x == 1 );
        REQUIRE( app.m_currentROI.bin_y == 1 );

        REQUIRE( app.m_configFile == "/tmp/nsv_nsvCtrl_test.cfg" );

        // frameGrabber and telemeter
        REQUIRE( app.m_shmimName == "nsvCtrl_test" );
        REQUIRE( app.m_circBuffLength == 4 );
        REQUIRE( app.m_maxInterval == Approx( 3.0 ) );

        // writeConfig was called and wrote the camera config file
        REQUIRE( fileExists( cfgOut ) );
        REQUIRE( readCfgValue( cfgOut, "depth" ) == "10" );
        REQUIRE( readCfgValue( cfgOut, "mode" ) == "fullframe" );
        REQUIRE( readCfgValue( cfgOut, "v crop" ) == "200" );

        ::remove( fname );
        ::remove( cfgOut.c_str() );
    }

    SECTION( "the default ROI is used when the full ROI is preset" )
    {
        nsvCtrl_test app( "nsvCtrl_test" );
        app.setupConfig();

        app.m_full_x = 1;
        app.m_full_y = 1;
        app.m_full_w = 1;
        app.m_full_h = 1;

        const char *fname = "/tmp/nsvCtrl_test_config_roi.conf";
        mx::app::writeConfigFile(
            fname,
            { "camera", "camera", "camera", "camera", "camera", "camera", "fullframe", "fullframe", "fullframe" },
            { "bitDepth",
              "startupMode",
              "default_x",
              "default_y",
              "default_w",
              "default_h",
              "configFile",
              "sizeX",
              "sizeY" },
            { "16", "fullframe", "900", "400", "200", "100", "none", "2000", "1000" } );
        app.config.readConfig( fname );

        app.loadConfig();

        REQUIRE( app.m_currentROI.x == Approx( 900 ) );
        REQUIRE( app.m_currentROI.y == Approx( 400 ) );
        REQUIRE( app.m_currentROI.w == 200 );
        REQUIRE( app.m_currentROI.h == 100 );
        REQUIRE( app.m_full_w == 2000 );
        REQUIRE( app.m_full_h == 1000 );

        REQUIRE( readCfgValue( cfgOut, "ROI.w" ) == "200" );
        REQUIRE( readCfgValue( cfgOut, "ROI.h" ) == "100" );

        ::remove( fname );
        ::remove( cfgOut.c_str() );
    }

    SECTION( "maxEMGain is clamped to at least 1" )
    {
        nsvCtrl_test app( "nsvCtrl_test" );
        app.setupConfig();

        const char *fname = "/tmp/nsvCtrl_test_config_gain.conf";
        mx::app::writeConfigFile( fname, { "camera", "camera" }, { "bitDepth", "maxEMGain" }, { "16", "0" } );
        app.config.readConfig( fname );

        app.loadConfig();

        REQUIRE( app.m_maxEMGain == Approx( 1 ) );
        REQUIRE( app.config.isSet( "camera.vcropoffset" ) == false );

        ::remove( fname );
        ::remove( cfgOut.c_str() );
    }

    SECTION( "an unwritable temporary config file shuts down the app" )
    {
        nsvCtrl_test app( "nsvCtrl_test_no_such_dir/x" );
        app.setupConfig();

        const char *fname = "/tmp/nsvCtrl_test_config_fail.conf";
        mx::app::writeConfigFile( fname, { "camera" }, { "bitDepth" }, { "16" } );
        app.config.readConfig( fname );

        app.loadConfig();

        REQUIRE( app.m_shutdown != 0 );

        ::remove( fname );
    }
}

/// Verify the contents of the temporary camera config file.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl writeConfig writes the camera settings", "[nsvCtrl][config]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::writeConfig();
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test_wc" );

    app.m_currentROI.x = 12.5;
    app.m_currentROI.y = 7;
    app.m_currentROI.w = 64;
    app.m_currentROI.h = 32;
    app.m_bitDepth     = 16;
    app.m_modeName     = "sliced";
    app.m_blacklevel   = 11;
    app.m_emGain       = 42;
    app.m_vCrop        = 300;

    const std::string fname = "/tmp/nsvCam_nsvCtrl_test_wc.cfg";

    REQUIRE( app.writeConfig() == 0 );
    REQUIRE( fileExists( fname ) );

    REQUIRE( readCfgValue( fname, "camera_class" ) == "\"nsvCam\"" );
    REQUIRE( readCfgValue( fname, "width" ) == "64" );
    REQUIRE( readCfgValue( fname, "height" ) == "32" );
    REQUIRE( readCfgValue( fname, "depth" ) == "16" );
    REQUIRE( readCfgValue( fname, "mode" ) == "sliced" );
    REQUIRE( readCfgValue( fname, "blacklevel" ) == "11" );
    REQUIRE( readCfgValue( fname, "gain" ) == "42" );
    REQUIRE( readCfgValue( fname, "v crop" ) == "300" );
    REQUIRE( readCfgValue( fname, "ROI.w" ) == "64" );
    REQUIRE( readCfgValue( fname, "ROI.h" ) == "32" );
    REQUIRE( readCfgValue( fname, "ROI.x" ) == "12.5" );
    REQUIRE( readCfgValue( fname, "ROI.y" ) == "7" );

    ::remove( fname.c_str() );
}

/// Verify parsing of v4l2-ctl output by cmdRes.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl cmdRes parses v4l2-ctl style output", "[nsvCtrl][parse]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::cmdRes( "" );
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test" );

    REQUIRE( app.cmdRes( "echo 'frame_rate: 1000'" ) == "1000\n" );
    REQUIRE( app.cmdRes( "echo 'exposure: 2500'" ) == "2500\n" );
    REQUIRE( app.cmdRes( "echo 'Cannot open device /dev/video5, exiting.'" ) == "error" );
    REQUIRE( app.cmdRes( "echo \"unknown control 'bogus'\"" ) == "error" );
    REQUIRE( app.cmdRes( "echo nospace" ) == "error" );
    REQUIRE( app.cmdRes( "true" ) == "error" );
}

/// Verify the v4l2-ctl getters parse values and report errors.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl v4l2-ctl getters", "[nsvCtrl][v4l2ctl]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::getFPS();
    nsvCtrl::getEMGain();
    nsvCtrl::getBlacklevel();
    nsvCtrl::getExpTime();
    nsvCtrl::getVCrop();
    nsvCtrl::getTemp();
    #endif
    // clang-format on

    fakeTools tools;
    REQUIRE( tools.resolvesToFake( "v4l2-ctl" ) );

    nsvCtrl_test      app( "nsvCtrl_test" );
    const std::string dev = std::string( " -d " ) + c_fakeCamPath;

    SECTION( "values are parsed" )
    {
        tools.output( "frame_rate: 250" );
        REQUIRE( app.getFPS() == 0 );
        REQUIRE( app.m_fps == Approx( 250 ) );

        tools.output( "gain: 42" );
        REQUIRE( app.getEMGain() == 0 );
        REQUIRE( app.m_emGain == Approx( 42 ) );

        tools.output( "blacklevel: 12" );
        REQUIRE( app.getBlacklevel() == 0 );
        REQUIRE( app.m_blacklevel == Approx( 12 ) );

        tools.output( "exposure: 2500" );
        REQUIRE( app.getExpTime() == 0 );
        REQUIRE( app.m_expTime == Approx( 0.0025 ) ); // microseconds to seconds

        tools.output( "vcropoffset: 100" );
        REQUIRE( app.getVCrop() == 0 );
        REQUIRE( app.m_vCrop == 100 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --get-ctrl frame_rate" + dev,
                                                              "v4l2-ctl --get-ctrl gain" + dev,
                                                              "v4l2-ctl --get-ctrl blacklevel" + dev,
                                                              "v4l2-ctl --get-ctrl exposure" + dev,
                                                              "v4l2-ctl --get-ctrl vcropoffset" + dev } ) );
    }

    SECTION( "device errors are reported and leave values unchanged" )
    {
        app.m_fps        = 1;
        app.m_emGain     = 2;
        app.m_blacklevel = 3;
        app.m_expTime    = 4;
        app.m_vCrop      = 5;

        tools.output( std::string( "Cannot open device " ) + c_fakeCamPath + ", exiting." );
        REQUIRE( app.getFPS() == -1 );
        REQUIRE( app.getEMGain() == -1 );
        REQUIRE( app.getBlacklevel() == -1 );
        REQUIRE( app.getExpTime() == -1 );
        REQUIRE( app.getVCrop() == -1 );

        tools.output( "unknown control 'frame_rate'" );
        REQUIRE( app.getFPS() == -1 );

        REQUIRE( app.m_fps == Approx( 1 ) );
        REQUIRE( app.m_emGain == Approx( 2 ) );
        REQUIRE( app.m_blacklevel == Approx( 3 ) );
        REQUIRE( app.m_expTime == Approx( 4 ) );
        REQUIRE( app.m_vCrop == 5 );
    }

    SECTION( "getTemp reports the no-sensor value" )
    {
        app.m_ccdTemp = 20;
        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
        REQUIRE( tools.calls().empty() );
    }
}

/// Verify the v4l2-ctl setters clamp their values and report errors.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl v4l2-ctl setters clamp values", "[nsvCtrl][v4l2ctl]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::setEMGain();
    nsvCtrl::setBlacklevel();
    nsvCtrl::setFPS();
    nsvCtrl::setExpTime();
    nsvCtrl::setVCrop( 0 );
    nsvCtrl::setBitDepth( 0 );
    #endif
    // clang-format on

    fakeTools tools;
    REQUIRE( tools.resolvesToFake( "v4l2-ctl" ) );

    nsvCtrl_test      app( "nsvCtrl_test" );
    const std::string dev = std::string( " -d " ) + c_fakeCamPath;

    SECTION( "EM gain" )
    {
        app.m_emGainSet = 500;
        REQUIRE( app.setEMGain() == 0 );
        app.m_emGainSet = -5;
        REQUIRE( app.setEMGain() == 0 );
        app.m_emGainSet = 100;
        REQUIRE( app.setEMGain() == 0 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl gain=360" + dev,
                                                              "v4l2-ctl --set-ctrl gain=0" + dev,
                                                              "v4l2-ctl --set-ctrl gain=100" + dev } ) );

        tools.returnCode( 1 );
        REQUIRE( app.setEMGain() == -1 );
    }

    SECTION( "black level" )
    {
        app.m_blacklevelSet = 70000;
        REQUIRE( app.setBlacklevel() == 0 );
        app.m_blacklevelSet = -3;
        REQUIRE( app.setBlacklevel() == 0 );
        app.m_blacklevelSet = 10;
        REQUIRE( app.setBlacklevel() == 0 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl blacklevel=65535" + dev,
                                                              "v4l2-ctl --set-ctrl blacklevel=0" + dev,
                                                              "v4l2-ctl --set-ctrl blacklevel=10" + dev } ) );

        tools.returnCode( 1 );
        REQUIRE( app.setBlacklevel() == -1 );
    }

    SECTION( "frame rate" )
    {
        app.m_minFPS = 10;
        app.m_maxFPS = 1000;

        app.m_fpsSet = 5;
        REQUIRE( app.setFPS() == 0 );
        app.m_fpsSet = 2000;
        REQUIRE( app.setFPS() == 0 );
        app.m_fpsSet = 500;
        REQUIRE( app.setFPS() == 0 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl frame_rate=10" + dev,
                                                              "v4l2-ctl --set-ctrl frame_rate=1000" + dev,
                                                              "v4l2-ctl --set-ctrl frame_rate=500" + dev } ) );

        tools.returnCode( 2 );
        REQUIRE( app.setFPS() == -1 );
    }

    SECTION( "exposure time is converted to microseconds" )
    {
        app.m_expTimeSet = 0.5;
        REQUIRE( app.setExpTime() == 0 );
        app.m_expTimeSet = 0.25;
        REQUIRE( app.setExpTime() == 0 );
        app.m_expTimeSet = 1e-6;
        REQUIRE( app.setExpTime() == 0 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl exposure=500000" + dev,
                                                              "v4l2-ctl --set-ctrl exposure=250000" + dev,
                                                              "v4l2-ctl --set-ctrl exposure=69" + dev } ) );

        tools.returnCode( 1 );
        REQUIRE( app.setExpTime() == -1 );
    }

    SECTION( "vertical crop offset" )
    {
        REQUIRE( app.setVCrop( 10 ) == 0 );
        REQUIRE( app.setVCrop( 5000 ) == 0 );
        REQUIRE( app.setVCrop( 100 ) == 0 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl vcropoffset=25" + dev,
                                                              "v4l2-ctl --set-ctrl vcropoffset=3699" + dev,
                                                              "v4l2-ctl --set-ctrl vcropoffset=100" + dev } ) );

        tools.returnCode( 1 );
        REQUIRE( app.setVCrop( 100 ) == -1 );
    }

    SECTION( "bit depth is always rejected" )
    {
        // The validity test in setBitDepth uses || so every value is rejected before any command is run.
        app.m_bitDepth = 16;
        REQUIRE( app.setBitDepth( 8 ) == -1 );
        REQUIRE( app.setBitDepth( 10 ) == -1 );
        REQUIRE( app.setBitDepth( 16 ) == -1 );
        REQUIRE( app.setBitDepth( 12 ) == -1 );
        REQUIRE( app.m_bitDepth == 16 );
        REQUIRE( tools.calls().empty() );
    }
}

/// Verify setReadoutMode selects the sensor mode and resets the ROI.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl setReadoutMode", "[nsvCtrl][v4l2ctl]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::setReadoutMode();
    #endif
    // clang-format on

    fakeTools tools;
    REQUIRE( tools.resolvesToFake( "v4l2-ctl" ) );

    nsvCtrl_test      app( "nsvCtrl_test" );
    const std::string dev = std::string( " -d " ) + c_fakeCamPath;

    dev::cameraConfig full;
    full.m_centerX = 1000;
    full.m_centerY = 500;
    full.m_sizeX   = 2000;
    full.m_sizeY   = 1000;
    full.m_maxFPS  = 60;

    dev::cameraConfig sliced;
    sliced.m_centerX = 1000;
    sliced.m_centerY = 50;
    sliced.m_sizeX   = 2000;
    sliced.m_sizeY   = 100;
    sliced.m_maxFPS  = 900;

    app.m_cameraModes["fullframe"] = full;
    app.m_cameraModes["sliced"]    = sliced;

    SECTION( "fullframe with a valid default ROI" )
    {
        app.m_modeName  = "fullframe";
        app.m_default_x = 1000;
        app.m_default_y = 500;
        app.m_default_w = 200;
        app.m_default_h = 100;

        REQUIRE( app.setReadoutMode() == 0 );
        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl sensor_mode=0" + dev } ) );

        REQUIRE( app.m_full_x == Approx( 1000 ) );
        REQUIRE( app.m_full_y == Approx( 500 ) );
        REQUIRE( app.m_full_w == 2000 );
        REQUIRE( app.m_full_h == 1000 );
        REQUIRE( app.m_maxFPS == Approx( 60 ) );
        REQUIRE( app.m_minFPS == Approx( 60 ) );

        REQUIRE( app.m_currentROI.x == Approx( 1000 ) );
        REQUIRE( app.m_currentROI.y == Approx( 500 ) );
        REQUIRE( app.m_currentROI.w == 200 );
        REQUIRE( app.m_currentROI.h == 100 );
        REQUIRE( app.m_currentROI.bin_x == 1 );
        REQUIRE( app.m_currentROI.bin_y == 1 );
        REQUIRE( app.m_nextROI.x == Approx( 1000 ) );
        REQUIRE( app.m_nextROI.w == 200 );
    }

    SECTION( "sliced with an oversize default ROI is clamped to the mode" )
    {
        app.m_modeName  = "sliced";
        app.m_default_x = 1000;
        app.m_default_y = 500;
        app.m_default_w = 200;
        app.m_default_h = 400;

        REQUIRE( app.setReadoutMode() == 0 );
        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl sensor_mode=1" + dev } ) );

        REQUIRE( app.m_full_h == 100 );
        REQUIRE( app.m_maxFPS == Approx( 900 ) );

        REQUIRE( app.m_currentROI.x == Approx( 1000 ) );
        REQUIRE( app.m_currentROI.y == Approx( 50 ) ); // recentered on the mode
        REQUIRE( app.m_currentROI.w == 200 );
        REQUIRE( app.m_currentROI.h == 100 ); // limited to the mode height
        REQUIRE( app.m_nextROI.y == Approx( 50 ) );
        REQUIRE( app.m_nextROI.h == 100 );
    }

    SECTION( "an off-frame default x is recentered" )
    {
        app.m_modeName  = "fullframe";
        app.m_default_x = 50;
        app.m_default_y = 500;
        app.m_default_w = 200;
        app.m_default_h = 100;

        REQUIRE( app.setReadoutMode() == 0 );
        REQUIRE( app.m_currentROI.x == Approx( 1000 ) );
        REQUIRE( app.m_nextROI.x == Approx( 1000 ) );
    }

    SECTION( "a v4l2-ctl failure is reported" )
    {
        app.m_modeName = "sliced";
        tools.returnCode( 1 );
        REQUIRE( app.setReadoutMode() == -1 );
    }
}

/// Verify the camera power I2C commands.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl I2C power commands", "[nsvCtrl][power]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    send_i2c_cmd( 0, 0 );
    turn_on_power();
    turn_off_power();
    #endif
    // clang-format on

    fakeTools tools;

    // Never run the real tool, which would write to the power controller.
    REQUIRE( tools.resolvesToFake( "i2ctransfer" ) );

    SECTION( "single channel commands" )
    {
        send_i2c_cmd( 0, 1 );
        send_i2c_cmd( 1, 0 );
        send_i2c_cmd( 4, 1 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "i2ctransfer -f -y 8 w3@0x48 2 191 144",
                                                              "i2ctransfer -f -y 8 w3@0x48 2 176 128",
                                                              "i2ctransfer -f -y 1 w3@0x48 2 191 144" } ) );
    }

    SECTION( "power on addresses all six channels" )
    {
        turn_on_power();

        REQUIRE( tools.calls() == std::vector<std::string>( { "i2ctransfer -f -y 8 w3@0x48 2 191 144",
                                                              "i2ctransfer -f -y 8 w3@0x48 2 176 144",
                                                              "i2ctransfer -f -y 2 w3@0x48 2 191 144",
                                                              "i2ctransfer -f -y 2 w3@0x48 2 176 144",
                                                              "i2ctransfer -f -y 1 w3@0x48 2 191 144",
                                                              "i2ctransfer -f -y 1 w3@0x48 2 176 144" } ) );
    }

    SECTION( "power off addresses all six channels" )
    {
        turn_off_power();

        REQUIRE( tools.calls() == std::vector<std::string>( { "i2ctransfer -f -y 8 w3@0x48 2 191 128",
                                                              "i2ctransfer -f -y 8 w3@0x48 2 176 128",
                                                              "i2ctransfer -f -y 2 w3@0x48 2 191 128",
                                                              "i2ctransfer -f -y 2 w3@0x48 2 176 128",
                                                              "i2ctransfer -f -y 1 w3@0x48 2 191 128",
                                                              "i2ctransfer -f -y 1 w3@0x48 2 176 128" } ) );
    }
}

/// Verify the v4l2lib wrappers fail cleanly without a camera.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "v4l2lib wrappers fail cleanly without a device", "[nsvCtrl][v4l2lib]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    openCamera( "" );
    closeCamera();
    initCamera( 0, 0, 0 );
    requestBuffers( 0 );
    queryBuffers();
    queueBuffers();
    queueBuffer( 0 );
    dequeueBuffer( 0 );
    startStreaming();
    stopStreaming();
    getCameraParams();
    #endif
    // clang-format on

    resetV4l2Globals();

    SECTION( "opening a missing device fails" )
    {
        REQUIRE( openCamera( c_fakeCamPath ) == -1 );
        REQUIRE( ::fd < 0 );
    }

    SECTION( "unsupported bit depths are rejected" )
    {
        REQUIRE( initCamera( 640, 480, 12 ) == -1 );
        REQUIRE( initCamera( 640, 480, 0 ) == -1 );
    }

    SECTION( "format and buffer ioctls fail on a closed descriptor" )
    {
        ::params.width = 7;

        REQUIRE( initCamera( 640, 480, 8 ) == -1 );
        REQUIRE( initCamera( 640, 480, 10 ) == -1 );
        REQUIRE( initCamera( 640, 480, 16 ) == -1 );
        REQUIRE( ::params.width == 7 ); // unchanged on failure

        REQUIRE( requestBuffers( 4 ) == -1 );
        REQUIRE( ::bufferCount == 0 );

        REQUIRE( queryBuffers() == -1 ); // no buffers
        REQUIRE( queueBuffers() == 0 );  // nothing to queue
        REQUIRE( queueBuffer( 0 ) == -1 );
        REQUIRE( dequeueBuffer( 0 ) == -1 );
    }

    SECTION( "streaming ioctls fail and leave the stream flag unchanged" )
    {
        ::stream_on = false;
        REQUIRE( startStreaming() == -1 );
        REQUIRE( ::stream_on == false );

        ::stream_on = true;
        REQUIRE( stopStreaming() == -1 );
        REQUIRE( ::stream_on == true );
    }

    SECTION( "getCameraParams returns the stored parameters" )
    {
        ::params.width       = 1920;
        ::params.height      = 1080;
        ::params.bitDepth    = 10;
        ::params.pixelFormat = "RG10";

        CameraParams p = getCameraParams();
        REQUIRE( p.width == 1920 );
        REQUIRE( p.height == 1080 );
        REQUIRE( p.bitDepth == 10 );
        REQUIRE( p.pixelFormat == "RG10" );
    }

    REQUIRE( closeCamera() == 0 );

    resetV4l2Globals();
}

/// Verify the device-facing app functions fail cleanly without a camera.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl device errors without a camera", "[nsvCtrl][fsm]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::cameraSelect();
    nsvCtrl::startAcquisition();
    nsvCtrl::acquireAndCheckValid();
    nsvCtrl::reconfig();
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test" );

    SECTION( "cameraSelect with a missing device goes to NODEVICE" )
    {
        app.m_powerState = 1;
        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_init == false );
    }

    SECTION( "startAcquisition failure goes to ERROR" )
    {
        REQUIRE( app.startAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "acquireAndCheckValid does nothing before init" )
    {
        app.m_init = false;
        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( app.m_oldest_frame == 0 );
    }

    SECTION( "acquireAndCheckValid dequeue failure goes to ERROR" )
    {
        app.m_init = true;
        REQUIRE( app.acquireAndCheckValid() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_oldest_frame == 0 );
    }

    SECTION( "reconfig without a mode change goes to CONNECTED" )
    {
        app.m_modeName = "fullframe";
        app.m_nextMode = "fullframe";
        REQUIRE( app.reconfig() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }

    SECTION( "reconfig with a mode change fails to stop the stream" )
    {
        app.m_modeName = "fullframe";
        app.m_nextMode = "sliced";
        app.m_init     = true;

        REQUIRE( app.reconfig() == -1 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_init == false );
        REQUIRE( app.m_modeName == "fullframe" );
    }
}

/// Verify the stdCamera interface hooks that need no hardware.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl stdCamera hooks", "[nsvCtrl][stdCamera]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::powerOnDefaults();
    nsvCtrl::checkNextROI();
    nsvCtrl::setNextROI();
    nsvCtrl::setTempControl();
    nsvCtrl::setCropMode();
    nsvCtrl::whilePowerOff();
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test" );

    app.m_full_x = 1000;
    app.m_full_y = 500;
    app.m_full_w = 2000;
    app.m_full_h = 1000;

    SECTION( "powerOnDefaults applies a valid default ROI" )
    {
        app.m_default_x         = 900;
        app.m_default_y         = 400;
        app.m_default_w         = 200;
        app.m_default_h         = 100;
        app.m_default_bin_x     = 2;
        app.m_default_bin_y     = 2;
        app.m_reconfig          = false;
        app.m_tempControlStatus = true;

        REQUIRE( app.powerOnDefaults() == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 900 ) );
        REQUIRE( app.m_currentROI.y == Approx( 400 ) );
        REQUIRE( app.m_currentROI.w == 200 );
        REQUIRE( app.m_currentROI.h == 100 );
        REQUIRE( app.m_currentROI.bin_x == 2 );
        REQUIRE( app.m_currentROI.bin_y == 2 );
        REQUIRE( app.m_nextROI.x == Approx( 900 ) );
        REQUIRE( app.m_nextROI.bin_y == 2 );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_tempControlStatus == false );
        REQUIRE( app.m_tempControlStatusStr == "OFF" );
    }

    SECTION( "powerOnDefaults clamps an invalid default ROI" )
    {
        app.m_default_x = 150;
        app.m_default_y = 20;
        app.m_default_w = 3000;
        app.m_default_h = 5000;

        REQUIRE( app.powerOnDefaults() == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 1000 ) );
        REQUIRE( app.m_currentROI.y == Approx( 500 ) );
        REQUIRE( app.m_currentROI.w == 2000 );
        REQUIRE( app.m_currentROI.h == 1000 );
        REQUIRE( app.m_nextROI.x == Approx( 1000 ) );
        REQUIRE( app.m_nextROI.y == Approx( 500 ) );
        REQUIRE( app.m_nextROI.w == 2000 );
        REQUIRE( app.m_nextROI.h == 1000 );
    }

    SECTION( "setNextROI requests a reconfig only when powered on" )
    {
        app.m_reconfig  = false;
        app.m_poweredOn = false;
        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( app.m_reconfig == false );

        app.m_poweredOn = true;
        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "no-op hooks" )
    {
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( app.setCropMode() == 0 );
    }

    SECTION( "whilePowerOff reports the shutter as powered off" )
    {
        app.m_shutterStatus = "READY";
        app.m_shutterState  = 1;

        REQUIRE( app.whilePowerOff() == 0 );
        REQUIRE( app.m_shutterStatus == "POWEROFF" );
        REQUIRE( app.m_shutterState == 0 );
    }
}

/// Verify configureAcquisition snaps the ROI and sets the frame geometry.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl configureAcquisition", "[nsvCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::configureAcquisition();
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test" );

    SECTION( "odd sizes keep an integer center" )
    {
        app.m_nextROI.x     = 100;
        app.m_nextROI.y     = 50;
        app.m_nextROI.w     = 11;
        app.m_nextROI.h     = 21;
        app.m_nextROI.bin_x = 1;
        app.m_nextROI.bin_y = 1;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 100 ) );
        REQUIRE( app.m_currentROI.y == Approx( 50 ) );
        REQUIRE( app.m_currentROI.w == 11 );
        REQUIRE( app.m_currentROI.h == 21 );
        REQUIRE( app.m_width == 11 );
        REQUIRE( app.m_height == 21 );
    }

    SECTION( "even sizes snap to a half-pixel center and binning divides the frame" )
    {
        app.m_nextROI.x     = 100.7;
        app.m_nextROI.y     = 50.2;
        app.m_nextROI.w     = 10;
        app.m_nextROI.h     = 4;
        app.m_nextROI.bin_x = 2;
        app.m_nextROI.bin_y = 2;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 100.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 49.5 ) );
        REQUIRE( app.m_currentROI.bin_x == 2 );
        REQUIRE( app.m_currentROI.bin_y == 2 );
        REQUIRE( app.m_width == 5 );
        REQUIRE( app.m_height == 2 );

        // the next ROI is updated to the realized ROI
        REQUIRE( app.m_nextROI.x == Approx( 100.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 49.5 ) );
        REQUIRE( app.m_nextROI.w == 10 );
        REQUIRE( app.m_nextROI.h == 4 );
    }

    REQUIRE( app.m_dataType == IMAGESTRUCT_UINT16 );
    REQUIRE( app.m_typeSize == 2 );
}

/// Verify the ROI subframe extraction and the copy into the stream.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl ROI subframe extraction", "[nsvCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::writeROISubframe();
    nsvCtrl::loadImageIntoStream( nullptr );
    nsvCtrl::resizeROIbufs();
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test" );

    const int fullW = 8;
    const int fullH = 6;

    app.m_full_w = fullW;
    app.m_full_h = fullH;

    // Full frame pixel value is its index
    std::vector<uint16_t> image( fullW * fullH );
    for( size_t n = 0; n < image.size(); ++n )
    {
        image[n] = static_cast<uint16_t>( n );
    }

    std::vector<uint16_t> roi( fullW * fullH, 0xFFFF );

    ::buffers.assign( 1, static_cast<void *>( image.data() ) );
    app.ROIbuffers.assign( 1, static_cast<void *>( roi.data() ) );
    app.m_current_frame = 0;

    app.m_currentROI.w = 4;
    app.m_currentROI.h = 2;

    /// Expected ROI value at row i, column j for a given start pixel.
    auto expected = [&]( int startX, int startY, int i, int j )
    { return static_cast<uint16_t>( ( startY + i ) * fullW + ( startX + j ) ); };

    SECTION( "an interior ROI" )
    {
        app.m_currentROI.x = 4;
        app.m_currentROI.y = 3;

        REQUIRE( app.writeROISubframe() == 0 );

        for( int i = 0; i < 2; ++i )
        {
            for( int j = 0; j < 4; ++j )
            {
                REQUIRE( roi[i * 4 + j] == expected( 2, 2, i, j ) );
            }
        }
        REQUIRE( roi[8] == 0xFFFF ); // nothing past w*h

        std::vector<uint16_t> dest( 8, 0 );
        app.m_typeSize = sizeof( uint16_t );
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        for( int n = 0; n < 8; ++n )
        {
            REQUIRE( dest[n] == roi[n] );
        }
    }

    SECTION( "a ROI off the low edges is moved inside the frame" )
    {
        app.m_currentROI.x = 1;
        app.m_currentROI.y = 0;

        REQUIRE( app.writeROISubframe() == 0 );
        REQUIRE( app.m_currentROI.x == Approx( 2 ) );
        REQUIRE( app.m_currentROI.y == Approx( 1 ) );
        REQUIRE( roi[0] == expected( 0, 0, 0, 0 ) );
        REQUIRE( roi[7] == expected( 0, 0, 1, 3 ) );
    }

    SECTION( "a ROI off the high edges is moved inside the frame" )
    {
        app.m_currentROI.x = 7;
        app.m_currentROI.y = 5;

        REQUIRE( app.writeROISubframe() == 0 );
        REQUIRE( app.m_currentROI.x == Approx( 5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 4 ) );
        REQUIRE( roi[0] == expected( 3, 3, 0, 0 ) );
        REQUIRE( roi[7] == expected( 3, 3, 1, 3 ) );
    }

    ::buffers.clear();
    app.ROIbuffers.clear();

    SECTION( "resizeROIbufs allocates one buffer per circular buffer entry" )
    {
        app.m_circBuffLength = 3;

        REQUIRE( app.resizeROIbufs() == 0 );
        REQUIRE( app.ROIbuffers.size() == 3 );
        for( void *p : app.ROIbuffers )
        {
            REQUIRE( p != nullptr );
            free( p ); // allocated with malloc
        }
        app.ROIbuffers.clear();
    }
}

/// Verify the vcropoffset new-property callback.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl vcropoffset callback", "[nsvCtrl][indi]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::newCallBack_m_indiP_vCrop( pcf::IndiProperty() );
    #endif
    // clang-format on

    fakeTools tools;
    REQUIRE( tools.resolvesToFake( "v4l2-ctl" ) );

    nsvCtrl_test      app( "nsvCtrl_test" );
    const std::string dev = std::string( " -d " ) + c_fakeCamPath;

    SECTION( "wrong property name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_vCrop( makeNumberProp( "nsvCtrl_test", "wrong", true, 0, true, 100 ) ) == -1 );
        REQUIRE( app.m_vCrop == 0 );
        REQUIRE( tools.calls().empty() );
    }

    SECTION( "without camera.vcropoffset configured the request is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_vCrop( makeNumberProp( "nsvCtrl_test", "vcropoffset", true, 0, true, 100 ) ) ==
                 -1 );
        REQUIRE( app.m_vCrop == 100 ); // note: the member is updated even though the request is rejected
        REQUIRE( tools.calls().empty() );
    }

    SECTION( "with camera.vcropoffset configured the offset is set and read back" )
    {
        app.setupConfig();
        const char *fname = "/tmp/nsvCtrl_test_vcrop.conf";
        mx::app::writeConfigFile( fname, { "camera" }, { "vcropoffset" }, { "50" } );
        app.config.readConfig( fname );
        REQUIRE( app.config.isSet( "camera.vcropoffset" ) );

        tools.output( "vcropoffset: 120" );

        REQUIRE( app.newCallBack_m_indiP_vCrop( makeNumberProp( "nsvCtrl_test", "vcropoffset", true, 0, true, 10 ) ) ==
                 0 );
        REQUIRE( app.m_vCrop == 120 );

        tools.clearLog();
        REQUIRE( app.newCallBack_m_indiP_vCrop(
                     makeNumberProp( "nsvCtrl_test", "vcropoffset", true, 200, false, 0 ) ) == 0 );

        REQUIRE( tools.calls() == std::vector<std::string>( { "v4l2-ctl --set-ctrl vcropoffset=200" + dev,
                                                              "v4l2-ctl --get-ctrl vcropoffset" + dev } ) );

        tools.returnCode( 1 );
        REQUIRE( app.newCallBack_m_indiP_vCrop( makeNumberProp( "nsvCtrl_test", "vcropoffset", true, 0, true, 300 ) ) ==
                 -1 );

        ::remove( fname );
    }
}

/// Verify the bitDepth new-property callback.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl bitDepth callback", "[nsvCtrl][indi]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::newCallBack_m_indiP_bitDepth( pcf::IndiProperty() );
    #endif
    // clang-format on

    fakeTools tools;
    REQUIRE( tools.resolvesToFake( "v4l2-ctl" ) );

    nsvCtrl_test app( "nsvCtrl_test" );

    SECTION( "wrong property name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_bitDepth( makeNumberProp( "nsvCtrl_test", "wrong", true, 0, true, 10 ) ) ==
                 -1 );
        REQUIRE( app.m_bitDepth == 16 );
    }

    SECTION( "valid requests are rejected by setBitDepth" )
    {
        REQUIRE( app.newCallBack_m_indiP_bitDepth( makeNumberProp( "nsvCtrl_test", "bitDepth", true, 0, true, 10 ) ) ==
                 -1 );
        REQUIRE( app.m_bitDepth == 10 ); // note: the member is updated even though the request is rejected
        REQUIRE( tools.calls().empty() );
    }
}

/// Verify the power new-property callback validates the property.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl power callback validation", "[nsvCtrl][indi]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::newCallBack_m_indiP_power( pcf::IndiProperty() );
    #endif
    // clang-format on

    // With no toggle element the live callback body returns 0 after validation, so the standard macro applies.
    XWCTEST_INDI_NEW_CALLBACK( nsvCtrl, power );
}

/// Verify the telemetry hooks record stdcam telemetry when due.
/**
 * \ingroup nsvCtrl_unit_test
 */
TEST_CASE( "nsvCtrl telemetry records stdcam", "[nsvCtrl][telem]" )
{
    // clang-format off
    #ifdef NSVCTRL_TEST_DOXYGEN_REF
    nsvCtrl::checkRecordTimes();
    nsvCtrl::recordTelem( nullptr );
    #endif
    // clang-format on

    nsvCtrl_test app( "nsvCtrl_test" );

    SECTION( "recordTelem always records" )
    {
        MagAOX::logger::telem_stdcam::lastRecord = timespec{ 0, 0 };

        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_stdcam *>( nullptr ) ) == 0 );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records a stale entry" )
    {
        MagAOX::logger::telem_stdcam::lastRecord = timespec{ 0, 0 };

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes skips a recent entry" )
    {
        timespec now;
        clock_gettime( CLOCK_REALTIME, &now );
        MagAOX::logger::telem_stdcam::lastRecord = now;

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_sec == now.tv_sec );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_nsec == now.tv_nsec );
    }
}

} // namespace nsvCtrlTest

} // namespace libXWCTest
