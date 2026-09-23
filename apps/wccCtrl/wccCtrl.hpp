/** \file wccCtrl.hpp
 * \brief The MagAO-X WCC high level acquisition and guiding controller.
 * \author Adam Schilperoort
 *
 * \ingroup wccCtrl_files
 */

#ifndef wccCtrl_hpp
#define wccCtrl_hpp

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ImageStreamIO/ImageStreamIO.h>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include "../wccCommon/wccAstrometry.hpp"
#include "../wccCommon/wccFocalPlane.hpp"
#include "../wccCommon/wccIndiRate.hpp"
#include "../wccCommon/wccNumeric.hpp"
#include "../wccCommon/wccSensorConfig.hpp"
#include "../wccCommon/wccSkyWCS.hpp"
#include "../wccCommon/wccStarCatalog.hpp"
#include "../wccCommon/wccVisit.hpp"
#include "../wccCommon/wccVisitIndi.hpp"

/** \defgroup wccCtrl WCC Acquisition Controller
 * \brief High level logic controller for WCC target acquisition and guiding.
 *
 * wccCtrl runs the acquisition sequence for a visit:
 *
 * 1. ingest a visit file, selecting the guide star, the roll star, the sensors to
 *    solve on and the per camera configuration
 * 2. configure every acquisition sensor's region of interest, frame rate and
 *    exposure time, enable streaming, and confirm the cameras report back what
 *    was asked for
 * 3. collect one frame from every acquisition sensor and solve astrometry on each
 *    against the star catalog, then combine the per sensor solutions into a
 *    single boresight and roll correction
 * 4. offset the telescope and wait for it to report the move complete
 * 5. re-acquire and confirm the guide star landed within tolerance of its target
 *    pixel; check the roll star too and issue a roll maneuver if needed
 * 6. reconfigure the guide and roll cameras from full frame down to a small
 *    region of interest centered on each target pixel, and raise their frame rate
 * 7. hand off to the centroid controllers and close a fast pointing and roll loop
 *    on the centroids they publish
 *
 * The telescope and centroid interfaces are entirely configurable, so the same
 * application drives `tcsInterface`, a simulated telescope, or wccSim acting as
 * its own pointing authority.
 *
 * <a href="../handbook/operating/software/apps/wccCtrl.html">Application Documentation</a>
 *
 * \ingroup apps
 */

/** \defgroup wccCtrl_files WCC Acquisition Controller Files
 * \ingroup wccCtrl
 */

namespace MagAOX
{
namespace app
{

/// The stages of the WCC acquisition sequence.
/** \ingroup wccCtrl
 */
enum class wccAcqState
{
    idle,          ///< No visit loaded.
    loaded,        ///< A visit is loaded and validated, waiting for a start request.
    configuring,   ///< Commanding region of interest, frame rate, exposure time and streaming.
    confirming,    ///< Waiting for the cameras to report the commanded configuration.
    acquiring,     ///< Collecting one frame from each acquisition sensor.
    solving,       ///< Solving astrometry and combining into a boresight correction.
    offsetting,    ///< Sending the pointing offset to the telescope.
    waitTelescope, ///< Waiting for the telescope to report the move complete.
    verifying,     ///< Re-acquiring and checking the guide star against its tolerance.
    rolling,       ///< Sending a roll maneuver.
    verifyRoll,    ///< Re-acquiring and checking the roll star against its tolerance.
    reconfiguring, ///< Narrowing the guide and roll cameras to the tracking region of interest.
    trackingStart, ///< Enabling the centroid controllers.
    tracking,      ///< Closing the fast pointing and roll loop on published centroids.
    complete,      ///< The sequence finished and tracking was handed off.
    failed         ///< The sequence gave up. acqMessage() says why.
};

/// Name of an acquisition state, for logging and INDI.
/** \returns the state name
 *
 * \ingroup wccCtrl
 */
inline std::string wccAcqStateName( wccAcqState s /**< [in] the state */ )
{
    switch( s )
    {
    case wccAcqState::idle:
        return "IDLE";
    case wccAcqState::loaded:
        return "LOADED";
    case wccAcqState::configuring:
        return "CONFIGURING";
    case wccAcqState::confirming:
        return "CONFIRMING";
    case wccAcqState::acquiring:
        return "ACQUIRING";
    case wccAcqState::solving:
        return "SOLVING";
    case wccAcqState::offsetting:
        return "OFFSETTING";
    case wccAcqState::waitTelescope:
        return "WAIT_TELESCOPE";
    case wccAcqState::verifying:
        return "VERIFYING";
    case wccAcqState::rolling:
        return "ROLLING";
    case wccAcqState::verifyRoll:
        return "VERIFY_ROLL";
    case wccAcqState::reconfiguring:
        return "RECONFIGURING";
    case wccAcqState::trackingStart:
        return "TRACKING_START";
    case wccAcqState::tracking:
        return "TRACKING";
    case wccAcqState::complete:
        return "COMPLETE";
    default:
        return "FAILED";
    }
}

/// One sensor the controller reads frames from and commands.
/** \ingroup wccCtrl
 */
struct wccCtrlSensor
{
    /** \name Identity - Data
     *@{
     */
    size_t m_index{ 0 }; ///< Index of this sensor in the focal plane model.

    std::string m_name; ///< Logical sensor name, e.g. "IMX-18".

    std::string m_indiDevice; ///< INDI camera device this sensor is commanded through.

    std::string m_shmimIn; ///< ImageStreamIO stream frames are read from.
    ///@}

    /** \name Reported Camera State - Data
     *@{
     */
    std::mutex m_mutex; ///< Guards the reported state below.

    double m_fps{ 0 }; ///< Frame rate the camera reports [Hz].

    double m_expTime{ 0 }; ///< Exposure time the camera reports [s].

    wcc::roiSpec m_roi; ///< Region of interest the camera reports.

    bool m_haveFps{ false }; ///< True once the camera has reported a frame rate.

    bool m_haveExpTime{ false }; ///< True once the camera has reported an exposure time.

    bool m_haveROI{ false }; ///< True once the camera has reported a full region of interest.

    int m_roiFields{ 0 }; ///< Bit mask of which region of interest elements have reported.
    ///@}

    /** \name Commanded Configuration - Data
     *@{
     */
    double m_cmdFps{ -1 }; ///< Frame rate last commanded [Hz]. Negative means not commanded.

    double m_cmdExpTime{ -1 }; ///< Exposure time last commanded [s]. Negative means not commanded.

    wcc::roiSpec m_cmdROI; ///< Region of interest last commanded.

    bool m_cmdROIValid{ false }; ///< True once a region of interest has been commanded.

    bool m_inSolution{ false }; ///< Whether this sensor participates in the astrometric solution.
    ///@}

    /** \name Frames and Solutions - Data
     *@{
     */
    IMAGE m_stream{}; ///< The input stream.

    bool m_streamOpen{ false }; ///< True while m_stream is open.

    int m_semIndex{ -1 }; ///< Semaphore index reserved on m_stream.

    std::vector<float> m_frame; ///< Most recently grabbed frame, row major.

    int m_frameW{ 0 }; ///< Width of m_frame [pixels].

    int m_frameH{ 0 }; ///< Height of m_frame [pixels].

    wcc::roiSpec m_frameROI; ///< Region of interest m_frame was grabbed under.

    std::vector<wcc::detection> m_detections; ///< Sources detected in m_frame.

    wcc::backgroundEstimate m_background; ///< Background of m_frame.

    wcc::frameSolution m_solution; ///< Astrometric solution for m_frame.
    ///@}
};

/// The MagAO-X WCC high level acquisition and guiding controller.
/** \ingroup wccCtrl
 */
class wccCtrl : public MagAOXApp<true>
{

    // Give the test harness access.
    friend class wccCtrl_test;

    /// Bit flags marking which region of interest elements a camera has reported.
    enum roiField
    {
        roiFieldX = 1,   ///< roi_region_x has reported.
        roiFieldY = 2,   ///< roi_region_y has reported.
        roiFieldW = 4,   ///< roi_region_w has reported.
        roiFieldH = 8,   ///< roi_region_h has reported.
        roiFieldAll = 15 ///< All four have reported.
    };

    /** \name Visit Configuration - Data
     *@{
     */
  protected:
    /// INDI device publishing the visit, normally visitCtrl.
    /** wccCtrl does not read the visit file. visitCtrl parses it and republishes
     * every parameter, which keeps one parser and one interpretation of the schema
     * in the system.
     */
    std::string m_visitDevice;

    /// Whether to begin the sequence as soon as a complete visit arrives.
    bool m_autoStart{ false };
    ///@}

    /** \name Telescope Interface Configuration - Data
     *@{
     */
  protected:
    std::string m_telDevice; ///< INDI device the pointing offsets are sent to.

    std::string m_telOffsetProperty{ "offset" }; ///< Property carrying the pointing offset.

    std::string m_telOffsetXElement{ "x" }; ///< Element carrying the offset along focal plane X [arcsec].

    std::string m_telOffsetYElement{ "y" }; ///< Element carrying the offset along focal plane Y [arcsec].

    /// Property carrying the roll offset. Empty means use m_telOffsetProperty.
    std::string m_telRollProperty{ "offset" };

    std::string m_telRollElement{ "roll" }; ///< Element carrying the roll offset [deg].

    /// Sign applied to commanded offsets, to adapt to a telescope's own convention.
    double m_telOffsetSign{ 1.0 };

    /// Whether to swap the two offset elements. tcsInterface pyrNudge needs this.
    bool m_telOffsetSwapXY{ false };

    /// Property reporting motion in progress. Empty means fall back to m_telSettleTime.
    std::string m_telDoneProperty;

    /// Elements of m_telDoneProperty that must all read zero for the move to be complete.
    std::vector<std::string> m_telDoneElements;

    std::string m_telPosProperty{ "pointing" }; ///< Property reporting the current pointing.

    std::string m_telRAElement{ "ra" }; ///< Element reporting right ascension [deg].

    std::string m_telDecElement{ "dec" }; ///< Element reporting declination [deg].

    std::string m_telPAElement{ "pa" }; ///< Element reporting the position angle [deg].

    double m_telSettleTime{ 2.0 }; ///< Time to wait after an offset when no done property exists [s].

    double m_telMoveTimeout{ 60.0 }; ///< Give up on a telescope move after this long [s].
    ///@}

    /** \name Telescope Geometry - Data
     *@{
     */
  protected:
    double m_diameter{ 6.5 }; ///< Telescope clear aperture diameter [m]. Must match wccSim.

    double m_fNumber{ 12.0 }; ///< Telescope focal ratio. Must match wccSim.

    /// Handedness of focal plane X relative to increasing right ascension. Must match wccSim.
    double m_parity{ -1.0 };
    ///@}

    /** \name Catalog Configuration - Data
     *@{
     */
  protected:
    std::string m_catalogPath; ///< Path to the star catalog CSV used for prediction.

    std::string m_magColumn{ "mag" }; ///< Catalog column supplying the magnitude.

    std::string m_idColumn{ "gsc2ID" }; ///< Catalog column supplying the identifier.

    double m_catalogMagLimit{ 22.0 }; ///< Drop catalog sources fainter than this on load.

    /// Only predict sources brighter than this when solving, which keeps the match short.
    double m_predictMagLimit{ 19.0 };
    ///@}

    /** \name Acquisition Configuration - Data
     *@{
     */
  protected:
    /// Names of the sensors in the array. Each needs a like named configuration section.
    std::vector<std::string> m_sensorNames;

    double m_guideTolPix{ 50.0 }; ///< Default guide star acceptance radius [pixels].

    double m_rollTolPix{ 50.0 }; ///< Default roll star acceptance radius [pixels].

    int m_maxIterations{ 3 }; ///< Default limit on offset and verify cycles.

    double m_frameTimeout{ 30.0 }; ///< Give up waiting for a frame after this long [s].

    double m_confirmTimeout{ 15.0 }; ///< Give up waiting for a camera to confirm after this long [s].

    double m_roiTolPix{ 1.0 }; ///< Region of interest confirmation tolerance [pixels].

    double m_paramTolFrac{ 0.05 }; ///< Frame rate and exposure time confirmation tolerance, fractional.

    int m_minSolvedSensors{ 1 }; ///< Minimum sensors that must solve for a boresight correction.

    wcc::detectConfig m_detect; ///< Source detection tuning.

    wcc::solveConfig m_solve; ///< Astrometric solver tuning.
    ///@}

    /** \name Tracking Configuration - Data
     *@{
     */
  protected:
    int m_trackROIW{ 128 }; ///< Default tracking region of interest width [pixels].

    int m_trackROIH{ 128 }; ///< Default tracking region of interest height [pixels].

    double m_trackFps{ 100.0 }; ///< Default tracking frame rate [Hz].

    double m_trackExpTime{ 0.01 }; ///< Default tracking exposure time [s].

    double m_trackGain{ 0.3 }; ///< Default proportional gain of the fast pointing loop.

    double m_trackRollGain{ 0.3 }; ///< Default proportional gain of the fast roll loop.

    std::string m_centroidProperty{ "centroid" }; ///< Property the centroid controllers publish.

    std::string m_centroidXElement{ "x" }; ///< Element carrying the centroid column [pixels].

    std::string m_centroidYElement{ "y" }; ///< Element carrying the centroid row [pixels].

    /// Toggle on a centroid controller that enables it. Empty means it needs no enabling.
    std::string m_centroidEnableProperty{ "loop" };

    double m_trackMaxOffset{ 5.0 }; ///< Largest single fast loop offset [arcsec]; larger is rejected.

    double m_trackPeriod{ 0.05 }; ///< Interval between fast loop corrections [s].

    double m_trackDeadband{ 0.05 }; ///< Ignore centroid errors below this [pixels].
    ///@}

    /** \name Sequence State - Data
     *@{
     */
  protected:
    /// The visit as mirrored from visitCtrl over INDI.
    /** Assembled by setCallBack_remote() as the properties arrive, then promoted to
     * the working configuration by visitFromIndi() once complete.
     */
    wcc::visitSelection m_selection;

    std::mutex m_selMutex; ///< Guards m_selection and the arrival flags below.

    /// Which of the visit properties have arrived, so completeness can be tested.
    bool m_haveTarget{ false };

    bool m_haveGuideIndi{ false }; ///< Guide star property has arrived.

    bool m_haveRollIndi{ false }; ///< Roll star property has arrived.

    bool m_haveAcqParams{ false }; ///< Acquisition parameters have arrived.

    /// The loader state visitCtrl reports; only LOADED is acted on.
    std::string m_visitState;

    bool m_visitLoaded{ false }; ///< True once a complete visit has been promoted.

    /// The star catalog. Immutable after appStartup.
    wcc::starCatalog m_catalog;

    /// Sensor array geometry and the pointing the solver predicts against.
    wcc::focalPlaneModel m_focalPlane;

    std::mutex m_stateMutex; ///< Guards the sequence state below.

    wccAcqState m_acqState{ wccAcqState::idle }; ///< Current stage of the sequence.

    std::string m_acqMessage; ///< Why the sequence is where it is, especially on failure.

    int m_iteration{ 0 }; ///< Offset and verify cycles completed.

    double m_guideErrPix{ -1 }; ///< Distance of the guide star from its target pixel [pixels].

    double m_rollErrPix{ -1 }; ///< Distance of the roll star from its target pixel [pixels].

    int m_nSolved{ 0 }; ///< Sensors that solved on the last pass.

    wcc::boresightSolution m_boresight; ///< Last boresight correction solved.

    /// Reported telescope pointing, guarded by m_telMutex.
    double m_telRA{ 0 };

    double m_telDec{ 0 }; ///< Reported telescope declination [deg].

    double m_telPA{ 0 }; ///< Reported telescope position angle [deg].

    bool m_haveTelPos{ false }; ///< True once the telescope has reported a pointing.

    bool m_telMoving{ false }; ///< True while the telescope reports motion in progress.

    bool m_haveTelDone{ false }; ///< True once the done property has reported at least once.

    std::mutex m_telMutex; ///< Guards the reported telescope state.

    /// The sensors, held by pointer because each owns a mutex.
    std::vector<std::unique_ptr<wccCtrlSensor>> m_sensors;

    int m_guideSensor{ -1 }; ///< Index into m_sensors of the guide sensor, -1 if unresolved.

    int m_rollSensor{ -1 }; ///< Index into m_sensors of the roll sensor, -1 if unresolved.

    wcc::visitStar m_guideStar; ///< The selected guide star.

    wcc::visitStar m_rollStarSel; ///< The selected roll star.

    /// Sensors the loaded visit put into the astrometric solution.
    int m_nInSolution{ 0 };

    bool m_haveGuideStar{ false }; ///< True once a guide star has been selected and resolved.

    bool m_haveRollStar{ false }; ///< True once a roll star has been selected and resolved.

    std::atomic<bool> m_startRequest{ false }; ///< Set by the start switch, consumed by the sequencer.

    std::atomic<bool> m_abortRequest{ false }; ///< Set by the abort switch, polled throughout.

    std::atomic<bool> m_shutdownSequencer{ false }; ///< Tells the sequencer thread to exit.

    std::thread m_sequencer; ///< Thread running the acquisition sequence.
    ///@}

    /** \name Tracking State - Data
     *@{
     */
  protected:
    std::mutex m_trackMutex; ///< Guards the centroid state below.

    double m_guideCentroidX{ -1 }; ///< Guide sensor centroid column [pixels].

    double m_guideCentroidY{ -1 }; ///< Guide sensor centroid row [pixels].

    bool m_haveGuideCentroid{ false }; ///< True once the guide centroid controller has reported.

    double m_rollCentroidX{ -1 }; ///< Roll sensor centroid column [pixels].

    double m_rollCentroidY{ -1 }; ///< Roll sensor centroid row [pixels].

    bool m_haveRollCentroid{ false }; ///< True once the roll centroid controller has reported.

    double m_trackGuideTargetX{ 0 }; ///< Guide star target column in tracking ROI pixels.

    double m_trackGuideTargetY{ 0 }; ///< Guide star target row in tracking ROI pixels.

    double m_trackRollTargetX{ 0 }; ///< Roll star target column in tracking ROI pixels.

    double m_trackRollTargetY{ 0 }; ///< Roll star target row in tracking ROI pixels.

    uint64_t m_trackCorrections{ 0 }; ///< Fast loop corrections sent.
    ///@}

    /** \name INDI - Data
     *@{
     */
  protected:
    /// Subscribed visitCtrl properties, held so they stay registered.
    pcf::IndiProperty m_indiP_vTarget;

    pcf::IndiProperty m_indiP_vTargetInfo;

    pcf::IndiProperty m_indiP_vGuideStar;

    pcf::IndiProperty m_indiP_vGuideStarInfo;

    pcf::IndiProperty m_indiP_vRollStar;

    pcf::IndiProperty m_indiP_vRollStarInfo;

    pcf::IndiProperty m_indiP_vAcqParams;

    pcf::IndiProperty m_indiP_vTrackParams;

    pcf::IndiProperty m_indiP_vTrackDevices;

    pcf::IndiProperty m_indiP_vConfigSensors;

    pcf::IndiProperty m_indiP_vStatus;

    pcf::IndiProperty m_indiP_start; ///< Request switch that begins the sequence.
    INDI_NEWCALLBACK_DECL( wccCtrl, m_indiP_start );

    pcf::IndiProperty m_indiP_abort; ///< Request switch that aborts the sequence.
    INDI_NEWCALLBACK_DECL( wccCtrl, m_indiP_abort );

    pcf::IndiProperty m_indiP_acqState; ///< Read-only acquisition state name and message.

    pcf::IndiProperty m_indiP_acqStatus; ///< Read-only acquisition numbers.

    pcf::IndiProperty m_indiP_visit; ///< Read-only summary of the loaded visit.

    /// Read-only parameters the loaded visit supplied, refreshed on every load.
    /** These are the values that change when a different visit file is loaded, so
     * they are published rather than left implicit in a configuration file: an
     * operator can see the tolerances and tracking configuration the running
     * sequence is actually using, not the ones the .conf file happened to default
     * to at startup.
     */
    pcf::IndiProperty m_indiP_visitParams;

    pcf::IndiProperty m_indiP_trackStatus; ///< Read-only fast loop status.

    /// Telescope pointing property, when a telescope device is configured.
    pcf::IndiProperty m_indiP_telPos;

    /// Telescope motion property, when a done property is configured.
    pcf::IndiProperty m_indiP_telDone;

    /// Every remote property this app subscribes to, held so they stay registered.
    std::vector<std::shared_ptr<pcf::IndiProperty>> m_remoteProps;

    /// Centroid controller devices already subscribed to, so a reload does not double subscribe.
    std::vector<std::string> m_centroidSubscribed;

    /// What a subscribed remote property updates.
    struct remoteBinding
    {
        wccCtrlSensor *m_sensor{ nullptr }; ///< Sensor the property belongs to, if any.

        std::string m_what; ///< Parameter selector.
    };

    /// Subscriptions indexed by pcf::IndiProperty::createUniqueKey().
    std::map<std::string, remoteBinding> m_bindings;
    ///@}

  public:
    /// Default c'tor.
    wccCtrl();

    /// D'tor, declared and defined for noexcept.
    ~wccCtrl() noexcept;

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** \returns 0 on success
     * \returns -1 on a configuration error, in which case the app shuts down
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] configuration to load from */ );

    virtual void loadConfig();

    virtual int appStartup();

    /// Implementation of the FSM for wccCtrl.
    /** \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
    virtual int appLogic();

    virtual int appShutdown();

    /** \name Visit Handling
     *@{
     */
  protected:
    /// True once every visit property needed to run a sequence has arrived.
    /** \returns true if the mirrored visit is complete and visitCtrl reports LOADED
     */
    bool visitComplete();

    /// Promote the mirrored visit to the working configuration.
    /** Applies the visit's tolerances and tracking parameters over the configured
     * defaults, marks which sensors take part in the solution, and resolves the
     * guide and roll stars onto sensor indices.
     *
     * \returns 0 on success
     * \returns -1 if the visit is incomplete or its stars are not on configured sensors
     */
    int visitFromIndi();

    /// Resolve the selected guide and roll stars onto configured sensor indices.
    /** Rank selection belongs to visitCtrl, which publishes one already-chosen pair,
     * so this only has to find the sensors. If either star is on a sensor this
     * controller does not have configured the visit is unusable, and the operator
     * should move visitCtrl's `select_rank` to a pair that fits the array.
     *
     * \returns 0 on success
     * \returns -1 if either star is not on a configured sensor
     */
    int resolveStars();

    /// The acquisition configuration for one sensor.
    /** The per sensor overrides the visit file's TA_SENSORS block could carry are not
     * mirrored over INDI: the acquisition parameters apply to every participating
     * sensor. That is the simplification that moving the parser out bought, and it
     * matches how the array is actually configured in practice.
     *
     * \returns the configuration to command
     */
    wcc::visitSensorConfig sensorConfigFor( const std::string &name /**< [in] logical sensor name */ );
    ///@}

    /** \name Sequencer
     *@{
     */
  protected:
    /// Thread entry for the sequencer.
    static void sequencerStart( wccCtrl *s /**< [in] this */ );

    /// Run the acquisition sequence, then hold in the tracking loop.
    void sequencerExec();

    /// Set the sequence state and message, and log the transition.
    void setAcqState( wccAcqState s /**< [in] the new state */,
                      const std::string &msg = std::string() /**< [in] why */ );

    /// The current sequence state.
    /** \returns the state
     */
    wccAcqState acqState();

    /// True if the sequence should stop, because of an abort or a shutdown.
    /** \returns true to stop
     */
    bool stopRequested();

    /// Fail the sequence with a message.
    /** \returns -1 always, so callers can `return failSequence(...)`
     */
    int failSequence( const std::string &why /**< [in] why the sequence failed */ );
    ///@}

    /** \name Sequence Stages
     *@{
     */
  protected:
    /// Command the acquisition configuration onto every participating sensor.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int configureSensors();

    /// Wait for the cameras to report the configuration that was commanded.
    /** \returns 0 once every commanded sensor agrees
     * \returns -1 on timeout, naming the sensors that did not confirm
     */
    int confirmSensors();

    /// Grab one frame from every participating sensor.
    /** \returns the number of sensors that yielded a frame
     */
    int acquireFrames();

    /// Detect sources and solve astrometry on every sensor that has a frame.
    /** \returns the number of sensors that solved
     */
    int solveFrames();

    /// Combine the per sensor solutions into a boresight and roll correction.
    /** \returns 0 on success
     * \returns -1 if too few sensors solved
     */
    int solveBoresightCorrection();

    /// Send a pointing offset, then wait for the telescope to finish moving.
    /** \returns 0 on success
     * \returns -1 on a command failure or a move timeout
     */
    int offsetTelescope( double fieldX /**< [in] offset along focal plane X [arcsec] */,
                         double fieldY /**< [in] offset along focal plane Y [arcsec] */,
                         double roll /**< [in] roll offset [deg] */ );

    /// Wait for the telescope to report that it has stopped moving.
    /** \returns 0 once the telescope is done
     * \returns -1 on timeout
     */
    int waitForTelescope();

    /// Measure how far a star is from its target pixel on its sensor.
    /** Uses the sensor's astrometric solution to place the star, so it works even
     * when the star is not the brightest source in the frame.
     *
     * \returns 0 on success
     * \returns -1 if the sensor has no valid solution
     */
    int measureStarError( int iSensor /**< [in] index into m_sensors */,
                          const wcc::visitStar &star /**< [in] the star to place */,
                          double &errPix /**< [out] distance from the target pixel [pixels] */,
                          double &fieldX /**< [out] field shift needed to correct it [arcsec] */,
                          double &fieldY /**< [out] field shift needed to correct it [arcsec] */ );

    /// Narrow the guide and roll cameras to the tracking region of interest.
    /** \returns 0 on success
     * \returns -1 on a command or confirmation failure
     */
    int reconfigureForTracking();

    /// Enable the centroid controllers.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int startCentroidControllers();

    /// Run one iteration of the fast pointing and roll loop.
    /** \returns 0 on success, including when there is nothing to correct
     * \returns -1 on a command failure
     */
    int trackingIteration();
    ///@}

    /** \name Camera Commands
     *@{
     */
  protected:
    /// Command a region of interest onto a camera and trigger the reconfiguration.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int commandROI( wccCtrlSensor *sen /**< [in,out] the sensor */,
                    const wcc::roiSpec &roi /**< [in] the region of interest to command */ );

    /// Command a frame rate onto a camera.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int commandFps( wccCtrlSensor *sen /**< [in,out] the sensor */,
                    double fps /**< [in] frame rate [Hz] */ );

    /// Command an exposure time onto a camera.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int commandExpTime( wccCtrlSensor *sen /**< [in,out] the sensor */,
                        double expTime /**< [in] exposure time [s] */ );

    /// Command a gain onto a camera.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int commandGain( wccCtrlSensor *sen /**< [in] the sensor */, double gain /**< [in] gain */ );

    /// Turn a camera's streaming toggle on or off.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int commandStreaming( wccCtrlSensor *sen /**< [in] the sensor */, bool on /**< [in] desired state */ );

    /// Send one numeric element to a remote device.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int sendNumber( const std::string &device /**< [in] target device */,
                    const std::string &property /**< [in] target property */,
                    const std::string &element /**< [in] target element */,
                    double value /**< [in] value to send */ );

    /// Send a request switch to a remote device.
    /** \returns 0 on success
     * \returns -1 on a command failure
     */
    int sendRequest( const std::string &device /**< [in] target device */,
                     const std::string &property /**< [in] target property */ );
    ///@}

    /** \name Frame Handling
     *@{
     */
  protected:
    /// Open a sensor's input stream if it is not already open.
    /** \returns 0 on success
     * \returns -1 if the stream cannot be opened
     */
    int openSensorStream( wccCtrlSensor *sen /**< [in,out] the sensor */ );

    /// Close a sensor's input stream.
    void closeSensorStream( wccCtrlSensor *sen /**< [in,out] the sensor */ );

    /// Wait for and copy the most recent frame from a sensor's input stream.
    /** Converts whatever pixel type the stream holds into float, and records the
     * region of interest the camera reported at grab time so the astrometric
     * prediction uses the right geometry.
     *
     * \returns 0 on success
     * \returns -1 on a timeout or a stream error
     */
    int grabFrame( wccCtrlSensor *sen /**< [in,out] the sensor */,
                   double timeout /**< [in] how long to wait [s] */ );
    ///@}

    /** \name INDI Helpers
     *@{
     */
  protected:
    /// Register the INDI subscriptions for one sensor's camera device.
    /** \returns 0 on success
     * \returns -1 on a registration failure
     */
    int registerSensorSubscriptions( wccCtrlSensor *sen /**< [in,out] the sensor */ );

    /// Subscribe to a centroid controller's output property.
    /** The centroid controller device names come from the visit file, so this
     * cannot happen at startup. Subscribing twice to the same device is a no-op,
     * because INDI subscriptions cannot be withdrawn once made.
     *
     * \returns 0 on success, including when already subscribed
     * \returns -1 on a registration failure
     */
    int registerCentroidSubscription( const std::string &device /**< [in] centroid controller device */,
                                      bool isGuide /**< [in] true for the guide sensor's controller */ );

    /// The region of interest a sensor is believed to be running.
    /** Prefers the camera's own report over what was last commanded, and takes the
     * sensor's lock itself.
     *
     * \returns the region of interest
     */
    wcc::roiSpec reportedROI( wccCtrlSensor *sen /**< [in] the sensor */ );

    /// Shared SET callback for every remote property this app subscribes to.
    /** A single callback is used because the number of cameras is a runtime
     * quantity, so the per-property INDI_SETCALLBACK macros cannot be applied.
     *
     * \returns 0 always, so an unexpected property is ignored rather than fatal
     */
    int setCallBack_remote( const pcf::IndiProperty &ipRecv /**< [in] the received property */ );

    /// Static trampoline for setCallBack_remote().
    /** \returns the result of setCallBack_remote()
     */
    static int st_setCallBack_remote( void *app /**< [in] the wccCtrl instance */,
                                      const pcf::IndiProperty &ipRecv /**< [in] the received property */ );

    /// Publish the status properties.
    void updateStatus();

    /// Read a numeric element from a received property, tolerating text encoding.
    /** \returns true if the element was present and parsed to a finite number
     */
    static bool elementValue( const pcf::IndiProperty &ip /**< [in] the property */,
                              const std::string &el /**< [in] element name */,
                              double &out /**< [out] the value */ );
    ///@}

    /** \name Geometry Helpers
     *@{
     */
  protected:
    /// Build the world coordinate system of a sensor's last grabbed frame.
    /** Uses the reported telescope pointing when available, otherwise the visit
     * file's requested pointing.
     *
     * \returns 0 on success
     * \returns -1 if the sensor index is invalid
     */
    int frameWCS( const wccCtrlSensor *sen /**< [in] the sensor */, wcc::skyWCS &wcs /**< [out] the WCS */ );

    /// The pointing the astrometric prediction should use.
    void predictPointing( double &ra /**< [out] right ascension [deg] */,
                          double &dec /**< [out] declination [deg] */,
                          double &pa /**< [out] position angle [deg] */ );

    /// Convert a pixel displacement on a sensor into a focal plane field shift.
    /** Differences the field angle at two pixels, so the sensor's plate scale and
     * mounting rotation are both accounted for.
     */
    void pixelToFieldShift( size_t iSensor /**< [in] sensor index in the focal plane model */,
                            double x /**< [in] reference column [pixels] */,
                            double y /**< [in] reference row [pixels] */,
                            double dx /**< [in] displacement along columns [pixels] */,
                            double dy /**< [in] displacement along rows [pixels] */,
                            double &fieldX /**< [out] field shift along focal plane X [arcsec] */,
                            double &fieldY /**< [out] field shift along focal plane Y [arcsec] */ );

    /// Find a sensor by name, tolerating the HWK and HAWK spellings.
    /** \returns the index into m_sensors
     * \returns -1 if no sensor matches
     */
    int sensorByName( const std::string &name /**< [in] logical sensor name */ ) const;
    ///@}
};

inline wccCtrl::wccCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    // No PDU: this is a pure software controller.
    m_powerMgtEnabled = false;

    // The sequencer thread carries the work; appLogic only publishes status. Every
    // WCC application holds its INDI traffic to 1 Hz.
    m_loopPause = wcc::indiLoopPause;

    return;
}

inline wccCtrl::~wccCtrl() noexcept
{
    return;
}

inline void wccCtrl::setupConfig()
{
    config.add( "visit.device", "", "visit.device", argType::Required, "visit", "device", false, "string",
                "INDI device publishing the visit, normally visitCtrl. wccCtrl does not read the visit file "
                "itself." );
    config.add( "visit.auto_start", "", "visit.auto_start", argType::Required, "visit", "auto_start", false,
                "bool", "Begin the acquisition sequence as soon as a complete visit arrives." );

    config.add( "telescope.device", "", "telescope.device", argType::Required, "telescope", "device", false,
                "string", "INDI device pointing offsets are sent to, e.g. tcsi or a simulator." );
    config.add( "telescope.offset_property", "", "telescope.offset_property", argType::Required, "telescope",
                "offset_property", false, "string",
                "Property carrying the pointing offset. tcsInterface uses pyrNudge." );
    config.add( "telescope.offset_x_element", "", "telescope.offset_x_element", argType::Required, "telescope",
                "offset_x_element", false, "string", "Element carrying the offset along focal plane X [arcsec]." );
    config.add( "telescope.offset_y_element", "", "telescope.offset_y_element", argType::Required, "telescope",
                "offset_y_element", false, "string", "Element carrying the offset along focal plane Y [arcsec]." );
    config.add( "telescope.offset_sign", "", "telescope.offset_sign", argType::Required, "telescope",
                "offset_sign", false, "double",
                "Sign applied to commanded offsets, to match the telescope's own convention." );
    config.add( "telescope.offset_swap_xy", "", "telescope.offset_swap_xy", argType::Required, "telescope",
                "offset_swap_xy", false, "bool",
                "Swap the two offset elements. Needed for tcsInterface pyrNudge, whose x and y are the WFS "
                "axes rather than the focal plane axes." );
    config.add( "telescope.roll_property", "", "telescope.roll_property", argType::Required, "telescope",
                "roll_property", false, "string", "Property carrying the roll offset." );
    config.add( "telescope.roll_element", "", "telescope.roll_element", argType::Required, "telescope",
                "roll_element", false, "string", "Element carrying the roll offset [deg]." );
    config.add( "telescope.done_property", "", "telescope.done_property", argType::Required, "telescope",
                "done_property", false, "string",
                "Property reporting motion in progress, e.g. teldata. Empty falls back to settle_time." );
    config.add( "telescope.done_elements", "", "telescope.done_elements", argType::Required, "telescope",
                "done_elements", false, "vector<string>",
                "Elements of done_property that must all read zero, e.g. slewing,guider_moving." );
    config.add( "telescope.pos_property", "", "telescope.pos_property", argType::Required, "telescope",
                "pos_property", false, "string", "Property reporting the current pointing." );
    config.add( "telescope.ra_element", "", "telescope.ra_element", argType::Required, "telescope", "ra_element",
                false, "string", "Element reporting right ascension [deg]." );
    config.add( "telescope.dec_element", "", "telescope.dec_element", argType::Required, "telescope",
                "dec_element", false, "string", "Element reporting declination [deg]." );
    config.add( "telescope.pa_element", "", "telescope.pa_element", argType::Required, "telescope", "pa_element",
                false, "string", "Element reporting the position angle [deg]." );
    config.add( "telescope.settle_time", "", "telescope.settle_time", argType::Required, "telescope",
                "settle_time", false, "double", "Wait after an offset when no done property exists [s]." );
    config.add( "telescope.move_timeout", "", "telescope.move_timeout", argType::Required, "telescope",
                "move_timeout", false, "double", "Give up on a telescope move after this long [s]." );
    config.add( "telescope.diameter", "", "telescope.diameter", argType::Required, "telescope", "diameter", false,
                "double", "Clear aperture diameter [m]. Must match wccSim." );
    config.add( "telescope.f_number", "", "telescope.f_number", argType::Required, "telescope", "f_number", false,
                "double", "Focal ratio. Must match wccSim." );
    config.add( "telescope.parity", "", "telescope.parity", argType::Required, "telescope", "parity", false,
                "double", "Handedness of focal plane X against increasing RA. Must match wccSim." );

    config.add( "catalog.path", "", "catalog.path", argType::Required, "catalog", "path", false, "string",
                "Path to the star catalog CSV used to predict source positions." );
    config.add( "catalog.mag_column", "", "catalog.mag_column", argType::Required, "catalog", "mag_column", false,
                "string", "Header name of the catalog column supplying the magnitude." );
    config.add( "catalog.id_column", "", "catalog.id_column", argType::Required, "catalog", "id_column", false,
                "string", "Header name of the catalog column supplying the identifier." );
    config.add( "catalog.mag_limit", "", "catalog.mag_limit", argType::Required, "catalog", "mag_limit", false,
                "double", "Drop catalog sources fainter than this on load." );
    config.add( "catalog.predict_mag_limit", "", "catalog.predict_mag_limit", argType::Required, "catalog",
                "predict_mag_limit", false, "double",
                "Only predict sources brighter than this when solving, which keeps the match short." );

    config.add( "acq.sensors", "", "acq.sensors", argType::Required, "acq", "sensors", false, "vector<string>",
                "Comma separated sensor names. Each needs a configuration section of the same name." );
    config.add( "acq.guide_tol_px", "", "acq.guide_tol_px", argType::Required, "acq", "guide_tol_px", false,
                "double", "Guide star acceptance radius [pixels]. The visit file may override this." );
    config.add( "acq.roll_tol_px", "", "acq.roll_tol_px", argType::Required, "acq", "roll_tol_px", false, "double",
                "Roll star acceptance radius [pixels]. The visit file may override this." );
    config.add( "acq.max_iterations", "", "acq.max_iterations", argType::Required, "acq", "max_iterations", false,
                "int", "Limit on offset and verify cycles. The visit file may override this." );
    config.add( "acq.frame_timeout", "", "acq.frame_timeout", argType::Required, "acq", "frame_timeout", false,
                "double", "Give up waiting for a frame after this long [s]." );
    config.add( "acq.confirm_timeout", "", "acq.confirm_timeout", argType::Required, "acq", "confirm_timeout",
                false, "double", "Give up waiting for a camera to confirm its configuration after this long [s]." );
    config.add( "acq.roi_tol_px", "", "acq.roi_tol_px", argType::Required, "acq", "roi_tol_px", false, "double",
                "Region of interest confirmation tolerance [pixels]." );
    config.add( "acq.param_tol_frac", "", "acq.param_tol_frac", argType::Required, "acq", "param_tol_frac", false,
                "double", "Fractional tolerance when confirming frame rate and exposure time." );
    config.add( "acq.min_solved_sensors", "", "acq.min_solved_sensors", argType::Required, "acq",
                "min_solved_sensors", false, "int",
                "Minimum sensors that must solve before a boresight correction is trusted." );
    config.add( "acq.detect_threshold", "", "acq.detect_threshold", argType::Required, "acq", "detect_threshold",
                false, "double", "Source detection threshold in units of the background sigma." );
    config.add( "acq.detect_box_half", "", "acq.detect_box_half", argType::Required, "acq", "detect_box_half",
                false, "int", "Half width of the centroid window [pixels]." );
    config.add( "acq.min_separation", "", "acq.min_separation", argType::Required, "acq", "min_separation", false,
                "int", "Minimum separation between accepted sources [pixels]." );
    config.add( "acq.max_sources", "", "acq.max_sources", argType::Required, "acq", "max_sources", false, "int",
                "Stop detecting after this many sources, brightest first." );
    config.add( "acq.ring_suppress_radius", "", "acq.ring_suppress_radius", argType::Required, "acq",
                "ring_suppress_radius", false, "double",
                "Radius over which a bright star suppresses much fainter neighbours [pixels]." );
    config.add( "acq.ring_suppress_ratio", "", "acq.ring_suppress_ratio", argType::Required, "acq",
                "ring_suppress_ratio", false, "double",
                "Brightness fraction a source must reach to survive ring suppression." );
    config.add( "acq.solve_search_px", "", "acq.solve_search_px", argType::Required, "acq", "solve_search_px",
                false, "double", "Largest pointing error the astrometric match will search [pixels]." );
    config.add( "acq.solve_vote_bin", "", "acq.solve_vote_bin", argType::Required, "acq", "solve_vote_bin", false,
                "double", "Offset vote bin size [pixels]." );
    config.add( "acq.solve_match_px", "", "acq.solve_match_px", argType::Required, "acq", "solve_match_px", false,
                "double", "Source pairing radius once the coarse offset is known [pixels]." );
    config.add( "acq.solve_min_matched", "", "acq.solve_min_matched", argType::Required, "acq",
                "solve_min_matched", false, "int", "Minimum matched sources for a valid solution." );
    config.add( "acq.solve_rotation", "", "acq.solve_rotation", argType::Required, "acq", "solve_rotation", false,
                "bool", "Solve for field rotation in addition to translation." );

    config.add( "track.roi_w", "", "track.roi_w", argType::Required, "track", "roi_w", false, "int",
                "Tracking region of interest width [pixels]. The visit file may override this." );
    config.add( "track.roi_h", "", "track.roi_h", argType::Required, "track", "roi_h", false, "int",
                "Tracking region of interest height [pixels]. The visit file may override this." );
    config.add( "track.fps", "", "track.fps", argType::Required, "track", "fps", false, "double",
                "Tracking frame rate [Hz]. The visit file may override this." );
    config.add( "track.exptime", "", "track.exptime", argType::Required, "track", "exptime", false, "double",
                "Tracking exposure time [s]. The visit file may override this." );
    config.add( "track.loop_gain", "", "track.loop_gain", argType::Required, "track", "loop_gain", false,
                "double", "Proportional gain of the fast pointing loop." );
    config.add( "track.roll_gain", "", "track.roll_gain", argType::Required, "track", "roll_gain", false,
                "double", "Proportional gain of the fast roll loop." );
    config.add( "track.centroid_property", "", "track.centroid_property", argType::Required, "track",
                "centroid_property", false, "string", "Property the centroid controllers publish." );
    config.add( "track.centroid_x_element", "", "track.centroid_x_element", argType::Required, "track",
                "centroid_x_element", false, "string", "Element carrying the centroid column [pixels]." );
    config.add( "track.centroid_y_element", "", "track.centroid_y_element", argType::Required, "track",
                "centroid_y_element", false, "string", "Element carrying the centroid row [pixels]." );
    config.add( "track.centroid_enable_property", "", "track.centroid_enable_property", argType::Required,
                "track", "centroid_enable_property", false, "string",
                "Toggle on a centroid controller that enables it. Empty means none is needed." );
    config.add( "track.max_offset_arcsec", "", "track.max_offset_arcsec", argType::Required, "track",
                "max_offset_arcsec", false, "double",
                "Largest single fast loop offset [arcsec]. Larger corrections are rejected as glitches." );
    config.add( "track.period", "", "track.period", argType::Required, "track", "period", false, "double",
                "Interval between fast loop corrections [s]." );
    config.add( "track.deadband_px", "", "track.deadband_px", argType::Required, "track", "deadband_px", false,
                "double", "Ignore centroid errors below this [pixels]." );
}

inline int wccCtrl::loadConfigImpl( mx::app::appConfigurator &_config )
{
    _config( m_visitDevice, "visit.device" );
    _config( m_autoStart, "visit.auto_start" );

    _config( m_telDevice, "telescope.device" );
    _config( m_telOffsetProperty, "telescope.offset_property" );
    _config( m_telOffsetXElement, "telescope.offset_x_element" );
    _config( m_telOffsetYElement, "telescope.offset_y_element" );
    _config( m_telOffsetSign, "telescope.offset_sign" );
    _config( m_telOffsetSwapXY, "telescope.offset_swap_xy" );
    _config( m_telRollProperty, "telescope.roll_property" );
    _config( m_telRollElement, "telescope.roll_element" );
    _config( m_telDoneProperty, "telescope.done_property" );
    _config( m_telDoneElements, "telescope.done_elements" );
    _config( m_telPosProperty, "telescope.pos_property" );
    _config( m_telRAElement, "telescope.ra_element" );
    _config( m_telDecElement, "telescope.dec_element" );
    _config( m_telPAElement, "telescope.pa_element" );
    _config( m_telSettleTime, "telescope.settle_time" );
    _config( m_telMoveTimeout, "telescope.move_timeout" );
    _config( m_diameter, "telescope.diameter" );
    _config( m_fNumber, "telescope.f_number" );
    _config( m_parity, "telescope.parity" );

    _config( m_catalogPath, "catalog.path" );
    _config( m_magColumn, "catalog.mag_column" );
    _config( m_idColumn, "catalog.id_column" );
    _config( m_catalogMagLimit, "catalog.mag_limit" );
    _config( m_predictMagLimit, "catalog.predict_mag_limit" );

    _config( m_sensorNames, "acq.sensors" );
    _config( m_guideTolPix, "acq.guide_tol_px" );
    _config( m_rollTolPix, "acq.roll_tol_px" );
    _config( m_maxIterations, "acq.max_iterations" );
    _config( m_frameTimeout, "acq.frame_timeout" );
    _config( m_confirmTimeout, "acq.confirm_timeout" );
    _config( m_roiTolPix, "acq.roi_tol_px" );
    _config( m_paramTolFrac, "acq.param_tol_frac" );
    _config( m_minSolvedSensors, "acq.min_solved_sensors" );

    _config( m_detect.m_threshold, "acq.detect_threshold" );
    _config( m_detect.m_boxHalf, "acq.detect_box_half" );
    _config( m_detect.m_minSeparation, "acq.min_separation" );
    _config( m_detect.m_ringSuppressRadius, "acq.ring_suppress_radius" );
    _config( m_detect.m_ringSuppressRatio, "acq.ring_suppress_ratio" );

    {
        int maxSrc = static_cast<int>( m_detect.m_maxSources );
        _config( maxSrc, "acq.max_sources" );
        if( maxSrc > 0 )
        {
            m_detect.m_maxSources = static_cast<size_t>( maxSrc );
        }
    }

    _config( m_solve.m_searchRadius, "acq.solve_search_px" );
    _config( m_solve.m_voteBin, "acq.solve_vote_bin" );
    _config( m_solve.m_matchRadius, "acq.solve_match_px" );
    _config( m_solve.m_minMatched, "acq.solve_min_matched" );
    _config( m_solve.m_solveRotation, "acq.solve_rotation" );

    _config( m_trackROIW, "track.roi_w" );
    _config( m_trackROIH, "track.roi_h" );
    _config( m_trackFps, "track.fps" );
    _config( m_trackExpTime, "track.exptime" );
    _config( m_trackGain, "track.loop_gain" );
    _config( m_trackRollGain, "track.roll_gain" );
    _config( m_centroidProperty, "track.centroid_property" );
    _config( m_centroidXElement, "track.centroid_x_element" );
    _config( m_centroidYElement, "track.centroid_y_element" );
    _config( m_centroidEnableProperty, "track.centroid_enable_property" );
    _config( m_trackMaxOffset, "track.max_offset_arcsec" );
    _config( m_trackPeriod, "track.period" );
    _config( m_trackDeadband, "track.deadband_px" );

    if( m_diameter <= 0 || m_fNumber <= 0 )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "telescope.diameter and telescope.f_number must both be positive" } );
    }

    if( m_sensorNames.empty() )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "acq.sensors is empty; at least one sensor must be configured" } );
    }

    if( m_telDevice.empty() )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "telescope.device is not set; there is nothing to send offsets to" } );
    }

    if( m_visitDevice.empty() )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__,
              "visit.device is not set; wccCtrl reads the visit from visitCtrl over INDI and has nothing to "
              "read without it" } );
    }

    // The fast loop sends INDI offsets, so its period is subject to the same 1 Hz
    // limit. Clamping here rather than silently exceeding it keeps the constraint
    // visible in the configuration.
    if( m_trackPeriod < wcc::indiMinPeriod )
    {
        log<text_log>( "track.period raised from " + std::to_string( m_trackPeriod ) + " to " +
                           std::to_string( wcc::indiMinPeriod ) +
                           " s: INDI traffic is limited to 1 Hz to avoid overloading the server",
                       logPrio::LOG_NOTICE );

        m_trackPeriod = wcc::indiMinPeriod;
    }

    m_focalPlane.setTelescope( m_diameter, m_fNumber, m_parity );

    for( const std::string &name : m_sensorNames )
    {
        wcc::sensorConfig sc;
        std::string err;

        if( wcc::loadSensorGeometry( _config, name, sc, err ) < 0 )
        {
            return log<software_critical, -1>( { __FILE__, __LINE__, err } );
        }

        std::unique_ptr<wccCtrlSensor> sen( new wccCtrlSensor );
        sen->m_index = m_focalPlane.nSensors();
        sen->m_name = sc.m_name;

        if( !wcc::sensorConfigString( _config, name, "indi_device", sen->m_indiDevice ) )
        {
            return log<software_critical, -1>(
                { __FILE__, __LINE__, "sensor section [" + name + "] is missing indi_device" } );
        }

        // The controller reads what the simulator writes, so the default mirrors
        // wccSim's own default output name.
        sen->m_shmimIn = sen->m_indiDevice + "sim";
        wcc::sensorConfigString( _config, name, "shmim_in", sen->m_shmimIn );

        sen->m_roi = sc.fullFrameROI();
        sen->m_cmdROI = sen->m_roi;

        m_focalPlane.addSensor( sc );
        m_sensors.push_back( std::move( sen ) );
    }

    return 0;
}

inline void wccCtrl::loadConfig()
{
    if( loadConfigImpl( config ) < 0 )
    {
        m_shutdown = true;
    }
}

inline int wccCtrl::appStartup()
{
    // ------------------------------------------------------------- catalog
    if( m_catalogPath.empty() )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, "catalog.path is not set" } );
    }

    if( m_catalog.load( m_catalogPath, m_magColumn, m_idColumn, m_catalogMagLimit ) < 0 || m_catalog.empty() )
    {
        return log<software_critical, -1>(
            { __FILE__, __LINE__, "failed to load catalog " + m_catalogPath } );
    }

    log<text_log>( "loaded " + std::to_string( m_catalog.size() ) + " sources from " + m_catalogPath );

    // ---------------------------------------------------------- local INDI
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_start, "start" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_abort, "abort" );

    if( createROIndiText( m_indiP_acqState, "acq_state", "state", "Acquisition state", "acq", "state" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText acq_state" } );
    }
    m_indiP_acqState.add( pcf::IndiElement( "message" ) );
    m_indiP_acqState["state"].set( wccAcqStateName( m_acqState ) );
    m_indiP_acqState["message"].set( std::string() );
    registerIndiPropertyReadOnly( m_indiP_acqState );

    if( createROIndiNumber( m_indiP_acqStatus, "acq_status", "Acquisition status", "acq" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber acq_status" } );
    }
    m_indiP_acqStatus.add( pcf::IndiElement( "iteration" ) );
    m_indiP_acqStatus.add( pcf::IndiElement( "guide_err_px" ) );
    m_indiP_acqStatus.add( pcf::IndiElement( "roll_err_px" ) );
    m_indiP_acqStatus.add( pcf::IndiElement( "n_solved" ) );
    m_indiP_acqStatus.add( pcf::IndiElement( "boresight_x" ) );
    m_indiP_acqStatus.add( pcf::IndiElement( "boresight_y" ) );
    m_indiP_acqStatus.add( pcf::IndiElement( "roll_err_deg" ) );
    m_indiP_acqStatus.add( pcf::IndiElement( "solve_rms" ) );
    registerIndiPropertyReadOnly( m_indiP_acqStatus );

    if( createROIndiText( m_indiP_visit, "visit", "target", "Loaded visit", "visit", "target" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText visit" } );
    }
    m_indiP_visit.add( pcf::IndiElement( "guide_sensor" ) );
    m_indiP_visit.add( pcf::IndiElement( "roll_sensor" ) );
    m_indiP_visit.add( pcf::IndiElement( "ra" ) );
    m_indiP_visit.add( pcf::IndiElement( "dec" ) );
    m_indiP_visit.add( pcf::IndiElement( "rollpa" ) );
    m_indiP_visit.add( pcf::IndiElement( "guide_star" ) );
    m_indiP_visit.add( pcf::IndiElement( "roll_star" ) );
    m_indiP_visit.add( pcf::IndiElement( "centroid_guide" ) );
    m_indiP_visit.add( pcf::IndiElement( "centroid_roll" ) );
    registerIndiPropertyReadOnly( m_indiP_visit );

    if( createROIndiNumber( m_indiP_visitParams, "visit_params", "Parameters from the loaded visit",
                            "visit" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber visit_params" } );
    }
    m_indiP_visitParams.add( pcf::IndiElement( "guide_tol_px" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "roll_tol_px" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "max_iterations" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "ta_exptime" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "ta_frame_rate" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "n_config_sensors" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "n_in_solution" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "track_roi_w" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "track_roi_h" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "track_fps" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "track_exptime" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "track_loop_gain" ) );
    m_indiP_visitParams.add( pcf::IndiElement( "track_roll_gain" ) );
    registerIndiPropertyReadOnly( m_indiP_visitParams );

    if( createROIndiNumber( m_indiP_trackStatus, "track_status", "Fast loop status", "track" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber track_status" } );
    }
    m_indiP_trackStatus.add( pcf::IndiElement( "guide_x" ) );
    m_indiP_trackStatus.add( pcf::IndiElement( "guide_y" ) );
    m_indiP_trackStatus.add( pcf::IndiElement( "guide_err_px" ) );
    m_indiP_trackStatus.add( pcf::IndiElement( "roll_x" ) );
    m_indiP_trackStatus.add( pcf::IndiElement( "roll_y" ) );
    m_indiP_trackStatus.add( pcf::IndiElement( "roll_err_px" ) );
    m_indiP_trackStatus.add( pcf::IndiElement( "corrections" ) );
    registerIndiPropertyReadOnly( m_indiP_trackStatus );

    // ------------------------------------------------------------ remote subs
    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        if( registerSensorSubscriptions( sen.get() ) < 0 )
        {
            return -1;
        }
    }

    if( !m_telPosProperty.empty() )
    {
        if( registerIndiPropertySet( m_indiP_telPos, m_telDevice, m_telPosProperty, st_setCallBack_remote ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "failed to subscribe to " + m_telDevice + "." + m_telPosProperty } );
        }

        remoteBinding rb;
        rb.m_what = "telpos";
        m_bindings[m_indiP_telPos.createUniqueKey()] = rb;
    }

    // ------------------------------------------------- visitCtrl subscription
    {
        const std::pair<pcf::IndiProperty *, const char *> vprops[] = {
            { &m_indiP_vTarget, wcc::visitIndi::propTarget },
            { &m_indiP_vTargetInfo, wcc::visitIndi::propTargetInfo },
            { &m_indiP_vGuideStar, wcc::visitIndi::propGuideStar },
            { &m_indiP_vGuideStarInfo, wcc::visitIndi::propGuideStarInfo },
            { &m_indiP_vRollStar, wcc::visitIndi::propRollStar },
            { &m_indiP_vRollStarInfo, wcc::visitIndi::propRollStarInfo },
            { &m_indiP_vAcqParams, wcc::visitIndi::propAcqParams },
            { &m_indiP_vTrackParams, wcc::visitIndi::propTrackParams },
            { &m_indiP_vTrackDevices, wcc::visitIndi::propTrackDevices },
            { &m_indiP_vConfigSensors, wcc::visitIndi::propConfigSensors },
            { &m_indiP_vStatus, wcc::visitIndi::propStatus },
        };

        for( const auto &vp : vprops )
        {
            const std::string devName = m_visitDevice;
            const std::string propName = vp.second;

            if( registerIndiPropertySet( *vp.first, devName, propName, st_setCallBack_remote ) < 0 )
            {
                return log<software_error, -1>(
                    { __FILE__, __LINE__, "failed to subscribe to " + devName + "." + propName } );
            }

            remoteBinding rb;
            rb.m_sensor = nullptr;
            rb.m_what = std::string( "visit_" ) + propName;
            m_bindings[vp.first->createUniqueKey()] = rb;
        }

        log<text_log>( "reading the visit from " + m_visitDevice );
    }

    if( !m_telDoneProperty.empty() )
    {
        if( registerIndiPropertySet( m_indiP_telDone, m_telDevice, m_telDoneProperty, st_setCallBack_remote ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "failed to subscribe to " + m_telDevice + "." + m_telDoneProperty } );
        }

        remoteBinding rb;
        rb.m_what = "teldone";
        m_bindings[m_indiP_telDone.createUniqueKey()] = rb;

        log<text_log>( "telescope motion tracked through " + m_telDevice + "." + m_telDoneProperty );
    }
    else
    {
        log<text_log>( "no telescope done property configured; moves will be timed with a " +
                       std::to_string( m_telSettleTime ) + " s settle" );
    }

    // ------------------------------------------------------------- sequencer
    // There is nothing to load here: the visit arrives asynchronously from
    // visitCtrl and appLogic promotes it once complete.
    m_sequencer = std::thread( sequencerStart, this );

    state( stateCodes::READY );

    log<text_log>( "wccCtrl ready with " + std::to_string( m_sensors.size() ) + " sensors, telescope " +
                   m_telDevice + "." + m_telOffsetProperty );

    return 0;
}

inline int wccCtrl::appLogic()
{
    // Promote a newly arrived visit here rather than in the INDI callback, so the
    // driver thread is never held while sensors are matched and subscriptions added.
    // Only do so between sequences: a visit that changes mid-acquisition would move
    // the target out from under the sequencer.
    {
        const wccAcqState cur = acqState();

        if( cur == wccAcqState::idle || cur == wccAcqState::loaded || cur == wccAcqState::failed ||
            cur == wccAcqState::complete )
        {
            if( !m_visitLoaded && visitComplete() )
            {
                visitFromIndi();
            }
            else if( m_visitLoaded )
            {
                // visitCtrl unloaded or failed: drop the visit rather than run a stale one.
                std::string vs;

                { //mutex scope
                    std::lock_guard<std::mutex> lock( m_selMutex );
                    vs = m_visitState;
                }

                if( vs != wcc::visitIndi::stateLoaded )
                {
                    m_visitLoaded = false;
                    m_haveGuideStar = false;
                    m_haveRollStar = false;

                    setAcqState( wccAcqState::idle, m_visitDevice + " no longer reports a loaded visit" );
                    log<text_log>( m_visitDevice + " no longer reports a loaded visit; dropping it",
                                   logPrio::LOG_NOTICE );
                }
            }
        }
    }

    const wccAcqState s = acqState();

    if( s == wccAcqState::failed )
    {
        state( stateCodes::ERROR );
    }
    else if( s == wccAcqState::tracking || s == wccAcqState::complete )
    {
        state( stateCodes::OPERATING );
    }
    else if( s == wccAcqState::idle || s == wccAcqState::loaded )
    {
        state( stateCodes::READY );
    }
    else
    {
        state( stateCodes::CONFIGURING );
    }

    updateStatus();

    return 0;
}

inline int wccCtrl::appShutdown()
{
    m_shutdownSequencer = true;

    try
    {
        if( m_sequencer.joinable() )
        {
            m_sequencer.join();
        }
    }
    catch( ... )
    {
    }

    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        closeSensorStream( sen.get() );
    }

    return 0;
}

//------------------------------------------------------------------------
// Visit handling
//------------------------------------------------------------------------

inline int wccCtrl::sensorByName( const std::string &name ) const
{
    const std::string want = wcc::normalizeSensorName( name );

    for( size_t i = 0; i < m_sensors.size(); ++i )
    {
        if( wcc::normalizeSensorName( m_sensors[i]->m_name ) == want )
        {
            return static_cast<int>( i );
        }
    }

    return -1;
}

inline bool wccCtrl::visitComplete()
{
    std::lock_guard<std::mutex> lock( m_selMutex );

    // The roll star is optional in principle, but the sequence checks and corrects
    // roll, so require it rather than discovering it is missing mid-sequence.
    return m_visitState == wcc::visitIndi::stateLoaded && m_haveTarget && m_haveGuideIndi &&
           m_haveRollIndi && m_haveAcqParams;
}

inline int wccCtrl::visitFromIndi()
{
    wcc::visitSelection sel;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_selMutex );

        if( m_visitState != wcc::visitIndi::stateLoaded || !m_haveTarget || !m_haveGuideIndi ||
            !m_haveAcqParams )
        {
            return -1;
        }

        m_selection.m_valid = true;
        sel = m_selection;
    }

    // The visit supersedes the configured defaults. Publishing both on visit_params
    // is what makes which one won unambiguous.
    m_guideTolPix = sel.m_guideTolPix;
    m_rollTolPix = sel.m_rollTolPix;
    m_maxIterations = sel.m_maxIterations;

    m_trackROIW = sel.m_tracking.m_roiW;
    m_trackROIH = sel.m_tracking.m_roiH;
    m_trackFps = sel.m_tracking.m_frameRate;
    m_trackExpTime = sel.m_tracking.m_expTime;
    m_trackGain = sel.m_tracking.m_loopGain;
    m_trackRollGain = sel.m_tracking.m_rollGain;

    // Mark which sensors take part in the astrometric solution. An empty list means
    // use every configured sensor.
    m_nInSolution = 0;

    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        if( sel.m_configSensors.empty() )
        {
            sen->m_inSolution = true;
        }
        else
        {
            sen->m_inSolution = false;

            for( const std::string &n : sel.m_configSensors )
            {
                if( wcc::normalizeSensorName( n ) == wcc::normalizeSensorName( sen->m_name ) )
                {
                    sen->m_inSolution = true;
                    break;
                }
            }
        }

        if( sen->m_inSolution )
        {
            ++m_nInSolution;
        }
    }

    if( m_nInSolution == 0 )
    {
        setAcqState( wccAcqState::idle, "none of the visit's sensors matches a configured sensor" );
        m_visitLoaded = false;

        return log<software_error, -1>(
            { __FILE__, __LINE__, "none of the visit's sensors matches a configured sensor" } );
    }

    if( resolveStars() < 0 )
    {
        m_visitLoaded = false;
        return -1;
    }

    // The centroid controller names come from the visit, so they can only be
    // subscribed to now.
    registerCentroidSubscription( sel.m_tracking.m_centroidGuide, true );
    registerCentroidSubscription( sel.m_tracking.m_centroidRoll, false );

    // Predict against the requested pointing until the telescope reports its own.
    m_focalPlane.setPointing( sel.m_ra, sel.m_dec, sel.m_rollPA );

    m_visitLoaded = true;

    setAcqState( wccAcqState::loaded, "visit " + sel.m_targetName + " received from " + m_visitDevice );

    log<text_log>( "visit " + sel.m_targetName + " received from " + m_visitDevice + ": ra " +
                   std::to_string( sel.m_ra ) + " dec " + std::to_string( sel.m_dec ) + " rollPA " +
                   std::to_string( sel.m_rollPA ) + ", rank " + std::to_string( sel.m_rank ) + ", " +
                   std::to_string( m_nInSolution ) + " sensors in the solution, guide star on " +
                   m_sensors[m_guideSensor]->m_name + ", roll star on " + m_sensors[m_rollSensor]->m_name );

    if( m_autoStart )
    {
        m_startRequest = true;
    }

    return 0;
}

inline int wccCtrl::resolveStars()
{
    m_haveGuideStar = false;
    m_haveRollStar = false;
    m_guideSensor = -1;
    m_rollSensor = -1;

    wcc::visitStar guide, roll;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_selMutex );
        guide = m_selection.m_guide;
        roll = m_selection.m_roll;
    }

    const int gi = sensorByName( guide.m_sensor );

    if( gi < 0 )
    {
        setAcqState( wccAcqState::idle, "guide star is on unconfigured sensor " + guide.m_sensor );

        return log<software_error, -1>(
            { __FILE__, __LINE__,
              "guide star is on unconfigured sensor " + guide.m_sensor +
                  "; move visitCtrl select_rank to a pair that fits this array" } );
    }

    const int ri = sensorByName( roll.m_sensor );

    if( ri < 0 )
    {
        setAcqState( wccAcqState::idle, "roll star is on unconfigured sensor " + roll.m_sensor );

        return log<software_error, -1>(
            { __FILE__, __LINE__,
              "roll star is on unconfigured sensor " + roll.m_sensor +
                  "; move visitCtrl select_rank to a pair that fits this array" } );
    }

    m_guideStar = guide;
    m_guideSensor = gi;
    m_haveGuideStar = true;

    m_rollStarSel = roll;
    m_rollSensor = ri;
    m_haveRollStar = true;

    return 0;
}

inline wcc::visitSensorConfig wccCtrl::sensorConfigFor( const std::string &name )
{
    wcc::visitSensorConfig out;
    out.m_name = name;

    std::lock_guard<std::mutex> lock( m_selMutex );

    out.m_expTime = m_selection.m_taExpTime;
    out.m_frameRate = m_selection.m_taFrameRate;
    out.m_gain = -1;   // leave the camera at whatever gain it has
    out.m_stream = true;

    return out;
}
//------------------------------------------------------------------------
// Sequencer
//------------------------------------------------------------------------

inline void wccCtrl::setAcqState( wccAcqState s, const std::string &msg )
{
    bool changed = false;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_stateMutex );
        changed = ( m_acqState != s );
        m_acqState = s;

        if( !msg.empty() )
        {
            m_acqMessage = msg;
        }
    }

    if( changed )
    {
        log<text_log>( "acquisition state -> " + wccAcqStateName( s ) + ( msg.empty() ? "" : ": " + msg ) );
    }
}

inline wccAcqState wccCtrl::acqState()
{
    std::lock_guard<std::mutex> lock( m_stateMutex );
    return m_acqState;
}

inline bool wccCtrl::stopRequested()
{
    return m_abortRequest.load() || m_shutdownSequencer.load() || m_shutdown;
}

inline int wccCtrl::failSequence( const std::string &why )
{
    setAcqState( wccAcqState::failed, why );
    log<software_error>( { __FILE__, __LINE__, "acquisition failed: " + why } );

    return -1;
}

inline void wccCtrl::sequencerStart( wccCtrl *s )
{
    s->sequencerExec();
}

inline void wccCtrl::sequencerExec()
{
    while( !m_shutdownSequencer.load() && !m_shutdown )
    {
        // ------------------------------------------------------- idle waiting
        if( !m_startRequest.exchange( false ) )
        {
            // While tracking, keep the fast loop running instead of idling.
            if( acqState() == wccAcqState::tracking )
            {
                if( trackingIteration() < 0 )
                {
                    failSequence( "fast loop command failed" );
                }

                if( m_abortRequest.exchange( false ) )
                {
                    setAcqState( wccAcqState::loaded, "aborted by request" );
                }

                mx::sys::microSleep( static_cast<unsigned>( std::max( 0.001, m_trackPeriod ) * 1e6 ) );
                continue;
            }

            if( m_abortRequest.exchange( false ) )
            {
                setAcqState( m_visitLoaded ? wccAcqState::loaded : wccAcqState::idle, "aborted by request" );
            }

            mx::sys::milliSleep( 100 );
            continue;
        }

        // --------------------------------------------------- sequence start
        m_abortRequest = false;

        if( !m_visitLoaded )
        {
            failSequence( "no visit loaded" );
            continue;
        }

        if( !m_haveGuideStar || !m_haveRollStar )
        {
            failSequence( "guide or roll star not resolved" );
            continue;
        }

        m_iteration = 0;
        m_guideErrPix = -1;
        m_rollErrPix = -1;

        // 1. Configure every participating camera.
        setAcqState( wccAcqState::configuring, "commanding acquisition configuration" );

        if( configureSensors() < 0 )
        {
            failSequence( "failed to command the acquisition configuration" );
            continue;
        }

        // 2. Confirm the cameras took the configuration.
        setAcqState( wccAcqState::confirming, "waiting for cameras to confirm" );

        if( confirmSensors() < 0 )
        {
            continue; // confirmSensors() already failed the sequence with detail.
        }

        // 3-5. Offset and verify until the guide star is inside tolerance.
        bool guideOK = false;

        while( m_iteration < m_maxIterations && !stopRequested() )
        {
            ++m_iteration;

            setAcqState( wccAcqState::acquiring,
                         "acquiring frames, pass " + std::to_string( m_iteration ) );

            if( acquireFrames() < 1 )
            {
                failSequence( "no sensor produced a frame" );
                break;
            }

            setAcqState( wccAcqState::solving, "solving astrometry" );

            m_nSolved = solveFrames();

            if( m_nSolved < m_minSolvedSensors )
            {
                failSequence( "only " + std::to_string( m_nSolved ) + " sensor(s) solved, need " +
                              std::to_string( m_minSolvedSensors ) );
                break;
            }

            solveBoresightCorrection();

            // The guide star is the thing that has to end up in the right place,
            // so drive the offset from it rather than from the array average.
            double errPix, fx, fy;

            if( measureStarError( m_guideSensor, m_guideStar, errPix, fx, fy ) < 0 )
            {
                failSequence( "guide sensor " + m_sensors[m_guideSensor]->m_name + " did not solve" );
                break;
            }

            m_guideErrPix = errPix;

            log<text_log>( "pass " + std::to_string( m_iteration ) + ": guide star is " +
                           std::to_string( errPix ) + " px from its target pixel on " +
                           m_sensors[m_guideSensor]->m_name + " (tolerance " + std::to_string( m_guideTolPix ) +
                           " px); offset " + std::to_string( fx ) + ", " + std::to_string( fy ) + " arcsec" );

            if( errPix <= m_guideTolPix )
            {
                guideOK = true;
                break;
            }

            setAcqState( wccAcqState::offsetting, "offsetting the telescope" );

            if( offsetTelescope( fx, fy, 0.0 ) < 0 )
            {
                failSequence( "telescope offset failed" );
                break;
            }
        }

        if( stopRequested() )
        {
            setAcqState( m_visitLoaded ? wccAcqState::loaded : wccAcqState::idle, "aborted by request" );
            m_abortRequest = false;
            continue;
        }

        if( acqState() == wccAcqState::failed )
        {
            continue;
        }

        if( !guideOK )
        {
            failSequence( "guide star did not reach tolerance in " + std::to_string( m_maxIterations ) +
                          " passes; last error " + std::to_string( m_guideErrPix ) + " px" );
            continue;
        }

        // 6. Check the roll star and roll if needed.
        bool rollOK = false;

        for( int rollPass = 0; rollPass < m_maxIterations && !stopRequested(); ++rollPass )
        {
            setAcqState( wccAcqState::verifyRoll, "checking the roll star" );

            double errPix, fx, fy;

            if( measureStarError( m_rollSensor, m_rollStarSel, errPix, fx, fy ) < 0 )
            {
                failSequence( "roll sensor " + m_sensors[m_rollSensor]->m_name + " did not solve" );
                break;
            }

            m_rollErrPix = errPix;

            log<text_log>( "roll pass " + std::to_string( rollPass + 1 ) + ": roll star is " +
                           std::to_string( errPix ) + " px from its target pixel on " +
                           m_sensors[m_rollSensor]->m_name + " (tolerance " + std::to_string( m_rollTolPix ) +
                           " px)" );

            if( errPix <= m_rollTolPix )
            {
                rollOK = true;
                break;
            }

            // A pointing error has already been removed, so what is left at a
            // large field radius is roll. Decompose the required field shift into
            // its tangential component about the boresight.
            //
            // A boresight roll of dtheta moves a star at field position r to
            // R(-dtheta) r, i.e. by dtheta * (Yr, -Xr) to first order. Dotting that
            // with the tangential unit vector t = (-Yr, Xr)/|r| gives -dtheta*|r|,
            // so dtheta = -(S . t)/|r| for a required star motion S.
            //
            // fx and fy from measureStarError are the telescope offset, which is
            // the negative of the required star motion, hence the sign below.
            const double rx = m_rollStarSel.m_fieldX;
            const double ry = m_rollStarSel.m_fieldY;
            const double rr = std::hypot( rx, ry );

            if( !( rr > 0 ) )
            {
                failSequence( "roll star sits on the boresight, so roll cannot be measured from it" );
                break;
            }

            const double sx = -fx;
            const double sy = -fy;
            const double tangential = ( -ry * sx + rx * sy ) / rr;
            const double dtheta = -tangential / rr * wcc::rad2deg;

            setAcqState( wccAcqState::rolling, "rolling by " + std::to_string( dtheta ) + " deg" );

            log<text_log>( "roll maneuver " + std::to_string( dtheta ) + " deg from a tangential error of " +
                           std::to_string( tangential ) + " arcsec at field radius " + std::to_string( rr ) +
                           " arcsec" );

            if( offsetTelescope( 0.0, 0.0, dtheta ) < 0 )
            {
                failSequence( "roll maneuver failed" );
                break;
            }

            setAcqState( wccAcqState::acquiring, "re-acquiring after the roll" );

            if( acquireFrames() < 1 )
            {
                failSequence( "no sensor produced a frame after the roll" );
                break;
            }

            setAcqState( wccAcqState::solving, "solving astrometry after the roll" );
            m_nSolved = solveFrames();
            solveBoresightCorrection();
        }

        if( stopRequested() )
        {
            setAcqState( m_visitLoaded ? wccAcqState::loaded : wccAcqState::idle, "aborted by request" );
            m_abortRequest = false;
            continue;
        }

        if( acqState() == wccAcqState::failed )
        {
            continue;
        }

        if( !rollOK )
        {
            failSequence( "roll star did not reach tolerance; last error " + std::to_string( m_rollErrPix ) +
                          " px" );
            continue;
        }

        // 7. Narrow the guide and roll cameras and hand off to the fast loop.
        setAcqState( wccAcqState::reconfiguring, "narrowing to the tracking region of interest" );

        if( reconfigureForTracking() < 0 )
        {
            continue; // reconfigureForTracking() failed the sequence with detail.
        }

        setAcqState( wccAcqState::trackingStart, "enabling the centroid controllers" );

        if( startCentroidControllers() < 0 )
        {
            failSequence( "failed to enable the centroid controllers" );
            continue;
        }

        setAcqState( wccAcqState::tracking, "guiding on centroids" );

        std::string cg, cr;

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_selMutex );
            cg = m_selection.m_tracking.m_centroidGuide;
            cr = m_selection.m_tracking.m_centroidRoll;
        }

        log<text_log>( "acquisition complete: guide error " + std::to_string( m_guideErrPix ) +
                       " px, roll error " + std::to_string( m_rollErrPix ) + " px; tracking on " + cg +
                       " and " + cr );
    }
}

//------------------------------------------------------------------------
// Sequence stages
//------------------------------------------------------------------------

inline int wccCtrl::configureSensors()
{
    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        if( !sen->m_inSolution )
        {
            continue;
        }

        const wcc::visitSensorConfig vsc = sensorConfigFor( sen->m_name );

        if( !vsc.m_stream )
        {
            sen->m_inSolution = false;
            continue;
        }

        // Region of interest: the visit file's, or this sensor's full frame.
        wcc::roiSpec roi;

        if( vsc.m_roi.m_valid )
        {
            roi = vsc.m_roi.toROISpec();
        }
        else
        {
            roi = m_focalPlane.sensor( sen->m_index ).fullFrameROI();
        }

        if( commandROI( sen.get(), roi ) < 0 )
        {
            return -1;
        }

        if( vsc.m_expTime > 0 && commandExpTime( sen.get(), vsc.m_expTime ) < 0 )
        {
            return -1;
        }

        if( vsc.m_frameRate > 0 && commandFps( sen.get(), vsc.m_frameRate ) < 0 )
        {
            return -1;
        }

        if( vsc.m_gain > 0 && commandGain( sen.get(), vsc.m_gain ) < 0 )
        {
            return -1;
        }

        if( commandStreaming( sen.get(), true ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

inline int wccCtrl::confirmSensors()
{
    const double t0 = mx::sys::get_curr_time();

    while( mx::sys::get_curr_time() - t0 < m_confirmTimeout )
    {
        if( stopRequested() )
        {
            return -1;
        }

        std::vector<std::string> pending;

        for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
        {
            if( !sen->m_inSolution )
            {
                continue;
            }

            std::lock_guard<std::mutex> lock( sen->m_mutex );

            if( sen->m_cmdROIValid )
            {
                if( !sen->m_haveROI )
                {
                    pending.push_back( sen->m_name + ".roi(no report)" );
                    continue;
                }

                if( std::fabs( sen->m_roi.m_centerX - sen->m_cmdROI.m_centerX ) > m_roiTolPix ||
                    std::fabs( sen->m_roi.m_centerY - sen->m_cmdROI.m_centerY ) > m_roiTolPix ||
                    std::fabs( static_cast<double>( sen->m_roi.m_w - sen->m_cmdROI.m_w ) ) > m_roiTolPix ||
                    std::fabs( static_cast<double>( sen->m_roi.m_h - sen->m_cmdROI.m_h ) ) > m_roiTolPix )
                {
                    pending.push_back( sen->m_name + ".roi" );
                    continue;
                }
            }

            if( sen->m_cmdExpTime > 0 )
            {
                if( !sen->m_haveExpTime ||
                    std::fabs( sen->m_expTime - sen->m_cmdExpTime ) >
                        m_paramTolFrac * std::fabs( sen->m_cmdExpTime ) )
                {
                    pending.push_back( sen->m_name + ".exptime" );
                    continue;
                }
            }

            if( sen->m_cmdFps > 0 )
            {
                if( !sen->m_haveFps ||
                    std::fabs( sen->m_fps - sen->m_cmdFps ) > m_paramTolFrac * std::fabs( sen->m_cmdFps ) )
                {
                    pending.push_back( sen->m_name + ".fps" );
                    continue;
                }
            }
        }

        if( pending.empty() )
        {
            log<text_log>( "all acquisition sensors confirmed their configuration" );
            return 0;
        }

        mx::sys::milliSleep( 200 );
    }

    // Report exactly what did not confirm, because that is the actionable detail.
    std::string pendingList;

    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        if( !sen->m_inSolution )
        {
            continue;
        }

        std::lock_guard<std::mutex> lock( sen->m_mutex );

        if( sen->m_cmdROIValid && !sen->m_haveROI )
        {
            pendingList += " " + sen->m_name + "(no ROI report)";
        }
        else if( sen->m_cmdROIValid &&
                 ( std::fabs( sen->m_roi.m_centerX - sen->m_cmdROI.m_centerX ) > m_roiTolPix ||
                   std::fabs( static_cast<double>( sen->m_roi.m_w - sen->m_cmdROI.m_w ) ) > m_roiTolPix ) )
        {
            pendingList += " " + sen->m_name + "(ROI " + std::to_string( sen->m_roi.m_w ) + "x" +
                           std::to_string( sen->m_roi.m_h ) + " want " + std::to_string( sen->m_cmdROI.m_w ) +
                           "x" + std::to_string( sen->m_cmdROI.m_h ) + ")";
        }
        else if( sen->m_cmdFps > 0 && !sen->m_haveFps )
        {
            pendingList += " " + sen->m_name + "(no fps report)";
        }
    }

    return failSequence( "cameras did not confirm within " + std::to_string( m_confirmTimeout ) + " s:" +
                         pendingList );
}

inline int wccCtrl::acquireFrames()
{
    int n = 0;

    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        if( !sen->m_inSolution )
        {
            continue;
        }

        if( stopRequested() )
        {
            return n;
        }

        if( grabFrame( sen.get(), m_frameTimeout ) == 0 )
        {
            ++n;
        }
        else
        {
            log<text_log>( "no frame from " + sen->m_name + " on " + sen->m_shmimIn + " within " +
                               std::to_string( m_frameTimeout ) + " s",
                           logPrio::LOG_WARNING );
        }
    }

    return n;
}

inline void wccCtrl::predictPointing( double &ra, double &dec, double &pa )
{
    std::lock_guard<std::mutex> lock( m_telMutex );

    if( m_haveTelPos )
    {
        ra = m_telRA;
        dec = m_telDec;
        pa = m_telPA;
        return;
    }

    // Without a telescope report, the visit file's requested pointing is the best
    // available prior. The solver's search radius absorbs the difference.
    std::lock_guard<std::mutex> slock( m_selMutex );

    ra = m_selection.m_ra;
    dec = m_selection.m_dec;
    pa = m_selection.m_rollPA;
}

inline int wccCtrl::frameWCS( const wccCtrlSensor *sen, wcc::skyWCS &wcs )
{
    return m_focalPlane.roiWCS( sen->m_index, sen->m_frameROI, wcs );
}

inline int wccCtrl::solveFrames()
{
    double ra, dec, pa;
    predictPointing( ra, dec, pa );

    // Only the sequencer thread touches the geometry model's pointing.
    m_focalPlane.setPointing( ra, dec, pa );

    int nSolved = 0;

    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        sen->m_solution = wcc::frameSolution();

        if( !sen->m_inSolution || sen->m_frame.empty() )
        {
            continue;
        }

        wcc::detectSources( sen->m_frame.data(), sen->m_frameW, sen->m_frameH, m_detect, sen->m_detections,
                            &sen->m_background );

        wcc::skyWCS wcs;

        if( frameWCS( sen.get(), wcs ) < 0 )
        {
            continue;
        }

        // Predict from a cone that covers the frame with a margin, so sources just
        // outside can still match a detection near the edge.
        const double radius = m_focalPlane.roiSearchRadius( sen->m_index, sen->m_frameROI, m_detect.m_boxHalf );

        double cra, cdec;
        wcs.pix2world( 0.5 * ( sen->m_frameW - 1 ), 0.5 * ( sen->m_frameH - 1 ), cra, cdec );

        std::vector<size_t> hits;
        m_catalog.coneSearch( cra, cdec, radius, hits, m_predictMagLimit );

        std::vector<double> pra, pdec, pmag;
        pra.reserve( hits.size() );
        pdec.reserve( hits.size() );
        pmag.reserve( hits.size() );

        for( size_t i : hits )
        {
            pra.push_back( m_catalog[i].m_ra );
            pdec.push_back( m_catalog[i].m_dec );
            pmag.push_back( m_catalog[i].m_mag );
        }

        std::vector<wcc::prediction> preds;
        wcc::predictSources( wcs, sen->m_frameW, sen->m_frameH, pra, pdec, pmag, preds, m_solve.m_searchRadius );

        if( wcc::solveFrame( sen->m_detections, preds, m_solve, sen->m_solution ) == 0 )
        {
            ++nSolved;

            log<text_log>( sen->m_name + ": " + std::to_string( sen->m_detections.size() ) + " sources, " +
                           std::to_string( preds.size() ) + " predictions, " +
                           std::to_string( sen->m_solution.m_nMatched ) + " matched, rotation " +
                           std::to_string( sen->m_solution.m_rotation ) + " deg, rms " +
                           std::to_string( sen->m_solution.m_rms ) + " px" );
        }
        else
        {
            log<text_log>( sen->m_name + " did not solve: " + std::to_string( sen->m_detections.size() ) +
                               " sources against " + std::to_string( preds.size() ) + " predictions",
                           logPrio::LOG_WARNING );
        }
    }

    return nSolved;
}

inline void wccCtrl::pixelToFieldShift( size_t iSensor,
                                        double x,
                                        double y,
                                        double dx,
                                        double dy,
                                        double &fieldX,
                                        double &fieldY )
{
    double f0x = 0, f0y = 0, f1x = 0, f1y = 0;

    m_focalPlane.pixelToField( iSensor, x, y, f0x, f0y );
    m_focalPlane.pixelToField( iSensor, x + dx, y + dy, f1x, f1y );

    fieldX = f1x - f0x;
    fieldY = f1y - f0y;
}

inline int wccCtrl::solveBoresightCorrection()
{
    std::vector<wcc::sensorMeasurement> meas;

    for( std::unique_ptr<wccCtrlSensor> &sen : m_sensors )
    {
        if( !sen->m_inSolution || !sen->m_solution.m_valid )
        {
            continue;
        }

        const double cx = 0.5 * ( sen->m_frameW - 1 );
        const double cy = 0.5 * ( sen->m_frameH - 1 );

        double ox, oy;
        sen->m_solution.offsetAt( cx, cy, ox, oy );

        // The frame is a region of interest, so convert to full sensor pixels
        // before asking the geometry model for a field shift.
        const double fullX = sen->m_frameROI.toFullX( cx );
        const double fullY = sen->m_frameROI.toFullY( cy );
        const double fullDX = ox * std::max( sen->m_frameROI.m_binX, 1 );
        const double fullDY = oy * std::max( sen->m_frameROI.m_binY, 1 );

        wcc::sensorMeasurement m;
        m.m_sensor = sen->m_index;
        m.m_fieldX = m_focalPlane.sensor( sen->m_index ).m_fieldX;
        m.m_fieldY = m_focalPlane.sensor( sen->m_index ).m_fieldY;
        m.m_rotation = sen->m_solution.m_rotation;
        m.m_weight = sen->m_solution.m_nMatched;

        pixelToFieldShift( sen->m_index, fullX, fullY, fullDX, fullDY, m.m_shiftX, m.m_shiftY );

        meas.push_back( m );
    }

    if( meas.empty() )
    {
        m_boresight = wcc::boresightSolution();
        return -1;
    }

    if( wcc::solveBoresight( meas, m_boresight ) < 0 )
    {
        return -1;
    }

    log<text_log>( "boresight solution from " + std::to_string( m_boresight.m_nSensors ) + " sensor(s): field " +
                   std::to_string( m_boresight.m_fieldX ) + ", " + std::to_string( m_boresight.m_fieldY ) +
                   " arcsec, roll " + std::to_string( m_boresight.m_roll ) + " deg, rms " +
                   std::to_string( m_boresight.m_rms ) + " arcsec" );

    return 0;
}

inline int wccCtrl::measureStarError( int iSensor,
                                      const wcc::visitStar &star,
                                      double &errPix,
                                      double &fieldX,
                                      double &fieldY )
{
    errPix = -1;
    fieldX = 0;
    fieldY = 0;

    if( iSensor < 0 || static_cast<size_t>( iSensor ) >= m_sensors.size() )
    {
        return -1;
    }

    wccCtrlSensor *sen = m_sensors[iSensor].get();

    if( !sen->m_solution.m_valid || sen->m_frame.empty() )
    {
        return -1;
    }

    wcc::skyWCS wcs;

    if( frameWCS( sen, wcs ) < 0 )
    {
        return -1;
    }

    // Where the catalog says the star should be under the assumed pointing.
    double px, py;

    if( !wcs.world2pix( star.m_ra, star.m_dec, px, py ) )
    {
        return -1;
    }

    // Where the astrometric solution says it actually is. Using the solution
    // rather than the nearest detection means this works even if the guide star
    // is not the brightest thing in the frame.
    double ax, ay;
    sen->m_solution.apply( px, py, ax, ay );

    // The requested target pixel is in full sensor coordinates.
    const wcc::sensorConfig &sc = m_focalPlane.sensor( sen->m_index );
    const double targetFullX = star.targetX( sc );
    const double targetFullY = star.targetY( sc );
    const double targetImgX = sen->m_frameROI.toImageX( targetFullX );
    const double targetImgY = sen->m_frameROI.toImageY( targetFullY );

    const double dxPix = ax - targetImgX;
    const double dyPix = ay - targetImgY;

    errPix = std::hypot( dxPix, dyPix );

    // Sign: a star moves opposite to the boresight. To move the star by
    // (target - actual), the boresight must move by (actual - target), which is
    // exactly the displacement below. Converting through pixelToField picks up the
    // sensor's plate scale and mounting rotation.
    const double fullX = sen->m_frameROI.toFullX( ax );
    const double fullY = sen->m_frameROI.toFullY( ay );
    const double fullDX = dxPix * std::max( sen->m_frameROI.m_binX, 1 );
    const double fullDY = dyPix * std::max( sen->m_frameROI.m_binY, 1 );

    pixelToFieldShift( sen->m_index, fullX, fullY, fullDX, fullDY, fieldX, fieldY );

    return 0;
}

inline int wccCtrl::offsetTelescope( double fieldX, double fieldY, double roll )
{
    if( fieldX != 0 || fieldY != 0 )
    {
        double ox = m_telOffsetSign * fieldX;
        double oy = m_telOffsetSign * fieldY;

        // Some telescope interfaces label their offset axes by the wavefront
        // sensor rather than the focal plane. tcsInterface pyrNudge is one, which
        // psfAcq works around by crossing the assignments.
        if( m_telOffsetSwapXY )
        {
            std::swap( ox, oy );
        }

        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( m_telDevice );
        ip.setName( m_telOffsetProperty );
        ip.add( pcf::IndiElement( m_telOffsetXElement ) );
        ip.add( pcf::IndiElement( m_telOffsetYElement ) );
        ip[m_telOffsetXElement] = ox;
        ip[m_telOffsetYElement] = oy;

        if( sendNewProperty( ip ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "failed to send " + m_telDevice + "." + m_telOffsetProperty } );
        }

        log<text_log>( "sent pointing offset " + std::to_string( ox ) + ", " + std::to_string( oy ) +
                       " arcsec to " + m_telDevice + "." + m_telOffsetProperty );
    }

    if( roll != 0 && !m_telRollProperty.empty() )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( m_telDevice );
        ip.setName( m_telRollProperty );
        ip.add( pcf::IndiElement( m_telRollElement ) );
        ip[m_telRollElement] = roll;

        if( sendNewProperty( ip ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "failed to send " + m_telDevice + "." + m_telRollProperty } );
        }

        log<text_log>( "sent roll offset " + std::to_string( roll ) + " deg to " + m_telDevice + "." +
                       m_telRollProperty );
    }

    setAcqState( wccAcqState::waitTelescope, "waiting for the telescope" );

    return waitForTelescope();
}

inline int wccCtrl::waitForTelescope()
{
    // Without a motion property there is nothing to poll, so fall back to a fixed
    // settle. This is what psfAcq does after a nudge.
    if( m_telDoneProperty.empty() || m_telDoneElements.empty() )
    {
        const double t0 = mx::sys::get_curr_time();

        while( mx::sys::get_curr_time() - t0 < m_telSettleTime )
        {
            if( stopRequested() )
            {
                return -1;
            }

            mx::sys::milliSleep( 50 );
        }

        return 0;
    }

    // Give the telescope a moment to raise its moving flag before watching for it
    // to clear, otherwise a fast poll can see the pre-move idle state and return
    // immediately.
    const double t0 = mx::sys::get_curr_time();

    while( mx::sys::get_curr_time() - t0 < 1.0 )
    {
        if( stopRequested() )
        {
            return -1;
        }

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_telMutex );

            if( m_telMoving )
            {
                break;
            }
        }

        mx::sys::milliSleep( 50 );
    }

    while( mx::sys::get_curr_time() - t0 < m_telMoveTimeout )
    {
        if( stopRequested() )
        {
            return -1;
        }

        bool moving = false;
        bool reported = false;

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_telMutex );
            moving = m_telMoving;
            reported = m_haveTelDone;
        }

        if( reported && !moving )
        {
            // Let the pointing report catch up with the mount before solving.
            mx::sys::milliSleep( static_cast<unsigned>( 1000.0 * std::max( 0.0, m_telSettleTime ) ) );
            return 0;
        }

        mx::sys::milliSleep( 50 );
    }

    return log<software_error, -1>(
        { __FILE__, __LINE__,
          "telescope did not report the move complete within " + std::to_string( m_telMoveTimeout ) + " s" } );
}

inline int wccCtrl::reconfigureForTracking()
{
    struct target
    {
        int m_sensor;
        const wcc::visitStar *m_star;
        const char *m_what;
    };

    const target targets[2] = { { m_guideSensor, &m_guideStar, "guide" },
                                { m_rollSensor, &m_rollStarSel, "roll" } };

    for( const target &t : targets )
    {
        if( t.m_sensor < 0 )
        {
            continue;
        }

        wccCtrlSensor *sen = m_sensors[t.m_sensor].get();
        const wcc::sensorConfig &sc = m_focalPlane.sensor( sen->m_index );

        // Per star overrides win over the visit's tracking block, which in turn
        // wins over the configured defaults.
        const int w = ( t.m_star->m_roiWFG > 0 ) ? t.m_star->m_roiWFG : m_trackROIW;
        const int h = ( t.m_star->m_roiHFG > 0 ) ? t.m_star->m_roiHFG : m_trackROIH;
        const double fps = ( t.m_star->m_frameRateFG > 0 ) ? t.m_star->m_frameRateFG : m_trackFps;
        const double exp = ( t.m_star->m_expTimeFG > 0 ) ? t.m_star->m_expTimeFG : m_trackExpTime;

        wcc::roiSpec roi;
        roi.m_centerX = t.m_star->targetX( sc );
        roi.m_centerY = t.m_star->targetY( sc );
        roi.m_w = std::max( 4, std::min( w, sc.m_fullW ) );
        roi.m_h = std::max( 4, std::min( h, sc.m_fullH ) );
        roi.m_binX = 1;
        roi.m_binY = 1;

        // Keep the window inside the detector, otherwise a target pixel near an
        // edge would produce a ROI the camera has to clamp, and the star would no
        // longer sit where this loop expects it.
        const double halfW = 0.5 * ( roi.m_w - 1 );
        const double halfH = 0.5 * ( roi.m_h - 1 );
        roi.m_centerX = std::min( std::max( roi.m_centerX, halfW ), sc.m_fullW - 1 - halfW );
        roi.m_centerY = std::min( std::max( roi.m_centerY, halfH ), sc.m_fullH - 1 - halfH );

        if( commandROI( sen, roi ) < 0 || commandExpTime( sen, exp ) < 0 || commandFps( sen, fps ) < 0 )
        {
            return failSequence( std::string( "failed to command the tracking configuration onto the " ) +
                                 t.m_what + " sensor " + sen->m_name );
        }

        // Where the star will sit inside the new window.
        const double tgtX = roi.toImageX( t.m_star->targetX( sc ) );
        const double tgtY = roi.toImageY( t.m_star->targetY( sc ) );

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_trackMutex );

            if( t.m_sensor == m_guideSensor )
            {
                m_trackGuideTargetX = tgtX;
                m_trackGuideTargetY = tgtY;
            }

            if( t.m_sensor == m_rollSensor )
            {
                m_trackRollTargetX = tgtX;
                m_trackRollTargetY = tgtY;
            }
        }

        log<text_log>( std::string( t.m_what ) + " sensor " + sen->m_name + " narrowed to " +
                       std::to_string( roi.m_w ) + "x" + std::to_string( roi.m_h ) + " at " +
                       std::to_string( roi.m_centerX ) + ", " + std::to_string( roi.m_centerY ) + " at " +
                       std::to_string( fps ) + " Hz; star target pixel in window is " + std::to_string( tgtX ) +
                       ", " + std::to_string( tgtY ) );

        // The window changed, so the old stream handle is the wrong size.
        closeSensorStream( sen );
    }

    if( confirmSensors() < 0 )
    {
        return -1;
    }

    // Reset the centroid state so the fast loop waits for fresh reports rather
    // than acting on centroids measured in the old, much larger window.
    { //mutex scope
        std::lock_guard<std::mutex> lock( m_trackMutex );
        m_haveGuideCentroid = false;
        m_haveRollCentroid = false;
        m_trackCorrections = 0;
    }

    return 0;
}

inline int wccCtrl::startCentroidControllers()
{
    std::string devs[2];

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_selMutex );
        devs[0] = m_selection.m_tracking.m_centroidGuide;
        devs[1] = m_selection.m_tracking.m_centroidRoll;
    }

    for( const std::string &dev : devs )
    {
        if( dev.empty() )
        {
            continue;
        }

        if( !m_centroidEnableProperty.empty() )
        {
            if( sendNewStandardIndiToggle( dev, m_centroidEnableProperty, true ) < 0 )
            {
                return log<software_error, -1>(
                    { __FILE__, __LINE__,
                      "failed to enable " + dev + "." + m_centroidEnableProperty } );
            }

            log<text_log>( "enabled centroid controller " + dev );
        }
    }

    if( devs[0].empty() && devs[1].empty() )
    {
        log<text_log>( "the visit file names no centroid controllers, so the fast loop will idle until "
                       "TRACKING.CENTROID_DEVICE_GUIDE and CENTROID_DEVICE_ROLL are supplied",
                       logPrio::LOG_WARNING );
    }

    return 0;
}

inline int wccCtrl::trackingIteration()
{
    if( m_guideSensor < 0 )
    {
        return 0;
    }

    double gx = 0, gy = 0, rx = 0, ry = 0, gtx = 0, gty = 0, rtx = 0, rty = 0;
    bool haveG = false, haveR = false;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_trackMutex );
        haveG = m_haveGuideCentroid;
        haveR = m_haveRollCentroid;
        gx = m_guideCentroidX;
        gy = m_guideCentroidY;
        rx = m_rollCentroidX;
        ry = m_rollCentroidY;
        gtx = m_trackGuideTargetX;
        gty = m_trackGuideTargetY;
        rtx = m_trackRollTargetX;
        rty = m_trackRollTargetY;
    }

    if( !haveG )
    {
        return 0;
    }

    // ------------------------------------------------------------- pointing
    wccCtrlSensor *gsen = m_sensors[m_guideSensor].get();

    const wcc::roiSpec groi = reportedROI( gsen );

    const double edx = gx - gtx;
    const double edy = gy - gty;
    const double errPix = std::hypot( edx, edy );

    // Raw field error implied by the guide centroid, before any gain. The roll
    // term needs this ungained value, so keep it separate from the command.
    double guideFieldX = 0, guideFieldY = 0;

    if( errPix > m_trackDeadband )
    {
        // Same sign convention as measureStarError: the boresight moves by the
        // star's displacement from its target.
        pixelToFieldShift( gsen->m_index, groi.toFullX( gx ), groi.toFullY( gy ),
                           edx * std::max( groi.m_binX, 1 ), edy * std::max( groi.m_binY, 1 ), guideFieldX,
                           guideFieldY );
    }

    double fieldX = m_trackGain * guideFieldX;
    double fieldY = m_trackGain * guideFieldY;

    if( std::hypot( fieldX, fieldY ) > m_trackMaxOffset )
    {
        log<text_log>( "rejecting a fast loop offset of " + std::to_string( std::hypot( fieldX, fieldY ) ) +
                           " arcsec, above track.max_offset_arcsec",
                       logPrio::LOG_WARNING );
        fieldX = 0;
        fieldY = 0;
    }

    // ----------------------------------------------------------------- roll
    double roll = 0;

    if( haveR && m_rollSensor >= 0 )
    {
        wccCtrlSensor *rsen = m_sensors[m_rollSensor].get();

        const wcc::roiSpec rroi = reportedROI( rsen );

        const double rdx = rx - rtx;
        const double rdy = ry - rty;

        if( std::hypot( rdx, rdy ) > m_trackDeadband )
        {
            double rfx = 0, rfy = 0;

            pixelToFieldShift( rsen->m_index, rroi.toFullX( rx ), rroi.toFullY( ry ),
                               rdx * std::max( rroi.m_binX, 1 ), rdy * std::max( rroi.m_binY, 1 ), rfx, rfy );

            // Subtract the common pointing error measured on the guide sensor. What
            // remains is the differential motion between the two sensors, which is
            // what a roll error produces.
            const double dfx = rfx - guideFieldX;
            const double dfy = rfy - guideFieldY;

            const double fx = m_rollStarSel.m_fieldX;
            const double fy = m_rollStarSel.m_fieldY;
            const double rr = std::hypot( fx, fy );

            if( rr > 0 )
            {
                // See the roll derivation in sequencerExec(): dtheta is the
                // tangential component divided by the field radius.
                const double tangential = ( -fy * dfx + fx * dfy ) / rr;
                roll = m_trackRollGain * tangential / rr * wcc::rad2deg;
            }
        }
    }

    if( fieldX == 0 && fieldY == 0 && roll == 0 )
    {
        return 0;
    }

    if( offsetTelescope( fieldX, fieldY, roll ) < 0 )
    {
        return -1;
    }

    // offsetTelescope() moves the sequence into waitTelescope; the fast loop owns
    // the state while tracking, so put it back.
    setAcqState( wccAcqState::tracking );

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_trackMutex );
        ++m_trackCorrections;
    }

    return 0;
}

//------------------------------------------------------------------------
// Camera commands
//------------------------------------------------------------------------

inline int wccCtrl::sendNumber( const std::string &device,
                                const std::string &property,
                                const std::string &element,
                                double value )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( property );
    ip.add( pcf::IndiElement( element ) );
    ip[element] = value;

    if( sendNewProperty( ip ) < 0 )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to send " + device + "." + property + "." + element } );
    }

    return 0;
}

inline int wccCtrl::sendRequest( const std::string &device, const std::string &property )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( property );
    ip.add( pcf::IndiElement( "request" ) );
    ip["request"].setSwitchState( pcf::IndiElement::On );

    if( sendNewProperty( ip ) < 0 )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to send request " + device + "." + property } );
    }

    return 0;
}

inline int wccCtrl::commandROI( wccCtrlSensor *sen, const wcc::roiSpec &roi )
{
    if( sendNumber( sen->m_indiDevice, "roi_region_x", "target", roi.m_centerX ) < 0 ||
        sendNumber( sen->m_indiDevice, "roi_region_y", "target", roi.m_centerY ) < 0 ||
        sendNumber( sen->m_indiDevice, "roi_region_w", "target", roi.m_w ) < 0 ||
        sendNumber( sen->m_indiDevice, "roi_region_h", "target", roi.m_h ) < 0 ||
        sendNumber( sen->m_indiDevice, "roi_region_bin_x", "target", roi.m_binX ) < 0 ||
        sendNumber( sen->m_indiDevice, "roi_region_bin_y", "target", roi.m_binY ) < 0 )
    {
        return -1;
    }

    // dev::stdCamera only applies the staged region of interest on roi_set.
    if( sendRequest( sen->m_indiDevice, "roi_set" ) < 0 )
    {
        return -1;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( sen->m_mutex );
        sen->m_cmdROI = roi;
        sen->m_cmdROIValid = true;
        // Force a fresh report rather than confirming against a stale one.
        sen->m_haveROI = false;
        sen->m_roiFields = 0;
    }

    return 0;
}

inline int wccCtrl::commandFps( wccCtrlSensor *sen, double fps )
{
    if( !( fps > 0 ) )
    {
        return 0;
    }

    if( sendNumber( sen->m_indiDevice, "fps", "target", fps ) < 0 )
    {
        return -1;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( sen->m_mutex );
        sen->m_cmdFps = fps;
        sen->m_haveFps = false;
    }

    return 0;
}

inline int wccCtrl::commandExpTime( wccCtrlSensor *sen, double expTime )
{
    if( !( expTime > 0 ) )
    {
        return 0;
    }

    if( sendNumber( sen->m_indiDevice, "exptime", "target", expTime ) < 0 )
    {
        return -1;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( sen->m_mutex );
        sen->m_cmdExpTime = expTime;
        sen->m_haveExpTime = false;
    }

    return 0;
}

inline int wccCtrl::commandGain( wccCtrlSensor *sen, double gain )
{
    if( !( gain > 0 ) )
    {
        return 0;
    }

    return sendNumber( sen->m_indiDevice, "emgain", "target", gain );
}

inline int wccCtrl::commandStreaming( wccCtrlSensor *sen, bool on )
{
    if( sendNewStandardIndiToggle( sen->m_indiDevice, "streaming", on ) < 0 )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to toggle streaming on " + sen->m_indiDevice } );
    }

    return 0;
}

//------------------------------------------------------------------------
// Frame handling
//------------------------------------------------------------------------

inline int wccCtrl::openSensorStream( wccCtrlSensor *sen )
{
    if( sen->m_streamOpen )
    {
        return 0;
    }

    if( ImageStreamIO_openIm( &sen->m_stream, sen->m_shmimIn.c_str() ) != IMAGESTREAMIO_SUCCESS )
    {
        return -1;
    }

    sen->m_streamOpen = true;
    sen->m_semIndex = ImageStreamIO_getsemwaitindex( &sen->m_stream, 0 );

    if( sen->m_semIndex >= 0 )
    {
        ImageStreamIO_semflush( &sen->m_stream, sen->m_semIndex );
    }

    return 0;
}

inline void wccCtrl::closeSensorStream( wccCtrlSensor *sen )
{
    if( !sen->m_streamOpen )
    {
        return;
    }

    ImageStreamIO_closeIm( &sen->m_stream );
    sen->m_streamOpen = false;
    sen->m_semIndex = -1;
}

inline int wccCtrl::grabFrame( wccCtrlSensor *sen, double timeout )
{
    const double t0 = mx::sys::get_curr_time();

    while( !sen->m_streamOpen )
    {
        if( openSensorStream( sen ) == 0 )
        {
            break;
        }

        if( mx::sys::get_curr_time() - t0 > timeout || stopRequested() )
        {
            return -1;
        }

        mx::sys::milliSleep( 100 );
    }

    if( sen->m_stream.md == nullptr )
    {
        closeSensorStream( sen );
        return -1;
    }

    // Wait for a frame posted after this call, so a stale frame from before a
    // reconfiguration is never used.
    if( sen->m_semIndex >= 0 )
    {
        ImageStreamIO_semflush( &sen->m_stream, sen->m_semIndex );

        bool got = false;

        while( mx::sys::get_curr_time() - t0 < timeout )
        {
            if( stopRequested() )
            {
                return -1;
            }

            timespec ts{};

            if( clock_gettime( CLOCK_REALTIME, &ts ) < 0 )
            {
                return -1;
            }

            ts.tv_sec += 1;

            if( ImageStreamIO_semtimedwait( &sen->m_stream, sen->m_semIndex, &ts ) == 0 )
            {
                got = true;
                break;
            }
        }

        if( !got )
        {
            return -1;
        }
    }
    else
    {
        // No semaphore available: poll the counter instead.
        const uint64_t start = sen->m_stream.md->cnt0;

        while( sen->m_stream.md->cnt0 == start )
        {
            if( mx::sys::get_curr_time() - t0 > timeout || stopRequested() )
            {
                return -1;
            }

            mx::sys::milliSleep( 5 );
        }
    }

    while( sen->m_stream.md->write )
        ;

    const int w = static_cast<int>( sen->m_stream.md->size[0] );
    const int h = ( sen->m_stream.md->naxis > 1 ) ? static_cast<int>( sen->m_stream.md->size[1] ) : 1;

    if( w < 1 || h < 1 )
    {
        return -1;
    }

    const size_t npix = static_cast<size_t>( w ) * static_cast<size_t>( h );

    // Locate the live slice of a circular buffer.
    size_t slice = 0;

    if( sen->m_stream.md->naxis > 2 && sen->m_stream.md->size[2] > 1 )
    {
        slice = sen->m_stream.md->cnt1 % sen->m_stream.md->size[2];
    }

    sen->m_frame.resize( npix );

    const size_t offset = slice * npix;

    switch( sen->m_stream.md->datatype )
    {
    case _DATATYPE_UINT16:
    {
        const uint16_t *p = sen->m_stream.array.UI16 + offset;
        for( size_t i = 0; i < npix; ++i )
        {
            sen->m_frame[i] = static_cast<float>( p[i] );
        }
        break;
    }
    case _DATATYPE_INT16:
    {
        const int16_t *p = sen->m_stream.array.SI16 + offset;
        for( size_t i = 0; i < npix; ++i )
        {
            sen->m_frame[i] = static_cast<float>( p[i] );
        }
        break;
    }
    case _DATATYPE_UINT8:
    {
        const uint8_t *p = sen->m_stream.array.UI8 + offset;
        for( size_t i = 0; i < npix; ++i )
        {
            sen->m_frame[i] = static_cast<float>( p[i] );
        }
        break;
    }
    case _DATATYPE_FLOAT:
    {
        const float *p = sen->m_stream.array.F + offset;
        memcpy( sen->m_frame.data(), p, npix * sizeof( float ) );
        break;
    }
    case _DATATYPE_DOUBLE:
    {
        const double *p = sen->m_stream.array.D + offset;
        for( size_t i = 0; i < npix; ++i )
        {
            sen->m_frame[i] = static_cast<float>( p[i] );
        }
        break;
    }
    default:
        return log<software_error, -1>( { __FILE__, __LINE__,
                                          "unsupported pixel type on " + sen->m_shmimIn + ": " +
                                              std::to_string( sen->m_stream.md->datatype ) } );
    }

    sen->m_frameW = w;
    sen->m_frameH = h;

    // Record the geometry this frame was taken under, preferring what the camera
    // reports over what was commanded.
    { //mutex scope
        std::lock_guard<std::mutex> lock( sen->m_mutex );
        sen->m_frameROI = sen->m_haveROI ? sen->m_roi : sen->m_cmdROI;
    }

    // The stream dimensions are authoritative; if they disagree with the reported
    // region of interest then the report is stale and a centered window of the
    // right size is the best available interpretation.
    if( sen->m_frameROI.imageW() != w || sen->m_frameROI.imageH() != h )
    {
        const wcc::sensorConfig &sc = m_focalPlane.sensor( sen->m_index );

        log<text_log>( sen->m_name + " stream is " + std::to_string( w ) + "x" + std::to_string( h ) +
                           " but the reported ROI is " + std::to_string( sen->m_frameROI.imageW() ) + "x" +
                           std::to_string( sen->m_frameROI.imageH() ) + "; assuming a centered window",
                       logPrio::LOG_WARNING );

        sen->m_frameROI.m_binX = 1;
        sen->m_frameROI.m_binY = 1;
        sen->m_frameROI.m_w = w;
        sen->m_frameROI.m_h = h;
        sen->m_frameROI.m_centerX = sc.centerX();
        sen->m_frameROI.m_centerY = sc.centerY();
    }

    return 0;
}

//------------------------------------------------------------------------
// INDI
//------------------------------------------------------------------------

inline int wccCtrl::registerSensorSubscriptions( wccCtrlSensor *sen )
{
    const std::pair<const char *, const char *> props[] = {
        { "fps", "fps" },                    { "exptime", "exptime" },
        { "roi_region_x", "roi_x" },         { "roi_region_y", "roi_y" },
        { "roi_region_w", "roi_w" },         { "roi_region_h", "roi_h" },
        { "roi_region_bin_x", "roi_bin_x" }, { "roi_region_bin_y", "roi_bin_y" },
    };

    for( const auto &p : props )
    {
        std::shared_ptr<pcf::IndiProperty> ip( new pcf::IndiProperty );

        const std::string devName = sen->m_indiDevice;
        const std::string propName = p.first;

        if( registerIndiPropertySet( *ip, devName, propName, st_setCallBack_remote ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__, "failed to subscribe to " + devName + "." + propName } );
        }

        remoteBinding rb;
        rb.m_sensor = sen;
        rb.m_what = p.second;
        m_bindings[ip->createUniqueKey()] = rb;

        m_remoteProps.push_back( ip );
    }

    return 0;
}

inline int wccCtrl::registerCentroidSubscription( const std::string &device, bool isGuide )
{
    if( device.empty() )
    {
        return 0;
    }

    for( const std::string &d : m_centroidSubscribed )
    {
        if( d == device )
        {
            return 0;
        }
    }

    std::shared_ptr<pcf::IndiProperty> ip( new pcf::IndiProperty );

    if( registerIndiPropertySet( *ip, device, m_centroidProperty, st_setCallBack_remote ) < 0 )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to subscribe to " + device + "." + m_centroidProperty } );
    }

    remoteBinding rb;
    rb.m_sensor = nullptr;
    rb.m_what = isGuide ? "centroid_guide" : "centroid_roll";
    m_bindings[ip->createUniqueKey()] = rb;

    m_remoteProps.push_back( ip );
    m_centroidSubscribed.push_back( device );

    log<text_log>( "subscribed to centroid controller " + device + "." + m_centroidProperty + " for the " +
                   ( isGuide ? "guide" : "roll" ) + " sensor" );

    return 0;
}

inline wcc::roiSpec wccCtrl::reportedROI( wccCtrlSensor *sen )
{
    std::lock_guard<std::mutex> lock( sen->m_mutex );

    return sen->m_haveROI ? sen->m_roi : sen->m_cmdROI;
}

inline int wccCtrl::st_setCallBack_remote( void *app, const pcf::IndiProperty &ipRecv )
{
    return static_cast<wccCtrl *>( app )->setCallBack_remote( ipRecv );
}

inline bool wccCtrl::elementValue( const pcf::IndiProperty &ip, const std::string &el, double &out )
{
    try
    {
        if( !ip.find( el ) )
        {
            return false;
        }

        const std::string s = ip[el].getValue();

        if( s.empty() )
        {
            return false;
        }

        char *end = nullptr;
        const double v = std::strtod( s.c_str(), &end );

        if( end == s.c_str() || !wcc::isFinite( v ) )
        {
            return false;
        }

        out = v;

        return true;
    }
    catch( ... )
    {
        return false;
    }
}

inline int wccCtrl::setCallBack_remote( const pcf::IndiProperty &ipRecv )
{
    auto it = m_bindings.find( ipRecv.createUniqueKey() );

    if( it == m_bindings.end() )
    {
        return 0;
    }

    const remoteBinding &rb = it->second;

    // -------------------------------------------------------- visit from INDI
    // The visit is mirrored property by property as it arrives. appLogic waits for
    // the set to be complete before promoting it, so a partially delivered visit is
    // never acted on.
    if( rb.m_what.rfind( "visit_", 0 ) == 0 )
    {
        using namespace wcc::visitIndi;

        const std::string which = rb.m_what.substr( 6 );

        auto readStar = [&]( wcc::visitStar &star ) {
            double v = 0;

            if( elementValue( ipRecv, elRank, v ) )
            {
                star.m_rank = static_cast<int>( v );
            }
            elementValue( ipRecv, elRA, star.m_ra );
            elementValue( ipRecv, elDec, star.m_dec );
            elementValue( ipRecv, elFieldX, star.m_fieldX );
            elementValue( ipRecv, elFieldY, star.m_fieldY );
            elementValue( ipRecv, elMag, star.m_mag );
            elementValue( ipRecv, elTargetX, star.m_targetX );
            elementValue( ipRecv, elTargetY, star.m_targetY );
            elementValue( ipRecv, elExpTime, star.m_expTimeFG );
            elementValue( ipRecv, elFrameRate, star.m_frameRateFG );

            if( elementValue( ipRecv, elROIW, v ) )
            {
                star.m_roiWFG = static_cast<int>( v );
            }
            if( elementValue( ipRecv, elROIH, v ) )
            {
                star.m_roiHFG = static_cast<int>( v );
            }
        };

        auto readText = [&]( const std::string &el, std::string &into ) {
            try
            {
                if( ipRecv.find( el ) )
                {
                    into = ipRecv[el].getValue();
                }
            }
            catch( ... )
            {
            }
        };

        std::lock_guard<std::mutex> lock( m_selMutex );

        if( which == propTarget )
        {
            elementValue( ipRecv, elRA, m_selection.m_ra );
            elementValue( ipRecv, elDec, m_selection.m_dec );
            elementValue( ipRecv, elRollPA, m_selection.m_rollPA );
            m_haveTarget = true;
        }
        else if( which == propTargetInfo )
        {
            readText( elName, m_selection.m_targetName );
            readText( elProgID, m_selection.m_programID );
            readText( elObsID, m_selection.m_obsID );
            readText( elVisitID, m_selection.m_visitID );
        }
        else if( which == propGuideStar )
        {
            readStar( m_selection.m_guide );
            m_selection.m_rank = m_selection.m_guide.m_rank;
            m_haveGuideIndi = true;
        }
        else if( which == propGuideStarInfo )
        {
            readText( elID, m_selection.m_guide.m_id );
            readText( elSensor, m_selection.m_guide.m_sensor );
            readText( elCatalog, m_selection.m_guide.m_catalogName );
        }
        else if( which == propRollStar )
        {
            readStar( m_selection.m_roll );
            m_haveRollIndi = true;
        }
        else if( which == propRollStarInfo )
        {
            readText( elID, m_selection.m_roll.m_id );
            readText( elSensor, m_selection.m_roll.m_sensor );
            readText( elCatalog, m_selection.m_roll.m_catalogName );
        }
        else if( which == propAcqParams )
        {
            double v = 0;
            elementValue( ipRecv, elTAExpTime, m_selection.m_taExpTime );
            elementValue( ipRecv, elTAFrameRate, m_selection.m_taFrameRate );
            elementValue( ipRecv, elGuideTol, m_selection.m_guideTolPix );
            elementValue( ipRecv, elRollTol, m_selection.m_rollTolPix );

            if( elementValue( ipRecv, elMaxIter, v ) )
            {
                m_selection.m_maxIterations = std::max( 1, static_cast<int>( v ) );
            }

            m_haveAcqParams = true;
        }
        else if( which == propTrackParams )
        {
            double v = 0;

            if( elementValue( ipRecv, elROIW, v ) && v >= 4 )
            {
                m_selection.m_tracking.m_roiW = static_cast<int>( v );
            }
            if( elementValue( ipRecv, elROIH, v ) && v >= 4 )
            {
                m_selection.m_tracking.m_roiH = static_cast<int>( v );
            }

            elementValue( ipRecv, elFrameRate, m_selection.m_tracking.m_frameRate );
            elementValue( ipRecv, elExpTime, m_selection.m_tracking.m_expTime );
            elementValue( ipRecv, elLoopGain, m_selection.m_tracking.m_loopGain );
            elementValue( ipRecv, elRollGain, m_selection.m_tracking.m_rollGain );
        }
        else if( which == propTrackDevices )
        {
            readText( elCentroidGuide, m_selection.m_tracking.m_centroidGuide );
            readText( elCentroidRoll, m_selection.m_tracking.m_centroidRoll );
        }
        else if( which == propConfigSensors )
        {
            std::string list;
            readText( elList, list );
            m_selection.setConfigSensorList( list );
        }
        else if( which == propStatus )
        {
            readText( elState, m_visitState );
        }

        return 0;
    }

    // --------------------------------------------------------- telescope pos
    if( rb.m_what == "telpos" )
    {
        double ra = 0, dec = 0, pa = 0;
        const bool haveRA = elementValue( ipRecv, m_telRAElement, ra );
        const bool haveDec = elementValue( ipRecv, m_telDecElement, dec );
        const bool havePA = elementValue( ipRecv, m_telPAElement, pa );

        std::lock_guard<std::mutex> lock( m_telMutex );

        if( haveRA )
        {
            m_telRA = ra;
        }
        if( haveDec )
        {
            m_telDec = dec;
        }
        if( havePA )
        {
            m_telPA = pa;
        }

        if( haveRA && haveDec )
        {
            m_haveTelPos = true;
        }

        return 0;
    }

    // -------------------------------------------------------- telescope done
    if( rb.m_what == "teldone" )
    {
        bool moving = false;

        for( const std::string &el : m_telDoneElements )
        {
            double v = 0;

            if( elementValue( ipRecv, el, v ) && v != 0 )
            {
                moving = true;
                break;
            }
        }

        std::lock_guard<std::mutex> lock( m_telMutex );
        m_telMoving = moving;
        m_haveTelDone = true;

        return 0;
    }

    // -------------------------------------------------------------- centroid
    if( rb.m_what == "centroid_guide" || rb.m_what == "centroid_roll" )
    {
        double x = 0, y = 0;

        const bool haveX = elementValue( ipRecv, m_centroidXElement, x );
        const bool haveY = elementValue( ipRecv, m_centroidYElement, y );

        if( !haveX || !haveY )
        {
            return 0;
        }

        std::lock_guard<std::mutex> lock( m_trackMutex );

        if( rb.m_what == "centroid_guide" )
        {
            m_guideCentroidX = x;
            m_guideCentroidY = y;
            m_haveGuideCentroid = true;
        }
        else
        {
            m_rollCentroidX = x;
            m_rollCentroidY = y;
            m_haveRollCentroid = true;
        }

        return 0;
    }

    // ---------------------------------------------------------------- camera
    if( rb.m_sensor == nullptr )
    {
        return 0;
    }

    double v = 0;

    if( !elementValue( ipRecv, "current", v ) )
    {
        return 0;
    }

    wccCtrlSensor *sen = rb.m_sensor;

    std::lock_guard<std::mutex> lock( sen->m_mutex );

    if( rb.m_what == "fps" )
    {
        sen->m_fps = v;
        sen->m_haveFps = true;
    }
    else if( rb.m_what == "exptime" )
    {
        sen->m_expTime = v;
        sen->m_haveExpTime = true;
    }
    else if( rb.m_what == "roi_x" )
    {
        sen->m_roi.m_centerX = v;
        sen->m_roiFields |= roiFieldX;
    }
    else if( rb.m_what == "roi_y" )
    {
        sen->m_roi.m_centerY = v;
        sen->m_roiFields |= roiFieldY;
    }
    else if( rb.m_what == "roi_w" )
    {
        sen->m_roi.m_w = static_cast<int>( v );
        sen->m_roiFields |= roiFieldW;
    }
    else if( rb.m_what == "roi_h" )
    {
        sen->m_roi.m_h = static_cast<int>( v );
        sen->m_roiFields |= roiFieldH;
    }
    else if( rb.m_what == "roi_bin_x" )
    {
        sen->m_roi.m_binX = std::max( 1, static_cast<int>( v ) );
    }
    else if( rb.m_what == "roi_bin_y" )
    {
        sen->m_roi.m_binY = std::max( 1, static_cast<int>( v ) );
    }

    // A region of interest is only usable once all four of its elements report.
    if( ( sen->m_roiFields & roiFieldAll ) == roiFieldAll )
    {
        sen->m_haveROI = true;
    }

    return 0;
}

inline void wccCtrl::updateStatus()
{
    std::unique_lock<std::mutex> lock( m_indiMutex, std::try_to_lock );

    if( !lock.owns_lock() )
    {
        return;
    }

    wccAcqState s;
    std::string msg;
    int iteration, nSolved;
    double guideErr, rollErr;
    wcc::boresightSolution bs;

    { //mutex scope
        std::lock_guard<std::mutex> slock( m_stateMutex );
        s = m_acqState;
        msg = m_acqMessage;
        iteration = m_iteration;
        nSolved = m_nSolved;
        guideErr = m_guideErrPix;
        rollErr = m_rollErrPix;
        bs = m_boresight;
    }

    updateIfChanged( m_indiP_acqState, "state", wccAcqStateName( s ) );
    updateIfChanged( m_indiP_acqState, "message", msg );

    updateIfChanged( m_indiP_acqStatus, "iteration", static_cast<double>( iteration ) );
    updateIfChanged( m_indiP_acqStatus, "guide_err_px", guideErr );
    updateIfChanged( m_indiP_acqStatus, "roll_err_px", rollErr );
    updateIfChanged( m_indiP_acqStatus, "n_solved", static_cast<double>( nSolved ) );
    updateIfChanged( m_indiP_acqStatus, "boresight_x", bs.m_fieldX );
    updateIfChanged( m_indiP_acqStatus, "boresight_y", bs.m_fieldY );
    updateIfChanged( m_indiP_acqStatus, "roll_err_deg", bs.m_roll );
    updateIfChanged( m_indiP_acqStatus, "solve_rms", bs.m_rms );

    if( m_visitLoaded )
    {
        wcc::visitSelection sel;

        { //mutex scope
            std::lock_guard<std::mutex> slock2( m_selMutex );
            sel = m_selection;
        }

        updateIfChanged( m_indiP_visit, "target", sel.m_targetName );
        updateIfChanged( m_indiP_visit, "guide_sensor",
                         m_guideSensor >= 0 ? m_sensors[m_guideSensor]->m_name : std::string( "none" ) );
        updateIfChanged( m_indiP_visit, "roll_sensor",
                         m_rollSensor >= 0 ? m_sensors[m_rollSensor]->m_name : std::string( "none" ) );
        updateIfChanged( m_indiP_visit, "ra", std::to_string( sel.m_ra ) );
        updateIfChanged( m_indiP_visit, "dec", std::to_string( sel.m_dec ) );
        updateIfChanged( m_indiP_visit, "rollpa", std::to_string( sel.m_rollPA ) );
        updateIfChanged( m_indiP_visit, "guide_star", m_haveGuideStar ? m_guideStar.m_id : std::string( "none" ) );
        updateIfChanged( m_indiP_visit, "roll_star",
                         m_haveRollStar ? m_rollStarSel.m_id : std::string( "none" ) );
        updateIfChanged( m_indiP_visit, "centroid_guide", sel.m_tracking.m_centroidGuide );
        updateIfChanged( m_indiP_visit, "centroid_roll", sel.m_tracking.m_centroidRoll );

        // These are the values the running sequence is actually using. They come
        // from the visit when it supplies them and from configuration otherwise, so
        // publishing them removes any ambiguity about which won.
        updateIfChanged( m_indiP_visitParams, "guide_tol_px", m_guideTolPix );
        updateIfChanged( m_indiP_visitParams, "roll_tol_px", m_rollTolPix );
        updateIfChanged( m_indiP_visitParams, "max_iterations", static_cast<double>( m_maxIterations ) );
        updateIfChanged( m_indiP_visitParams, "ta_exptime", sel.m_taExpTime );
        updateIfChanged( m_indiP_visitParams, "ta_frame_rate", sel.m_taFrameRate );
        updateIfChanged( m_indiP_visitParams, "n_config_sensors",
                         static_cast<double>( sel.m_configSensors.size() ) );
        updateIfChanged( m_indiP_visitParams, "n_in_solution", static_cast<double>( m_nInSolution ) );
        updateIfChanged( m_indiP_visitParams, "track_roi_w", static_cast<double>( m_trackROIW ) );
        updateIfChanged( m_indiP_visitParams, "track_roi_h", static_cast<double>( m_trackROIH ) );
        updateIfChanged( m_indiP_visitParams, "track_fps", m_trackFps );
        updateIfChanged( m_indiP_visitParams, "track_exptime", m_trackExpTime );
        updateIfChanged( m_indiP_visitParams, "track_loop_gain", m_trackGain );
        updateIfChanged( m_indiP_visitParams, "track_roll_gain", m_trackRollGain );
    }

    { //mutex scope
        std::lock_guard<std::mutex> tlock( m_trackMutex );

        updateIfChanged( m_indiP_trackStatus, "guide_x", m_guideCentroidX );
        updateIfChanged( m_indiP_trackStatus, "guide_y", m_guideCentroidY );
        updateIfChanged( m_indiP_trackStatus, "guide_err_px",
                         m_haveGuideCentroid
                             ? std::hypot( m_guideCentroidX - m_trackGuideTargetX,
                                           m_guideCentroidY - m_trackGuideTargetY )
                             : -1.0 );
        updateIfChanged( m_indiP_trackStatus, "roll_x", m_rollCentroidX );
        updateIfChanged( m_indiP_trackStatus, "roll_y", m_rollCentroidY );
        updateIfChanged( m_indiP_trackStatus, "roll_err_px",
                         m_haveRollCentroid ? std::hypot( m_rollCentroidX - m_trackRollTargetX,
                                                          m_rollCentroidY - m_trackRollTargetY )
                                            : -1.0 );
        updateIfChanged( m_indiP_trackStatus, "corrections", static_cast<double>( m_trackCorrections ) );
    }
}

INDI_NEWCALLBACK_DEFN( wccCtrl, m_indiP_start )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_start, ipRecv );

    if( !ipRecv.find( "request" ) )
    {
        return 0;
    }

    if( ipRecv["request"].getSwitchState() != pcf::IndiElement::On )
    {
        return 0;
    }

    updateSwitchIfChanged( m_indiP_start, "request", pcf::IndiElement::Off, INDI_IDLE );

    if( !m_visitLoaded )
    {
        log<text_log>( "start rejected: no visit loaded", logPrio::LOG_WARNING );
        return 0;
    }

    const wccAcqState s = acqState();

    if( s != wccAcqState::idle && s != wccAcqState::loaded && s != wccAcqState::failed &&
        s != wccAcqState::complete )
    {
        log<text_log>( "start rejected: a sequence is already running in state " + wccAcqStateName( s ),
                       logPrio::LOG_WARNING );
        return 0;
    }

    m_startRequest = true;

    log<text_log>( "acquisition sequence requested" );

    return 0;
}

INDI_NEWCALLBACK_DEFN( wccCtrl, m_indiP_abort )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_abort, ipRecv );

    if( !ipRecv.find( "request" ) )
    {
        return 0;
    }

    if( ipRecv["request"].getSwitchState() != pcf::IndiElement::On )
    {
        return 0;
    }

    updateSwitchIfChanged( m_indiP_abort, "request", pcf::IndiElement::Off, INDI_IDLE );

    m_abortRequest = true;

    log<text_log>( "abort requested", logPrio::LOG_NOTICE );

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // wccCtrl_hpp
