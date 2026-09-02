/** \file iefcCtrl.hpp
  * \brief MagAO-X INDI front-end for Stream IEFC (ref PSF, dark library, calibrate, run).
  *
  * All of refPSF, darkLibrary, calibrate, and run execute natively in-process against
  * milk ImageStreamIO shmims via the vendored lina IEFC library. No external binary.
  *
  * Shared INDI numbers (cam_n_frame_delay / cam_r_delay, …) are reused across
  * all actions. Camera exptime/emgain are commanded on cam_name INDI, not here.
  *
  * \ingroup iefcCtrl_files
  */

#ifndef iefcCtrl_hpp
#define iefcCtrl_hpp

#include <atomic>
#include <algorithm>
#include <cctype>
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
#include <stdexcept>
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
        ReloadPsfRef,
        DarkLibLoad, ///< Index/validate dark_lib_path for shm_cam_input
        Calibrate,
        CalReload, ///< Load response/control/modes from cal_dir into memory
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

    std::string m_psfDir{ "./ref_psf" };  ///< Ref-PSF / Imax package (write+read)
    std::string m_calDir{ "./cal_a" };    ///< Calibration package (response/control matrices)
    std::string m_dmCmdPath{ "./dm_cmds" }; ///< Closed-loop DM command FITS archive
    unsigned m_clIndex{ 0 }; ///< Last archived / restored `{shm_dm}_cl_{N}.fits` index
    unsigned m_dmResetIndex{ 0 }; ///< Archive index loaded by `dm_reset`
    std::string m_darkLibPath; ///< External dark library dir (dark_metadata.txt from darkCtrl)
    std::string m_camName{ "camsci" }; ///< INDI device for cam_name.exptime / emgain (not dark match)
    std::string m_dhMaskPath; ///< External FITS mask path for dh_mask_reload (full path or filename)
    std::string m_satMaskPath; ///< FITS sat-check region for calibrate (raw ADU)
    float m_satThresh{ 55000.0f }; ///< Raw ADU threshold inside sat_mask (≥ → abort cal)

    unsigned m_nImages{ 10 }; ///< Frames averaged for calibrate grabs, cl_run grabs, and contrast
    /// Camera settle after DM write: use frame delay OR wall-clock delay (mutually exclusive).
    unsigned m_camNFrameDelay{ 1 };  ///< Skip this many new camsci frames (0 = use cam_r_delay)
    float m_camRDelay{ 0.0f };      ///< Wall-clock settle [s] (used only when cam_n_frame_delay==0)

    /// Live currents from cam_name.exptime / emgain SET (for dark matching / jobs).
    double m_remoteExp{ std::numeric_limits<double>::quiet_NaN() };
    double m_remoteGain{ std::numeric_limits<double>::quiet_NaN() };

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
    bool m_camsciOpen{ false };
    bool m_dmOpen{ false };
    int m_camsciSem{ -1 };
    ///@}

    /** \name INDI — shmim names (repointable)
      *@{
      */
    pcf::IndiProperty m_indiP_shmCamInput;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmCamInput);

    pcf::IndiProperty m_indiP_shmDm;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_shmDm);

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
    pcf::IndiProperty m_indiP_nImages;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_nImages);

    pcf::IndiProperty m_indiP_camNFrameDelay;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camNFrameDelay);

    pcf::IndiProperty m_indiP_camRDelay;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camRDelay);

    /// Remote SET subscriptions on cam_name.exptime / emgain (current only).
    pcf::IndiProperty m_indiP_remoteExptime;
    INDI_SETCALLBACK_DECL( iefcCtrl, m_indiP_remoteExptime );
    pcf::IndiProperty m_indiP_remoteEmgain;
    INDI_SETCALLBACK_DECL( iefcCtrl, m_indiP_remoteEmgain );

    pcf::IndiProperty m_indiP_psfDir;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfDir);

    pcf::IndiProperty m_indiP_calDir;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calDir);

    pcf::IndiProperty m_indiP_dmCmdPath;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dmCmdPath);

    pcf::IndiProperty m_indiP_darkLibPath;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_darkLibPath);

    pcf::IndiProperty m_indiP_camName;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_camName);

    pcf::IndiProperty m_indiP_dhMaskPath;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dhMaskPath);

    pcf::IndiProperty m_indiP_satMaskPath;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_satMaskPath);

    pcf::IndiProperty m_indiP_satThresh;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_satThresh);

    pcf::IndiProperty m_indiP_psfMaxRef; ///< current/target — manual set overrides until cal/refPSF
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_psfMaxRef);
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
    ///@}

    /** \name INDI — requests + status
      *@{
      */
    pcf::IndiProperty m_indiP_reloadPsfRef;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_reloadPsfRef);

    pcf::IndiProperty m_indiP_calibrate;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calibrate);

    pcf::IndiProperty m_indiP_darkLibLoad;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_darkLibLoad);

    pcf::IndiProperty m_indiP_calReload;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_calReload);

    pcf::IndiProperty m_indiP_clRun;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_clRun);

    pcf::IndiProperty m_indiP_dmReset;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dmReset);

    pcf::IndiProperty m_indiP_dmResetIndex;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dmResetIndex);

    pcf::IndiProperty m_indiP_dhMaskReload;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_dhMaskReload);

    pcf::IndiProperty m_indiP_stop;
    INDI_NEWCALLBACK_DECL(iefcCtrl, m_indiP_stop);

    pcf::IndiProperty m_indiP_status;   ///< RO text
    pcf::IndiProperty m_indiP_contrast; ///< RO number (last closed-loop iter)
    pcf::IndiProperty m_indiP_contrastAvg; ///< RO running average published to shmim
    pcf::IndiProperty m_indiP_contrastPosPixels; ///< RO % of DH-mask pixels with NI > 0
    pcf::IndiProperty m_indiP_calMode;   ///< RO: current calib mode (1..N, 0 idle)
    pcf::IndiProperty m_indiP_nCalModes; ///< RO: total calib modes in package / run
    pcf::IndiProperty m_indiP_clIndex; ///< RO: last archived / restored DM command index
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
    void closeStreams();

    int writeScalar( IMAGE &im, double value );
    int grabMeanCamsci( unsigned nframes, unsigned wait_frames, std::vector<float> &out,
                        uint32_t &w, uint32_t &h );

    int ensureDir( const std::string &dir );
    int saveFitsF32( const std::string &path, const std::vector<float> &im, uint32_t w,
                     uint32_t h );
    int writeConfigTxt( const std::string &path, const std::string &body );

    int doReloadPsfRef();
    int doCalibrate();
    int doCalReload(); ///< Load response/control/modes/mask from cal_dir into memory
    int doDarkLibLoad(); ///< Reload/validate dark_lib_path for shm_cam_input

    /// Dark-library match filter for shm_cam_input / live cam_name.emgain.current.
    lina::DarkMatchFilter darkFilter( double gain = std::numeric_limits<double>::quiet_NaN() ) const;
    int doRun();
    int doDmReset();

    /// `{dm_cmd_path}/{shm_dm}_cl_{index}.fits`
    std::string dmCmdFitsPath( unsigned index ) const;
    /// Write a closed-loop DM command (shmim units) to the archive, overwriting if present.
    int writeDmCmdArchive( unsigned index, const lina::Array2D<double> &cmd );
    /// Ensure `{shm_dm}_cl_0.fits` exists as the zero flat when `cl_index==0`.
    int ensureFlatDmCmdArchive( std::size_t rows, std::size_t cols );
    /// After each CL DM write: increment `cl_index`, archive, publish INDI.
    void archiveClosedLoopCommand( const lina::Array2D<double> &write_cmd );
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

    /// Live exposure [s] from cam_name.exptime.current (NaN until SET received).
    double liveCamExp() const;

    /// Live gain from cam_name.emgain.current (NaN until SET received).
    double liveCamGain() const;

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

    /// Accumulate one NI frame. Every n_images frames: publish mean →
    /// shm_cam_sub_norm and contrast(mean ∩ mask) → contrast_avg; also
    /// % of mask pixels with NI>0 → contrast_pos_pixels.
    int updateContrastFromNi( const lina::Array2D<double> &ni );

    /// Publish a float image to shm_cam_sub_norm (creates stream if needed).
    int publishSubNorm( const lina::Array2D<double> &im );

    /// Reset the NI-frame accumulator (e.g. when n_images changes).
    void resetContrastAccumulator();

    /// Format a double in scientific notation for logs.
    static std::string formatSci( double v, int precision = 6 );

    void setStatus( const std::string &s );
    void clearRequest( pcf::IndiProperty &p );
    void setClRunToggle( bool on );

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
                "Science-camera ImageStreamIO name (dark-library match key; default camsci)." );
    config.add( "iefc.shm_dm", "", "iefc.shm_dm", argType::Required, "iefc", "shm_dm", false,
                "string", "IEFC DM channel shmim (default dm01disp07)." );
    config.add( "iefc.cal_dir", "", "iefc.cal_dir", argType::Required, "iefc", "cal_dir", false,
                "string", "Calibration package dir (response/control matrices)." );
    config.add( "iefc.dm_cmd_path", "", "iefc.dm_cmd_path", argType::Required, "iefc", "dm_cmd_path",
                false, "string",
                "Directory for closed-loop DM command FITS ({shm_dm}_cl_{N}.fits)." );
    config.add( "iefc.dm_reset_index", "", "iefc.dm_reset_index", argType::Required, "iefc",
                "dm_reset_index", false, "unsigned",
                "Archive index loaded by dm_reset (default 0 = zero flat)." );
    config.add( "iefc.psf_dir", "", "iefc.psf_dir", argType::Required, "iefc", "psf_dir", false,
                "string",
                "Ref-PSF / Imax package (from psfRefCtrl; loaded by reload_psf_ref/calibrate/cl_run)." );
    config.add( "iefc.dark_lib_path", "", "iefc.dark_lib_path", argType::Required, "iefc", "dark_lib_path", false,
                "string", "Dark library directory (dark_metadata.txt from darkCtrl)." );
    config.add( "iefc.cam_name",
                "",
                "iefc.cam_name",
                argType::Required,
                "iefc",
                "cam_name",
                false,
                "string", "INDI device for exptime/emgain (cam_name.exptime.target, etc.)." );
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
    config.add( "iefc.n_images", "", "iefc.n_images", argType::Required, "iefc", "n_images", false,
                "unsigned",
                "Frames averaged per grab for calibrate, cl_run, and contrast / shm_cam_sub_norm." );
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
    config.add( "iefc.save_response_full", "", "iefc.save_response_full", argType::Required, "iefc", "save_response_full", false,
                "bool", "Write response_full.fits (multi-GB; needed to remask after restart)." );
}

void iefcCtrl::loadConfig()
{
    config( m_shmCamInput, "iefc.shm_cam_input" );
    config( m_shmDm, "iefc.shm_dm" );
    config( m_calDir, "iefc.cal_dir" );
    config( m_dmCmdPath, "iefc.dm_cmd_path" );
    config( m_dmResetIndex, "iefc.dm_reset_index" );
    config( m_psfDir, "iefc.psf_dir" );
    config( m_darkLibPath, "iefc.dark_lib_path" );
    config( m_camName, "iefc.cam_name" );
    config( m_dhMaskPath, "iefc.dh_mask_path" );
    config( m_satMaskPath, "iefc.sat_mask_path" );
    config( m_satThresh, "iefc.sat_thresh" );
    config( m_nImages, "iefc.n_images" );
    config( m_camNFrameDelay, "iefc.cam_n_frame_delay" );
    config( m_camRDelay, "iefc.cam_r_delay" );
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
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmCamSubNorm, "shm_cam_sub_norm", "Dark-sub+norm camera stream", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmContrastAvg, "shm_contrast_avg", "Running-avg contrast shmim name", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmDhMask, "shm_dh_mask", "WFS/control mask image shmim", "shmims" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmSatMask, "shm_sat_mask", "Saturation-check mask image shmim", "shmims" );

    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_nImages, "n_images", 1, 10000, 1, "%u",
                                 "Frames averaged for calibrate, cl_run, and contrast", "shared" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_camNFrameDelay, "cam_n_frame_delay", 0, 1000, 1, "%u",
                                 "Skip N camsci frames after DM (XOR cam_r_delay)", "shared" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_camRDelay, "cam_r_delay", 0, 10, 0.01, "%0.3f",
                                 "Wall-clock settle after DM [s] (XOR cam_n_frame_delay)", "shared" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_calDir, "cal_dir", "Calibration package dir (response/control)", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_dmCmdPath, "dm_cmd_path", "Closed-loop DM command FITS directory", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_psfDir, "psf_dir", "Ref-PSF package from psfRefCtrl; loaded by reload_psf_ref / calibrate / cl_run", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_darkLibPath, "dark_lib_path", "Dark library directory (from darkCtrl)", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_camName, "cam_name", "INDI science-camera device (exptime/emgain)", "camera" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_dhMaskPath, "dh_mask_path", "External WFS/control mask FITS path", "paths" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_satMaskPath, "sat_mask_path", "Saturation-check mask FITS path", "paths" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_satThresh, "sat_thresh", 0, 1e7, 1, "%0.1f",
                                "Raw ADU sat threshold in sat_mask", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_psfMaxRef, "psf_max_ref", 0, 1e12, 1, "%0.6g",
                                "Ref-PSF peak / NI normalization", "shared" );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calRegCond, "cal_reg_cond", -20, 0, 0.1, "%0.2f", "beta_reg for control matrix", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calProbeAmp, "cal_probe_amp", 0, 1e-6, 1e-10, "%0.3e", "Calib probe amp [m]", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_calModeAmp, "cal_mode_amp", 0, 1e-6, 1e-10, "%0.3e", "Calib mode amp [m]", "calibrate" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_clProbeAmp, "cl_probe_amp", 0, 1e-6, 1e-10, "%0.3e", "Closed-loop probe amp [m]", "run" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_clIters, "cl_iters", 1, 1000, 1, "%u", "Closed-loop iterations", "run" );
    CREATE_REG_INDI_NEW_NUMBERU( m_indiP_dmResetIndex, "dm_reset_index", 0, 1000000, 1, "%u",
                                 "Archive index loaded by dm_reset", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_clLoopGain, "cl_loop_gain", 0, 2, 0.05, "%0.2f", "Closed-loop gain", "run" );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_clLeakage, "cl_leakage", 0, 1, 0.01, "%0.2f", "Closed-loop leakage", "run" );

    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_reloadPsfRef, "reload_psf_ref" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_darkLibLoad, "reload_dark_lib" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_calibrate, "calibrate" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_calReload, "cal_reload" );
    if( createStandardIndiToggleSw( m_indiP_clRun, "cl_run", "Run closed loop", "run" ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "createStandardIndiToggleSw cl_run" } );
    if( registerIndiPropertyNew( m_indiP_clRun, INDI_NEWCALLBACK( m_indiP_clRun ) ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "registerIndiPropertyNew cl_run" } );
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

    REG_INDI_NEWPROP_NOCB( m_indiP_contrastPosPixels, "contrast_pos_pixels", pcf::IndiProperty::Number );
    m_indiP_contrastPosPixels.add( pcf::IndiElement( "current" ) );
    m_indiP_contrastPosPixels["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_calMode, "cal_mode", pcf::IndiProperty::Number );
    m_indiP_calMode.add( pcf::IndiElement( "current" ) );
    m_indiP_calMode["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_nCalModes, "n_cal_modes", pcf::IndiProperty::Number );
    m_indiP_nCalModes.add( pcf::IndiElement( "current" ) );
    m_indiP_nCalModes["current"].set( 0.0 );

    REG_INDI_NEWPROP_NOCB( m_indiP_clIndex, "cl_index", pcf::IndiProperty::Number );
    m_indiP_clIndex.add( pcf::IndiElement( "current" ) );
    m_indiP_clIndex["current"].set( 0.0 );

    // Seed current/target from config (before INDI starts — use setValue, not updateIfChanged)
    m_indiP_shmCamInput["current"].setValue( m_shmCamInput );
    m_indiP_shmCamInput["target"].setValue( m_shmCamInput );
    m_indiP_shmDm["current"].setValue( m_shmDm );
    m_indiP_shmDm["target"].setValue( m_shmDm );
    m_indiP_shmCamSubNorm["current"].setValue( m_shmCamSubNorm );
    m_indiP_shmCamSubNorm["target"].setValue( m_shmCamSubNorm );
    m_indiP_shmContrastAvg["current"].setValue( m_contrastAvgName );
    m_indiP_shmContrastAvg["target"].setValue( m_contrastAvgName );
    m_indiP_shmDhMask["current"].setValue( m_shmDhMaskName );
    m_indiP_shmDhMask["target"].setValue( m_shmDhMaskName );
    m_indiP_shmSatMask["current"].setValue( m_shmSatMaskName );
    m_indiP_shmSatMask["target"].setValue( m_shmSatMaskName );
    m_indiP_nImages["current"].setValue( m_nImages );
    m_indiP_nImages["target"].setValue( m_nImages );
    m_indiP_camNFrameDelay["current"].setValue( m_camNFrameDelay );
    m_indiP_camNFrameDelay["target"].setValue( m_camNFrameDelay );
    m_indiP_camRDelay["current"].setValue( m_camRDelay );
    m_indiP_camRDelay["target"].setValue( m_camRDelay );
    m_indiP_calDir["current"].setValue( m_calDir );
    m_indiP_calDir["target"].setValue( m_calDir );
    m_indiP_dmCmdPath["current"].setValue( m_dmCmdPath );
    m_indiP_dmCmdPath["target"].setValue( m_dmCmdPath );
    m_indiP_psfDir["current"].setValue( m_psfDir );
    m_indiP_psfDir["target"].setValue( m_psfDir );
    m_indiP_darkLibPath["current"].setValue( m_darkLibPath );
    m_indiP_darkLibPath["target"].setValue( m_darkLibPath );
    m_indiP_camName["current"].setValue( m_camName );
    m_indiP_camName["target"].setValue( m_camName );
    m_indiP_dhMaskPath["current"].setValue( m_dhMaskPath );
    m_indiP_dhMaskPath["target"].setValue( m_dhMaskPath );
    m_indiP_satMaskPath["current"].setValue( m_satMaskPath );
    m_indiP_satMaskPath["target"].setValue( m_satMaskPath );
    m_indiP_satThresh["current"].setValue( m_satThresh );
    m_indiP_satThresh["target"].setValue( m_satThresh );
    m_indiP_psfMaxRef["current"].setValue( 0.0 );
    m_indiP_psfMaxRef["target"].setValue( 0.0 );
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
    m_indiP_dmResetIndex["current"].setValue( m_dmResetIndex );
    m_indiP_dmResetIndex["target"].setValue( m_dmResetIndex );
    m_indiP_clLoopGain["current"].setValue( m_clLoopGain );
    m_indiP_clLoopGain["target"].setValue( m_clLoopGain );
    m_indiP_clLeakage["current"].setValue( m_clLeakage );
    m_indiP_clLeakage["target"].setValue( m_clLeakage );
    m_indiP_clIndex["current"].setValue( m_clIndex );

    // Mirror live camera settings into INDI current via remote SET (registered below).
    REG_INDI_SETPROP( m_indiP_remoteExptime, m_camName, "exptime" );
    REG_INDI_SETPROP( m_indiP_remoteEmgain, m_camName, "emgain" );

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
        if( job == Job::Run )
        {
            // Off may have arrived after queue but before we started.
            if( m_stopRequested.load() )
            {
                m_busy = 0;
                m_stopRequested = false;
                setClRunToggle( false );
                setStatus( "idle" );
                continue;
            }
            state( stateCodes::OPERATING );
            setClRunToggle( true );
        }
        else
        {
            // Fresh non-run job: clear any stale stop from a previous abort.
            m_stopRequested = false;
        }
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
        if( job == Job::Run )
            setClRunToggle( false );
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
        case Job::ReloadPsfRef:
            return doReloadPsfRef();
        case Job::DarkLibLoad:
            return doDarkLibLoad();
        case Job::Calibrate:
            return doCalibrate();
        case Job::CalReload:
            return doCalReload();
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

void iefcCtrl::setClRunToggle( bool on )
{
    updateSwitchIfChanged( m_indiP_clRun, "toggle",
                           on ? pcf::IndiElement::On : pcf::IndiElement::Off,
                           on ? INDI_BUSY : INDI_IDLE );
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

std::string iefcCtrl::dmCmdFitsPath( unsigned index ) const
{
    std::string dir = m_dmCmdPath;
    while( !dir.empty() && dir.back() == '/' )
        dir.pop_back();
    return dir + "/" + m_shmDm + "_cl_" + std::to_string( index ) + ".fits";
}

int iefcCtrl::writeDmCmdArchive( unsigned index, const lina::Array2D<double> &cmd )
{
    if( m_dmCmdPath.empty() )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "dm_cmd_path is empty; cannot archive DM commands" } );
    }
    if( ensureDir( m_dmCmdPath ) < 0 )
        return -1;
    const std::string path = dmCmdFitsPath( index );
    try
    {
        lina::FitsHeader hdr;
        hdr.emplace_back( "SHMDM", "'" + m_shmDm + "'" );
        hdr.emplace_back( "CLINDEX", std::to_string( index ) );
        lina::save_fits( path, cmd, hdr, true );
    }
    catch( const std::exception &e )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__,
              std::string( "DM command FITS write failed (" ) + path + "): " + e.what() } );
    }
    return 0;
}

int iefcCtrl::ensureFlatDmCmdArchive( std::size_t rows, std::size_t cols )
{
    if( m_clIndex != 0 )
        return 0;
    lina::Array2D<double> zeros( rows, cols, 0.0 );
    if( writeDmCmdArchive( 0, zeros ) < 0 )
        return -1;
    updateIfChanged( m_indiP_clIndex, "current", static_cast<double>( m_clIndex ) );
    log<text_log>( "wrote flat DM command " + dmCmdFitsPath( 0 ) );
    return 0;
}

void iefcCtrl::archiveClosedLoopCommand( const lina::Array2D<double> &write_cmd )
{
    const unsigned next = m_clIndex + 1;
    if( writeDmCmdArchive( next, write_cmd ) < 0 )
    {
        throw std::runtime_error( "failed to archive DM command " + dmCmdFitsPath( next ) );
    }
    m_clIndex = next;
    updateIfChanged( m_indiP_clIndex, "current", static_cast<double>( m_clIndex ) );
    log<text_log>( "archived DM command " + dmCmdFitsPath( m_clIndex ) );
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

namespace
{

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

} // namespace

double iefcCtrl::liveCamExp() const
{
    return m_remoteExp;
}

double iefcCtrl::liveCamGain() const
{
    return m_remoteGain;
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
            double live_exptime = liveCamExp();
            std::size_t ncam = 0;
            if( !m_camsciOpen &&
                ImageStreamIO_openIm( &m_camsci, m_shmCamInput.c_str() ) == IMAGESTREAMIO_SUCCESS )
            {
                m_camsciOpen = true;
                m_camsciSem = ImageStreamIO_getsemwaitindex( &m_camsci, 0 );
            }
            if( m_camsciOpen )
                ncam = static_cast<std::size_t>( m_camsci.md->size[0] );
            auto setup = lina::load_setup_dir( m_psfDir, ncam, live_exptime , m_darkLibPath, darkFilter() );
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
    double live_exptime = liveCamExp();
    if( !m_psfDir.empty() &&
        ( m_liveDarkExptime < 0.0 ||
          std::fabs( live_exptime - m_liveDarkExptime ) > lina::kDarkExptimeMatchTol ) &&
        !m_busy.load() )
    {
        try
        {
            auto setup = lina::load_setup_dir( m_psfDir, w, live_exptime , m_darkLibPath, darkFilter() );
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

    // Single pipeline: accumulate NI; every n_images frames publish the
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
        const unsigned nwin = m_nImages < 1 ? 1 : m_nImages;

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

        // Non-overlapping block: cadence = n_images camera frames.
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
        const double pos_pct =
            cr.n_mask == 0 ? 0.0
                           : 100.0 * static_cast<double>( cr.n_positive ) /
                                 static_cast<double>( cr.n_mask );
        updateIfChanged( m_indiP_contrastPosPixels, "current", pos_pct );
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

int iefcCtrl::doReloadPsfRef()
{
    setStatus( "reload_psf_ref: starting" );
    log<text_log>( "reload_psf_ref psf_dir=" + m_psfDir );

    if( m_psfDir.empty() )
        return log<software_error, -1>( { __FILE__, __LINE__, "psf_dir is empty" } );

    // Try to open camsci for ncam size; if it fails, try 0 and let load_setup_dir handle it.
    uint32_t ncam = 0;
    if( openCamsci() == 0 )
    {
        ncam = static_cast<uint32_t>( m_camsci.md->size[0] );
    }
    else
    {
        log<text_log>( "reload_psf_ref: could not open camsci for size, assuming ncam=0",
                       logPrio::LOG_WARNING );
    }

    const double live_exptime = liveCamExp();
    log<text_log>( "reload_psf_ref live_exptime=" + std::to_string( live_exptime ) + " s" );

    lina::SetupData setup;
    try
    {
        setup = lina::load_setup_dir( m_psfDir, ncam, live_exptime, m_darkLibPath, darkFilter() );
    }
    catch( const std::exception &e )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "failed to load setup from " ) + m_psfDir + ": " + e.what() } );
    }

    // Always recompute peak from package dark-sub (or avg-dark) rather than trusting config only.
    double peak = -1.0;
    std::string psf_dark_sub_path = m_psfDir + "/ref_psf_dark_sub.fits";
    try
    {
        lina::Array2D<double> psf_sub = lina::load_fits_double( psf_dark_sub_path );
        peak = -1e30;
        for( size_t i = 0; i < psf_sub.size(); ++i )
        {
            if( psf_sub.data()[i] > peak )
                peak = psf_sub.data()[i];
        }
        log<text_log>( "reload_psf_ref: loaded " + psf_dark_sub_path + " peak=" + std::to_string( peak ) );
    }
    catch( const std::exception &e )
    {
        // Fall back to ref_psf_avg.fits minus setup.dark.
        log<text_log>( "reload_psf_ref: ref_psf_dark_sub.fits not found, loading ref_psf_avg.fits" );
        std::string psf_avg_path = m_psfDir + "/ref_psf_avg.fits";
        try
        {
            lina::Array2D<double> psf_avg = lina::load_fits_double( psf_avg_path );
            if( psf_avg.size() != setup.dark.size() )
                return log<software_error, -1>(
                    { __FILE__, __LINE__, "ref_psf_avg size does not match setup.dark" } );
            peak = -1e30;
            for( size_t i = 0; i < psf_avg.size(); ++i )
            {
                double val = psf_avg.data()[i] - setup.dark.data()[i];
                if( val > peak )
                    peak = val;
            }
            log<text_log>( "reload_psf_ref: computed peak from avg-dark=" + std::to_string( peak ) );
        }
        catch( const std::exception &e2 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, std::string( "failed to load ref_psf_avg.fits: " ) + e2.what() } );
        }
    }

    setup.Imax_ref = peak;

    if( peak >= m_satThresh )
    {
        log<text_log>( "reload_psf_ref: peak " + std::to_string( peak ) +
                       " >= sat_thresh " + std::to_string( m_satThresh ),
                       logPrio::LOG_WARNING );
    }

    m_imaxRefManual = false;  // Package reload owns the scale.
    updateLiveNormFromSetup( setup );
    resetContrastAccumulator();

    updateIfChanged( m_indiP_psfMaxRef, "current", peak );
    updateIfChanged( m_indiP_psfMaxRef, "target", peak );

    log<text_log>( "reload_psf_ref done psf_max_ref=" + std::to_string( peak ) );
    setStatus( "reload_psf_ref: done" );
    return 0;
}

int iefcCtrl::doDmReset()
{
    const unsigned idx = m_dmResetIndex;
    setStatus( "dm_reset: restoring " + m_shmDm + " from index " + std::to_string( idx ) );
    log<text_log>( "dm_reset: loading " + dmCmdFitsPath( idx ) );

    try
    {
        lina::ShmimStream dm( m_shmDm );
        lina::Array2D<double> cmd;

        const std::string path = dmCmdFitsPath( idx );
        const bool exists = !m_dmCmdPath.empty() && ::access( path.c_str(), F_OK ) == 0;

        if( !exists )
        {
            if( idx != 0 )
            {
                setStatus( "dm_reset: failed" );
                return log<software_error, -1>(
                    { __FILE__, __LINE__, "dm_reset: missing " + path } );
            }
            cmd = lina::Array2D<double>( dm.rows(), dm.cols(), 0.0 );
            if( writeDmCmdArchive( 0, cmd ) < 0 )
            {
                setStatus( "dm_reset: failed" );
                return -1;
            }
            log<text_log>( "dm_reset: wrote missing flat " + dmCmdFitsPath( 0 ) );
        }
        else
        {
            cmd = lina::load_fits_double( path );
        }

        if( cmd.rows() != dm.rows() || cmd.cols() != dm.cols() )
        {
            setStatus( "dm_reset: failed" );
            return log<software_error, -1>(
                { __FILE__, __LINE__,
                  "dm_reset: " + path + " is " + std::to_string( cmd.rows() ) + "x" +
                      std::to_string( cmd.cols() ) + ", shmim is " +
                      std::to_string( dm.rows() ) + "x" + std::to_string( dm.cols() ) } );
        }

        dm.write( cmd );
        m_clIndex = idx;
        updateIfChanged( m_indiP_clIndex, "current", static_cast<double>( m_clIndex ) );

        log<text_log>( "dm_reset: restored " + m_shmDm + " from " + path +
                       " (cl_index=" + std::to_string( m_clIndex ) + ")" );
        setStatus( "dm_reset: done" );
        return 0;
    }
    catch( const std::exception &e )
    {
        setStatus( "dm_reset: failed" );
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "dm_reset: " ) + e.what() } );
    }
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


lina::DarkMatchFilter iefcCtrl::darkFilter( double gain ) const
{
    lina::DarkMatchFilter f;
    f.shm_cam_input = m_shmCamInput;
    const double g = std::isfinite( gain ) ? gain : m_remoteGain;
    if( std::isfinite( g ) )
        f.gain = g;
    return f;
}

int iefcCtrl::doDarkLibLoad()
{
    setStatus( "reload_dark_lib: starting" );
    if( m_darkLibPath.empty() )
    {
        setStatus( "reload_dark_lib: failed" );
        return log<software_error, -1>( { __FILE__, __LINE__, "dark_lib_path is empty" } );
    }
    if( m_shmCamInput.empty() )
    {
        setStatus( "reload_dark_lib: failed" );
        return log<software_error, -1>( { __FILE__, __LINE__, "shm_cam_input is empty" } );
    }

    try
    {
        const auto all = lina::load_dark_library_manifest( m_darkLibPath );
        if( all.empty() )
        {
            setStatus( "reload_dark_lib: failed" );
            return log<software_error, -1>(
                { __FILE__, __LINE__,
                  "no dark_metadata.txt entries in " + absPath( m_darkLibPath ) } );
        }
        const auto matched = lina::filter_dark_library_entries( all, darkFilter() );
        if( matched.empty() )
        {
            setStatus( "reload_dark_lib: failed" );
            return log<software_error, -1>(
                { __FILE__, __LINE__,
                  "no darks for shm_cam_input=" + m_shmCamInput + " in " +
                      absPath( m_darkLibPath ) + " (total entries=" +
                      std::to_string( all.size() ) + ")" } );
        }

        std::ostringstream oss;
        oss << "reload_dark_lib: " << matched.size() << "/" << all.size()
            << " entries for shm_cam_input=" << m_shmCamInput << " in " << absPath( m_darkLibPath )
            << " (exptime range ";
        double tmin = matched.front().exptime, tmax = matched.front().exptime;
        for( const auto &e : matched )
        {
            tmin = std::min( tmin, e.exptime );
            tmax = std::max( tmax, e.exptime );
        }
        oss << tmin << " … " << tmax << " s)";
        log<text_log>( oss.str() );
        setStatus( "reload_dark_lib: done (" + std::to_string( matched.size() ) + " darks)" );
        return 0;
    }
    catch( const std::exception &e )
    {
        setStatus( "reload_dark_lib: failed" );
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "reload_dark_lib: " ) + e.what() } );
    }
}

int iefcCtrl::doCalReload()
{
    setStatus( "cal_reload: starting" );
    if( m_calDir.empty() )
    {
        setStatus( "cal_reload: failed" );
        return log<software_error, -1>( { __FILE__, __LINE__, "cal_dir is empty" } );
    }

    try
    {
        lina::PackagePaths pkg;
        pkg.dir = m_calDir;

        struct stat st {};
        if( ::stat( pkg.response_path().c_str(), &st ) != 0 || !S_ISREG( st.st_mode ) )
        {
            setStatus( "cal_reload: failed" );
            return log<software_error, -1>(
                { __FILE__, __LINE__,
                  "cal_reload: missing " + absPath( pkg.response_path() ) } );
        }

        setStatus( "cal_reload: loading modes / mask" );
        // Geometry must match package FITS cubes / mask; prefer live shmims, else config.txt.
        std::size_t ncam = 0;
        std::size_t nact = 0;
        try
        {
            lina::ShmimStream camsci( m_shmCamInput );
            ncam = camsci.rows();
        }
        catch( const std::exception &e )
        {
            log<text_log>( std::string( "cal_reload: camsci unavailable (" ) + e.what() +
                               "); will use package config",
                           logPrio::LOG_WARNING );
        }
        try
        {
            lina::ShmimStream dm( m_shmDm );
            nact = dm.rows();
        }
        catch( const std::exception &e )
        {
            log<text_log>( std::string( "cal_reload: shm_dm unavailable (" ) + e.what() +
                               "); will use package config",
                           logPrio::LOG_WARNING );
        }
        if( ncam == 0 || nact == 0 )
        {
            try
            {
                const auto cfg = lina::read_config( pkg.config_path() );
                if( ncam == 0 )
                    ncam = lina::cfg_z( cfg, "ncamsci", 0 );
                if( nact == 0 )
                    nact = lina::cfg_z( cfg, "nact", 0 );
            }
            catch( const std::exception &e )
            {
                setStatus( "cal_reload: failed" );
                return log<software_error, -1>(
                    { __FILE__, __LINE__,
                      std::string( "cal_reload: cannot resolve ncam/nact: " ) + e.what() } );
            }
        }
        if( ncam == 0 || nact == 0 )
        {
            setStatus( "cal_reload: failed" );
            return log<software_error, -1>(
                { __FILE__, __LINE__,
                  "cal_reload: ncam/nact unknown (open camsci+shm_dm or provide cal_dir/config.txt)" } );
        }

        auto in = lina::default_loop_inputs( ncam, nact );
        lina::load_modes_from_package( in, pkg );
        if( in.control_mask.size() == 0 )
        {
            setStatus( "cal_reload: failed" );
            return log<software_error, -1>(
                { __FILE__, __LINE__, "cal_reload: package has empty control mask" } );
        }

        double live_exptime = liveCamExp();

        lina::SetupData setupData;
        if( !m_psfDir.empty() )
        {
            try
            {
                setupData = lina::load_setup_dir( m_psfDir, ncam, live_exptime , m_darkLibPath, darkFilter() );
            }
            catch( const std::exception &e )
            {
                log<text_log>( std::string( "cal_reload: psf_dir setup skipped: " ) + e.what(),
                               logPrio::LOG_WARNING );
            }
        }
        if( !setupData.loaded )
        {
            try
            {
                setupData = lina::load_setup_from_package( pkg, ncam, live_exptime , m_darkLibPath, darkFilter() );
            }
            catch( const std::exception &e )
            {
                log<text_log>( std::string( "cal_reload: package dark/setup skipped: " ) +
                                   e.what(),
                               logPrio::LOG_WARNING );
            }
        }

        setStatus( "cal_reload: loading response matrix" );
        auto response = lina::load_matrix( pkg.response_path() );
        log<text_log>( "cal_reload: response " + std::to_string( response.rows() ) + "x" +
                       std::to_string( response.cols() ) + " from " +
                       absPath( pkg.response_path() ) );

        // Force disk/build path for current cal_reg_cond (ignore stale memory).
        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            m_haveCalibration = false;
            m_cachedControl = {};
            m_cachedRegCond = std::numeric_limits<float>::quiet_NaN();
            m_cachedResponse = {};
            m_cachedResponseFull = {};
        }

        lina::Array2D<double> control;
        if( loadOrBuildControl( response, m_calRegCond, control ) < 0 )
        {
            setStatus( "cal_reload: failed" );
            return -1;
        }

        lina::Array2D<double> response_full;
        bool have_full = false;
        try
        {
            response_full = lina::load_response_full( pkg, ncam, in.calib_modes.rows(),
                                                      in.probe_modes.rows() );
            have_full = response_full.size() > 0;
            log<text_log>( "cal_reload: loaded response_full for remask support" );
        }
        catch( const std::exception &e )
        {
            log<text_log>( std::string( "cal_reload: response_full not loaded (" ) + e.what() +
                               "); dh_mask_reload remask after this load may need recalibrate",
                           logPrio::LOG_WARNING );
        }

        {
            std::lock_guard<std::mutex> lock( m_calMutex );
            m_cachedResponse = std::move( response );
            if( have_full )
                m_cachedResponseFull = std::move( response_full );
            // control + reg set by loadOrBuildControl
            m_cachedProbeModes = in.probe_modes;
            m_cachedCalibModes = in.calib_modes;
            m_cachedMask = in.control_mask;
            if( setupData.loaded )
            {
                m_cachedDark = setupData.dark;
                m_cachedImaxRef = setupData.Imax_ref;
                m_cachedPsfExptime = setupData.psf_exptime;
                m_cachedGain = setupData.gain;
            }
            m_haveCalibration = true;
            m_haveUserDhMask = false;
        }

        m_liveContrastMask = in.control_mask;
        m_haveContrastMask = true;
        (void)publishDhMask( in.control_mask );
        if( setupData.loaded )
            updateLiveNormFromSetup( setupData );
        resetContrastAccumulator();

        updateIfChanged( m_indiP_nCalModes, "current",
                         static_cast<double>( in.calib_modes.rows() ) );
        updateIfChanged( m_indiP_calMode, "current", 0.0 );
        if( setupData.loaded && setupData.Imax_ref > 0.0 && !m_imaxRefManual )
        {
            updateIfChanged( m_indiP_psfMaxRef, "current", setupData.Imax_ref );
            updateIfChanged( m_indiP_psfMaxRef, "target", setupData.Imax_ref );
        }

        setStatus( "cal_reload: done" );
        log<text_log>( "cal_reload: loaded package from " + absPath( m_calDir ) +
                       " (control for cal_reg_cond=" + formatSci( m_calRegCond ) +
                       ", nmodes=" + std::to_string( in.calib_modes.rows() ) + ")" );
        return 0;
    }
    catch( const std::exception &e )
    {
        setStatus( "cal_reload: failed" );
        return log<software_error, -1>(
            { __FILE__, __LINE__, std::string( "cal_reload: " ) + e.what() } );
    }
}

int iefcCtrl::doCalibrate()
{
    setStatus( "calibrate: starting" );
    try
    {
        lina::ShmimStream camsci( m_shmCamInput );
        lina::ShmimStream dm( m_shmDm );

        double live_exptime = liveCamExp();

        auto in = lina::default_loop_inputs( camsci.rows(), dm.rows() );
        in.nframes = m_nImages < 1 ? 1 : m_nImages;
        resolveCamSettle( in.wait_frames, in.delay_s );
        in.calib_probe_amp = m_calProbeAmp;
        in.calib_amp = m_calModeAmp;
        in.reg_cond = m_calRegCond;

        if( m_psfDir.empty() )
            return log<software_error, -1>( { __FILE__, __LINE__, "psf_dir required" } );
        auto setup = lina::load_setup_dir( m_psfDir, camsci.rows(), live_exptime , m_darkLibPath, darkFilter() );
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

        double live_exptime = liveCamExp();

        auto in = lina::default_loop_inputs( camsci.rows(), dm.rows() );
        in.nframes = m_nImages < 1 ? 1 : m_nImages;
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
                setupData = lina::load_setup_dir( m_psfDir, camsci.rows(), live_exptime , m_darkLibPath, darkFilter() );
            else
                setupData = lina::load_setup_from_package( pkg, camsci.rows(), live_exptime , m_darkLibPath, darkFilter() );
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
                        lina::load_setup_dir( m_psfDir, camsci.rows(), live_exptime , m_darkLibPath, darkFilter() );
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

        if( ensureFlatDmCmdArchive( dm.rows(), dm.cols() ) < 0 )
            return -1;

        setStatus( "run: closed loop" );
        lina::run( data, camsci, in.nframes, dm, in.im_params, in.ref_params, setupData.dark,
                   control, in.run_probe_amp, in.probe_modes, in.calib_modes, in.control_mask,
                   in.delay_s, in.num_iters, in.gain, in.leakage, in.dm_scale, in.wait_frames,
                   makeStopCheck(),
                   [this]( const lina::Array2D<double> &write_cmd ) {
                       archiveClosedLoopCommand( write_cmd );
                   } );

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



INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_nImages )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_nImages, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_nImages, target, ipRecv, false ) < 0 )
        return -1;
    if( target < 1 )
        target = 1;
    if( target != m_nImages )
        log<text_log>( "n_images: " + std::to_string( m_nImages ) + " -> " +
                       std::to_string( target ) + " (resets NI block accumulator)" );
    {
        std::lock_guard<std::mutex> lock( m_contrastAvgMutex );
        m_nImages = target;
    }
    resetContrastAccumulator();
    updateIfChanged( m_indiP_nImages, "current", m_nImages );
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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_dmCmdPath )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dmCmdPath, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_dmCmdPath, target, ipRecv, false ) < 0 )
        return -1;
    if( target.empty() || target == m_dmCmdPath )
    {
        updateIfChanged( m_indiP_dmCmdPath, "current", m_dmCmdPath );
        updateIfChanged( m_indiP_dmCmdPath, "target", m_dmCmdPath );
        return 0;
    }
    if( m_busy.load() )
    {
        log<text_log>( "cannot change dm_cmd_path while busy", logPrio::LOG_WARNING );
        updateIfChanged( m_indiP_dmCmdPath, "current", m_dmCmdPath );
        updateIfChanged( m_indiP_dmCmdPath, "target", m_dmCmdPath );
        return 0;
    }
    log<text_log>( "dm_cmd_path: " + m_dmCmdPath + " -> " + target );
    m_dmCmdPath = target;
    updateIfChanged( m_indiP_dmCmdPath, "current", m_dmCmdPath );
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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_dmResetIndex )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dmResetIndex, ipRecv );
    unsigned target;
    if( indiTargetUpdate( m_indiP_dmResetIndex, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_dmResetIndex )
        log<text_log>( "dm_reset_index: " + std::to_string( m_dmResetIndex ) + " -> " +
                       std::to_string( target ) );
    m_dmResetIndex = target;
    updateIfChanged( m_indiP_dmResetIndex, "current", m_dmResetIndex );
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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_reloadPsfRef )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_reloadPsfRef, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_reloadPsfRef, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::ReloadPsfRef );
        clearRequest( m_indiP_reloadPsfRef );
    }
    return 0;
}


INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_darkLibPath )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkLibPath, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_darkLibPath, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_darkLibPath )
        log<text_log>( "dark_lib_path: " + m_darkLibPath + " -> " + target );
    m_darkLibPath = target;
    updateIfChanged( m_indiP_darkLibPath, "current", m_darkLibPath );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_camName )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camName, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_camName, target, ipRecv, false ) < 0 )
        return -1;
    if( target != m_camName )
        log<text_log>( "cam_name: " + m_camName + " -> " + target +
                       " (restart app to rebind SET subscription)",
                       logPrio::LOG_WARNING );
    m_camName = target;
    updateIfChanged( m_indiP_camName, "current", m_camName );
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_darkLibLoad )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_darkLibLoad, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_darkLibLoad, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::DarkLibLoad );
        clearRequest( m_indiP_darkLibLoad );
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

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_calReload )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_calReload, ipRecv );
    if( !ipRecv.find( "request" ) )
        return -1;
    if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
    {
        updateSwitchIfChanged( m_indiP_calReload, "request", pcf::IndiElement::On, INDI_BUSY );
        queueJob( Job::CalReload );
        clearRequest( m_indiP_calReload );
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN( iefcCtrl, m_indiP_clRun )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_clRun, ipRecv );
    if( !ipRecv.find( "toggle" ) )
        return 0;

    if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
    {
        if( m_busy.load() )
        {
            log<text_log>( "cl_run: already busy; ignoring", logPrio::LOG_WARNING );
            setClRunToggle( false );
            return 0;
        }
        setClRunToggle( true );
        state( stateCodes::OPERATING );
        queueJob( Job::Run );
        return 0;
    }

    // Toggle Off: abort a pending or in-flight closed loop.
    queueJob( Job::Stop );
    setClRunToggle( false );
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


INDI_SETCALLBACK_DEFN( iefcCtrl, m_indiP_remoteExptime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteExptime, ipRecv );
    parseIndiCurrentNumber( ipRecv, m_remoteExp );
    return 0;
}

INDI_SETCALLBACK_DEFN( iefcCtrl, m_indiP_remoteEmgain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteEmgain, ipRecv );
    parseIndiCurrentNumber( ipRecv, m_remoteGain );
    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // iefcCtrl_hpp
