/** \file alignPSFCtrl.hpp
  * \brief The MagAO-X PSF alignment controller header file
  *
  * Measure PSF displacement (delta_x, delta_y) relative to the coronagraph
  * mask center, then publishes those deltas for closedLoopIndi to control FSM
  * and align the PSF onto the coronagraph mask.
  *
  * Three displacement measurement methods are available, selected via the
  * writable INDI property "method" (or align.method in the config for the
  * default at startup):
  *
  *  Method 1 (method=1, default): Restricted-area cross-correlation.
  *    Uses imageXCorrFFT to compute a full cross-correlation image, then
  *    searches only a restricted box around the current estimated PSF shift
  *    for the peak. On the first frame the initial shift is bootstrapped
  *    from speckle centroids (same geometry as Method 2); subsequent frames
  *    track iteratively within the restricted box.
  *    Key parameters: align.maxLag, align.searchHalfWidth, align.speckleSep,
  *    align.speckleAngle, align.speckleSearchR.
  *
  *  Method 2 (method=2): Speckle centroid.
  *    Detects four DM speckles in each camera frame and computes the mean
  *    displacement from their reference positions. Unaffected by whether the
  *    central PSF core is visible or suppressed.
  *    Key parameters: align.speckleSep, align.speckleAngle, align.speckleSearchR.
  *
  *  Method 3 (method=3): Intensity-minimising raster scan.
  *    Intended for fine alignment once Methods 1 or 2 have brought the PSF
  *    close to the mask centre.  Commands a (2H+1)x(2H+1) grid of offsets via
  *    the existing closedLoopIndi measurement interface, measures the total
  *    pixel intensity inside a circular aperture centred on the mask centre at
  *    each position, fits a 2-D quadratic surface to the collected data, and
  *    holds at the fitted minimum thereafter.  The scan is triggered by
  *    switching the "method" INDI property to 3 from an external app; the
  *    external app is also responsible for adjusting the closedLoopIndi gain
  *    to a value appropriate for raster positioning speed before the switch.
  *    Key parameters: align.rasterHalfSteps, align.rasterStepSize,
  *    align.rasterSettleFrames, align.rasterMeasureFrames,
  *    align.rasterCenterRadius.
  *
  * This app monitors two shared memory images:
  *   - [shmimMonitor].shmimName : the science camera stream
  *   - [refIm].shmimName : a reference image shmim (the "where we want to be"
  *                         template, written once when the PSF is centered)
  *
  * The reference can be updated at any time by writing a new frame to the
  * reference shmim -- the app picks it up without restarting.
  *
  * --- DM setup: write a sine cross to create speckle fiducials ---
  *
  * Run this Python snippet once (before setting the reference image and
  * before starting this app).
  *
  *  dm_mask = dm.create_mask(Nact=50)
  *  reload(dm)
  *
  *  waffle_command = dm.make_fourier_command(x_cpa=25, y_cpa=25, Nact=50) * dm_mask
  *  pattern = 5e-8 * waffle_command
  *
  *  utils.imshow([dm_mask, pattern], titles=['DM Mask', 'DM Command Pattern'])
  *  write_dm_pattern(pattern)
  *
  * To clear the pattern afterwards:
  *   pattern = np.zeros_like(waffle_command)
  *   write_dm_pattern(pattern)
  *
  * --- DM setup: write a sine waffle to create speckle fiducials ---
  *
  *  dm_mask = dm.create_mask(Nact=50)
  *  waffle_command = dm.make_fourier_command(x_cpa=25, y_cpa=25, Nact=50) * dm_mask
  *  write_dm_pattern(5e-8 * waffle_command)
  *
  * --- Setting the reference image ---
  *
  * With the PSF centered on the mask and the sine pattern active:
  *
  *  import ImageStreamIOWrap as shmio
  *  REF_SHM = "alignPSFCtrl_ref"
  *  camlo, camsci = grab_frames()
  *  ref = shmio.Image()
  *  ref.create(REF_SHM, camsci.astype(np.float32))
  *
  * To update the reference live, call ref.write(new_frame) on the same object.
  *
  * \ingroup alignPSFCtrl_files
  */

#ifndef alignPSFCtrl_hpp
#define alignPSFCtrl_hpp

#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <utility>

#include <mx/improc/imageXCorrFFT.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

using namespace mx::improc;

/** \defgroup alignPSFCtrl
  * \brief PSF-to-coronagraph-mask alignment via FFT cross-correlation or speckle centroid
  *
  * <a href="../handbook/operating/software/apps/alignPSFCtrl.html">Application Documentation</a>
  *
  * \ingroup apps
  */

/** \defgroup alignPSFCtrl_files
  * \ingroup alignPSFCtrl
  */

namespace MagAOX
{
namespace app
{

/// shmimMonitor tag type for the cross-correlation reference image shmim.
struct refShmimT
{
   static std::string configSection() { return "refIm"; }
   static std::string indiPrefix() { return "refIm"; }
};

/// The MagAO-X PSF alignment controller
/**
  * \ingroup alignPSFCtrl
  */
class alignPSFCtrl : public MagAOXApp<true>,
                     public dev::shmimMonitor<alignPSFCtrl>,
                     public dev::shmimMonitor<alignPSFCtrl, refShmimT>
{
   //Give the test harness access.
   friend class alignPSFCtrl_test;

   friend class dev::shmimMonitor<alignPSFCtrl>;
   friend class dev::shmimMonitor<alignPSFCtrl, refShmimT>;

   typedef dev::shmimMonitor<alignPSFCtrl> shmimMonitorT;
   typedef dev::shmimMonitor<alignPSFCtrl, refShmimT> refShmimMonitorT;

protected:

   /** \name Configurable Parameters
     *@{
     */

   int m_method{1};               ///< Displacement measurement method: 1=xcorr, 2=speckle centroid, 3=raster. Default 1.

   int m_maxLag{10};               ///< Method 1: Maximum lag (pixels) passed to imageXCorrFFT. Default 10.
   float m_xcorrTol{0.001f};       ///< Method 1: Interpolation tolerance for imageXCorrFFT. Default 0.001.
   float m_xcorrPadFactor{1.0f};   ///< Method 1: Padding factor for the reference FFT. Default 1.
   int m_searchHalfWidth{5};       ///< Method 1: xcorr search box half-width (pixels). Default 5.

   float m_speckleSep{50.0f};      ///< Methods 1&2: speckle distance from PSF centre (pixels). Default 50.

   float m_speckleAngle{45.0f};    ///< Methods 1&2: angle of first speckle from image row axis (degrees). Default 45.
   int m_speckleSearchR{8};        ///< Methods 1&2: local peak search radius around each speckle (pixels). Default 8.

   int m_minSpeckles{4};           ///< Methods 1&2: minimum speckles required per frame. Default 4.

   float m_predGain{1.0f};         ///< Methods 1&2: assumed loop gain for predicting next-frame PSF position. Default 1.

   /// Where the current mask centre came from, in ascending priority (config wins).
   enum class maskCenterSource { imageCentre, reference, config };
   maskCenterSource m_maskCenterSource{maskCenterSource::imageCentre};

   float m_maskCenterX{-1.0f};     ///< Coronagraph mask centre row (pixels).
   float m_maskCenterY{-1.0f};     ///< Coronagraph mask centre col (pixels).

   int   m_rasterHalfSteps{3};     ///< Method 3: half-number of steps per axis; total scan = (2H+1)^2. Default 3.
   float m_rasterStepSize{2.0f};   ///< Method 3: pixel step size between raster points. Default 2.
   int   m_rasterSettleFrames{10}; ///< Method 3: camera frames to discard after each FSM step. Default 10.
   int   m_rasterMeasureFrames{5}; ///< Method 3: camera frames to average per raster point. Default 5.
   float m_rasterCenterRadius{20.0f}; ///< Method 3: radius (pixels) of central aperture for intensity sum. Default 20.

   ///@}

   mx::improc::imageXCorrFFT<eigenImage<float>> m_xcorr; ///< The cross-correlator.

   eigenImage<float> m_camImage; ///< Float buffer for the current camera frame.
   eigenImage<float> m_refImage; ///< Float buffer for the reference frame.

   float (*m_camPixget)(void *, size_t) {nullptr}; ///< Pixel accessor for the camera shmim.
   float (*m_refPixget)(void *, size_t) {nullptr}; ///< Pixel accessor for the reference shmim.

   bool m_refImLoaded{false};    ///< True once a reference frame has been received and set.
   int64_t m_counter{0};         ///< Monotonically increasing frame counter published to INDI.
   bool m_loopClosed{false};     ///< Whether the loop is closed.

   // --- PSF tracking state (Methods 1 & 2) ---
   bool m_initialized{false};    ///< True after first-frame PSF detection.
   float m_psfEstX{0.0f};        ///< Predicted PSF shift in X for the next frame (pixels).
   float m_psfEstY{0.0f};        ///< Predicted PSF shift in Y for the next frame (pixels).

   // --- Raster scan state (Method 3) ---
   static constexpr int RASTER_IDLE      = 0; ///< Scan not started.
   static constexpr int RASTER_SETTLING  = 1; ///< Waiting for FSM to settle after each step.
   static constexpr int RASTER_MEASURING = 2; ///< Accumulating intensity frames at current position.
   static constexpr int RASTER_FITTING   = 3; ///< All points collected; running 2-D quadratic fit.
   static constexpr int RASTER_DONE      = 4; ///< Fit complete; holding at found minimum.

   int   m_rasterState{RASTER_IDLE};   ///< Current raster state machine state.
   int   m_rasterIx{0};                ///< X-step index for the next position (−halfSteps … +halfSteps).
   int   m_rasterIy{0};                ///< Y-step index for the next position.
   int   m_rasterFrameCount{0};        ///< Frame counter within current settle/measure phase.
   float m_rasterAccum{0.0f};          ///< Accumulated intensity for the current raster point.
   float m_rasterHoldX{0.0f};          ///< X offset currently being held during settle/measure (pixels).
   float m_rasterHoldY{0.0f};          ///< Y offset currently being held during settle/measure (pixels).
   float m_rasterMinX{0.0f};           ///< Fitted X position of intensity minimum (pixels from mask centre).
   float m_rasterMinY{0.0f};           ///< Fitted Y position of intensity minimum (pixels from mask centre).

   struct RasterPoint { float x; float y; float intensity; }; ///< One raster measurement sample.
   std::vector<RasterPoint> m_rasterData; ///< All (x, y, intensity) samples collected so far.

   // --- INDI ---
   pcf::IndiProperty m_indiP_measurement;  ///< Read-only: delta_x, delta_y, counter.
   pcf::IndiProperty m_indiP_ctrlEnabled;  ///< Toggle: close/open the measurement loop.
   INDI_NEWCALLBACK_DECL(alignPSFCtrl, m_indiP_ctrlEnabled);

   pcf::IndiProperty m_indiP_method;       ///< Writable: active method (1/2/3).
   INDI_NEWCALLBACK_DECL(alignPSFCtrl, m_indiP_method);

   pcf::IndiProperty m_indiP_rasterStatus; ///< Read-only: raster scan state and results.

public:
   /// Default c'tor.
   alignPSFCtrl();

   /// D'tor, declared and defined for noexcept.
   ~alignPSFCtrl() noexcept
   {}

   virtual void setupConfig();

   /// Implementation of loadConfig logic, separated for testing.
   /** This is called by loadConfig().
     */
   int loadConfigImpl( mx::app::appConfigurator & _config /**< [in] an application configuration from which to load values*/);

   virtual void loadConfig();

   /// Startup function
   /** Set up INDI props & other startup prep
     *
     */
   virtual int appStartup();

   /// Implementation of the logic for alignPSFCtrl.
   /**
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
   virtual int appLogic();

   /// Shutdown the app.
   virtual int appShutdown();

   /// Change the loop state.
   int toggleLoop( bool onoff );

   /** \name shmimMonitor Interface -- camera stream
     * @{
     */
   int allocate( const dev::shmimT & );
   int processImage( void *curr_src, const dev::shmimT & );
   ///@}

   /** \name shmimMonitor Interface -- reference image
     * @{
     */
   int allocate( const refShmimT & );
   int processImage( void *curr_src, const refShmimT & );
   ///@}

protected:

   /** \name PSF detection helpers
     * @{
     */

   /// Find the local intensity peak nearest to (eX, eY) within a square half-width searchR.
   /** A 3×3 intensity-weighted centroid is applied for sub-pixel accuracy.
     * \returns true on success, false if the expected position lies outside the image.
     */
   bool findLocalPeak( const eigenImage<float> & im,
                       float eX, float eY, int searchR,
                       float & foundX, float & foundY );

   /// Find the four first-order DM speckles in an image by thresholding,
   /// clustering, and geometric consistency filtering.
   /** Searches the entire image for 4 clusters whose
     * centroids form a square of side ~= speckleSep * sqrt(2), which is the
     * geometry produced by the DM sine/waffle pattern.
     * \param[in] im        the image to search
     * \param[out] foundX   row coordinates of the 4 found speckle centroids
     * \param[out] foundY   col coordinates of the 4 found speckle centroids
     * \returns 0 on success, -1 if a consistent group of 4 cannot be found.
     */
   int findFirstOrderSpeckles( const eigenImage<float> & im,
                                float foundX[4], float foundY[4] );

   /// Sum pixel values inside a circular aperture centred on (cx, cy).
   /** Used by Method 3 to measure PSF suppression at each raster position.
     * The mask centre coordinates are used for (cx, cy); the aperture is
     * fixed in the image regardless of where the PSF has been commanded.
     * \returns total intensity within radius pixels of (cx, cy).
     */
   float measureCentralIntensity( const eigenImage<float> & im,
                                   float cx, float cy, float radius );

   /// Fit a 2-D quadratic surface to the raster scan data and find its minimum.
   /** Solves: I(x,y) = a*x^2 + b*y^2 + c*x*y + d*x + e*y + f in least squares,
     * then solves the 2x2 linear system for the minimum analytically.  Checks
     * that the fitted Hessian is positive-definite (true minimum, not a saddle
     * or maximum); if not, falls back to the raw argmin of the measured data.
     * The result is clamped to the scanned range in each axis.
     * Sets m_rasterMinX and m_rasterMinY on success.
     * \returns 0 on success, -1 if there are too few data points to fit.
     */
   int fitRaster();

   ///@}
};

alignPSFCtrl::alignPSFCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   // Load any existing reference image rather than waiting for a new post.
   refShmimMonitorT::m_getExistingFirst = true;
}

void alignPSFCtrl::setupConfig()
{
   SHMIMMONITOR_SETUP_CONFIG(config);
   SHMIMMONITORT_SETUP_CONFIG(refShmimMonitorT, config);

   config.add("align.method", "", "align.method", argType::Optional, "align", "method", false, "int", "Displacement measurement method: 1=restricted xcorr (default), 2=speckle centroid, 3=raster scan.");
   config.add("align.maxLag", "", "align.maxLag", argType::Optional, "align", "maxLag", false, "int", "Maximum lag in pixels for cross-correlation. Default 10.");
   config.add("align.tol", "", "align.tol", argType::Optional, "align", "tol", false, "float", "Interpolation tolerance for cross-correlation peak. Default 0.001.");
   config.add("align.padFactor", "", "align.padFactor", argType::Optional, "align", "padFactor", false, "float", "Padding factor for the reference FFT. Default 1.");
   config.add("align.searchHalfWidth", "", "align.searchHalfWidth", argType::Optional, "align", "searchHalfWidth", false, "int", "Method 1: xcorr search box half-width in pixels. Default 5.");
   config.add("align.speckleSep", "", "align.speckleSep", argType::Optional, "align", "speckleSep", false, "float", "Methods 1&2: speckle distance from PSF centre in pixels. Default 50.");
   config.add("align.speckleAngle", "", "align.speckleAngle", argType::Optional, "align", "speckleAngle", false, "float", "Methods 1&2: angle of first speckle from row axis in degrees. Default 45.");
   config.add("align.speckleSearchR", "", "align.speckleSearchR", argType::Optional, "align", "speckleSearchR", false, "int", "Methods 1&2: search radius around each expected speckle position. Default 8.");
   config.add("align.minSpeckles", "", "align.minSpeckles", argType::Optional, "align", "minSpeckles",  false, "int", "Methods 1&2: minimum speckles required per frame. Default 4.");
   config.add("align.predGain", "", "align.predGain", argType::Optional, "align", "predGain",    false, "float", "Methods 1&2: assumed loop gain for next-frame prediction. 0=no correction, 1=full correction. Default 1.");
   config.add("align.maskCenterX", "", "align.maskCenterX", argType::Optional, "align", "maskCenterX", false, "float", "Coronagraph mask centre row in pixels. Default: image centre.");
   config.add("align.maskCenterY", "", "align.maskCenterY", argType::Optional, "align", "maskCenterY", false, "float", "Coronagraph mask centre col in pixels. Default: image centre.");
   config.add("align.rasterHalfSteps", "", "align.rasterHalfSteps", argType::Optional, "align", "rasterHalfSteps", false, "int", "Method 3: half-number of steps per axis; total scan = (2H+1)^2 points. Default 3.");
   config.add("align.rasterStepSize", "", "align.rasterStepSize", argType::Optional, "align", "rasterStepSize", false, "float", "Method 3: pixel step size between raster points. Default 2.");
   config.add("align.rasterSettleFrames", "", "align.rasterSettleFrames", argType::Optional, "align", "rasterSettleFrames", false, "int", "Method 3: camera frames to discard after each FSM step. Default 10.");
   config.add("align.rasterMeasureFrames", "", "align.rasterMeasureFrames", argType::Optional, "align", "rasterMeasureFrames", false, "int", "Method 3: camera frames to average per raster point. Default 5.");
   config.add("align.rasterCenterRadius", "", "align.rasterCenterRadius", argType::Optional, "align", "rasterCenterRadius", false, "float", "Method 3: radius of central aperture for intensity sum (pixels). Default 20.");
}

int alignPSFCtrl::loadConfigImpl(mx::app::appConfigurator & _config)
{
   SHMIMMONITOR_LOAD_CONFIG(_config);
   SHMIMMONITORT_LOAD_CONFIG(refShmimMonitorT, _config);

   _config(m_method, "align.method");
   _config(m_maxLag, "align.maxLag");
   _config(m_xcorrTol, "align.tol");
   _config(m_xcorrPadFactor, "align.padFactor");
   _config(m_searchHalfWidth, "align.searchHalfWidth");
   _config(m_speckleSep, "align.speckleSep");
   _config(m_speckleAngle, "align.speckleAngle");
   _config(m_speckleSearchR, "align.speckleSearchR");
   _config(m_minSpeckles, "align.minSpeckles");
   _config(m_predGain, "align.predGain");
   _config(m_maskCenterX, "align.maskCenterX");
   _config(m_maskCenterY, "align.maskCenterY");
   // Priority (1, highest): a config-file mask centre is never overridden.
   if(m_maskCenterX >= 0 && m_maskCenterY >= 0) m_maskCenterSource = maskCenterSource::config;
   _config(m_rasterHalfSteps, "align.rasterHalfSteps");
   _config(m_rasterStepSize, "align.rasterStepSize");
   _config(m_rasterSettleFrames, "align.rasterSettleFrames");
   _config(m_rasterMeasureFrames, "align.rasterMeasureFrames");
   _config(m_rasterCenterRadius, "align.rasterCenterRadius");

   float halfSep = m_speckleSep / 2.0f;

   if(m_searchHalfWidth >= halfSep)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "searchHalfWidth (" + std::to_string(m_searchHalfWidth)
         + ") must be < speckleSep/2 (" + std::to_string(halfSep)
         + ") to exclude higher-order speckle peaks"});
   }

   if(m_speckleSearchR >= halfSep)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "speckleSearchR (" + std::to_string(m_speckleSearchR)
         + ") must be < speckleSep/2 (" + std::to_string(halfSep)
         + ") to exclude higher-order speckle peaks"});
   }

   if(m_minSpeckles < 1 || m_minSpeckles > 4)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "minSpeckles must be between 1 and 4"});
   }

   if(m_predGain < 0.0f || m_predGain > 1.0f)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "predGain must be between 0 and 1"});
   }

   if(m_rasterHalfSteps < 1)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "rasterHalfSteps must be >= 1"});
   }

   if(m_rasterStepSize <= 0.0f)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "rasterStepSize must be > 0"});
   }

   if(m_rasterSettleFrames < 1)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "rasterSettleFrames must be >= 1"});
   }

   if(m_rasterMeasureFrames < 1)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "rasterMeasureFrames must be >= 1"});
   }

   if(m_rasterCenterRadius <= 0.0f)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "rasterCenterRadius must be > 0"});
   }

   return 0;
}

void alignPSFCtrl::loadConfig()
{
   if(loadConfigImpl(config) < 0)
   {
      m_shutdown = true;
   }
}

int alignPSFCtrl::appStartup()
{
   SHMIMMONITOR_APP_STARTUP;
   SHMIMMONITORT_APP_STARTUP(refShmimMonitorT);

   // Configure xcorr parameters now; refIm() will be set once the reference
   // shmim posts its first frame to processImage(refShmimT).
   m_xcorr.peakMethod(xcorrPeakMethod::mftOversamp);
   m_xcorr.normalize(true);
   m_xcorr.maxLag(m_maxLag);
   m_xcorr.tol(m_xcorrTol);

   // measurement (read by closedLoopIndi)
   registerIndiPropertyReadOnly(m_indiP_measurement, "measurement",
      pcf::IndiProperty::Number,
      pcf::IndiProperty::ReadOnly,
      pcf::IndiProperty::Idle);
   m_indiP_measurement.add({"delta_x", 0.0});
   m_indiP_measurement.add({"delta_y", 0.0});
   m_indiP_measurement.add({"counter", 0});

   // loop toggle
   CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_ctrlEnabled, "loop_state");

   // method selector (writable; external app switches between 1, 2, 3)
   CREATE_REG_INDI_NEW_NUMBERU( m_indiP_method, "method", 1, 3, 1, "%u", "method", "alignment");
   m_indiP_method["current"].set(static_cast<unsigned>(m_method));

   // raster scan status (read-only monitoring)
   registerIndiPropertyReadOnly(m_indiP_rasterStatus, "raster_status",
      pcf::IndiProperty::Number,
      pcf::IndiProperty::ReadOnly,
      pcf::IndiProperty::Idle);
   m_indiP_rasterStatus.add({"state",         0.0}); // RASTER_IDLE=0 … RASTER_DONE=4
   m_indiP_rasterStatus.add({"n_measured",     0.0}); // points measured so far
   m_indiP_rasterStatus.add({"last_intensity", 0.0}); // most recent averaged intensity
   m_indiP_rasterStatus.add({"result_x",       0.0}); // fitted minimum x (valid in DONE)
   m_indiP_rasterStatus.add({"result_y",       0.0}); // fitted minimum y (valid in DONE)

   state(stateCodes::READY);

   return 0;
}

int alignPSFCtrl::appLogic()
{
   SHMIMMONITOR_APP_LOGIC;
   SHMIMMONITORT_APP_LOGIC(refShmimMonitorT);

   SHMIMMONITOR_UPDATE_INDI;
   SHMIMMONITORT_UPDATE_INDI(refShmimMonitorT);

   if(m_loopClosed)
   {
      state(stateCodes::OPERATING);
   }
   else
   {
      state(stateCodes::READY);
   }

   return 0;
}

int alignPSFCtrl::appShutdown()
{
   SHMIMMONITOR_APP_SHUTDOWN;
   SHMIMMONITORT_APP_SHUTDOWN(refShmimMonitorT);

   return 0;
}

// --- camera shmim ---

int alignPSFCtrl::allocate(const dev::shmimT &)
{
   m_camImage.resize(shmimMonitorT::m_width, shmimMonitorT::m_height);

   m_camPixget = getPixPointer<float>(shmimMonitorT::m_dataType);

   if(m_camPixget == nullptr)
   {
      return log<software_error, -1>({__FILE__, __LINE__, "unsupported camera shmim data type"});
   }

   // Priority (3, lowest): image centre only if neither the config file (1)
   // nor the reference image (2) has provided a mask centre.
   if(m_maskCenterSource == maskCenterSource::imageCentre)
   {
      m_maskCenterX = shmimMonitorT::m_width  / 2.0f;
      m_maskCenterY = shmimMonitorT::m_height / 2.0f;
   }

   return 0;
}

int alignPSFCtrl::processImage(void *curr_src, const dev::shmimT &)
{
   if(!m_loopClosed || m_camPixget == nullptr)
   {
      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
         std::ostringstream oss;
         oss << "Skipping frame: loopClosed=" << m_loopClosed
             << " camPixget=" << (m_camPixget != nullptr);
         log<software_debug>({__FILE__, __LINE__, oss.str()});
      }
      return 0;
   }

   // Method 1 requires a reference image for cross-correlation.
   if(m_method == 1 && !m_refImLoaded)
   {
      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
         log<software_debug>({__FILE__, __LINE__,
            "Skipping frame: method=1 but no reference image loaded"});
      }
      return 0;
   }

   // Copy the incoming frame to the float buffer.
   const uint64_t npix = shmimMonitorT::m_width * shmimMonitorT::m_height;
   for(uint64_t nn = 0; nn < npix; ++nn)
      m_camImage.data()[nn] = m_camPixget(curr_src, nn);

   // -------------------------------------------------------
   // First-frame bootstrap (Methods 1 & 2 only).
   // Method 3 manages its own state machine from RASTER_IDLE.
   // -------------------------------------------------------
   if(m_method != 3 && !m_initialized)
   {
      float camSpeckleX[4], camSpeckleY[4];
      if(findFirstOrderSpeckles(m_camImage, camSpeckleX, camSpeckleY) == 0)
      {
         if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
         {
            std::ostringstream oss;
            oss << "First frame speckles: ";
            for(int n = 0; n < 4; ++n)
               oss << "(" << camSpeckleX[n] << ", " << camSpeckleY[n] << ") ";
            log<software_debug>({__FILE__, __LINE__, oss.str()});
         }

         float sumX = 0.0f, sumY = 0.0f;
         for(int n = 0; n < 4; ++n) { sumX += camSpeckleX[n]; sumY += camSpeckleY[n]; }

         m_psfEstX = sumX / 4.0f - m_maskCenterX;
         m_psfEstY = sumY / 4.0f - m_maskCenterY;

         log<software_info>({__FILE__, __LINE__, "Bootstrap PSF shift: (" +
                       std::to_string(m_psfEstX) + ", " + std::to_string(m_psfEstY) + ")"});
      }
      else
      {
         log<software_warning>({__FILE__, __LINE__,
            "findFirstOrderSpeckles failed; starting from shift (0,0)"});
      }
      m_initialized = true;
   }

   float xs = 0.0f, ys = 0.0f;

   // --------------------------
   // Method 2: speckle centroid
   // --------------------------
   if(m_method == 2)
   {
      float psfCX = m_maskCenterX + m_psfEstX;
      float psfCY = m_maskCenterY + m_psfEstY;
      float angRad = m_speckleAngle * static_cast<float>(M_PI) / 180.0f;

      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
         std::ostringstream oss;
         oss << "Estimated PSF location: " << psfCX << ", " << psfCY;
         log<software_debug>({__FILE__, __LINE__, oss.str()});
      }

      float sumDx = 0.0f, sumDy = 0.0f;
      int nFound = 0;

      for(int n = 0; n < 4; ++n)
      {
         float a = angRad + n * (static_cast<float>(M_PI) / 2.0f);
         float eX = psfCX + m_speckleSep * std::cos(a);
         float eY = psfCY + m_speckleSep * std::sin(a);

         if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
         {
            std::ostringstream oss;
            oss << "Estimated speckle " << n << " location: " << eX << ", " << eY;
            log<software_debug>({__FILE__, __LINE__, oss.str()});
         }

         float fX, fY;
         if(findLocalPeak(m_camImage, eX, eY, m_speckleSearchR, fX, fY))
         {
            sumDx += fX - eX;
            sumDy += fY - eY;
            ++nFound;

            if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
            {
               std::ostringstream oss;
               oss << "Found speckle " << n << " location: " << fX << ", " << fY;
               log<software_debug>({__FILE__, __LINE__, oss.str()});
            }
         }
      }

      if(nFound < m_minSpeckles)
      {
         log<software_error>({__FILE__, __LINE__,
            "only " + std::to_string(nFound) + " of " + std::to_string(m_minSpeckles)
            + " required speckles found"});
         return 0;
      }

      xs = m_psfEstX + sumDx / nFound;
      ys = m_psfEstY + sumDy / nFound;

      m_psfEstX = xs * (1.0f - m_predGain);
      m_psfEstY = ys * (1.0f - m_predGain);
   }
   // ---------------------------------------------------------
   // Method 3: intensity-minimising raster scan
   // ---------------------------------------------------------
   else if(m_method == 3)
   {
      if(m_rasterState == RASTER_IDLE)
      {
         m_rasterState      = RASTER_SETTLING;
         m_rasterIx         = -m_rasterHalfSteps;
         m_rasterIy         = -m_rasterHalfSteps;
         m_rasterHoldX      = m_rasterIx * m_rasterStepSize;
         m_rasterHoldY      = m_rasterIy * m_rasterStepSize;
         m_rasterFrameCount = 0;
         m_rasterAccum      = 0.0f;
         m_rasterData.clear();
         m_rasterData.reserve((2*m_rasterHalfSteps+1) * (2*m_rasterHalfSteps+1));
         log<text_log>("Method 3: starting raster scan (" +
                       std::to_string(2*m_rasterHalfSteps+1) + "x" +
                       std::to_string(2*m_rasterHalfSteps+1) + " points, " +
                       std::to_string(m_rasterStepSize) + " pix/step)");
      }

      // Publish negative of the commanded offset.
      // closedLoopIndi: cmd = current − gain*(measurement − ref)
      //               = current − gain*(−offset − 0)
      //               = current + gain*offset
      // → FSM drives PSF to +offset pixels from mask centre.
      xs = -m_rasterHoldX;
      ys = -m_rasterHoldY;

      if(m_rasterState == RASTER_SETTLING)
      {
         if(++m_rasterFrameCount >= m_rasterSettleFrames)
         {
            m_rasterFrameCount = 0;
            m_rasterAccum      = 0.0f;
            m_rasterState      = RASTER_MEASURING;
         }
      }
      else if(m_rasterState == RASTER_MEASURING)
      {
         m_rasterAccum += measureCentralIntensity(m_camImage,
                                                  m_maskCenterX, m_maskCenterY,
                                                  m_rasterCenterRadius);
         if(++m_rasterFrameCount >= m_rasterMeasureFrames)
         {
            m_rasterData.push_back({m_rasterHoldX, m_rasterHoldY,
                                    m_rasterAccum / m_rasterMeasureFrames});

            if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
            {
               std::ostringstream oss;
               oss << "Raster point (" << m_rasterIx << "," << m_rasterIy << ")"
                   << " offset=(" << m_rasterHoldX << "," << m_rasterHoldY << ")"
                   << " intensity=" << m_rasterData.back().intensity;
               log<software_debug>({__FILE__, __LINE__, oss.str()});
            }

            // Advance to next point (x outer loop, y inner loop).
            if(++m_rasterIy > m_rasterHalfSteps)
            {
               m_rasterIy = -m_rasterHalfSteps;
               ++m_rasterIx;
            }

            if(m_rasterIx > m_rasterHalfSteps)
            {
               m_rasterState = RASTER_FITTING;
            }
            else
            {
               m_rasterHoldX      = m_rasterIx * m_rasterStepSize;
               m_rasterHoldY      = m_rasterIy * m_rasterStepSize;
               m_rasterFrameCount = 0;
               m_rasterState      = RASTER_SETTLING;
            }
         }
      }

      if(m_rasterState == RASTER_FITTING)
      {
         if(fitRaster() == 0)
         {
            m_rasterState = RASTER_DONE;
            xs = -m_rasterMinX;
            ys = -m_rasterMinY;
            log<text_log>("Raster complete. Minimum at (" +
                          std::to_string(m_rasterMinX) + ", " +
                          std::to_string(m_rasterMinY) + ") pixels from mask centre.");
         }
         else
         {
            log<software_warning>({__FILE__,__LINE__,"fitRaster failed; restarting scan"});
            m_rasterState = RASTER_IDLE;
         }
      }
      else if(m_rasterState == RASTER_DONE)
      {
         xs = -m_rasterMinX;
         ys = -m_rasterMinY;
      }

      updateIfChanged(m_indiP_rasterStatus,
         std::vector<std::string>({"state", "n_measured", "last_intensity",
                                    "result_x", "result_y"}),
         std::vector<float>({static_cast<float>(m_rasterState),
                             static_cast<float>(m_rasterData.size()),
                             m_rasterData.empty() ? 0.0f : m_rasterData.back().intensity,
                             m_rasterMinX,
                             m_rasterMinY}));
   }
   // -------------------------------------------------------
   // Method 1: restricted cross-correlation (default)
   // -------------------------------------------------------
   else
   {
      m_xcorr.searchBox(m_psfEstX, m_psfEstY, m_searchHalfWidth);

      try
      {
         m_xcorr(xs, ys, m_camImage);
      }
      catch(const std::exception & e)
      {
         log<software_error>({__FILE__, __LINE__,
            std::string("xcorr exception: ") + e.what()});
         return 0;
      }

      m_psfEstX = xs * (1.0f - m_predGain);
      m_psfEstY = ys * (1.0f - m_predGain);
   }

   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      log<software_debug>({__FILE__, __LINE__,
         "shift=(" + std::to_string(xs) + "," + std::to_string(ys) + ")"
         " method=" + std::to_string(m_method)});
   }

   ++m_counter;
   updateIfChanged(m_indiP_measurement,
      std::vector<std::string>({"delta_x", "delta_y", "counter"}),
      std::vector<float>({xs, ys, static_cast<float>(m_counter)}));

   return 0;
}

// --- reference shmim ---

int alignPSFCtrl::allocate(const refShmimT &)
{
   m_refImage.resize(refShmimMonitorT::m_width, refShmimMonitorT::m_height);

   m_refPixget = getPixPointer<float>(refShmimMonitorT::m_dataType);

   if(m_refPixget == nullptr)
   {
      return log<software_error, -1>({__FILE__, __LINE__, "unsupported reference shmim data type"});
   }

   m_refImLoaded = false;

   return 0;
}

int alignPSFCtrl::processImage(void *curr_src, const refShmimT &)
{
   if(m_refPixget == nullptr)
   {
      return 0;
   }

   const uint64_t npix = refShmimMonitorT::m_width * refShmimMonitorT::m_height;
   for(uint64_t nn = 0; nn < npix; ++nn)
      m_refImage.data()[nn] = m_refPixget(curr_src, nn);

   // refIm() auto-correlates the reference to calibrate refX0/refY0 (the
   // coordinate origin). That internal xcorr needs a full-image search
   // because refX0==0 at that point maps the search box to the wrong corner.
   m_xcorr.disableSearchBox();
   try
   {
      m_xcorr.refIm(m_refImage, m_xcorrPadFactor);
   }
   catch(const std::exception & e)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         std::string("exception setting xcorr reference: ") + e.what()});
   }

   // New reference: reset tracking state so the next camera frame
   // re-runs findFirstOrderSpeckles to locate the PSF.
   m_initialized = false;
   m_psfEstX = 0.0f;
   m_psfEstY = 0.0f;

   m_refImLoaded = true;
   log<text_log>("Reference image updated (method=" + std::to_string(m_method) + ")");

   // Priority (2): unless the config file set it, (re)extrapolate the mask centre
   // from the reference image -- the PSF sits on the mask there, so its centre
   // (the mean of the first-order speckles) is the mask centre.
   if(m_maskCenterSource != maskCenterSource::config)
   {
      float rsx[4], rsy[4];
      if(findFirstOrderSpeckles(m_refImage, rsx, rsy) == 0)
      {
         float sx = 0.0f, sy = 0.0f;
         for(int n = 0; n < 4; ++n) { sx += rsx[n]; sy += rsy[n]; }
         m_maskCenterX = sx / 4.0f;
         m_maskCenterY = sy / 4.0f;
         m_maskCenterSource = maskCenterSource::reference;
         log<text_log>("Mask centre extrapolated from reference image: (" +
                       std::to_string(m_maskCenterX) + ", " + std::to_string(m_maskCenterY) + ")");
      }
      else
      {
         log<software_warning>({__FILE__, __LINE__,
            "could not extrapolate mask centre from reference image"});
      }
   }

   return 0;
}


// ============================================================
// PSF detection helpers
// ============================================================

bool alignPSFCtrl::findLocalPeak( const eigenImage<float> & im,
                                   float eX, float eY, int searchR,
                                   float & foundX, float & foundY )
{
   int r0 = std::max(0, (int)std::floor(eX) - searchR);
   int r1 = std::min((int)im.rows()-1, (int)std::ceil(eX)  + searchR);
   int c0 = std::max(0, (int)std::floor(eY) - searchR);
   int c1 = std::min((int)im.cols()-1, (int)std::ceil(eY)  + searchR);

   if(r0 > r1 || c0 > c1) return false;

   float maxVal = im(r0, c0);
   int maxR = r0, maxC = c0;
   for(int r = r0; r <= r1; ++r)
      for(int c = c0; c <= c1; ++c)
         if(im(r, c) > maxVal) { maxVal = im(r, c); maxR = r; maxC = c; }

   float sumW = 0.0f, sumR = 0.0f, sumC = 0.0f;
   for(int r = std::max(0, maxR-1); r <= std::min((int)im.rows()-1, maxR+1); ++r)
   {
      for(int c = std::max(0, maxC-1); c <= std::min((int)im.cols()-1, maxC+1); ++c)
      {
         float w = std::max(0.0f, im(r, c));
         sumW += w;
         sumR += w * r;
         sumC += w * c;
      }
   }

   // TODO: Is it okay to default to edge of range if sumW==0?
   foundX = (sumW > 0.0f) ? sumR / sumW : (float)maxR;
   foundY = (sumW > 0.0f) ? sumC / sumW : (float)maxC;

   return true;
}

int alignPSFCtrl::findFirstOrderSpeckles( const eigenImage<float> & im,
                                           float foundX[4], float foundY[4] )
{
   const int nrows = im.rows();
   const int ncols = im.cols();
   const int npix = nrows * ncols;

   if(npix == 0) return -1;

   // --- Step 1: threshold at the 99th percentile ---
   std::vector<float> vals(npix);
   for(int i = 0; i < npix; ++i) vals[i] = im.data()[i];

   size_t threshIdx = static_cast<size_t>(npix * 0.99);
   if(threshIdx >= static_cast<size_t>(npix)) threshIdx = npix - 1;
   std::nth_element(vals.begin(), vals.begin() + threshIdx, vals.end());
   float threshold = vals[threshIdx];

   // --- Step 2: connected-component labelling via 4-connected flood fill ---
   struct Cluster { float sumWR, sumWC, sumW; int count; };

   std::vector<int> labels(npix, 0);
   std::vector<Cluster> clusters;
   int nextLabel = 1;

   for(int col = 0; col < ncols; ++col)
   {
      for(int row = 0; row < nrows; ++row)
      {
         int li = row + col * nrows; // column-major (Eigen default)
         if(labels[li] != 0 || im(row, col) < threshold) continue;

         Cluster cl = {0, 0, 0, 0};
         std::vector<std::pair<int,int>> stack;
         stack.push_back(std::make_pair(row, col));
         labels[li] = nextLabel;

         while(!stack.empty())
         {
            int cr = stack.back().first;
            int cc = stack.back().second;
            stack.pop_back();

            float w = std::max(0.0f, im(cr, cc));
            cl.sumWR += w * cr;
            cl.sumWC += w * cc;
            cl.sumW  += w;
            cl.count++;

            static const int dr[] = {-1, 1, 0, 0};
            static const int dc[] = {0, 0, -1, 1};
            for(int d = 0; d < 4; ++d)
            {
               int nr = cr + dr[d], nc = cc + dc[d];
               if(nr < 0 || nr >= nrows || nc < 0 || nc >= ncols) continue;
               int ni = nr + nc * nrows;
               if(labels[ni] != 0 || im(nr, nc) < threshold) continue;
               labels[ni] = nextLabel;
               stack.push_back(std::make_pair(nr, nc));
            }
         }

         if(cl.count >= 4 && cl.sumW > 0) // skip hot-pixel clusters
            clusters.push_back(cl);

         ++nextLabel;
      }
   }

   if(clusters.size() < 4)
   {
      log<software_debug>({__FILE__, __LINE__,
         "findFirstOrderSpeckles: only " + std::to_string(clusters.size()) +
         " clusters found (need 4)"});
      return -1;
   }

   // --- Step 3: compute centroids ---
   struct CInfo { float x, y; };
   std::vector<CInfo> cen(clusters.size());
   for(size_t i = 0; i < clusters.size(); ++i)
   {
      cen[i].x = clusters[i].sumWR / clusters[i].sumW;
      cen[i].y = clusters[i].sumWC / clusters[i].sumW;
   }

   // --- Step 4: geometric consistency ---
   const float expectedDiag = 2.0f * m_speckleSep;
   const float distTol = m_speckleSep * 0.3f;
   const size_t N = cen.size();

   float bestScore = std::numeric_limits<float>::max();
   int bestQuad[4] = {-1, -1, -1, -1};

   for(size_t i = 0; i < N; ++i)
   {
      for(size_t j = i + 1; j < N; ++j)
      {
         float dij = std::sqrt(std::pow(cen[i].x - cen[j].x, 2) +
                               std::pow(cen[i].y - cen[j].y, 2));
         if(std::abs(dij - expectedDiag) > distTol) continue;

         float midX = (cen[i].x + cen[j].x) / 2.0f;
         float midY = (cen[i].y + cen[j].y) / 2.0f;

         for(size_t k = 0; k < N; ++k)
         {
            if(k == i || k == j) continue;
            float dk = std::sqrt(std::pow(cen[k].x - midX, 2) +
                                 std::pow(cen[k].y - midY, 2));
            if(std::abs(dk - m_speckleSep) > distTol) continue;

            for(size_t l = k + 1; l < N; ++l)
            {
               if(l == i || l == j) continue;
               float dl = std::sqrt(std::pow(cen[l].x - midX, 2) +
                                    std::pow(cen[l].y - midY, 2));
               if(std::abs(dl - m_speckleSep) > distTol) continue;

               float dkl = std::sqrt(std::pow(cen[k].x - cen[l].x, 2) +
                                     std::pow(cen[k].y - cen[l].y, 2));
               if(std::abs(dkl - expectedDiag) > distTol) continue;

               float di = std::sqrt(std::pow(cen[i].x - midX, 2) +
                                    std::pow(cen[i].y - midY, 2));
               float dj = std::sqrt(std::pow(cen[j].x - midX, 2) +
                                    std::pow(cen[j].y - midY, 2));
               float score = std::pow(di - m_speckleSep, 2)
                           + std::pow(dj - m_speckleSep, 2)
                           + std::pow(dk - m_speckleSep, 2)
                           + std::pow(dl - m_speckleSep, 2);

               if(score < bestScore)
               {
                  bestScore = score;
                  bestQuad[0] = static_cast<int>(i);
                  bestQuad[1] = static_cast<int>(j);
                  bestQuad[2] = static_cast<int>(k);
                  bestQuad[3] = static_cast<int>(l);
               }
            }
         }
      }
   }

   if(bestQuad[0] < 0)
   {
      log<software_debug>({__FILE__, __LINE__,
         "findFirstOrderSpeckles: no geometrically consistent quad among " +
         std::to_string(N) + " clusters"});
      return -1;
   }

   for(int n = 0; n < 4; ++n)
   {
      foundX[n] = cen[bestQuad[n]].x;
      foundY[n] = cen[bestQuad[n]].y;
   }

   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      std::ostringstream oss;
      oss << "findFirstOrderSpeckles: found 4 speckles at"
          << " (" << foundX[0] << "," << foundY[0] << ")"
          << " (" << foundX[1] << "," << foundY[1] << ")"
          << " (" << foundX[2] << "," << foundY[2] << ")"
          << " (" << foundX[3] << "," << foundY[3] << ")";
      log<software_debug>({__FILE__, __LINE__, oss.str()});
   }

   return 0;
}

float alignPSFCtrl::measureCentralIntensity( const eigenImage<float> & im,
                                              float cx, float cy, float radius )
{
   const int r0 = std::max(0, static_cast<int>(std::floor(cx - radius)));
   const int r1 = std::min(static_cast<int>(im.rows())-1,
                            static_cast<int>(std::ceil(cx + radius)));
   const int c0 = std::max(0, static_cast<int>(std::floor(cy - radius)));
   const int c1 = std::min(static_cast<int>(im.cols())-1,
                            static_cast<int>(std::ceil(cy + radius)));
   const float r2 = radius * radius;

   float total = 0.0f;
   for(int r = r0; r <= r1; ++r)
      for(int c = c0; c <= c1; ++c)
      {
         float dr = r - cx, dc = c - cy;
         if(dr*dr + dc*dc <= r2)
            total += im(r, c);
      }
   return total;
}

int alignPSFCtrl::fitRaster()
{
   const int n = static_cast<int>(m_rasterData.size());
   if(n < 6)
   {
      return log<software_error,-1>({__FILE__,__LINE__,
         "fitRaster: need at least 6 data points to fit 6-parameter model, have " +
         std::to_string(n)});
   }

   // Fit: I(x,y) = a*x^2 + b*y^2 + c*x*y + d*x + e*y + f  (least squares)
   Eigen::MatrixXf A(n, 6);
   Eigen::VectorXf bvec(n);
   for(int i = 0; i < n; ++i)
   {
      float x = m_rasterData[i].x;
      float y = m_rasterData[i].y;
      A(i,0) = x*x;  A(i,1) = y*y;  A(i,2) = x*y;
      A(i,3) = x;    A(i,4) = y;    A(i,5) = 1.0f;
      bvec(i) = m_rasterData[i].intensity;
   }

   auto sol = A.colPivHouseholderQr().solve(bvec);
   const float a = sol(0), b = sol(1), c = sol(2);
   const float d = sol(3), e = sol(4);

   // Minimum of the quadratic satisfies:
   //   [2a  c ] [x_min]   [-d]
   //   [c   2b] [y_min] = [-e]
   //
   // Positive-definite Hessian requires: 2a > 0 AND det = 4ab - c^2 > 0.
   const float det = 4.0f*a*b - c*c;
   const float maxOffset = m_rasterHalfSteps * m_rasterStepSize;

   if(a <= 0.0f || det <= 0.0f)
   {
      // Surface has no interior minimum in this region (saddle or maximum).
      // Fall back to the raw data point with the lowest measured intensity.
      log<software_warning>({__FILE__,__LINE__,
         "fitRaster: Hessian not positive definite (a=" + std::to_string(a) +
         ", det=" + std::to_string(det) + "); using raw intensity minimum"});

      float minIntensity = std::numeric_limits<float>::max();
      m_rasterMinX = 0.0f;
      m_rasterMinY = 0.0f;
      for(auto & pt : m_rasterData)
      {
         if(pt.intensity < minIntensity)
         {
            minIntensity = pt.intensity;
            m_rasterMinX = pt.x;
            m_rasterMinY = pt.y;
         }
      }
      return 0;
   }

   // Cramer's rule on the 2x2 system.
   float xMin = (-d*2.0f*b + e*c) / det;
   float yMin = (-e*2.0f*a + d*c) / det;

   // Clamp to the scanned range so we don't extrapolate beyond the data.
   m_rasterMinX = std::max(-maxOffset, std::min(maxOffset, xMin));
   m_rasterMinY = std::max(-maxOffset, std::min(maxOffset, yMin));

   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      std::ostringstream oss;
      oss << "fitRaster: analytic minimum (" << xMin << ", " << yMin << ")"
          << " clamped to (" << m_rasterMinX << ", " << m_rasterMinY << ")";
      log<software_debug>({__FILE__, __LINE__, oss.str()});
   }

   return 0;
}

int alignPSFCtrl::toggleLoop(bool onoff)
{
   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      std::ostringstream oss;
      oss << "Received request to toggle loop to " << (onoff ? "ON" : "OFF") << ".";
      oss <<"Current loop state is " << (m_loopClosed ? "ON" : "OFF");
      log<software_debug>({__FILE__, __LINE__, oss.str()});
   }

   if(!m_loopClosed && onoff)
   {
      m_loopClosed = true;
      log<loop_closed>();
      updateSwitchIfChanged(m_indiP_ctrlEnabled, "toggle", pcf::IndiElement::On, INDI_OK);
      return 0;
   }

   if(m_loopClosed && !onoff)
   {
      m_loopClosed = false;
      log<loop_open>();
      updateSwitchIfChanged(m_indiP_ctrlEnabled, "toggle", pcf::IndiElement::Off, INDI_IDLE);
      return 0;
   }

   return 0;
}


INDI_NEWCALLBACK_DEFN(alignPSFCtrl, m_indiP_ctrlEnabled)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_ctrlEnabled);

    if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
    {
       return toggleLoop(true);
    }

    if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::Off)
    {
       return toggleLoop(false);
    }

    return 0;
}

INDI_NEWCALLBACK_DEFN(alignPSFCtrl, m_indiP_method)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_method);

   unsigned newMethod = ipRecv["current"].get<unsigned>();
   if(newMethod < 1 || newMethod > 3) return 0;

   if(static_cast<int>(newMethod) != m_method)
   {
      m_method = static_cast<int>(newMethod);

      if(m_method == 3)
      {
         // Fresh raster scan whenever method 3 is selected.
         m_rasterState = RASTER_IDLE;
      }
      else
      {
         // Re-bootstrap PSF tracking when returning to Methods 1 or 2.
         m_rasterState = RASTER_IDLE;
         m_initialized = false;
         m_psfEstX     = 0.0f;
         m_psfEstY     = 0.0f;
      }

      log<text_log>("Method changed to " + std::to_string(m_method));
   }

   updateIfChanged(m_indiP_method, "current", static_cast<unsigned>(m_method));
   return 0;
}

} //namespace app
} //namespace MagAOX

#endif //alignPSFCtrl_hpp
