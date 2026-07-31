/** \file iefcCtrl.hpp
  * \brief MagAO-X INDI front-end for Stream IEFC (ref PSF, dark library, calibrate, run).
  *
  * All of refPSF, darkLibrary, calibrate, and run execute natively in-process against
  * milk ImageStreamIO shmims via the vendored lina IEFC library. No external binary.
  *
  * Shared INDI numbers (nFrames, waitFrames, delay_s, exptime, …) are reused across
  * all actions. Request switches trigger one-shot worker jobs.
  *
  * \ingroup iefcCtrl_files
  */

#ifndef iefcCtrl_hpp
#define iefcCtrl_hpp

#include <atomic>
#include <cstdint>
#include <deque>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>

#include <ImageStreamIO/ImageStreamIO.h>

#include <mx/improc/eigenImage.hpp>
#include <mx/ioutils/fits/fitsFile.hpp>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

#include "lina/iefc.h"
#include "lina/iefc_package.h"
#include "lina/coro_utils.h"
#include "lina/shmim_utils.h"

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
        ClearDm,
        Stop
    };

    /// D'tor, declared and defined for noexcept (required by MagAOXApp).
    ~iefcCtrl() noexcept
    {
    }

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
    float m_exptime{ 1.0f };      ///< Live / general camera exposure [s] (camsciexptime)
    float m_psfExptime{ 1.0f };   ///< Exposure used for doRefPsf (same milk channel)
    bool m_setExptime{ false };   ///< True after INDI exptime was set (calibrate/run)

    float m_regCond{ -2.5f };
    float m_clProbeAmp{ 1e-9f };     ///< Closed-loop probe amp [m] (INDI cl_probe_amp)
    float m_calibProbeAmp{ 5e-9f };  ///< Calib probe amp [m] (INDI calib_probe_amp)
    float m_calibAmp{ 2e-9f };       ///< Calib mode poke amp [m] (INDI calib_mode_amp)
    unsigned m_iters{ 3 };
    float m_loopGain{ 1.0f };
    float m_leakage{ 0.0f };

    std::string m_subNormName{ "camsci_sub_norm" };
    std::string m_contrastAvgName{ "contrast_avg" };
    unsigned m_contrastAvgN{ 10 }; ///< Frames in running contrast average
    bool m_saveResponseFull{ false }; ///< Full-frame response cube (GB-scale; off by default)
    ///@}

    /** \name Worker thread
      *@{
      */
    std::thread m_worker;
    std::thread m_subNormThread;
    std::atomic<bool> m_workerShutdown{ false };
    std::atomic<bool> m_subNormShutdown{ false };
    std::atomic<bool> m_stopRequested{ false };
    sem_t m_jobSem;
    std::mutex m_jobMutex;
    Job m_pendingJob{ Job::Idle };
    std::atomic<int> m_busy{ 0 }; ///< 0 idle, 1 busy
    std::string m_status{ "idle" };
    ///@}

    /** \name In-memory calibration (filled by doCalibrate; used by doRun)
      *@{
      */
    std::mutex m_calMutex;
    bool m_haveCalibration{ false };
    lina::Array2D<double> m_cachedResponse;   ///< masked (nmodes, nmeas)
    lina::Array2D<double> m_cachedControl;    ///< (nmodes, nmeas) after beta_reg
    lina::Array2D<double> m_cachedProbeModes;
    lina::Array2D<double> m_cachedCalibModes;
    lina::Array2D<std::uint8_t> m_cachedMask;
    lina::Array2D<double> m_cachedDark;
    double m_cachedImaxRef{ 0.0 };
    double m_cachedPsfExptime{ 1.0 };
    double m_cachedGain{ 0.0 };
    ///@}

    /** \name camsci_sub_norm continuous stream
      *@{
      */
    IMAGE m_subNorm{};
    bool m_subNormCreated{ false };
    uint64_t m_lastSubNormCnt0{ 0 };
    std::mutex m_subNormMutex;
    lina::Array2D<double> m_liveDark; ///< exposure-matched dark for sub_norm
    double m_liveImaxRef{ 0.0 };
    double m_livePsfExptime{ 1.0 };
    double m_liveGain{ 0.0 };
    double m_liveDarkExptime{ -1.0 };
    bool m_haveLiveNorm{ false };
    ///@}

    /** \name Continuous contrast average (mask mean of NI>0)
      *@{
      */
    IMAGE m_contrastAvg{};
    bool m_contrastAvgCreated{ false };
    std::deque<double> m_contrastSamples;
    double m_contrastSampleSum{ 0.0 };
    lina::Array2D<std::uint8_t> m_liveContrastMask;
    bool m_haveContrastMask{ false };
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

    /** \name INDI — shmim names (repointable)
      *@{
      */
    pcf::IndiProperty m_indiP_camsci;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camsci);

    pcf::IndiProperty m_indiP_dmChannel;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dmChannel);

    pcf::IndiProperty m_indiP_fsmChannel;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_fsmChannel);

    pcf::IndiProperty m_indiP_shutterShmim;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shutterShmim );

    pcf::IndiProperty m_indiP_shutter; // toggle: On=closed, Off=open
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shutter );

    /// Milk name for exposure-time scalar (not the numeric exposure value).
    pcf::IndiProperty m_indiP_exptimeShmim;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_exptimeShmim);

    /// Milk name for camera-gain scalar (not loopGain).
    pcf::IndiProperty m_indiP_gainShmim;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_gainShmim);

    pcf::IndiProperty m_indiP_subNorm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_subNorm);

    pcf::IndiProperty m_indiP_contrastAvgShmim;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_contrastAvgShmim);
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

    /// Exposure used specifically for ref-PSF / Imax_ref (writes same milk as exptime).
    pcf::IndiProperty m_indiP_psfExptime;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfExptime);

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

    pcf::IndiProperty m_indiP_calibProbeAmp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calibProbeAmp);

    pcf::IndiProperty m_indiP_calibModeAmp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calibModeAmp);

    pcf::IndiProperty m_indiP_clProbeAmp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clProbeAmp);

    pcf::IndiProperty m_indiP_iters;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_iters);

    pcf::IndiProperty m_indiP_loopGain;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_loopGain);

    pcf::IndiProperty m_indiP_leakage;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_leakage);

    pcf::IndiProperty m_indiP_contrastAvgN;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_contrastAvgN);
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

    pcf::IndiProperty m_indiP_clearDm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clearDm);

    pcf::IndiProperty m_indiP_stop;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_stop);

    pcf::IndiProperty m_indiP_status;   ///< RO text
    pcf::IndiProperty m_indiP_Imax_ref; ///< RO number
    pcf::IndiProperty m_indiP_contrast; ///< RO number (last closed-loop iter)
    pcf::IndiProperty m_indiP_contrastAvg; ///< RO running average published to shmim
    pcf::IndiProperty m_indiP_calibMode;   ///< RO: current calib mode (1..N, 0 idle)
    pcf::IndiProperty m_indiP_nCalibModes; ///< RO: total calib modes in package / run
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

    static void subNormStart( iefcCtrl *s );
    void subNormExec();

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
    int doClearDm();

    /// Write shutter milk scalar and publish INDI toggle (On=closed).
    int setShutterClosed( bool closed );

    /// Publish INDI shutter toggle from a known closed/open state (no SHM write).
    void publishShutterIndi( bool closed );

    /// Write exposure-time milk scalar, update INDI exptime current/target, and log.
    int setExptimeValue( double seconds );

    /// Publish psfExptime INDI and write the same milk channel (camsciexptime).
    int setPsfExptimeValue( double seconds );

    /// Cache setup (dark + Imax) for continuous camsci_sub_norm.
    void updateLiveNormFromSetup( const lina::SetupData &setup );

    /// Ensure camsci_sub_norm shmim exists matching camsci geometry.
    int ensureSubNormStream( uint32_t w, uint32_t h );

    /// If a new camsci frame is available, dark-sub + normalize and publish.
    int processSubNormFrame();

    /// Ensure control mask available for continuous contrast (cal cache / caldir / default).
    int ensureContrastMask( uint32_t w, uint32_t h );

    /// Ensure scalar contrast_avg shmim exists.
    int ensureContrastAvgStream();

    /// Push one NI contrast sample and publish running average.
    int updateContrastAverage( double contrast );

    void setStatus( const std::string &s );
    void clearRequest( pcf::IndiProperty &p );

    /// Cooperative cancel predicate for lina calibrate/run/grab.
    lina::StopCheck makeStopCheck();
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
                "string", "Shutter scalar shmim name (INDI shutterShmim; default camscishutter)." );
    config.add( "iefc.exptime", "", "iefc.exptime", argType::Required, "iefc", "exptime", false,
                "string", "Exposure-time scalar shmim name (default camsciexptime)." );
    config.add( "iefc.exptimeShmim",
                "",
                "iefc.exptimeShmim",
                argType::Required,
                "iefc",
                "exptimeShmim",
                false,
                "string",
                "Alias for iefc.exptime (exposure-time shmim name)." );
    config.add( "iefc.gain", "", "iefc.gain", argType::Required, "iefc", "gain", false, "string",
                "Camera-gain scalar shmim (default camscigain)." );
    config.add( "iefc.gainShmim",
                "",
                "iefc.gainShmim",
                argType::Required,
                "iefc",
                "gainShmim",
                false,
                "string",
                "Alias for iefc.gain (camera-gain shmim name)." );
    config.add( "iefc.outdir", "", "iefc.outdir", argType::Required, "iefc", "outdir", false,
                "string", "Default output directory for ref-PSF / darks." );
    config.add( "iefc.setupdir", "", "iefc.setupdir", argType::Required, "iefc", "setupdir", false,
                "string", "Ref-PSF / dark-library directory used by calibrate/run." );
    config.add( "iefc.caldir", "", "iefc.caldir", argType::Required, "iefc", "caldir", false,
                "string", "Calibration package directory (response/control)." );
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
    config.add( "iefc.psf_exptime",
                "",
                "iefc.psf_exptime",
                argType::Required,
                "iefc",
                "psf_exptime",
                false,
                "float",
                "Exposure [s] applied to camsciexptime during doRefPsf (default = exptime)." );
    config.add( "iefc.exptimes", "", "iefc.exptimes", argType::Required, "iefc", "exptimes", false,
                "string", "CSV exposure times for dark-library." );
    config.add( "iefc.regCond", "", "iefc.regCond", argType::Required, "iefc", "regCond", false,
                "float", "beta_reg regularization for run." );
    config.add( "iefc.cl_probe_amp", "", "iefc.cl_probe_amp", argType::Required, "iefc", "cl_probe_amp", false,
                "float", "Closed-loop probe amplitude [m]." );
    config.add( "iefc.calib_probe_amp", "", "iefc.calib_probe_amp", argType::Required, "iefc", "calib_probe_amp", false,
                "float", "Calibration probe amplitude [m]." );
    config.add( "iefc.calib_mode_amp", "", "iefc.calib_mode_amp", argType::Required, "iefc", "calib_mode_amp", false,
                "float", "Calibration mode poke amplitude [m]." );
    config.add( "iefc.iters", "", "iefc.iters", argType::Required, "iefc", "iters", false,
                "unsigned", "Closed-loop iterations per run." );
    config.add( "shmims.camsci_sub_norm", "", "shmims.camsci_sub_norm", argType::Required, "shmims", "camsci_sub_norm", false,
                "string", "Dark-sub + Imax-normalized camsci stream name." );
    config.add( "shmims.contrast_avg", "", "shmims.contrast_avg", argType::Required, "shmims", "contrast_avg", false,
                "string", "Running-average contrast scalar stream name." );
    config.add( "iefc.contrast_avg_n", "", "iefc.contrast_avg_n", argType::Required, "iefc", "contrast_avg_n", false,
                "unsigned", "Number of frames in contrast running average." );
    config.add( "iefc.save_response_full", "", "iefc.save_response_full", argType::Required, "iefc", "save_response_full", false,
                "bool", "Save full-frame response cube (very large; default false)." );
}

void iefcCtrl::loadConfig()
{
    config( m_camsciName, "iefc.camsci" );
    config( m_dmChan, "iefc.dmChannel" );
    config( m_fsmChan, "iefc.fsmChannel" );
    config( m_shutterName, "iefc.shutter" );
    config( m_exptimeName, "iefc.exptime" );
    config( m_exptimeName, "iefc.exptimeShmim" ); // alias overrides if set
    config( m_gainName, "iefc.gain" );
    config( m_gainName, "iefc.gainShmim" ); // alias overrides if set
    config( m_outdir, "iefc.outdir" );
    config( m_setupdir, "iefc.setupdir" );
    config( m_caldir, "iefc.caldir" );
    config( m_nFrames, "iefc.nFrames" );
    config( m_waitFrames, "iefc.waitFrames" );
    config( m_delay_s, "iefc.delay_s" );
    config( m_fsmAmp_nm, "iefc.fsmAmp_nm" );
    m_fsmTip_nm = m_fsmAmp_nm;
    m_fsmTilt_nm = m_fsmAmp_nm;
    config( m_nDark, "iefc.nDark" );
    config( m_nPsf, "iefc.nPsf" );
    config( m_psfExptime, "iefc.psf_exptime" );
    config( m_exptimesCsv, "iefc.exptimes" );
    config( m_regCond, "iefc.regCond" );
    config( m_clProbeAmp, "iefc.cl_probe_amp" );
    config( m_calibProbeAmp, "iefc.calib_probe_amp" );
    config( m_calibAmp, "iefc.calib_mode_amp" );
    config( m_iters, "iefc.iters" );
    config( m_subNormName, "shmims.camsci_sub_norm" );
    config( m_contrastAvgName, "shmims.contrast_avg" );
    config( m_contrastAvgN, "iefc.contrast_avg_n" );
    config( m_saveResponseFull, "iefc.save_response_full" );
}

int iefcCtrl::appStartup()
{
    if( sem_init( &m_jobSem, 0, 0 ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "sem_init failed" } );
    }

    CREATE_REG_INDI_NEW_TEXT( m_indiP_camsci, "camsci", "Science camera shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_dmChannel, "dmChannel", "IEFC DM channel shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_fsmChannel, "fsmChannel", "FSM channel shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shutterShmim, "shutterShmim", "Shutter scalar shmim", "shmims" );
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_shutter, "shutter" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_exptimeShmim, "exptimeShmim", "Exposure-time scalar shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_gainShmim, "gainShmim", "Camera-gain scalar shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_subNorm, "camsci_sub_norm", "Dark-sub+norm camsci stream", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_contrastAvgShmim, "contrastAvgShmim", "Running-avg contrast shmim name", "shmims" );

    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_nFrames, "nFrames", 1, 10000, 1, "%u", "Frames to average", "shared" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_waitFrames, "waitFrames", 0, 1000, 1, "%u", "Frames to skip after DM", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_delay_s, "delay_s", 0, 10, 0.01, "%0.3f", "DM settle [s]", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_exptime, "exptime", 0, 1000, 0.1, "%0.3f", "Live exposure [s] → camsciexptime", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_psfExptime, "psf_exptime", 0, 1000, 0.1, "%0.3f", "Ref-PSF exposure [s] → camsciexptime", "refPsf" );
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
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calibProbeAmp, "calib_probe_amp", 0, 1e-6, 1e-10, "%0.3e", "Calib probe amp [m]", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calibModeAmp, "calib_mode_amp", 0, 1e-6, 1e-10, "%0.3e", "Calib mode amp [m]", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_clProbeAmp, "cl_probe_amp", 0, 1e-6, 1e-10, "%0.3e", "Closed-loop probe amp [m]", "run" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_iters, "iters", 1, 1000, 1, "%u", "Run iterations", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_loopGain, "loopGain", 0, 2, 0.05, "%0.2f", "Loop gain", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_leakage, "leakage", 0, 1, 0.01, "%0.2f", "Leakage", "run" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_contrastAvgN, "contrast_avg_n", 1, 10000, 1, "%u", "Contrast avg window [frames]", "contrast" );

    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doRefPsf, "doRefPsf" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doDarkLibrary, "doDarkLibrary" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doCalibrate, "doCalibrate" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_doRun, "doRun" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_clearDm, "clearDm" );
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

    REG_INDI_NEWPROP_NOCB( m_indiP_contrastAvg, "contrast_avg", pcf::IndiProperty::Number );
    m_indiP_contrastAvg.add( pcf::IndiElement( "current" ) );
    m_indiP_contrastAvg["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_calibMode, "calibMode", pcf::IndiProperty::Number );
    m_indiP_calibMode.add( pcf::IndiElement( "current" ) );
    m_indiP_calibMode["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_nCalibModes, "nCalibModes", pcf::IndiProperty::Number );
    m_indiP_nCalibModes.add( pcf::IndiElement( "current" ) );
    m_indiP_nCalibModes["current"].set( 0.0 );

    // Seed current/target from config (before INDI starts — use setValue, not updateIfChanged)
    m_indiP_camsci["current"].setValue( m_camsciName );
    m_indiP_camsci["target"].setValue( m_camsciName );
    m_indiP_dmChannel["current"].setValue( m_dmChan );
    m_indiP_dmChannel["target"].setValue( m_dmChan );
    m_indiP_fsmChannel["current"].setValue( m_fsmChan );
    m_indiP_fsmChannel["target"].setValue( m_fsmChan );
    m_indiP_shutterShmim["current"].setValue( m_shutterName );
    m_indiP_shutterShmim["target"].setValue( m_shutterName );
    // Seed shutter toggle Off (open) before INDI is up — use setSwitchState, not update*
    m_indiP_shutter["toggle"].setSwitchState( pcf::IndiElement::Off );
    m_indiP_exptimeShmim["current"].setValue( m_exptimeName );
    m_indiP_exptimeShmim["target"].setValue( m_exptimeName );
    m_indiP_gainShmim["current"].setValue( m_gainName );
    m_indiP_gainShmim["target"].setValue( m_gainName );
    m_indiP_subNorm["current"].setValue( m_subNormName );
    m_indiP_subNorm["target"].setValue( m_subNormName );
    m_indiP_contrastAvgShmim["current"].setValue( m_contrastAvgName );
    m_indiP_contrastAvgShmim["target"].setValue( m_contrastAvgName );
    m_indiP_nFrames["current"].setValue( m_nFrames );
    m_indiP_nFrames["target"].setValue( m_nFrames );
    m_indiP_waitFrames["current"].setValue( m_waitFrames );
    m_indiP_waitFrames["target"].setValue( m_waitFrames );
    m_indiP_delay_s["current"].setValue( m_delay_s );
    m_indiP_delay_s["target"].setValue( m_delay_s );
    m_indiP_exptime["current"].setValue( m_exptime );
    m_indiP_exptime["target"].setValue( m_exptime );
    m_indiP_psfExptime["current"].setValue( m_psfExptime );
    m_indiP_psfExptime["target"].setValue( m_psfExptime );
    m_indiP_outdir["current"].setValue( m_outdir );
    m_indiP_outdir["target"].setValue( m_outdir );
    m_indiP_setupdir["current"].setValue( m_setupdir );
    m_indiP_setupdir["target"].setValue( m_setupdir );
    m_indiP_caldir["current"].setValue( m_caldir );
    m_indiP_caldir["target"].setValue( m_caldir );
    m_indiP_fsmAmp_nm["current"].setValue( m_fsmAmp_nm );
    m_indiP_fsmAmp_nm["target"].setValue( m_fsmAmp_nm );
    m_indiP_fsmTip_nm["current"].setValue( m_fsmTip_nm );
    m_indiP_fsmTip_nm["target"].setValue( m_fsmTip_nm );
    m_indiP_fsmTilt_nm["current"].setValue( m_fsmTilt_nm );
    m_indiP_fsmTilt_nm["target"].setValue( m_fsmTilt_nm );
    m_indiP_fsmRefTip_nm["current"].setValue( m_fsmRefTip_nm );
    m_indiP_fsmRefTip_nm["target"].setValue( m_fsmRefTip_nm );
    m_indiP_fsmRefTilt_nm["current"].setValue( m_fsmRefTilt_nm );
    m_indiP_fsmRefTilt_nm["target"].setValue( m_fsmRefTilt_nm );
    m_indiP_nDark["current"].setValue( m_nDark );
    m_indiP_nDark["target"].setValue( m_nDark );
    m_indiP_nPsf["current"].setValue( m_nPsf );
    m_indiP_nPsf["target"].setValue( m_nPsf );
    m_indiP_exptimes["current"].setValue( m_exptimesCsv );
    m_indiP_exptimes["target"].setValue( m_exptimesCsv );
    m_indiP_regCond["current"].setValue( m_regCond );
    m_indiP_regCond["target"].setValue( m_regCond );
    m_indiP_calibProbeAmp["current"].setValue( m_calibProbeAmp );
    m_indiP_calibProbeAmp["target"].setValue( m_calibProbeAmp );
    m_indiP_calibModeAmp["current"].setValue( m_calibAmp );
    m_indiP_calibModeAmp["target"].setValue( m_calibAmp );
    m_indiP_clProbeAmp["current"].setValue( m_clProbeAmp );
    m_indiP_clProbeAmp["target"].setValue( m_clProbeAmp );
    m_indiP_iters["current"].setValue( m_iters );
    m_indiP_iters["target"].setValue( m_iters );
    m_indiP_loopGain["current"].setValue( m_loopGain );
    m_indiP_loopGain["target"].setValue( m_loopGain );
    m_indiP_leakage["current"].setValue( m_leakage );
    m_indiP_leakage["target"].setValue( m_leakage );
    m_indiP_contrastAvgN["current"].setValue( m_contrastAvgN );
    m_indiP_contrastAvgN["target"].setValue( m_contrastAvgN );

    m_worker = std::thread( workerStart, this );
    m_subNormShutdown = false;
    m_subNormThread = std::thread( subNormStart, this );

    state( stateCodes::READY );

    log<text_log>( "iefcCtrl started (native jobs + camsci_sub_norm frame thread)" );
    return 0;
}

int iefcCtrl::appLogic()
{
    // Keep FSM in OPERATING while a job runs; otherwise leave READY from appStartup.
    if( m_busy.load() )
    {
        state( stateCodes::OPERATING );
    }
    else if( state() == stateCodes::OPERATING )
    {
        state( stateCodes::READY );
    }

    // camsci_sub_norm / contrast_avg run on m_subNormThread (semaphore-driven).
    return 0;
}

int iefcCtrl::appShutdown()
{
    m_workerShutdown = true;
    m_subNormShutdown = true;
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
    try
    {
        if( m_subNormThread.joinable() )
            m_subNormThread.join();
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

void iefcCtrl::subNormStart( iefcCtrl *s )
{
    s->subNormExec();
}

void iefcCtrl::subNormExec()
{
    // Frame-driven: wait on camsci semaphore so we keep up with the camera,
    // independent of MagAOX loopPause (often 1s by default).
    log<text_log>( "camsci_sub_norm thread started" );
    int sem = -1;
    while( !m_subNormShutdown.load() )
    {
        if( !m_camsciOpen )
        {
            if( ImageStreamIO_openIm( &m_camsci, m_camsciName.c_str() ) != IMAGESTREAMIO_SUCCESS )
            {
                mx::sys::milliSleep( 200 );
                continue;
            }
            m_camsciOpen = true;
            m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
        }
        if( sem < 0 )
            sem = ( m_camsciSem >= 0 ) ? m_camsciSem
                                      : ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
        if( sem < 0 )
        {
            mx::sys::milliSleep( 100 );
            continue;
        }

        timespec ts{};
        clock_gettime( CLOCK_REALTIME, &ts );
        ts.tv_nsec += 200000000; // 200 ms
        if( ts.tv_nsec >= 1000000000 )
        {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        if( ImageStreamIO_semtimedwait( &m_camsci, sem, &ts ) != 0 )
            continue;

        processSubNormFrame();
    }
    log<text_log>( "camsci_sub_norm thread stopped" );
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

        if( job == Job::Stop )
        {
            // Stop while idle: just acknowledge.
            setStatus( "idle" );
            continue;
        }

        m_busy = 1;
        // Fresh job: clear any stale stop from a previous abort.
        m_stopRequested = false;
        try
        {
            runJob( job );
            if( m_stopRequested.load() )
                setStatus( "stopped" );
            else
                setStatus( "idle" );
        }
        catch( ... )
        {
            setStatus( "idle" );
        }
        m_busy = 0;
        updateIfChanged( m_indiP_calibMode, "current", 0.0 );
    }
}

void iefcCtrl::queueJob( Job j )
{
    if( j == Job::Stop )
    {
        // Cooperative cancel for the in-flight job (calibrate/run/refPsf/...).
        m_stopRequested = true;
        log<text_log>( "stop requested" +
                       std::string( m_busy.load() ? " (aborting active job)" : "" ) );
        return;
    }

    {
        std::lock_guard<std::mutex> lock( m_jobMutex );
        if( m_busy.load() )
        {
            log<text_log>( "iefcCtrl busy; ignoring new job (use stop first)",
                           logPrio::LOG_WARNING );
            return;
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
        case Job::ClearDm:
            return doClearDm();
        case Job::Stop:
            setStatus( "stop requested" );
            return 0;
        default:
            return 0;
    }
}

lina::StopCheck iefcCtrl::makeStopCheck()
{
    return [this]() { return m_stopRequested.load() || m_workerShutdown.load(); };
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
    if( m_subNormCreated )
    {
        ImageStreamIO_closeIm( &m_subNorm );
        m_subNormCreated = false;
    }
    if( m_contrastAvgCreated )
    {
        ImageStreamIO_closeIm( &m_contrastAvg );
        m_contrastAvgCreated = false;
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
    else if( im.md->datatype == _DATATYPE_INT32 )
    {
        ( (int32_t *)im.array.raw )[0] = static_cast<int32_t>( value );
    }
    else if( im.md->datatype == _DATATYPE_UINT16 )
    {
        ( (uint16_t *)im.array.raw )[0] = static_cast<uint16_t>( value );
    }
    else
    {
        im.md->write = 0;
        return log<software_error, -1>(
            { __FILE__, __LINE__,
              "unsupported scalar datatype " + std::to_string( im.md->datatype ) } );
    }
    // UpdateIm bumps cnt0 and posts semaphores (required for many milk consumers).
    if( ImageStreamIO_UpdateIm( &im ) != IMAGESTREAMIO_SUCCESS )
    {
        im.md->cnt0++;
        im.md->write = 0;
        ImageStreamIO_sempost( &im, -1 );
    }
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
    const auto t0 = std::chrono::steady_clock::now();
    while( collected < nframes )
    {
        if( m_stopRequested.load() )
            return -1;

        timespec ts{};
        clock_gettime( CLOCK_REALTIME, &ts );
        ts.tv_nsec += 200000000; // 200 ms slices for responsive stop
        if( ts.tv_nsec >= 1000000000 )
        {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        if( ImageStreamIO_semtimedwait( &m_camsci, m_camsciSem, &ts ) != 0 )
        {
            const double elapsed = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - t0 )
                                       .count();
            if( elapsed > 30.0 )
            {
                return log<software_error, -1>(
                    { __FILE__, __LINE__, "timeout waiting for camsci frame" } );
            }
            continue;
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

void iefcCtrl::publishShutterIndi( bool closed )
{
    if( closed )
        updateSwitchIfChanged( m_indiP_shutter, "toggle", pcf::IndiElement::On, INDI_OK );
    else
        updateSwitchIfChanged( m_indiP_shutter, "toggle", pcf::IndiElement::Off, INDI_IDLE );
}

int iefcCtrl::setShutterClosed( bool closed )
{
    if( openShutter() < 0 )
        return -1;
    log<text_log>( std::string( "shutter -> " ) + ( closed ? "CLOSED" : "OPEN" ) +
                   " (shmim " + m_shutterName + ")" );
    if( writeScalar( m_shutter, closed ? 1.0 : 0.0 ) < 0 )
        return -1;
    publishShutterIndi( closed );
    return 0;
}

int iefcCtrl::setExptimeValue( double seconds )
{
    if( openExptime() < 0 )
        return -1;
    log<text_log>( "exptime -> " + std::to_string( seconds ) + " s (shmim " + m_exptimeName +
                   ", dtype=" + std::to_string( m_exptimeShm.md->datatype ) + ")" );
    if( writeScalar( m_exptimeShm, seconds ) < 0 )
        return -1;
    m_exptime = static_cast<float>( seconds );
    updateIfChanged( m_indiP_exptime, "current", m_exptime );
    updateIfChanged( m_indiP_exptime, "target", m_exptime );
    return 0;
}

int iefcCtrl::setPsfExptimeValue( double seconds )
{
    m_psfExptime = static_cast<float>( seconds );
    updateIfChanged( m_indiP_psfExptime, "current", m_psfExptime );
    updateIfChanged( m_indiP_psfExptime, "target", m_psfExptime );
    // Same milk channel as live exptime.
    return setExptimeValue( seconds );
}

void iefcCtrl::updateLiveNormFromSetup( const lina::SetupData &setup )
{
    if( !setup.loaded || !( setup.Imax_ref > 0.0 ) || setup.dark.size() == 0 )
        return;
    {
        std::lock_guard<std::mutex> lock( m_subNormMutex );
        m_liveDark = setup.dark;
        m_liveImaxRef = setup.Imax_ref;
        m_livePsfExptime = ( setup.psf_exptime > 0.0 ) ? setup.psf_exptime : 1.0;
        m_liveGain = setup.gain;
        m_liveDarkExptime = setup.dark_exptime;
        m_haveLiveNorm = true;
    }
    if( setup.dark_from_library )
    {
        std::ostringstream oss;
        oss << "dark library match: using " << setup.dark_path_used
            << " (library exptime=" << setup.dark_exptime
            << " s, |err|=" << setup.dark_match_err << " s)";
        if( setup.dark_match_err > lina::kDarkExptimeMatchTol )
        {
            log<text_log>( oss.str() + " — exceeds tol "
                               + std::to_string( lina::kDarkExptimeMatchTol ) + " s",
                           logPrio::LOG_WARNING );
        }
        else
        {
            log<text_log>( oss.str() );
        }
    }
}

int iefcCtrl::ensureSubNormStream( uint32_t w, uint32_t h )
{
    if( m_subNormCreated )
    {
        const uint32_t ow = m_subNorm.md ? m_subNorm.md->size[0] : 0;
        const uint32_t oh =
            ( m_subNorm.md && m_subNorm.md->naxis > 1 ) ? m_subNorm.md->size[1] : 0;
        if( ow == w && oh == h )
            return 0;
        ImageStreamIO_closeIm( &m_subNorm );
        m_subNormCreated = false;
    }

    // Prefer attaching to an existing stream; create if missing/mismatched.
    if( ImageStreamIO_openIm( &m_subNorm, m_subNormName.c_str() ) == IMAGESTREAMIO_SUCCESS )
    {
        const uint32_t ow = m_subNorm.md->size[0];
        const uint32_t oh = ( m_subNorm.md->naxis > 1 ) ? m_subNorm.md->size[1] : 1;
        if( ow == w && oh == h && m_subNorm.md->datatype == _DATATYPE_FLOAT )
        {
            m_subNormCreated = true;
            return 0;
        }
        ImageStreamIO_closeIm( &m_subNorm );
    }

    uint32_t imsize[3] = { w, h, 0 };
    if( ImageStreamIO_createIm_gpu( &m_subNorm,
                                    m_subNormName.c_str(),
                                    2,
                                    imsize,
                                    _DATATYPE_FLOAT,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    MATH_DATA,
                                    0 ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to create " + m_subNormName } );
    }
    m_subNormCreated = true;
    log<text_log>( "created shmim " + m_subNormName + " " + std::to_string( w ) + "x" +
                   std::to_string( h ) );
    return 0;
}

int iefcCtrl::processSubNormFrame()
{
    // Lazy-load setup for continuous norm if not yet available.
    if( !m_haveLiveNorm && !m_setupdir.empty() && !m_busy.load() )
    {
        try
        {
            double live_exptime = m_exptime;
            if( openExptime() == 0 )
            {
                live_exptime = ( m_exptimeShm.md->datatype == _DATATYPE_DOUBLE )
                                   ? ( (double *)m_exptimeShm.array.raw )[0]
                                   : (double)( (float *)m_exptimeShm.array.raw )[0];
            }
            std::size_t ncam = 0;
            if( !m_camsciOpen &&
                ImageStreamIO_openIm( &m_camsci, m_camsciName.c_str() ) == IMAGESTREAMIO_SUCCESS )
            {
                m_camsciOpen = true;
                m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
            }
            if( m_camsciOpen )
                ncam = static_cast<std::size_t>( m_camsci.md->size[0] );
            auto setup = lina::load_setup_dir( m_setupdir, ncam, live_exptime );
            updateLiveNormFromSetup( setup );
        }
        catch( ... )
        {
            // Setup not ready yet; stay quiet in the tight appLogic loop.
        }
    }

    if( !m_haveLiveNorm )
        return 0;

    // Quiet attach: appLogic runs continuously; do not spam logs if camsci is down.
    if( !m_camsciOpen )
    {
        if( ImageStreamIO_openIm( &m_camsci, m_camsciName.c_str() ) != IMAGESTREAMIO_SUCCESS )
            return 0;
        m_camsciOpen = true;
        m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
    }

    const uint64_t cnt = m_camsci.md->cnt0;
    if( cnt == 0 || cnt == m_lastSubNormCnt0 )
        return 0;
    m_lastSubNormCnt0 = cnt;

    const uint32_t w = m_camsci.md->size[0];
    const uint32_t h = ( m_camsci.md->naxis > 1 ) ? m_camsci.md->size[1] : 1;
    const size_t npix = static_cast<size_t>( w ) * static_cast<size_t>( h );
    if( ensureSubNormStream( w, h ) < 0 )
        return -1;

    // Refresh exposure-matched dark when live exptime changes.
    double live_exptime = m_exptime;
    if( openExptime() == 0 )
    {
        live_exptime = ( m_exptimeShm.md->datatype == _DATATYPE_DOUBLE )
                           ? ( (double *)m_exptimeShm.array.raw )[0]
                           : (double)( (float *)m_exptimeShm.array.raw )[0];
    }
    if( !m_setupdir.empty() &&
        ( m_liveDarkExptime < 0.0 ||
          std::fabs( live_exptime - m_liveDarkExptime ) > lina::kDarkExptimeMatchTol ) &&
        !m_busy.load() )
    {
        try
        {
            auto setup = lina::load_setup_dir( m_setupdir, w, live_exptime );
            updateLiveNormFromSetup( setup );
        }
        catch( ... )
        {
        }
    }

    lina::Array2D<double> dark;
    double imax = 0.0;
    double psf_exptime = 1.0;
    double gain = 0.0;
    {
        std::lock_guard<std::mutex> lock( m_subNormMutex );
        if( !m_haveLiveNorm || m_liveDark.size() != npix )
            return 0;
        dark = m_liveDark;
        imax = m_liveImaxRef;
        psf_exptime = m_livePsfExptime;
        gain = m_liveGain;
    }
    if( !( imax > 0.0 ) )
        return 0;

    lina::Array2D<double> raw( w, h, 0.0 ); // rows=size[0], cols=size[1]
    const size_t slice =
        ( m_camsci.md->naxis >= 3 )
            ? static_cast<size_t>( m_camsci.md->cnt1 % m_camsci.md->size[2] )
            : 0;

    if( m_camsci.md->datatype == _DATATYPE_FLOAT )
    {
        const float *src = (const float *)m_camsci.array.raw + slice * npix;
        for( size_t i = 0; i < npix; ++i )
            raw.data()[i] = static_cast<double>( src[i] );
    }
    else if( m_camsci.md->datatype == _DATATYPE_DOUBLE )
    {
        const double *src = (const double *)m_camsci.array.raw + slice * npix;
        for( size_t i = 0; i < npix; ++i )
            raw.data()[i] = src[i];
    }
    else if( m_camsci.md->datatype == _DATATYPE_UINT16 )
    {
        const uint16_t *src = (const uint16_t *)m_camsci.array.raw + slice * npix;
        for( size_t i = 0; i < npix; ++i )
            raw.data()[i] = static_cast<double>( src[i] );
    }
    else
    {
        return 0;
    }

    lina::ImParams im_params;
    im_params.exp_time = live_exptime > 0.0 ? live_exptime : 1.0;
    im_params.gain = gain;
    im_params.Imax = imax;

    lina::ImParams ref_params;
    ref_params.exp_time = psf_exptime > 0.0 ? psf_exptime : 1.0;
    ref_params.gain = gain;
    ref_params.Imax = imax;

    lina::Array2D<double> ni;
    try
    {
        ni = lina::normalize_coro_im( raw, im_params, ref_params, dark );
    }
    catch( ... )
    {
        return 0;
    }

    m_subNorm.md->write = 1;
    float *dst = (float *)m_subNorm.array.raw;
    for( size_t i = 0; i < npix; ++i )
        dst[i] = static_cast<float>( ni.data()[i] );
    if( ImageStreamIO_UpdateIm( &m_subNorm ) != IMAGESTREAMIO_SUCCESS )
    {
        m_subNorm.md->cnt0++;
        m_subNorm.md->write = 0;
        ImageStreamIO_sempost( &m_subNorm, -1 );
    }

    // Running contrast over control mask (NI>0 mean), published to contrast_avg.
    if( ensureContrastMask( w, h ) == 0 )
    {
        try
        {
            const auto cr = lina::compute_contrast( ni, m_liveContrastMask );
            updateContrastAverage( cr.contrast );
        }
        catch( ... )
        {
        }
    }
    return 0;
}

int iefcCtrl::ensureContrastMask( uint32_t w, uint32_t h )
{
    if( m_haveContrastMask && m_liveContrastMask.rows() == w &&
        m_liveContrastMask.cols() == h )
        return 0;

    // Prefer in-memory calibration mask.
    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        if( m_haveCalibration && m_cachedMask.rows() == w && m_cachedMask.cols() == h )
        {
            m_liveContrastMask = m_cachedMask;
            m_haveContrastMask = true;
            return 0;
        }
    }

    // Try caldir package wfs_mask only (avoid requiring full modes package).
    if( !m_caldir.empty() )
    {
        try
        {
            lina::PackagePaths pkg;
            pkg.dir = m_caldir;
            auto m = lina::load_fits_double( pkg.wfs_mask_path() );
            if( m.rows() == w && m.cols() == h )
            {
                m_liveContrastMask = lina::Array2D<std::uint8_t>( w, h, 0 );
                for( size_t i = 0; i < m.size(); ++i )
                    m_liveContrastMask.data()[i] = m.data()[i] > 0.5 ? 1 : 0;
                m_haveContrastMask = true;
                return 0;
            }
        }
        catch( ... )
        {
        }
    }

    // Fall back to default half-annulus DH mask (square camsci).
    if( w == h && w > 0 )
    {
        auto in = lina::default_loop_inputs( w, 34 );
        m_liveContrastMask = lina::create_annular_focal_plane_mask(
            in.ncam, in.pxscl, in.dh_iwa, in.dh_owa, in.dh_iwa, "odd", in.dh_rot );
        m_haveContrastMask = true;
        return 0;
    }
    return -1;
}

int iefcCtrl::ensureContrastAvgStream()
{
    if( m_contrastAvgCreated )
        return 0;

    if( ImageStreamIO_openIm( &m_contrastAvg, m_contrastAvgName.c_str() ) ==
        IMAGESTREAMIO_SUCCESS )
    {
        const uint32_t n =
            m_contrastAvg.md->size[0] *
            ( ( m_contrastAvg.md->naxis > 1 ) ? m_contrastAvg.md->size[1] : 1 );
        if( n >= 1 && ( m_contrastAvg.md->datatype == _DATATYPE_FLOAT ||
                        m_contrastAvg.md->datatype == _DATATYPE_DOUBLE ) )
        {
            m_contrastAvgCreated = true;
            return 0;
        }
        ImageStreamIO_closeIm( &m_contrastAvg );
    }

    uint32_t imsize[3] = { 1, 1, 0 };
    if( ImageStreamIO_createIm_gpu( &m_contrastAvg,
                                    m_contrastAvgName.c_str(),
                                    2,
                                    imsize,
                                    _DATATYPE_DOUBLE,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    MATH_DATA,
                                    0 ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to create " + m_contrastAvgName } );
    }
    m_contrastAvgCreated = true;
    log<text_log>( "created shmim " + m_contrastAvgName );
    return 0;
}

int iefcCtrl::updateContrastAverage( double contrast )
{
    const unsigned nwin = m_contrastAvgN < 1 ? 1 : m_contrastAvgN;
    m_contrastSamples.push_back( contrast );
    m_contrastSampleSum += contrast;
    while( m_contrastSamples.size() > nwin )
    {
        m_contrastSampleSum -= m_contrastSamples.front();
        m_contrastSamples.pop_front();
    }
    if( m_contrastSamples.empty() )
        return 0;

    const double avg = m_contrastSampleSum / static_cast<double>( m_contrastSamples.size() );
    if( ensureContrastAvgStream() == 0 )
        writeScalar( m_contrastAvg, avg );
    updateIfChanged( m_indiP_contrastAvg, "current", avg );
    return 0;
}

int iefcCtrl::doRefPsf()
{
    setStatus( "refPsf: starting" );
    log<text_log>( "doRefPsf outdir=" + m_outdir + " psf_exptime=" +
                   std::to_string( m_psfExptime ) + " s" );

    if( openCamsci() < 0 || openFsm() < 0 || openShutter() < 0 || openExptime() < 0 )
        return -1;
    if( ensureDir( m_outdir ) < 0 )
        return -1;

    // Apply PSF exposure to camsciexptime before dark + PSF averages.
    setStatus( "refPsf: setting psf_exptime" );
    if( setPsfExptimeValue( m_psfExptime ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

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
    if( setShutterClosed( true ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    std::vector<float> dark;
    uint32_t w = 0, h = 0;
    if( grabMeanCamsci( m_nDark, m_waitFrames, dark, w, h ) < 0 )
        return -1;
    if( saveFitsF32( m_outdir + "/dark_avg.fits", dark, w, h ) < 0 )
        return -1;

    setStatus( "refPsf: opening shutter / PSF" );
    if( setShutterClosed( false ) < 0 )
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
        << "camsci=" << m_camsciName << "\n"
        << "dm_channel=" << m_dmChan << "\n"
        << "fsm_channel=" << m_fsmChan << "\n"
        << "shutter_shm=" << m_shutterName << "\n"
        << "exptime_shm=" << m_exptimeName << "\n"
        << "gain_shm=" << m_gainName << "\n"
        << "exptime=" << m_psfExptime << "\n"
        << "psf_exptime=" << m_psfExptime << "\n"
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

    // Seed continuous camsci_sub_norm from this ref-PSF package.
    {
        lina::SetupData setup;
        setup.loaded = true;
        setup.dir = m_outdir;
        setup.dark = lina::Array2D<double>( w, h, 0.0 ); // rows=size[0], cols=size[1]
        for( size_t i = 0; i < dark.size(); ++i )
            setup.dark.data()[i] = static_cast<double>( dark[i] );
        setup.Imax_ref = static_cast<double>( peak );
        setup.psf_exptime = m_psfExptime;
        setup.dark_exptime = m_psfExptime;
        updateLiveNormFromSetup( setup );
    }

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

    log<text_log>( "darkLibrary: " + std::to_string( times.size() ) +
                   " exposures from exptimes=[" + m_exptimesCsv + "], nDark=" +
                   std::to_string( m_nDark ) + ", start exptime=" +
                   std::to_string( exptime_start ) + " s, shutter_start=" +
                   std::to_string( shutter_start ) );
    if( setShutterClosed( true ) < 0 )
        return -1;

    std::ofstream manifest( m_outdir + "/dark_library.txt" );
    manifest << "# dark library: exptime  relative_path  ndark\n";

    for( size_t i = 0; i < times.size(); ++i )
    {
        if( m_stopRequested.load() )
            break;
        setStatus( "darkLibrary: exptime=" + std::to_string( times[i] ) );
        log<text_log>( "darkLibrary: [" + std::to_string( i + 1 ) + "/" +
                       std::to_string( times.size() ) + "] setting exptime=" +
                       std::to_string( times[i] ) + " s" );
        if( setExptimeValue( times[i] ) < 0 )
            return -1;
        mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

        // Prefer the camera-reported exptime in the manifest so later matching
        // against live milk values (which may round) stays consistent.
        double reported = times[i];
        if( openExptime() == 0 )
        {
            reported = ( m_exptimeShm.md->datatype == _DATATYPE_DOUBLE )
                           ? ( (double *)m_exptimeShm.array.raw )[0]
                           : static_cast<double>( ( (float *)m_exptimeShm.array.raw )[0] );
        }

        std::vector<float> dark;
        uint32_t w = 0, h = 0;
        if( grabMeanCamsci( m_nDark, m_waitFrames, dark, w, h ) < 0 )
            return -1;

        char rel[64];
        std::snprintf( rel, sizeof( rel ), "darks/dark_%03zu.fits", i );
        if( saveFitsF32( m_outdir + "/" + rel, dark, w, h ) < 0 )
            return -1;
        manifest << std::setprecision( 17 ) << reported << "  " << rel << "  " << m_nDark
                 << "\n";
        log<text_log>( "darkLibrary: wrote " + m_outdir + "/" + rel + " (requested=" +
                       std::to_string( times[i] ) + " s, camera=" +
                       std::to_string( reported ) + " s)" );
    }

    log<text_log>( "darkLibrary: restoring shutter/exptime" );
    if( shutter_start > 0.5 )
        setShutterClosed( true );
    else
        setShutterClosed( false );
    if( setExptimeValue( exptime_start ) < 0 )
        return -1;
    setStatus( "darkLibrary: done" );
    log<text_log>( "doDarkLibrary wrote " + m_outdir + "/dark_library.txt" );
    return 0;
}

int iefcCtrl::doClearDm()
{
    setStatus( "clearDm: zeroing " + m_dmChan );
    log<text_log>( "clearDm: writing zeros to " + m_dmChan );

    if( openDm() < 0 )
        return -1;

    const uint32_t w = m_dm.md->size[0];
    const uint32_t h = ( m_dm.md->naxis > 1 ) ? m_dm.md->size[1] : 1;
    const size_t npix = static_cast<size_t>( w ) * static_cast<size_t>( h );

    m_dm.md->write = 1;
    if( m_dm.md->datatype == _DATATYPE_FLOAT )
    {
        float *p = (float *)m_dm.array.raw;
        for( size_t i = 0; i < npix; ++i )
            p[i] = 0.0f;
    }
    else if( m_dm.md->datatype == _DATATYPE_DOUBLE )
    {
        double *p = (double *)m_dm.array.raw;
        for( size_t i = 0; i < npix; ++i )
            p[i] = 0.0;
    }
    else
    {
        m_dm.md->write = 0;
        return log<software_error, -1>(
            { __FILE__, __LINE__,
              "unsupported DM datatype " + std::to_string( m_dm.md->datatype ) } );
    }

    if( ImageStreamIO_UpdateIm( &m_dm ) != IMAGESTREAMIO_SUCCESS )
    {
        m_dm.md->cnt0++;
        m_dm.md->write = 0;
        ImageStreamIO_sempost( &m_dm, -1 );
    }

    log<text_log>( "clearDm: zeroed " + m_dmChan + " (" + std::to_string( w ) + "x" +
                   std::to_string( h ) + ")" );
    setStatus( "clearDm: done" );
    return 0;
}

int iefcCtrl::doCalibrate()
{
    setStatus( "calibrate: starting" );
    try
    {
        lina::ShmimStream camsci( m_camsciName );
        lina::ShmimStream dm( m_dmChan );

        if( m_setExptime )
        {
            if( setExptimeValue( m_exptime ) < 0 )
                return -1;
        }
        double live_exptime = m_exptime;
        if( openExptime() == 0 )
        {
            live_exptime = ( m_exptimeShm.md->datatype == _DATATYPE_DOUBLE )
                               ? ( (double *)m_exptimeShm.array.raw )[0]
                               : (double)( (float *)m_exptimeShm.array.raw )[0];
        }

        auto in = lina::default_loop_inputs( camsci.rows(), dm.rows() );
        in.nframes = m_nFrames;
        in.wait_frames = m_waitFrames;
        in.delay_s = m_delay_s;
        in.calib_probe_amp = m_calibProbeAmp;
        in.calib_amp = m_calibAmp;
        in.reg_cond = m_regCond;

        if( m_setupdir.empty() )
            return log<software_error, -1>( { __FILE__, __LINE__, "setupdir required" } );
        auto setup = lina::load_setup_dir( m_setupdir, camsci.rows(), live_exptime );
        lina::apply_setup( in, setup, live_exptime );
        lina::generate_modes( in );
        updateLiveNormFromSetup( setup );

        const unsigned nmodes = static_cast<unsigned>( in.calib_modes.rows() );
        updateIfChanged( m_indiP_nCalibModes, "current", static_cast<double>( nmodes ) );
        updateIfChanged( m_indiP_calibMode, "current", 0.0 );
        log<text_log>( "calibrate: " + std::to_string( nmodes ) + " calib modes, " +
                       std::to_string( in.probe_modes.rows() ) + " probes" );

        setStatus( "calibrate: measuring response (0/" + std::to_string( nmodes ) + ")" );
        auto cal = lina::calibrate(
            camsci,
            in.nframes,
            dm,
            in.im_params,
            in.ref_params,
            in.control_mask,
            in.calib_probe_amp,
            in.probe_modes,
            in.calib_amp,
            in.calib_modes,
            in.delay_s,
            in.dm_scale,
            nullptr, // no dark sub during calib (difference images)
            in.wait_frames,
            [this]( std::size_t mode, std::size_t nmodes_cb ) {
                updateIfChanged( m_indiP_nCalibModes, "current",
                                 static_cast<double>( nmodes_cb ) );
                updateIfChanged( m_indiP_calibMode, "current", static_cast<double>( mode ) );
                if( mode == 0 )
                {
                    setStatus( "calibrate: measuring response (0/" +
                               std::to_string( nmodes_cb ) + ")" );
                }
                else
                {
                    setStatus( "calibrate: mode " + std::to_string( mode ) + "/" +
                               std::to_string( nmodes_cb ) );
                    if( mode == 1 || mode == nmodes_cb || ( mode % 10 ) == 0 )
                    {
                        log<text_log>( "calibrate: mode " + std::to_string( mode ) + "/" +
                                       std::to_string( nmodes_cb ) );
                    }
                }
            },
            m_saveResponseFull,
            makeStopCheck() );
        dm.zero();

        updateIfChanged( m_indiP_calibMode, "current", static_cast<double>( nmodes ) );
        setStatus( "calibrate: computing control matrix (beta_reg)" );
        log<text_log>( "calibrate: beta_reg on response "
                       + std::to_string( cal.response_masked.rows() ) + "x"
                       + std::to_string( cal.response_masked.cols() ) );
        auto control = lina::beta_reg_cpu( lina::transpose( cal.response_masked ), in.reg_cond );
        log<text_log>( "calibrate: beta_reg done" );

        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            m_cachedResponse = cal.response_masked;
            m_cachedControl = control;
            m_cachedProbeModes = in.probe_modes;
            m_cachedCalibModes = in.calib_modes;
            m_cachedMask = in.control_mask;
            m_cachedDark = setup.dark;
            m_cachedImaxRef = setup.Imax_ref;
            m_cachedPsfExptime = setup.psf_exptime;
            m_cachedGain = setup.gain;
            m_haveCalibration = true;
        }
        m_liveContrastMask = in.control_mask;
        m_haveContrastMask = true;

        setStatus( "calibrate: writing package" );
        lina::PackagePaths pkg;
        pkg.dir = m_caldir;
        const lina::Array2D<double> *full_ptr =
            ( m_saveResponseFull && cal.response_full.size() > 0 ) ? &cal.response_full
                                                                  : nullptr;
        lina::save_package( pkg, in, cal.response_masked, control, setup, full_ptr );

        updateIfChanged( m_indiP_Imax_ref, "current", setup.Imax_ref );
        setStatus( "calibrate: done" );
        log<text_log>( "calibrate wrote package to " + m_caldir +
                       " (control cached in memory)" );

        return 0;
    }
    catch( const lina::Cancelled & )
    {
        try
        {
            lina::ShmimStream dm( m_dmChan );
            dm.zero();
        }
        catch( ... )
        {
        }
        updateIfChanged( m_indiP_calibMode, "current", 0.0 );
        setStatus( "calibrate: stopped" );
        log<text_log>( "calibrate stopped by user; DM cleared, matrix not saved" );
        return 0;
    }
    catch( const std::exception &e )
    {
        setStatus( "calibrate: failed" );
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "calibrate: " ) + e.what() } );
    }
}

int iefcCtrl::doRun()
{
    setStatus( "run: starting" );
    try
    {
        lina::ShmimStream camsci( m_camsciName );
        lina::ShmimStream dm( m_dmChan );

        if( m_setExptime )
        {
            if( setExptimeValue( m_exptime ) < 0 )
                return -1;
        }
        double live_exptime = m_exptime;
        if( openExptime() == 0 )
        {
            live_exptime = ( m_exptimeShm.md->datatype == _DATATYPE_DOUBLE )
                               ? ( (double *)m_exptimeShm.array.raw )[0]
                               : (double)( (float *)m_exptimeShm.array.raw )[0];
        }

        auto in = lina::default_loop_inputs( camsci.rows(), dm.rows() );
        in.nframes = m_nFrames;
        in.wait_frames = m_waitFrames;
        in.delay_s = m_delay_s;
        in.reg_cond = m_regCond;
        in.run_probe_amp = m_clProbeAmp;
        in.num_iters = m_iters;
        in.gain = m_loopGain;
        in.leakage = m_leakage;

        lina::Array2D<double> control;
        lina::SetupData setupData;
        bool used_cache = false;

        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            if( m_haveCalibration )
            {
                in.probe_modes = m_cachedProbeModes;
                in.calib_modes = m_cachedCalibModes;
                in.control_mask = m_cachedMask;
                control = m_cachedControl;
                setupData.loaded = true;
                setupData.dark = m_cachedDark;
                setupData.Imax_ref = m_cachedImaxRef;
                setupData.psf_exptime = m_cachedPsfExptime;
                setupData.gain = m_cachedGain;
                setupData.dark_exptime = m_cachedPsfExptime;
                used_cache = true;
            }
        }

        if( !used_cache )
        {
            setStatus( "run: loading calibration package" );
            lina::PackagePaths pkg;
            pkg.dir = m_caldir;
            lina::load_modes_from_package( in, pkg );

            if( !m_setupdir.empty() )
                setupData = lina::load_setup_dir( m_setupdir, camsci.rows(), live_exptime );
            else
                setupData = lina::load_setup_from_package( pkg, camsci.rows(), live_exptime );
            if( !setupData.loaded )
                return log<software_error, -1>( { __FILE__, __LINE__, "setup/dark not loaded" } );

            auto response = lina::load_matrix( pkg.response_path() );
            try
            {
                control = lina::load_matrix( pkg.control_path() );
            }
            catch( const std::exception & )
            {
                setStatus( "run: computing control matrix (beta_reg)" );
                control = lina::beta_reg_cpu( lina::transpose( response ), in.reg_cond );
            }

            {
                std::lock_guard<std::mutex> lock( m_calMutex );
                m_cachedResponse = std::move( response );
                m_cachedControl = control;
                m_cachedProbeModes = in.probe_modes;
                m_cachedCalibModes = in.calib_modes;
                m_cachedMask = in.control_mask;
                m_cachedDark = setupData.dark;
                m_cachedImaxRef = setupData.Imax_ref;
                m_cachedPsfExptime = setupData.psf_exptime;
                m_cachedGain = setupData.gain;
                m_haveCalibration = true;
            }
            log<text_log>( "run: loaded calibration package into memory from " + m_caldir );
        }
        else
        {
            log<text_log>( "run: using in-memory calibration matrices" );
            // Refresh dark for current live exptime from setupdir when available.
            if( !m_setupdir.empty() )
            {
                try
                {
                    auto live_setup =
                        lina::load_setup_dir( m_setupdir, camsci.rows(), live_exptime );
                    setupData.dark = live_setup.dark;
                    setupData.dark_exptime = live_setup.dark_exptime;
                    if( live_setup.Imax_ref > 0.0 )
                        setupData.Imax_ref = live_setup.Imax_ref;
                }
                catch( const std::exception &e )
                {
                    log<text_log>( std::string( "run: setupdir dark refresh skipped: " ) +
                                       e.what(),
                                   logPrio::LOG_WARNING );
                }
            }
        }

        lina::apply_setup( in, setupData, live_exptime );
        updateLiveNormFromSetup( setupData );
        m_liveContrastMask = in.control_mask;
        m_haveContrastMask = true;

        updateIfChanged( m_indiP_nCalibModes, "current",
                         static_cast<double>( in.calib_modes.rows() ) );
        updateIfChanged( m_indiP_calibMode, "current", 0.0 );

        lina::IefcData data;
        auto current = dm.grab_latest();
        for( size_t i = 0; i < current.size(); ++i )
            current.data()[i] *= in.dm_scale;
        data.commands.push_back( current );

        setStatus( "run: closed loop" );
        lina::run( data, camsci, in.nframes, dm, in.im_params, in.ref_params, setupData.dark,
                   control, in.run_probe_amp, in.probe_modes, in.calib_modes, in.control_mask,
                   in.delay_s, in.num_iters, in.gain, in.leakage, in.dm_scale, in.wait_frames,
                   makeStopCheck() );

        if( !data.contrasts.empty() )
            updateIfChanged( m_indiP_contrast, "current", data.contrasts.back() );
        updateIfChanged( m_indiP_Imax_ref, "current", setupData.Imax_ref );

        for( size_t i = 0; i < data.contrasts.size(); ++i )
            log<text_log>( "run iter" + std::to_string( i ) + " contrast="
                           + std::to_string( data.contrasts[i] ) );

        setStatus( "run: done" );
        return 0;
    }
    catch( const lina::Cancelled & )
    {
        setStatus( "run: stopped" );
        log<text_log>( "run stopped by user; leaving last DM command" );
        return 0;
    }
    catch( const std::exception &e )
    {
        setStatus( "run: failed" );
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "run: " ) + e.what() } );
    }
}

// ----------------- INDI callbacks -----------------

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camsci )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camsci, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_camsci, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_camsciName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change camsci while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_camsci, "current", m_camsciName );
        updateIfChanged( m_indiP_camsci, "target", m_camsciName );
        return 0;
    }
    log<text_log>( "camsci shmim: " + m_camsciName + " -> " + target );
    m_camsciName = target;
    updateIfChanged( m_indiP_camsci, "current", m_camsciName );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_dmChannel )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dmChannel, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_dmChannel, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_dmChan )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change dmChannel while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_dmChannel, "current", m_dmChan );
        updateIfChanged( m_indiP_dmChannel, "target", m_dmChan );
        return 0;
    }
    log<text_log>( "dmChannel: " + m_dmChan + " -> " + target );
    m_dmChan = target;
    updateIfChanged( m_indiP_dmChannel, "current", m_dmChan );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmChannel )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmChannel, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_fsmChannel, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_fsmChan )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change fsmChannel while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_fsmChannel, "current", m_fsmChan );
        updateIfChanged( m_indiP_fsmChannel, "target", m_fsmChan );
        return 0;
    }
    log<text_log>( "fsmChannel: " + m_fsmChan + " -> " + target );
    m_fsmChan = target;
    updateIfChanged( m_indiP_fsmChannel, "current", m_fsmChan );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shutterShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shutterShmim, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shutterShmim, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shutterName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shutterShmim while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shutterShmim, "current", m_shutterName );
        updateIfChanged( m_indiP_shutterShmim, "target", m_shutterName );
        return 0;
    }
    log<text_log>( "shutterShmim: " + m_shutterName + " -> " + target );
    m_shutterName = target;
    updateIfChanged( m_indiP_shutterShmim, "current", m_shutterName );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shutter )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shutter, ipRecv );
    if( !ipRecv.find( "toggle" ) )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot toggle shutter while IEFC job busy", logPrio::LOG_WARNING );
        return 0;
    }
    if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_shutter, "toggle", pcf::IndiElement::On, INDI_BUSY );
        if( setShutterClosed( true ) < 0 )
            return -1;
    }
    else if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::Off )
    {
        updateSwitchIfChanged( m_indiP_shutter, "toggle", pcf::IndiElement::Off, INDI_BUSY );
        if( setShutterClosed( false ) < 0 )
            return -1;
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_exptimeShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_exptimeShmim, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_exptimeShmim, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_exptimeName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change exptimeShmim while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_exptimeShmim, "current", m_exptimeName );
        updateIfChanged( m_indiP_exptimeShmim, "target", m_exptimeName );
        return 0;
    }
    log<text_log>( "exptimeShmim: " + m_exptimeName + " -> " + target );
    m_exptimeName = target;
    updateIfChanged( m_indiP_exptimeShmim, "current", m_exptimeName );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_gainShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_gainShmim, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_gainShmim, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_gainName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change gainShmim while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_gainShmim, "current", m_gainName );
        updateIfChanged( m_indiP_gainShmim, "target", m_gainName );
        return 0;
    }
    log<text_log>( "gainShmim: " + m_gainName + " -> " + target );
    m_gainName = target;
    updateIfChanged( m_indiP_gainShmim, "current", m_gainName );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_subNorm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_subNorm, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_subNorm, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_subNormName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change camsci_sub_norm while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_subNorm, "current", m_subNormName );
        updateIfChanged( m_indiP_subNorm, "target", m_subNormName );
        return 0;
    }
    log<text_log>( "camsci_sub_norm: " + m_subNormName + " -> " + target );
    m_subNormName = target;
    updateIfChanged( m_indiP_subNorm, "current", m_subNormName );
    if( m_subNormCreated )
    {
        ImageStreamIO_closeIm( &m_subNorm );
        m_subNormCreated = false;
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_contrastAvgShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_contrastAvgShmim, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_contrastAvgShmim, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_contrastAvgName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change contrast_avg shmim while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_contrastAvgShmim, "current", m_contrastAvgName );
        updateIfChanged( m_indiP_contrastAvgShmim, "target", m_contrastAvgName );
        return 0;
    }
    log<text_log>( "contrast_avg shmim: " + m_contrastAvgName + " -> " + target );
    m_contrastAvgName = target;
    updateIfChanged( m_indiP_contrastAvgShmim, "current", m_contrastAvgName );
    if( m_contrastAvgCreated )
    {
        ImageStreamIO_closeIm( &m_contrastAvg );
        m_contrastAvgCreated = false;
    }
    return 0;
}



INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_nFrames )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nFrames, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nFrames, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_nFrames )
        log<text_log>( "nFrames: " + std::to_string( m_nFrames ) + " -> " + std::to_string( target ) );
    m_nFrames = target;
    updateIfChanged( m_indiP_nFrames, "current", m_nFrames );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_waitFrames )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_waitFrames, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_waitFrames, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_waitFrames )
        log<text_log>( "waitFrames: " + std::to_string( m_waitFrames ) + " -> " +
                       std::to_string( target ) );
    m_waitFrames = target;
    updateIfChanged( m_indiP_waitFrames, "current", m_waitFrames );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_delay_s )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_delay_s, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_delay_s, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_delay_s )
        log<text_log>( "delay_s: " + std::to_string( m_delay_s ) + " -> " + std::to_string( target ) );
    m_delay_s = target;
    updateIfChanged( m_indiP_delay_s, "current", m_delay_s );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_exptime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_exptime, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_exptime, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_exptime )
        log<text_log>( "exptime target: " + std::to_string( m_exptime ) + " -> " +
                       std::to_string( target ) + " s" );
    m_exptime = target;
    m_setExptime = true;
    updateIfChanged( m_indiP_exptime, "current", m_exptime );
    // Always try to write milk when idle so camsciexptime tracks INDI.
    if( m_busy.load() )
    {
        log<text_log>( "exptime stored; will apply when idle / next job",
                       logPrio::LOG_WARNING );
        return 0;
    }
    return setExptimeValue( m_exptime );
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfExptime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfExptime, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_psfExptime, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_psfExptime )
        log<text_log>( "psf_exptime: " + std::to_string( m_psfExptime ) + " -> " +
                       std::to_string( target ) + " s" );
    m_psfExptime = target;
    updateIfChanged( m_indiP_psfExptime, "current", m_psfExptime );
    // Preview on camera when idle (same milk channel as exptime).
    if( m_busy.load() )
    {
        log<text_log>( "psf_exptime stored; applied at start of doRefPsf",
                       logPrio::LOG_WARNING );
        return 0;
    }
    return setPsfExptimeValue( m_psfExptime );
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_outdir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_outdir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_outdir, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_outdir )
        log<text_log>( "outdir: " + m_outdir + " -> " + target );
    m_outdir = target;
    updateIfChanged( m_indiP_outdir, "current", m_outdir );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_setupdir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_setupdir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_setupdir, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_setupdir )
        log<text_log>( "setupdir: " + m_setupdir + " -> " + target );
    m_setupdir = target;
    updateIfChanged( m_indiP_setupdir, "current", m_setupdir );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_caldir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_caldir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_caldir, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_caldir )
        log<text_log>( "caldir: " + m_caldir + " -> " + target );
    m_caldir = target;
    updateIfChanged( m_indiP_caldir, "current", m_caldir );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmAmp_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmAmp_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmAmp_nm, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_fsmAmp_nm )
        log<text_log>( "fsmAmp_nm: " + std::to_string( m_fsmAmp_nm ) + " -> " +
                       std::to_string( target ) + " (also sets tip/tilt)" );
    m_fsmAmp_nm = target;
    m_fsmTip_nm = target;
    m_fsmTilt_nm = target;
    updateIfChanged( m_indiP_fsmAmp_nm, "current", m_fsmAmp_nm );
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
    if( target != m_fsmTip_nm )
        log<text_log>( "fsmTip_nm: " + std::to_string( m_fsmTip_nm ) + " -> " +
                       std::to_string( target ) );
    m_fsmTip_nm = target;
    updateIfChanged( m_indiP_fsmTip_nm, "current", m_fsmTip_nm );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmTilt_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmTilt_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmTilt_nm, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_fsmTilt_nm )
        log<text_log>( "fsmTilt_nm: " + std::to_string( m_fsmTilt_nm ) + " -> " +
                       std::to_string( target ) );
    m_fsmTilt_nm = target;
    updateIfChanged( m_indiP_fsmTilt_nm, "current", m_fsmTilt_nm );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmRefTip_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmRefTip_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmRefTip_nm, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_fsmRefTip_nm )
        log<text_log>( "fsmRefTip_nm: " + std::to_string( m_fsmRefTip_nm ) + " -> " +
                       std::to_string( target ) );
    m_fsmRefTip_nm = target;
    updateIfChanged( m_indiP_fsmRefTip_nm, "current", m_fsmRefTip_nm );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_fsmRefTilt_nm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmRefTilt_nm, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_fsmRefTilt_nm, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_fsmRefTilt_nm )
        log<text_log>( "fsmRefTilt_nm: " + std::to_string( m_fsmRefTilt_nm ) + " -> " +
                       std::to_string( target ) );
    m_fsmRefTilt_nm = target;
    updateIfChanged( m_indiP_fsmRefTilt_nm, "current", m_fsmRefTilt_nm );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_nDark )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nDark, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nDark, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_nDark )
        log<text_log>( "nDark: " + std::to_string( m_nDark ) + " -> " + std::to_string( target ) );
    m_nDark = target;
    updateIfChanged( m_indiP_nDark, "current", m_nDark );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_nPsf )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nPsf, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nPsf, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_nPsf )
        log<text_log>( "nPsf: " + std::to_string( m_nPsf ) + " -> " + std::to_string( target ) );
    m_nPsf = target;
    updateIfChanged( m_indiP_nPsf, "current", m_nPsf );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_exptimes )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_exptimes, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_exptimes, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_exptimesCsv )
        log<text_log>( "exptimes: [" + m_exptimesCsv + "] -> [" + target + "]" );
    m_exptimesCsv = target;
    // Soft param: adopt immediately so GUIs see current == target.
    updateIfChanged( m_indiP_exptimes, "current", m_exptimesCsv );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_regCond )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_regCond, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_regCond, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_regCond )
        log<text_log>( "regCond: " + std::to_string( m_regCond ) + " -> " + std::to_string( target ) );
    m_regCond = target;
    updateIfChanged( m_indiP_regCond, "current", m_regCond );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calibProbeAmp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calibProbeAmp, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_calibProbeAmp, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_calibProbeAmp )
        log<text_log>( "calib_probe_amp: " + std::to_string( m_calibProbeAmp ) + " -> " +
                       std::to_string( target ) );
    m_calibProbeAmp = target;
    updateIfChanged( m_indiP_calibProbeAmp, "current", m_calibProbeAmp );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calibModeAmp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calibModeAmp, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_calibModeAmp, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_calibAmp )
        log<text_log>( "calib_mode_amp: " + std::to_string( m_calibAmp ) + " -> " +
                       std::to_string( target ) );
    m_calibAmp = target;
    updateIfChanged( m_indiP_calibModeAmp, "current", m_calibAmp );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clProbeAmp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clProbeAmp, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_clProbeAmp, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_clProbeAmp )
        log<text_log>( "cl_probe_amp: " + std::to_string( m_clProbeAmp ) + " -> " +
                       std::to_string( target ) );
    m_clProbeAmp = target;
    updateIfChanged( m_indiP_clProbeAmp, "current", m_clProbeAmp );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_contrastAvgN )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_contrastAvgN, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_contrastAvgN, target, ipRecv, false ) < 0 )
        return -1;
    if( target < 1 )
        target = 1;
    if( target != m_contrastAvgN )
        log<text_log>( "contrast_avg_n: " + std::to_string( m_contrastAvgN ) + " -> " +
                       std::to_string( target ) );
    m_contrastAvgN = target;
    // Trim window immediately if shortened.
    while( m_contrastSamples.size() > m_contrastAvgN )
    {
        m_contrastSampleSum -= m_contrastSamples.front();
        m_contrastSamples.pop_front();
    }
    updateIfChanged( m_indiP_contrastAvgN, "current", m_contrastAvgN );
    if( !m_contrastSamples.empty() )
    {
        const double avg =
            m_contrastSampleSum / static_cast<double>( m_contrastSamples.size() );
        if( ensureContrastAvgStream() == 0 )
            writeScalar( m_contrastAvg, avg );
        updateIfChanged( m_indiP_contrastAvg, "current", avg );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_iters )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_iters, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_iters, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_iters )
        log<text_log>( "iters: " + std::to_string( m_iters ) + " -> " + std::to_string( target ) );
    m_iters = target;
    updateIfChanged( m_indiP_iters, "current", m_iters );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_loopGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_loopGain, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_loopGain, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_loopGain )
        log<text_log>( "loopGain: " + std::to_string( m_loopGain ) + " -> " +
                       std::to_string( target ) );
    m_loopGain = target;
    updateIfChanged( m_indiP_loopGain, "current", m_loopGain );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_leakage )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_leakage, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_leakage, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_leakage )
        log<text_log>( "leakage: " + std::to_string( m_leakage ) + " -> " +
                       std::to_string( target ) );
    m_leakage = target;
    updateIfChanged( m_indiP_leakage, "current", m_leakage );
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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clearDm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clearDm, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_clearDm, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::ClearDm );
        clearRequest( m_indiP_clearDm );
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
