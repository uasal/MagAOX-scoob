/** \file iefcCtrl.hpp
  * \brief MagAO-X INDI front-end for Stream IEFC (ref PSF, dark library, calibrate, run).
  *
  * All of refPSF, darkLibrary, calibrate, and run execute natively in-process against
  * milk ImageStreamIO shmims via the vendored lina IEFC library. No external binary.
  *
  * Shared INDI numbers (cam_n_frame_delay / cam_r_delay, cam_exp, cam_gain, …) are reused across
  * all actions. Request switches trigger one-shot worker jobs.
  *
  * \ingroup iefcCtrl_files
  */

#ifndef iefcCtrl_hpp
#define iefcCtrl_hpp

#include <atomic>
#include <climits>
#include <cstdint>
#include <chrono>
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
        DmReset,
        RecomputeControl, ///< beta_reg from cached response with current cal_reg_cond
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
    std::string m_shmCamInput{ "camsci" };
    std::string m_shmDm{ "dm01disp07" };
    std::string m_shmFsm{ "dm00disp01" };
    std::string m_shutterName{ "camscishutter" };
    std::string m_camExpShmim{ "camsciexptime" };
    std::string m_camGainShmim{ "camscigain" };

    std::string m_psfDir{ "./ref_psf" };  ///< Ref-PSF / dark / Imax package (write+read)
    std::string m_calDir{ "./cal_a" };    ///< Calibration package (response/control matrices)
    std::string m_dhMaskPath; ///< External FITS mask path for dh_mask_reload (full path or filename)
    std::string m_satMaskPath; ///< FITS sat-check region for calibrate (raw ADU)
    float m_satThresh{ 55000.0f }; ///< Raw ADU threshold inside sat_mask (≥ → abort cal)

    unsigned m_calNImages{ 5 }; ///< Frames averaged per grab during calibrate
    /// Camera settle after DM write: use frame delay OR wall-clock delay (mutually exclusive).
    unsigned m_camNFrameDelay{ 1 };  ///< Skip this many new camsci frames (0 = use cam_r_delay)
    float m_camRDelay{ 0.0f };      ///< Wall-clock settle [s] (used only when cam_n_frame_delay==0)

    float m_psfFsmPokeTip{ 1000.0f };
    float m_psfFsmPokeTilt{ 1000.0f };
    float m_psfFsmRefTip{ 0.0f };
    float m_psfFsmRefTilt{ 0.0f };
    float m_psfFsmRefPiston{ 0.0f };
    unsigned m_darkNImages{ 20 };
    unsigned m_psfNFrames{ 20 };
    float m_settle_s{ 0.5f };

    std::string m_darkExptimesCsv{ "0.5,1,2,5" };
    float m_camExp{ 1.0f };       ///< Target exposure [s]; writes cam_exp_shmim when set
    float m_camGain{ 0.0f };      ///< Target gain; writes cam_gain_shmim when set
    bool m_setCamExp{ false };    ///< True after INDI cam_exp was set (calibrate/run)
    bool m_setCamGain{ false };   ///< True after INDI cam_gain was set (calibrate/run)

    float m_calRegCond{ -2.5f };
    float m_clProbeAmp{ 1e-9f };     ///< Closed-loop probe amp [m] (INDI cl_probe_amp)
    float m_calProbeAmp{ 5e-9f };  ///< Calib probe amp [m] (INDI cal_probe_amp)
    float m_calModeAmp{ 2e-9f };       ///< Calib mode poke amp [m] (INDI cal_mode_amp)
    unsigned m_clIters{ 3 };
    float m_clLoopGain{ 1.0f };
    float m_clLeakage{ 0.0f };

    std::string m_shmCamSubNorm{ "camsci_sub_norm" };
    std::string m_contrastAvgName{ "contrast_avg" };
    std::string m_shmDhMaskName{ "iefc_mask" }; ///< Live WFS/control mask image for verification
    std::string m_shmSatMaskName{ "iefc_sat_mask" }; ///< Saturation-check mask image
    unsigned m_clContrastAvgN{ 10 }; ///< NI frames avg for contrast / cl_run grab cadence
    bool m_saveResponseFull{ true }; ///< Save full-frame response (needed to remask on dh_mask_reload)
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
    lina::Array2D<double> m_cachedResponseFull; ///< (nmodes, nprobes*npix); for remask
    lina::Array2D<double> m_cachedControl;    ///< (nmodes, nmeas) after beta_reg
    float m_cachedRegCond{ std::numeric_limits<float>::quiet_NaN() }; ///< reg used for m_cachedControl
    lina::Array2D<double> m_cachedProbeModes;
    lina::Array2D<double> m_cachedCalibModes;
    lina::Array2D<std::uint8_t> m_cachedMask;
    bool m_haveUserDhMask{ false }; ///< True after dh_mask_reload (prefer for next calibrate)
    lina::Array2D<double> m_cachedDark;
    double m_cachedImaxRef{ 0.0 };
    double m_cachedPsfExptime{ 1.0 };
    double m_cachedGain{ 0.0 };
    ///@}

    /** \name shm_cam_sub_norm continuous stream
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
    std::atomic<bool> m_imaxRefManual{ false }; ///< True after INDI Imax_ref set; preserve across dark reloads
    ///@}

    /** \name Continuous contrast (avg N NI frames, then mean of mask ∩ NI>0)
      *@{
      */
    IMAGE m_contrastAvg{};
    bool m_contrastAvgCreated{ false };
    std::mutex m_contrastAvgMutex;
    lina::Array2D<double> m_contrastSumIm; ///< Accumulator for current NI block
    unsigned m_contrastFrameCount{ 0 };    ///< Frames in m_contrastSumIm
    lina::Array2D<std::uint8_t> m_liveContrastMask;
    bool m_haveContrastMask{ false };
    IMAGE m_shmDhMask{};
    bool m_shmDhMaskCreated{ false };
    lina::Array2D<std::uint8_t> m_satMask;
    bool m_haveSatMask{ false };
    IMAGE m_shmSatMask{};
    bool m_shmSatMaskCreated{ false };
    ///@}

    /** \name Open shmims (opened on demand per job)
      *@{
      */
    IMAGE m_camsci{};
    IMAGE m_dm{};
    IMAGE m_fsm{};
    IMAGE m_shutter{};
    IMAGE m_camExpShm{};
    IMAGE m_camGainShm{};
    bool m_camsciOpen{ false };
    bool m_dmOpen{ false };
    bool m_fsmOpen{ false };
    bool m_shutterOpen{ false };
    bool m_camExpOpen{ false };
    bool m_camGainOpen{ false };
    int m_camsciSem{ -1 };
    ///@}

    /** \name INDI — shmim names (repointable)
      *@{
      */
    pcf::IndiProperty m_indiP_shmCamInput;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmCamInput);

    pcf::IndiProperty m_indiP_shmDm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmDm);

    pcf::IndiProperty m_indiP_shmFsm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmFsm);

    pcf::IndiProperty m_indiP_shmShutter;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmShutter );

    pcf::IndiProperty m_indiP_shutter; // toggle: On=closed, Off=open
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shutter );

    /// Milk name for exposure-time scalar (not the numeric exposure value).
    pcf::IndiProperty m_indiP_camExpShmim;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camExpShmim);

    /// Milk name for camera-gain scalar (not cl_loop_gain).
    pcf::IndiProperty m_indiP_camGainShmim;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camGainShmim);

    pcf::IndiProperty m_indiP_shmCamSubNorm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmCamSubNorm);

    pcf::IndiProperty m_indiP_shmContrastAvg;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmContrastAvg);

    pcf::IndiProperty m_indiP_shmDhMask;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmDhMask);

    pcf::IndiProperty m_indiP_shmSatMask;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmSatMask);
    ///@}

    /** \name INDI — shared
      *@{
      */
    pcf::IndiProperty m_indiP_calNImages;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calNImages);

    pcf::IndiProperty m_indiP_camNFrameDelay;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camNFrameDelay);

    pcf::IndiProperty m_indiP_camRDelay;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camRDelay);

    pcf::IndiProperty m_indiP_camExp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camExp);

    pcf::IndiProperty m_indiP_camGain;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camGain);

    pcf::IndiProperty m_indiP_psfDir;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfDir);

    pcf::IndiProperty m_indiP_calDir;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calDir);

    pcf::IndiProperty m_indiP_dhMaskPath;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dhMaskPath);

    pcf::IndiProperty m_indiP_satMaskPath;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_satMaskPath);

    pcf::IndiProperty m_indiP_satThresh;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_satThresh);

    pcf::IndiProperty m_indiP_psfMaxRef; ///< current/target — manual set overrides until cal/refPSF
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfMaxRef);
    ///@}

    /** \name INDI — ref PSF / FSM
      *@{
      */
    pcf::IndiProperty m_indiP_psfFsmPokeTip;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfFsmPokeTip);

    pcf::IndiProperty m_indiP_psfFsmPokeTilt;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfFsmPokeTilt);

    pcf::IndiProperty m_indiP_psfFsmRefTip;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfFsmRefTip);

    pcf::IndiProperty m_indiP_psfFsmRefTilt;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfFsmRefTilt);

    pcf::IndiProperty m_indiP_darkNImages;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_darkNImages);

    pcf::IndiProperty m_indiP_psfNFrames;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfNFrames);

    pcf::IndiProperty m_indiP_darkExptimes;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_darkExptimes);
    ///@}

    /** \name INDI — calibrate / run
      *@{
      */
    pcf::IndiProperty m_indiP_calRegCond;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calRegCond);

    pcf::IndiProperty m_indiP_calProbeAmp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calProbeAmp);

    pcf::IndiProperty m_indiP_calModeAmp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calModeAmp);

    pcf::IndiProperty m_indiP_clProbeAmp;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clProbeAmp);

    pcf::IndiProperty m_indiP_clIters;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clIters);

    pcf::IndiProperty m_indiP_clLoopGain;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clLoopGain);

    pcf::IndiProperty m_indiP_clLeakage;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clLeakage);

    pcf::IndiProperty m_indiP_clContrastAvgN;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clContrastAvgN);
    ///@}

    /** \name INDI — requests + status
      *@{
      */
    pcf::IndiProperty m_indiP_psfTakeRef;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfTakeRef);

    pcf::IndiProperty m_indiP_darkLibBuild;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_darkLibBuild);

    pcf::IndiProperty m_indiP_calibrate;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calibrate);

    pcf::IndiProperty m_indiP_clRun;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clRun);

    pcf::IndiProperty m_indiP_dmReset;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dmReset);

    pcf::IndiProperty m_indiP_dhMaskReload;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dhMaskReload);

    pcf::IndiProperty m_indiP_stop;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_stop);

    pcf::IndiProperty m_indiP_status;   ///< RO text
    pcf::IndiProperty m_indiP_contrast; ///< RO number (last closed-loop iter)
    pcf::IndiProperty m_indiP_contrastAvg; ///< RO running average published to shmim
    pcf::IndiProperty m_indiP_calMode;   ///< RO: current calib mode (1..N, 0 idle)
    pcf::IndiProperty m_indiP_nCalModes; ///< RO: total calib modes in package / run
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
    int openCamExp();
    int openCamGain();
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
    int doDmReset();
    int doRecomputeControl(); ///< Load or build control for current cal_reg_cond
    int doDhMaskReload();      ///< Load WFS/control mask; remask+rebuild control from dir_cal

    /// Apply FITS mask as control+contrast; remask response / beta_reg when cal data exists.
    int applyDhMaskFromFits( const std::string &path );

    /// Warn (do not abort) if any raw pixel >= sat_thresh over the full frame.
    void warnIfSaturatedFullFrame( const std::vector<float> &im,
                                   uint32_t w,
                                   uint32_t h,
                                   const char *context );

    /// Load saturation-check mask from FITS; publish iefc_sat_mask.
    int applySatMaskFromFits( const std::string &path );

    /// Ensure sat mask loaded from sat_mask_path (if set) before calibrate.
    int ensureSatMaskLoaded();

    /// Apply manual Imax_ref and refresh live NI normalization scale.
    int setImaxRefValue( double imax );

    /// On-disk path: dir_cal/control_matrix_reg_<tag>.fits
    std::string controlMatrixPath( float reg ) const;

    /// Prefer memory → tagged FITS → beta_reg(+write). Updates m_cachedControl/RegCond.
    int loadOrBuildControl( const lina::Array2D<double> &response,
                            float reg,
                            lina::Array2D<double> &control_out );

    /// Write shutter milk scalar and publish INDI toggle (On=closed).
    int setShutterClosed( bool closed );

    /// Publish INDI shutter toggle from a known closed/open state (no SHM write).
    void publishShutterIndi( bool closed );

    /// Write cam_exp milk scalar and update INDI cam_exp current (not target).
    int setCamExpValue( double seconds );

    /// Write cam_gain milk scalar and update INDI cam_gain current (not target).
    int setCamGainValue( double gain );

    /// Read a scalar IMAGE (float/double/int) as double; returns NaN on unsupported type.
    static double readScalarValue( const IMAGE &im );

    /// Quietly open cam_exp / cam_gain shmims if present and mirror values into INDI current.
    void syncCamExpGainCurrent();

    /// Cache setup (dark + Imax) for continuous shm_cam_sub_norm.
    void updateLiveNormFromSetup( const lina::SetupData &setup );

    /// Ensure shm_cam_sub_norm shmim exists matching camera geometry.
    int ensureSubNormStream( uint32_t w, uint32_t h );

    /// If a new camsci frame is available, dark-sub + normalize and publish.
    int processSubNormFrame();

    /// Ensure control mask available for continuous contrast (cal cache / dir_cal / default).
    int ensureContrastMask( uint32_t w, uint32_t h );

    /// Ensure scalar contrast_avg shmim exists.
    int ensureContrastAvgStream();

    /// Ensure iefc_mask image shmim exists (for verifying dh_mask_reload).
    int ensureDhMaskStream( uint32_t w, uint32_t h );

    /// Publish binary mask (0/1 float) to iefc_mask shmim.
    int publishDhMask( const lina::Array2D<std::uint8_t> &mask );

    /// Ensure iefc_sat_mask image shmim exists.
    int ensureSatMaskStream( uint32_t w, uint32_t h );

    /// Publish binary sat mask (0/1 float) to iefc_sat_mask shmim.
    int publishSatMask( const lina::Array2D<std::uint8_t> &mask );

    /// Remask cached/disk response_full with mask and rebuild control for m_calRegCond.
    /// Returns 0 on success, 1 if skipped (no cal data), -1 on hard failure.
    int remaskControlFromCalibration( const lina::Array2D<std::uint8_t> &mask );

    /// Absolute path helper for logs (falls back to input on failure).
    static std::string absPath( const std::string &path );

    /// Accumulate one NI frame. Every cl_contrast_avg_n frames: publish mean →
    /// shm_cam_sub_norm and contrast(mean ∩ mask) → contrast_avg.
    int updateContrastFromNi( const lina::Array2D<double> &ni );

    /// Publish a float image to shm_cam_sub_norm (creates stream if needed).
    int publishSubNorm( const lina::Array2D<double> &im );

    /// Reset the NI-frame accumulator (e.g. when cl_contrast_avg_n changes).
    void resetContrastAccumulator();

    /// Format a double in scientific notation for logs.
    static std::string formatSci( double v, int precision = 6 );

    void setStatus( const std::string &s );
    void clearRequest( pcf::IndiProperty &p );

    /// Cooperative cancel predicate for lina calibrate/run/grab.
    lina::StopCheck makeStopCheck();

    /// Resolve mutually exclusive camera settle: frame count XOR wall-clock delay.
    void resolveCamSettle( std::size_t &wait_frames, double &delay_s ) const;
};

iefcCtrl::iefcCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
}

void iefcCtrl::setupConfig()
{
    config.add( "iefc.shm_cam_input", "", "iefc.shm_cam_input", argType::Required, "iefc",
                "shm_cam_input", false, "string",
                "Science-camera ImageStreamIO name (default camsci)." );
    config.add( "iefc.shm_dm", "", "iefc.shm_dm", argType::Required, "iefc", "shm_dm", false,
                "string", "IEFC DM channel shmim (default dm01disp07)." );
    config.add( "iefc.shm_fsm", "", "iefc.shm_fsm", argType::Required, "iefc", "shm_fsm", false,
                "string", "FSM DMcomb channel shmim (default dm00disp01)." );
    config.add( "iefc.shm_shutter", "", "iefc.shm_shutter", argType::Required, "iefc", "shm_shutter", false,
                "string", "Shutter scalar shmim name (INDI shm_shutter; default camscishutter)." );
    config.add( "iefc.cam_exp_shmim",
                "",
                "iefc.cam_exp_shmim",
                argType::Required,
                "iefc",
                "cam_exp_shmim",
                false,
                "string",
                "Exposure-time scalar shmim name (default camsciexptime)." );
    config.add( "iefc.cam_gain_shmim",
                "",
                "iefc.cam_gain_shmim",
                argType::Required,
                "iefc",
                "cam_gain_shmim",
                false,
                "string",
                "Camera-gain scalar shmim name (default camscigain)." );
    config.add( "iefc.cam_exp",
                "",
                "iefc.cam_exp",
                argType::Required,
                "iefc",
                "cam_exp",
                false,
                "float",
                "Target camera exposure [s]; writes cam_exp_shmim when set (current tracks shmim)." );
    config.add( "iefc.cam_gain",
                "",
                "iefc.cam_gain",
                argType::Required,
                "iefc",
                "cam_gain",
                false,
                "float",
                "Target camera gain; writes cam_gain_shmim when set (current tracks shmim)." );
    config.add( "iefc.cal_dir", "", "iefc.cal_dir", argType::Required, "iefc", "cal_dir", false,
                "string", "Calibration package dir (response/control matrices)." );
    config.add( "iefc.psf_dir", "", "iefc.psf_dir", argType::Required, "iefc", "psf_dir", false,
                "string",
                "Ref-PSF / dark-library / Imax package (written by doRefPsf/doDarkLibrary; "
                "read by calibrate/run)." );
    config.add( "iefc.dh_mask_path",
                "",
                "iefc.dh_mask_path",
                argType::Required,
                "iefc",
                "dh_mask_path",
                false,
                "string",
                "FITS path for dh_mask_reload (control+contrast; remasks dir_cal if present)." );
    config.add( "iefc.sat_mask_path",
                "",
                "iefc.sat_mask_path",
                argType::Required,
                "iefc",
                "sat_mask_path",
                false,
                "string",
                "FITS path for calibration saturation-check region (raw ADU)." );
    config.add( "iefc.sat_thresh",
                "",
                "iefc.sat_thresh",
                argType::Required,
                "iefc",
                "sat_thresh",
                false,
                "float",
                "Raw ADU threshold inside sat_mask (>= aborts calibrate)." );
    config.add( "iefc.cal_n_images", "", "iefc.cal_n_images", argType::Required, "iefc", "cal_n_images", false,
                "unsigned", "Frames averaged per grab during calibrate." );
    config.add( "iefc.cam_n_frame_delay",
                "",
                "iefc.cam_n_frame_delay",
                argType::Required,
                "iefc",
                "cam_n_frame_delay",
                false,
                "unsigned",
                "Skip N new camera frames after DM write (mutually exclusive with cam_r_delay)." );
    config.add( "iefc.cam_r_delay",
                "",
                "iefc.cam_r_delay",
                argType::Required,
                "iefc",
                "cam_r_delay",
                false,
                "float",
                "Wall-clock settle [s] after DM write (used only when cam_n_frame_delay==0)." );
    config.add( "iefc.psf_fsm_poke_tip", "", "iefc.psf_fsm_poke_tip", argType::Required, "iefc", "psf_fsm_poke_tip",
                false, "float", "FSM tip poke amplitude [nm] for psf_take_ref." );
    config.add( "iefc.psf_fsm_poke_tilt", "", "iefc.psf_fsm_poke_tilt", argType::Required, "iefc", "psf_fsm_poke_tilt",
                false, "float", "FSM tilt poke amplitude [nm] for psf_take_ref." );
    config.add( "iefc.psf_fsm_ref_tip", "", "iefc.psf_fsm_ref_tip", argType::Required, "iefc", "psf_fsm_ref_tip",
                false, "float", "FSM home tip [nm] for psf_take_ref." );
    config.add( "iefc.psf_fsm_ref_tilt", "", "iefc.psf_fsm_ref_tilt", argType::Required, "iefc", "psf_fsm_ref_tilt",
                false, "float", "FSM home tilt [nm] for psf_take_ref." );
    config.add( "iefc.dark_n_images", "", "iefc.dark_n_images", argType::Required, "iefc", "dark_n_images", false,
                "unsigned", "Dark frames for ref-PSF / dark-library." );
    config.add( "iefc.psf_n_frames", "", "iefc.psf_n_frames", argType::Required, "iefc", "psf_n_frames", false, "unsigned",
                "PSF frames for ref-PSF." );
    config.add( "iefc.dark_exptimes", "", "iefc.dark_exptimes", argType::Required, "iefc", "dark_exptimes", false,
                "string", "CSV exposure times for dark-library." );
    config.add( "iefc.cal_reg_cond",
                "",
                "iefc.cal_reg_cond",
                argType::Required,
                "iefc",
                "cal_reg_cond",
                false,
                "float",
                "beta_reg regularization when building the control matrix." );
    config.add( "iefc.cal_probe_amp",
                "",
                "iefc.cal_probe_amp",
                argType::Required,
                "iefc",
                "cal_probe_amp",
                false,
                "float",
                "Calibration probe amplitude [m]." );
    config.add( "iefc.cal_mode_amp",
                "",
                "iefc.cal_mode_amp",
                argType::Required,
                "iefc",
                "cal_mode_amp",
                false,
                "float",
                "Calibration mode poke amplitude [m]." );
    config.add( "iefc.cl_probe_amp", "", "iefc.cl_probe_amp", argType::Required, "iefc", "cl_probe_amp",
                false, "float", "Closed-loop probe amplitude [m]." );
    config.add( "iefc.cl_iters", "", "iefc.cl_iters", argType::Required, "iefc", "cl_iters", false,
                "unsigned", "Closed-loop iterations per run." );
    config.add( "iefc.cl_loop_gain",
                "",
                "iefc.cl_loop_gain",
                argType::Required,
                "iefc",
                "cl_loop_gain",
                false,
                "float",
                "Closed-loop gain." );
    config.add( "iefc.cl_leakage",
                "",
                "iefc.cl_leakage",
                argType::Required,
                "iefc",
                "cl_leakage",
                false,
                "float",
                "Closed-loop leakage." );
    config.add( "shmims.shm_cam_sub_norm",
                "",
                "shmims.shm_cam_sub_norm",
                argType::Required,
                "shmims",
                "shm_cam_sub_norm",
                false,
                "string",
                "Dark-sub + Imax-normalized camera stream name." );
    config.add( "shmims.shm_contrast_avg", "", "shmims.shm_contrast_avg", argType::Required, "shmims", "shm_contrast_avg", false,
                "string", "Running-average contrast scalar stream name (default contrast_avg)." );
    config.add( "shmims.shm_dh_mask", "", "shmims.shm_dh_mask", argType::Required, "shmims", "shm_dh_mask", false,
                "string", "Binary WFS/control mask image stream (default iefc_mask)." );
    config.add( "shmims.shm_sat_mask", "", "shmims.shm_sat_mask", argType::Required, "shmims",
                "shm_sat_mask", false, "string",
                "Binary saturation-check mask image stream (default iefc_sat_mask)." );
    config.add( "iefc.cl_contrast_avg_n", "", "iefc.cl_contrast_avg_n", argType::Required, "iefc", "cl_contrast_avg_n", false,
                "unsigned", "NI frames averaged for contrast / cl_run grabs (sets update cadence)." );
    config.add( "iefc.save_response_full", "", "iefc.save_response_full", argType::Required, "iefc", "save_response_full", false,
                "bool", "Write response_full.fits (multi-GB; needed to remask after restart)." );
}

void iefcCtrl::loadConfig()
{
    config( m_shmCamInput, "iefc.shm_cam_input" );
    config( m_shmDm, "iefc.shm_dm" );
    config( m_shmFsm, "iefc.shm_fsm" );
    config( m_shutterName, "iefc.shm_shutter" );
    config( m_camExpShmim, "iefc.cam_exp_shmim" );
    config( m_camGainShmim, "iefc.cam_gain_shmim" );
    config( m_camExp, "iefc.cam_exp" );
    config( m_camGain, "iefc.cam_gain" );
    config( m_calDir, "iefc.cal_dir" );
    config( m_psfDir, "iefc.psf_dir" );
    config( m_dhMaskPath, "iefc.dh_mask_path" );
    config( m_satMaskPath, "iefc.sat_mask_path" );
    config( m_satThresh, "iefc.sat_thresh" );
    config( m_calNImages, "iefc.cal_n_images" );
    config( m_camNFrameDelay, "iefc.cam_n_frame_delay" );
    config( m_camRDelay, "iefc.cam_r_delay" );
    config( m_psfFsmPokeTip, "iefc.psf_fsm_poke_tip" );
    config( m_psfFsmPokeTilt, "iefc.psf_fsm_poke_tilt" );
    config( m_psfFsmRefTip, "iefc.psf_fsm_ref_tip" );
    config( m_psfFsmRefTilt, "iefc.psf_fsm_ref_tilt" );
    config( m_darkNImages, "iefc.dark_n_images" );
    config( m_psfNFrames, "iefc.psf_n_frames" );
    config( m_darkExptimesCsv, "iefc.dark_exptimes" );
    config( m_calRegCond, "iefc.cal_reg_cond" );
    config( m_calProbeAmp, "iefc.cal_probe_amp" );
    config( m_calModeAmp, "iefc.cal_mode_amp" );
    config( m_clProbeAmp, "iefc.cl_probe_amp" );
    config( m_clIters, "iefc.cl_iters" );
    config( m_clLoopGain, "iefc.cl_loop_gain" );
    config( m_clLeakage, "iefc.cl_leakage" );
    config( m_shmCamSubNorm, "shmims.shm_cam_sub_norm" );
    config( m_contrastAvgName, "shmims.shm_contrast_avg" );
    config( m_shmDhMaskName, "shmims.shm_dh_mask" );
    config( m_shmSatMaskName, "shmims.shm_sat_mask" );
    config( m_clContrastAvgN, "iefc.cl_contrast_avg_n" );
    config( m_saveResponseFull, "iefc.save_response_full" );

    // Enforce mutual exclusivity: frame delay wins if both are set.
    if( m_camNFrameDelay > 0 && m_camRDelay > 0.0f )
    {
        log<text_log>( "cam_n_frame_delay and cam_r_delay both set; using frame delay only",
                       logPrio::LOG_WARNING );
        m_camRDelay = 0.0f;
    }
}

int iefcCtrl::appStartup()
{
    if( sem_init( &m_jobSem, 0, 0 ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "sem_init failed" } );
    }

    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmCamInput, "shm_cam_input", "Science camera input shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmDm, "shm_dm", "IEFC DM channel shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmFsm, "shm_fsm", "FSM channel shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmShutter, "shm_shutter", "Shutter scalar shmim", "shmims" );
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_shutter, "shutter" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_camExpShmim, "cam_exp_shmim", "Exposure-time scalar shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_camGainShmim, "cam_gain_shmim", "Camera-gain scalar shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmCamSubNorm, "shm_cam_sub_norm", "Dark-sub+norm camera stream", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmContrastAvg, "shm_contrast_avg", "Running-avg contrast shmim name", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmDhMask, "shm_dh_mask", "WFS/control mask image shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmSatMask, "shm_sat_mask", "Saturation-check mask image shmim", "shmims" );

    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_calNImages, "cal_n_images", 1, 10000, 1, "%u", "Frames averaged during calibrate", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_camNFrameDelay, "cam_n_frame_delay", 0, 1000, 1, "%u",
                                 "Skip N camsci frames after DM (XOR cam_r_delay)", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_camRDelay, "cam_r_delay", 0, 10, 0.01, "%0.3f",
                                 "Wall-clock settle after DM [s] (XOR cam_n_frame_delay)", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_camExp, "cam_exp", 0, 1000, 0.1, "%0.3f",
                                 "Target exposure [s] → cam_exp_shmim (current tracks shmim)", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_camGain, "cam_gain", -100, 100, 0.1, "%0.3f",
                                 "Target gain → cam_gain_shmim (current tracks shmim)", "shared" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_calDir, "cal_dir", "Calibration package dir (response/control)", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_psfDir, "psf_dir", "Ref-PSF / dark / Imax package dir", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_dhMaskPath, "dh_mask_path", "External WFS/control mask FITS path", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_satMaskPath, "sat_mask_path", "Saturation-check mask FITS path", "paths" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_satThresh, "sat_thresh", 0, 1e7, 1, "%0.1f",
                                "Raw ADU sat threshold in sat_mask", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_psfMaxRef, "psf_max_ref", 0, 1e12, 1, "%0.6g",
                                "Ref-PSF peak / NI normalization", "shared" );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_psfFsmPokeTip, "psf_fsm_poke_tip", -1e5, 1e5, 1, "%0.1f", "Tip poke [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_psfFsmPokeTilt, "psf_fsm_poke_tilt", -1e5, 1e5, 1, "%0.1f", "Tilt poke [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_psfFsmRefTip, "psf_fsm_ref_tip", -1e5, 1e5, 1, "%0.1f", "FSM home tip [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_psfFsmRefTilt, "psf_fsm_ref_tilt", -1e5, 1e5, 1, "%0.1f", "FSM home tilt [nm]", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_darkNImages, "dark_n_images", 1, 10000, 1, "%u", "Dark frames", "refPsf" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_psfNFrames, "psf_n_frames", 1, 10000, 1, "%u", "PSF frames", "refPsf" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_darkExptimes, "dark_exptimes", "Dark-library CSV exposures", "refPsf" );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calRegCond, "cal_reg_cond", -20, 0, 0.1, "%0.2f", "beta_reg for control matrix", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calProbeAmp, "cal_probe_amp", 0, 1e-6, 1e-10, "%0.3e", "Calib probe amp [m]", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calModeAmp, "cal_mode_amp", 0, 1e-6, 1e-10, "%0.3e", "Calib mode amp [m]", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_clProbeAmp, "cl_probe_amp", 0, 1e-6, 1e-10, "%0.3e", "Closed-loop probe amp [m]", "run" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_clIters, "cl_iters", 1, 1000, 1, "%u", "Closed-loop iterations", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_clLoopGain, "cl_loop_gain", 0, 2, 0.05, "%0.2f", "Closed-loop gain", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_clLeakage, "cl_leakage", 0, 1, 0.01, "%0.2f", "Closed-loop leakage", "run" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_clContrastAvgN, "cl_contrast_avg_n", 1, 10000, 1, "%u",
                                 "NI frames averaged for contrast / cl_run (sets cadence)", "run" );

    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_psfTakeRef, "psf_take_ref" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_darkLibBuild, "dark_lib_build" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_calibrate, "calibrate" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_clRun, "cl_run" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_dmReset, "dm_reset" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_dhMaskReload, "dh_mask_reload" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_stop, "stop" );

    REG_INDI_NEWPROP_NOCB( m_indiP_status, "status", pcf::IndiProperty::Text );
    m_indiP_status.add( pcf::IndiElement( "current" ) );
    m_indiP_status["current"].set( m_status );

    REG_INDI_NEWPROP_NOCB( m_indiP_contrast, "contrast", pcf::IndiProperty::Number );
    m_indiP_contrast.add( pcf::IndiElement( "current" ) );
    m_indiP_contrast["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_contrastAvg, "contrast_avg", pcf::IndiProperty::Number );
    m_indiP_contrastAvg.add( pcf::IndiElement( "current" ) );
    m_indiP_contrastAvg["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_calMode, "cal_mode", pcf::IndiProperty::Number );
    m_indiP_calMode.add( pcf::IndiElement( "current" ) );
    m_indiP_calMode["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_nCalModes, "n_cal_modes", pcf::IndiProperty::Number );
    m_indiP_nCalModes.add( pcf::IndiElement( "current" ) );
    m_indiP_nCalModes["current"].set( 0.0 );

    // Seed current/target from config (before INDI starts — use setValue, not updateIfChanged)
    m_indiP_shmCamInput["current"].setValue( m_shmCamInput );
    m_indiP_shmCamInput["target"].setValue( m_shmCamInput );
    m_indiP_shmDm["current"].setValue( m_shmDm );
    m_indiP_shmDm["target"].setValue( m_shmDm );
    m_indiP_shmFsm["current"].setValue( m_shmFsm );
    m_indiP_shmFsm["target"].setValue( m_shmFsm );
    m_indiP_shmShutter["current"].setValue( m_shutterName );
    m_indiP_shmShutter["target"].setValue( m_shutterName );
    // Seed shutter toggle Off (open) before INDI is up — use setSwitchState, not update*
    m_indiP_shutter["toggle"].setSwitchState( pcf::IndiElement::Off );
    m_indiP_camExpShmim["current"].setValue( m_camExpShmim );
    m_indiP_camExpShmim["target"].setValue( m_camExpShmim );
    m_indiP_camGainShmim["current"].setValue( m_camGainShmim );
    m_indiP_camGainShmim["target"].setValue( m_camGainShmim );
    m_indiP_shmCamSubNorm["current"].setValue( m_shmCamSubNorm );
    m_indiP_shmCamSubNorm["target"].setValue( m_shmCamSubNorm );
    m_indiP_shmContrastAvg["current"].setValue( m_contrastAvgName );
    m_indiP_shmContrastAvg["target"].setValue( m_contrastAvgName );
    m_indiP_shmDhMask["current"].setValue( m_shmDhMaskName );
    m_indiP_shmDhMask["target"].setValue( m_shmDhMaskName );
    m_indiP_shmSatMask["current"].setValue( m_shmSatMaskName );
    m_indiP_shmSatMask["target"].setValue( m_shmSatMaskName );
    m_indiP_calNImages["current"].setValue( m_calNImages );
    m_indiP_calNImages["target"].setValue( m_calNImages );
    m_indiP_camNFrameDelay["current"].setValue( m_camNFrameDelay );
    m_indiP_camNFrameDelay["target"].setValue( m_camNFrameDelay );
    m_indiP_camRDelay["current"].setValue( m_camRDelay );
    m_indiP_camRDelay["target"].setValue( m_camRDelay );
    m_indiP_camExp["current"].setValue( m_camExp );
    m_indiP_camExp["target"].setValue( m_camExp );
    m_indiP_camGain["current"].setValue( m_camGain );
    m_indiP_camGain["target"].setValue( m_camGain );
    m_indiP_calDir["current"].setValue( m_calDir );
    m_indiP_calDir["target"].setValue( m_calDir );
    m_indiP_psfDir["current"].setValue( m_psfDir );
    m_indiP_psfDir["target"].setValue( m_psfDir );
    m_indiP_dhMaskPath["current"].setValue( m_dhMaskPath );
    m_indiP_dhMaskPath["target"].setValue( m_dhMaskPath );
    m_indiP_satMaskPath["current"].setValue( m_satMaskPath );
    m_indiP_satMaskPath["target"].setValue( m_satMaskPath );
    m_indiP_satThresh["current"].setValue( m_satThresh );
    m_indiP_satThresh["target"].setValue( m_satThresh );
    m_indiP_psfMaxRef["current"].setValue( 0.0 );
    m_indiP_psfMaxRef["target"].setValue( 0.0 );
    m_indiP_psfFsmPokeTip["current"].setValue( m_psfFsmPokeTip );
    m_indiP_psfFsmPokeTip["target"].setValue( m_psfFsmPokeTip );
    m_indiP_psfFsmPokeTilt["current"].setValue( m_psfFsmPokeTilt );
    m_indiP_psfFsmPokeTilt["target"].setValue( m_psfFsmPokeTilt );
    m_indiP_psfFsmRefTip["current"].setValue( m_psfFsmRefTip );
    m_indiP_psfFsmRefTip["target"].setValue( m_psfFsmRefTip );
    m_indiP_psfFsmRefTilt["current"].setValue( m_psfFsmRefTilt );
    m_indiP_psfFsmRefTilt["target"].setValue( m_psfFsmRefTilt );
    m_indiP_darkNImages["current"].setValue( m_darkNImages );
    m_indiP_darkNImages["target"].setValue( m_darkNImages );
    m_indiP_psfNFrames["current"].setValue( m_psfNFrames );
    m_indiP_psfNFrames["target"].setValue( m_psfNFrames );
    m_indiP_darkExptimes["current"].setValue( m_darkExptimesCsv );
    m_indiP_darkExptimes["target"].setValue( m_darkExptimesCsv );
    m_indiP_calRegCond["current"].setValue( m_calRegCond );
    m_indiP_calRegCond["target"].setValue( m_calRegCond );
    m_indiP_calProbeAmp["current"].setValue( m_calProbeAmp );
    m_indiP_calProbeAmp["target"].setValue( m_calProbeAmp );
    m_indiP_calModeAmp["current"].setValue( m_calModeAmp );
    m_indiP_calModeAmp["target"].setValue( m_calModeAmp );
    m_indiP_clProbeAmp["current"].setValue( m_clProbeAmp );
    m_indiP_clProbeAmp["target"].setValue( m_clProbeAmp );
    m_indiP_clIters["current"].setValue( m_clIters );
    m_indiP_clIters["target"].setValue( m_clIters );
    m_indiP_clLoopGain["current"].setValue( m_clLoopGain );
    m_indiP_clLoopGain["target"].setValue( m_clLoopGain );
    m_indiP_clLeakage["current"].setValue( m_clLeakage );
    m_indiP_clLeakage["target"].setValue( m_clLeakage );
    m_indiP_clContrastAvgN["current"].setValue( m_clContrastAvgN );
    m_indiP_clContrastAvgN["target"].setValue( m_clContrastAvgN );

    // Mirror live camera settings into INDI current without writing shmims.
    syncCamExpGainCurrent();

    m_worker = std::thread( workerStart, this );
    m_subNormShutdown = false;
    m_subNormThread = std::thread( subNormStart, this );

    state( stateCodes::READY );

    log<text_log>( "iefcCtrl started (native jobs + shm_cam_sub_norm frame thread)" );
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

    // Mirror cam_exp / cam_gain shmims into INDI current (default loopPause = 1 s).
    syncCamExpGainCurrent();

    // shm_cam_sub_norm / contrast_avg run on m_subNormThread (semaphore-driven).
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
    log<text_log>( "shm_cam_sub_norm thread started" );
    int sem = -1;
    while( !m_subNormShutdown.load() )
    {
        if( !m_camsciOpen )
        {
            if( ImageStreamIO_openIm( &m_camsci, m_shmCamInput.c_str() ) != IMAGESTREAMIO_SUCCESS )
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
    log<text_log>( "shm_cam_sub_norm thread stopped" );
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
        updateIfChanged( m_indiP_calMode, "current", 0.0 );
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
        case Job::DmReset:
            return doDmReset();
        case Job::RecomputeControl:
            return doRecomputeControl();
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

void iefcCtrl::resolveCamSettle( std::size_t &wait_frames, double &delay_s ) const
{
    if( m_camNFrameDelay > 0 )
    {
        wait_frames = m_camNFrameDelay;
        delay_s = 0.0;
    }
    else
    {
        wait_frames = 0;
        delay_s = static_cast<double>( m_camRDelay );
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
    if( ImageStreamIO_openIm( &m_camsci, m_shmCamInput.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_shmCamInput } );
    }
    m_camsciOpen = true;
    m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
    return 0;
}

int iefcCtrl::openDm()
{
    if( m_dmOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_dm, m_shmDm.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_shmDm } );
    }
    m_dmOpen = true;
    return 0;
}

int iefcCtrl::openFsm()
{
    if( m_fsmOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_fsm, m_shmFsm.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_shmFsm } );
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

int iefcCtrl::openCamExp()
{
    if( m_camExpOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_camExpShm, m_camExpShmim.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_camExpShmim } );
    }
    m_camExpOpen = true;
    return 0;
}

int iefcCtrl::openCamGain()
{
    if( m_camGainOpen )
        return 0;
    if( ImageStreamIO_openIm( &m_camGainShm, m_camGainShmim.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to open " + m_camGainShmim } );
    }
    m_camGainOpen = true;
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
    if( m_camExpOpen )
    {
        ImageStreamIO_closeIm( &m_camExpShm );
        m_camExpOpen = false;
    }
    if( m_camGainOpen )
    {
        ImageStreamIO_closeIm( &m_camGainShm );
        m_camGainOpen = false;
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
    if( m_shmDhMaskCreated )
    {
        ImageStreamIO_closeIm( &m_shmDhMask );
        m_shmDhMaskCreated = false;
    }
    if( m_shmSatMaskCreated )
    {
        ImageStreamIO_closeIm( &m_shmSatMask );
        m_shmSatMaskCreated = false;
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

double iefcCtrl::readScalarValue( const IMAGE &im )
{
    if( !im.md )
        return std::numeric_limits<double>::quiet_NaN();
    if( im.md->datatype == _DATATYPE_DOUBLE )
        return ( (double *)im.array.raw )[0];
    if( im.md->datatype == _DATATYPE_FLOAT )
        return static_cast<double>( ( (float *)im.array.raw )[0] );
    if( im.md->datatype == _DATATYPE_INT32 )
        return static_cast<double>( ( (int32_t *)im.array.raw )[0] );
    if( im.md->datatype == _DATATYPE_UINT16 )
        return static_cast<double>( ( (uint16_t *)im.array.raw )[0] );
    return std::numeric_limits<double>::quiet_NaN();
}

void iefcCtrl::syncCamExpGainCurrent()
{
    if( !m_camExpOpen )
    {
        if( ImageStreamIO_openIm( &m_camExpShm, m_camExpShmim.c_str() ) == IMAGESTREAMIO_SUCCESS )
            m_camExpOpen = true;
    }
    if( m_camExpOpen )
    {
        const double v = readScalarValue( m_camExpShm );
        if( std::isfinite( v ) )
            updateIfChanged( m_indiP_camExp, "current", static_cast<float>( v ) );
    }

    if( !m_camGainOpen )
    {
        if( ImageStreamIO_openIm( &m_camGainShm, m_camGainShmim.c_str() ) == IMAGESTREAMIO_SUCCESS )
            m_camGainOpen = true;
    }
    if( m_camGainOpen )
    {
        const double v = readScalarValue( m_camGainShm );
        if( std::isfinite( v ) )
            updateIfChanged( m_indiP_camGain, "current", static_cast<float>( v ) );
    }
}

int iefcCtrl::setCamExpValue( double seconds )
{
    if( openCamExp() < 0 )
        return -1;
    log<text_log>( "cam_exp -> " + std::to_string( seconds ) + " s (shmim " + m_camExpShmim +
                   ", dtype=" + std::to_string( m_camExpShm.md->datatype ) + ")" );
    if( writeScalar( m_camExpShm, seconds ) < 0 )
        return -1;
    updateIfChanged( m_indiP_camExp, "current", static_cast<float>( seconds ) );
    return 0;
}

int iefcCtrl::setCamGainValue( double gain )
{
    if( openCamGain() < 0 )
        return -1;
    log<text_log>( "cam_gain -> " + std::to_string( gain ) + " (shmim " + m_camGainShmim +
                   ", dtype=" + std::to_string( m_camGainShm.md->datatype ) + ")" );
    if( writeScalar( m_camGainShm, gain ) < 0 )
        return -1;
    updateIfChanged( m_indiP_camGain, "current", static_cast<float>( gain ) );
    return 0;
}

void iefcCtrl::updateLiveNormFromSetup( const lina::SetupData &setup )
{
    if( !setup.loaded || setup.dark.size() == 0 )
        return;
    // When Imax was set manually via INDI, keep it; still require a positive Imax
    // from setup only when we would adopt it.
    if( !m_imaxRefManual && !( setup.Imax_ref > 0.0 ) )
        return;
    {
        std::lock_guard<std::mutex> lock( m_subNormMutex );
        m_liveDark = setup.dark;
        if( !m_imaxRefManual )
            m_liveImaxRef = setup.Imax_ref;
        m_livePsfExptime = ( setup.psf_exptime > 0.0 ) ? setup.psf_exptime : 1.0;
        m_liveGain = setup.gain;
        m_liveDarkExptime = setup.dark_exptime;
        m_haveLiveNorm = ( m_liveImaxRef > 0.0 );
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
    if( ImageStreamIO_openIm( &m_subNorm, m_shmCamSubNorm.c_str() ) == IMAGESTREAMIO_SUCCESS )
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
                                    m_shmCamSubNorm.c_str(),
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
            { __FILE__, __LINE__, "failed to create " + m_shmCamSubNorm } );
    }
    m_subNormCreated = true;
    log<text_log>( "created shmim " + m_shmCamSubNorm + " " + std::to_string( w ) + "x" +
                   std::to_string( h ) );
    return 0;
}

int iefcCtrl::processSubNormFrame()
{
    // Lazy-load setup for continuous norm if not yet available.
    if( !m_haveLiveNorm && !m_psfDir.empty() && !m_busy.load() )
    {
        try
        {
            double live_exptime = m_camExp;
            if( openCamExp() == 0 )
            {
                live_exptime = ( m_camExpShm.md->datatype == _DATATYPE_DOUBLE )
                                   ? ( (double *)m_camExpShm.array.raw )[0]
                                   : (double)( (float *)m_camExpShm.array.raw )[0];
            }
            std::size_t ncam = 0;
            if( !m_camsciOpen &&
                ImageStreamIO_openIm( &m_camsci, m_shmCamInput.c_str() ) == IMAGESTREAMIO_SUCCESS )
            {
                m_camsciOpen = true;
                m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
            }
            if( m_camsciOpen )
                ncam = static_cast<std::size_t>( m_camsci.md->size[0] );
            auto setup = lina::load_setup_dir( m_psfDir, ncam, live_exptime );
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
        if( ImageStreamIO_openIm( &m_camsci, m_shmCamInput.c_str() ) != IMAGESTREAMIO_SUCCESS )
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

    // Refresh exposure-matched dark when live exptime changes.
    double live_exptime = m_camExp;
    if( openCamExp() == 0 )
    {
        live_exptime = ( m_camExpShm.md->datatype == _DATATYPE_DOUBLE )
                           ? ( (double *)m_camExpShm.array.raw )[0]
                           : (double)( (float *)m_camExpShm.array.raw )[0];
    }
    if( !m_psfDir.empty() &&
        ( m_liveDarkExptime < 0.0 ||
          std::fabs( live_exptime - m_liveDarkExptime ) > lina::kDarkExptimeMatchTol ) &&
        !m_busy.load() )
    {
        try
        {
            auto setup = lina::load_setup_dir( m_psfDir, w, live_exptime );
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

    // Single pipeline: accumulate NI; every cl_contrast_avg_n frames publish the
    // block mean to shm_cam_sub_norm and compute contrast from that same mean.
    if( ensureContrastMask( w, h ) == 0 )
        updateContrastFromNi( ni );
    return 0;
}

int iefcCtrl::ensureContrastMask( uint32_t w, uint32_t h )
{
    if( m_haveContrastMask && m_liveContrastMask.rows() == w &&
        m_liveContrastMask.cols() == h )
        return 0;

    // Prefer in-memory calibration / user-loaded control mask.
    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        if( m_cachedMask.rows() == w && m_cachedMask.cols() == h &&
            m_cachedMask.size() > 0 )
        {
            m_liveContrastMask = m_cachedMask;
            m_haveContrastMask = true;
            return 0;
        }
    }

    // Try dir_cal package wfs_mask only (avoid requiring full modes package).
    if( !m_calDir.empty() )
    {
        try
        {
            lina::PackagePaths pkg;
            pkg.dir = m_calDir;
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

std::string iefcCtrl::absPath( const std::string &path )
{
    if( path.empty() )
        return path;
    char *rp = ::realpath( path.c_str(), nullptr );
    if( rp )
    {
        std::string out( rp );
        std::free( rp );
        return out;
    }
    if( !path.empty() && path[0] == '/' )
        return path;
    char cwd[PATH_MAX];
    if( ::getcwd( cwd, sizeof( cwd ) ) != nullptr )
        return std::string( cwd ) + "/" + path;
    return path;
}

int iefcCtrl::ensureDhMaskStream( uint32_t w, uint32_t h )
{
    if( m_shmDhMaskCreated )
    {
        const uint32_t ow = m_shmDhMask.md ? m_shmDhMask.md->size[0] : 0;
        const uint32_t oh =
            ( m_shmDhMask.md && m_shmDhMask.md->naxis > 1 ) ? m_shmDhMask.md->size[1] : 0;
        if( ow == w && oh == h )
            return 0;
        ImageStreamIO_closeIm( &m_shmDhMask );
        m_shmDhMaskCreated = false;
    }

    if( ImageStreamIO_openIm( &m_shmDhMask, m_shmDhMaskName.c_str() ) == IMAGESTREAMIO_SUCCESS )
    {
        const uint32_t ow = m_shmDhMask.md->size[0];
        const uint32_t oh = ( m_shmDhMask.md->naxis > 1 ) ? m_shmDhMask.md->size[1] : 1;
        if( ow == w && oh == h && m_shmDhMask.md->datatype == _DATATYPE_FLOAT )
        {
            m_shmDhMaskCreated = true;
            return 0;
        }
        ImageStreamIO_closeIm( &m_shmDhMask );
    }

    uint32_t imsize[3] = { w, h, 0 };
    if( ImageStreamIO_createIm_gpu( &m_shmDhMask,
                                    m_shmDhMaskName.c_str(),
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
            { __FILE__, __LINE__, "failed to create " + m_shmDhMaskName } );
    }
    m_shmDhMaskCreated = true;
    log<text_log>( "created shmim " + m_shmDhMaskName + " " + std::to_string( w ) + "x" +
                   std::to_string( h ) );
    return 0;
}

int iefcCtrl::publishDhMask( const lina::Array2D<std::uint8_t> &mask )
{
    if( mask.rows() == 0 || mask.cols() == 0 )
        return -1;
    const uint32_t w = static_cast<uint32_t>( mask.rows() );
    const uint32_t h = static_cast<uint32_t>( mask.cols() );
    if( ensureDhMaskStream( w, h ) < 0 )
        return -1;

    m_shmDhMask.md->write = 1;
    auto *out = reinterpret_cast<float *>( m_shmDhMask.array.raw );
    for( size_t i = 0; i < mask.size(); ++i )
        out[i] = mask.data()[i] ? 1.0f : 0.0f;
    m_shmDhMask.md->cnt0++;
    m_shmDhMask.md->write = 0;
    ImageStreamIO_sempost( &m_shmDhMask, -1 );
    return 0;
}

int iefcCtrl::ensureSatMaskStream( uint32_t w, uint32_t h )
{
    if( m_shmSatMaskCreated )
    {
        const uint32_t ow = m_shmSatMask.md ? m_shmSatMask.md->size[0] : 0;
        const uint32_t oh =
            ( m_shmSatMask.md && m_shmSatMask.md->naxis > 1 ) ? m_shmSatMask.md->size[1]
                                                               : 0;
        if( ow == w && oh == h )
            return 0;
        ImageStreamIO_closeIm( &m_shmSatMask );
        m_shmSatMaskCreated = false;
    }

    if( ImageStreamIO_openIm( &m_shmSatMask, m_shmSatMaskName.c_str() ) ==
        IMAGESTREAMIO_SUCCESS )
    {
        const uint32_t ow = m_shmSatMask.md->size[0];
        const uint32_t oh =
            ( m_shmSatMask.md->naxis > 1 ) ? m_shmSatMask.md->size[1] : 1;
        if( ow == w && oh == h && m_shmSatMask.md->datatype == _DATATYPE_FLOAT )
        {
            m_shmSatMaskCreated = true;
            return 0;
        }
        ImageStreamIO_closeIm( &m_shmSatMask );
    }

    uint32_t imsize[3] = { w, h, 0 };
    if( ImageStreamIO_createIm_gpu( &m_shmSatMask,
                                    m_shmSatMaskName.c_str(),
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
            { __FILE__, __LINE__, "failed to create " + m_shmSatMaskName } );
    }
    m_shmSatMaskCreated = true;
    log<text_log>( "created shmim " + m_shmSatMaskName + " " + std::to_string( w ) + "x" +
                   std::to_string( h ) );
    return 0;
}

int iefcCtrl::publishSatMask( const lina::Array2D<std::uint8_t> &mask )
{
    if( mask.rows() == 0 || mask.cols() == 0 )
        return -1;
    const uint32_t w = static_cast<uint32_t>( mask.rows() );
    const uint32_t h = static_cast<uint32_t>( mask.cols() );
    if( ensureSatMaskStream( w, h ) < 0 )
        return -1;

    m_shmSatMask.md->write = 1;
    auto *out = reinterpret_cast<float *>( m_shmSatMask.array.raw );
    for( size_t i = 0; i < mask.size(); ++i )
        out[i] = mask.data()[i] ? 1.0f : 0.0f;
    m_shmSatMask.md->cnt0++;
    m_shmSatMask.md->write = 0;
    ImageStreamIO_sempost( &m_shmSatMask, -1 );
    return 0;
}

int iefcCtrl::setImaxRefValue( double imax )
{
    if( !( imax > 0.0 ) )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "psf_max_ref must be > 0" } );

    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        m_cachedImaxRef = imax;
    }
    {
        std::lock_guard<std::mutex> lock( m_subNormMutex );
        m_liveImaxRef = imax;
        m_imaxRefManual = true;
        // Keep m_haveLiveNorm if dark already loaded — only the scale changed.
        if( m_liveDark.size() > 0 )
            m_haveLiveNorm = true;
    }
    // Discard NI block built under the previous scale.
    resetContrastAccumulator();
    updateIfChanged( m_indiP_psfMaxRef, "current", imax );
    updateIfChanged( m_indiP_psfMaxRef, "target", imax );
    log<text_log>( "psf_max_ref set manually to " + formatSci( imax ) +
                   " (NI + contrast_avg now use this scale)" );
    return 0;
}

int iefcCtrl::applySatMaskFromFits( const std::string &path )
{
    if( path.empty() )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "sat_mask_path is empty" } );

    lina::Array2D<double> m;
    try
    {
        m = lina::load_fits_double( path );
    }
    catch( const std::exception &e )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "load sat mask failed: " ) + e.what() } );
    }
    if( m.rows() == 0 || m.cols() == 0 )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "sat mask FITS is empty: " + path } );

    lina::Array2D<std::uint8_t> mask( m.rows(), m.cols(), 0 );
    std::size_t nones = 0;
    for( size_t i = 0; i < m.size(); ++i )
    {
        const std::uint8_t v = m.data()[i] > 0.5 ? 1 : 0;
        mask.data()[i] = v;
        if( v )
            ++nones;
    }
    if( nones == 0 )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "sat mask has no positive pixels: " + path } );

    m_satMask = mask;
    m_haveSatMask = true;
    if( publishSatMask( mask ) < 0 )
        log<text_log>( "shm_sat_mask publish failed", logPrio::LOG_WARNING );
    else
        log<text_log>( "wrote sat mask to shmim " + m_shmSatMaskName );

    log<text_log>( "loaded sat mask from " + absPath( path ) + " (" +
                   std::to_string( mask.rows() ) + "x" + std::to_string( mask.cols() ) +
                   ", ones=" + std::to_string( nones ) + ", thresh=" +
                   std::to_string( m_satThresh ) + " ADU)" );
    return 0;
}

int iefcCtrl::ensureSatMaskLoaded()
{
    if( m_haveSatMask && m_satMask.size() > 0 )
        return 0;
    if( m_satMaskPath.empty() )
        return 0; // optional
    return applySatMaskFromFits( m_satMaskPath );
}

int iefcCtrl::remaskControlFromCalibration( const lina::Array2D<std::uint8_t> &mask )
{
    struct stat st {};
    if( m_calDir.empty() || ::stat( m_calDir.c_str(), &st ) != 0 || !S_ISDIR( st.st_mode ) )
    {
        log<text_log>( "dh_mask_reload: calibration package does not exist yet at cal_dir=" +
                           ( m_calDir.empty() ? std::string( "(empty)" ) : absPath( m_calDir ) ) +
                           " — mask kept for next calibrate; control not recomputed",
                       logPrio::LOG_WARNING );
        return 1;
    }

    lina::Array2D<double> response_full;
    lina::Array2D<double> probe_modes;
    lina::Array2D<double> calib_modes;
    std::size_t nprobes = 0;
    std::size_t nmodes = 0;
    std::size_t ncam = mask.rows();

    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        response_full = m_cachedResponseFull;
        probe_modes = m_cachedProbeModes;
        calib_modes = m_cachedCalibModes;
        nprobes = probe_modes.rows();
        nmodes = calib_modes.rows();
    }

    // Load modes from package if not cached.
    if( nprobes == 0 || nmodes == 0 )
    {
        try
        {
            lina::PackagePaths pkg;
            pkg.dir = m_calDir;
            lina::LoopInputs in = lina::default_loop_inputs( ncam, 34 );
            lina::load_modes_from_package( in, pkg );
            probe_modes = in.probe_modes;
            calib_modes = in.calib_modes;
            nprobes = probe_modes.rows();
            nmodes = calib_modes.rows();
            ncam = in.ncam > 0 ? in.ncam : ncam;
        }
        catch( const std::exception &e )
        {
            log<text_log>( std::string( "dh_mask_reload: cannot load modes from cal_dir: " ) +
                               e.what() + " — mask kept for next calibrate",
                           logPrio::LOG_WARNING );
            return 1;
        }
    }

    if( response_full.size() == 0 )
    {
        try
        {
            lina::PackagePaths pkg;
            pkg.dir = m_calDir;
            response_full = lina::load_response_full( pkg, ncam, nmodes, nprobes );
            log<text_log>( "dh_mask_reload: loaded response_full from " +
                           absPath( pkg.response_full_path() ) );
        }
        catch( const std::exception &e )
        {
            log<text_log>(
                std::string( "dh_mask_reload: cannot remask — no response_full in memory or at "
                             "cal_dir (" ) +
                    e.what() +
                    "). Enable save_response_full on calibrate, or re-run doCalibrate with "
                    "this mask. Control cache cleared.",
                logPrio::LOG_WARNING );
            {
                std::lock_guard<std::mutex> lock( m_calMutex );
                m_cachedControl = {};
                m_cachedResponse = {};
                m_cachedRegCond = std::numeric_limits<float>::quiet_NaN();
                // Keep modes/mask/dark; force recalibrate before run.
                m_haveCalibration = false;
            }
            return 1;
        }
    }

    try
    {
        setStatus( "dh_mask_reload: remasking response / beta_reg" );
        auto response_masked =
            lina::mask_response_full( response_full, mask, nprobes );
        log<text_log>( "dh_mask_reload: remasked response " +
                       std::to_string( response_masked.rows() ) + "x" +
                       std::to_string( response_masked.cols() ) + " (ones=" +
                       std::to_string( response_masked.cols() / nprobes ) + ")" );

        // Invalidate old per-reg control files (nmeas changed).
        lina::clear_control_reg_files( m_calDir );

        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            m_cachedResponseFull = response_full;
            m_cachedResponse = response_masked;
            m_cachedProbeModes = probe_modes;
            m_cachedCalibModes = calib_modes;
            m_cachedMask = mask;
            m_cachedControl = {};
            m_cachedRegCond = std::numeric_limits<float>::quiet_NaN();
            m_haveCalibration = true;
        }

        lina::Array2D<double> control;
        if( loadOrBuildControl( response_masked, m_calRegCond, control ) < 0 )
            return -1;

        // Persist remasked package pieces under dir_cal.
        lina::PackagePaths pkg;
        pkg.dir = m_calDir;
        if( ensureDir( m_calDir ) == 0 )
        {
            lina::Array2D<double> mask_f( mask.rows(), mask.cols(), 0.0 );
            for( size_t i = 0; i < mask.size(); ++i )
                mask_f.data()[i] = mask.data()[i] ? 1.0 : 0.0;
            lina::save_fits( pkg.wfs_mask_path(),
                             mask_f,
                             { { "KIND", "'wfs_mask'" }, { "SOURCE", "'dh_mask_reload'" } },
                             true );
            log<text_log>( "wrote " + absPath( pkg.wfs_mask_path() ) );

            const std::size_t nmask = response_masked.cols() / nprobes;
            lina::save_matrix(
                pkg.response_path(),
                response_masked,
                { { "KIND", "'response_masked'" },
                  { "NMODES", std::to_string( nmodes ) },
                  { "NPROBES", std::to_string( nprobes ) },
                  { "NMASK", std::to_string( nmask ) },
                  { "NMEAS", std::to_string( response_masked.cols() ) },
                  { "RESPONSE_LAYOUT", "'nmodes_nmeas'" } } );
            log<text_log>( "wrote remasked " + absPath( pkg.response_path() ) );
        }

        log<text_log>( "dh_mask_reload: control recomputed for cal_reg_cond=" +
                       formatSci( m_calRegCond ) );
        return 0;
    }
    catch( const std::exception &e )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "dh_mask_reload remask failed: " ) + e.what() } );
    }
}

int iefcCtrl::applyDhMaskFromFits( const std::string &path )
{
    if( path.empty() )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "dh_mask_path is empty" } );

    lina::Array2D<double> m;
    try
    {
        m = lina::load_fits_double( path );
    }
    catch( const std::exception &e )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "load wfs mask failed: " ) + e.what() } );
    }

    if( m.rows() == 0 || m.cols() == 0 )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "wfs mask FITS is empty: " + path } );

    lina::Array2D<std::uint8_t> mask( m.rows(), m.cols(), 0 );
    std::size_t nones = 0;
    for( size_t i = 0; i < m.size(); ++i )
    {
        const std::uint8_t v = m.data()[i] > 0.5 ? 1 : 0;
        mask.data()[i] = v;
        if( v )
            ++nones;
    }
    if( nones == 0 )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "wfs mask has no positive pixels: " + path } );

    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        m_cachedMask = mask;
        m_haveUserDhMask = true;
    }
    {
        std::lock_guard<std::mutex> lock( m_contrastAvgMutex );
        m_liveContrastMask = mask;
        m_haveContrastMask = true;
        m_contrastFrameCount = 0;
        if( m_contrastSumIm.size() > 0 )
            m_contrastSumIm = lina::Array2D<double>( m_contrastSumIm.rows(),
                                                     m_contrastSumIm.cols(),
                                                     0.0 );
    }

    if( publishDhMask( mask ) < 0 )
        log<text_log>( "shm_dh_mask publish failed", logPrio::LOG_WARNING );
    else
        log<text_log>( "wrote mask to shmim " + m_shmDhMaskName );

    log<text_log>( "loaded WFS/control mask from " + absPath( path ) + " (" +
                   std::to_string( mask.rows() ) + "x" + std::to_string( mask.cols() ) +
                   ", ones=" + std::to_string( nones ) + ")" );

    // Write mask into dir_cal even if remask is skipped (next calibrate uses it).
    struct stat st {};
    if( !m_calDir.empty() && ::stat( m_calDir.c_str(), &st ) == 0 && S_ISDIR( st.st_mode ) )
    {
        try
        {
            if( ensureDir( m_calDir ) == 0 )
            {
                lina::PackagePaths pkg;
                pkg.dir = m_calDir;
                lina::Array2D<double> mask_f( mask.rows(), mask.cols(), 0.0 );
                for( size_t i = 0; i < mask.size(); ++i )
                    mask_f.data()[i] = mask.data()[i] ? 1.0 : 0.0;
                lina::save_fits( pkg.wfs_mask_path(),
                                 mask_f,
                                 { { "KIND", "'wfs_mask'" },
                                   { "SOURCE", "'dh_mask_reload'" } },
                                 true );
                log<text_log>( "wrote " + absPath( pkg.wfs_mask_path() ) );
            }
        }
        catch( const std::exception &e )
        {
            log<text_log>( std::string( "cal_dir wfs_mask write skipped: " ) + e.what(),
                           logPrio::LOG_WARNING );
        }
    }

    const int remask_rc = remaskControlFromCalibration( mask );
    if( remask_rc < 0 )
        return -1;

    return 0;
}

int iefcCtrl::doDhMaskReload()
{
    std::string path = m_dhMaskPath;
    if( path.empty() && !m_calDir.empty() )
    {
        lina::PackagePaths pkg;
        pkg.dir = m_calDir;
        path = pkg.wfs_mask_path();
        log<text_log>( "dh_mask_path empty; falling back to " + path );
    }

    setStatus( "dh_mask_reload: " + path );
    if( applyDhMaskFromFits( path ) < 0 )
    {
        setStatus( "dh_mask_reload: failed" );
        return -1;
    }
    setStatus( "dh_mask_reload: done" );
    return 0;
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

int iefcCtrl::publishSubNorm( const lina::Array2D<double> &im )
{
    if( im.rows() == 0 || im.cols() == 0 )
        return -1;
    const uint32_t w = static_cast<uint32_t>( im.rows() );
    const uint32_t h = static_cast<uint32_t>( im.cols() );
    if( ensureSubNormStream( w, h ) < 0 )
        return -1;

    m_subNorm.md->write = 1;
    float *dst = (float *)m_subNorm.array.raw;
    for( size_t i = 0; i < im.size(); ++i )
        dst[i] = static_cast<float>( im.data()[i] );
    if( ImageStreamIO_UpdateIm( &m_subNorm ) != IMAGESTREAMIO_SUCCESS )
    {
        m_subNorm.md->cnt0++;
        m_subNorm.md->write = 0;
        ImageStreamIO_sempost( &m_subNorm, -1 );
    }
    return 0;
}

int iefcCtrl::updateContrastFromNi( const lina::Array2D<double> &ni )
{
    lina::Array2D<double> mean;
    bool ready = false;
    {
        std::lock_guard<std::mutex> lock( m_contrastAvgMutex );
        const unsigned nwin = m_clContrastAvgN < 1 ? 1 : m_clContrastAvgN;

        if( m_contrastFrameCount == 0 || m_contrastSumIm.rows() != ni.rows() ||
            m_contrastSumIm.cols() != ni.cols() )
        {
            m_contrastSumIm = lina::Array2D<double>( ni.rows(), ni.cols(), 0.0 );
            m_contrastFrameCount = 0;
        }

        for( size_t i = 0; i < ni.size(); ++i )
            m_contrastSumIm.data()[i] += ni.data()[i];
        ++m_contrastFrameCount;

        if( m_contrastFrameCount < nwin )
            return 0;

        // Non-overlapping block: cadence = cl_contrast_avg_n camera frames.
        mean = lina::Array2D<double>( ni.rows(), ni.cols(), 0.0 );
        const double inv = 1.0 / static_cast<double>( nwin );
        for( size_t i = 0; i < mean.size(); ++i )
            mean.data()[i] = m_contrastSumIm.data()[i] * inv;

        m_contrastSumIm = lina::Array2D<double>( ni.rows(), ni.cols(), 0.0 );
        m_contrastFrameCount = 0;
        ready = true;
    }
    if( !ready )
        return 0;

    // Same block-averaged NI image → sub_norm shmim and contrast scalar.
    (void)publishSubNorm( mean );

    try
    {
        lina::Array2D<std::uint8_t> mask;
        {
            std::lock_guard<std::mutex> lock( m_contrastAvgMutex );
            if( !m_haveContrastMask || m_liveContrastMask.size() == 0 )
                return 0;
            mask = m_liveContrastMask;
        }
        // Mean of pixels that are inside the mask AND > 0; divisor is that count only.
        const auto cr = lina::compute_contrast( mean, mask );
        if( ensureContrastAvgStream() == 0 )
            writeScalar( m_contrastAvg, cr.contrast );
        updateIfChanged( m_indiP_contrastAvg, "current", cr.contrast );
    }
    catch( ... )
    {
        return -1;
    }
    return 0;
}

void iefcCtrl::resetContrastAccumulator()
{
    std::lock_guard<std::mutex> lock( m_contrastAvgMutex );
    m_contrastFrameCount = 0;
    if( m_contrastSumIm.size() > 0 )
        m_contrastSumIm = lina::Array2D<double>( m_contrastSumIm.rows(),
                                                 m_contrastSumIm.cols(),
                                                 0.0 );
}

std::string iefcCtrl::formatSci( double v, int precision )
{
    std::ostringstream oss;
    oss << std::scientific << std::setprecision( precision ) << v;
    return oss.str();
}

std::string iefcCtrl::controlMatrixPath( float reg ) const
{
    lina::PackagePaths pkg;
    pkg.dir = m_calDir;
    return pkg.control_path_for_reg( static_cast<double>( reg ) );
}

int iefcCtrl::loadOrBuildControl( const lina::Array2D<double> &response,
                                  float reg,
                                  lina::Array2D<double> &control_out )
{
    // 1) In-memory hit for this reg.
    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        if( m_haveCalibration && m_cachedControl.size() > 0 &&
            std::isfinite( m_cachedRegCond ) &&
            std::fabs( m_cachedRegCond - reg ) < 1e-6f )
        {
            control_out = m_cachedControl;
            return 0;
        }
    }

    if( response.size() == 0 )
        return log<software_error, -1>(
            { __FILE__, __LINE__, "loadOrBuildControl: empty response" } );

    const std::string path = controlMatrixPath( reg );

    // 2) On-disk hit for this reg under dir_cal.
    struct stat st {};
    if( !m_calDir.empty() && ::stat( path.c_str(), &st ) == 0 && S_ISREG( st.st_mode ) )
    {
        try
        {
            {
                std::ostringstream oss;
                oss << "loading matrix for " << formatSci( reg, 4 ) << " reg param";
                setStatus( oss.str() );
            }
            log<text_log>( "load control from " + path );
            control_out = lina::load_matrix( path );
            {
                std::lock_guard<std::mutex> lock( m_calMutex );
                m_cachedControl = control_out;
                m_cachedRegCond = reg;
            }
            return 0;
        }
        catch( const std::exception &e )
        {
            log<text_log>( std::string( "failed to load " ) + path + ": " + e.what() +
                               " — will regenerate",
                           logPrio::LOG_WARNING );
        }
    }

    // 3) Generate, write tagged file, cache.
    {
        std::ostringstream oss;
        oss << "gen. matrix for " << formatSci( reg, 4 ) << " reg param";
        setStatus( oss.str() );
    }
    log<text_log>( "beta_reg cal_reg_cond=" + formatSci( reg ) + " on response " +
                   std::to_string( response.rows() ) + "x" +
                   std::to_string( response.cols() ) );

    control_out =
        lina::beta_reg_cpu( lina::transpose( response ), static_cast<double>( reg ) );

    if( !m_calDir.empty() )
    {
        try
        {
            if( ensureDir( m_calDir ) == 0 )
            {
                lina::save_matrix(
                    path,
                    control_out,
                    { { "KIND", "'control_matrix'" },
                      { "REGCOND", lina::PackagePaths::control_reg_tag( reg ) },
                      { "RESPONSE_LAYOUT", "'nmodes_nmeas'" } } );
                // Keep legacy alias pointing at the most recently built reg.
                lina::PackagePaths pkg;
                pkg.dir = m_calDir;
                lina::save_matrix(
                    pkg.control_path(),
                    control_out,
                    { { "KIND", "'control_matrix'" },
                      { "REGCOND", lina::PackagePaths::control_reg_tag( reg ) },
                      { "RESPONSE_LAYOUT", "'nmodes_nmeas'" } } );
                log<text_log>( "wrote " + path );
            }
        }
        catch( const std::exception &e )
        {
            log<text_log>( std::string( "control matrix write skipped: " ) + e.what(),
                           logPrio::LOG_WARNING );
        }
    }

    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        m_cachedControl = control_out;
        m_cachedRegCond = reg;
    }
    return 0;
}

void iefcCtrl::warnIfSaturatedFullFrame( const std::vector<float> &im,
                                         uint32_t w,
                                         uint32_t h,
                                         const char *context )
{
    if( !( m_satThresh > 0.0f ) || im.empty() )
        return;

    for( size_t i = 0; i < im.size(); ++i )
    {
        if( im[i] >= m_satThresh )
        {
            log<text_log>( std::string( context ? context : "iefc" ) +
                               ": saturation warning (full frame, raw>=" +
                               std::to_string( m_satThresh ) + " ADU, " +
                               std::to_string( w ) + "x" + std::to_string( h ) + ")",
                           logPrio::LOG_WARNING );
            return;
        }
    }
}

int iefcCtrl::doRefPsf()
{
    setStatus( "refPsf: starting" );
    log<text_log>( "psf_take_ref psf_dir=" + m_psfDir );

    if( openCamsci() < 0 || openFsm() < 0 || openShutter() < 0 || openCamExp() < 0 ||
        openCamGain() < 0 )
        return -1;
    if( ensureDir( m_psfDir ) < 0 )
        return -1;

    // Apply target cam_exp / cam_gain if the user set them; otherwise use live shmim values.
    if( m_setCamExp )
    {
        setStatus( "refPsf: applying cam_exp" );
        if( setCamExpValue( m_camExp ) < 0 )
            return -1;
    }
    if( m_setCamGain )
    {
        setStatus( "refPsf: applying cam_gain" );
        if( setCamGainValue( m_camGain ) < 0 )
            return -1;
    }
    if( m_setCamExp || m_setCamGain )
        mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    const double live_exp = readScalarValue( m_camExpShm );
    const double live_gain = readScalarValue( m_camGainShm );
    const float psf_exp = std::isfinite( live_exp ) ? static_cast<float>( live_exp ) : m_camExp;
    const float psf_gain = std::isfinite( live_gain ) ? static_cast<float>( live_gain ) : m_camGain;
    log<text_log>( "psf_take_ref cam_exp=" + std::to_string( psf_exp ) +
                   " s cam_gain=" + std::to_string( psf_gain ) );

    const double tip_m = m_psfFsmPokeTip * 1e-9;
    const double tilt_m = m_psfFsmPokeTilt * 1e-9;
    const double ref_tip_m = m_psfFsmRefTip * 1e-9;
    const double ref_tilt_m = m_psfFsmRefTilt * 1e-9;
    const double ref_pist_m = m_psfFsmRefPiston * 1e-9;

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

    std::size_t settle_frames = 0;
    double settle_s = 0.0;
    resolveCamSettle( settle_frames, settle_s );

    std::vector<float> dark;
    uint32_t w = 0, h = 0;
    if( settle_s > 0.0 )
        mx::sys::milliSleep( static_cast<unsigned>( settle_s * 1000.0 ) );
    if( grabMeanCamsci( m_darkNImages, static_cast<unsigned>( settle_frames ), dark, w, h ) < 0 )
        return -1;
    if( saveFitsF32( m_psfDir + "/dark_avg.fits", dark, w, h ) < 0 )
        return -1;

    setStatus( "refPsf: opening shutter / PSF" );
    if( setShutterClosed( false ) < 0 )
        return -1;
    mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

    std::vector<float> psf;
    if( settle_s > 0.0 )
        mx::sys::milliSleep( static_cast<unsigned>( settle_s * 1000.0 ) );
    if( grabMeanCamsci( m_psfNFrames, static_cast<unsigned>( settle_frames ), psf, w, h ) < 0 )
        return -1;

    // Full-frame saturation check on raw PSF average (before dark subtraction).
    warnIfSaturatedFullFrame( psf, w, h, "psf_take_ref" );

    std::vector<float> psf_sub( psf.size() );
    float peak = -1e30f;
    for( size_t i = 0; i < psf.size(); ++i )
    {
        psf_sub[i] = psf[i] - dark[i];
        if( psf_sub[i] > peak )
            peak = psf_sub[i];
    }

    if( saveFitsF32( m_psfDir + "/ref_psf_avg.fits", psf, w, h ) < 0 )
        return -1;
    if( saveFitsF32( m_psfDir + "/ref_psf_dark_sub.fits", psf_sub, w, h ) < 0 )
        return -1;

    std::ostringstream cfg;
    cfg << "# Reference PSF package (iefcCtrl)\n"
        << "camsci=" << m_shmCamInput << "\n"
        << "dm_channel=" << m_shmDm << "\n"
        << "fsm_channel=" << m_shmFsm << "\n"
        << "shutter_shm=" << m_shutterName << "\n"
        << "cam_exp_shmim=" << m_camExpShmim << "\n"
        << "cam_gain_shmim=" << m_camGainShmim << "\n"
        << "cam_exp=" << psf_exp << "\n"
        << "cam_gain=" << psf_gain << "\n"
        << "exptime=" << psf_exp << "\n"           // legacy key for lina loaders
        << "psf_exptime=" << psf_exp << "\n"       // legacy
        << "gain=" << psf_gain << "\n"             // legacy
        << "psf_gain=" << psf_gain << "\n"         // legacy
        << "psf_max_ref=" << peak << "\n"
        << "Imax_ref=" << peak << "\n"  // legacy
        << "peak_dark_sub=" << peak << "\n"
        << "tip_nm=" << m_psfFsmPokeTip << "\n"
        << "tilt_nm=" << m_psfFsmPokeTilt << "\n"
        << "ref_tip_nm=" << m_psfFsmRefTip << "\n"
        << "ref_tilt_nm=" << m_psfFsmRefTilt << "\n"
        << "ndark=" << m_darkNImages << "\n"
        << "npsf=" << m_psfNFrames << "\n"
        << "nframes=" << m_calNImages << "\n"
        << "cam_n_frame_delay=" << m_camNFrameDelay << "\n"
        << "cam_r_delay=" << m_camRDelay << "\n"
        << "dark_file=dark_avg.fits\n"
        << "ref_psf_file=ref_psf_avg.fits\n"
        << "ref_psf_dark_sub_file=ref_psf_dark_sub.fits\n";
    if( writeConfigTxt( m_psfDir + "/config.txt", cfg.str() ) < 0 )
        return -1;

    updateIfChanged( m_indiP_psfMaxRef, "current", static_cast<double>( peak ) );
    updateIfChanged( m_indiP_psfMaxRef, "target", static_cast<double>( peak ) );

    // Seed continuous shm_cam_sub_norm from this ref-PSF package.
    {
        m_imaxRefManual = false; // refPSF measurement replaces any manual override
        lina::SetupData setup;
        setup.loaded = true;
        setup.dir = m_psfDir;
        setup.dark = lina::Array2D<double>( w, h, 0.0 ); // rows=size[0], cols=size[1]
        for( size_t i = 0; i < dark.size(); ++i )
            setup.dark.data()[i] = static_cast<double>( dark[i] );
        setup.Imax_ref = static_cast<double>( peak );
        setup.psf_exptime = psf_exp;
        setup.dark_exptime = psf_exp;
        setup.gain = psf_gain;
        updateLiveNormFromSetup( setup );
        resetContrastAccumulator();
    }

    setStatus( "refPsf: restoring FSM home" );
    writeFsmTipTiltPiston( ref_tip_m, ref_tilt_m, ref_pist_m );

    log<text_log>( "psf_take_ref done psf_max_ref=" + std::to_string( peak ) );
    setStatus( "refPsf: done" );
    return 0;
}

int iefcCtrl::doDarkLibrary()
{
    setStatus( "darkLibrary: starting" );
    if( openCamsci() < 0 || openShutter() < 0 || openCamExp() < 0 )
        return -1;
    if( ensureDir( m_psfDir ) < 0 )
        return -1;
    if( ensureDir( m_psfDir + "/darks" ) < 0 )
        return -1;

    std::vector<double> times;
    {
        std::stringstream ss( m_darkExptimesCsv );
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
        return log<software_error, -1>( { __FILE__, __LINE__, "dark_exptimes CSV empty" } );
    }

    const double exptime_start =
        ( m_camExpShm.md->datatype == _DATATYPE_DOUBLE )
            ? ( (double *)m_camExpShm.array.raw )[0]
            : static_cast<double>( ( (float *)m_camExpShm.array.raw )[0] );
    const double shutter_start =
        ( m_shutter.md->datatype == _DATATYPE_DOUBLE )
            ? ( (double *)m_shutter.array.raw )[0]
            : static_cast<double>( ( (float *)m_shutter.array.raw )[0] );

    log<text_log>( "darkLibrary: " + std::to_string( times.size() ) +
                   " exposures from dark_exptimes=[" + m_darkExptimesCsv + "], nDark=" +
                   std::to_string( m_darkNImages ) + ", start exptime=" +
                   std::to_string( exptime_start ) + " s, shutter_start=" +
                   std::to_string( shutter_start ) );
    if( setShutterClosed( true ) < 0 )
        return -1;

    std::ofstream manifest( m_psfDir + "/dark_library.txt" );
    manifest << "# dark library: exptime  relative_path  ndark\n";

    for( size_t i = 0; i < times.size(); ++i )
    {
        if( m_stopRequested.load() )
            break;
        setStatus( "darkLibrary: exptime=" + std::to_string( times[i] ) );
        log<text_log>( "darkLibrary: [" + std::to_string( i + 1 ) + "/" +
                       std::to_string( times.size() ) + "] setting exptime=" +
                       std::to_string( times[i] ) + " s" );
        if( setCamExpValue( times[i] ) < 0 )
            return -1;
        mx::sys::milliSleep( static_cast<unsigned>( m_settle_s * 1000 ) );

        // Prefer the camera-reported exptime in the manifest so later matching
        // against live milk values (which may round) stays consistent.
        double reported = times[i];
        if( openCamExp() == 0 )
        {
            reported = ( m_camExpShm.md->datatype == _DATATYPE_DOUBLE )
                           ? ( (double *)m_camExpShm.array.raw )[0]
                           : static_cast<double>( ( (float *)m_camExpShm.array.raw )[0] );
        }

        std::vector<float> dark;
        uint32_t w = 0, h = 0;
        {
            std::size_t settle_frames = 0;
            double settle_s = 0.0;
            resolveCamSettle( settle_frames, settle_s );
            if( settle_s > 0.0 )
                mx::sys::milliSleep( static_cast<unsigned>( settle_s * 1000.0 ) );
            if( grabMeanCamsci( m_darkNImages, static_cast<unsigned>( settle_frames ), dark, w, h ) < 0 )
                return -1;
        }

        char rel[64];
        std::snprintf( rel, sizeof( rel ), "darks/dark_%03zu.fits", i );
        if( saveFitsF32( m_psfDir + "/" + rel, dark, w, h ) < 0 )
            return -1;
        manifest << std::setprecision( 17 ) << reported << "  " << rel << "  " << m_darkNImages
                 << "\n";
        log<text_log>( "darkLibrary: wrote " + m_psfDir + "/" + rel + " (requested=" +
                       std::to_string( times[i] ) + " s, camera=" +
                       std::to_string( reported ) + " s)" );
    }

    log<text_log>( "darkLibrary: restoring shutter/exptime" );
    if( shutter_start > 0.5 )
        setShutterClosed( true );
    else
        setShutterClosed( false );
    if( setCamExpValue( exptime_start ) < 0 )
        return -1;
    setStatus( "darkLibrary: done" );
    log<text_log>( "dark_lib_build wrote " + m_psfDir + "/dark_library.txt" );
    return 0;
}

int iefcCtrl::doDmReset()
{
    setStatus( "dm_reset: zeroing " + m_shmDm );
    log<text_log>( "dm_reset: writing zeros to " + m_shmDm );

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

    log<text_log>( "dm_reset: zeroed " + m_shmDm + " (" + std::to_string( w ) + "x" +
                   std::to_string( h ) + ")" );
    setStatus( "dm_reset: done" );
    return 0;
}

int iefcCtrl::doRecomputeControl()
{
    lina::Array2D<double> response;
    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        if( !m_haveCalibration || m_cachedResponse.size() == 0 )
        {
            // Cold: try loading response from dir_cal so we can still load/build control.
            if( m_calDir.empty() )
            {
                setStatus( "idle" );
                log<text_log>( "cal_reg_cond set; no calibration package — control will be "
                               "built on next calibrate/run",
                               logPrio::LOG_WARNING );
                return 0;
            }
        }
        else
        {
            response = m_cachedResponse;
        }
    }

    if( response.size() == 0 && !m_calDir.empty() )
    {
        try
        {
            lina::PackagePaths pkg;
            pkg.dir = m_calDir;
            response = lina::load_matrix( pkg.response_path() );
            std::lock_guard<std::mutex> lock( m_calMutex );
            m_cachedResponse = response;
            m_haveCalibration = m_cachedResponse.size() > 0;
        }
        catch( const std::exception &e )
        {
            setStatus( "idle" );
            log<text_log>( std::string( "cal_reg_cond set; cannot load response: " ) + e.what(),
                           logPrio::LOG_WARNING );
            return 0;
        }
    }

    if( response.size() == 0 )
    {
        setStatus( "idle" );
        return 0;
    }

    // Short-circuit if memory already has this reg (e.g. rapid duplicate INDI sets).
    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        if( m_cachedControl.size() > 0 && std::isfinite( m_cachedRegCond ) &&
            std::fabs( m_cachedRegCond - m_calRegCond ) < 1e-6f )
        {
            setStatus( "idle" );
            log<text_log>( "control already in memory for cal_reg_cond=" +
                           formatSci( m_calRegCond ) );
            return 0;
        }
    }

    lina::Array2D<double> control;
    if( loadOrBuildControl( response, m_calRegCond, control ) < 0 )
    {
        setStatus( "recompute: failed" );
        return -1;
    }

    setStatus( "idle" );
    log<text_log>( "control ready for cal_reg_cond=" + formatSci( m_calRegCond ) );
    return 0;
}

int iefcCtrl::doCalibrate()
{
    setStatus( "calibrate: starting" );
    try
    {
        lina::ShmimStream camsci( m_shmCamInput );
        lina::ShmimStream dm( m_shmDm );

        if( m_setCamExp )
        {
            if( setCamExpValue( m_camExp ) < 0 )
                return -1;
        }
        if( m_setCamGain )
        {
            if( setCamGainValue( m_camGain ) < 0 )
                return -1;
        }
        double live_exptime = m_camExp;
        if( openCamExp() == 0 )
        {
            live_exptime = ( m_camExpShm.md->datatype == _DATATYPE_DOUBLE )
                               ? ( (double *)m_camExpShm.array.raw )[0]
                               : (double)( (float *)m_camExpShm.array.raw )[0];
        }

        auto in = lina::default_loop_inputs( camsci.rows(), dm.rows() );
        in.nframes = m_calNImages;
        resolveCamSettle( in.wait_frames, in.delay_s );
        in.calib_probe_amp = m_calProbeAmp;
        in.calib_amp = m_calModeAmp;
        in.reg_cond = m_calRegCond;

        if( m_psfDir.empty() )
            return log<software_error, -1>( { __FILE__, __LINE__, "psf_dir required" } );
        auto setup = lina::load_setup_dir( m_psfDir, camsci.rows(), live_exptime );
        lina::apply_setup( in, setup, live_exptime );
        lina::generate_modes( in );
        m_imaxRefManual = false; // calibrate adopts package Imax_ref
        updateLiveNormFromSetup( setup );
        resetContrastAccumulator();

        // Prefer user-loaded / cached WFS mask over default annulus.
        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            if( m_cachedMask.size() > 0 && m_cachedMask.rows() == in.control_mask.rows() &&
                m_cachedMask.cols() == in.control_mask.cols() )
            {
                in.control_mask = m_cachedMask;
                log<text_log>( "calibrate: using loaded/cached WFS mask (" +
                               std::to_string( m_cachedMask.rows() ) + "x" +
                               std::to_string( m_cachedMask.cols() ) + ")" );
            }
            else if( m_haveUserDhMask )
            {
                return log<software_error, -1>(
                    { __FILE__, __LINE__,
                      "loaded WFS mask size does not match camsci — reload mask" } );
            }
        }

        if( ensureSatMaskLoaded() < 0 )
            return -1;
        const lina::Array2D<std::uint8_t> *sat_ptr =
            ( m_haveSatMask && m_satMask.size() > 0 ) ? &m_satMask : nullptr;
        if( sat_ptr )
        {
            if( m_satMask.rows() != in.control_mask.rows() ||
                m_satMask.cols() != in.control_mask.cols() )
            {
                return log<software_error, -1>(
                    { __FILE__, __LINE__,
                      "sat_mask size does not match camsci / control mask" } );
            }
            log<text_log>( "calibrate: saturation check enabled (thresh=" +
                           std::to_string( m_satThresh ) + " ADU)" );
        }

        const unsigned nmodes = static_cast<unsigned>( in.calib_modes.rows() );
        updateIfChanged( m_indiP_nCalModes, "current", static_cast<double>( nmodes ) );
        updateIfChanged( m_indiP_calMode, "current", 0.0 );
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
                updateIfChanged( m_indiP_nCalModes, "current",
                                 static_cast<double>( nmodes_cb ) );
                updateIfChanged( m_indiP_calMode, "current", static_cast<double>( mode ) );
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
            /*keep_full_response=*/true, // required for dh_mask_reload remask
            makeStopCheck(),
            sat_ptr,
            static_cast<double>( m_satThresh ),
            [this]( std::size_t cal_mode ) {
                log<text_log>( "calibrate: saturation warning cal_mode=" +
                                   std::to_string( cal_mode ) + " (raw>=" +
                                   std::to_string( m_satThresh ) + " ADU in sat_mask)",
                               logPrio::LOG_WARNING );
            } );
        dm.zero();

        updateIfChanged( m_indiP_calMode, "current", static_cast<double>( nmodes ) );
        setStatus( "calibrate: computing control matrix (beta_reg)" );
        log<text_log>( "calibrate: beta_reg on response "
                       + std::to_string( cal.response_masked.rows() ) + "x"
                       + std::to_string( cal.response_masked.cols() ) );
        auto control = lina::beta_reg_cpu( lina::transpose( cal.response_masked ), in.reg_cond );
        log<text_log>( "calibrate: beta_reg done" );

        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            m_cachedResponse = cal.response_masked;
            m_cachedResponseFull = cal.response_full;
            m_cachedControl = control;
            m_cachedProbeModes = in.probe_modes;
            m_cachedCalibModes = in.calib_modes;
            m_cachedMask = in.control_mask;
            m_haveUserDhMask = false; // consumed into this package
            m_cachedDark = setup.dark;
            m_cachedImaxRef = setup.Imax_ref;
            m_cachedPsfExptime = setup.psf_exptime;
            m_cachedGain = setup.gain;
            m_haveCalibration = true;
            m_cachedRegCond = static_cast<float>( in.reg_cond );
        }
        m_liveContrastMask = in.control_mask;
        m_haveContrastMask = true;
        (void)publishDhMask( in.control_mask );

        setStatus( "calibrate: writing package" );
        lina::PackagePaths pkg;
        pkg.dir = m_calDir;
        // Always write response_full when available so dh_mask_reload can remask from dir_cal.
        const lina::Array2D<double> *full_ptr =
            ( cal.response_full.size() > 0 && m_saveResponseFull ) ? &cal.response_full
                                                                  : nullptr;
        if( cal.response_full.size() > 0 && !m_saveResponseFull )
        {
            log<text_log>( "calibrate: response_full kept in memory but not written "
                           "(save_response_full=false); remask after restart will need "
                           "recalibrate or enable save_response_full",
                           logPrio::LOG_WARNING );
        }
        lina::save_package( pkg, in, cal.response_masked, control, setup, full_ptr );

        updateIfChanged( m_indiP_psfMaxRef, "current", setup.Imax_ref );
        updateIfChanged( m_indiP_psfMaxRef, "target", setup.Imax_ref );
        setStatus( "calibrate: done" );
        log<text_log>( "calibrate wrote package to " + absPath( m_calDir ) +
                       " (control cached in memory)" );

        return 0;
    }
    catch( const lina::Cancelled & )
    {
        try
        {
            lina::ShmimStream dm( m_shmDm );
            dm.zero();
        }
        catch( ... )
        {
        }
        updateIfChanged( m_indiP_calMode, "current", 0.0 );
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
        lina::ShmimStream camsci( m_shmCamInput );
        lina::ShmimStream dm( m_shmDm );

        if( m_setCamExp )
        {
            if( setCamExpValue( m_camExp ) < 0 )
                return -1;
        }
        if( m_setCamGain )
        {
            if( setCamGainValue( m_camGain ) < 0 )
                return -1;
        }
        double live_exptime = m_camExp;
        if( openCamExp() == 0 )
        {
            live_exptime = ( m_camExpShm.md->datatype == _DATATYPE_DOUBLE )
                               ? ( (double *)m_camExpShm.array.raw )[0]
                               : (double)( (float *)m_camExpShm.array.raw )[0];
        }

        auto in = lina::default_loop_inputs( camsci.rows(), dm.rows() );
        in.nframes = m_clContrastAvgN < 1 ? 1 : m_clContrastAvgN;
        resolveCamSettle( in.wait_frames, in.delay_s );
        in.reg_cond = m_calRegCond;
        in.run_probe_amp = m_clProbeAmp;
        in.num_iters = m_clIters;
        in.gain = m_clLoopGain;
        in.leakage = m_clLeakage;

        lina::Array2D<double> control;
        lina::SetupData setupData;
        bool used_cache = false;
        lina::Array2D<double> response;

        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            if( m_haveCalibration )
            {
                in.probe_modes = m_cachedProbeModes;
                in.calib_modes = m_cachedCalibModes;
                in.control_mask = m_cachedMask;
                control = m_cachedControl;
                response = m_cachedResponse;
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
            pkg.dir = m_calDir;
            lina::load_modes_from_package( in, pkg );

            if( !m_psfDir.empty() )
                setupData = lina::load_setup_dir( m_psfDir, camsci.rows(), live_exptime );
            else
                setupData = lina::load_setup_from_package( pkg, camsci.rows(), live_exptime );
            if( !setupData.loaded )
                return log<software_error, -1>( { __FILE__, __LINE__, "setup/dark not loaded" } );

            response = lina::load_matrix( pkg.response_path() );
            if( loadOrBuildControl( response, m_calRegCond, control ) < 0 )
                return -1;

            {
                std::lock_guard<std::mutex> lock( m_calMutex );
                m_cachedResponse = response;
                // control + reg already set by loadOrBuildControl
                m_cachedProbeModes = in.probe_modes;
                m_cachedCalibModes = in.calib_modes;
                m_cachedMask = in.control_mask;
                m_cachedDark = setupData.dark;
                m_cachedImaxRef = setupData.Imax_ref;
                m_cachedPsfExptime = setupData.psf_exptime;
                m_cachedGain = setupData.gain;
                m_haveCalibration = true;
            }
            try
            {
                auto full = lina::load_response_full( pkg, in.ncam, in.calib_modes.rows(),
                                                      in.probe_modes.rows() );
                std::lock_guard<std::mutex> lock( m_calMutex );
                m_cachedResponseFull = std::move( full );
                log<text_log>( "run: cached response_full for remask support" );
            }
            catch( ... )
            {
                // Optional — remask after restart needs save_response_full.
            }
            log<text_log>( "run: loaded calibration package into memory from " + m_calDir );
        }
        else
        {
            log<text_log>( "run: using in-memory calibration matrices" );
            if( loadOrBuildControl( response, m_calRegCond, control ) < 0 )
                return -1;
            // Refresh dark for current live exptime from dir_psf when available.
            if( !m_psfDir.empty() )
            {
                try
                {
                    auto live_setup =
                        lina::load_setup_dir( m_psfDir, camsci.rows(), live_exptime );
                    setupData.dark = live_setup.dark;
                    setupData.dark_exptime = live_setup.dark_exptime;
                    if( live_setup.Imax_ref > 0.0 && !m_imaxRefManual )
                        setupData.Imax_ref = live_setup.Imax_ref;
                }
                catch( const std::exception &e )
                {
                    log<text_log>( std::string( "run: psf_dir dark refresh skipped: " ) +
                                       e.what(),
                                   logPrio::LOG_WARNING );
                }
            }
        }

        // Prefer manual INDI Imax_ref for NI / closed-loop contrast when set.
        if( m_imaxRefManual )
        {
            std::lock_guard<std::mutex> lock( m_subNormMutex );
            if( m_liveImaxRef > 0.0 )
                setupData.Imax_ref = m_liveImaxRef;
        }

        lina::apply_setup( in, setupData, live_exptime );
        updateLiveNormFromSetup( setupData );
        m_liveContrastMask = in.control_mask;
        m_haveContrastMask = true;
        (void)publishDhMask( in.control_mask );

        updateIfChanged( m_indiP_nCalModes, "current",
                         static_cast<double>( in.calib_modes.rows() ) );
        updateIfChanged( m_indiP_calMode, "current", 0.0 );

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
        {
            double imax_pub = setupData.Imax_ref;
            {
                std::lock_guard<std::mutex> lock( m_subNormMutex );
                if( m_imaxRefManual && m_liveImaxRef > 0.0 )
                    imax_pub = m_liveImaxRef;
            }
            updateIfChanged( m_indiP_psfMaxRef, "current", imax_pub );
            updateIfChanged( m_indiP_psfMaxRef, "target", imax_pub );
        }

        for( size_t i = 0; i < data.contrasts.size(); ++i )
            log<text_log>( "run iter" + std::to_string( i ) + " contrast=" +
                           formatSci( data.contrasts[i] ) );

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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmCamInput )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmCamInput, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmCamInput, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shmCamInput )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_cam_input while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmCamInput, "current", m_shmCamInput );
        updateIfChanged( m_indiP_shmCamInput, "target", m_shmCamInput );
        return 0;
    }
    log<text_log>( "shm_cam_input: " + m_shmCamInput + " -> " + target );
    m_shmCamInput = target;
    updateIfChanged( m_indiP_shmCamInput, "current", m_shmCamInput );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmDm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmDm, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmDm, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shmDm )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_dm while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmDm, "current", m_shmDm );
        updateIfChanged( m_indiP_shmDm, "target", m_shmDm );
        return 0;
    }
    log<text_log>( "shm_dm: " + m_shmDm + " -> " + target );
    m_shmDm = target;
    updateIfChanged( m_indiP_shmDm, "current", m_shmDm );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmFsm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmFsm, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmFsm, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shmFsm )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_fsm while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmFsm, "current", m_shmFsm );
        updateIfChanged( m_indiP_shmFsm, "target", m_shmFsm );
        return 0;
    }
    log<text_log>( "shm_fsm: " + m_shmFsm + " -> " + target );
    m_shmFsm = target;
    updateIfChanged( m_indiP_shmFsm, "current", m_shmFsm );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmShutter )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmShutter, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmShutter, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shutterName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_shutter while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmShutter, "current", m_shutterName );
        updateIfChanged( m_indiP_shmShutter, "target", m_shutterName );
        return 0;
    }
    log<text_log>( "shm_shutter: " + m_shutterName + " -> " + target );
    m_shutterName = target;
    updateIfChanged( m_indiP_shmShutter, "current", m_shutterName );
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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camExpShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camExpShmim, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_camExpShmim, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_camExpShmim )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change cam_exp_shmim while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_camExpShmim, "current", m_camExpShmim );
        updateIfChanged( m_indiP_camExpShmim, "target", m_camExpShmim );
        return 0;
    }
    log<text_log>( "cam_exp_shmim: " + m_camExpShmim + " -> " + target );
    m_camExpShmim = target;
    updateIfChanged( m_indiP_camExpShmim, "current", m_camExpShmim );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camGainShmim )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camGainShmim, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_camGainShmim, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_camGainShmim )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change cam_gain_shmim while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_camGainShmim, "current", m_camGainShmim );
        updateIfChanged( m_indiP_camGainShmim, "target", m_camGainShmim );
        return 0;
    }
    log<text_log>( "cam_gain_shmim: " + m_camGainShmim + " -> " + target );
    m_camGainShmim = target;
    updateIfChanged( m_indiP_camGainShmim, "current", m_camGainShmim );
    closeStreams();
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmCamSubNorm )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmCamSubNorm, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmCamSubNorm, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shmCamSubNorm )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_cam_sub_norm while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmCamSubNorm, "current", m_shmCamSubNorm );
        updateIfChanged( m_indiP_shmCamSubNorm, "target", m_shmCamSubNorm );
        return 0;
    }
    log<text_log>( "shm_cam_sub_norm: " + m_shmCamSubNorm + " -> " + target );
    m_shmCamSubNorm = target;
    updateIfChanged( m_indiP_shmCamSubNorm, "current", m_shmCamSubNorm );
    if( m_subNormCreated )
    {
        ImageStreamIO_closeIm( &m_subNorm );
        m_subNormCreated = false;
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmContrastAvg )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmContrastAvg, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmContrastAvg, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_contrastAvgName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_contrast_avg while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmContrastAvg, "current", m_contrastAvgName );
        updateIfChanged( m_indiP_shmContrastAvg, "target", m_contrastAvgName );
        return 0;
    }
    log<text_log>( "shm_contrast_avg: " + m_contrastAvgName + " -> " + target );
    m_contrastAvgName = target;
    updateIfChanged( m_indiP_shmContrastAvg, "current", m_contrastAvgName );
    if( m_contrastAvgCreated )
    {
        ImageStreamIO_closeIm( &m_contrastAvg );
        m_contrastAvgCreated = false;
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmDhMask )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmDhMask, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmDhMask, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shmDhMaskName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_dh_mask while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmDhMask, "current", m_shmDhMaskName );
        updateIfChanged( m_indiP_shmDhMask, "target", m_shmDhMaskName );
        return 0;
    }
    log<text_log>( "shm_dh_mask: " + m_shmDhMaskName + " -> " + target );
    m_shmDhMaskName = target;
    updateIfChanged( m_indiP_shmDhMask, "current", m_shmDhMaskName );
    if( m_shmDhMaskCreated )
    {
        ImageStreamIO_closeIm( &m_shmDhMask );
        m_shmDhMaskCreated = false;
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_shmSatMask )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmSatMask, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmSatMask, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_shmSatMaskName )
        return 0;
    if( m_busy.load() )
    {
        log<text_log>( "cannot change shm_sat_mask while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_shmSatMask, "current", m_shmSatMaskName );
        updateIfChanged( m_indiP_shmSatMask, "target", m_shmSatMaskName );
        return 0;
    }
    log<text_log>( "shm_sat_mask: " + m_shmSatMaskName + " -> " + target );
    m_shmSatMaskName = target;
    updateIfChanged( m_indiP_shmSatMask, "current", m_shmSatMaskName );
    if( m_shmSatMaskCreated )
    {
        ImageStreamIO_closeIm( &m_shmSatMask );
        m_shmSatMaskCreated = false;
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfMaxRef )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfMaxRef, ipRecv );
    double target = 0.0;
    if( indiTargetUpdate( m_indiP_psfMaxRef, target, ipRecv, false ) < 0 )
        return -1;
    if( !( target > 0.0 ) )
    {
        log<text_log>( "psf_max_ref target must be > 0", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_psfMaxRef, "target", m_liveImaxRef > 0.0 ? m_liveImaxRef
                                                                       : m_cachedImaxRef );
        return 0;
    }
    return setImaxRefValue( target );
}



INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calNImages )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calNImages, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_calNImages, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_calNImages )
        log<text_log>( "cal_n_images: " + std::to_string( m_calNImages ) + " -> " + std::to_string( target ) );
    m_calNImages = target;
    updateIfChanged( m_indiP_calNImages, "current", m_calNImages );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camNFrameDelay )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camNFrameDelay, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_camNFrameDelay, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_camNFrameDelay )
        log<text_log>( "cam_n_frame_delay: " + std::to_string( m_camNFrameDelay ) + " -> " +
                       std::to_string( target ) );
    m_camNFrameDelay = target;
    updateIfChanged( m_indiP_camNFrameDelay, "current", m_camNFrameDelay );
    // Mutual exclusivity: enabling frame delay clears wall-clock delay.
    if( m_camNFrameDelay > 0 && m_camRDelay > 0.0f )
    {
        m_camRDelay = 0.0f;
        updateIfChanged( m_indiP_camRDelay, "current", m_camRDelay );
        updateIfChanged( m_indiP_camRDelay, "target", m_camRDelay );
        log<text_log>( "cam_r_delay cleared (using cam_n_frame_delay)" );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camRDelay )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camRDelay, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_camRDelay, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_camRDelay )
        log<text_log>( "cam_r_delay: " + std::to_string( m_camRDelay ) + " -> " +
                       std::to_string( target ) );
    m_camRDelay = target;
    updateIfChanged( m_indiP_camRDelay, "current", m_camRDelay );
    // Mutual exclusivity: enabling wall-clock delay clears frame delay.
    if( m_camRDelay > 0.0f && m_camNFrameDelay > 0 )
    {
        m_camNFrameDelay = 0;
        updateIfChanged( m_indiP_camNFrameDelay, "current", m_camNFrameDelay );
        updateIfChanged( m_indiP_camNFrameDelay, "target", m_camNFrameDelay );
        log<text_log>( "cam_n_frame_delay cleared (using cam_r_delay)" );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camExp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camExp, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_camExp, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_camExp )
        log<text_log>( "cam_exp: " + std::to_string( m_camExp ) + " -> " +
                       std::to_string( target ) + " s" );
    m_camExp = target;
    m_setCamExp = true;
    if( m_busy.load() )
    {
        log<text_log>( "cam_exp stored; will apply when idle / next job",
                       logPrio::LOG_WARNING );
        return 0;
    }
    return setCamExpValue( m_camExp );
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camGain, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_camGain, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_camGain )
        log<text_log>( "cam_gain: " + std::to_string( m_camGain ) + " -> " +
                       std::to_string( target ) );
    m_camGain = target;
    m_setCamGain = true;
    if( m_busy.load() )
    {
        log<text_log>( "cam_gain stored; will apply when idle / next job",
                       logPrio::LOG_WARNING );
        return 0;
    }
    return setCamGainValue( m_camGain );
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfDir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfDir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_psfDir, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_psfDir )
        log<text_log>( "psf_dir: " + m_psfDir + " -> " + target );
    m_psfDir = target;
    updateIfChanged( m_indiP_psfDir, "current", m_psfDir );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calDir )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calDir, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_calDir, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_calDir )
        log<text_log>( "cal_dir: " + m_calDir + " -> " + target );
    m_calDir = target;
    updateIfChanged( m_indiP_calDir, "current", m_calDir );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_dhMaskPath )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dhMaskPath, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_dhMaskPath, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_dhMaskPath )
        log<text_log>( "dh_mask_path: " + m_dhMaskPath + " -> " + target );
    m_dhMaskPath = target;
    updateIfChanged( m_indiP_dhMaskPath, "current", m_dhMaskPath );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_satMaskPath )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_satMaskPath, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_satMaskPath, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_satMaskPath )
        log<text_log>( "sat_mask_path: " + m_satMaskPath + " -> " + target );
    m_satMaskPath = target;
    updateIfChanged( m_indiP_satMaskPath, "current", m_satMaskPath );
    if( !m_satMaskPath.empty() && !m_busy.load() )
    {
        if( applySatMaskFromFits( m_satMaskPath ) < 0 )
            log<text_log>( "sat_mask_path set but load failed", logPrio::LOG_WARNING );
    }
    else if( m_satMaskPath.empty() )
    {
        m_haveSatMask = false;
        m_satMask = {};
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_satThresh )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_satThresh, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_satThresh, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_satThresh )
        log<text_log>( "sat_thresh: " + std::to_string( m_satThresh ) + " -> " +
                       std::to_string( target ) );
    m_satThresh = target;
    updateIfChanged( m_indiP_satThresh, "current", m_satThresh );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfFsmPokeTip )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfFsmPokeTip, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_psfFsmPokeTip, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_psfFsmPokeTip )
        log<text_log>( "psf_fsm_poke_tip: " + std::to_string( m_psfFsmPokeTip ) + " -> " +
                       std::to_string( target ) );
    m_psfFsmPokeTip = target;
    updateIfChanged( m_indiP_psfFsmPokeTip, "current", m_psfFsmPokeTip );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfFsmPokeTilt )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfFsmPokeTilt, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_psfFsmPokeTilt, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_psfFsmPokeTilt )
        log<text_log>( "psf_fsm_poke_tilt: " + std::to_string( m_psfFsmPokeTilt ) + " -> " +
                       std::to_string( target ) );
    m_psfFsmPokeTilt = target;
    updateIfChanged( m_indiP_psfFsmPokeTilt, "current", m_psfFsmPokeTilt );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfFsmRefTip )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfFsmRefTip, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_psfFsmRefTip, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_psfFsmRefTip )
        log<text_log>( "psf_fsm_ref_tip: " + std::to_string( m_psfFsmRefTip ) + " -> " +
                       std::to_string( target ) );
    m_psfFsmRefTip = target;
    updateIfChanged( m_indiP_psfFsmRefTip, "current", m_psfFsmRefTip );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfFsmRefTilt )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfFsmRefTilt, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_psfFsmRefTilt, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_psfFsmRefTilt )
        log<text_log>( "psf_fsm_ref_tilt: " + std::to_string( m_psfFsmRefTilt ) + " -> " +
                       std::to_string( target ) );
    m_psfFsmRefTilt = target;
    updateIfChanged( m_indiP_psfFsmRefTilt, "current", m_psfFsmRefTilt );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_darkNImages )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkNImages, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_darkNImages, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_darkNImages )
        log<text_log>( "dark_n_images: " + std::to_string( m_darkNImages ) + " -> " + std::to_string( target ) );
    m_darkNImages = target;
    updateIfChanged( m_indiP_darkNImages, "current", m_darkNImages );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfNFrames )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfNFrames, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_psfNFrames, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_psfNFrames )
        log<text_log>( "psf_n_frames: " + std::to_string( m_psfNFrames ) + " -> " + std::to_string( target ) );
    m_psfNFrames = target;
    updateIfChanged( m_indiP_psfNFrames, "current", m_psfNFrames );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_darkExptimes )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkExptimes, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_darkExptimes, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_darkExptimesCsv )
        log<text_log>( "dark_exptimes: [" + m_darkExptimesCsv + "] -> [" + target + "]" );
    m_darkExptimesCsv = target;
    // Soft param: adopt immediately so GUIs see current == target.
    updateIfChanged( m_indiP_darkExptimes, "current", m_darkExptimesCsv );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calRegCond )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calRegCond, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_calRegCond, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_calRegCond )
        log<text_log>( "cal_reg_cond: " + formatSci( m_calRegCond ) + " -> " +
                       formatSci( target ) );
    m_calRegCond = target;
    updateIfChanged( m_indiP_calRegCond, "current", m_calRegCond );

    // Rebuild in-memory control from cached response at the new regularization.
    bool have_response = false;
    {
        std::lock_guard<std::mutex> lock( m_calMutex );
        have_response = m_haveCalibration && m_cachedResponse.size() > 0;
    }
    if( have_response )
    {
        if( m_busy.load() )
        {
            log<text_log>( "cal_reg_cond stored; control recompute deferred (busy)",
                           logPrio::LOG_WARNING );
        }
        else
        {
            queueJob( Job::RecomputeControl );
        }
    }
    else
    {
        log<text_log>( "cal_reg_cond stored; no cached response yet" );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calProbeAmp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calProbeAmp, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_calProbeAmp, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_calProbeAmp )
        log<text_log>( "cal_probe_amp: " + std::to_string( m_calProbeAmp ) + " -> " +
                       std::to_string( target ) );
    m_calProbeAmp = target;
    updateIfChanged( m_indiP_calProbeAmp, "current", m_calProbeAmp );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calModeAmp )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calModeAmp, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_calModeAmp, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_calModeAmp )
        log<text_log>( "cal_mode_amp: " + std::to_string( m_calModeAmp ) + " -> " +
                       std::to_string( target ) );
    m_calModeAmp = target;
    updateIfChanged( m_indiP_calModeAmp, "current", m_calModeAmp );
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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clContrastAvgN )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clContrastAvgN, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_clContrastAvgN, target, ipRecv, false ) < 0 )
        return -1;
    if( target < 1 )
        target = 1;
    if( target != m_clContrastAvgN )
        log<text_log>( "cl_contrast_avg_n: " + std::to_string( m_clContrastAvgN ) + " -> " +
                       std::to_string( target ) + " (resets NI block accumulator)" );
    {
        std::lock_guard<std::mutex> lock( m_contrastAvgMutex );
        m_clContrastAvgN = target;
    }
    resetContrastAccumulator();
    updateIfChanged( m_indiP_clContrastAvgN, "current", m_clContrastAvgN );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clIters )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clIters, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_clIters, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_clIters )
        log<text_log>( "cl_iters: " + std::to_string( m_clIters ) + " -> " + std::to_string( target ) );
    m_clIters = target;
    updateIfChanged( m_indiP_clIters, "current", m_clIters );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clLoopGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clLoopGain, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_clLoopGain, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_clLoopGain )
        log<text_log>( "cl_loop_gain: " + std::to_string( m_clLoopGain ) + " -> " +
                       std::to_string( target ) );
    m_clLoopGain = target;
    updateIfChanged( m_indiP_clLoopGain, "current", m_clLoopGain );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clLeakage )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clLeakage, ipRecv );
    float target;
    if( indiTargetUpdate( m_indiP_clLeakage, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_clLeakage )
        log<text_log>( "cl_leakage: " + std::to_string( m_clLeakage ) + " -> " +
                       std::to_string( target ) );
    m_clLeakage = target;
    updateIfChanged( m_indiP_clLeakage, "current", m_clLeakage );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_psfTakeRef )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psfTakeRef, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_psfTakeRef, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::RefPsf );
        clearRequest( m_indiP_psfTakeRef );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_darkLibBuild )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkLibBuild, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_darkLibBuild, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::DarkLibrary );
        clearRequest( m_indiP_darkLibBuild );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calibrate )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calibrate, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_calibrate, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::Calibrate );
        clearRequest( m_indiP_calibrate );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clRun )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clRun, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_clRun, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::Run );
        clearRequest( m_indiP_clRun );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_dmReset )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dmReset, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_dmReset, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::DmReset );
        clearRequest( m_indiP_dmReset );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_dhMaskReload )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dhMaskReload, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        // Load synchronously so contrast updates even while another job is running.
        updateSwitchIfChanged( m_indiP_dhMaskReload, "request", pcf::IndiElement::On, INDI_BUSY );
        doDhMaskReload();
        clearRequest( m_indiP_dhMaskReload );
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
