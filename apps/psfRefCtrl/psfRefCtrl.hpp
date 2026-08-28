/** \file psfRefCtrl.hpp
  * \brief MagAO-X app that extracts PSF reference-taking from iefcCtrl.
  *
  * Takes reference PSF with FSM pokes, picks dark from library, writes package directory.
  *
  * \ingroup psfRefCtrl_files
  */

#ifndef psfRefCtrl_hpp
#define psfRefCtrl_hpp

#include <atomic>
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

#include <lina/iefc_package.h>
#include <lina/dark_library.h>

/** \defgroup psfRefCtrl MagAO-X PSF reference controller
  * \ingroup app_files
  */

namespace MagAOX
{
namespace app
{

class psfRefCtrl : public MagAOXApp<true>
{
  public:
    enum class Job : int
    {
        Idle = 0,
        TakeRef,
        Stop
    };

    ~psfRefCtrl() noexcept
    {
    }

  protected:
    std::string m_shmCamInput{ "camsci_sim" };
    std::string m_camName{ "nsvsim" };
    std::string m_fsmName{ "fsmsim" };
    std::string m_darkLibPath{ };
    std::string m_dir{ "./ref_psf" };

    unsigned m_nFrames{ 20 };
    double m_fsmPokeTip{ 1000.0 }; // nm
    double m_fsmPokeTilt{ 1000.0 }; // nm
    double m_fsmRefTip{ 0.0 }; // nm
    double m_fsmRefTilt{ 0.0 }; // nm
    double m_maxRef{ 0.0 };
    double m_satThresh{ 55000.0 };
    float m_settle_s{ 0.5f };
    unsigned m_camNFrameDelay{ 1 };

    // Remote currents mirrored from INDI SET callbacks.
    double m_remoteExp{ std::numeric_limits<double>::quiet_NaN() };
    double m_remoteGain{ std::numeric_limits<double>::quiet_NaN() };
    double m_remoteBlacklevel{ std::numeric_limits<double>::quiet_NaN() };

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
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_shmCamInput );
    pcf::IndiProperty m_indiP_camName;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_camName );
    pcf::IndiProperty m_indiP_fsmName;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_fsmName );
    pcf::IndiProperty m_indiP_darkLibPath;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_darkLibPath );
    pcf::IndiProperty m_indiP_dir;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_dir );

    /// Remote SET subscriptions (camera device).
    pcf::IndiProperty m_indiP_remoteExptime;
    INDI_SETCALLBACK_DECL( psfRefCtrl, m_indiP_remoteExptime );
    pcf::IndiProperty m_indiP_remoteEmgain;
    INDI_SETCALLBACK_DECL( psfRefCtrl, m_indiP_remoteEmgain );
    pcf::IndiProperty m_indiP_remoteBlacklevel;
    INDI_SETCALLBACK_DECL( psfRefCtrl, m_indiP_remoteBlacklevel );

    pcf::IndiProperty m_indiP_nFrames;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_nFrames );
    pcf::IndiProperty m_indiP_fsmPokeTip;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_fsmPokeTip );
    pcf::IndiProperty m_indiP_fsmPokeTilt;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_fsmPokeTilt );
    pcf::IndiProperty m_indiP_fsmRefTip;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_fsmRefTip );
    pcf::IndiProperty m_indiP_fsmRefTilt;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_fsmRefTilt );
    pcf::IndiProperty m_indiP_maxRef;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_maxRef );
    pcf::IndiProperty m_indiP_satThresh;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_satThresh );
    pcf::IndiProperty m_indiP_settleS;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_settleS );
    pcf::IndiProperty m_indiP_camNFrameDelay;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_camNFrameDelay );

    pcf::IndiProperty m_indiP_takeRef;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_takeRef );
    pcf::IndiProperty m_indiP_stop;
    INDI_NEWCALLBACK_DECL( psfRefCtrl, m_indiP_stop );
    pcf::IndiProperty m_indiP_status;

  public:
    psfRefCtrl();

    virtual void setupConfig();
    virtual void loadConfig();
    virtual int appStartup();
    virtual int appLogic();
    virtual int appShutdown();

  protected:
    static void workerStart( psfRefCtrl *s );
    void workerExec();
    void queueJob( Job j );
    int runJob( Job j );

    void setStatus( const std::string &s );
    void clearRequest( pcf::IndiProperty &p );

    int openCamsci();
    void closeStreams();

    int setFsmPos( double tip_nm, double tilt_nm );
    int restoreFsmToRef();

    std::string formatDarkEntry( const lina::DarkLibraryEntry &e ) const;
    std::string pickDark( double target_exptime, const lina::DarkMatchFilter &filter,
                          lina::DarkLibraryEntry *matched, double *match_err );

    int grabMean( unsigned nframes, unsigned wait_frames, std::vector<float> &out, uint32_t &w,
                  uint32_t &h );
    int ensureDir( const std::string &dir );
    int saveFitsF32( const std::string &path, const std::vector<float> &im, uint32_t w,
                     uint32_t h );
    int writeConfigTxt( const std::string &path, const std::string &darkFile,
                        const std::string &refPsfFile, const std::string &refPsfDarkSubFile,
                        uint32_t w, uint32_t h, double peakMax );

    int doTakeRef();
};

psfRefCtrl::psfRefCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
}

void psfRefCtrl::setupConfig()
{
    config.add( "ref.shm_cam_input", "", "ref.shm_cam_input", argType::Required, "ref",
                "shm_cam_input", false, "string",
                "Science-camera ImageStreamIO name (e.g. camsci_sim; dark-library match key)." );
    config.add( "ref.cam_name", "", "ref.cam_name", argType::Required, "ref", "cam_name", false,
                "string", "INDI device for exptime/emgain/blacklevel (e.g. nsvsim)." );
    config.add( "ref.fsm_name", "", "ref.fsm_name", argType::Required, "ref", "fsm_name", false,
                "string", "INDI FSM device for tip/tilt positioning (e.g. fsmsim)." );
    config.add( "ref.dark_lib_path", "", "ref.dark_lib_path", argType::Required, "ref",
                "dark_lib_path", false, "string",
                "Directory with dark_NNN.fits + dark_metadata.txt (required)." );
    config.add( "ref.dir", "", "ref.dir", argType::Required, "ref", "dir", false, "string",
                "Package directory for reference PSF output." );
    config.add( "ref.n_frames", "", "ref.n_frames", argType::Required, "ref", "n_frames", false,
                "unsigned", "Frames averaged for reference PSF." );
    config.add( "ref.fsm_poke_tip", "", "ref.fsm_poke_tip", argType::Required, "ref",
                "fsm_poke_tip", false, "double", "FSM poke tip offset [nm]." );
    config.add( "ref.fsm_poke_tilt", "", "ref.fsm_poke_tilt", argType::Required, "ref",
                "fsm_poke_tilt", false, "double", "FSM poke tilt offset [nm]." );
    config.add( "ref.fsm_ref_tip", "", "ref.fsm_ref_tip", argType::Required, "ref", "fsm_ref_tip",
                false, "double", "FSM reference tip position [nm]." );
    config.add( "ref.fsm_ref_tilt", "", "ref.fsm_ref_tilt", argType::Required, "ref",
                "fsm_ref_tilt", false, "double", "FSM reference tilt position [nm]." );
    config.add( "ref.sat_thresh", "", "ref.sat_thresh", argType::Required, "ref", "sat_thresh",
                false, "double", "Saturation threshold (warn if any pixel >= this)." );
    config.add( "ref.settle_s", "", "ref.settle_s", argType::Required, "ref", "settle_s", false,
                "float", "Wall-clock settle [s] after FSM moves." );
    config.add( "ref.cam_n_frame_delay", "", "ref.cam_n_frame_delay", argType::Required, "ref",
                "cam_n_frame_delay", false, "unsigned", "Skip N frames after FSM move." );
}

void psfRefCtrl::loadConfig()
{
    config( m_shmCamInput, "ref.shm_cam_input" );
    config( m_camName, "ref.cam_name" );
    config( m_fsmName, "ref.fsm_name" );
    config( m_darkLibPath, "ref.dark_lib_path" );
    config( m_dir, "ref.dir" );
    config( m_nFrames, "ref.n_frames" );
    config( m_fsmPokeTip, "ref.fsm_poke_tip" );
    config( m_fsmPokeTilt, "ref.fsm_poke_tilt" );
    config( m_fsmRefTip, "ref.fsm_ref_tip" );
    config( m_fsmRefTilt, "ref.fsm_ref_tilt" );
    config( m_satThresh, "ref.sat_thresh" );
    config( m_settle_s, "ref.settle_s" );
    config( m_camNFrameDelay, "ref.cam_n_frame_delay" );
}

int psfRefCtrl::appStartup()
{
    if( sem_init( &m_jobSem, 0, 0 ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "sem_init failed" } );

    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmCamInput, "shm_cam_input", "Camera input shmim",
                              "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_camName, "cam_name",
                              "INDI camera device (exptime/emgain/blacklevel)", "camera" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_fsmName, "fsm_name", "INDI FSM device (tip/tilt)", "fsm" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_darkLibPath, "dark_lib_path", "Dark library directory",
                              "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_dir, "dir", "Package output directory", "paths" );

    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_nFrames, "n_frames", 1, 10000, 1, "%u",
                                 "Frames averaged for reference PSF", "ref" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmPokeTip, "fsm_poke_tip", -100000, 100000, 1, "%0.3f",
                                 "FSM poke tip [nm]", "fsm" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmPokeTilt, "fsm_poke_tilt", -100000, 100000, 1,
                                 "%0.3f", "FSM poke tilt [nm]", "fsm" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmRefTip, "fsm_ref_tip", -100000, 100000, 1, "%0.3f",
                                 "FSM reference tip [nm]", "fsm" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmRefTilt, "fsm_ref_tilt", -100000, 100000, 1, "%0.3f",
                                 "FSM reference tilt [nm]", "fsm" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_maxRef, "max_ref", 0, 1e9, 1, "%0.3f",
                                 "Max reference intensity (updated after take_ref)", "ref" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_satThresh, "sat_thresh", 0, 1e9, 1, "%0.3f",
                                 "Saturation threshold (warn only)", "ref" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_settleS, "settle_s", 0, 60, 0.01, "%0.3f",
                                 "Wall-clock settle after FSM move [s]", "ref" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_camNFrameDelay, "cam_n_frame_delay", 0, 1000, 1, "%u",
                                 "Skip N frames after FSM move", "ref" );

    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_takeRef, "take_ref" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_stop, "stop" );

    REG_INDI_NEWPROP_NOCB( m_indiP_status, "status", pcf::IndiProperty::Text );
    m_indiP_status.add( pcf::IndiElement( "current" ) );
    m_indiP_status["current"].set( m_status );

    m_indiP_shmCamInput["current"].setValue( m_shmCamInput );
    m_indiP_shmCamInput["target"].setValue( m_shmCamInput );
    m_indiP_camName["current"].setValue( m_camName );
    m_indiP_camName["target"].setValue( m_camName );
    m_indiP_fsmName["current"].setValue( m_fsmName );
    m_indiP_fsmName["target"].setValue( m_fsmName );
    m_indiP_darkLibPath["current"].setValue( m_darkLibPath );
    m_indiP_darkLibPath["target"].setValue( m_darkLibPath );
    m_indiP_dir["current"].setValue( m_dir );
    m_indiP_dir["target"].setValue( m_dir );
    m_indiP_nFrames["current"].setValue( m_nFrames );
    m_indiP_nFrames["target"].setValue( m_nFrames );
    m_indiP_fsmPokeTip["current"].setValue( m_fsmPokeTip );
    m_indiP_fsmPokeTip["target"].setValue( m_fsmPokeTip );
    m_indiP_fsmPokeTilt["current"].setValue( m_fsmPokeTilt );
    m_indiP_fsmPokeTilt["target"].setValue( m_fsmPokeTilt );
    m_indiP_fsmRefTip["current"].setValue( m_fsmRefTip );
    m_indiP_fsmRefTip["target"].setValue( m_fsmRefTip );
    m_indiP_fsmRefTilt["current"].setValue( m_fsmRefTilt );
    m_indiP_fsmRefTilt["target"].setValue( m_fsmRefTilt );
    m_indiP_maxRef["current"].setValue( m_maxRef );
    m_indiP_maxRef["target"].setValue( m_maxRef );
    m_indiP_satThresh["current"].setValue( m_satThresh );
    m_indiP_satThresh["target"].setValue( m_satThresh );
    m_indiP_settleS["current"].setValue( m_settle_s );
    m_indiP_settleS["target"].setValue( m_settle_s );
    m_indiP_camNFrameDelay["current"].setValue( m_camNFrameDelay );
    m_indiP_camNFrameDelay["target"].setValue( m_camNFrameDelay );

    REG_INDI_SETPROP( m_indiP_remoteExptime, m_camName, "exptime" );
    REG_INDI_SETPROP( m_indiP_remoteEmgain, m_camName, "emgain" );
    REG_INDI_SETPROP( m_indiP_remoteBlacklevel, m_camName, "blacklevel" );

    m_worker = std::thread( workerStart, this );
    state( stateCodes::READY );
    log<text_log>( "psfRefCtrl started" );
    return 0;
}

int psfRefCtrl::appLogic()
{
    if( m_busy.load() )
        state( stateCodes::OPERATING );
    else if( state() == stateCodes::OPERATING )
        state( stateCodes::READY );
    return 0;
}

int psfRefCtrl::appShutdown()
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

void psfRefCtrl::workerStart( psfRefCtrl *s )
{
    s->workerExec();
}

void psfRefCtrl::workerExec()
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

void psfRefCtrl::queueJob( Job j )
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
            log<text_log>( "psfRefCtrl busy; ignoring job", logPrio::LOG_WARNING );
            return;
        }
        m_pendingJob = j;
    }
    sem_post( &m_jobSem );
}

int psfRefCtrl::runJob( Job j )
{
    if( j == Job::TakeRef )
        return doTakeRef();
    return 0;
}

void psfRefCtrl::setStatus( const std::string &s )
{
    m_status = s;
    updateIfChanged( m_indiP_status, "current", m_status );
}

void psfRefCtrl::clearRequest( pcf::IndiProperty &p )
{
    updateSwitchIfChanged( p, "request", pcf::IndiElement::Off, INDI_IDLE );
}

int psfRefCtrl::openCamsci()
{
    if( m_camsciOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_camsci, m_shmCamInput.c_str() ) != IMAGESTREAMIO_SUCCESS )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_shmCamInput } );
    m_camsciOpen = true;
    return 0;
}

void psfRefCtrl::closeStreams()
{
    if( m_camsciOpen )
    {
        ImageStreamIO_closeIm( &m_camsci );
        m_camsciOpen = false;
    }
}

int psfRefCtrl::setFsmPos( double tip_nm, double tilt_nm )
{
    log<text_log>( "FSM -> tip=" + std::to_string( tip_nm ) + " nm, tilt=" +
                   std::to_string( tilt_nm ) + " nm (" + m_fsmName + ")" );

    pcf::IndiProperty ipX( pcf::IndiProperty::Number );
    ipX.setDevice( m_fsmName );
    ipX.setName( "val_1" );
    ipX.add( pcf::IndiElement( "target" ) );
    ipX["target"].setValue( tip_nm );
    if( sendNewProperty( ipX ) < 0 )
        return -1;

    pcf::IndiProperty ipY( pcf::IndiProperty::Number );
    ipY.setDevice( m_fsmName );
    ipY.setName( "val_2" );
    ipY.add( pcf::IndiElement( "target" ) );
    ipY["target"].setValue( tilt_nm );
    return sendNewProperty( ipY );
}

int psfRefCtrl::restoreFsmToRef()
{
    setStatus( "take_ref: restoring FSM" );
    return setFsmPos( m_fsmRefTip, m_fsmRefTilt );
}

std::string psfRefCtrl::formatDarkEntry( const lina::DarkLibraryEntry &e ) const
{
    std::ostringstream ss;
    ss << std::setprecision( 17 );
    ss << ( e.relpath.empty() ? "-" : e.relpath ) << " exptime=";
    if( std::isfinite( e.exptime ) )
        ss << e.exptime;
    else
        ss << "nan";
    ss << " shm_cam_input=" << ( e.shm_cam_input.empty() ? "-" : e.shm_cam_input )
       << " emgain=";
    if( std::isfinite( e.gain ) )
        ss << e.gain;
    else
        ss << "nan";
    ss << " blacklevel=";
    if( std::isfinite( e.blacklevel ) )
        ss << e.blacklevel;
    else
        ss << "nan";
    if( e.width > 0 && e.height > 0 )
        ss << " " << e.width << "x" << e.height;
    return ss.str();
}

std::string psfRefCtrl::pickDark( double target_exptime, const lina::DarkMatchFilter &filter,
                                  lina::DarkLibraryEntry *matched, double *match_err )
{
    const auto all = lina::load_dark_library_manifest( m_darkLibPath );
    const auto strict = lina::filter_dark_library_entries( all, filter );
    lina::DarkMatchFilter hard = filter;
    hard.gain = std::numeric_limits<double>::quiet_NaN();
    hard.blacklevel = std::numeric_limits<double>::quiet_NaN();
    const auto sized = lina::filter_dark_library_entries( all, hard );
    const bool relaxed = strict.empty() && !sized.empty();
    const auto &entries = relaxed ? sized : strict;
    if( entries.empty() )
        return {};

    std::size_t best = 0;
    double best_err = std::numeric_limits<double>::infinity();
    bool found = false;
    for( std::size_t i = 0; i < entries.size(); ++i )
    {
        if( !std::isfinite( entries[i].exptime ) || !std::isfinite( target_exptime ) )
            continue;
        const double err = std::fabs( entries[i].exptime - target_exptime );
        if( err < best_err )
        {
            best_err = err;
            best = i;
            found = true;
        }
    }
    if( !found )
        return {};

    if( matched )
        *matched = entries[best];
    if( match_err )
        *match_err = best_err;
    if( relaxed )
    {
        log<text_log>( "take_ref: live emgain/blacklevel did not match library; "
                       "using closest exptime for shm_cam_input (" +
                           formatDarkEntry( entries[best] ) + ")",
                       logPrio::LOG_WARNING );
    }

    const std::string &rel = entries[best].relpath;
    if( !rel.empty() && rel[0] == '/' )
        return rel;
    if( m_darkLibPath.empty() )
        return rel;
    if( m_darkLibPath.back() == '/' )
        return m_darkLibPath + rel;
    return m_darkLibPath + "/" + rel;
}

int psfRefCtrl::grabMean( unsigned nframes, unsigned wait_frames, std::vector<float> &out,
                          uint32_t &w, uint32_t &h )
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

    out.resize( npix );
    const float inv = 1.0f / static_cast<float>( nframes );
    for( size_t i = 0; i < npix; ++i )
        out[i] = static_cast<float>( acc[i] ) * inv;
    return 0;
}

int psfRefCtrl::ensureDir( const std::string &dir )
{
    if( dir.empty() )
        return -1;
    if( mkdir( dir.c_str(), 0755 ) != 0 && errno != EEXIST )
        return log<software_error, -1>( { __FILE__, __LINE__, "mkdir failed: " + dir } );
    return 0;
}

int psfRefCtrl::saveFitsF32( const std::string &path, const std::vector<float> &im, uint32_t w,
                             uint32_t h )
{
    try
    {
        mx::fits::fitsFile<float> ff;
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

int psfRefCtrl::writeConfigTxt( const std::string &path, const std::string &darkFile,
                                const std::string &refPsfFile,
                                const std::string &refPsfDarkSubFile, uint32_t w, uint32_t h,
                                double peakMax )
{
    std::ostringstream ss;
    ss << std::setprecision( 17 );
    ss << "camsci=" << m_shmCamInput << "\n";
    ss << "shm_cam_input=" << m_shmCamInput << "\n";
    ss << "fsm_name=" << m_fsmName << "\n";
    ss << "cam_name=" << m_camName << "\n";
    if( std::isfinite( m_remoteExp ) )
    {
        ss << "exptime=" << m_remoteExp << "\n";
        ss << "exposure=" << m_remoteExp << "\n";
        ss << "psf_exptime=" << m_remoteExp << "\n";
        ss << "cam_exp=" << m_remoteExp << "\n";
    }
    if( std::isfinite( m_remoteGain ) )
    {
        ss << "emgain=" << m_remoteGain << "\n";
        ss << "psf_gain=" << m_remoteGain << "\n";
        ss << "gain=" << m_remoteGain << "\n";
        ss << "cam_gain=" << m_remoteGain << "\n";
    }
    if( std::isfinite( m_remoteBlacklevel ) )
        ss << "blacklevel=" << m_remoteBlacklevel << "\n";
    ss << "psf_max_ref=" << peakMax << "\n";
    ss << "Imax_ref=" << peakMax << "\n";
    ss << "peak_dark_sub=" << peakMax << "\n";
    ss << "width=" << w << "\n";
    ss << "height=" << h << "\n";
    ss << "tip_nm=" << m_fsmPokeTip << "\n";
    ss << "tilt_nm=" << m_fsmPokeTilt << "\n";
    ss << "ref_tip_nm=" << m_fsmRefTip << "\n";
    ss << "ref_tilt_nm=" << m_fsmRefTilt << "\n";
    ss << "dark_lib_path=" << m_darkLibPath << "\n";
    ss << "npsf=" << m_nFrames << "\n";
    ss << "dark_file=" << darkFile << "\n";
    ss << "ref_psf_file=" << refPsfFile << "\n";
    ss << "ref_psf_dark_sub_file=" << refPsfDarkSubFile << "\n";

    std::ofstream out( path );
    if( !out )
        return log<software_error, -1>( { __FILE__, __LINE__, "failed to write " + path } );
    out << ss.str();
    return 0;
}

int psfRefCtrl::doTakeRef()
{
    setStatus( "take_ref: starting" );

    if( m_darkLibPath.empty() )
    {
        log<text_log>( "take_ref: dark_lib_path not set", logPrio::LOG_ERROR );
        return -1;
    }

    if( openCamsci() < 0 )
        return -1;
    if( ensureDir( m_dir ) < 0 )
        return -1;

    // Always return the FSM to fsm_ref_tip / fsm_ref_tilt when leaving take_ref,
    // including dark-match failures after the poke.
    struct RestoreFsm
    {
        psfRefCtrl *self;
        bool restored{ false };
        int restore()
        {
            if( restored )
                return 0;
            restored = true;
            return self->restoreFsmToRef();
        }
        ~RestoreFsm()
        {
            if( restore() < 0 )
                self->log<text_log>( "take_ref: failed to restore FSM to fsm_ref_tip/fsm_ref_tilt",
                                     logPrio::LOG_WARNING );
        }
    } restore{ this };

    const double pokeTip = m_fsmRefTip + m_fsmPokeTip;
    const double pokeTilt = m_fsmRefTilt + m_fsmPokeTilt;

    setStatus( "take_ref: parking FSM" );
    if( setFsmPos( m_fsmRefTip, m_fsmRefTilt ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    setStatus( "take_ref: poking FSM" );
    if( setFsmPos( pokeTip, pokeTilt ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    setStatus( "take_ref: picking dark from library" );
    const double target_exptime = std::isfinite( m_remoteExp ) ? m_remoteExp : 1.0;

    lina::DarkMatchFilter filter;
    filter.shm_cam_input = m_shmCamInput;
    if( m_camsciOpen )
    {
        filter.width = m_camsci.md->size[0];
        filter.height = ( m_camsci.md->naxis > 1 ) ? m_camsci.md->size[1] : 1;
    }
    if( std::isfinite( m_remoteGain ) )
        filter.gain = m_remoteGain;
    if( std::isfinite( m_remoteBlacklevel ) )
        filter.blacklevel = m_remoteBlacklevel;

    lina::DarkLibraryEntry matched;
    double match_err = 0.0;
    std::string dark_path = pickDark( target_exptime, filter, &matched, &match_err );
    if( dark_path.empty() )
    {
        const auto all = lina::load_dark_library_manifest( m_darkLibPath );
        const auto filt = lina::filter_dark_library_entries( all, filter );
        std::ostringstream ss;
        ss << std::setprecision( 17 );
        ss << "take_ref: no matching dark in library (" << m_darkLibPath
           << ", entries=" << all.size() << ", after_filter=" << filt.size()
           << ", shm_cam_input=" << m_shmCamInput << ", target_exptime=" << target_exptime
           << ", live_emgain=";
        if( std::isfinite( m_remoteGain ) )
            ss << m_remoteGain;
        else
            ss << "nan";
        ss << ", live_blacklevel=";
        if( std::isfinite( m_remoteBlacklevel ) )
            ss << m_remoteBlacklevel;
        else
            ss << "nan";
        ss << ")";
        if( !all.empty() )
            ss << "; first entry: " << formatDarkEntry( all.front() );
        log<text_log>( ss.str(), logPrio::LOG_ERROR );
        return -1;
    }

    log<text_log>( "take_ref: using dark " + dark_path + " (" + formatDarkEntry( matched ) +
                   ", err=" + std::to_string( match_err ) + " s)" );

    setStatus( "take_ref: loading dark" );
    lina::Array2D<double> dark_arr;
    try
    {
        dark_arr = lina::load_matrix( dark_path );
    }
    catch( const std::exception &e )
    {
        log<text_log>( std::string( "take_ref: failed to load dark: " ) + e.what(),
                       logPrio::LOG_ERROR );
        return -1;
    }

    std::vector<float> dark_f32( dark_arr.data(), dark_arr.data() + dark_arr.size() );
    // Array2D rows/cols match ImageStreamIO size[0]/size[1] (iefc convention).
    if( saveFitsF32( m_dir + "/dark_avg.fits", dark_f32,
                     static_cast<uint32_t>( dark_arr.rows() ),
                     static_cast<uint32_t>( dark_arr.cols() ) ) < 0 )
        return -1;

    setStatus( "take_ref: grabbing frames" );
    std::vector<float> psf_avg;
    uint32_t w = 0, h = 0;
    if( grabMean( m_nFrames, m_camNFrameDelay, psf_avg, w, h ) < 0 )
        return -1;

    bool saturated = false;
    for( auto val : psf_avg )
    {
        if( val >= m_satThresh )
        {
            saturated = true;
            break;
        }
    }
    if( saturated )
        log<text_log>( "take_ref: WARNING - saturation detected (>= " +
                           std::to_string( m_satThresh ) + ")",
                       logPrio::LOG_WARNING );

    setStatus( "take_ref: dark-subtracting" );
    if( dark_f32.size() != psf_avg.size() )
    {
        log<text_log>( "take_ref: dark size mismatch", logPrio::LOG_ERROR );
        return -1;
    }

    std::vector<float> psf_dark_sub( psf_avg.size() );
    double peak_max = -1e30;
    for( size_t i = 0; i < psf_avg.size(); ++i )
    {
        psf_dark_sub[i] = psf_avg[i] - dark_f32[i];
        if( psf_dark_sub[i] > peak_max )
            peak_max = psf_dark_sub[i];
    }

    setStatus( "take_ref: saving FITS" );
    if( saveFitsF32( m_dir + "/ref_psf_avg.fits", psf_avg, w, h ) < 0 )
        return -1;
    if( saveFitsF32( m_dir + "/ref_psf_dark_sub.fits", psf_dark_sub, w, h ) < 0 )
        return -1;

    setStatus( "take_ref: writing config.txt" );
    if( writeConfigTxt( m_dir + "/config.txt", "dark_avg.fits", "ref_psf_avg.fits",
                        "ref_psf_dark_sub.fits", w, h, peak_max ) < 0 )
        return -1;

    setStatus( "take_ref: updating max_ref" );
    m_maxRef = peak_max;
    updateIfChanged( m_indiP_maxRef, "current", m_maxRef );
    updateIfChanged( m_indiP_maxRef, "target", m_maxRef );

    if( restore.restore() < 0 )
        return -1;

    setStatus( "take_ref: done" );
    log<text_log>( "take_ref: completed (peak=" + std::to_string( peak_max ) +
                   "), package in " + m_dir );
    return 0;
}

// --- INDI callbacks (soft params) ---

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_shmCamInput )( const pcf::IndiProperty &ipRecv )
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

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_camName )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camName, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_camName, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_camName )
        log<text_log>( "cam_name -> " + target + " (restart app to rebind SET subscription)",
                       logPrio::LOG_WARNING );
    m_camName = target;
    updateIfChanged( m_indiP_camName, "current", m_camName );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_fsmName )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmName, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_fsmName, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmName = target;
    updateIfChanged( m_indiP_fsmName, "current", m_fsmName );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_darkLibPath )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkLibPath, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_darkLibPath, target, ipRecv, false ) < 0 )
        return -1;
    m_darkLibPath = target;
    updateIfChanged( m_indiP_darkLibPath, "current", m_darkLibPath );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_dir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_dir, target, ipRecv, false ) < 0 )
        return -1;
    m_dir = target;
    updateIfChanged( m_indiP_dir, "current", m_dir );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_nFrames )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nFrames, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nFrames, target, ipRecv, false ) < 0 )
        return -1;
    m_nFrames = target;
    updateIfChanged( m_indiP_nFrames, "current", m_nFrames );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_fsmPokeTip )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmPokeTip, ipRecv );
    double target;
    if( indiTargetUpdate( m_indiP_fsmPokeTip, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmPokeTip = target;
    updateIfChanged( m_indiP_fsmPokeTip, "current", m_fsmPokeTip );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_fsmPokeTilt )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmPokeTilt, ipRecv );
    double target;
    if( indiTargetUpdate( m_indiP_fsmPokeTilt, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmPokeTilt = target;
    updateIfChanged( m_indiP_fsmPokeTilt, "current", m_fsmPokeTilt );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_fsmRefTip )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmRefTip, ipRecv );
    double target;
    if( indiTargetUpdate( m_indiP_fsmRefTip, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmRefTip = target;
    updateIfChanged( m_indiP_fsmRefTip, "current", m_fsmRefTip );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_fsmRefTilt )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmRefTilt, ipRecv );
    double target;
    if( indiTargetUpdate( m_indiP_fsmRefTilt, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmRefTilt = target;
    updateIfChanged( m_indiP_fsmRefTilt, "current", m_fsmRefTilt );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_maxRef )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_maxRef, ipRecv );
    double target;
    if( indiTargetUpdate( m_indiP_maxRef, target, ipRecv, false ) < 0 )
        return -1;
    m_maxRef = target;
    updateIfChanged( m_indiP_maxRef, "current", m_maxRef );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_satThresh )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_satThresh, ipRecv );
    double target;
    if( indiTargetUpdate( m_indiP_satThresh, target, ipRecv, false ) < 0 )
        return -1;
    m_satThresh = target;
    updateIfChanged( m_indiP_satThresh, "current", m_satThresh );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_settleS )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_settleS, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_settleS, target, ipRecv, false ) < 0 )
        return -1;
    m_settle_s = target;
    updateIfChanged( m_indiP_settleS, "current", m_settle_s );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_camNFrameDelay )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camNFrameDelay, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_camNFrameDelay, target, ipRecv, false ) < 0 )
        return -1;
    m_camNFrameDelay = target;
    updateIfChanged( m_indiP_camNFrameDelay, "current", m_camNFrameDelay );
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_takeRef )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_takeRef, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_takeRef, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::TakeRef );
        clearRequest( m_indiP_takeRef );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( psfRefCtrl, m_indiP_stop )( const pcf::IndiProperty &ipRecv )
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

INDI_SETCALLBACK_DEFN( psfRefCtrl, m_indiP_remoteExptime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteExptime, ipRecv );
    if( !ipRecv.find( "current" ) )
        return 0;
    const std::string s = ipRecv["current"].getValue();
    if( s.empty() )
        return 0;
    char *end = nullptr;
    const double v = std::strtod( s.c_str(), &end );
    if( end != s.c_str() && std::isfinite( v ) )
        m_remoteExp = v;
    return 0;
}

INDI_SETCALLBACK_DEFN( psfRefCtrl, m_indiP_remoteEmgain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteEmgain, ipRecv );
    if( !ipRecv.find( "current" ) )
        return 0;
    const std::string s = ipRecv["current"].getValue();
    if( s.empty() )
        return 0;
    char *end = nullptr;
    const double v = std::strtod( s.c_str(), &end );
    if( end != s.c_str() && std::isfinite( v ) )
        m_remoteGain = v;
    return 0;
}

INDI_SETCALLBACK_DEFN( psfRefCtrl, m_indiP_remoteBlacklevel )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteBlacklevel, ipRecv );
    if( !ipRecv.find( "current" ) )
        return 0;
    const std::string s = ipRecv["current"].getValue();
    if( s.empty() )
        return 0;
    char *end = nullptr;
    const double v = std::strtod( s.c_str(), &end );
    if( end != s.c_str() && std::isfinite( v ) )
        m_remoteBlacklevel = v;
    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // psfRefCtrl_hpp
