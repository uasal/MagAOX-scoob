/** \file darkCtrl.hpp
  * \brief MagAO-X app that builds a camera dark library (exptime sweep + shutter).
  *
 * Writes dark_lib_path/dark_NNN.fits and dark_lib_path/dark_metadata.txt
 * (CSV of camera parameters) for psfRefCtrl / iefcCtrl.
  *
  * \ingroup darkCtrl_files
  */

#ifndef darkCtrl_hpp
#define darkCtrl_hpp

#include <atomic>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include <ImageStreamIO/ImageStreamIO.h>

#include <mx/ioutils/fits/fitsFile.hpp>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

#include <lina/dark_library.h>

/** \defgroup darkCtrl MagAO-X dark library builder
  * \ingroup app_files
  */

namespace MagAOX
{
namespace app
{

class darkCtrl : public MagAOXApp<true>
{
  public:
    enum class Job : int
    {
        Idle = 0,
        Build,
        Stop
    };

    ~darkCtrl() noexcept
    {
    }

  protected:
    std::string m_shmCamInput{ "camsci_sim" }; ///< Frame input (e.g. llowfscSim output)
    std::string m_camName{ "nsv455sim" }; ///< INDI device to query/set camera params
    std::string m_darkLibPath{ "./darks_lib" };
    std::string m_shutterDevice{ "llowfscsim" }; ///< INDI device that owns shutter toggle

    unsigned m_darkNImages{ 20 };
    std::string m_darkExptimesCsv{ "0.5,1,2,5" };
    float m_settle_s{ 0.5f };
    unsigned m_camNFrameDelay{ 1 };

    // Camera record mirrored from cam_name INDI (RO cam_* props). Used as library metadata.
    double m_camExptime{ std::numeric_limits<double>::quiet_NaN() };
    double m_camEmgain{ std::numeric_limits<double>::quiet_NaN() };
    double m_camFps{ std::numeric_limits<double>::quiet_NaN() };
    double m_camBlacklevel{ std::numeric_limits<double>::quiet_NaN() };
    unsigned m_camBitdepth{ 0 };
    int m_camRoiX{ 0 };
    int m_camRoiY{ 0 };
    unsigned m_camRoiWidth{ 0 };  ///< 0 → stamp grabbed frame width
    unsigned m_camRoiHeight{ 0 }; ///< 0 → stamp grabbed frame height
    double m_remoteMaxFps{ std::numeric_limits<double>::quiet_NaN() };
    bool m_haveCamExptime{ false };
    bool m_haveCamEmgain{ false };
    bool m_haveCamFps{ false };
    bool m_haveCamBlacklevel{ false };
    bool m_haveCamBitdepth{ false };
    bool m_haveCamRoiX{ false };
    bool m_haveCamRoiY{ false };
    bool m_haveCamRoiW{ false };
    bool m_haveCamRoiH{ false };
    bool m_haveRemoteFastCam{ false }; ///< True once cam_name.fast_cam SET has been seen.
    bool m_remoteFastCamOn{ false };   ///< cam_name.fast_cam.toggle == On
    bool m_remoteShutterClosed{ false };
    bool m_haveRemoteShutter{ false };

    /// Wait for cam_name.exptime.current to match commanded value (<1 s).
    unsigned m_expConfirmTimeout_ms{ 1000 };
    double m_expConfirmTolAbs{ 1e-6 };   ///< Absolute [s] tolerance
    double m_expConfirmTolRel{ 1e-3 };   ///< Relative tolerance (fraction of target)

    std::thread m_worker;
    std::atomic<bool> m_workerShutdown{ false };
    std::atomic<bool> m_stopRequested{ false };
    sem_t m_jobSem;
    std::mutex m_jobMutex;
    Job m_pendingJob{ Job::Idle };
    std::atomic<int> m_busy{ 0 };
    std::string m_status{ "idle" };

    IMAGE m_camsci{};
    bool m_camsciOpen{ false };

    pcf::IndiProperty m_indiP_shmCamInput;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_shmCamInput );
    pcf::IndiProperty m_indiP_camName;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_camName );
    pcf::IndiProperty m_indiP_darkLibPath;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_darkLibPath );
    pcf::IndiProperty m_indiP_shutterDevice;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_shutterDevice );
    pcf::IndiProperty m_indiP_shutter;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_shutter );

    /// Remote SET subscriptions (camera + shutter devices).
    pcf::IndiProperty m_indiP_remoteExptime;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteExptime );
    pcf::IndiProperty m_indiP_remoteEmgain;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteEmgain );
    pcf::IndiProperty m_indiP_remoteFps;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteFps );
    pcf::IndiProperty m_indiP_remoteBlacklevel;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteBlacklevel );
    pcf::IndiProperty m_indiP_remoteRoiX;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteRoiX );
    pcf::IndiProperty m_indiP_remoteRoiY;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteRoiY );
    pcf::IndiProperty m_indiP_remoteRoiW;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteRoiW );
    pcf::IndiProperty m_indiP_remoteRoiH;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteRoiH );
    pcf::IndiProperty m_indiP_remoteBitDepth;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteBitDepth );
    pcf::IndiProperty m_indiP_remoteFastCam;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteFastCam );
    pcf::IndiProperty m_indiP_remoteShutter;
    INDI_SETCALLBACK_DECL( darkCtrl, m_indiP_remoteShutter );

    pcf::IndiProperty m_indiP_darkNImages;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_darkNImages );
    pcf::IndiProperty m_indiP_darkExptimes;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_darkExptimes );
    pcf::IndiProperty m_indiP_camNFrameDelay;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_camNFrameDelay );

    /// RO mirrors of cam_name (current only; no local target).
    pcf::IndiProperty m_indiP_camExptime;
    pcf::IndiProperty m_indiP_camEmgain;
    pcf::IndiProperty m_indiP_camFps;
    pcf::IndiProperty m_indiP_camBlacklevel;
    pcf::IndiProperty m_indiP_camBitdepth;
    pcf::IndiProperty m_indiP_camRoiX;
    pcf::IndiProperty m_indiP_camRoiY;
    pcf::IndiProperty m_indiP_camRoiWidth;
    pcf::IndiProperty m_indiP_camRoiHeight;

    pcf::IndiProperty m_indiP_darkLibBuild;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_darkLibBuild );
    pcf::IndiProperty m_indiP_stop;
    INDI_NEWCALLBACK_DECL( darkCtrl, m_indiP_stop );
    pcf::IndiProperty m_indiP_status;

  public:
    darkCtrl();

    virtual void setupConfig();
    virtual void loadConfig();
    virtual int appStartup();
    virtual int appLogic();
    virtual int appShutdown();

  protected:
    static void workerStart( darkCtrl *s );
    void workerExec();
    void queueJob( Job j );
    int runJob( Job j );

    void setStatus( const std::string &s );
    void clearRequest( pcf::IndiProperty &p );

    int openCamsci();
    void closeStreams();

    int setShutterClosed( bool closed );
    void publishShutterIndi( bool closed );
    int setCamExpValue( double seconds );
    int setCamFpsValue( double fps );

    int registerCamRoNumber( pcf::IndiProperty &p, const char *name, const char *label,
                             double minv, double maxv, double step, const char *fmt );
    void publishCamMirrors();

    /// Max fps that can still accommodate exptime (≈ 1/exptime), clamped to camera max if known.
    double maxFpsForExp( double seconds ) const;
    /// Poll a mirrored camera number until it matches expected.
    /// If required is false, timeout logs a warning and returns 0 (do not fail the job).
    int waitCamValue( const double &current, double expected, unsigned timeout_ms,
                      const char *what, bool required = true );
    /// Poll cam_name.exptime.current until within tolerance or timeout.
    int waitCamExpCurrent( double expected_s, unsigned timeout_ms );
    /// Poll cam_name.fps.current (warning-only timeout — cameras often clamp fps).
    int waitCamFpsCurrent( double expected_fps, unsigned timeout_ms );
    /// Set camera exposure for one dark. fast_cam On → exptime only.
    /// fast_cam Off or absent → set exptime, confirm, then max fps for that exposure.
    int applyCamExpForDark( double seconds );

    /// Open shutter and put cam_name.exptime (and fps, if we changed it) back.
    void restoreAfterDarkLib( double exptime0, double fps0, bool restore_fps );

    /// Re-request cam_name SET properties and wait until stamp fields have real currents.
    /// Returns -1 if the camera never reports emgain/blacklevel/bitdepth/ROI.
    int waitCamStampMeta( unsigned timeout_ms );

    /// Store cam_name.*.current from a SET/Def. Does not send INDI (appLogic publishes).
    bool storeRemoteCurrent( const pcf::IndiProperty &want, const pcf::IndiProperty &ipRecv,
                             double &dest, bool &have, const char *what );

    int grabMean( unsigned nframes, unsigned wait_frames, std::vector<float> &out, uint32_t &w,
                  uint32_t &h, unsigned *n_collected = nullptr );
    int ensureDir( const std::string &dir );
    int saveFitsF32( const std::string &path, const std::vector<float> &im, uint32_t w,
                     uint32_t h );

    int doDarkLibBuild();
};

namespace
{

/// Parse cam_name.*.current from SET/Def. Empty current is not a number (INDI get<double>() UB).
bool parseIndiCurrentNumber( const pcf::IndiProperty &ip, double &out )
{
    try
    {
        if( !ip.find( "current" ) )
            return false;
        const std::string s = ip["current"].getValue();
        if( s.empty() )
            return false;
        char *end = nullptr;
        const double v = std::strtod( s.c_str(), &end );
        if( end == s.c_str() )
            return false;
        while( *end != '\0' && std::isspace( static_cast<unsigned char>( *end ) ) )
            ++end;
        if( *end != '\0' )
            return false;
        if( !std::isfinite( v ) )
            return false;
        out = v;
        return true;
    }
    catch( ... )
    {
        return false;
    }
}

std::string trimCopy( const std::string &s )
{
    size_t a = 0;
    size_t b = s.size();
    while( a < b && std::isspace( static_cast<unsigned char>( s[a] ) ) )
        ++a;
    while( b > a && std::isspace( static_cast<unsigned char>( s[b - 1] ) ) )
        --b;
    return s.substr( a, b - a );
}

} // namespace

darkCtrl::darkCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
}

void darkCtrl::setupConfig()
{
    config.add( "dark.shm_cam_input", "", "dark.shm_cam_input", argType::Required, "dark",
                "shm_cam_input", false, "string",
                "Science-camera ImageStreamIO name (e.g. camsci_sim from llowfscSim)." );
    config.add( "dark.cam_name", "", "dark.cam_name", argType::Required, "dark", "cam_name", false,
                "string",
                "INDI camera device to query/set (exptime/emgain/fps/blacklevel/bitDepth/ROI)." );
    config.add( "dark.dark_lib_path", "", "dark.dark_lib_path", argType::Required, "dark",
                "dark_lib_path", false, "string", "Directory for dark_NNN.fits + dark_metadata.txt." );
    config.add( "dark.shutter_device", "", "dark.shutter_device", argType::Required, "dark",
                "shutter_device", false, "string",
                "INDI device that owns shutter toggle (e.g. llowfscsim)." );
    config.add( "dark.dark_n_images", "", "dark.dark_n_images", argType::Required, "dark",
                "dark_n_images", false, "unsigned", "Frames averaged per dark." );
    config.add( "dark.dark_exptimes", "", "dark.dark_exptimes", argType::Required, "dark",
                "dark_exptimes", false, "string", "CSV exposure times for the library." );
    config.add( "dark.cam_n_frame_delay", "", "dark.cam_n_frame_delay", argType::Required, "dark",
                "cam_n_frame_delay", false, "unsigned", "Skip N frames after changing exptime." );
}

void darkCtrl::loadConfig()
{
    config( m_shmCamInput, "dark.shm_cam_input" );
    config( m_camName, "dark.cam_name" );
    config( m_darkLibPath, "dark.dark_lib_path" );
    config( m_shutterDevice, "dark.shutter_device" );
    config( m_darkNImages, "dark.dark_n_images" );
    config( m_darkExptimesCsv, "dark.dark_exptimes" );
    config( m_camNFrameDelay, "dark.cam_n_frame_delay" );
}

int darkCtrl::appStartup()
{
    if( sem_init( &m_jobSem, 0, 0 ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "sem_init failed" } );

    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmCamInput, "shm_cam_input", "Camera input shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_camName, "cam_name",
                              "INDI camera device (query/set via cam_name.*)", "camera" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_darkLibPath, "dark_lib_path", "Dark library output directory",
                              "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shutterDevice, "shutter_device", "INDI shutter device",
                              "camera" );
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_shutter, "shutter" );

    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_darkNImages, "dark_n_images", 1, 10000, 1, "%u",
                                 "Frames averaged per dark", "library" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_darkExptimes, "dark_exptimes", "CSV exposures for library",
                              "library" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_camNFrameDelay, "cam_n_frame_delay", 0, 1000, 1, "%u",
                                 "Skip N frames after exptime change", "library" );

    if( registerCamRoNumber( m_indiP_camExptime, "cam_exptime",
                             "cam_name.exptime.current [s]", 0, 1e6, 1e-6, "%0.6f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camEmgain, "cam_emgain",
                             "cam_name.emgain.current", 0, 1000, 0.1, "%0.3f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camFps, "cam_fps",
                             "cam_name.fps.current", 0, 1e6, 0.01, "%0.3f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camBlacklevel, "cam_blacklevel",
                             "cam_name.blacklevel.current", -1e6, 1e6, 1, "%0.3f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camBitdepth, "cam_bitdepth",
                             "cam_name.bitDepth.current", 0, 32, 1, "%0.0f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camRoiX, "cam_roi_x",
                             "cam_name.roi_region_x.current", 0, 1e6, 1, "%0.0f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camRoiY, "cam_roi_y",
                             "cam_name.roi_region_y.current", 0, 1e6, 1, "%0.0f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camRoiWidth, "cam_roi_width",
                             "cam_name.roi_region_w.current", 0, 1e6, 1, "%0.0f" ) < 0 )
        return -1;
    if( registerCamRoNumber( m_indiP_camRoiHeight, "cam_roi_height",
                             "cam_name.roi_region_h.current", 0, 1e6, 1, "%0.0f" ) < 0 )
        return -1;

    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_darkLibBuild, "dark_lib_build" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_stop, "stop" );

    REG_INDI_NEWPROP_NOCB( m_indiP_status, "status", pcf::IndiProperty::Text );
    m_indiP_status.add( pcf::IndiElement( "current" ) );
    m_indiP_status["current"].set( m_status );

    m_indiP_shmCamInput["current"].setValue( m_shmCamInput );
    m_indiP_shmCamInput["target"].setValue( m_shmCamInput );
    m_indiP_camName["current"].setValue( m_camName );
    m_indiP_camName["target"].setValue( m_camName );
    m_indiP_darkLibPath["current"].setValue( m_darkLibPath );
    m_indiP_darkLibPath["target"].setValue( m_darkLibPath );
    m_indiP_shutterDevice["current"].setValue( m_shutterDevice );
    m_indiP_shutterDevice["target"].setValue( m_shutterDevice );
    m_indiP_shutter["toggle"].setSwitchState( pcf::IndiElement::Off );
    m_indiP_darkNImages["current"].setValue( m_darkNImages );
    m_indiP_darkNImages["target"].setValue( m_darkNImages );
    m_indiP_darkExptimes["current"].setValue( m_darkExptimesCsv );
    m_indiP_darkExptimes["target"].setValue( m_darkExptimesCsv );
    m_indiP_camNFrameDelay["current"].setValue( m_camNFrameDelay );
    m_indiP_camNFrameDelay["target"].setValue( m_camNFrameDelay );
    publishCamMirrors();

    REG_INDI_SETPROP( m_indiP_remoteExptime, m_camName, "exptime" );
    REG_INDI_SETPROP( m_indiP_remoteEmgain, m_camName, "emgain" );
    REG_INDI_SETPROP( m_indiP_remoteFps, m_camName, "fps" );
    REG_INDI_SETPROP( m_indiP_remoteBlacklevel, m_camName, "blacklevel" );
    REG_INDI_SETPROP( m_indiP_remoteBitDepth, m_camName, "bitDepth" );
    REG_INDI_SETPROP( m_indiP_remoteRoiX, m_camName, "roi_region_x" );
    REG_INDI_SETPROP( m_indiP_remoteRoiY, m_camName, "roi_region_y" );
    REG_INDI_SETPROP( m_indiP_remoteRoiW, m_camName, "roi_region_w" );
    REG_INDI_SETPROP( m_indiP_remoteRoiH, m_camName, "roi_region_h" );
    REG_INDI_SETPROP( m_indiP_remoteFastCam, m_camName, "fast_cam" );
    REG_INDI_SETPROP( m_indiP_remoteShutter, m_shutterDevice, "shutter" );

    m_worker = std::thread( workerStart, this );
    state( stateCodes::READY );
    log<text_log>( "darkCtrl started" );
    return 0;
}

int darkCtrl::appLogic()
{
    if( m_busy.load() )
        state( stateCodes::OPERATING );
    else if( state() == stateCodes::OPERATING )
        state( stateCodes::READY );

    // Publish RO mirrors here — never from SET callbacks (sending SET on the
    // INDI thread drops later cam_name Def/SET, leaving exptime/ROI/blacklevel empty).
    std::unique_lock<std::mutex> lock( m_indiMutex, std::try_to_lock );
    if( lock.owns_lock() )
        publishCamMirrors();
    return 0;
}

int darkCtrl::appShutdown()
{
    m_workerShutdown = true;
    m_stopRequested = true;
    sem_post( &m_jobSem );
    try
    {
        if( m_worker.joinable() )
            m_worker.join();
    }
    catch( ... )
    {
    }
    closeStreams();
    sem_destroy( &m_jobSem );
    return 0;
}

void darkCtrl::workerStart( darkCtrl *s )
{
    s->workerExec();
}

void darkCtrl::workerExec()
{
    while( !m_workerShutdown.load() )
    {
        sem_wait( &m_jobSem );
        if( m_workerShutdown.load() )
            break;
        Job job = Job::Idle;
        {
            std::lock_guard<std::mutex> lock( m_jobMutex );
            job = m_pendingJob;
            m_pendingJob = Job::Idle;
        }
        if( job == Job::Idle )
            continue;
        m_busy = 1;
        m_stopRequested = false;
        (void)runJob( job );
        m_busy = 0;
        setStatus( "idle" );
    }
}

void darkCtrl::queueJob( Job j )
{
    if( j == Job::Stop )
    {
        m_stopRequested = true;
        return;
    }
    {
        std::lock_guard<std::mutex> lock( m_jobMutex );
        if( m_busy.load() )
        {
            log<text_log>( "darkCtrl busy; ignoring job", logPrio::LOG_WARNING );
            return;
        }
        m_pendingJob = j;
    }
    sem_post( &m_jobSem );
}

int darkCtrl::runJob( Job j )
{
    if( j == Job::Build )
        return doDarkLibBuild();
    return 0;
}

void darkCtrl::setStatus( const std::string &s )
{
    m_status = s;
    updateIfChanged( m_indiP_status, "current", m_status );
}

void darkCtrl::clearRequest( pcf::IndiProperty &p )
{
    updateSwitchIfChanged( p, "request", pcf::IndiElement::Off, INDI_IDLE );
}

int darkCtrl::registerCamRoNumber( pcf::IndiProperty &p, const char *name, const char *label,
                                   double minv, double maxv, double step, const char *fmt )
{
    if( createROIndiNumber( p, name, label, "camera" ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber" } );
    if( indi::addNumberElement<double>( p, "current", minv, maxv, step, fmt, "current" ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "addNumberElement" } );
    if( registerIndiPropertyReadOnly( p ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "registerIndiPropertyReadOnly" } );
    return 0;
}

void darkCtrl::publishCamMirrors()
{
    if( std::isfinite( m_camExptime ) )
        updateIfChanged( m_indiP_camExptime, "current", m_camExptime );
    if( std::isfinite( m_camEmgain ) )
        updateIfChanged( m_indiP_camEmgain, "current", m_camEmgain );
    if( std::isfinite( m_camFps ) )
        updateIfChanged( m_indiP_camFps, "current", m_camFps );
    if( std::isfinite( m_camBlacklevel ) )
        updateIfChanged( m_indiP_camBlacklevel, "current", m_camBlacklevel );
    updateIfChanged( m_indiP_camBitdepth, "current", static_cast<double>( m_camBitdepth ) );
    updateIfChanged( m_indiP_camRoiX, "current", static_cast<double>( m_camRoiX ) );
    updateIfChanged( m_indiP_camRoiY, "current", static_cast<double>( m_camRoiY ) );
    updateIfChanged( m_indiP_camRoiWidth, "current", static_cast<double>( m_camRoiWidth ) );
    updateIfChanged( m_indiP_camRoiHeight, "current", static_cast<double>( m_camRoiHeight ) );
}

int darkCtrl::openCamsci()
{
    if( m_camsciOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_camsci, m_shmCamInput.c_str() ) != IMAGESTREAMIO_SUCCESS )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_shmCamInput } );
    m_camsciOpen = true;
    return 0;
}

void darkCtrl::closeStreams()
{
    if( m_camsciOpen )
    {
        ImageStreamIO_closeIm( &m_camsci );
        m_camsciOpen = false;
    }
}

void darkCtrl::publishShutterIndi( bool closed )
{
    if( closed )
        updateSwitchIfChanged( m_indiP_shutter, "toggle", pcf::IndiElement::On, INDI_OK );
    else
        updateSwitchIfChanged( m_indiP_shutter, "toggle", pcf::IndiElement::Off, INDI_IDLE );
}

int darkCtrl::setShutterClosed( bool closed )
{
    log<text_log>( std::string( "shutter -> " ) + ( closed ? "CLOSED" : "OPEN" ) + " (" +
                   m_shutterDevice + ".shutter)" );

    // Same pattern as observerCtrl::commandStreamWriter / sendNewStandardIndiToggle.
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( m_shutterDevice );
    ip.setName( "shutter" );
    ip.add( pcf::IndiElement( "toggle" ) );
    ip["toggle"].setSwitchState( closed ? pcf::IndiElement::On : pcf::IndiElement::Off );
    if( sendNewProperty( ip ) < 0 )
        return -1;

    publishShutterIndi( closed );
    return 0;
}

int darkCtrl::setCamExpValue( double seconds )
{
    log<text_log>( "cam_exp -> " + std::to_string( seconds ) + " s (" + m_camName +
                   ".exptime)" );

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( m_camName );
    ip.setName( "exptime" );
    ip.add( pcf::IndiElement( "target" ) );
    ip["target"].setValue( seconds );
    return sendNewProperty( ip );
}

int darkCtrl::setCamFpsValue( double fps )
{
    log<text_log>( "cam_fps -> " + std::to_string( fps ) + " (" + m_camName + ".fps)" );

    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( m_camName );
    ip.setName( "fps" );
    ip.add( pcf::IndiElement( "target" ) );
    ip["target"].setValue( fps );
    return sendNewProperty( ip );
}

double darkCtrl::maxFpsForExp( double seconds ) const
{
    // Need period >= exptime (+ tiny margin) so the camera can accept the exposure.
    constexpr double margin_s = 1e-6;
    double period = seconds + margin_s;
    if( !( period > 0.0 ) )
        period = margin_s;
    double fps = 1.0 / period;
    if( std::isfinite( m_remoteMaxFps ) && m_remoteMaxFps > 0.0 && fps > m_remoteMaxFps )
        fps = m_remoteMaxFps;
    // Avoid absurdly tiny fps requests.
    if( fps < 1e-3 )
        fps = 1e-3;
    return fps;
}

int darkCtrl::waitCamValue( const double &current, double expected, unsigned timeout_ms,
                            const char *what, bool required )
{
    const double tol =
        std::max( m_expConfirmTolAbs, m_expConfirmTolRel * std::fabs( expected ) );
    const double t0 = mx::sys::get_curr_time();
    while( true )
    {
        if( m_stopRequested.load() || m_workerShutdown.load() )
            return -1;

        double cur = std::numeric_limits<double>::quiet_NaN();
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            cur = current;
        }
        if( std::isfinite( cur ) && std::fabs( cur - expected ) <= tol )
        {
            log<text_log>( std::string( what ) + ".current confirmed=" + std::to_string( cur ) +
                           " (target=" + std::to_string( expected ) + ")" );
            return 0;
        }

        const double elapsed_ms = ( mx::sys::get_curr_time() - t0 ) * 1000.0;
        if( elapsed_ms >= static_cast<double>( timeout_ms ) )
        {
            std::ostringstream ss;
            ss << what << ".current not confirmed within " << timeout_ms << " ms"
               << " (target=" << expected << ", current=";
            if( std::isfinite( cur ) )
                ss << cur;
            else
                ss << "nan";
            ss << ", tol=" << tol << ")";
            if( required )
                return log<software_error, -1>( { __FILE__, __LINE__, ss.str() } );
            log<text_log>( ss.str(), logPrio::LOG_WARNING );
            return 0;
        }
        mx::sys::milliSleep( 10 );
    }
}

int darkCtrl::waitCamExpCurrent( double expected_s, unsigned timeout_ms )
{
    return waitCamValue( m_camExptime, expected_s, timeout_ms, "exptime", true );
}

int darkCtrl::waitCamFpsCurrent( double expected_fps, unsigned timeout_ms )
{
    return waitCamValue( m_camFps, expected_fps, timeout_ms, "fps", false );
}

void darkCtrl::restoreAfterDarkLib( double exptime0, double fps0, bool restore_fps )
{
    log<text_log>( "dark_lib_build: opening shutter (" + m_shutterDevice + ".shutter Off)" );
    if( setShutterClosed( false ) < 0 )
        log<text_log>( "dark_lib_build: failed to open shutter", logPrio::LOG_WARNING );

    if( restore_fps && std::isfinite( fps0 ) )
    {
        if( setCamFpsValue( fps0 ) < 0 )
            log<text_log>( "dark_lib_build: failed to restore fps", logPrio::LOG_WARNING );
    }

    if( std::isfinite( exptime0 ) )
    {
        log<text_log>( "dark_lib_build: restoring " + m_camName + ".exptime.target=" +
                       std::to_string( exptime0 ) + " s" );
        if( setCamExpValue( exptime0 ) < 0 )
        {
            log<text_log>( "dark_lib_build: failed to restore exptime", logPrio::LOG_WARNING );
            return;
        }
        waitCamValue( m_camExptime, exptime0, m_expConfirmTimeout_ms, "exptime", false );
    }
    else
    {
        log<text_log>( "dark_lib_build: no prior exptime.current to restore",
                       logPrio::LOG_WARNING );
    }
}

int darkCtrl::applyCamExpForDark( double seconds )
{
    // fast_cam is optional. If the camera has no such property, SET never arrives
    // (m_haveRemoteFastCam stays false) — same path as fast_cam Off.
    const bool fastOn = m_haveRemoteFastCam && m_remoteFastCamOn;
    if( fastOn )
    {
        log<text_log>( "fast_cam On: setting exptime only (leaving fps=" +
                       ( std::isfinite( m_camFps ) ? std::to_string( m_camFps ) : std::string( "nan" ) ) +
                       ")" );
        if( setCamExpValue( seconds ) < 0 )
            return -1;
        return waitCamExpCurrent( seconds, m_expConfirmTimeout_ms );
    }

    if( m_haveRemoteFastCam )
        log<text_log>( "fast_cam Off: set exptime then max fps for that exposure" );
    else
        log<text_log>( "fast_cam not present on " + m_camName +
                       ": set exptime then max fps (same as Off)" );

    // 1) Command exposure and wait for cam_exptime. If the camera recaps because
    //    fps is too high, drop fps and retry the exposure once.
    if( setCamExpValue( seconds ) < 0 )
        return -1;
    if( waitCamValue( m_camExptime, seconds, m_expConfirmTimeout_ms, "exptime", false ) < 0 )
        return -1;

    const bool expOk = std::isfinite( m_camExptime ) &&
                       std::fabs( m_camExptime - seconds ) <=
                           std::max( m_expConfirmTolAbs, m_expConfirmTolRel * std::fabs( seconds ) );
    if( !expOk )
    {
        const double fpsRetry = maxFpsForExp( seconds );
        log<text_log>( "exptime not at command yet; setting fps=" + std::to_string( fpsRetry ) +
                       " then retrying exptime" );
        if( setCamFpsValue( fpsRetry ) < 0 )
            return -1;
        if( waitCamFpsCurrent( fpsRetry, m_expConfirmTimeout_ms ) < 0 )
            return -1;
        if( setCamExpValue( seconds ) < 0 )
            return -1;
        if( waitCamExpCurrent( seconds, m_expConfirmTimeout_ms ) < 0 )
            return -1;
    }

    // 2) Fastest framerate that still fits the confirmed exposure.
    const double expNow = std::isfinite( m_camExptime ) ? m_camExptime : seconds;
    const double fps = maxFpsForExp( expNow );
    if( setCamFpsValue( fps ) < 0 )
        return -1;
    if( waitCamFpsCurrent( fps, m_expConfirmTimeout_ms ) < 0 )
        return -1;

    // Reconfirm exptime in case the camera recapped it when fps changed.
    return waitCamExpCurrent( seconds, m_expConfirmTimeout_ms );
}

bool darkCtrl::storeRemoteCurrent( const pcf::IndiProperty &want, const pcf::IndiProperty &ipRecv,
                                   double &dest, bool &have, const char *what )
{
    try
    {
        if( want.getDevice() != ipRecv.getDevice() || want.getName() != ipRecv.getName() )
            return false;
        double v = 0.0;
        if( !parseIndiCurrentNumber( ipRecv, v ) )
            return false;
        bool first = false;
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            first = !have;
            dest = v;
            have = true;
        }
        if( first && what )
            log<text_log>( std::string( what ) + ".current=" + std::to_string( v ) + " (" +
                           m_camName + ")" );
        return true;
    }
    catch( ... )
    {
        return false;
    }
}

int darkCtrl::waitCamStampMeta( unsigned timeout_ms )
{
    sendGetPropertySetList( true );
    const double t0 = mx::sys::get_curr_time();
    std::string missing;
    while( true )
    {
        missing.clear();
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            if( !m_haveCamEmgain )
                missing += "emgain ";
            if( !m_haveCamBlacklevel )
                missing += "blacklevel ";
            if( !m_haveCamBitdepth )
                missing += "bitDepth ";
            if( !m_haveCamRoiX )
                missing += "roi_region_x ";
            if( !m_haveCamRoiY )
                missing += "roi_region_y ";
            if( !m_haveCamRoiW )
                missing += "roi_region_w ";
            if( !m_haveCamRoiH )
                missing += "roi_region_h ";
        }
        if( missing.empty() )
            break;
        if( m_stopRequested.load() || m_workerShutdown.load() )
            return -1;
        if( ( mx::sys::get_curr_time() - t0 ) * 1000.0 >= static_cast<double>( timeout_ms ) )
        {
            return log<software_error, -1>(
                { __FILE__,
                  __LINE__,
                  "camera stamp fields not received from " + m_camName + " within " +
                      std::to_string( timeout_ms ) + " ms (missing " + missing +
                      "). Check getINDI " + m_camName +
                      ".{emgain,blacklevel,bitDepth,roi_region_*}." } );
        }
        mx::sys::milliSleep( 20 );
    }

    std::ostringstream ss;
    ss << "camera meta from " << m_camName << ": exptime=";
    if( m_haveCamExptime && std::isfinite( m_camExptime ) )
        ss << m_camExptime;
    else
        ss << "(pending command)";
    ss << " emgain=" << m_camEmgain << " blacklevel=" << m_camBlacklevel
       << " bitdepth=" << m_camBitdepth << " roi=" << m_camRoiX << "," << m_camRoiY << " "
       << m_camRoiWidth << "x" << m_camRoiHeight;
    log<text_log>( ss.str() );
    return 0;
}

int darkCtrl::grabMean( unsigned nframes, unsigned wait_frames, std::vector<float> &out,
                        uint32_t &w, uint32_t &h, unsigned *n_collected )
{
    if( openCamsci() < 0 )
        return -1;
    if( nframes == 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "nframes==0" } );

    w = m_camsci.md->size[0];
    h = ( m_camsci.md->naxis > 1 ) ? m_camsci.md->size[1] : 1;
    const size_t npix = static_cast<size_t>( w ) * static_cast<size_t>( h );
    std::vector<double> acc( npix, 0.0 );

    uint64_t cnt0 = m_camsci.md->cnt0;
    unsigned skipped = 0;
    unsigned collected = 0;
    while( collected < nframes )
    {
        if( m_stopRequested.load() || m_workerShutdown.load() )
            return -1;
        ImageStreamIO_semwait( &m_camsci, 1 );
        if( m_camsci.md->cnt0 == cnt0 )
            continue;
        cnt0 = m_camsci.md->cnt0;
        if( skipped < wait_frames )
        {
            ++skipped;
            continue;
        }

        if( m_camsci.md->datatype == _DATATYPE_FLOAT )
        {
            const float *p = (const float *)m_camsci.array.raw;
            for( size_t i = 0; i < npix; ++i )
                acc[i] += p[i];
        }
        else if( m_camsci.md->datatype == _DATATYPE_UINT16 )
        {
            const uint16_t *p = (const uint16_t *)m_camsci.array.raw;
            for( size_t i = 0; i < npix; ++i )
                acc[i] += p[i];
        }
        else if( m_camsci.md->datatype == _DATATYPE_INT32 )
        {
            const int32_t *p = (const int32_t *)m_camsci.array.raw;
            for( size_t i = 0; i < npix; ++i )
                acc[i] += p[i];
        }
        else
        {
            return log<software_error, -1>( { __FILE__, __LINE__, "unsupported camsci dtype" } );
        }
        ++collected;
    }

    if( collected == 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "grabMean collected 0 frames" } );

    out.resize( npix );
    const float inv = 1.0f / static_cast<float>( collected );
    for( size_t i = 0; i < npix; ++i )
        out[i] = static_cast<float>( acc[i] ) * inv;
    if( n_collected )
        *n_collected = collected;
    return 0;
}

int darkCtrl::ensureDir( const std::string &dir )
{
    if( dir.empty() )
        return -1;
    if( mkdir( dir.c_str(), 0755 ) != 0 && errno != EEXIST )
        return log<software_error, -1>( { __FILE__, __LINE__, "mkdir failed: " + dir } );
    return 0;
}

int darkCtrl::saveFitsF32( const std::string &path, const std::vector<float> &im, uint32_t w,
                           uint32_t h )
{
    try
    {
        mx::fits::fitsFile<float> ff;
        // mxlib: write(fname, data, d1, d2, d3) — d3=1 for a 2D image
        if( ff.write( path, im.data(), static_cast<int>( w ), static_cast<int>( h ), 1 ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "FITS write failed: " + path } );
        }
    }
    catch( const std::exception &e )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "FITS write failed: " ) + e.what() } );
    }
    return 0;
}

int darkCtrl::doDarkLibBuild()
{
    setStatus( "dark_lib_build: starting" );
    if( openCamsci() < 0 )
        return -1;
    if( ensureDir( m_darkLibPath ) < 0 )
        return -1;

    std::vector<double> times;
    {
        std::stringstream ss( m_darkExptimesCsv );
        std::string tok;
        while( std::getline( ss, tok, ',' ) )
        {
            tok = trimCopy( tok );
            if( tok.empty() )
                continue;
            char *end = nullptr;
            const double v = std::strtod( tok.c_str(), &end );
            if( end == tok.c_str() || !std::isfinite( v ) || v <= 0.0 )
            {
                log<text_log>( "dark_lib_build: skipping invalid exptime token '" + tok + "'",
                               logPrio::LOG_WARNING );
                continue;
            }
            times.push_back( v );
        }
    }
    if( times.empty() )
        return log<software_error, -1>( { __FILE__, __LINE__, "dark_exptimes CSV empty" } );

    log<text_log>( "dark_lib_build: " + std::to_string( times.size() ) + " exposures, cam=" +
                   m_camName + ", shutter=" + m_shutterDevice + ", path=" + m_darkLibPath +
                   ", fast_cam=" +
                   ( !m_haveRemoteFastCam
                         ? "absent (will set max fps per exposure)"
                         : ( m_remoteFastCamOn ? "On (exptime only)" : "Off (max fps per exposure)" ) ) );

    if( waitCamStampMeta( 10000 ) < 0 )
        return -1;

    double exptime_start = std::numeric_limits<double>::quiet_NaN();
    double fps_start = std::numeric_limits<double>::quiet_NaN();
    bool restore_fps = true;
    {
        std::unique_lock<std::mutex> lock( m_indiMutex );
        exptime_start = m_camExptime;
        fps_start = m_camFps;
        restore_fps = !( m_haveRemoteFastCam && m_remoteFastCamOn );
    }
    log<text_log>( "dark_lib_build: prior exptime=" +
                   ( std::isfinite( exptime_start ) ? std::to_string( exptime_start ) + " s"
                                                    : std::string( "nan" ) ) );

    if( setShutterClosed( true ) < 0 )
        return -1;

    struct Restore
    {
        darkCtrl *self;
        double exp0;
        double fps0;
        bool do_fps;
        ~Restore()
        {
            self->restoreAfterDarkLib( exp0, fps0, do_fps );
        }
    } restore{ this, exptime_start, fps_start, restore_fps };

    std::vector<lina::DarkLibraryEntry> entries;

    for( size_t i = 0; i < times.size(); ++i )
    {
        if( m_stopRequested.load() )
            break;
        setStatus( "dark_lib_build: exptime=" + std::to_string( times[i] ) );
        if( applyCamExpForDark( times[i] ) < 0 )
            return -1;
        mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

        double stamp_exptime = 0.0;
        double stamp_emgain = 0.0;
        double stamp_blacklevel = 0.0;
        unsigned stamp_bitdepth = 0;
        int stamp_roi_x = 0;
        int stamp_roi_y = 0;
        unsigned stamp_roi_w = 0;
        unsigned stamp_roi_h = 0;
        std::string stamp_err;
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            if( !m_haveCamExptime || !std::isfinite( m_camExptime ) )
                stamp_err = m_camName + ".exptime.current not received";
            else if( !m_haveCamEmgain || !std::isfinite( m_camEmgain ) )
                stamp_err = m_camName + ".emgain.current not received";
            else if( !m_haveCamBlacklevel || !std::isfinite( m_camBlacklevel ) )
                stamp_err = m_camName + ".blacklevel.current not received";
            else if( !m_haveCamBitdepth )
                stamp_err = m_camName + ".bitDepth.current not received";
            else if( !m_haveCamRoiX || !m_haveCamRoiY || !m_haveCamRoiW || !m_haveCamRoiH )
                stamp_err = m_camName + ".roi_region_*.current not received";
            else
            {
                stamp_exptime = m_camExptime;
                stamp_emgain = m_camEmgain;
                stamp_blacklevel = m_camBlacklevel;
                stamp_bitdepth = m_camBitdepth;
                stamp_roi_x = m_camRoiX;
                stamp_roi_y = m_camRoiY;
                stamp_roi_w = m_camRoiWidth;
                stamp_roi_h = m_camRoiHeight;
            }
        }
        if( !stamp_err.empty() )
            return log<software_error, -1>( { __FILE__, __LINE__, stamp_err } );

        std::vector<float> dark;
        uint32_t w = 0, h = 0;
        unsigned collected = 0;
        if( grabMean( m_darkNImages, m_camNFrameDelay, dark, w, h, &collected ) < 0 )
            return -1;

        char rel[64];
        std::snprintf( rel, sizeof( rel ), "dark_%03zu.fits", i );
        if( saveFitsF32( m_darkLibPath + "/" + rel, dark, w, h ) < 0 )
            return -1;

        lina::DarkLibraryEntry e;
        e.exptime = stamp_exptime;
        e.relpath = rel;
        e.ndark = ( collected > 0 ) ? collected : m_darkNImages;
        e.shm_cam_input = m_shmCamInput;
        e.cam_name = m_camName;
        e.width = w;
        e.height = h;
        e.bitdepth = stamp_bitdepth;
        e.roi_x = stamp_roi_x;
        e.roi_y = stamp_roi_y;
        e.roi_width = stamp_roi_w;
        e.roi_height = stamp_roi_h;
        e.gain = stamp_emgain;
        e.blacklevel = stamp_blacklevel;
        entries.push_back( e );

        try
        {
            lina::write_dark_library_manifest( m_darkLibPath, entries );
        }
        catch( const std::exception &ex )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, std::string( "failed to write dark_metadata.txt: " ) + ex.what() } );
        }

        log<text_log>( "dark_lib_build: wrote " + m_darkLibPath + "/" + rel +
                       " (exptime=" + std::to_string( stamp_exptime ) + " s, ndark=" +
                       std::to_string( e.ndark ) + ", roi=" + std::to_string( stamp_roi_x ) + "," +
                       std::to_string( stamp_roi_y ) + " " + std::to_string( stamp_roi_w ) + "x" +
                       std::to_string( stamp_roi_h ) + ", emgain=" + std::to_string( stamp_emgain ) +
                       ", blacklevel=" + std::to_string( stamp_blacklevel ) + ")" );
    }

    if( entries.empty() )
        return log<software_error, -1>( { __FILE__, __LINE__, "no darks written" } );

    setStatus( "dark_lib_build: done" );
    log<text_log>( "dark_lib_build wrote " + std::to_string( entries.size() ) + " darks + " +
                   m_darkLibPath + "/" + lina::kDarkMetadataFile );
    return 0;
}

// --- INDI callbacks (soft params) ---

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_shmCamInput )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmCamInput, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmCamInput, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shmCamInput )
        return 0;
    if( m_busy.load() )
        return 0;
    m_shmCamInput = target;
    updateIfChanged( m_indiP_shmCamInput, "current", m_shmCamInput );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_camName )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camName, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_camName, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_camName )
        log<text_log>( "cam_name -> " + target +
                       " (restart app to rebind SET subscription)",
                       logPrio::LOG_WARNING );
    m_camName = target;
    updateIfChanged( m_indiP_camName, "current", m_camName );
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_darkLibPath )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkLibPath, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_darkLibPath, target, ipRecv, false ) < 0 )
        return -1;
    m_darkLibPath = target;
    updateIfChanged( m_indiP_darkLibPath, "current", m_darkLibPath );
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_shutterDevice )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shutterDevice, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shutterDevice, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shutterDevice )
        return 0;
    if( m_busy.load() )
        return 0;
    m_shutterDevice = target;
    updateIfChanged( m_indiP_shutterDevice, "current", m_shutterDevice );
    log<text_log>( "shutter_device -> " + m_shutterDevice +
                   " (restart app to rebind SET subscription)",
                   logPrio::LOG_WARNING );
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_shutter )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shutter, ipRecv );
    if( !ipRecv.find( "toggle" ) )
        return -1;
    if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
        return setShutterClosed( true );
    if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::Off )
        return setShutterClosed( false );
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_darkNImages )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkNImages, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_darkNImages, target, ipRecv, false ) < 0 )
        return -1;
    m_darkNImages = target;
    updateIfChanged( m_indiP_darkNImages, "current", m_darkNImages );
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_darkExptimes )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkExptimes, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_darkExptimes, target, ipRecv, false ) < 0 )
        return -1;
    m_darkExptimesCsv = target;
    updateIfChanged( m_indiP_darkExptimes, "current", m_darkExptimesCsv );
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_camNFrameDelay )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camNFrameDelay, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_camNFrameDelay, target, ipRecv, false ) < 0 )
        return -1;
    m_camNFrameDelay = target;
    updateIfChanged( m_indiP_camNFrameDelay, "current", m_camNFrameDelay );
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_darkLibBuild )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkLibBuild, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_darkLibBuild, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::Build );
        clearRequest( m_indiP_darkLibBuild );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( darkCtrl, m_indiP_stop )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_stop, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_stop, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::Stop );
        clearRequest( m_indiP_stop );
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteExptime )( const pcf::IndiProperty &ipRecv )
{
    storeRemoteCurrent( m_indiP_remoteExptime, ipRecv, m_camExptime, m_haveCamExptime, "exptime" );
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteEmgain )( const pcf::IndiProperty &ipRecv )
{
    storeRemoteCurrent( m_indiP_remoteEmgain, ipRecv, m_camEmgain, m_haveCamEmgain, "emgain" );
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteFps )( const pcf::IndiProperty &ipRecv )
{
    storeRemoteCurrent( m_indiP_remoteFps, ipRecv, m_camFps, m_haveCamFps, "fps" );
    try
    {
        if( ipRecv.find( "current" ) )
        {
            const std::string mx = ipRecv["current"].getMax();
            if( !mx.empty() )
            {
                const double vmax = std::strtod( mx.c_str(), nullptr );
                if( vmax > 0.0 )
                    m_remoteMaxFps = vmax;
            }
        }
    }
    catch( ... )
    {
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteBlacklevel )( const pcf::IndiProperty &ipRecv )
{
    storeRemoteCurrent( m_indiP_remoteBlacklevel, ipRecv, m_camBlacklevel, m_haveCamBlacklevel,
                        "blacklevel" );
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteBitDepth )( const pcf::IndiProperty &ipRecv )
{
    try
    {
        if( m_indiP_remoteBitDepth.getDevice() != ipRecv.getDevice() ||
            m_indiP_remoteBitDepth.getName() != ipRecv.getName() )
            return 0;
        double v = 0.0;
        if( !parseIndiCurrentNumber( ipRecv, v ) || v < 0.0 )
            return 0;
        bool first = false;
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            first = !m_haveCamBitdepth;
            m_camBitdepth = static_cast<unsigned>( std::lround( v ) );
            m_haveCamBitdepth = true;
        }
        if( first )
            log<text_log>( "bitDepth.current=" + std::to_string( m_camBitdepth ) + " (" + m_camName +
                           ")" );
    }
    catch( ... )
    {
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteRoiX )( const pcf::IndiProperty &ipRecv )
{
    try
    {
        if( m_indiP_remoteRoiX.getDevice() != ipRecv.getDevice() ||
            m_indiP_remoteRoiX.getName() != ipRecv.getName() )
            return 0;
        double v = 0.0;
        if( !parseIndiCurrentNumber( ipRecv, v ) )
            return 0;
        bool first = false;
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            first = !m_haveCamRoiX;
            m_camRoiX = static_cast<int>( std::lround( v ) );
            m_haveCamRoiX = true;
        }
        if( first )
            log<text_log>( "roi_region_x.current=" + std::to_string( m_camRoiX ) + " (" + m_camName +
                           ")" );
    }
    catch( ... )
    {
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteRoiY )( const pcf::IndiProperty &ipRecv )
{
    try
    {
        if( m_indiP_remoteRoiY.getDevice() != ipRecv.getDevice() ||
            m_indiP_remoteRoiY.getName() != ipRecv.getName() )
            return 0;
        double v = 0.0;
        if( !parseIndiCurrentNumber( ipRecv, v ) )
            return 0;
        bool first = false;
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            first = !m_haveCamRoiY;
            m_camRoiY = static_cast<int>( std::lround( v ) );
            m_haveCamRoiY = true;
        }
        if( first )
            log<text_log>( "roi_region_y.current=" + std::to_string( m_camRoiY ) + " (" + m_camName +
                           ")" );
    }
    catch( ... )
    {
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteRoiW )( const pcf::IndiProperty &ipRecv )
{
    try
    {
        if( m_indiP_remoteRoiW.getDevice() != ipRecv.getDevice() ||
            m_indiP_remoteRoiW.getName() != ipRecv.getName() )
            return 0;
        double v = 0.0;
        if( !parseIndiCurrentNumber( ipRecv, v ) || v <= 0.0 )
            return 0;
        bool first = false;
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            first = !m_haveCamRoiW;
            m_camRoiWidth = static_cast<unsigned>( std::lround( v ) );
            m_haveCamRoiW = true;
        }
        if( first )
            log<text_log>( "roi_region_w.current=" + std::to_string( m_camRoiWidth ) + " (" +
                           m_camName + ")" );
    }
    catch( ... )
    {
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteRoiH )( const pcf::IndiProperty &ipRecv )
{
    try
    {
        if( m_indiP_remoteRoiH.getDevice() != ipRecv.getDevice() ||
            m_indiP_remoteRoiH.getName() != ipRecv.getName() )
            return 0;
        double v = 0.0;
        if( !parseIndiCurrentNumber( ipRecv, v ) || v <= 0.0 )
            return 0;
        bool first = false;
        {
            std::unique_lock<std::mutex> lock( m_indiMutex );
            first = !m_haveCamRoiH;
            m_camRoiHeight = static_cast<unsigned>( std::lround( v ) );
            m_haveCamRoiH = true;
        }
        if( first )
            log<text_log>( "roi_region_h.current=" + std::to_string( m_camRoiHeight ) + " (" +
                           m_camName + ")" );
    }
    catch( ... )
    {
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteFastCam )( const pcf::IndiProperty &ipRecv )
{
    // Optional property: cameras without fast_cam never send this. Never throw / never
    // fail the INDI path if the payload is incomplete.
    try
    {
        if( ipRecv.getDevice() != m_indiP_remoteFastCam.getDevice() ||
            ipRecv.getName() != m_indiP_remoteFastCam.getName() )
            return 0;
        if( !ipRecv.find( "toggle" ) )
            return 0;
        m_remoteFastCamOn = ( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On );
        m_haveRemoteFastCam = true;
    }
    catch( ... )
    {
        return 0;
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( darkCtrl, m_indiP_remoteShutter )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteShutter, ipRecv );
    if( !ipRecv.find( "toggle" ) )
        return 0;
    m_remoteShutterClosed = ( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On );
    m_haveRemoteShutter = true;
    publishShutterIndi( m_remoteShutterClosed );
    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // darkCtrl_hpp
