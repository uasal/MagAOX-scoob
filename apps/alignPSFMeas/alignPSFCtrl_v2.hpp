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
  *    displacement from their reference positions.
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
   int m_searchHalfWidth{10};       ///< Method 1: xcorr search box half-width (pixels). Default 10.

   float m_speckleSep{50.0f};      ///< Methods 1&2: 1st order speckle distance from PSF centre (pixels). Default 50.

   float m_speckleAngle{45.0f};    ///< Methods 1&2: angle of 1st order speckle from image row axis (degrees). Default 45.
   int m_speckleSearchR{8};        ///< Methods 1&2: local peak search radius around each speckle (pixels). Default 8.

   int m_minSpeckles{4};           ///< Methods 1&2: minimum speckles required per frame. Default 4.

   float m_predGain{0.1f};         ///< Methods 1&2: assumed loop gain for predicting next-frame PSF position. Default 0.1.

   float m_centroidRadius{6.0f};   ///< Radius (pixels) of the background-subtracted centroid window. Default 6.
   int m_maxPeakCands{24};         ///< First frame (Methods 1&2): max local-maxima candidates kept for the quad search. Default 24.
   float m_quadFluxRatio{3.0f};    ///< First frame (Methods 1&2): max raw-flux ratio between brightest and faintest speckle in a quad. Default 3. From tests, the first order speckles are about 8-10 times brighter than the second order, so this should not be set higher than 4-5.
   float m_coreDomFactor{25.0f};   ///< First frame (Methods 1&2) fallback: raw-flux factor by which one candidate must dominate all others to be identified as the unsuppressed core. Default 25. From tests, the unsuppressed core tends to be ~60 times brighter than the first order speckles.
   float m_trackGeomTol{10.0f};    ///< Tracking: max deviation (pixels) of any speckle's implied PSF centre from the consensus before it is dropped or the frame rejected. <=0 disables. Default 10.
   float m_trackFluxRatio{10.0f};  ///< Tracking: max raw-flux ratio between brightest and faintest tracked speckle. Faint outliers are dropped down to minSpeckles; a bright outlier rejects the frame. <=0 disables. Default 10.
   int m_trackFailLimit{10};       ///< Tracking: consecutive rejected frames before the PSF is re-bootstrapped. Default 10.

   /// Where the current mask centre came from, in ascending priority (config wins).
   enum class maskCenterSource { imageCentre, reference, config };
   maskCenterSource m_maskCenterSource{maskCenterSource::imageCentre};

   float m_maskCenterX{-1.0f};     ///< Coronagraph mask centre row (pixels).
   float m_maskCenterY{-1.0f};     ///< Coronagraph mask centre col (pixels).

   int m_rasterHalfSteps{3};          ///< Method 3: half-number of steps per axis; total scan = (2H+1)^2. Default 3.
   float m_rasterStepSize{2.0f};      ///< Method 3: pixel step size between raster points. Default 2.
   int m_rasterSettleFrames{10};      ///< Method 3: camera frames to discard after each FSM step. Default 10.
   int m_rasterMeasureFrames{5};      ///< Method 3: camera frames to average per raster point. Default 5.
   float m_rasterCenterRadius{20.0f}; ///< Method 3: radius (pixels) of central aperture for intensity sum. Default 20.

   ///@}

   mx::improc::imageXCorrFFT<eigenImage<float>> m_xcorr; ///< The cross-correlator.

   eigenImage<float> m_camImage; ///< Float buffer for the current camera frame.
   eigenImage<float> m_refImage; ///< Float buffer for the reference frame.

   float (*m_camPixget)(void *, size_t) {nullptr}; ///< Pixel accessor for the camera shmim.
   float (*m_refPixget)(void *, size_t) {nullptr}; ///< Pixel accessor for the reference shmim.

   bool m_refImLoaded{false};    ///< True once a reference frame has been received and set.
   int64_t m_counter{0};         ///< Monotonically increasing frame counter published to INDI. 
                                 /// This will only work for 1,000,000 frames.
                                 /// At counter = 1,000,000 the wire text becomes "1e+06"; closedLoopIndi's get<int>() parses that as 1,
                                 /// and consecutive counters serialize identically so updateIfChanged will stop sending at all. 
   bool m_loopClosed{false};     ///< Whether the loop is closed.

   uint64_t m_lastProcCnt0{std::numeric_limits<uint64_t>::max()}; ///< cnt0 of the last camera frame processed; used to skip backlogged semaphore wakeups that re-deliver an already-processed frame.
   uint64_t m_backlogSkips{0};   ///< Count of backlogged wakeups skipped (diagnostic).

   float m_estShiftX{0.0f};           ///< Predicted PSF shift in X for the next frame (pixels).
   float m_estShiftY{0.0f};           ///< Predicted PSF shift in Y for the next frame (pixels).

   // --- PSF tracking state (Methods 1 & 2) ---
   bool m_initialized{false};       ///< True after first-frame PSF detection.
   int m_bootstrapFailCount{0};     ///< Consecutive frames on which bootstrap detection has failed.
   int m_trackFailCount{0};         ///< Consecutive tracking frames rejected by validation.

   // --- Debug image products ---
   int m_debugImgEvery{0};    ///< Publish debug shmims on the 1st and every Nth detection attempt. 0 = off.
   uint64_t m_dbgAttempts{0}; ///< Detection attempts since enable (decimation counter for debug products).

   IMAGE m_dbgSmoothStream;         ///< Debug shmim: smoothed detection image (Step 1 output).
   bool m_dbgSmoothCreated{false};  ///< True once the smooth debug shmim has been created.
   IMAGE m_dbgCandsStream;          ///< Debug shmim: smoothed image with NMS candidates marked.
   bool m_dbgCandsCreated{false};   ///< True once the candidates debug shmim has been created.
   IMAGE m_dbgQuadStream;           ///< Debug shmim: original image with found speckles + PSF centre marked.
   bool m_dbgQuadCreated{false};    ///< True once the quad debug shmim has been created.

   // --- Raster scan state (Method 3) ---
   static constexpr int RASTER_IDLE = 0;      ///< Scan not started.
   static constexpr int RASTER_SETTLING = 1;  ///< Waiting for FSM to settle after each step.
   static constexpr int RASTER_MEASURING = 2; ///< Accumulating intensity frames at current position.
   static constexpr int RASTER_FITTING = 3;   ///< All points collected; running 2-D quadratic fit.
   static constexpr int RASTER_DONE = 4;      ///< Fit complete; holding at found minimum.

   int m_rasterState{RASTER_IDLE};     ///< Current raster state machine state.
   int m_rasterIx{0};                  ///< X-step index for the next position (−halfSteps … +halfSteps).
   int m_rasterIy{0};                  ///< Y-step index for the next position.
   int m_rasterFrameCount{0};          ///< Frame counter within current settle/measure phase.
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

   pcf::IndiProperty m_indiP_debugEvery;   ///< Writable: publish debug image products every N detection attempts (0 = off).
   INDI_NEWCALLBACK_DECL(alignPSFCtrl, m_indiP_debugEvery);

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

   /// Find the local intensity peak nearest to (eX, eY) within a square half-width searchR.
   /** The peak is refined to sub-pixel accuracy with a background-subtracted
     * windowed centroid (windowedCentroid); if that fails the raw peak pixel
     * is returned.
     * No validation is performed: the brightest pixel of the (frame-clipped)
     * search box is always returned, so the caller must judge whether the
     * result is trustworthy.
     * \returns true if any part of the search box intersects the image.
     * \returns false if the search box lies entirely outside the image.
     */
   bool findLocalPeak( const eigenImage<float> & im,
                       float eX, float eY, int searchR,
                       float & foundX, float & foundY );

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
   config.add("align.searchHalfWidth", "", "align.searchHalfWidth", argType::Optional, "align", "searchHalfWidth", false, "int", "Method 1: xcorr search box half-width in pixels. Default 10.");
   config.add("align.speckleSep", "", "align.speckleSep", argType::Optional, "align", "speckleSep", false, "float", "Methods 1&2: speckle distance from PSF centre in pixels. Default 50.");
   config.add("align.speckleAngle", "", "align.speckleAngle", argType::Optional, "align", "speckleAngle", false, "float", "Methods 1&2: angle of first speckle from row axis in degrees. Default 45.");
   config.add("align.speckleSearchR", "", "align.speckleSearchR", argType::Optional, "align", "speckleSearchR", false, "int", "Methods 1&2: search radius around each expected speckle position. Default 8.");
   config.add("align.minSpeckles", "", "align.minSpeckles", argType::Optional, "align", "minSpeckles",  false, "int", "Methods 1&2: minimum speckles required per frame. Default 4.");
   config.add("align.predGain", "", "align.predGain", argType::Optional, "align", "predGain",    false, "float", "Methods 1&2: assumed loop gain for next-frame prediction. 0=no correction, 1=full correction. Default 0.1.");
   config.add("align.centroidRadius", "", "align.centroidRadius", argType::Optional, "align", "centroidRadius", false, "float", "Radius in pixels of the background-subtracted window used to centroid speckles. Default 6.");
   config.add("align.maxPeakCands", "", "align.maxPeakCands", argType::Optional, "align", "maxPeakCands", false, "int", "Bootstrap: maximum number of local-maxima candidates for the speckle quad search. Default 24.");
   config.add("align.quadFluxRatio", "", "align.quadFluxRatio", argType::Optional, "align", "quadFluxRatio", false, "float", "Bootstrap: maximum raw-flux ratio between brightest and faintest speckle in a quad. Default 3.");
   config.add("align.coreDomFactor", "", "align.coreDomFactor", argType::Optional, "align", "coreDomFactor", false, "float", "Bootstrap fallback: raw-flux dominance factor for identifying the unsuppressed core when no speckle quad exists. Default 25.");
   config.add("align.trackGeomTol", "", "align.trackGeomTol", argType::Optional, "align", "trackGeomTol", false, "float", "Tracking: max deviation in pixels of a speckle's implied PSF centre from the consensus before it is dropped (down to minSpeckles) or the frame rejected. <=0 disables. Default 10.");
   config.add("align.trackFluxRatio", "", "align.trackFluxRatio", argType::Optional, "align", "trackFluxRatio", false, "float", "Tracking: max raw-flux ratio between brightest and faintest tracked speckle. Faint outliers (occulted/clipped) are dropped down to minSpeckles; a bright outlier (imposter capture) rejects the frame. <=0 disables. Default 10.");
   config.add("align.trackFailLimit", "", "align.trackFailLimit", argType::Optional, "align", "trackFailLimit", false, "int", "Tracking: consecutive rejected frames before the PSF is re-bootstrapped with the full validated detector. Default 10.");
   config.add("align.debugImgEvery", "", "align.debugImgEvery", argType::Optional, "align", "debugImgEvery", false, "int", "Publish debug image shmims (smoothed, candidates, found quad) on the first and every Nth detection attempt. 0 (default) disables.");
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
   _config(m_centroidRadius, "align.centroidRadius");
   _config(m_maxPeakCands, "align.maxPeakCands");
   _config(m_quadFluxRatio, "align.quadFluxRatio");
   _config(m_coreDomFactor, "align.coreDomFactor");
   _config(m_trackGeomTol, "align.trackGeomTol");
   _config(m_trackFluxRatio, "align.trackFluxRatio");
   _config(m_trackFailLimit, "align.trackFailLimit");
   if(m_trackFailLimit < 1) m_trackFailLimit = 1;
   _config(m_debugImgEvery, "align.debugImgEvery");
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

   // debug image products: publish every N detection attempts (0 = off)
   CREATE_REG_INDI_NEW_NUMBERU( m_indiP_debugEvery, "debug_imgs", 0, 1000000, 1, "%u", "debug images every", "alignment");
   m_indiP_debugEvery["current"].set(static_cast<unsigned>(m_debugImgEvery));

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

   // New/reconnected stream: its write counter is unrelated to the old one.
   m_lastProcCnt0 = std::numeric_limits<uint64_t>::max();

   return 0;
}

int alignPSFCtrl::processImage(void *curr_src, const dev::shmimT &)
{
   // Skip backlogged semaphore wakeups. shmimMonitor hands every wakeup a
   // pointer to the NEWEST frame slot, so when a slow frame (e.g. the ~40 ms
   // bootstrap detection) lets camera posts queue up, successive unchanged
   // frames are delivered within milliseconds. Measuring each of them
   // publishes a burst of identical stale measurements, each producing a fsm
   // command and lurching the PSF far enough to lose track of expected location.
   // Process each distinct camera frame (identified by the stream's write 
   // counter) at most once.
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
      // 0 = speckle quad found; 1 = dominant-core fallback (all four entries
      // hold the core position, so the mean-of-4 below works for both paths).
      int ffs = findFirstOrderSpeckles(m_camImage, camSpeckleX, camSpeckleY);
      if(ffs >= 0)
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

         m_estShiftX = sumX / 4.0f - m_maskCenterX;
         m_estShiftY = sumY / 4.0f - m_maskCenterY;

         log<software_info>({__FILE__, __LINE__, "Bootstrap PSF shift: (" +
                       std::to_string(m_estShiftX) + ", " + std::to_string(m_estShiftY) +
                       (ffs == 0 ? ") via speckle quad" : ") via dominant core")});

         m_initialized = true;

         if(m_bootstrapFailCount > 0)
         {
            // The Alert state on the measurement property clears automatically:
            // the measurement publish below sends with the default Ok state.
            log<text_log>("bootstrap succeeded after " + std::to_string(m_bootstrapFailCount) +
                          " failed frames", logPrio::LOG_NOTICE);
            m_bootstrapFailCount = 0;
         }
      }
      else
      {
         // Skip this frame's measurement entirely (the loop holds) and retry 
         // on the next frame. Don't spam the log with repeated failures - warn 
         // once and put the measurement property into the INDI Alert state 
         // (red in GUIs) until detection succeeds.
         if(m_bootstrapFailCount == 0)
         {
            log<software_warning>({__FILE__, __LINE__,
               "bootstrap speckle detection failed; holding measurements and retrying (measurement set to Alert)"});

            updateIfChanged(m_indiP_measurement,
               std::vector<std::string>({"delta_x", "delta_y", "counter"}),
               std::vector<float>({m_indiP_measurement["delta_x"].get<float>(),
                                   m_indiP_measurement["delta_y"].get<float>(),
                                   m_indiP_measurement["counter"].get<float>()}),
               INDI_ALERT);
         }
         ++m_bootstrapFailCount;
         return 0;
      }
   }

   float xs = 0.0f, ys = 0.0f;

   // --------------------------
   // Method 2: speckle centroid
   // --------------------------
   if(m_method == 2)
   {
      float psfCX = m_maskCenterX + m_estShiftX;
      float psfCY = m_maskCenterY + m_estShiftY;
      float angRad = m_speckleAngle * static_cast<float>(M_PI) / 180.0f;

      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
         std::ostringstream oss;
         oss << "Estimated PSF location: " << psfCX << ", " << psfCY;
         log<software_debug>({__FILE__, __LINE__, oss.str()});
      }

      // Each speckle sits at a known offset from the PSF core, so each found
      // speckle implies a core position: speckle_n - offset_n.
      // The measurement is the mean of these implied positions.
      // Validation:
      // (1) all implied positions must agree (a window that grabbed junk
      // implies a position far from the others), and 
      // (2) the speckles' raw fluxes must be similar (a window that grabbed 
      // the core or a bright harmonic is ~10-60x brighter than a true speckle).
      struct TrackedSpeckle
      {
         float impX, impY; // implied PSF centre
         float rawFlux;    // raw window flux; -1 if the window is clipped by the frame edge
         int n;            // speckle index (for logging)
      };
      std::vector<TrackedSpeckle> spk;

      const float fluxR = 2.0f * m_centroidRadius;

      for(int n = 0; n < 4; ++n)
      {
         float a = angRad + n * (static_cast<float>(M_PI) / 2.0f);
         float offX = m_speckleSep * std::cos(a);
         float offY = m_speckleSep * std::sin(a);
         float eX = psfCX + offX;
         float eY = psfCY + offY;

         if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
         {
            std::ostringstream oss;
            oss << "Estimated speckle " << n << " location: " << eX << ", " << eY;
            log<software_debug>({__FILE__, __LINE__, oss.str()});
         }

         float fX, fY;
         if(findLocalPeak(m_camImage, eX, eY, m_speckleSearchR, fX, fY))
         {
            // Flux is only comparable when the whole window is in-frame: a
            // clipped window under-measures and would falsely inflate the
            // ratio (e.g. a true speckle in a frame corner at large offset).
            float flux = -1.0f;
            if(fX - fluxR >= 0.0f && fX + fluxR <= (float)(m_camImage.rows() - 1) &&
               fY - fluxR >= 0.0f && fY + fluxR <= (float)(m_camImage.cols() - 1))
            {
               flux = rawWindowFlux(m_camImage, fX, fY, fluxR);
            }

            spk.push_back({fX - offX, fY - offY, flux, n});

            if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
            {
               std::ostringstream oss;
               oss << "Found speckle " << n << " location: " << fX << ", " << fY;
               log<software_debug>({__FILE__, __LINE__, oss.str()});
            }
         }
      }

      std::string reject;

      // --- Validation stage 0: enough speckles found? ---
      if((int)spk.size() < m_minSpeckles)
      {
         reject = "only " + std::to_string(spk.size()) + " of " +
                  std::to_string(m_minSpeckles) + " required speckles found";
      }

      // --- Validation stage 1: geometric consensus of implied centres ---
      // Iteratively drop the worst outlier (down to minSpeckles) until the
      // remaining implied centres agree: a speckle whose window captured junk
      // (occulted by the mask, clipped, halo structure) is discarded.  If
      // agreement cannot be reached the frame is rejected.
      if(reject.empty() && m_trackGeomTol > 0 && spk.size() >= 2)
      {
         while(true)
         {
            float mX = 0.0f, mY = 0.0f;
            for(size_t i = 0; i < spk.size(); ++i) { mX += spk[i].impX; mY += spk[i].impY; }
            mX /= spk.size();
            mY /= spk.size();

            size_t iw = 0;
            float dw = -1.0f;
            for(size_t i = 0; i < spk.size(); ++i)
            {
               const float d = std::sqrt(std::pow(spk[i].impX - mX, 2) +
                                         std::pow(spk[i].impY - mY, 2));
               if(d > dw) { dw = d; iw = i; }
            }

            if(dw <= m_trackGeomTol) break;

            if((int)spk.size() - 1 < m_minSpeckles)
            {
               reject = "speckle geometry inconsistent (worst implied-centre deviation " +
                        std::to_string(dw) + " px)";
               break;
            }

            if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
            {
               log<software_debug>({__FILE__, __LINE__,
                  "dropping speckle " + std::to_string(spk[iw].n) +
                  ": implied centre off consensus by " + std::to_string(dw) + " px"});
            }
            spk.erase(spk.begin() + iw);
         }
      }

      // --- Validation stage 2: raw-flux symmetry ---
      // Iteratively drop anomalously faint members (occulted or extinguished)
      // down to minSpeckles. An anomalously bright member is not
      // reconciled this way (the maximum is not dropped) so a window
      // that captured the PSF core or a bright harmonic (the lattice-trap
      // signature) always rejects the frame.
      if(reject.empty() && m_trackFluxRatio > 0)
      {
         while(true)
         {
            float fmin = -1.0f, fmax = -1.0f;
            size_t imin = 0;
            int nVal = 0;
            for(size_t i = 0; i < spk.size(); ++i)
            {
               if(spk[i].rawFlux < 0) continue; // clipped window: not comparable
               ++nVal;
               if(fmin < 0 || spk[i].rawFlux < fmin) { fmin = spk[i].rawFlux; imin = i; }
               if(spk[i].rawFlux > fmax) fmax = spk[i].rawFlux;
            }

            if(nVal < 2) break;                          // nothing to compare
            if(fmax <= m_trackFluxRatio * fmin) break;   // symmetric enough

            if((int)spk.size() - 1 < m_minSpeckles)
            {
               reject = "speckle fluxes asymmetric (ratio " +
                        std::to_string(fmax / fmin) + ")";
               break;
            }

            if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
            {
               log<software_debug>({__FILE__, __LINE__,
                  "dropping faint speckle " + std::to_string(spk[imin].n) +
                  ": flux ratio " + std::to_string(fmax / fmin)});
            }
            spk.erase(spk.begin() + imin);
         }
      }

      // --- Verdict ---
      if(!reject.empty())
      {
         ++m_trackFailCount;

         if(m_trackFailCount == 1)
         {
            log<software_warning>({__FILE__, __LINE__,
               "tracking validation rejected frame: " + reject + "; holding measurements"});
         }
         else if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
         {
            log<software_debug>({__FILE__, __LINE__,
               "tracking validation rejected frame (" + std::to_string(m_trackFailCount) +
               " consecutive): " + reject});
         }

         if(m_trackFailCount >= m_trackFailLimit)
         {
            // The loop has published nothing while rejecting, so unless
            // something external is moving the FSM the scene is static.
            // Re-run the full bootstrap detector on the next frame.
            log<text_log>("tracking validation failed " + std::to_string(m_trackFailCount) +
                          " consecutive frames (last: " + reject + "); re-bootstrapping",
                          logPrio::LOG_NOTICE);
            m_initialized = false;
            m_trackFailCount = 0;
         }

         return 0;
      }

      if(m_trackFailCount > 0)
      {
         log<text_log>("tracking validation recovered after " +
                       std::to_string(m_trackFailCount) + " rejected frames",
                       logPrio::LOG_NOTICE);
         m_trackFailCount = 0;
      }

      float mX = 0.0f, mY = 0.0f;
      for(size_t i = 0; i < spk.size(); ++i) { mX += spk[i].impX; mY += spk[i].impY; }
      mX /= spk.size();
      mY /= spk.size();

      xs = mX - m_maskCenterX;
      ys = mY - m_maskCenterY;

      m_estShiftX = xs * (1.0f - m_predGain);
      m_estShiftY = ys * (1.0f - m_predGain);
   }
   // ---------------------------------------------------------
   // Method 3: intensity-minimising raster scan
   // ---------------------------------------------------------
   else if(m_method == 3)
   {
      if(m_rasterState == RASTER_IDLE)
      {
         m_rasterState = RASTER_SETTLING;
         m_rasterIx = -m_rasterHalfSteps;
         m_rasterIy = -m_rasterHalfSteps;
         m_rasterHoldX = m_rasterIx * m_rasterStepSize;
         m_rasterHoldY = m_rasterIy * m_rasterStepSize;
         m_rasterFrameCount = 0;
         m_rasterAccum = 0.0f;
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
            m_rasterAccum = 0.0f;
            m_rasterState = RASTER_MEASURING;
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
               m_rasterHoldX = m_rasterIx * m_rasterStepSize;
               m_rasterHoldY = m_rasterIy * m_rasterStepSize;
               m_rasterFrameCount = 0;
               m_rasterState = RASTER_SETTLING;
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
      m_xcorr.searchBox(m_estShiftX, m_estShiftY, m_searchHalfWidth);

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

      m_estShiftX = xs * (1.0f - m_predGain);
      m_estShiftY = ys * (1.0f - m_predGain);
   }

   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      log<software_debug>({__FILE__, __LINE__,
         "shift=(" + std::to_string(xs) + "," + std::to_string(ys) + ")"
         " method=" + std::to_string(m_method)});
   }

   // The counter is published through a float element whose INDI text
   // serialization keeps ~6 significant digits: beyond 1e6, consecutive
   // values serialize identically ("1e+06") and consumers stop seeing
   // changes. Wrap at 1e6 - consumers (closedLoopIndi) act on counter
   // changes only, so the wrap is harmless but doesn't overflow the range.
   ++m_counter;
   updateIfChanged(m_indiP_measurement,
      std::vector<std::string>({"delta_x", "delta_y", "counter"}),
      std::vector<float>({xs, ys, static_cast<float>(m_counter % 1000000)}));

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
   m_estShiftX = 0.0f;
   m_estShiftY = 0.0f;

   m_refImLoaded = true;
   log<text_log>("Reference image updated (method=" + std::to_string(m_method) + ")");

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
         log<text_log>("Mask centre extrapolated from reference image: (" +
                       std::to_string(m_maskCenterX) + ", " + std::to_string(m_maskCenterY) + ")");
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

   return 0;
}


// ============================================================
// PSF detection helpers
// ============================================================

bool alignPSFCtrl::windowedCentroid( const eigenImage<float> & im,
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

float alignPSFCtrl::rawWindowFlux( const eigenImage<float> & im, float x0, float y0, float r )
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

   // Refine to sub-pixel accuracy with a background-subtracted windowed
   // centroid.  Fall back to the raw peak pixel if the window contains no
   // flux above the local background.
   float cx, cy, flux;
   if(windowedCentroid(im, (float)maxR, (float)maxC,
                       m_centroidRadius, m_centroidRadius + 2.0f, m_centroidRadius + 6.0f,
                       cx, cy, flux))
   {
      foundX = cx;
      foundY = cy;
   }
   else
   {
      foundX = (float)maxR;
      foundY = (float)maxC;
   }

   return true;
}

void alignPSFCtrl::drawCross( eigenImage<float> & img, float x, float y, int half, float val )
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

int alignPSFCtrl::dbgPublish( IMAGE & stream, bool & created, const std::string & suffix,
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
      log<text_log>("created debug shmim " + configName() + suffix);
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

int alignPSFCtrl::findFirstOrderSpeckles( const eigenImage<float> & im,
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
   // BEFORE the size/geometry checks so failed attempts can be inspected too.
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
   const float expectedDiag = 2.0f * m_speckleSep;
   const float distTol = m_speckleSep * 0.3f;
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

   float target;
   if( indiTargetUpdate(m_indiP_method, target, ipRecv, true) < 0 )
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }

   unsigned newMethod = static_cast<unsigned>(target + 0.5f);
   if(newMethod < 1 || newMethod > 3)
   {
      updateIfChanged(m_indiP_method, "current", static_cast<unsigned>(m_method));
      return 0;
   }

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
         m_estShiftX     = 0.0f;
         m_estShiftY     = 0.0f;
      }

      log<text_log>("Method changed to " + std::to_string(m_method));
   }

   updateIfChanged(m_indiP_method, "current", static_cast<unsigned>(m_method));
   return 0;
}

INDI_NEWCALLBACK_DEFN(alignPSFCtrl, m_indiP_debugEvery)(const pcf::IndiProperty &ipRecv)
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
         log<text_log>("debug image publishing enabled: every " +
                       std::to_string(m_debugImgEvery) + " detection attempts");
      }
      else
      {
         log<text_log>("debug image publishing disabled");
      }
   }

   updateIfChanged(m_indiP_debugEvery, "current", static_cast<unsigned>(m_debugImgEvery));
   return 0;
}

} //namespace app
} //namespace MagAOX

#endif //alignPSFCtrl_hpp
