/** \file iefcCtrl.hpp
  * \brief MagAO-X INDI front-end for Stream IEFC (ref PSF, dark library, calibrate, run).
  *
  * Mirrors the standalone lina tools:
  *   iefc_ref_psf_init  (ref-psf + dark-library modes)
  *   iefc_camsci_once   (calibrate + run)
  *
  * Shared INDI numbers (nFrames, waitFrames, delay_s, exptime, …) are reused across
  * all actions. Request switches trigger one-shot worker jobs.
  *
  * Ref-PSF / dark-library run natively against milk shmims. Calibrate / run currently
  * invoke the configured iefc_camsci_once binary with CLI args built from INDI.
  *
  * \ingroup iefcCtrl_files
  */

#ifndef iefcCtrl_hpp
#define iefcCtrl_hpp

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ImageStreamIO.h>
#include <ImageStruct.h>

#include <mx/improc/eigenImage.hpp>
#include <mx/ioutils/fits/fitsFile.hpp>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

/** \defgroup iefcCtrl
  * \brief MagAO-X Stream IEFC controller
  *
  * <a href="../handbook/operating/software/apps/iefcCtrl.html">Application Documentation</a>
  *
  * \ingroup apps
  */

/** \defgroup iefcCtrl_files
  * \ingroup iefcCtrl
  */

namespace MagAOX
{
namespace app
{

/// MagAO-X Stream IEFC controller (INDI front-end for ref-PSF / calibrate / run).
/** \ingroup iefcCtrl
  */
class iefcCtrl : public MagAOXApp<true>
{
  public:
    enum class Job : int
    {
        Idle = 0,
        RefPsf,
        DarkLibrary,
        Calibrate,
        Run,
        Stop
    };

  protected:
    /** \name Configurable defaults (config file / CLI)
      *@{
      */
    std::string m_camsciName{ "camsci" };
    std::string m_dmChan{ "dm01disp07" };
    std::string m_fsmChan{ "dm00disp01" };
    std::string m_shutterName{ "camscishutter" };
    std::string m_exptimeName{ "camsciexptime" };
    std::string m_gainName{ "camscigain" };

    std::string m_outdir{ "./iefc_out" };
    std::string m_setupdir{ "./ref_psf" };
    std::string m_caldir{ "./cal_a" };

    /// Path to iefc_camsci_once (calibrate / run). Empty → skip / error.
    std::string m_camsciOnceBin;

    unsigned m_nFrames{ 5 };
    unsigned m_waitFrames{ 1 };
    float m_delay_s{ 0.05f };

    float m_fsmAmp_nm{ 1000.0f };
    float m_fsmTip_nm{ 1000.0f };
    float m_fsmTilt_nm{ 1000.0f };
    float m_fsmRefTip_nm{ 0.0f };
    float m_fsmRefTilt_nm{ 0.0f };
    float m_fsmRefPiston_nm{ 0.0f };
    unsigned m_nDark{ 20 };
    unsigned m_nPsf{ 20 };
    float m_settle_s{ 0.5f };

    std::string m_exptimesCsv{ "0.5,1,2,5" };
    float m_exptime{ 1.0f };
    bool m_setExptime{ false };

    float m_regCond{ -2.5f };
    float m_probeAmp{ 1e-9f };
    float m_calibProbeAmp{ 5e-9f };
    float m_calibAmp{ 2e-9f };
    unsigned m_iters{ 3 };
    float m_loopGain{ 1.0f };
    float m_leakage{ 0.0f };
    ///@}

    /** \name Worker thread
      *@{
      */
    std::thread m_worker;
    std::atomic<bool> m_workerShutdown{ false };
    std::atomic<bool> m_stopRequested{ false };
    sem_t m_jobSem;
    std::mutex m_jobMutex;
    Job m_pendingJob{ Job::Idle };
    std::atomic<int> m_busy{ 0 }; ///< 0 idle, 1 busy
    std::string m_status{ "idle" };
    ///@}

    /** \name Open shmims (opened on demand per job)
      *@{
      */
    IMAGE m_camsci{};
    IMAGE m_dm{};
    IMAGE m_fsm{};
    IMAGE m_shutter{};
    IMAGE m_exptimeShm{};
    bool m_camsciOpen{ false };
    bool m_dmOpen{ false };
    bool m_fsmOpen{ false };
    bool m_shutterOpen{ false };
    bool m_exptimeOpen{ false };
    int m_camsciSem{ -1 };
    ///@}

    /** \name INDI — shared
      *@{
      */
    pcf::IndiProperty m_indiP_nFrames;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_nFrames);

    pcf::IndiProperty m_indiP_waitFrames;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_waitFrames);

    pcf::IndiProperty m_indiP_delay_s;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_delay_s);

    pcf::IndiProperty m_indiP_exptime;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_exptime);

    pcf::IndiProperty m_indiP_outdir;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_outdir);

    pcf::IndiProperty m_indiP_setupdir;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_setupdir);

    pcf::IndiProperty m_indiP_caldir;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_caldir);
    ///@}

    /** \name INDI — ref PSF / FSM
      *@{
      */
    pcf::IndiProperty m_indiP_fsmAmp_nm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_fsmAmp_nm);

    pcf::IndiProperty m_indiP_fsmTip_nm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_fsmTip_nm);

    pcf::IndiProperty m_indiP_fsmTilt_nm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_fsmTilt_nm);

    pcf::IndiProperty m_indiP_fsmRefTip_nm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_fsmRefTip_nm);

    pcf::IndiProperty m_indiP_fsmRefTilt_nm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_fsmRefTilt_nm);

    pcf::IndiProperty m_indiP_nDark;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_nDark);

    pcf::IndiProperty m_indiP_nPsf;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_nPsf);

    pcf::IndiProperty m_indiP_exptimes;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_exptimes);
    ///@}

    /** \name INDI — calibrate / run
      *@{
      */
    pcf::IndiProperty m_indiP_regCond;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_regCond);

    pcf::IndiProperty m_indiP_probeAmp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_probeAmp);

    pcf::IndiProperty m_indiP_iters;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_iters);

    pcf::IndiProperty m_indiP_loopGain;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_loopGain);

    pcf::IndiProperty m_indiP_leakage;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_leakage);
    ///@}

    /** \name INDI — requests + status
      *@{
      */
    pcf::IndiProperty m_indiP_doRefPsf;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_doRefPsf);

    pcf::IndiProperty m_indiP_doDarkLibrary;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_doDarkLibrary);

    pcf::IndiProperty m_indiP_doCalibrate;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_doCalibrate);

    pcf::IndiProperty m_indiP_doRun;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_doRun);

    pcf::IndiProperty m_indiP_stop;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_stop);

    pcf::IndiProperty m_indiP_status;   ///< RO text
    pcf::IndiProperty m_indiP_Imax_ref; ///< RO number
    pcf::IndiProperty m_indiP_contrast; ///< RO number
    ///@}

  public:
    iefcCtrl();

    virtual void setupConfig();
    virtual void loadConfig();
    virtual int appStartup();
    virtual int appLogic();
    virtual int appShutdown();

  protected:
    static void workerStart( iefcCtrl *s );
    void workerExec();

    void queueJob( Job j );
    int runJob( Job j );

    int openCamsci();
    int openDm();
    int openFsm();
    int openShutter();
    int openExptime();
    void closeStreams();

    int writeScalar( IMAGE &im, double value );
    int writeFsmTipTiltPiston( double tip_m, double tilt_m, double piston_m );
    int grabMeanCamsci( unsigned nframes, unsigned wait_frames, std::vector<float> &out,
                        uint32_t &w, uint32_t &h );

    int ensureDir( const std::string &dir );
    int saveFitsF32( const std::string &path, const std::vector<float> &im, uint32_t w,
                     uint32_t h );
    int writeConfigTxt( const std::string &path, const std::string &body );

    int doRefPsf();
    int doDarkLibrary();
    int doCalibrate();
    int doRun();

    int spawnCamsciOnce( const std::vector<std::string> &args );
    void setStatus( const std::string &s );
    void clearRequest( pcf::IndiProperty &p );
};

iefcCtrl::iefcCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
}

void iefcCtrl::setupConfig()
{
    config.add( "iefc.camsci", "", "iefc.camsci", argType::Required, "iefc", "camsci", false,
                "string", "Camera shmim name (default camsci)." );
    config.add( "iefc.dmChannel", "", "iefc.dmChannel", argType::Required, "iefc", "dmChannel",
                false, "string", "IEFC DM channel (default dm01disp07)." );
    config.add( "iefc.fsmChannel", "", "iefc.fsmChannel", argType::Required, "iefc", "fsmChannel",
                false, "string", "FSM DMcomb channel (default dm00disp01)." );
    config.add( "iefc.shutter", "", "iefc.shutter", argType::Required, "iefc", "shutter", false,
                "string", "Shutter scalar shmim (default camscishutter)." );
    config.add( "iefc.exptime", "", "iefc.exptime", argType::Required, "iefc", "exptime", false,
                "string", "Exposure-time scalar shmim (default camsciexptime)." );
    config.add( "iefc.outdir", "", "iefc.outdir", argType::Required, "iefc", "outdir", false,
                "string", "Default output directory for ref-PSF / darks." );
    config.add( "iefc.setupdir", "", "iefc.setupdir", argType::Required, "iefc", "setupdir", false,
                "string", "Ref-PSF / dark-library directory used by calibrate/run." );
    config.add( "iefc.caldir", "", "iefc.caldir", argType::Required, "iefc", "caldir", false,
                "string", "Calibration package directory (response/control)." );
    config.add( "iefc.camsciOnceBin", "", "iefc.camsciOnceBin", argType::Required, "iefc",
                "camsciOnceBin", false, "string",
                "Path to iefc_camsci_once binary for calibrate/run." );
    config.add( "iefc.nFrames", "", "iefc.nFrames", argType::Required, "iefc", "nFrames", false,
                "unsigned", "Frames to average per grab (shared)." );
    config.add( "iefc.waitFrames", "", "iefc.waitFrames", argType::Required, "iefc", "waitFrames",
                false, "unsigned", "Frames to skip after DM write (shared)." );
    config.add( "iefc.delay_s", "", "iefc.delay_s", argType::Required, "iefc", "delay_s", false,
                "float", "Wall-clock settle after DM write [s]." );
    config.add( "iefc.fsmAmp_nm", "", "iefc.fsmAmp_nm", argType::Required, "iefc", "fsmAmp_nm",
                false, "float", "Default tip+tilt poke amplitude [nm]." );
    config.add( "iefc.nDark", "", "iefc.nDark", argType::Required, "iefc", "nDark", false,
                "unsigned", "Dark frames for ref-PSF / dark-library." );
    config.add( "iefc.nPsf", "", "iefc.nPsf", argType::Required, "iefc", "nPsf", false, "unsigned",
                "PSF frames for ref-PSF." );
    config.add( "iefc.exptimes", "", "iefc.exptimes", argType::Required, "iefc", "exptimes", false,
                "string", "CSV exposure times for dark-library." );
    config.add( "iefc.regCond", "", "iefc.regCond", argType::Required, "iefc", "regCond", false,
                "float", "beta_reg regularization for run." );
    config.add( "iefc.probeAmp", "", "iefc.probeAmp", argType::Required, "iefc", "probeAmp", false,
                "float", "Run probe amplitude [m]." );
    config.add( "iefc.iters", "", "iefc.iters", argType::Required, "iefc", "iters", false,
                "unsigned", "Closed-loop iterations per run." );
}

void iefcCtrl::loadConfig()
{
    config( m_camsciName, "iefc.camsci" );
    config( m_dmChan, "iefc.dmChannel" );
    config( m_fsmChan, "iefc.fsmChannel" );
    config( m_shutterName, "iefc.shutter" );
    config( m_exptimeName, "iefc.exptime" );
    config( m_outdir, "iefc.outdir" );
    config( m_setupdir, "iefc.setupdir" );
    config( m_caldir, "iefc.caldir" );
    config( m_camsciOnceBin, "iefc.camsciOnceBin" );
    config( m_nFrames, "iefc.nFrames" );
    config( m_waitFrames, "iefc.waitFrames" );
    config( m_delay_s, "iefc.delay_s" );
    config( m_fsmAmp_nm, "iefc.fsmAmp_nm" );
    m_fsmTip_nm = m_fsmAmp_nm;
    m_fsmTilt_nm = m_fsmAmp_nm;
    config( m_nDark, "iefc.nDark" );
    config( m_nPsf, "iefc.nPsf" );
    config( m_exptimesCsv, "iefc.exptimes" );
    config( m_regCond, "iefc.regCond" );
    config( m_probeAmp, "iefc.probeAmp" );
    config( m_iters, "iefc.iters" );
}

int iefcCtrl::appStartup()
{
    if( sem_init( &m_jobSem, 0, 0 ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "sem_init failed" } );
    }

    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_nFrames, "nFrames", 1, 10000, 1, "%u", "Frames to average", "shared" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_waitFrames, "waitFrames", 0, 1000, 1, "%u", "Frames to skip after DM", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_delay_s, "delay_s", 0, 10, 0.01, "%0.3f", "DM settle [s]", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_exptime, "exptime", 0, 1000, 0.1, "%0.3f", "camsciexptime", "shared" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_outdir, "outdir", "Output directory", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_setupdir, "setupdir", "Ref-PSF / dark library dir", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_caldir, "caldir", "Calibration package dir", "paths" );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmAmp_nm, "fsmAmp_nm", -1e5, 1e5, 1, "%0.1f", "Tip+tilt poke [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmTip_nm, "fsmTip_nm", -1e5, 1e5, 1, "%0.1f", "Tip poke [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmTilt_nm, "fsmTilt_nm", -1e5, 1e5, 1, "%0.1f", "Tilt poke [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmRefTip_nm, "fsmRefTip_nm", -1e5, 1e5, 1, "%0.1f", "FSM home tip [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_fsmRefTilt_nm, "fsmRefTilt_nm", -1e5, 1e5, 1, "%0.1f", "FSM home tilt [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_nDark, "nDark", 1, 10000, 1, "%u", "Dark frames", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_nPsf, "nPsf", 1, 10000, 1, "%u", "PSF frames", "refPsf" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_exptimes, "exptimes", "Dark-library CSV exposures", "refPsf" );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_regCond, "regCond", -20, 0, 0.1, "%0.2f", "beta_reg", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_probeAmp, "probeAmp", 0, 1e-6, 1e-10, "%0.3e", "Run probe amp [m]", "run" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_iters, "iters", 1, 1000, 1, "%u", "Run iterations", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_loopGain, "loopGain", 0, 2, 0.05, "%0.2f", "Loop gain", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_leakage, "leakage", 0, 1, 0.01, "%0.2f", "Leakage", "run" );

    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doRefPsf, "doRefPsf" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doDarkLibrary, "doDarkLibrary" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doCalibrate, "doCalibrate" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doRun, "doRun" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_stop, "stop" );

    REG_INDI_NEWPROP_NOCB( m_indiP_status, "status", pcf::IndiProperty::Text );
    m_indiP_status.add( pcf::IndiElement( "current" ) );
    m_indiP_status["current"].set( m_status );

    REG_INDI_NEWPROP_NOCB( m_indiP_Imax_ref, "Imax_ref", pcf::IndiProperty::Number );
    m_indiP_Imax_ref.add( pcf::IndiElement( "current" ) );
    m_indiP_Imax_ref["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_contrast, "contrast", pcf::IndiProperty::Number );
    m_indiP_contrast.add( pcf::IndiElement( "current" ) );
    m_indiP_contrast["current"].set( 0.0 );

    // Seed current/target from config
    updateIfChanged( m_indiP_nFrames, "current", m_nFrames );
    updateIfChanged( m_indiP_nFrames, "target", m_nFrames );
    updateIfChanged( m_indiP_waitFrames, "current", m_waitFrames );
    updateIfChanged( m_indiP_waitFrames, "target", m_waitFrames );
    updateIfChanged( m_indiP_delay_s, "current", m_delay_s );
    updateIfChanged( m_indiP_delay_s, "target", m_delay_s );
    updateIfChanged( m_indiP_exptime, "current", m_exptime );
    updateIfChanged( m_indiP_exptime, "target", m_exptime );
    updateIfChanged( m_indiP_outdir, "current", m_outdir );
    updateIfChanged( m_indiP_outdir, "target", m_outdir );
    updateIfChanged( m_indiP_setupdir, "current", m_setupdir );
    updateIfChanged( m_indiP_setupdir, "target", m_setupdir );
    updateIfChanged( m_indiP_caldir, "current", m_caldir );
    updateIfChanged( m_indiP_caldir, "target", m_caldir );
    updateIfChanged( m_indiP_fsmAmp_nm, "current", m_fsmAmp_nm );
    updateIfChanged( m_indiP_fsmAmp_nm, "target", m_fsmAmp_nm );
    updateIfChanged( m_indiP_fsmTip_nm, "current", m_fsmTip_nm );
    updateIfChanged( m_indiP_fsmTip_nm, "target", m_fsmTip_nm );
    updateIfChanged( m_indiP_fsmTilt_nm, "current", m_fsmTilt_nm );
    updateIfChanged( m_indiP_fsmTilt_nm, "target", m_fsmTilt_nm );
    updateIfChanged( m_indiP_fsmRefTip_nm, "current", m_fsmRefTip_nm );
    updateIfChanged( m_indiP_fsmRefTip_nm, "target", m_fsmRefTip_nm );
    updateIfChanged( m_indiP_fsmRefTilt_nm, "current", m_fsmRefTilt_nm );
    updateIfChanged( m_indiP_fsmRefTilt_nm, "target", m_fsmRefTilt_nm );
    updateIfChanged( m_indiP_nDark, "current", m_nDark );
    updateIfChanged( m_indiP_nDark, "target", m_nDark );
    updateIfChanged( m_indiP_nPsf, "current", m_nPsf );
    updateIfChanged( m_indiP_nPsf, "target", m_nPsf );
    updateIfChanged( m_indiP_exptimes, "current", m_exptimesCsv );
    updateIfChanged( m_indiP_exptimes, "target", m_exptimesCsv );
    updateIfChanged( m_indiP_regCond, "current", m_regCond );
    updateIfChanged( m_indiP_regCond, "target", m_regCond );
    updateIfChanged( m_indiP_probeAmp, "current", m_probeAmp );
    updateIfChanged( m_indiP_probeAmp, "target", m_probeAmp );
    updateIfChanged( m_indiP_iters, "current", m_iters );
    updateIfChanged( m_indiP_iters, "target", m_iters );
    updateIfChanged( m_indiP_loopGain, "current", m_loopGain );
    updateIfChanged( m_indiP_loopGain, "target", m_loopGain );
    updateIfChanged( m_indiP_leakage, "current", m_leakage );
    updateIfChanged( m_indiP_leakage, "target", m_leakage );

    m_worker = std::thread( workerStart, this );

    log<text_log>( "iefcCtrl started (refPSF/darkLibrary native; calibrate/run via "
                   "iefc_camsci_once)" );
    return 0;
}

int iefcCtrl::appLogic()
{
    if( m_busy.load() == 0 )
    {
        state( stateCodes::READY );
    }
    else
    {
        state( stateCodes::OPERATING );
    }
    return 0;
}

int iefcCtrl::appShutdown()
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

void iefcCtrl::workerStart( iefcCtrl *s )
{
    s->workerExec();
}

void iefcCtrl::workerExec()
{
    while( !m_workerShutdown.load() )
    {
        timespec ts{};
        clock_gettime( CLOCK_REALTIME, &ts );
        ts.tv_sec += 1;
        sem_timedwait( &m_jobSem, &ts );
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
        runJob( job );
        m_busy = 0;
        setStatus( "idle" );
    }
}

void iefcCtrl::queueJob( Job j )
{
    {
        std::lock_guard<std::mutex> lock( m_jobMutex );
        if( m_busy.load() && j != Job::Stop )
        {
            log<text_log>( "iefcCtrl busy; ignoring new job (use stop first)",
                           logPrio::LOG_WARNING );
            return;
        }
        if( j == Job::Stop )
        {
            m_stopRequested = true;
        }
        m_pendingJob = j;
    }
    sem_post( &m_jobSem );
}

int iefcCtrl::runJob( Job j )
{
    switch( j )
    {
        case Job::RefPsf:
            return doRefPsf();
        case Job::DarkLibrary:
            return doDarkLibrary();
        case Job::Calibrate:
            return doCalibrate();
        case Job::Run:
            return doRun();
        case Job::Stop:
            setStatus( "stop requested" );
            return 0;
        default:
            return 0;
    }
}

void iefcCtrl::setStatus( const std::string &s )
{
    m_status = s;
    updateIfChanged( m_indiP_status, "current", m_status );
}

void iefcCtrl::clearRequest( pcf::IndiProperty &p )
{
    updateSwitchIfChanged( p, "request", pcf::IndiElement::Off, INDI_IDLE );
}

int iefcCtrl::openCamsci()
{
    if( m_camsciOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_camsci, m_camsciName.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_camsciName } );
    }
    m_camsciOpen = true;
    m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
    return 0;
}

int iefcCtrl::openDm()
{
    if( m_dmOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_dm, m_dmChan.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_dmChan } );
    }
    m_dmOpen = true;
    return 0;
}

int iefcCtrl::openFsm()
{
    if( m_fsmOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_fsm, m_fsmChan.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_fsmChan } );
    }
    m_fsmOpen = true;
    return 0;
}

int iefcCtrl::openShutter()
{
    if( m_shutterOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_shutter, m_shutterName.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_shutterName } );
    }
    m_shutterOpen = true;
    return 0;
}

int iefcCtrl::openExptime()
{
    if( m_exptimeOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_exptimeShm, m_exptimeName.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_exptimeName } );
    }
    m_exptimeOpen = true;
    return 0;
}

void iefcCtrl::closeStreams()
{
    if( m_camsciOpen )
    {
        ImageStreamIO_closeIm( &m_camsci );
        m_camsciOpen = false;
    }
    if( m_dmOpen )
    {
        ImageStreamIO_closeIm( &m_dm );
        m_dmOpen = false;
    }
    if( m_fsmOpen )
    {
        ImageStreamIO_closeIm( &m_fsm );
        m_fsmOpen = false;
    }
    if( m_shutterOpen )
    {
        ImageStreamIO_closeIm( &m_shutter );
        m_shutterOpen = false;
    }
    if( m_exptimeOpen )
    {
        ImageStreamIO_closeIm( &m_exptimeShm );
        m_exptimeOpen = false;
    }
}

int iefcCtrl::writeScalar( IMAGE &im, double value )
{
    im.md->write = 1;
    if( im.md->datatype == _DATATYPE_FLOAT )
    {
        ( (float *)im.array.raw )[0] = static_cast<float>( value );
    }
    else if( im.md->datatype == _DATATYPE_DOUBLE )
    {
        ( (double *)im.array.raw )[0] = value;
    }
    else
    {
        im.md->write = 0;
        return log<software_error, -1>( { __FILE__, __LINE__, "unsupported scalar datatype" } );
    }
    im.md->cnt0++;
    im.md->write = 0;
    ImageStreamIO_sempost( &im, -1 );
    return 0;
}

int iefcCtrl::writeFsmTipTiltPiston( double tip_m, double tilt_m, double piston_m )
{
    // Layout tip,tilt,piston at indices 0,1,2 (run_camsci_cpp convention).
    if( !m_fsmOpen )
        return -1;
    const uint32_t n = m_fsm.md->size[0] * ( m_fsm.md->naxis > 1 ? m_fsm.md->size[1] : 1 );
    if( n < 3 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "FSM channel size < 3" } );
    }

    m_fsm.md->write = 1;
    if( m_fsm.md->datatype == _DATATYPE_FLOAT )
    {
        float *p = (float *)m_fsm.array.raw;
        for( uint32_t i = 0; i < n; ++i )
            p[i] = 0.0f;
        p[0] = static_cast<float>( tip_m );
        p[1] = static_cast<float>( tilt_m );
        p[2] = static_cast<float>( piston_m );
    }
    else if( m_fsm.md->datatype == _DATATYPE_DOUBLE )
    {
        double *p = (double *)m_fsm.array.raw;
        for( uint32_t i = 0; i < n; ++i )
            p[i] = 0.0;
        p[0] = tip_m;
        p[1] = tilt_m;
        p[2] = piston_m;
    }
    else
    {
        m_fsm.md->write = 0;
        return log<software_error, -1>( { __FILE__, __LINE__, "unsupported FSM datatype" } );
    }
    m_fsm.md->cnt0++;
    m_fsm.md->write = 0;
    ImageStreamIO_sempost( &m_fsm, -1 );
    return 0;
}

int iefcCtrl::grabMeanCamsci( unsigned nframes, unsigned wait_frames, std::vector<float> &out,
                              uint32_t &w, uint32_t &h )
{
    if( !m_camsciOpen )
        return -1;
    if( nframes == 0 )
        return -1;

    w = m_camsci.md->size[0];
    h = ( m_camsci.md->naxis > 1 ) ? m_camsci.md->size[1] : 1;
    const size_t npix = static_cast<size_t>( w ) * static_cast<size_t>( h );
    out.assign( npix, 0.0f );

    if( m_camsciSem < 0 )
    {
        m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
    }
    if( m_camsciSem < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "no camsci semaphore" } );
    }

    ImageStreamIO_semflush( &m_camsci, m_camsciSem );
    const uint64_t cnt0_min = m_camsci.md->cnt0 + wait_frames;

    unsigned collected = 0;
    while( collected < nframes )
    {
        if( m_stopRequested.load() )
            return -1;

        timespec ts{};
        clock_gettime( CLOCK_REALTIME, &ts );
        ts.tv_sec += 30;
        if( ImageStreamIO_semtimedwait( &m_camsci, m_camsciSem, &ts ) != 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "timeout waiting for camsci frame" } );
        }
        if( m_camsci.md->cnt0 < cnt0_min )
            continue;

        if( m_camsci.md->datatype == _DATATYPE_FLOAT )
        {
            const float *src = (const float *)m_camsci.array.raw;
            // For circular buffer streams, last slice is at cnt1
            if( m_camsci.md->naxis >= 3 )
            {
                const uint64_t slice = m_camsci.md->cnt1 % m_camsci.md->size[2];
                src = (const float *)m_camsci.array.raw + slice * npix;
            }
            for( size_t i = 0; i < npix; ++i )
                out[i] += src[i];
        }
        else if( m_camsci.md->datatype == _DATATYPE_DOUBLE )
        {
            const double *src = (const double *)m_camsci.array.raw;
            if( m_camsci.md->naxis >= 3 )
            {
                const uint64_t slice = m_camsci.md->cnt1 % m_camsci.md->size[2];
                src = (const double *)m_camsci.array.raw + slice * npix;
            }
            for( size_t i = 0; i < npix; ++i )
                out[i] += static_cast<float>( src[i] );
        }
        else if( m_camsci.md->datatype == _DATATYPE_UINT16 )
        {
            const uint16_t *src = (const uint16_t *)m_camsci.array.raw;
            if( m_camsci.md->naxis >= 3 )
            {
                const uint64_t slice = m_camsci.md->cnt1 % m_camsci.md->size[2];
                src = (const uint16_t *)m_camsci.array.raw + slice * npix;
            }
            for( size_t i = 0; i < npix; ++i )
                out[i] += static_cast<float>( src[i] );
        }
        else
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "unsupported camsci datatype" } );
        }
        ++collected;
    }

    const float inv = 1.0f / static_cast<float>( nframes );
    for( size_t i = 0; i < npix; ++i )
        out[i] *= inv;
    return 0;
}

int iefcCtrl::ensureDir( const std::string &dir )
{
    if( dir.empty() )
        return -1;
    if( mkdir( dir.c_str(), 0755 ) != 0 && errno != EEXIST )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "mkdir failed: " + dir } );
    }
    return 0;
}

int iefcCtrl::saveFitsF32( const std::string &path, const std::vector<float> &im, uint32_t w,
                           uint32_t h )
{
    // Minimal FITS writer via mx if available; else raw dump with .fits extension note.
    // Prefer mx fitsFile when linked through MagAOX.
    try
    {
        mx::improc::eigenImage<float> eig;
        eig.resize( static_cast<int>( w ), static_cast<int>( h ) );
        for( uint32_t r = 0; r < h; ++r )
        {
            for( uint32_t c = 0; c < w; ++c )
            {
                eig( static_cast<int>( c ), static_cast<int>( r ) ) =
                    im[static_cast<size_t>( r ) * w + c];
            }
        }
        mx::fits::fitsFile<float> ff;
        ff.write( path, eig );
    }
    catch( const std::exception &e )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "FITS write failed: " ) + e.what() } );
    }
    return 0;
}

int iefcCtrl::writeConfigTxt( const std::string &path, const std::string &body )
{
    std::ofstream out( path );
    if( !out )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "failed to write " + path } );
    }
    out << body;
    return 0;
}

int iefcCtrl::doRefPsf()
{
    setStatus( "refPsf: starting" );
    log<text_log>( "doRefPsf outdir=" + m_outdir );

    if( openCamsci() < 0 || openFsm() < 0 || openShutter() < 0 )
        return -1;
    if( ensureDir( m_outdir ) < 0 )
        return -1;

    const double tip_m = m_fsmTip_nm * 1e-9;
    const double tilt_m = m_fsmTilt_nm * 1e-9;
    const double ref_tip_m = m_fsmRefTip_nm * 1e-9;
    const double ref_tilt_m = m_fsmRefTilt_nm * 1e-9;
    const double ref_pist_m = m_fsmRefPiston_nm * 1e-9;

    // Park at reference home, then poke home+offset.
    setStatus( "refPsf: parking FSM home" );
    if( writeFsmTipTiltPiston( ref_tip_m, ref_tilt_m, ref_pist_m ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    setStatus( "refPsf: poking FSM" );
    if( writeFsmTipTiltPiston( ref_tip_m + tip_m, ref_tilt_m + tilt_m, ref_pist_m ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    if( m_stopRequested.load() )
        return -1;

    setStatus( "refPsf: closing shutter / darks" );
    if( writeScalar( m_shutter, 1.0 ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    std::vector<float> dark;
    uint32_t w = 0, h = 0;
    if( grabMeanCamsci( m_nDark, m_waitFrames, dark, w, h ) < 0 )
        return -1;
    if( saveFitsF32( m_outdir + "/dark_avg.fits", dark, w, h ) < 0 )
        return -1;

    setStatus( "refPsf: opening shutter / PSF" );
    if( writeScalar( m_shutter, 0.0 ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    std::vector<float> psf;
    if( grabMeanCamsci( m_nPsf, m_waitFrames, psf, w, h ) < 0 )
        return -1;

    std::vector<float> psf_sub( psf.size() );
    float peak = -1e30f;
    for( size_t i = 0; i < psf.size(); ++i )
    {
        psf_sub[i] = psf[i] - dark[i];
        if( psf_sub[i] > peak )
            peak = psf_sub[i];
    }

    if( saveFitsF32( m_outdir + "/ref_psf_avg.fits", psf, w, h ) < 0 )
        return -1;
    if( saveFitsF32( m_outdir + "/ref_psf_dark_sub.fits", psf_sub, w, h ) < 0 )
        return -1;

    std::ostringstream cfg;
    cfg << "# Reference PSF package (iefcCtrl)\n"
        << "Imax_ref=" << peak << "\n"
        << "peak_dark_sub=" << peak << "\n"
        << "tip_nm=" << m_fsmTip_nm << "\n"
        << "tilt_nm=" << m_fsmTilt_nm << "\n"
        << "ref_tip_nm=" << m_fsmRefTip_nm << "\n"
        << "ref_tilt_nm=" << m_fsmRefTilt_nm << "\n"
        << "ndark=" << m_nDark << "\n"
        << "npsf=" << m_nPsf << "\n"
        << "nframes=" << m_nFrames << "\n"
        << "wait_frames=" << m_waitFrames << "\n"
        << "dark_file=dark_avg.fits\n"
        << "ref_psf_file=ref_psf_avg.fits\n"
        << "ref_psf_dark_sub_file=ref_psf_dark_sub.fits\n";
    if( writeConfigTxt( m_outdir + "/config.txt", cfg.str() ) < 0 )
        return -1;

    updateIfChanged( m_indiP_Imax_ref, "current", static_cast<double>( peak ) );

    setStatus( "refPsf: restoring FSM home" );
    writeFsmTipTiltPiston( ref_tip_m, ref_tilt_m, ref_pist_m );

    // Keep setupdir in sync if user used outdir as the setup package.
    if( m_setupdir.empty() )
        m_setupdir = m_outdir;

    log<text_log>( "doRefPsf done Imax_ref=" + std::to_string( peak ) );
    setStatus( "refPsf: done" );
    return 0;
}

int iefcCtrl::doDarkLibrary()
{
    setStatus( "darkLibrary: starting" );
    if( openCamsci() < 0 || openShutter() < 0 || openExptime() < 0 )
        return -1;
    if( ensureDir( m_outdir ) < 0 )
        return -1;
    if( ensureDir( m_outdir + "/darks" ) < 0 )
        return -1;

    std::vector<double> times;
    {
        std::stringstream ss( m_exptimesCsv );
        std::string tok;
        while( std::getline( ss, tok, ',' ) )
        {
            if( tok.empty() )
                continue;
            times.push_back( std::atof( tok.c_str() ) );
        }
    }
    if( times.empty() )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "exptimes CSV empty" } );
    }

    const double exptime_start =
        ( m_exptimeShm.md->datatype == _DATATYPE_DOUBLE )
            ? ( (double *)m_exptimeShm.array.raw )[0]
            : static_cast<double>( ( (float *)m_exptimeShm.array.raw )[0] );
    const double shutter_start =
        ( m_shutter.md->datatype == _DATATYPE_DOUBLE )
            ? ( (double *)m_shutter.array.raw )[0]
            : static_cast<double>( ( (float *)m_shutter.array.raw )[0] );

    if( writeScalar( m_shutter, 1.0 ) < 0 )
        return -1;

    std::ofstream manifest( m_outdir + "/dark_library.txt" );
    manifest << "# dark library: exptime  relative_path  ndark\n";

    for( size_t i = 0; i < times.size(); ++i )
    {
        if( m_stopRequested.load() )
            break;
        setStatus( "darkLibrary: exptime=" + std::to_string( times[i] ) );
        if( writeScalar( m_exptimeShm, times[i] ) < 0 )
            return -1;
        mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

        std::vector<float> dark;
        uint32_t w = 0, h = 0;
        if( grabMeanCamsci( m_nDark, m_waitFrames, dark, w, h ) < 0 )
            return -1;

        char rel[64];
        std::snprintf( rel, sizeof( rel ), "darks/dark_%03zu.fits", i );
        if( saveFitsF32( m_outdir + "/" + rel, dark, w, h ) < 0 )
            return -1;
        manifest << times[i] << "  " << rel << "  " << m_nDark << "\n";
    }

    writeScalar( m_shutter, shutter_start );
    writeScalar( m_exptimeShm, exptime_start );
    setStatus( "darkLibrary: done" );
    log<text_log>( "doDarkLibrary wrote " + m_outdir + "/dark_library.txt" );
    return 0;
}

int iefcCtrl::spawnCamsciOnce( const std::vector<std::string> &args )
{
    if( m_camsciOnceBin.empty() )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__,
              "iefc.camsciOnceBin not configured — set path to iefc_camsci_once" } );
    }

    std::ostringstream cmd;
    cmd << "'" << m_camsciOnceBin << "'";
    for( const auto &a : args )
    {
        cmd << " '" << a << "'";
    }
    log<text_log>( "spawn: " + cmd.str() );
    setStatus( "running: " + args[0] );

    const int rc = std::system( cmd.str().c_str() );
    if( rc != 0 )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "iefc_camsci_once exited " + std::to_string( rc ) } );
    }
    return 0;
}

int iefcCtrl::doCalibrate()
{
    std::vector<std::string> args = {
        "calibrate",
        "--outdir=" + m_caldir,
        "--setupdir=" + m_setupdir,
        "--nframes=" + std::to_string( m_nFrames ),
        "--wait-frames=" + std::to_string( m_waitFrames ),
        "--no-run"
    };
    if( m_setExptime )
    {
        args.push_back( "--exptime=" + std::to_string( m_exptime ) );
    }
    const int rc = spawnCamsciOnce( args );
    setStatus( rc == 0 ? "calibrate: done" : "calibrate: failed" );
    return rc;
}

int iefcCtrl::doRun()
{
    std::vector<std::string> args = {
        "run",
        "--indir=" + m_caldir,
        "--setupdir=" + m_setupdir,
        "--nframes=" + std::to_string( m_nFrames ),
        "--wait-frames=" + std::to_string( m_waitFrames ),
        "--reg-cond=" + std::to_string( m_regCond ),
        "--probe-amp=" + std::to_string( m_probeAmp ),
        "--iters=" + std::to_string( m_iters ),
        "--gain=" + std::to_string( m_loopGain ),
        "--leakage=" + std::to_string( m_leakage )
    };
    if( m_setExptime )
    {
        args.push_back( "--exptime=" + std::to_string( m_exptime ) );
    }
    const int rc = spawnCamsciOnce( args );
    setStatus( rc == 0 ? "run: done" : "run: failed" );
    return rc;
}

// ----------------- INDI callbacks -----------------

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_nFrames )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nFrames, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nFrames, target, ipRecv, false ) < 0 )
        return -1;
    m_nFrames = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_waitFrames )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_waitFrames, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_waitFrames, target, ipRecv, false ) < 0 )
        return -1;
    m_waitFrames = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_delay_s )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_delay_s, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_delay_s, target, ipRecv, false ) < 0 )
        return -1;
    m_delay_s = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_exptime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_exptime, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_exptime, target, ipRecv, false ) < 0 )
        return -1;
    m_exptime = target;
    m_setExptime = true;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_outdir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_outdir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_outdir, target, ipRecv, false ) < 0 )
        return -1;
    m_outdir = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_setupdir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_setupdir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_setupdir, target, ipRecv, false ) < 0 )
        return -1;
    m_setupdir = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_caldir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_caldir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_caldir, target, ipRecv, false ) < 0 )
        return -1;
    m_caldir = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmAmp_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmAmp_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmAmp_nm, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmAmp_nm = target;
    m_fsmTip_nm = target;
    m_fsmTilt_nm = target;
    updateIfChanged( m_indiP_fsmTip_nm, "current", m_fsmTip_nm );
    updateIfChanged( m_indiP_fsmTip_nm, "target", m_fsmTip_nm );
    updateIfChanged( m_indiP_fsmTilt_nm, "current", m_fsmTilt_nm );
    updateIfChanged( m_indiP_fsmTilt_nm, "target", m_fsmTilt_nm );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmTip_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmTip_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmTip_nm, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmTip_nm = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmTilt_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmTilt_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmTilt_nm, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmTilt_nm = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmRefTip_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmRefTip_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmRefTip_nm, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmRefTip_nm = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmRefTilt_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmRefTilt_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmRefTilt_nm, target, ipRecv, false ) < 0 )
        return -1;
    m_fsmRefTilt_nm = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_nDark )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nDark, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nDark, target, ipRecv, false ) < 0 )
        return -1;
    m_nDark = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_nPsf )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nPsf, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nPsf, target, ipRecv, false ) < 0 )
        return -1;
    m_nPsf = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_exptimes )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_exptimes, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_exptimes, target, ipRecv, false ) < 0 )
        return -1;
    m_exptimesCsv = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_regCond )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_regCond, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_regCond, target, ipRecv, false ) < 0 )
        return -1;
    m_regCond = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_probeAmp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_probeAmp, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_probeAmp, target, ipRecv, false ) < 0 )
        return -1;
    m_probeAmp = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_iters )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_iters, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_iters, target, ipRecv, false ) < 0 )
        return -1;
    m_iters = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_loopGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_loopGain, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_loopGain, target, ipRecv, false ) < 0 )
        return -1;
    m_loopGain = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_leakage )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_leakage, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_leakage, target, ipRecv, false ) < 0 )
        return -1;
    m_leakage = target;
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_doRefPsf )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_doRefPsf, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_doRefPsf, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::RefPsf );
        clearRequest( m_indiP_doRefPsf );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_doDarkLibrary )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_doDarkLibrary, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_doDarkLibrary, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::DarkLibrary );
        clearRequest( m_indiP_doDarkLibrary );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_doCalibrate )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_doCalibrate, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_doCalibrate, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::Calibrate );
        clearRequest( m_indiP_doCalibrate );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_doRun )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_doRun, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_doRun, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::Run );
        clearRequest( m_indiP_doRun );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_stop )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_stop, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        queueJob( Job::Stop );
        clearRequest( m_indiP_stop );
    }
    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // iefcCtrl_hpp
