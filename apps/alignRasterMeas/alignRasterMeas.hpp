/** \file alignRasterMeas.hpp
  * \brief The MagAO-X photometric raster alignment header file
  *
  * Measures the intensity leaking past the coronagraph mask as a function of
  * commanded FSM offset, by stepping the FSM through a small raster grid and
  * summing the pixel intensity inside a fixed aperture centred on the mask.
  * A 2-D quadratic surface is fit to the samples and the FSM is parked at the
  * fitted minimum.
  *
  * This is the fine-alignment stage: it assumes coarse alignment has already
  * placed the PSF near the mask centre, and that nothing else is commanding the
  * FSM during the scan.
  * 
  * A scan is started and aborted explicitly via the INDI "scan" toggle - it
  * never starts a scan on its own.
  *
  * A start commanded while a scan is active or an abort is pending is
  * refused; commanding apps should verify that raster_status.state is IDLE,
  * DONE, or FAILED before starting a scan.
  *
  * The app is READY when idle and OPERATING only from a scan request until
  * the scan ends: the camera stream is opened on demand for the scan (the
  * shmimMonitor connects only while OPERATING) and dropped when idle, so
  * there is a short connect delay at the start of each scan.  If the camera
  * never connects the scan simply does not progress -- a commanding app
  * should time out on raster_status and abort.
  *
  * The FSM is commanded open-loop through a fsm control app via absolute values,
  * one command per grid point. Offsets are in FSM units, not pixels.
  * 
  * The mask-centre position is not measured here to avoid ownership duplication.
  * The app that owns this value (pixels, for the aperture) needs to be specified
  * in the config.
  *
  * \ingroup alignRasterMeas_files
  */

#ifndef alignRasterMeas_hpp
#define alignRasterMeas_hpp

#include <cmath>
#include <vector>
#include <limits>
#include <atomic>

#include <mx/improc/eigenImage.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

using namespace mx::improc;

/** \defgroup alignRasterMeas
  * \brief Photometric fine alignment via an FSM raster scan
  *
  * <a href="../handbook/operating/software/apps/alignRasterMeas.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup alignRasterMeas_files
  * \ingroup alignRasterMeas
  */

namespace MagAOX
{
namespace app
{

/// The MagAO-X photometric raster alignment app
/**
  * \ingroup alignRasterMeas
  */
class alignRasterMeas : public MagAOXApp<true>,
                        public dev::shmimMonitor<alignRasterMeas>
{

   //Give the test harness access.
   friend class alignRasterMeas_test;

   friend class dev::shmimMonitor<alignRasterMeas>;

   typedef dev::shmimMonitor<alignRasterMeas> shmimMonitorT;

protected:

   /** \name Configurable Parameters
     *@{
     */

   std::string m_fsmDevice{"fsm_sim"};      ///< INDI device name of the FSM controller.
   std::string m_fsmProp1{"val_1"};         ///< FSM property commanded for axis 1.
   std::string m_fsmProp2{"val_2"};         ///< FSM property commanded for axis 2.

   std::string m_measDevice{"alignPSFMeas"}; ///< Device publishing mask_center (single owner of the value).

   int m_halfSteps{3};        ///< Half-number of grid steps per axis; total scan = (2H+1)^2 points. Default 3.
   float m_stepSize{0.0f};    ///< Grid step size in FSM units (the units of the FSM val properties). Must be configured > 0.
   int m_settleFrames{10};    ///< Camera frames to discard after each FSM command. Default 10.
   int m_measureFrames{5};    ///< Camera frames to average per grid point. Default 5.
   std::atomic<float> m_centerRadius{20.0f}; ///< Radius (pixels) of the intensity aperture centred on the mask centre. Written by the INDI thread (center_radius), read by the shmim thread; atomic. Default 20.

   ///@}

   eigenImage<float> m_camImage; ///< Float buffer for the current camera frame.
   float (*m_camPixget)(void *, size_t) {nullptr}; ///< Pixel accessor for the camera shmim.

   // --- Debug scan-result maps (optional) ---
   std::atomic<bool> m_debugMap{false}; ///< When true, publish the measured and fitted intensity grids to debug shmims at scan end.  Set by config/INDI (main thread), read by the shmim thread.
   IMAGE m_dbgMapStream;                 ///< Debug shmim: (2H+1)x(2H+1) map of measured intensity vs grid offset.
   bool m_dbgMapCreated{false};          ///< True once the measured-map debug shmim has been created (shmim-thread only).
   IMAGE m_dbgFitStream;                 ///< Debug shmim: fitted quadratic surface on the same grid, with the found minimum marked.
   bool m_dbgFitCreated{false};          ///< True once the fitted-map debug shmim has been created (shmim-thread only).

   // Fitted-surface coefficients, kept for the debug fit map:
   //   I(x,y) = a x^2 + b y^2 + c xy + d x + e y + f   (x,y in FSM units).
   // Written by fitRaster, read by publishFitMap; shmim-thread only.
   float m_fitA{0}, m_fitB{0}, m_fitC{0}, m_fitD{0}, m_fitE{0}, m_fitF{0};
   bool m_fitCoeffsValid{false};        ///< True once fitRaster has produced coefficients this scan.

   uint64_t m_lastProcCnt0{std::numeric_limits<uint64_t>::max()}; ///< cnt0 of the last processed frame; skips backlogged wakeups re-delivering the same frame.

   // --- Subscribed state ---
   // These are written by the INDI SET callbacks and read only by the scan
   // toggle callback's precondition check -- all on the INDI thread, so no
   // cross-thread sharing. processImage (shmim thread) never touches them;
   // the values it needs are captured into the handoff snapshot below.
   float m_maskCenterX{-1.0f};      ///< Mask centre row (pixels), from alignPSFMeas.mask_center.
   float m_maskCenterY{-1.0f};      ///< Mask centre col (pixels), from alignPSFMeas.mask_center.
   bool m_maskCenterValid{false};   ///< True once mask_center has been received.

   float m_fsmCurrent1{0.0f};       ///< FSM axis-1 current value (FSM units).
   float m_fsmCurrent2{0.0f};       ///< FSM axis-2 current value (FSM units).
   bool m_fsmCurrentValid1{false};  ///< True once axis-1 current has been received.
   bool m_fsmCurrentValid2{false};  ///< True once axis-2 current has been received.

   std::atomic<bool> m_camConnected{false}; ///< True once the camera shmim has been allocated (shmim thread).  Not used for gating; retained for status/diagnostics.

   // --- Cross-thread scan request (INDI thread -> shmim thread) ---
   // The scan toggle callback does no scan-state mutation itself; it only
   // raises one of these request flags and lets processImage do all the
   // work.  The handoff floats below are guarded by m_startReq: the
   // requester writes them first and then release-stores the flag; the
   // shmim thread acquire-loads the flag, so observing it true guarantees
   // the snapshot writes are visible.  Atomics (not plain words) because
   // the deployment target includes weakly-ordered ARM (Jetson Orin),
   // where plain stores are not guaranteed visible in program order.
   // On x86 the release/acquire operations compile to plain moves.
   std::atomic<bool> m_startReq{false}; ///< Set by the toggle callback to request a scan start.  Guards the m_req* snapshot.
   std::atomic<bool> m_abortReq{false}; ///< Set by the toggle callback to request an abort.
   float m_reqBase1{0.0f};          ///< Handoff: FSM axis-1 base captured at request time.
   float m_reqBase2{0.0f};          ///< Handoff: FSM axis-2 base captured at request time.
   float m_reqMaskX{0.0f};          ///< Handoff: mask centre row captured at request time.
   float m_reqMaskY{0.0f};          ///< Handoff: mask centre col captured at request time.

   // --- Scan state machine (owned exclusively by the shmim thread) ---
   // Every field below is written only by processImage.  m_scanState is
   // atomic because it is also read by appLogic and the toggle callback
   // for status/gating; the rest are plain, single-thread fields.
   static constexpr int RASTER_IDLE = 0;      ///< No scan active.
   static constexpr int RASTER_SETTLING = 1;  ///< Waiting for the FSM/scene to settle after a command.
   static constexpr int RASTER_MEASURING = 2; ///< Accumulating intensity frames at the current point.
   static constexpr int RASTER_FITTING = 3;   ///< All points collected; fitting.
   static constexpr int RASTER_PARKING = 4;   ///< Optimum commanded; waiting for settle before declaring DONE.
   static constexpr int RASTER_DONE = 5;      ///< Scan complete; FSM parked at the optimum.  Quiet.
   static constexpr int RASTER_FAILED = 6;    ///< Scan failed; see log.  Quiet until a new scan is started.

   std::atomic<int> m_scanState{RASTER_IDLE}; ///< Current scan state.  Written by the shmim thread; read cross-thread for status/gating.
   int m_ix{0};                     ///< Axis-1 grid index of the current point (-H..H).
   int m_iy{0};                     ///< Axis-2 grid index of the current point.
   int m_frameCount{0};             ///< Frame counter within the current settle/measure/park phase.
   float m_accum{0.0f};             ///< Intensity accumulator for the current point.
   float m_hold1{0.0f};             ///< Current grid offset, axis 1 (FSM units, relative to base).
   float m_hold2{0.0f};             ///< Current grid offset, axis 2.
   float m_base1{0.0f};             ///< FSM axis-1 value at scan start (scan origin).
   float m_base2{0.0f};             ///< FSM axis-2 value at scan start.
   float m_scanMaskX{0.0f};         ///< Mask centre row snapshot for this scan (aperture is fixed during a scan).
   float m_scanMaskY{0.0f};         ///< Mask centre col snapshot for this scan.
   float m_min1{0.0f};              ///< Fitted optimum offset, axis 1 (FSM units).
   float m_min2{0.0f};              ///< Fitted optimum offset, axis 2.
   int m_fitQuality{0};             ///< 0 = no result, 1 = analytic fit, 2 = argmin fallback.

   struct RasterPoint { float x; float y; float intensity; }; ///< One sample: grid offsets (FSM units) + mean intensity.
   std::vector<RasterPoint> m_rasterData; ///< All samples of the current scan.

   // --- INDI ---
   pcf::IndiProperty m_indiP_scan;         ///< Toggle: start (On) / abort (Off) the scan.
   INDI_NEWCALLBACK_DECL(alignRasterMeas, m_indiP_scan);

   pcf::IndiProperty m_indiP_status;       ///< Read-only: state, n_measured, last_intensity.
   pcf::IndiProperty m_indiP_result;       ///< Read-only: offset_1/2, target_1/2, quality (final fit quality lives here, not in status).

   pcf::IndiProperty m_indiP_maskCenter;   ///< Subscription: alignPSFMeas.mask_center.
   INDI_SETCALLBACK_DECL(alignRasterMeas, m_indiP_maskCenter);

   pcf::IndiProperty m_indiP_fsmVal1;      ///< Subscription: FSM axis-1 property (current element).
   INDI_SETCALLBACK_DECL(alignRasterMeas, m_indiP_fsmVal1);

   pcf::IndiProperty m_indiP_fsmVal2;      ///< Subscription: FSM axis-2 property (current element).
   INDI_SETCALLBACK_DECL(alignRasterMeas, m_indiP_fsmVal2);

   pcf::IndiProperty m_indiP_debugMap;     ///< Toggle: publish the measured intensity map to a debug shmim at scan end.
   INDI_NEWCALLBACK_DECL(alignRasterMeas, m_indiP_debugMap);

   pcf::IndiProperty m_indiP_centerRadius; ///< Writable: intensity-aperture radius (pixels). Rejected while a scan is running.
   INDI_NEWCALLBACK_DECL(alignRasterMeas, m_indiP_centerRadius);

public:
   /// Default c'tor.
   alignRasterMeas();

   /// D'tor, declared and defined for noexcept.
   ~alignRasterMeas() noexcept
   {}

   virtual void setupConfig();

   /// Implementation of loadConfig logic, separated for testing.
   /** This is called by loadConfig().
     */
   int loadConfigImpl( mx::app::appConfigurator & _config /**< [in] an application configuration from which to load values*/);

   virtual void loadConfig();

   /// Startup function
   /**
     *
     */
   virtual int appStartup();

   /// Implementation of the FSM for alignRasterMeas.
   /**
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
   virtual int appLogic();

   /// Shutdown the app.
   /**
     *
     */
   virtual int appShutdown();

   /** \name shmimMonitor Interface
     * @{
     */
   int allocate( const dev::shmimT & );
   int processImage( void *curr_src, const dev::shmimT & );
   ///@}

protected:

   /// Validate preconditions and request a scan start (called on the INDI thread).
   /** Checks preconditions (reading only INDI-thread state), and on success
     * captures the FSM base + mask-centre handoff snapshot and raises
     * m_startReq for processImage to service.  Refuses loudly (software_error,
     * scan switch returned to Off/Alert) if any precondition is unmet.  Does
     * NOT mutate any scan-progression state -- that is the shmim thread's job.
     * \returns 0 if the start was requested, -1 if refused.
     */
   int requestScanStart();

   /// Begin a scan (called on the shmim thread from processImage).
   /** Consumes the handoff snapshot, initialises the scan grid, commands the
     * first point, and enters SETTLING.
     */
   void beginScan();

   /// Abort an active scan (called on the shmim thread from processImage).
   /** Restores the base position and returns to IDLE. */
   void doAbort();

   /// Command the FSM to absolute values (v1, v2) via INDI targets.
   int commandFSM( float v1, float v2 );

   /// Sum pixel values inside a circular aperture centred on (cx, cy).
   float measureCentralIntensity( const eigenImage<float> & im,
                                   float cx, float cy, float radius );

   /// Fit a 2-D quadratic surface to the raster data and find its minimum.
   /** Least-squares fit of I(x,y) = a x^2 + b y^2 + c xy + d x + e y + f;
     * requires a positive-definite Hessian for the analytic minimum
     * (m_fitQuality = 1), otherwise falls back to the argmin of the samples
     * (m_fitQuality = 2).  The result is clamped to the scanned range.
     * Sets m_min1/m_min2.
     * \returns 0 on success, -1 if there are too few points to fit.
     */
   int fitRaster();

   /// Create (on first use) and publish an image to a debug shmim named <configName><suffix>.
   /** \returns 0 on success, -1 if the stream could not be created.
     */
   int dbgPublish( IMAGE & stream, bool & created, const std::string & suffix,
                   const eigenImage<float> & img );

   /// Publish the measured intensity map (one cell per grid point) to the debug shmim.
   /** Builds a (2H+1)x(2H+1) image, cell (ix+H, iy+H) = mean intensity measured
     * at grid offset (ix, iy).  Lets the raster surface be inspected in a shmim
     * viewer -- a clean bowl vs. a saddle/exotic shape.  No-op if no data has
     * been collected.
     */
   void publishScanMap();

   /// Publish the fitted quadratic surface on the same grid, with the minimum marked.
   /** Evaluates the stored fit coefficients at each grid point and marks the
     * found minimum cell with a bright value.  Differencing against the
     * measured map reveals non-parabolic structure.  No-op if the fit did not
     * produce coefficients.
     */
   void publishFitMap();

   /// Publish raster_status.
   /** The internal SETTLING and MEASURING states flip once per grid point, far
     * faster than an INDI client refreshes; they are collapsed to a single
     * SCANNING code here.  n_measured is the meaningful progress indicator
     * during a scan.  Fit quality is not published here (it is meaningless until
     * the fit runs) -- see the 'result' property and the scan-complete log.
     */
   void publishStatus()
   {
      float pubState; // 0=IDLE 1=SCANNING 2=FITTING 3=PARKING 4=DONE 5=FAILED
      switch(m_scanState.load())
      {
         case RASTER_SETTLING:  // fall through: both are SCANNING to the outside
         case RASTER_MEASURING: pubState = 1.0f; break;
         case RASTER_FITTING:   pubState = 2.0f; break;
         case RASTER_PARKING:   pubState = 3.0f; break;
         case RASTER_DONE:      pubState = 4.0f; break;
         case RASTER_FAILED:    pubState = 5.0f; break;
         default:               pubState = 0.0f; break; // RASTER_IDLE
      }

      updateIfChanged(m_indiP_status,
         std::vector<std::string>({"state", "n_measured", "last_intensity"}),
         std::vector<float>({pubState,
                             static_cast<float>(m_rasterData.size()),
                             m_rasterData.empty() ? 0.0f : m_rasterData.back().intensity}));
   }
};

alignRasterMeas::alignRasterMeas() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   return;
}

void alignRasterMeas::setupConfig()
{
   SHMIMMONITOR_SETUP_CONFIG(config);

   config.add("fsm.device", "", "fsm.device", argType::Optional, "fsm", "device", false, "string", "INDI device name of the FSM controller. Default fsm_sim.");
   config.add("fsm.prop1", "", "fsm.prop1", argType::Optional, "fsm", "prop1", false, "string", "FSM property for axis 1. Default val_1.");
   config.add("fsm.prop2", "", "fsm.prop2", argType::Optional, "fsm", "prop2", false, "string", "FSM property for axis 2. Default val_2.");

   config.add("meas.device", "", "meas.device", argType::Optional, "meas", "device", false, "string", "Device publishing mask_center. Default alignPSFMeas.");

   config.add("raster.halfSteps", "", "raster.halfSteps", argType::Optional, "raster", "halfSteps", false, "int", "Half-number of grid steps per axis; total scan = (2H+1)^2 points. Default 3.");
   config.add("raster.stepSize", "", "raster.stepSize", argType::Required, "raster", "stepSize", false, "float", "Grid step size in FSM units. Required, must be > 0.");
   config.add("raster.settleFrames", "", "raster.settleFrames", argType::Optional, "raster", "settleFrames", false, "int", "Camera frames to discard after each FSM command. Default 10.");
   config.add("raster.measureFrames", "", "raster.measureFrames", argType::Optional, "raster", "measureFrames", false, "int", "Camera frames to average per grid point. Default 5.");
   config.add("raster.centerRadius", "", "raster.centerRadius", argType::Optional, "raster", "centerRadius", false, "float", "Radius (pixels) of the intensity aperture at the mask centre. Default 20.");
   config.add("raster.debugMap", "", "raster.debugMap", argType::Optional, "raster", "debugMap", false, "bool", "Publish the measured intensity grid to a debug shmim (<name>_dbg_map) at scan end. Also settable at runtime via the INDI 'debug_map' toggle. Default false.");
}

int alignRasterMeas::loadConfigImpl(mx::app::appConfigurator & _config)
{
   SHMIMMONITOR_LOAD_CONFIG(_config);

   _config(m_fsmDevice, "fsm.device");
   _config(m_fsmProp1, "fsm.prop1");
   _config(m_fsmProp2, "fsm.prop2");
   _config(m_measDevice, "meas.device");

   _config(m_halfSteps, "raster.halfSteps");
   _config(m_stepSize, "raster.stepSize");
   _config(m_settleFrames, "raster.settleFrames");
   _config(m_measureFrames, "raster.measureFrames");

   float centerRadius = m_centerRadius.load();
   _config(centerRadius, "raster.centerRadius");
   m_centerRadius.store(centerRadius);

   bool dbgMap = m_debugMap.load();
   _config(dbgMap, "raster.debugMap");
   m_debugMap.store(dbgMap);

   if(m_halfSteps < 1)
   {
      return log<software_error, -1>({__FILE__, __LINE__, "raster.halfSteps must be >= 1"});
   }

   if(m_stepSize <= 0.0f)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "raster.stepSize must be configured > 0 (FSM units)"});
   }

   if(m_settleFrames < 1 || m_measureFrames < 1)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "raster.settleFrames and raster.measureFrames must be >= 1"});
   }

   if(m_centerRadius.load() <= 0.0f)
   {
      return log<software_error, -1>({__FILE__, __LINE__, "raster.centerRadius must be > 0"});
   }

   return 0;
}

void alignRasterMeas::loadConfig()
{
   if(loadConfigImpl(config) < 0)
   {
      m_shutdown = true;
   }
}

int alignRasterMeas::appStartup()
{
   SHMIMMONITOR_APP_STARTUP;

   // scan control
   CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_scan, "scan");

   // status (read-only)
   registerIndiPropertyReadOnly(m_indiP_status, "raster_status",
      pcf::IndiProperty::Number,
      pcf::IndiProperty::ReadOnly,
      pcf::IndiProperty::Idle);
   m_indiP_status.add({"state",          0.0}); // 0=IDLE 1=SCANNING 2=FITTING 3=PARKING 4=DONE 5=FAILED
   m_indiP_status.add({"n_measured",     0.0}); // samples collected this scan
   m_indiP_status.add({"last_intensity", 0.0}); // most recent averaged intensity

   // result (read-only)
   registerIndiPropertyReadOnly(m_indiP_result, "result",
      pcf::IndiProperty::Number,
      pcf::IndiProperty::ReadOnly,
      pcf::IndiProperty::Idle);
   m_indiP_result.add({"offset_1", 0.0}); // optimum offset from scan base, FSM units
   m_indiP_result.add({"offset_2", 0.0});
   m_indiP_result.add({"target_1", 0.0}); // absolute FSM value commanded at park
   m_indiP_result.add({"target_2", 0.0});
   m_indiP_result.add({"quality",  0.0}); // as raster_status.quality

   // subscriptions: single-owner values consumed at runtime
   REG_INDI_SETPROP(m_indiP_maskCenter, m_measDevice, "mask_center");
   REG_INDI_SETPROP(m_indiP_fsmVal1, m_fsmDevice, m_fsmProp1);
   REG_INDI_SETPROP(m_indiP_fsmVal2, m_fsmDevice, m_fsmProp2);

   // intensity-aperture radius (writable)
   CREATE_REG_INDI_NEW_NUMBERF( m_indiP_centerRadius, "center_radius", 0, 1e6, 0, "%0.2f", "aperture radius (px)", "raster");
   m_indiP_centerRadius["current"].set(m_centerRadius.load());
   m_indiP_centerRadius["target"].set(m_centerRadius.load());

   // debug scan-result map toggle
   CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_debugMap, "debug_map");
   // Seed the config value with a direct element set, not updateSwitchIfChanged:
   // m_indiDriver is still null during appStartup, so the update helpers no-op
   // and the def would otherwise ship the default Off.
   m_indiP_debugMap["toggle"].setSwitchState(m_debugMap.load() ? pcf::IndiElement::On
                                                               : pcf::IndiElement::Off);

   state(stateCodes::READY);

   return 0;
}

int alignRasterMeas::appLogic()
{
   SHMIMMONITOR_APP_LOGIC;
   SHMIMMONITOR_UPDATE_INDI;

   // The shmimMonitor only opens the camera stream while the app FSM state is
   // OPERATING (shmimMonitor.hpp).  Hold OPERATING from the moment a scan is
   // requested until it finishes, so the camera comes up for the scan and is
   // dropped when idle.  m_startReq stays raised through the camera-connect
   // wait (processImage, which clears it, cannot run until the camera is
   // connected); m_scanState covers the running scan after beginScan.
   const int st = m_scanState.load();
   const bool engaged = m_startReq.load(std::memory_order_acquire) ||
                        (st >= RASTER_SETTLING && st <= RASTER_PARKING);

   state(engaged ? stateCodes::OPERATING : stateCodes::READY);

   return 0;
}

int alignRasterMeas::appShutdown()
{
   SHMIMMONITOR_APP_SHUTDOWN;

   return 0;
}

int alignRasterMeas::allocate(const dev::shmimT &)
{
   m_camImage.resize(shmimMonitorT::m_width, shmimMonitorT::m_height);

   m_camPixget = getPixPointer<float>(shmimMonitorT::m_dataType);

   if(m_camPixget == nullptr)
   {
      return log<software_error, -1>({__FILE__, __LINE__, "unsupported camera shmim data type"});
   }

   // New/reconnected stream: its write counter is unrelated to the old one.
   m_lastProcCnt0 = std::numeric_limits<uint64_t>::max();

   m_camConnected = true;

   return 0;
}

int alignRasterMeas::processImage(void *curr_src, const dev::shmimT &)
{
   // Process each distinct camera frame at most once: backlogged semaphore
   // wakeups re-deliver the newest frame and would corrupt the settle- and
   // measure-frame counting.
   const uint64_t cnt0 = shmimMonitorT::m_imageStream.md[0].cnt0;
   if(cnt0 == m_lastProcCnt0)
   {
      return 0;
   }
   m_lastProcCnt0 = cnt0;

   if(m_camPixget == nullptr) return 0;

   // Service cross-thread requests here, on the shmim thread, so that all
   // scan-progression state is mutated by this thread alone.  Abort wins if
   // both are somehow set.  The acquire load pairs with the requester's
   // release store: observing the flag true guarantees the handoff snapshot
   // writes are visible (required on weakly-ordered ARM; free on x86).
   if(m_abortReq.load(std::memory_order_acquire))
   {
      m_abortReq.store(false, std::memory_order_relaxed);
      m_startReq.store(false, std::memory_order_relaxed);
      doAbort();
   }
   if(m_startReq.load(std::memory_order_acquire))
   {
      // Clear the flag only after beginScan has raised m_scanState, so
      // appLogic's "engaged = m_startReq || m_scanState active" test never
      // sees both false during the hand-off and never blips the app to READY
      // mid-start (which would drop the camera connection).
      beginScan();
      m_startReq.store(false, std::memory_order_relaxed);
   }

   if(m_scanState == RASTER_IDLE || m_scanState == RASTER_DONE ||
      m_scanState == RASTER_FAILED)
   {
      return 0;
   }

   if(m_scanState == RASTER_SETTLING)
   {
      if(++m_frameCount >= m_settleFrames)
      {
         m_frameCount = 0;
         m_accum = 0.0f;
         m_scanState = RASTER_MEASURING;
      }
   }
   else if(m_scanState == RASTER_MEASURING)
   {
      // Copy the frame and measure the aperture intensity.
      const uint64_t npix = shmimMonitorT::m_width * shmimMonitorT::m_height;
      for(uint64_t nn = 0; nn < npix; ++nn)
         m_camImage.data()[nn] = m_camPixget(curr_src, nn);

      // Use the mask-centre snapshot frozen at scan start, not the live
      // (INDI-thread-owned) value: the aperture stays fixed for the scan and
      // this thread never reads cross-thread state.
      m_accum += measureCentralIntensity(m_camImage, m_scanMaskX, m_scanMaskY,
                                         m_centerRadius.load());

      if(++m_frameCount >= m_measureFrames)
      {
         m_rasterData.push_back({m_hold1, m_hold2, m_accum / m_measureFrames});

         if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
         {
            std::ostringstream oss;
            oss << "raster point (" << m_ix << "," << m_iy << ")"
                << " offset=(" << m_hold1 << "," << m_hold2 << ")"
                << " intensity=" << m_rasterData.back().intensity;
            log<software_debug>({__FILE__, __LINE__, oss.str()});
         }

         // Advance to the next point (axis 1 outer, axis 2 inner).
         if(++m_iy > m_halfSteps)
         {
            m_iy = -m_halfSteps;
            ++m_ix;
         }

         if(m_ix > m_halfSteps)
         {
            m_scanState = RASTER_FITTING;
         }
         else
         {
            m_hold1 = m_ix * m_stepSize;
            m_hold2 = m_iy * m_stepSize;
            m_frameCount = 0;
            m_scanState = RASTER_SETTLING;

            if(commandFSM(m_base1 + m_hold1, m_base2 + m_hold2) < 0)
            {
               log<software_error>({__FILE__, __LINE__, "FSM command failed; aborting scan"});
               m_scanState = RASTER_FAILED;
            }
         }
      }
   }

   if(m_scanState == RASTER_FITTING)
   {
      const bool dbg = m_debugMap.load(std::memory_order_relaxed);

      // Publish the measured surface before fitting, so it is available for
      // inspection even if the fit then fails.
      if(dbg) publishScanMap();

      int fitrv = fitRaster();

      // The fitted surface (with the found minimum marked) is available once
      // fitRaster has produced coefficients, on both the analytic and
      // argmin-fallback paths.
      if(dbg) publishFitMap();

      if(fitrv == 0)
      {
         // Park at the optimum: one absolute command through the FSM's
         // owner, so the position that persists into science lives in the
         // device's own state.
         if(commandFSM(m_base1 + m_min1, m_base2 + m_min2) == 0)
         {
            m_frameCount = 0;
            m_scanState = RASTER_PARKING;
         }
         else
         {
            log<software_error>({__FILE__, __LINE__, "FSM park command failed"});
            m_scanState = RASTER_FAILED;
         }
      }
      else
      {
         // Too few points: a programming error, not an environmental
         // condition -- the same scan would fail identically forever, so do
         // not retry.
         log<software_error>({__FILE__, __LINE__, "raster fit failed; scan FAILED"});
         m_scanState = RASTER_FAILED;
      }
   }
   else if(m_scanState == RASTER_PARKING)
   {
      if(++m_frameCount >= m_settleFrames)
      {
         m_scanState = RASTER_DONE;

         updateIfChanged(m_indiP_result,
            std::vector<std::string>({"offset_1", "offset_2", "target_1", "target_2", "quality"}),
            std::vector<float>({m_min1, m_min2,
                                m_base1 + m_min1, m_base2 + m_min2,
                                static_cast<float>(m_fitQuality)}));

         updateSwitchIfChanged(m_indiP_scan, "toggle", pcf::IndiElement::Off, INDI_OK);

         log<software_info>({__FILE__, __LINE__, "raster complete: optimum offset (" +
            std::to_string(m_min1) + ", " + std::to_string(m_min2) +
            ") FSM units from base; parked at (" +
            std::to_string(m_base1 + m_min1) + ", " + std::to_string(m_base2 + m_min2) +
            "), quality " + std::to_string(m_fitQuality)});
      }
   }

   // A failure anywhere above must not leave the scan switch stuck On/Busy.
   // updateSwitchIfChanged only sends on change, so this fires once.
   if(m_scanState == RASTER_FAILED)
   {
      updateSwitchIfChanged(m_indiP_scan, "toggle", pcf::IndiElement::Off, INDI_ALERT);
   }

   publishStatus();

   return 0;
}

int alignRasterMeas::requestScanStart()
{
   // Runs on the INDI thread.  It only validates and hands off; the actual
   // scan-state mutation happens in beginScan() on the shmim thread.

   if(m_scanState >= RASTER_SETTLING && m_scanState <= RASTER_PARKING)
   {
      log<software_warning>({__FILE__, __LINE__, "scan already active; start request ignored"});
      return 0;
   }

   // Check only the INDI inputs this app directly needs: the mask centre (aperture position)
   // and the FSM's current position (scan origin).
   std::string missing;
   if(!m_maskCenterValid) missing += "mask_center not received from " + m_measDevice + "; ";
   if(!m_fsmCurrentValid1 || !m_fsmCurrentValid2) missing += "FSM current values not received; ";

   if(!missing.empty())
   {
      log<software_error>({__FILE__, __LINE__, "scan refused: " + missing});
      updateSwitchIfChanged(m_indiP_scan, "toggle", pcf::IndiElement::Off, INDI_ALERT);
      return -1;
   }

   // Capture the handoff snapshot: the scan origin (from the FSM's published
   // state) and the aperture centre.  These are written before m_startReq so
   // the shmim thread sees a consistent snapshot when it observes the flag.
   m_reqBase1 = m_fsmCurrent1;
   m_reqBase2 = m_fsmCurrent2;
   m_reqMaskX = m_maskCenterX;
   m_reqMaskY = m_maskCenterY;

   // Release store: pairs with the acquire load in processImage so the
   // snapshot writes above are guaranteed visible when the flag is observed.
   m_startReq.store(true, std::memory_order_release);

   updateSwitchIfChanged(m_indiP_scan, "toggle", pcf::IndiElement::On, INDI_BUSY);

   return 0;
}

void alignRasterMeas::beginScan()
{
   // Runs on the shmim thread.  Consume the handoff snapshot and initialise
   // all scan-progression state (this thread is its sole writer).
   m_base1 = m_reqBase1;
   m_base2 = m_reqBase2;
   m_scanMaskX = m_reqMaskX;
   m_scanMaskY = m_reqMaskY;

   m_ix = -m_halfSteps;
   m_iy = -m_halfSteps;
   m_hold1 = m_ix * m_stepSize;
   m_hold2 = m_iy * m_stepSize;
   m_frameCount = 0;
   m_accum = 0.0f;
   m_fitQuality = 0;
   m_fitCoeffsValid = false;
   m_rasterData.clear();
   m_rasterData.reserve((2*m_halfSteps+1) * (2*m_halfSteps+1));

   if(commandFSM(m_base1 + m_hold1, m_base2 + m_hold2) < 0)
   {
      log<software_error>({__FILE__, __LINE__, "initial FSM command failed; scan FAILED"});
      m_scanState = RASTER_FAILED;
      updateSwitchIfChanged(m_indiP_scan, "toggle", pcf::IndiElement::Off, INDI_ALERT);
      publishStatus();
      return;
   }

   m_scanState = RASTER_SETTLING;

   log<software_info>({__FILE__, __LINE__, "raster scan started: " +
      std::to_string(2*m_halfSteps+1) + "x" + std::to_string(2*m_halfSteps+1) +
      " points, step " + std::to_string(m_stepSize) + " FSM units, base (" +
      std::to_string(m_base1) + ", " + std::to_string(m_base2) + ")"});

   publishStatus();
}

void alignRasterMeas::doAbort()
{
   // Runs on the shmim thread.
   if(m_scanState < RASTER_SETTLING || m_scanState > RASTER_PARKING)
   {
      return; // nothing active
   }

   // Restore the base position so an abort leaves the system where the scan
   // found it.  If the restore command fails the abort still completes --
   // the FSM is wherever the scan left it, and the error log says so.
   if(commandFSM(m_base1, m_base2) < 0)
   {
      log<software_error>({__FILE__, __LINE__,
         "failed to restore base position on abort; FSM left at last scan position"});
   }

   m_scanState = RASTER_IDLE;

   log<software_notice>({__FILE__, __LINE__, "raster scan aborted; base position restored"});

   publishStatus();
}

int alignRasterMeas::commandFSM(float v1, float v2)
{
   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      std::ostringstream oss;
      oss << "commanding FSM: " << m_fsmProp1 << "=" << v1
          << " " << m_fsmProp2 << "=" << v2;
      log<software_debug>({__FILE__, __LINE__, oss.str()});
   }

   pcf::IndiProperty ip1(pcf::IndiProperty::Number);
   ip1.setDevice(m_fsmDevice);
   ip1.setName(m_fsmProp1);
   ip1.add(pcf::IndiElement("target"));
   ip1["target"] = v1;
   if(sendNewProperty(ip1) < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "failed to send " + m_fsmDevice + "." + m_fsmProp1});
   }

   pcf::IndiProperty ip2(pcf::IndiProperty::Number);
   ip2.setDevice(m_fsmDevice);
   ip2.setName(m_fsmProp2);
   ip2.add(pcf::IndiElement("target"));
   ip2["target"] = v2;
   if(sendNewProperty(ip2) < 0)
   {
      return log<software_error, -1>({__FILE__, __LINE__,
         "failed to send " + m_fsmDevice + "." + m_fsmProp2});
   }

   return 0;
}

float alignRasterMeas::measureCentralIntensity( const eigenImage<float> & im,
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

int alignRasterMeas::fitRaster()
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
      // Fit in grid-index units (offset / stepSize), not raw FSM units.
      // The minimum is converted back to FSM units below.
      const float x = m_rasterData[i].x / m_stepSize;
      const float y = m_rasterData[i].y / m_stepSize;
      A(i,0) = x*x;  A(i,1) = y*y;  A(i,2) = x*y;
      A(i,3) = x;    A(i,4) = y;    A(i,5) = 1.0f;
      bvec(i) = m_rasterData[i].intensity;
   }

   // NB: assign to a concrete VectorXf, not auto.  A.colPivHouseholderQr()
   // is a temporary; .solve() returns a lazy expression referencing it, so
   // "auto sol" would dangle once the temporary is destroyed at the
   // semicolon and every sol(i) would read freed memory.  Assigning to a
   // VectorXf evaluates immediately while the decomposition is still alive.
   const Eigen::VectorXf sol = A.colPivHouseholderQr().solve(bvec);

   // Store the coefficients once (kept for the debug fit map; valid on both
   // the analytic and argmin-fallback paths).  The short const aliases below
   // just keep the minimum-finding math readable -- no separate storage.
   m_fitA = sol(0); m_fitB = sol(1); m_fitC = sol(2);
   m_fitD = sol(3); m_fitE = sol(4); m_fitF = sol(5);
   m_fitCoeffsValid = true;

   const float & a = m_fitA; const float & b = m_fitB; const float & c = m_fitC;
   const float & d = m_fitD; const float & e = m_fitE;

   // Minimum of the quadratic satisfies:
   //   [2a  c ] [x_min]   [-d]
   //   [c   2b] [y_min] = [-e]
   // Positive-definite Hessian requires: 2a > 0 AND det = 4ab - c^2 > 0.
   const float det = 4.0f*a*b - c*c;

   if(a <= 0.0f || det <= 0.0f)
   {
      // Surface has no interior minimum in this region (saddle or maximum).
      // Fall back to the raw data point with the lowest measured intensity.
      log<software_warning>({__FILE__,__LINE__,
         "fitRaster: Hessian not positive definite (a=" + std::to_string(a) +
         ", det=" + std::to_string(det) + "); using raw intensity minimum"});

      float minIntensity = std::numeric_limits<float>::max();
      m_min1 = 0.0f;
      m_min2 = 0.0f;
      for(auto & pt : m_rasterData)
      {
         if(pt.intensity < minIntensity)
         {
            minIntensity = pt.intensity;
            m_min1 = pt.x;
            m_min2 = pt.y;
         }
      }
      m_fitQuality = 2;
      return 0;
   }

   // Cramer's rule on the 2x2 system -- solved in grid-index units (the
   // coefficients were fit in those units), then clamped to the scanned
   // index range and converted back to FSM units.
   const float xMinIdx = (-d*2.0f*b + e*c) / det;
   const float yMinIdx = (-e*2.0f*a + d*c) / det;

   const float H = m_halfSteps;
   m_min1 = std::max(-H, std::min(H, xMinIdx)) * m_stepSize;
   m_min2 = std::max(-H, std::min(H, yMinIdx)) * m_stepSize;
   m_fitQuality = 1;

   if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
   {
      std::ostringstream oss;
      oss << "fitRaster: analytic minimum index (" << xMinIdx << ", " << yMinIdx << ")"
          << " -> FSM (" << m_min1 << ", " << m_min2 << ")";
      log<software_debug>({__FILE__, __LINE__, oss.str()});
   }

   return 0;
}

int alignRasterMeas::dbgPublish( IMAGE & stream, bool & created, const std::string & suffix,
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

void alignRasterMeas::publishScanMap()
{
   if(m_rasterData.empty()) return;

   const int N = 2 * m_halfSteps + 1;
   eigenImage<float> map(N, N);
   map.setZero();

   // Cell (ix+H, iy+H) = mean intensity at grid offset (ix, iy).  Indices are
   // recovered from the stored FSM-unit offsets.
   for(const auto & pt : m_rasterData)
   {
      const int i = (int)std::lround(pt.x / m_stepSize) + m_halfSteps;
      const int j = (int)std::lround(pt.y / m_stepSize) + m_halfSteps;
      if(i >= 0 && i < N && j >= 0 && j < N) map(i, j) = pt.intensity;
   }

   dbgPublish(m_dbgMapStream, m_dbgMapCreated, "_dbg_map", map);
}

void alignRasterMeas::publishFitMap()
{
   if(!m_fitCoeffsValid) return;

   const int N = 2 * m_halfSteps + 1;
   eigenImage<float> fit(N, N);

   // Evaluate I(x,y) = a x^2 + b y^2 + c xy + d x + e y + f at each grid point.
   // x,y are grid-index units (the units the coefficients were fit in), which
   // for cell (i,j) is exactly (i-H, j-H).
   float vmin = std::numeric_limits<float>::max();
   float vmax = std::numeric_limits<float>::lowest();
   for(int i = 0; i < N; ++i)
   {
      const float x = i - m_halfSteps;
      for(int j = 0; j < N; ++j)
      {
         const float y = j - m_halfSteps;
         const float v = m_fitA*x*x + m_fitB*y*y + m_fitC*x*y +
                         m_fitD*x + m_fitE*y + m_fitF;
         fit(i, j) = v;
         if(v < vmin) vmin = v;
         if(v > vmax) vmax = v;
      }
   }

   // Mark the found minimum with the brightest cell -- but only a little
   // above the surface max, scaled to the surface's own range, so it does
   // not blow out the colour scale (intensities can be ~1e-3, an absolute
   // bump would swamp the surface and hide its shape).  m_min1/m_min2 are
   // clamped to the grid, so the rounded indices are always in range.
   const float span = vmax - vmin;
   const int mi = (int)std::lround(m_min1 / m_stepSize) + m_halfSteps;
   const int mj = (int)std::lround(m_min2 / m_stepSize) + m_halfSteps;
   if(mi >= 0 && mi < N && mj >= 0 && mj < N && span > 0.0f)
   {
      fit(mi, mj) = vmax + 0.25f * span;
   }

   dbgPublish(m_dbgFitStream, m_dbgFitCreated, "_dbg_fit", fit);
}

INDI_NEWCALLBACK_DEFN(alignRasterMeas, m_indiP_scan)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_scan);

   if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
   {
      // Validate and hand off; processImage (shmim thread) does the work.
      requestScanStart();
      return 0;
   }

   if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::Off)
   {
      // Request an abort; processImage services it on the shmim thread.
      m_abortReq.store(true, std::memory_order_release);
      updateSwitchIfChanged(m_indiP_scan, "toggle", pcf::IndiElement::Off, INDI_IDLE);
      return 0;
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(alignRasterMeas, m_indiP_debugMap)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_debugMap);

   if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
   {
      m_debugMap.store(true);
      updateSwitchIfChanged(m_indiP_debugMap, "toggle", pcf::IndiElement::On, INDI_IDLE);
   }
   else if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::Off)
   {
      m_debugMap.store(false);
      updateSwitchIfChanged(m_indiP_debugMap, "toggle", pcf::IndiElement::Off, INDI_IDLE);
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(alignRasterMeas, m_indiP_centerRadius)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_centerRadius);

   float target;
   if( indiTargetUpdate(m_indiP_centerRadius, target, ipRecv, true) < 0 )
   {
      return log<software_error, -1>({__FILE__, __LINE__});
   }

   if(target <= 0.0f)
   {
      log<software_error>({__FILE__, __LINE__, "center_radius must be > 0; ignoring " +
                    std::to_string(target)});
      updateIfChanged(m_indiP_centerRadius, "current", m_centerRadius.load());
      return 0;
   }

   // Refuse to move the aperture out from under an in-progress scan: the surface
   // must be measured against a fixed aperture.  Change it when the scan is IDLE,
   // DONE, or FAILED; it takes effect on the next scan.
   const int st = m_scanState.load();
   if(st >= RASTER_SETTLING && st <= RASTER_PARKING)
   {
      log<software_error>({__FILE__, __LINE__,
                    "center_radius change refused while a scan is running"});
      updateIfChanged(m_indiP_centerRadius, "current", m_centerRadius.load());
      return 0;
   }

   if(target != m_centerRadius.load())
   {
      m_centerRadius.store(target);
      log<software_info>({__FILE__, __LINE__, "center_radius set to " +
                    std::to_string(target) + " px"});
   }

   updateIfChanged(m_indiP_centerRadius, "current", m_centerRadius.load());
   return 0;
}

INDI_SETCALLBACK_DEFN(alignRasterMeas, m_indiP_maskCenter)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_maskCenter);

   if(ipRecv.find("x") && ipRecv.find("y"))
   {
      m_maskCenterX = ipRecv["x"].get<float>();
      m_maskCenterY = ipRecv["y"].get<float>();

      if(!m_maskCenterValid)
      {
         log<software_info>({__FILE__, __LINE__, "mask_center received from " +
            m_measDevice + ": (" + std::to_string(m_maskCenterX) + ", " +
            std::to_string(m_maskCenterY) + ")"});
      }
      m_maskCenterValid = true;
   }

   return 0;
}

INDI_SETCALLBACK_DEFN(alignRasterMeas, m_indiP_fsmVal1)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_fsmVal1);

   if(ipRecv.find("current"))
   {
      m_fsmCurrent1 = ipRecv["current"].get<float>();
      m_fsmCurrentValid1 = true;
   }

   return 0;
}

INDI_SETCALLBACK_DEFN(alignRasterMeas, m_indiP_fsmVal2)(const pcf::IndiProperty &ipRecv)
{
   INDI_VALIDATE_CALLBACK_PROPS(ipRecv, m_indiP_fsmVal2);

   if(ipRecv.find("current"))
   {
      m_fsmCurrent2 = ipRecv["current"].get<float>();
      m_fsmCurrentValid2 = true;
   }

   return 0;
}

} //namespace app
} //namespace MagAOX

#endif //alignRasterMeas_hpp
