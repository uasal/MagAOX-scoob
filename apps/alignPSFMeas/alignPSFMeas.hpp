/** \file alignPSFMeas.hpp
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
  *    from the speckle detector (same detector as Method 2); subsequent
  *    frames track iteratively within the restricted box.
  *    Key parameters: align.maxLag, align.searchHalfWidth, align.speckleSep,
  *    align.speckleAngle, align.predGain.
  *
  *  Method 2 (method=2): Speckle centroid.
  *    Runs the full speckle detector on every frame -- stateless, with no
  *    tracking priors.  The detector finds the four first-order DM speckles
  *    (validated by square geometry and raw-flux similarity) and the mean of
  *    their positions, relative to the mask centre, is the measurement.
  *    When no quad is visible (speckles off-frame at large offsets, or one
  *    occulted while transiting the mask) but the unsuppressed PSF core
  *    dominates the scene, the core position is used instead -- so the core
  *    carries the measurement coarsely until the quad takes over.
  *    Key parameters: align.speckleSep, align.speckleAngle,
  *    align.quadFluxRatio, align.coreDomFactor.t.
  *
  * Every processed frame also updates the read-only INDI property
  * "measurement_status": ok (1 = this frame published a measurement),
  * source of the most recent measurement (0 = none yet, 1 = xcorr,
  * 2 = speckle quad, 3 = dominant core), and fail_count (consecutive frames
  * without a valid measurement).  After align.failLimit consecutive failures
  * the loop is opened and the app holds ERROR until the loop is closed again.
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
  * --- Setting the reference image ---
  *
  * With the PSF centered on the mask and the sine pattern active:
  *
  *  import ImageStreamIOWrap as shmio
  *  REF_SHM = "alignPSFMeas_ref"
  *  camlo, camsci = grab_frames()
  *  ref = shmio.Image()
  *  ref.create(REF_SHM, camsci.astype(np.float32))
  *
  * To update the reference live, call ref.write(new_frame) on the same object.
  *
  * \ingroup alignPSFMeas_files
  */

#ifndef alignPSFMeas_hpp
#define alignPSFMeas_hpp

#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <utility>
#include <atomic>

#include <mx/improc/imageXCorrFFT.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

using namespace mx::improc;

/** \defgroup alignPSFMeas
  * \brief PSF-to-coronagraph-mask alignment via FFT cross-correlation or speckle centroid
  *
  * <a href="../handbook/operating/software/apps/alignPSFMeas.html">Application Documentation</a>
  *
  * \ingroup apps
  */

/** \defgroup alignPSFMeas_files
  * \ingroup alignPSFMeas
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
  * \ingroup alignPSFMeas
  */
class alignPSFMeas : public MagAOXApp<true>,
                     public dev::shmimMonitor<alignPSFMeas>,
                     public dev::shmimMonitor<alignPSFMeas, refShmimT>
{
   //Give the test harness access.
   friend class alignPSFMeas_test;

   friend class dev::shmimMonitor<alignPSFMeas>;
   friend class dev::shmimMonitor<alignPSFMeas, refShmimT>;

   typedef dev::shmimMonitor<alignPSFMeas> shmimMonitorT;
   typedef dev::shmimMonitor<alignPSFMeas, refShmimT> refShmimMonitorT;

protected:

   /** \name Configurable Parameters
     *@{
     */

   /// Displacement measurement method: 1=xcorr, 2=speckle centroid. Default 1.
   /** Atomic with release/acquire: the method callback writes the associated
     * tracking resets BEFORE release-storing this, and the camera thread
     * acquire-loads it each frame to branch, so the resets are published with
     * it and the reset fields themselves stay plain.
     */
   std::atomic<int> m_method{1};

   int m_maxLag{10};               ///< Method 1: Maximum lag (pixels) passed to imageXCorrFFT. Default 10.
   float m_xcorrTol{0.001f};       ///< Method 1: Interpolation tolerance for imageXCorrFFT. Default 0.001.
   float m_xcorrPadFactor{1.0f};   ///< Method 1: Padding factor for the reference FFT. Default 1.
   int m_searchHalfWidth{10};       ///< Method 1: xcorr search box half-width (pixels). Default 10.

   float m_speckleSep{50.0f};      ///< Methods 1&2: 1st order speckle distance from PSF centre (pixels). Default 50.

   float m_speckleAngle{45.0f};    ///< Methods 1&2: angle of 1st order speckle from image row axis (degrees). Default 45.

   /// Interval (seconds) at which mask_center is re-asserted for subscribers.  0 disables.
   /** mask_center is consumed by other apps (alignRasterMeas) but changes only
     * rarely, so updateIfChanged emits it almost never.  A subscriber that
     * starts afterwards is then relying on INDI replaying the cached value to
     * it, which is not reaching driver subscriptions here -- observed: the
     * subscriber only receives mask_center if this app is restarted, forcing a
     * fresh broadcast, while frequently-republished values (fsm_sim.val_*)
     * arrive fine.  So we re-assert it at a low rate, which lets any consumer
     * converge within one interval regardless of start order.  Set to 0 once
     * the underlying INDI behaviour makes this unnecessary.
     */
   float m_maskCenterRepublish{10.0f};

   /// Method 1: assumed loop gain for predicting the next-frame PSF position.
   /** Must be kept in step with the servo loop's own gain (loop.gain in the
     * closedLoopIndi config).  Set once at config load, before any thread
     * starts, so it needs no synchronisation.
     */
   float m_predGain{0.1f};

   float m_centroidRadius{6.0f};   ///< Radius (pixels) of the background-subtracted centroid window. Default 6.
   int m_maxPeakCands{24};         ///< Detector: max local-maxima candidates kept for the quad search. Default 24.
   float m_quadFluxRatio{3.0f};    ///< Detector: max raw-flux ratio between brightest and faintest speckle in a quad. Default 3. From tests, the first order speckles are about 8-10 times brighter than the second order, so this should not be set higher than 4-5.
   float m_coreDomFactor{25.0f};   ///< Detector fallback: raw-flux factor by which one candidate must dominate all others to be identified as the unsuppressed core. Default 25. From tests, the unsuppressed core tends to be ~60 times brighter than the first order speckles.
   int m_failLimit{50};            ///< Consecutive frames without a valid measurement before the loop is opened and the app holds ERROR. Default 50.

   /// Where the current mask centre came from, in ascending priority (config wins).
   enum class maskCenterSource { imageCentre, reference, config };
   maskCenterSource m_maskCenterSource{maskCenterSource::imageCentre};

   float m_maskCenterX{-1.0f};     ///< Coronagraph mask centre row (pixels).
   float m_maskCenterY{-1.0f};     ///< Coronagraph mask centre col (pixels).

   double m_lastMaskCenterPub{0};  ///< Time of the last mask_center re-assertion.  Camera-thread only, like the mask centre itself.

   ///@}

   mx::improc::imageXCorrFFT<eigenImage<float>> m_xcorr; ///< The cross-correlator.

   eigenImage<float> m_camImage; ///< Float buffer for the current camera frame.
   eigenImage<float> m_refImage; ///< Float buffer for the reference frame.

   float (*m_camPixget)(void *, size_t) {nullptr}; ///< Pixel accessor for the camera shmim.
   float (*m_refPixget)(void *, size_t) {nullptr}; ///< Pixel accessor for the reference shmim.

   bool m_refImLoaded{false};    ///< True once a reference frame has been received and set.  Camera-thread owned (see applyNewReference).
   int64_t m_counter{0};         ///< Count of measurements published this session (reset on loop close).
                                 /// Published wrapped to 1e6: at counter = 1,000,000 the wire text becomes "1e+06";
                                 /// closedLoopIndi's get<int>() parses that as 1, and consecutive counters serialize
                                 /// identically so updateIfChanged would stop sending at all.

   /// Whether the loop is closed.
   /** Atomic with release/acquire: toggleLoop writes the whole session-reset
     * batch (counter, fail counters, Method 1 bootstrap state) BEFORE
     * release-storing this flag, and the camera thread acquire-loads it at
     * its gate, so observing "closed" guarantees the resets are visible.
     * The batch itself therefore stays plain -- the flag publishes it.
     */
   std::atomic<bool> m_loopClosed{false};

   /// Set by the reference thread to hand a new reference image to the camera thread.
   /** The reference monitor only copies pixels into m_refImage and raises
     * this flag; the camera thread consumes it via applyNewReference().  That
     * keeps m_xcorr (and the mask-centre/tracking state) owned by exactly one
     * thread: rebuilding the correlator on the reference thread while the
     * camera thread was correlating was a genuine use-after-free race.
     * Release/acquire publishes the m_refImage pixels with the flag.
     */
   std::atomic<bool> m_newRefPending{false};

   uint64_t m_lastProcCnt0{std::numeric_limits<uint64_t>::max()}; ///< cnt0 of the last camera frame processed; used to skip backlogged semaphore wakeups that re-deliver an already-processed frame.
   uint64_t m_backlogSkips{0};   ///< Count of backlogged wakeups skipped (diagnostic).

   float m_estShiftX{0.0f};           ///< Predicted PSF shift in X for the next frame (pixels).
   float m_estShiftY{0.0f};           ///< Predicted PSF shift in Y for the next frame (pixels).

   // --- Method 1 tracking state ---
   bool m_initialized{false};       ///< Method 1: true after the one-shot bootstrap has located the PSF.

   // --- Measurement failure policy (all methods) ---
   int m_measureFailCount{0};       ///< Consecutive processed frames without a valid measurement.

   /// Latched when m_failLimit is reached; opens the loop and holds ERROR until the loop is re-closed.
   /** Atomic because it is genuinely three-threaded and not published by any
     * other flag: the camera thread sets it, appLogic (main thread) polls it
     * for the ERROR state, and toggleLoop (INDI thread) clears it.
     */
   std::atomic<bool> m_measureFailed{false};
   int m_lastSource{0};             ///< Source of the most recent published measurement (see measurement_status).

   // --- Debug image products ---
   int m_debugImgEvery{0};    ///< Publish debug shmims on the 1st and every Nth detection attempt. 0 = off.
   uint64_t m_dbgAttempts{0}; ///< Detection attempts since enable (decimation counter for debug products).

   IMAGE m_dbgSmoothStream;         ///< Debug shmim: smoothed detection image (Step 1 output).
   bool m_dbgSmoothCreated{false};  ///< True once the smooth debug shmim has been created.
   IMAGE m_dbgCandsStream;          ///< Debug shmim: smoothed image with NMS candidates marked.
   bool m_dbgCandsCreated{false};   ///< True once the candidates debug shmim has been created.
   IMAGE m_dbgQuadStream;           ///< Debug shmim: original image with found speckles + PSF centre marked.
   bool m_dbgQuadCreated{false};    ///< True once the quad debug shmim has been created.

   // --- INDI ---
   pcf::IndiProperty m_indiP_measurement;  ///< Read-only: delta_x, delta_y, counter.
   pcf::IndiProperty m_indiP_ctrlEnabled;  ///< Toggle: close/open the measurement loop.
   INDI_NEWCALLBACK_DECL(alignPSFMeas, m_indiP_ctrlEnabled);

   pcf::IndiProperty m_indiP_method;       ///< Writable: active method (1/2/3).
   INDI_NEWCALLBACK_DECL(alignPSFMeas, m_indiP_method);

   pcf::IndiProperty m_indiP_debugEvery;   ///< Writable: publish debug image products every N detection attempts (0 = off).
   INDI_NEWCALLBACK_DECL(alignPSFMeas, m_indiP_debugEvery);

   pcf::IndiProperty m_indiP_measStatus;   ///< Read-only: ok (this frame published), source of the latest measurement, fail_count.


   pcf::IndiProperty m_indiP_maskCenter;   ///< Read-only: mask centre x, y (pixels) and its source (0=image centre, 1=reference image, 2=config).

protected:
   /// Publish the current mask centre and its provenance to INDI.
   /** Called wherever the mask centre can change: startup (config source),
     * camera allocate (image-centre fallback), and reference processing
     * (reference-derived).  Consumers (e.g. alignRasterMeas) subscribe
     * rather than duplicating the value in their own configs.
     */
   void publishMaskCenter()
   {
      updateIfChanged(m_indiP_maskCenter,
         std::vector<std::string>({"x", "y", "source"}),
         std::vector<float>({m_maskCenterX, m_maskCenterY,
                             static_cast<float>(static_cast<int>(m_maskCenterSource))}));
   }

   /// Consume a newly-staged reference image.  Runs on the camera thread only.
   /** Rebuilds the cross-correlator against m_refImage, resets Method 1's
     * tracking state, and (unless the config pinned it) re-extracts the mask
     * centre.  Called from the camera processImage when m_newRefPending is
     * observed, so m_xcorr and the mask/tracking state have a single writer.
     */
   void applyNewReference();

public:
   /// Default c'tor.
   alignPSFMeas();

   /// D'tor, declared and defined for noexcept.
   ~alignPSFMeas() noexcept
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

   /// Implementation of the logic for alignPSFMeas.
   /**
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
   virtual int appLogic();

   /// Shutdown the app.
   virtual int appShutdown();

   /// Change the loop state.
   /** Closing the loop starts alignment from scratch: it clears the
     * measurement-failure ERROR latch and counter, resets Method 1's
     * one-shot bootstrap, and zeroes
     * the measurement counter and status source.  No measurement state
     * survives from previous sessions; the published counter counts
     * measurements of the current session.
     */
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

   /// Background-subtracted intensity-weighted centroid in a circular window.
   /** The local background is estimated as the median of an annulus
     * rIn <= d <= rOut around (x0, y0) and subtracted; the centroid is
     * computed over the disc d <= rWin using only pixels above that background.
     * Don't use this for flux comparisons; use rawWindowFlux instead.
     * \param[in] im     the image to measure
     * \param[in] x0     initial row position (window centre)
     * \param[in] y0     initial col position (window centre)
     * \param[in] rWin   centroid window radius (pixels)
     * \param[in] rIn    background annulus inner radius (pixels)
     * \param[in] rOut   background annulus outer radius (pixels)
     * \param[out] cx    centroid row
     * \param[out] cy    centroid col
     * \param[out] flux  total background-subtracted flux in the window
     * \returns true on success, false if the window falls off the image or
     *          contains no flux above the local background.
     */
   bool windowedCentroid( const eigenImage<float> & im,
                          float x0, float y0,
                          float rWin, float rIn, float rOut,
                          float & cx, float & cy, float & flux );

   /// Raw (no background subtraction) centroid in a circular window.
   /** Used for all flux comparisons (quad flux similarity, core
     * dominance).
     * The window radius should roughly match the flux-ladder tests (12 px)
     * \returns the sum of pixel values in the disc d <= r (clipped at image edges).
     */
   float rawWindowFlux( const eigenImage<float> & im, float x0, float y0, float r );

   /// Draw a cross marker into an image (debug overlays).
   static void drawCross( eigenImage<float> & img, float x, float y, int half, float val );

   /// Create (on first use) and publish a debug image to a shmim named <configName><suffix>.
   /** Used by findFirstOrderSpeckles to expose intermediate products
     * (smoothed image, NMS candidates, found quad) for live inspection.
     * \returns 0 on success, -1 if the stream could not be created.
     */
   int dbgPublish( IMAGE & stream, bool & created, const std::string & suffix,
                   const eigenImage<float> & img );

   /// Find the four first-order DM speckles in an image.
   /** 3x3 median + binomial smoothing (hot-pixel/noise rejection),
     * local-maxima detection, greedy non-maximum suppression at a spatial
     * scale tied to speckleSep (so the PSF core/halo collapses to few
     * candidates regardless of its brightness), background-subtracted
     * windowed centroids, then a geometric search for 4 candidates forming
     * a square of diagonal 2*speckleSep (the geometry produced by the DM
     * sine/waffle pattern) with similar fluxes (quadFluxRatio).
     * \param[in] im        the image to search
     * \param[out] foundX   row coordinates of the 4 found speckle centroids
     * \param[out] foundY   col coordinates of the 4 found speckle centroids
     * \returns 0 if the speckle quad was found; foundX/foundY hold the 4 speckles.
     * \returns 1 if no quad exists but one candidate's raw flux dominates all
     *          others by coreDomFactor -- the unsuppressed PSF core.  All four
     *          entries of foundX/foundY then hold the core position, so a
     *          caller's mean-of-4 evaluates to the core position.
     * \returns -1 if neither (measurements should be held).
     */
   int findFirstOrderSpeckles( const eigenImage<float> & im,
                                float foundX[4], float foundY[4] );

   ///@}
};

alignPSFMeas::alignPSFMeas() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   // Load any existing reference image rather than waiting for a new post.
   refShmimMonitorT::m_getExistingFirst = true;
}

void alignPSFMeas::setupConfig()
{
   SHMIMMONITOR_SETUP_CONFIG(config);
   SHMIMMONITORT_SETUP_CONFIG(refShmimMonitorT, config);

   config.add("align.method", "", "align.method", argType::Optional, "align", "method", false, "int", "Displacement measurement method: 1=restricted xcorr (default), 2=speckle centroid.");
   config.add("align.maxLag", "", "align.maxLag", argType::Optional, "align", "maxLag", false, "int", "Maximum lag in pixels for cross-correlation. Default 10.");
   config.add("align.tol", "", "align.tol", argType::Optional, "align", "tol", false, "float", "Interpolation tolerance for cross-correlation peak. Default 0.001.");
   config.add("align.padFactor", "", "align.padFactor", argType::Optional, "align", "padFactor", false, "float", "Padding factor for the reference FFT. Default 1.");
   config.add("align.searchHalfWidth", "", "align.searchHalfWidth", argType::Optional, "align", "searchHalfWidth", false, "int", "Method 1: xcorr search box half-width in pixels. Default 10.");
   config.add("align.speckleSep", "", "align.speckleSep", argType::Optional, "align", "speckleSep", false, "float", "Methods 1&2: speckle distance from PSF centre in pixels. Default 50.");
   config.add("align.speckleAngle", "", "align.speckleAngle", argType::Optional, "align", "speckleAngle", false, "float", "Methods 1&2: angle of first speckle from row axis in degrees. Default 45.");
   config.add("align.predGain", "", "align.predGain", argType::Optional, "align", "predGain", false, "float", "Method 1: assumed loop gain for next-frame prediction. 0=no correction, 1=full correction. Keep in step with the loop's gain. Default 0.1.");
   config.add("align.maskCenterRepublish", "", "align.maskCenterRepublish", argType::Optional, "align", "maskCenterRepublish", false, "float", "Interval in seconds at which mask_center is re-asserted so subscribing apps converge regardless of start order. 0 disables. Default 10.");
   config.add("align.centroidRadius", "", "align.centroidRadius", argType::Optional, "align", "centroidRadius", false, "float", "Radius in pixels of the background-subtracted window used to centroid speckles. Default 6.");
   config.add("align.maxPeakCands", "", "align.maxPeakCands", argType::Optional, "align", "maxPeakCands", false, "int", "Bootstrap: maximum number of local-maxima candidates for the speckle quad search. Default 24.");
   config.add("align.quadFluxRatio", "", "align.quadFluxRatio", argType::Optional, "align", "quadFluxRatio", false, "float", "Bootstrap: maximum raw-flux ratio between brightest and faintest speckle in a quad. Default 3.");
   config.add("align.coreDomFactor", "", "align.coreDomFactor", argType::Optional, "align", "coreDomFactor", false, "float", "Bootstrap fallback: raw-flux dominance factor for identifying the unsuppressed core when no speckle quad exists. Default 25.");
   config.add("align.failLimit", "", "align.failLimit", argType::Optional, "align", "failLimit", false, "int", "Consecutive frames without a valid measurement (any method) before the loop is opened and the app holds ERROR until the loop is re-closed. Default 50.");
   config.add("align.debugImgEvery", "", "align.debugImgEvery", argType::Optional, "align", "debugImgEvery", false, "int", "Publish debug image shmims (smoothed, candidates, found quad) on the first and every Nth detection attempt. 0 (default) disables.");
   config.add("align.maskCenterX", "", "align.maskCenterX", argType::Optional, "align", "maskCenterX", false, "float", "Coronagraph mask centre row in pixels. Default: image centre.");
   config.add("align.maskCenterY", "", "align.maskCenterY", argType::Optional, "align", "maskCenterY", false, "float", "Coronagraph mask centre col in pixels. Default: image centre.");
}

int alignPSFMeas::loadConfigImpl(mx::app::appConfigurator & _config)
{
   SHMIMMONITOR_LOAD_CONFIG(_config);
   SHMIMMONITORT_LOAD_CONFIG(refShmimMonitorT, _config);

   int method = m_method.load();
   _config(method, "align.method");
   m_method.store(method);

   _config(m_maxLag, "align.maxLag");
   _config(m_xcorrTol, "align.tol");
   _config(m_xcorrPadFactor, "align.padFactor");
   _config(m_searchHalfWidth, "align.searchHalfWidth");

   _config(m_speckleSep, "align.speckleSep");
   _config(m_speckleAngle, "align.speckleAngle");

   _config(m_predGain, "align.predGain");
   _config(m_maskCenterRepublish, "align.maskCenterRepublish");

   _config(m_centroidRadius, "align.centroidRadius");
   _config(m_maxPeakCands, "align.maxPeakCands");
   _config(m_quadFluxRatio, "align.quadFluxRatio");
   _config(m_coreDomFactor, "align.coreDomFactor");

   _config(m_failLimit, "align.failLimit");
   if(m_failLimit < 1) m_failLimit = 1;

   _config(m_debugImgEvery, "align.debugImgEvery");

   _config(m_maskCenterX, "align.maskCenterX");
   _config(m_maskCenterY, "align.maskCenterY");
   // Priority (1, highest): a config-file mask centre is never overridden.
   if(m_maskCenterX >= 0 && m_maskCenterY >= 0)
   {
      m_maskCenterSource = maskCenterSource::config;
   }


   float halfSep = m_speckleSep / 2.0f;

   if(m_searchHalfWidth >= halfSep)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "searchHalfWidth (" + std::to_string(m_searchHalfWidth)
         + ") must be < speckleSep/2 (" + std::to_string(halfSep)
         + ") to exclude higher-order speckle peaks"});
   }

   if(m_predGain < 0.0f || m_predGain > 1.0f)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "predGain must be between 0 and 1"});
   }

   return 0;
}

void alignPSFMeas::loadConfig()
{
   if(loadConfigImpl(config) < 0)
   {
      m_shutdown = true;
   }
}

int alignPSFMeas::appStartup()
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
   CREATE_REG_INDI_NEW_NUMBERU( m_indiP_method, "method", 1, 2, 1, "%u", "method", "alignment");
   m_indiP_method["current"].set(static_cast<unsigned>(m_method.load()));

   // debug image products: publish every N detection attempts (0 = off)
   CREATE_REG_INDI_NEW_NUMBERU( m_indiP_debugEvery, "debug_imgs", 0, 1000000, 1, "%u", "debug images every", "alignment");
   m_indiP_debugEvery["current"].set(static_cast<unsigned>(m_debugImgEvery));

   // measurement status (read-only monitoring; meaningful for all methods)
   registerIndiPropertyReadOnly(m_indiP_measStatus, "measurement_status",
      pcf::IndiProperty::Number,
      pcf::IndiProperty::ReadOnly,
      pcf::IndiProperty::Idle);
   m_indiP_measStatus.add({"ok", 0.0});                // 1 = this frame published a measurement
   m_indiP_measStatus.add({"source", 0.0});            // most recent measurement: 0=none yet, 1=xcorr,
                                                       // 2=speckle quad, 3=dominant core
   m_indiP_measStatus.add({"fail_count", 0.0});        // consecutive frames without a valid measurement

   // mask centre (read-only)
   registerIndiPropertyReadOnly(m_indiP_maskCenter, "mask_center",
      pcf::IndiProperty::Number,
      pcf::IndiProperty::ReadOnly,
      pcf::IndiProperty::Idle);
   m_indiP_maskCenter.add({"x", static_cast<double>(m_maskCenterX)});     // row, pixels
   m_indiP_maskCenter.add({"y", static_cast<double>(m_maskCenterY)});     // col, pixels
   m_indiP_maskCenter.add({"source",                                      // 0=image centre, 1=reference image, 2=config
      static_cast<double>(static_cast<int>(m_maskCenterSource))});


   state(stateCodes::READY);

   return 0;
}

int alignPSFMeas::appLogic()
{
   SHMIMMONITOR_APP_LOGIC;
   SHMIMMONITORT_APP_LOGIC(refShmimMonitorT);

   SHMIMMONITOR_UPDATE_INDI;
   SHMIMMONITORT_UPDATE_INDI(refShmimMonitorT);

   if(m_measureFailed.load())
   {
      // Latched by the measurement failure policy in processImage (which
      // also opened the loop).  Sticky: only re-closing the loop clears it.
      state(stateCodes::ERROR);
   }
   else if(m_loopClosed.load())
   {
      state(stateCodes::OPERATING);
   }
   else
   {
      state(stateCodes::READY);
   }

   return 0;
}

int alignPSFMeas::appShutdown()
{
   SHMIMMONITOR_APP_SHUTDOWN;
   SHMIMMONITORT_APP_SHUTDOWN(refShmimMonitorT);

   return 0;
}

// --- camera shmim ---

int alignPSFMeas::allocate(const dev::shmimT &)
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
      publishMaskCenter();
   }

   // New/reconnected stream: its write counter is unrelated to the old one.
   m_lastProcCnt0 = std::numeric_limits<uint64_t>::max();

   return 0;
}

int alignPSFMeas::processImage(void *curr_src, const dev::shmimT &)
{
   // Skip backlogged semaphore wakeups. shmimMonitor hands every wakeup a
   // pointer to the NEWEST frame slot, so when a slow frame (the ~40 ms
   // speckle detection) lets camera posts queue up, the drain re-delivers
   // one frame several times within milliseconds. Measuring it repeatedly
   // would publish a burst of identical stale measurements, each producing
   // an fsm command. With Method 2 running the detector on every frame this
   // is the steady state, not a transient: detection at ~25-40 Hz against a
   // 100 Hz camera, with the excess wakeups skipped here. Process each
   // distinct camera frame (identified by the stream's write counter) at
   // most once.
   const uint64_t cnt0 = shmimMonitorT::m_imageStream.md[0].cnt0;
   if(cnt0 == m_lastProcCnt0)
   {
      ++m_backlogSkips;
      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
         log<software_debug>({__FILE__, __LINE__,
            "skipping backlogged wakeup for already-processed frame cnt0=" +
            std::to_string(cnt0) + " (total skips " + std::to_string(m_backlogSkips) + ")"});
      }
      return 0;
   }
   m_lastProcCnt0 = cnt0;

   // Consume a staged reference image on this thread, so that m_xcorr and the
   // mask/tracking state it rebuilds are only ever touched here.  Done above
   // the loop-closed gate so a new reference is applied whether the loop is
   // open or closed; the acquire pairs with the reference thread's release.
   if(m_newRefPending.load(std::memory_order_acquire))
   {
      applyNewReference();
      m_newRefPending.store(false, std::memory_order_relaxed);
   }

   // Re-assert mask_center for subscribers (see m_maskCenterRepublish).  Done
   // here rather than in appLogic so the mask centre, the timestamp and the
   // validity check all stay owned by this thread -- no cross-thread state.
   // Guarded on a determined value so a consumer never receives the -1
   // "unset" sentinel and mistakes it for a real mask centre.
   if(m_maskCenterRepublish > 0.0f && m_maskCenterX >= 0.0f && m_indiDriver != nullptr)
   {
      const double tNow = mx::sys::get_curr_time();
      if(tNow - m_lastMaskCenterPub >= m_maskCenterRepublish)
      {
         m_lastMaskCenterPub = tNow;
         try
         {
            m_indiDriver->sendSetProperty(m_indiP_maskCenter);
         }
         catch(const std::exception & e)
         {
            log<software_error>({__FILE__, __LINE__,
               std::string("exception re-asserting mask_center: ") + e.what()});
         }
      }
   }

   const bool loopClosed = m_loopClosed.load(std::memory_order_acquire);

   if(!loopClosed || m_camPixget == nullptr)
   {
      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
         std::ostringstream oss;
         oss << "Skipping frame: loopClosed=" << loopClosed
             << " camPixget=" << (m_camPixget != nullptr);
         log<software_debug>({__FILE__, __LINE__, oss.str()});
      }
      return 0;
   }

   // Copy the incoming frame to the float buffer.
   const uint64_t npix = shmimMonitorT::m_width * shmimMonitorT::m_height;
   for(uint64_t nn = 0; nn < npix; ++nn)
      m_camImage.data()[nn] = m_camPixget(curr_src, nn);

   // Per-frame outcome, shared by all methods and consumed by the single
   // publish/failure policy at the end of this function.
   float xs = 0.0f, ys = 0.0f;
   bool measured = false;  // did this frame produce a valid measurement?
   int source = 0;         // how (measurement_status source code, see appStartup)
   std::string why;        // failure reason for logging when !measured

   // One acquire load per frame: pairs with the method callback's release so
   // any tracking reset that accompanied a method change is visible before we
   // read m_initialized/m_estShift below, and guarantees the whole frame is
   // processed with a single, consistent method.
   const int method = m_method.load(std::memory_order_acquire);

   // --------------------------
   // Method 2: speckle centroid
   // --------------------------
   // Stateless per-frame detection: every frame independently answers
   // "where is the PSF?" from the image alone -- no prior, no tracking
   // state, no assumed scenario.  findFirstOrderSpeckles validates
   // internally (quad geometry + raw-flux similarity; else raw-flux core
   // dominance), and its quad-else-core fallback doubles as the coarse/fine
   // measurement policy: far out, with speckles off the frame, the
   // unsuppressed core carries the measurement; the quad takes over as soon
   // as it is visible -- including across the mask-transit zone, where one
   // occulted speckle breaks the quad while the core is still clear.
   if(method == 2)
   {
      float sx[4], sy[4];
      int ffs = findFirstOrderSpeckles(m_camImage, sx, sy);

      if(ffs >= 0)
      {
         // Code 1 (dominant core) stores the core position in all four
         // slots, so the mean works identically for both outcomes.
         float sumX = 0.0f, sumY = 0.0f;
         for(int n = 0; n < 4; ++n) { sumX += sx[n]; sumY += sy[n]; }

         xs = sumX / 4.0f - m_maskCenterX;
         ys = sumY / 4.0f - m_maskCenterY;

         measured = true;
         source = (ffs == 0) ? 2 : 3;
      }
      else
      {
         why = "speckle detection found no quad and no dominant core";
      }
   }
   // -------------------------------------------------------
   // Method 1: restricted cross-correlation (default)
   // -------------------------------------------------------
   else
   {
      if(!m_refImLoaded)
      {
         why = "method 1 requires a reference image and none is loaded";
      }
      else
      {
         // One-shot bootstrap: locate the PSF once to place the xcorr search
         // box; afterwards xcorr tracks iteratively via m_estShiftX/Y.
         if(!m_initialized)
         {
            float sx[4], sy[4];
            int ffs = findFirstOrderSpeckles(m_camImage, sx, sy);
            if(ffs >= 0)
            {
               float sumX = 0.0f, sumY = 0.0f;
               for(int n = 0; n < 4; ++n) { sumX += sx[n]; sumY += sy[n]; }

               m_estShiftX = sumX / 4.0f - m_maskCenterX;
               m_estShiftY = sumY / 4.0f - m_maskCenterY;

               log<software_info>({__FILE__, __LINE__, "Bootstrap PSF shift: (" +
                             std::to_string(m_estShiftX) + ", " + std::to_string(m_estShiftY) +
                             (ffs == 0 ? ") via speckle quad" : ") via dominant core")});

               m_initialized = true;
            }
            else
            {
               why = "bootstrap speckle detection failed";
            }
         }

         if(m_initialized)
         {
            m_xcorr.searchBox(m_estShiftX, m_estShiftY, m_searchHalfWidth);

            try
            {
               m_xcorr(xs, ys, m_camImage);
               measured = true;
               source = 1;
            }
            catch(const std::exception & e)
            {
               why = std::string("xcorr exception: ") + e.what();
            }

            if(measured)
            {
               m_estShiftX = xs * (1.0f - m_predGain);
               m_estShiftY = ys * (1.0f - m_predGain);
            }
         }
      }
   }

   // ------------------------------------------------------------
   // Publish, or count the failure: one policy for all methods.
   // ------------------------------------------------------------
   if(measured)
   {
      if(m_measureFailCount > 0)
      {
         log<software_notice>({__FILE__, __LINE__,"measurement recovered after " +
                       std::to_string(m_measureFailCount) + " failed frames"});
         m_measureFailCount = 0;
      }
      m_lastSource = source;

      // The counter is published through a float element whose INDI text
      // serialization keeps ~6 significant digits: beyond 1e6, consecutive
      // values serialize identically ("1e+06") and consumers stop seeing
      // changes. Wrap at 1e6 - consumers (closedLoopIndi) act on counter
      // changes only, so the wrap is harmless but doesn't overflow the range.
      // This publish carries the default Ok state, which clears any Alert
      // set by an earlier failure streak.
      ++m_counter;
      updateIfChanged(m_indiP_measurement,
         std::vector<std::string>({"delta_x", "delta_y", "counter"}),
         std::vector<float>({xs, ys, static_cast<float>(m_counter % 1000000)}));
   }
   else
   {
      ++m_measureFailCount;

      if(m_measureFailCount == 1)
      {
         // Warn once per failure streak and put the measurement property
         // into the INDI Alert state (red in GUIs). The element values and
         // (crucially) the counter are unchanged, so the loop cannot act on
         // this update.
         log<software_warning>({__FILE__, __LINE__,
            "no valid measurement: " + why + "; holding (measurement set to Alert)"});

         updateIfChanged(m_indiP_measurement,
            std::vector<std::string>({"delta_x", "delta_y", "counter"}),
            std::vector<float>({m_indiP_measurement["delta_x"].get<float>(),
                                m_indiP_measurement["delta_y"].get<float>(),
                                m_indiP_measurement["counter"].get<float>()}),
            INDI_ALERT);
      }

      if(m_measureFailCount >= m_failLimit && !m_measureFailed.load())
      {
         // Fail loudly and stop: open the loop and hold ERROR (appLogic)
         // until an operator or external app re-closes the loop, which
         // clears this latch and the counter (toggleLoop).
         log<software_error>({__FILE__, __LINE__,
            "no valid measurement for " + std::to_string(m_measureFailCount) +
            " consecutive frames (last: " + why + "); opening loop and going to ERROR"});
         m_measureFailed.store(true);
         toggleLoop(false);
      }
   }

   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      std::ostringstream oss;
      oss << "frame: method=" << method
          << " ok=" << (measured ? 1 : 0)
          << " source=" << m_lastSource
          << " fail_count=" << m_measureFailCount;
      if(measured)
      {
         oss << " shift=(" << xs << "," << ys << ")";
      }
      else
      {
         oss << " (" << why << ")";
      }
      log<software_debug>({__FILE__, __LINE__, oss.str()});
   }

   updateIfChanged(m_indiP_measStatus,
      std::vector<std::string>({"ok", "source", "fail_count"}),
      std::vector<float>({measured ? 1.0f : 0.0f,
                          static_cast<float>(m_lastSource),
                          static_cast<float>(m_measureFailCount)}));

   return 0;
}

// --- reference shmim ---

int alignPSFMeas::allocate(const refShmimT &)
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

int alignPSFMeas::processImage(void *curr_src, const refShmimT &)
{
   if(m_refPixget == nullptr)
   {
      return 0;
   }

   const uint64_t npix = refShmimMonitorT::m_width * refShmimMonitorT::m_height;
   for(uint64_t nn = 0; nn < npix; ++nn)
      m_refImage.data()[nn] = m_refPixget(curr_src, nn);

   // Stage only.  The correlator rebuild, tracking reset and mask-centre
   // extraction all happen on the camera thread in applyNewReference(), so
   // m_xcorr is never mutated while that thread is correlating with it.
   // The release-store publishes the pixels written above.
   //
   // A newer reference simply overwrites an unconsumed one -- the latest is
   // the one we want.  This assumes reference writes are far apart relative
   // to one camera frame (they are operator/setup actions, while the camera
   // thread consumes within ~1 frame); two writes inside a single frame
   // would race on the buffer.
   m_newRefPending.store(true, std::memory_order_release);

   return 0;
}

void alignPSFMeas::applyNewReference()
{
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
      log<software_error>({__FILE__, __LINE__,
         std::string("exception setting xcorr reference: ") + e.what()});
      return;
   }

   // New reference: reset Method 1's tracking state so its next camera
   // frame re-bootstraps against the new template.
   m_initialized = false;
   m_estShiftX = 0.0f;
   m_estShiftY = 0.0f;

   m_refImLoaded = true;
   log<software_info>({__FILE__, __LINE__,
      "Reference image updated (method=" + std::to_string(m_method.load()) + ")"});

   // Priority (2): unless the config file set it, (re)extrapolate the mask centre
   // from the reference image -- the PSF sits on the mask there, so its centre
   // (the mean of the first-order speckles) is the mask centre.
   if(m_maskCenterSource != maskCenterSource::config)
   {
      float rsx[4], rsy[4];
      // The reference must be an on-mask scene, so only the quad result (0) is
      // acceptable here.  A dominant core (1) means the reference was captured
      // with the PSF off the mask -- flag it rather than use it.
      int rref = findFirstOrderSpeckles(m_refImage, rsx, rsy);
      if(rref == 0)
      {
         float sx = 0.0f, sy = 0.0f;
         for(int n = 0; n < 4; ++n) { sx += rsx[n]; sy += rsy[n]; }
         m_maskCenterX = sx / 4.0f;
         m_maskCenterY = sy / 4.0f;
         m_maskCenterSource = maskCenterSource::reference;
         publishMaskCenter();
         log<software_info>({__FILE__, __LINE__,
            "Mask centre extrapolated from reference image: (" +
            std::to_string(m_maskCenterX) + ", " + std::to_string(m_maskCenterY) + ")"});
      }
      else if(rref == 1)
      {
         log<software_warning>({__FILE__, __LINE__,
            "reference image contains a dominant unsuppressed core -- was it "
            "captured with the PSF off the mask?  Not using it for the mask centre."});
      }
      else
      {
         log<software_warning>({__FILE__, __LINE__,
            "could not extrapolate mask centre from reference image"});
      }
   }
}


// ============================================================
// PSF detection helpers
// ============================================================

bool alignPSFMeas::windowedCentroid( const eigenImage<float> & im,
                                      float x0, float y0,
                                      float rWin, float rIn, float rOut,
                                      float & cx, float & cy, float & flux )
{
   const int R = (int)std::ceil(rOut);
   const int r0 = std::max(0, (int)std::floor(x0) - R);
   const int r1 = std::min((int)im.rows()-1, (int)std::ceil(x0) + R);
   const int c0 = std::max(0, (int)std::floor(y0) - R);
   const int c1 = std::min((int)im.cols()-1, (int)std::ceil(y0) + R);

   if(r0 > r1 || c0 > c1) return false;

   // Local background: median of the annulus rIn <= d <= rOut.  The median is
   // robust to a neighbouring bright structure clipping the annulus.
   std::vector<float> ann;
   for(int r = r0; r <= r1; ++r)
   {
      for(int c = c0; c <= c1; ++c)
      {
         float d = std::sqrt(std::pow(r - x0, 2) + std::pow(c - y0, 2));
         if(d >= rIn && d <= rOut) ann.push_back(im(r, c));
      }
   }

   if(ann.size() < 8) return false; // window is (almost) entirely off the image

   std::nth_element(ann.begin(), ann.begin() + ann.size()/2, ann.end());
   const float bg = ann[ann.size()/2];

   // Intensity-weighted centroid of (im - bg) over the disc d <= rWin,
   // using only pixels above the local background.
   double sumW = 0, sumR = 0, sumC = 0;
   for(int r = r0; r <= r1; ++r)
   {
      for(int c = c0; c <= c1; ++c)
      {
         float d = std::sqrt(std::pow(r - x0, 2) + std::pow(c - y0, 2));
         if(d > rWin) continue;
         float w = im(r, c) - bg;
         if(w <= 0) continue;
         sumW += w;
         sumR += w * r;
         sumC += w * c;
      }
   }

   if(sumW <= 0) return false; // nothing above local background: not a compact peak

   cx   = (float)(sumR / sumW);
   cy   = (float)(sumC / sumW);
   flux = (float)sumW;

   return true;
}

float alignPSFMeas::rawWindowFlux( const eigenImage<float> & im, float x0, float y0, float r )
{
   const int R = (int)std::ceil(r);
   const int r0 = std::max(0, (int)std::floor(x0) - R);
   const int r1 = std::min((int)im.rows()-1, (int)std::ceil(x0) + R);
   const int c0 = std::max(0, (int)std::floor(y0) - R);
   const int c1 = std::min((int)im.cols()-1, (int)std::ceil(y0) + R);

   double sum = 0;
   for(int rr = r0; rr <= r1; ++rr)
   {
      for(int cc = c0; cc <= c1; ++cc)
      {
         float d = std::sqrt(std::pow(rr - x0, 2) + std::pow(cc - y0, 2));
         if(d <= r) sum += im(rr, cc);
      }
   }

   return (float)sum;
}

void alignPSFMeas::drawCross( eigenImage<float> & img, float x, float y, int half, float val )
{
   const int xr = (int)std::round(x);
   const int yc = (int)std::round(y);

   if(yc >= 0 && yc < (int)img.cols())
   {
      for(int r = std::max(0, xr - half); r <= std::min((int)img.rows() - 1, xr + half); ++r)
         img(r, yc) = val;
   }

   if(xr >= 0 && xr < (int)img.rows())
   {
      for(int c = std::max(0, yc - half); c <= std::min((int)img.cols() - 1, yc + half); ++c)
         img(xr, c) = val;
   }
}

int alignPSFMeas::dbgPublish( IMAGE & stream, bool & created, const std::string & suffix,
                              const eigenImage<float> & img )
{
   if(!created)
   {
      uint32_t imsize[3];
      imsize[0] = (uint32_t)img.rows();
      imsize[1] = (uint32_t)img.cols();
      imsize[2] = 0;

      if(ImageStreamIO_createIm_gpu(&stream, (configName() + suffix).c_str(), 2, imsize,
                                    _DATATYPE_FLOAT, -1, 1, IMAGE_NB_SEMAPHORE, 0, MATH_DATA, 0) != 0)
      {
         return log<software_error, -1>({__FILE__, __LINE__,
            "failed to create debug shmim " + configName() + suffix});
      }
      created = true;
      log<software_info>({__FILE__, __LINE__, "created debug shmim " + configName() + suffix});
   }

   stream.md->write = 1;
   float * dest = (float *)stream.array.raw;
   const uint64_t npix = (uint64_t)img.rows() * (uint64_t)img.cols();
   for(uint64_t n = 0; n < npix; ++n) dest[n] = img.data()[n];
   stream.md->write = 0;
   stream.md->cnt0++;
   ImageStreamIO_sempost(&stream, -1);

   return 0;
}

int alignPSFMeas::findFirstOrderSpeckles( const eigenImage<float> & im,
                                           float foundX[4], float foundY[4] )
{
   const int nrows = (int)im.rows();
   const int ncols = (int)im.cols();

   if(nrows < 7 || ncols < 7) return -1;

   // Debug image products: publish on the 1st and every Nth attempt when enabled.
   const bool dbg = (m_debugImgEvery > 0) && (m_dbgAttempts % (uint64_t)m_debugImgEvery == 0);
   ++m_dbgAttempts;

   // --- Step 1: hot-pixel-safe smoothing ---
   // All measurements happen on the original image; this
   // smoothed copy is used for detection only.

   // A 3x3 median filter erases single-pixel spikes (hot pixels)
   eigenImage<float> med = im;
   for(int c = 1; c < ncols-1; ++c)
   {
      for(int r = 1; r < nrows-1; ++r)
      {
         float v[9];
         int n = 0;
         for(int dc = -1; dc <= 1; ++dc)
            for(int dr = -1; dr <= 1; ++dr) 
            {
               v[n++] = im(r+dr, c+dc);
            }
         std::nth_element(v, v+4, v+9);
         med(r, c) = v[4];
      }
   }

   // A 5-tap binomial blur (~Gaussian, sigma ~1 px) suppresses pixel noise so
   // that only structures a few pixels wide (real speckles) survive as
   // prominent local maxima.
   static const float k5[5] = {1.0f/16, 4.0f/16, 6.0f/16, 4.0f/16, 1.0f/16};
   eigenImage<float> tmp = med;
   for(int c = 0; c < ncols; ++c)
   {
      for(int r = 2; r < nrows-2; ++r)
      {
         float s = 0;
         for(int i = 0; i < 5; ++i) 
         {
            s += k5[i] * med(r-2+i, c);
         }
         tmp(r, c) = s;
      }
   }
   eigenImage<float> sm = tmp;
   for(int c = 2; c < ncols-2; ++c)
   {
      for(int r = 0; r < nrows; ++r)
      {
         float s = 0;
         for(int i = 0; i < 5; ++i) s += k5[i] * tmp(r, c-2+i);
         sm(r, c) = s;
      }
   }

   if(dbg) dbgPublish(m_dbgSmoothStream, m_dbgSmoothCreated, "_dbg_smooth", sm);

   // --- Step 2: local maxima of the smoothed image ---
   struct Peak
   { 
      int r, c;
      float v; 
   };
   
   std::vector<Peak> peaks;
   for(int c = 1; c < ncols-1; ++c)
   {
      for(int r = 1; r < nrows-1; ++r)
      {
         const float v = sm(r, c);
         if(v <= 0) continue;
         bool isMax = true;
         for(int dc = -1; dc <= 1 && isMax; ++dc)
         {
            for(int dr = -1; dr <= 1; ++dr)
            {
               if(dr == 0 && dc == 0) continue;
               if(sm(r+dr, c+dc) > v)
               {
                  isMax = false;
                  break;
               }
            }
         }
         if(isMax) peaks.push_back({r, c, v});
      }
   }

   if(peaks.size() < 2)
   {
      log<software_debug>({__FILE__, __LINE__,
         "findFirstOrderSpeckles: only " + std::to_string(peaks.size()) +
         " local maxima found (needs at least 2 for a dominant core fallback)"});
      return -1;
   }

   std::sort(peaks.begin(), peaks.end(),
             [](const Peak & a, const Peak & b){ return a.v > b.v; });

   // --- Step 3: greedy non-maximum suppression + windowed centroids ---
   // Accept maxima brightest-first, suppressing anything within dMin of an
   // already-accepted peak. dMin is tied to the known speckle separation,
   // so the core+halo region collapses to a few candidates and doesn't 
   // crowd the true speckles out of the list.
   const float dMin = m_speckleSep / 3.0f;

   // rawFlux = raw (no background subtraction) window sum.
   struct Cand { float x, y, rawFlux; };
   std::vector<Cand> cand;
   std::vector<std::pair<int,int>> accepted; // accepted raw peak positions

   for(size_t p = 0; p < peaks.size() && (int)accepted.size() < m_maxPeakCands; ++p)
   {
      bool clear = true;
      for(size_t a = 0; a < accepted.size(); ++a)
      {
         const float d = std::sqrt(std::pow(peaks[p].r - accepted[a].first, 2) +
                                   std::pow(peaks[p].c - accepted[a].second, 2));
         if(d < dMin) { clear = false; break; }
      }
      if(!clear) continue;

      accepted.push_back(std::make_pair(peaks[p].r, peaks[p].c));

      float ccx, ccy, cfl;
      if(windowedCentroid(im, (float)peaks[p].r, (float)peaks[p].c,
                          m_centroidRadius, m_centroidRadius + 2.0f, m_centroidRadius + 6.0f,
                          ccx, ccy, cfl))
      {
         cand.push_back({ccx, ccy,
                         rawWindowFlux(im, ccx, ccy, 2.0f * m_centroidRadius)});
      }
   }

   // Debug: smoothed image with every NMS-accepted candidate marked, published
   // before the size/geometry checks so failed attempts can be inspected too.
   if(dbg)
   {
      eigenImage<float> ov = sm;
      const float mv = 1.1f * sm.maxCoeff();
      for(size_t a = 0; a < accepted.size(); ++a)
         drawCross(ov, (float)accepted[a].first, (float)accepted[a].second, 4, mv);
      dbgPublish(m_dbgCandsStream, m_dbgCandsCreated, "_dbg_cands", ov);
   }

   // Note: the quad search below needs 4 candidates to form a quad (with
   // fewer, its loops simply find nothing), but the dominant-core fallback
   // after it needs at least 2 candidates to compare fluxes.
   // No early return here.

   // --- Step 4: geometric consistency ---
   // A true quad measures to ~1-2 px, so 10% of the separation is a generous
   // distance tolerance while rejecting junk quads.
   const float expectedDiag = 2.0f * m_speckleSep;
   const float distTol = m_speckleSep * 0.1f;
   const size_t N = cand.size();

   float bestScore = std::numeric_limits<float>::max();
   int bestQuad[4] = {-1, -1, -1, -1};

   for(size_t i = 0; i < N; ++i)
   {
      for(size_t j = i + 1; j < N; ++j)
      {
         float dij = std::sqrt(std::pow(cand[i].x - cand[j].x, 2) +
                               std::pow(cand[i].y - cand[j].y, 2));
         if(std::abs(dij - expectedDiag) > distTol) continue;

         float midX = (cand[i].x + cand[j].x) / 2.0f;
         float midY = (cand[i].y + cand[j].y) / 2.0f;

         for(size_t k = 0; k < N; ++k)
         {
            if(k == i || k == j) continue;
            float dk = std::sqrt(std::pow(cand[k].x - midX, 2) +
                                 std::pow(cand[k].y - midY, 2));
            if(std::abs(dk - m_speckleSep) > distTol) continue;

            for(size_t l = k + 1; l < N; ++l)
            {
               if(l == i || l == j) continue;
               float dl = std::sqrt(std::pow(cand[l].x - midX, 2) +
                                    std::pow(cand[l].y - midY, 2));
               if(std::abs(dl - m_speckleSep) > distTol) continue;

               float dkl = std::sqrt(std::pow(cand[k].x - cand[l].x, 2) +
                                     std::pow(cand[k].y - cand[l].y, 2));
               if(std::abs(dkl - expectedDiag) > distTol) continue;

               // The two diagonals of a true (square) quad are orthogonal.
               const float dot = std::fabs(
                  (cand[j].x - cand[i].x) * (cand[l].x - cand[k].x) +
                  (cand[j].y - cand[i].y) * (cand[l].y - cand[k].y));
               if(dot > 0.15f * dij * dkl) continue;

               // True first-order siblings have near-identical raw flux
               // (measured spread ~1.1x), while any imposter quad must mix
               // different orders (core/1st ~60x, 1st/2nd ~10x).
               const float fmin = std::min(std::min(cand[i].rawFlux, cand[j].rawFlux),
                                           std::min(cand[k].rawFlux, cand[l].rawFlux));
               const float fmax = std::max(std::max(cand[i].rawFlux, cand[j].rawFlux),
                                           std::max(cand[k].rawFlux, cand[l].rawFlux));
               if(fmax > m_quadFluxRatio * fmin) continue;

               float di = std::sqrt(std::pow(cand[i].x - midX, 2) +
                                    std::pow(cand[i].y - midY, 2));
               float dj = std::sqrt(std::pow(cand[j].x - midX, 2) +
                                    std::pow(cand[j].y - midY, 2));
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
      // No quad exists in the image; e.g. one first-order speckle is behind
      // the coronagraph mask or speckles are off-frame.
      // Fallback: if one candidate's raw flux dominates every
      // other by coreDomFactor, it can only be the unsuppressed PSF core
      // so use its position directly. An on-mask case can never trigger
      // this: there the brightest candidates are sibling speckles (ratio ~1).
      if(cand.size() >= 2)
      {
         size_t ib = 0;
         for(size_t i = 1; i < cand.size(); ++i)
         {
            if(cand[i].rawFlux > cand[ib].rawFlux) ib = i;
         }

         float fSecond = -1;
         for(size_t i = 0; i < cand.size(); ++i)
         {
            if(i != ib && cand[i].rawFlux > fSecond) fSecond = cand[i].rawFlux;
         }

         if(fSecond > 0 && cand[ib].rawFlux >= m_coreDomFactor * fSecond)
         {
            // Return the core position in all four slots so the callers'
            // mean-of-4 evaluates to the core position.
            for(int n = 0; n < 4; ++n)
            {
               foundX[n] = cand[ib].x;
               foundY[n] = cand[ib].y;
            }

            log<software_info>({__FILE__, __LINE__, "findFirstOrderSpeckles: no quad; dominant core at (" +
                          std::to_string(cand[ib].x) + ", " + std::to_string(cand[ib].y) +
                          "), dominance ratio " + std::to_string(cand[ib].rawFlux / fSecond)});

            if(dbg)
            {
               eigenImage<float> ov = im;
               drawCross(ov, cand[ib].x, cand[ib].y, 2 * (int)m_centroidRadius,
                         1.1f * im.maxCoeff());
               dbgPublish(m_dbgQuadStream, m_dbgQuadCreated, "_dbg_quad", ov);
            }

            return 1;
         }

         log<software_debug>({__FILE__, __LINE__,
            "findFirstOrderSpeckles: no geometrically consistent quad among " +
            std::to_string(N) + " candidates and no dominant core (best ratio " +
            std::to_string(fSecond > 0 ? cand[ib].rawFlux / fSecond : 0.0f) + ")"});
         return -1;
      }

      log<software_debug>({__FILE__, __LINE__,
         "findFirstOrderSpeckles: only " + std::to_string(cand.size()) +
         " usable candidates (need 4 for a quad, 2 for the core fallback)"});
      return -1;
   }

   for(int n = 0; n < 4; ++n)
   {
      foundX[n] = cand[bestQuad[n]].x;
      foundY[n] = cand[bestQuad[n]].y;
   }

   // Debug: original image with the found speckles and the implied PSF centre marked.
   if(dbg)
   {
      eigenImage<float> ov = im;
      const float mv = 1.1f * im.maxCoeff();
      float cX = 0.0f, cY = 0.0f;
      for(int n = 0; n < 4; ++n)
      {
         drawCross(ov, foundX[n], foundY[n], (int)m_centroidRadius, mv);
         cX += foundX[n];
         cY += foundY[n];
      }
      drawCross(ov, cX / 4.0f, cY / 4.0f, 2 * (int)m_centroidRadius, mv); // PSF centre
      dbgPublish(m_dbgQuadStream, m_dbgQuadCreated, "_dbg_quad", ov);
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

int alignPSFMeas::toggleLoop(bool onoff)
{
   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      std::ostringstream oss;
      oss << "Received request to toggle loop to " << (onoff ? "ON" : "OFF") << ".";
      oss <<"Current loop state is " << (m_loopClosed.load() ? "ON" : "OFF");
      log<software_debug>({__FILE__, __LINE__, oss.str()});
   }

   if(!m_loopClosed.load() && onoff)
   {
      // Closing the loop means "begin alignment from scratch, with whatever
      // the camera shows now" -- no state survives from previous sessions.
      // It is also the recovery action for a measurement-failure ERROR:
      // clear the latch and the counter so acquisition starts fresh.
      //
      // NB: every reset below is written BEFORE the release-store of
      // m_loopClosed.  The camera thread acquire-loads that flag at its gate,
      // so observing "closed" guarantees it also sees these resets -- which
      // is what lets them stay plain (non-atomic) fields.  Setting the flag
      // first would let the camera thread start a frame on last session's
      // stale estimate on a weakly-ordered machine.
      m_measureFailCount = 0;
      m_measureFailed.store(false);

      // Method 1 re-bootstraps rather than resuming from a stale shift
      // estimate.  (Method 2 is stateless per frame and needs no reset.)
      m_initialized = false;
      m_estShiftX = 0.0f;
      m_estShiftY = 0.0f;

      // The counter counts measurements published this session, and
      // measurement_status starts at source=0 ("none yet").
      m_counter = 0;
      m_lastSource = 0;

      m_loopClosed.store(true, std::memory_order_release);

      log<loop_closed>();
      updateSwitchIfChanged(m_indiP_ctrlEnabled, "toggle", pcf::IndiElement::On, INDI_OK);
      return 0;
   }

   if(m_loopClosed.load() && !onoff)
   {
      m_loopClosed.store(false, std::memory_order_release);
      log<loop_open>();
      updateSwitchIfChanged(m_indiP_ctrlEnabled, "toggle", pcf::IndiElement::Off, INDI_IDLE);
      return 0;
   }

   return 0;
}


INDI_NEWCALLBACK_DEFN(alignPSFMeas, m_indiP_ctrlEnabled)(const pcf::IndiProperty &ipRecv)
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

INDI_NEWCALLBACK_DEFN(alignPSFMeas, m_indiP_method)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_method);

   float target;
   if( indiTargetUpdate(m_indiP_method, target, ipRecv, true) < 0 )
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }

   unsigned newMethod = static_cast<unsigned>(target + 0.5f);
   if(newMethod < 1 || newMethod > 2)
   {
      updateIfChanged(m_indiP_method, "current", static_cast<unsigned>(m_method.load()));
      return 0;
   }

   if(static_cast<int>(newMethod) != m_method.load())
   {
      // Reset Method 1's one-shot bootstrap so the new method starts clean.
      // (Method 2 is stateless per frame and needs no reset.)  As in
      // toggleLoop, these are written BEFORE the release-store of m_method,
      // which the camera thread acquire-loads each frame to branch -- so the
      // resets are published with the method change and stay plain fields.
      m_initialized = false;
      m_estShiftX = 0.0f;
      m_estShiftY = 0.0f;

      m_method.store(static_cast<int>(newMethod), std::memory_order_release);

      log<software_info>({__FILE__, __LINE__, "Method changed to " + std::to_string(m_method.load())});
   }

   updateIfChanged(m_indiP_method, "current", static_cast<unsigned>(m_method.load()));
   return 0;
}

INDI_NEWCALLBACK_DEFN(alignPSFMeas, m_indiP_debugEvery)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_debugEvery);

   float target;
   if( indiTargetUpdate(m_indiP_debugEvery, target, ipRecv, true) < 0 )
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }

   unsigned every = static_cast<unsigned>(target + 0.5f);

   if(static_cast<int>(every) != m_debugImgEvery)
   {
      m_debugImgEvery = static_cast<int>(every);
      m_dbgAttempts = 0; // ensures the very next attempt publishes when enabling

      if(m_debugImgEvery > 0)
      {
         log<software_info>({__FILE__, __LINE__, "debug image publishing enabled: every " +
                       std::to_string(m_debugImgEvery) + " detection attempts"});
      }
      else
      {
         log<software_info>({__FILE__, __LINE__, "debug image publishing disabled"});
      }
   }

   updateIfChanged(m_indiP_debugEvery, "current", static_cast<unsigned>(m_debugImgEvery));
   return 0;
}


} //namespace app
} //namespace MagAOX

#endif //alignPSFMeas_hpp
