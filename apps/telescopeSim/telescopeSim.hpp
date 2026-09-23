/** \file telescopeSim.hpp
 * \brief The MagAO-X WCC telescope simulator.
 * \author Adam Schilperoort
 *
 * \ingroup telescopeSim_files
 */

#ifndef telescopeSim_hpp
#define telescopeSim_hpp

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <ctime>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <ImageStreamIO/ImageStreamIO.h>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include "../wccCommon/wccFocalPlane.hpp"
#include "../wccCommon/wccIndiRate.hpp"
#include "../wccCommon/wccNumeric.hpp"
#include "../wccCommon/wccPointingShmim.hpp"
#include "../wccCommon/wccSensorModel.hpp"
#include "../wccCommon/wccSkyWCS.hpp"
#include "../wccCommon/wccUnits.hpp"
#include "../wccCommon/wccVisitIndi.hpp"

/** \defgroup telescopeSim WCC Telescope Simulator
 * \brief Simulates a telescope mount: slews to a visit target, then tracks with jitter.
 *
 * Closes the WCC loop without real hardware. It takes its target from the visit
 * `visitCtrl` published, slews there at a finite rate, and then reports a pointing
 * that wanders by a configurable amount so downstream guiding has something real
 * to correct.
 *
 * It is the authority on where the telescope is pointed. The current pointing is
 * written to an ImageStreamIO stream at a configurable tick rate (5000 Hz of
 * simulated time by default) so `wccSim` can integrate mount motion during a
 * camera exposure. Each write is one tick of duration `1/write_hz`; the writer is
 * not paced to the computer clock unless `pointing.pace_wallclock` is set, because
 * the rest of the WCC simulators are allowed to run faster than real time. The
 * same pointing is also published on the INDI `pointing` property at 1 Hz, which
 * is what `wccCtrl` and operators read. `wccCtrl` sends corrections to `offset`
 * and watches `teldata` to know when a move is done. The property and element
 * names match what `wccCtrl` expects from a real `tcsInterface` closely enough
 * that pointing it at either is a configuration change rather than a code change.
 *
 * \par Interface summary
 * | property      | type   | purpose                                              |
 * |---------------|--------|------------------------------------------------------|
 * | `start_visit` | toggle | Slew to the target `visitCtrl` is publishing         |
 * | `pointing`    | number | Current pointing: `ra`, `dec`, `pa` (1 Hz INDI)      |
 * | `target`      | number | Where it is going: `ra`, `dec`, `pa`                 |
 * | `teldata`     | number | `slewing`, `tracking`, `settling`. Done detection    |
 * | `tel_status`  | text   | `state`, `message`                                   |
 * | `offset`       | number | Relative correction: `x`, `y` [arcsec], `roll` [deg] |
 * | `goto_target`  | number | Requested slew: `ra`, `dec`, `pa`. Does not move the mount |
 * | `goto`         | toggle | Submit a slew to `goto_target`, then track with jitter     |
 * | `stop_tracking` | toggle | On: stop tracking and drift at `idle_drift_rate`. Off: track where it points |
 * | `tracking_drift_rate` | number | Drift while tracking: `ra`, `dec` [arcsec/s], `pa` [deg/s] |
 * | `idle_drift_rate` | number | Drift while not tracking: `ra`, `dec` [arcsec/s], `pa` [deg/s] |
 * | `slew_rate`    | number | Live slew rate [deg/s] (`current` / `target`)        |
 * | `roll_rate`    | number | Live rotation rate [deg/s]                           |
 * | `jitter_x`     | number | Live pointing jitter rms along focal plane X [arcsec] |
 * | `jitter_y`     | number | Live pointing jitter rms along focal plane Y [arcsec] |
 * | `jitter_roll`  | number | Live roll (image rotation) jitter rms [deg]          |
 * | `settle_time`  | number | Live settle time [s]                                 |
 * | `arrive_tol`   | number | Live arrival tolerance [arcsec]                      |
 * | `jitter_tau`   | number | Live jitter correlation time [s]                     |
 * | `write_hz`     | number | Live pointing tick rate [Hz of simulated time]       |
 * | `history_s`    | number | Live pointing buffer span [s of simulated time]      |
 *
 * High-rate pointing lives on the `telpointing` shmim (configurable): a 4×1×N
 * circular buffer of doubles, axes RA, Dec, PA [deg] and simulated time [s],
 * written at `pointing.write_hz` ticks of simulated time per second.
 *
 * \par Focal plane to sky
 * Offsets and translational jitter are in focal-plane axes, arcsec. With
 * parity \f$p\f$ and position angle PA of focal plane +Y east of north, a focal
 * plane offset \f$(x, y)\f$ maps to tangent-plane east/north offsets
 * \f[ \xi = p\,x\cos\mathrm{PA} + y\sin\mathrm{PA}, \qquad
 *     \eta = -p\,x\sin\mathrm{PA} + y\cos\mathrm{PA}, \f]
 * so for small offsets \f$\Delta\mathrm{RA} \approx \xi/\cos\delta\f$ and
 * \f$\Delta\mathrm{Dec} \approx \eta\f$. The exact gnomonic inverse is
 * wcc::offsetBoresight(), which `wccSim` also uses. Roll is a rotation about the
 * boresight and adds directly to PA.
 *
 * <a href="../handbook/operating/software/apps/telescopeSim.html">Application Documentation</a>
 *
 * \ingroup apps
 */

/** \defgroup telescopeSim_files WCC Telescope Simulator Files
 * \ingroup telescopeSim
 */

namespace MagAOX
{
namespace app
{

/// The state of the simulated mount.
/** \ingroup telescopeSim
 */
enum class telSimState
{
    idle,     ///< Not tracking: parked or stopped, drifting at idle_drift_rate.
    slewing,  ///< Moving toward the target.
    settling, ///< Arrived, waiting out the settle time.
    tracking  ///< On target, drifting at tracking_drift_rate and wandering by the jitter.
};

/// Name of a mount state, for logging and INDI.
/** \returns the state name
 *
 * \ingroup telescopeSim
 */
inline std::string telSimStateName( telSimState s /**< [in] the state */ )
{
    switch( s )
    {
    case telSimState::idle:
        return "IDLE";
    case telSimState::slewing:
        return "SLEWING";
    case telSimState::settling:
        return "SETTLING";
    default:
        return "TRACKING";
    }
}

/// The MagAO-X WCC telescope simulator.
/** \ingroup telescopeSim
 */
class telescopeSim : public MagAOXApp<true>
{

    // Give the test harness access.
    friend class telescopeSim_test;

    /** \name Configurable Parameters - Data
     *@{
     */
  protected:
    /// INDI device publishing the visit, normally visitCtrl. Empty disables start_visit.
    std::string m_visitDevice;

    /// Live sim parameters are atomics so an INDI overlay (jitter, slew rate, ...)
    /// does not wait on the pointing worker's mount lock.
    std::atomic<double> m_slewRate{ 1.0 }; ///< Slew rate on the sky [deg/s].

    std::atomic<double> m_rollRate{ 1.0 }; ///< Rotation rate [deg/s].

    std::atomic<double> m_jitterX{ 0.2 }; ///< Pointing jitter along focal plane X, rms [arcsec].

    std::atomic<double> m_jitterY{ 0.2 }; ///< Pointing jitter along focal plane Y, rms [arcsec].

    std::atomic<double> m_jitterRoll{ 0.002 }; ///< Roll (image rotation) jitter, rms [deg].

    /// RA drift while tracking or settling [arcsec of RA per s]. Not scaled by cos(Dec).
    std::atomic<double> m_trackDriftRA{ 0.0 };

    std::atomic<double> m_trackDriftDec{ 0.0 }; ///< Dec drift while tracking or settling [arcsec/s].

    std::atomic<double> m_trackDriftPA{ 0.0 }; ///< PA drift while tracking or settling [deg/s].

    /// RA drift while not tracking [arcsec of RA per s]. Sidereal is about 15.041.
    std::atomic<double> m_idleDriftRA{ 0.0 };

    std::atomic<double> m_idleDriftDec{ 0.0 }; ///< Dec drift while not tracking [arcsec/s].

    std::atomic<double> m_idleDriftPA{ 0.0 }; ///< PA drift while not tracking [deg/s].

    std::atomic<double> m_settleTime{ 2.0 }; ///< Time spent settling after a slew [s].

    /// Distance below which the mount is considered to have arrived [arcsec].
    std::atomic<double> m_arriveTol{ 1.0 };

    /// Handedness of focal plane X relative to increasing right ascension. Must match wccSim.
    double m_parity{ -1.0 };

    double m_startRA{ 0 }; ///< Pointing at startup [deg].

    double m_startDec{ 0 }; ///< Pointing at startup [deg].

    double m_startPA{ 0 }; ///< Position angle at startup [deg].

    uint64_t m_seed{ 8675309 }; ///< RNG seed for the jitter.

    /// ImageStreamIO stream the high-rate pointing is written to.
    std::string m_pointingShmim{ wcc::pointingShmimDefault };

    std::atomic<double> m_writeHz{ wcc::pointingWriteHzDefault }; ///< Pointing tick rate [Hz of simulated time].

    std::atomic<double> m_historyS{ wcc::pointingHistorySDefault }; ///< Circular-buffer span [s of simulated time].

    /// If true, the pointing worker sleeps to match write_hz in wall-clock time.
    std::atomic<bool> m_paceWallclock{ false };

    /// Correlation time of the pointing jitter [s]. 0 is white (independent draws).
    std::atomic<double> m_jitterTau{ 0.05 };
    ///@}

    /** \name Mount State - Data
     *@{
     */
  protected:
    /// Guards commanded/reported pointing and the slew state machine, not live sim rates.
    std::mutex m_mountMutex;

    telSimState m_state{ telSimState::idle }; ///< What the mount is doing.

    /// True while tracking is off (the `stop_tracking` toggle).
    /** A slew still runs, but on arrival the mount goes to idle and drifts at
     * idle_drift_rate instead of settling into tracking. `goto` and
     * `start_visit` clear it; `offset` does not. True at startup because the
     * mount has not been told to track anything yet.
     */
    bool m_trackingStopped{ true };

    std::string m_message; ///< Why the mount is in its current state.

    /// Commanded pointing, before jitter is added.
    double m_baseRA{ 0 };

    double m_baseDec{ 0 }; ///< Commanded declination, before jitter [deg].

    double m_basePA{ 0 }; ///< Commanded position angle, before jitter [deg].

    double m_targetRA{ 0 }; ///< Target right ascension [deg].

    double m_targetDec{ 0 }; ///< Target declination [deg].

    double m_targetPA{ 0 }; ///< Target position angle [deg].

    bool m_haveTarget{ false }; ///< True once a target has been commanded.

    /// RA staged on `goto_target`. Not applied until the `goto` toggle is On.
    double m_gotoRA{ 0 };

    double m_gotoDec{ 0 }; ///< Dec staged on `goto_target` [deg].

    double m_gotoPA{ 0 }; ///< PA staged on `goto_target` [deg].

    /// Reported pointing, which is the commanded pointing plus jitter.
    double m_reportRA{ 0 };

    double m_reportDec{ 0 }; ///< Reported declination [deg].

    double m_reportPA{ 0 }; ///< Reported position angle [deg].

    double m_settleStart{ 0 }; ///< Simulated time at which settling began [s].

    double m_lastUpdate{ 0 }; ///< Wall-clock time of the last updateMount() call [s].

    double m_simTime{ 0 }; ///< Simulated time, advanced by 1/write_hz on each pointing tick [s].

    wcc::fastRandom m_rng; ///< Jitter generator.

    /// Instantaneous focal plane X jitter [arcsec], evolved as an Ornstein-Uhlenbeck process.
    double m_jx{ 0 };

    double m_jy{ 0 }; ///< Instantaneous focal plane Y jitter [arcsec].

    double m_jroll{ 0 }; ///< Instantaneous roll jitter [deg].
    ///@}

    /** \name Pointing Stream - Data
     *@{
     */
  protected:
    IMAGE m_pointingStream{}; ///< High-rate pointing ImageStreamIO stream.

    bool m_pointingStreamOpen{ false }; ///< True while m_pointingStream is created.

    uint32_t m_pointingDepth{ 1 }; ///< Circular buffer length of the pointing stream.

    std::mutex m_pointingStreamMutex; ///< Guards creating, destroying and writing m_pointingStream.

    std::thread m_pointingThread; ///< Worker that advances the mount and writes the shmim.

    std::atomic<bool> m_shutdownPointing{ false }; ///< Tells the pointing thread to exit.

    /// True after appStartup, so a live INDI change of write_hz can start the worker.
    bool m_pointingLive{ false };
    ///@}

    /** \name Visit Subscription - Data
     *@{
     */
  protected:
    std::mutex m_visitMutex; ///< Guards the subscribed visit target below.

    double m_visitRA{ 0 }; ///< Target right ascension visitCtrl is publishing [deg].

    double m_visitDec{ 0 }; ///< Target declination visitCtrl is publishing [deg].

    double m_visitPA{ 0 }; ///< Target position angle visitCtrl is publishing [deg].

    bool m_haveVisit{ false }; ///< True once visitCtrl has published a target.

    std::string m_visitState; ///< The loader state visitCtrl reports.
    ///@}

    /** \name INDI - Data
     *@{
     */
  protected:
    pcf::IndiProperty m_indiP_startVisit; ///< Toggle that slews to the visit target.
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_startVisit );

    pcf::IndiProperty m_indiP_offset; ///< Relative correction request.
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_offset );

    pcf::IndiProperty m_indiP_goto; ///< Staged absolute pointing: ra, dec, pa. Does not move the mount.
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_goto );

    pcf::IndiProperty m_indiP_go; ///< Toggle that slews to the staged goto_target.
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_go );

    pcf::IndiProperty m_indiP_stopTracking; ///< Toggle: On stops tracking, Off tracks where the mount points.
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_stopTracking );

    pcf::IndiProperty m_indiP_trackDrift; ///< Drift while tracking: ra, dec [arcsec/s], pa [deg/s].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_trackDrift );

    pcf::IndiProperty m_indiP_idleDrift; ///< Drift while not tracking: ra, dec [arcsec/s], pa [deg/s].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_idleDrift );

    pcf::IndiProperty m_indiP_slewRate; ///< Live slew rate [deg/s].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_slewRate );

    pcf::IndiProperty m_indiP_rollRate; ///< Live rotation rate [deg/s].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_rollRate );

    pcf::IndiProperty m_indiP_jitterX; ///< Live focal plane X jitter rms [arcsec].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_jitterX );

    pcf::IndiProperty m_indiP_jitterY; ///< Live focal plane Y jitter rms [arcsec].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_jitterY );

    pcf::IndiProperty m_indiP_jitterRoll; ///< Live roll jitter rms [deg].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_jitterRoll );

    pcf::IndiProperty m_indiP_settleTime; ///< Live settle time [s].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_settleTime );

    pcf::IndiProperty m_indiP_arriveTol; ///< Live arrival tolerance [arcsec].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_arriveTol );

    pcf::IndiProperty m_indiP_jitterTau; ///< Live jitter correlation time [s].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_jitterTau );

    pcf::IndiProperty m_indiP_writeHz; ///< Live pointing tick rate [Hz].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_writeHz );

    pcf::IndiProperty m_indiP_historyS; ///< Live pointing buffer span [s].
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_historyS );

    pcf::IndiProperty m_indiP_pointing; ///< 1 Hz INDI pointing: ra, dec, pa. High-rate copy is the shmim.

    pcf::IndiProperty m_indiP_target; ///< Target pointing.

    pcf::IndiProperty m_indiP_teldata; ///< Motion flags, for done detection.

    pcf::IndiProperty m_indiP_status; ///< Mount state and message.

    /// Subscribed visitCtrl properties, held so they stay registered.
    pcf::IndiProperty m_indiP_visitTarget;

    pcf::IndiProperty m_indiP_visitStatus;

    /// Subscriptions indexed by pcf::IndiProperty::createUniqueKey().
    std::map<std::string, std::string> m_bindings;
    ///@}

  public:
    /// Default c'tor.
    telescopeSim();

    /// D'tor, declared and defined for noexcept.
    ~telescopeSim() noexcept;

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** \returns 0 on success
     * \returns -1 on a configuration error
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] configuration to load from */ );

    virtual void loadConfig();

    virtual int appStartup();

    /// Implementation of the FSM for telescopeSim.
    /** \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
    virtual int appLogic();

    virtual int appShutdown();

  protected:
    /// Advance the mount by `dt` seconds of simulated time and apply jitter.
    /** This is the mount clock. The pointing worker calls it once per shmim
     * write with `dt = 1/write_hz`. Wall-clock time is not involved.
     */
    void advanceMount( double dt /**< [in] simulated time step [s] */ );

    /// Advance the mount by measured wall-clock time.
    /** Used only when the pointing worker is disabled (`write_hz` 0) so the
     * 1 Hz INDI tick remains a usable mount clock, and to ignore a huge clock
     * step rather than teleporting. Prefer `advanceMount` when the simulated
     * time step is known.
     */
    void updateMount();

    /// Command an absolute target and begin slewing.
    /** Independent of visitCtrl. After the slew and settle, the mount tracks
     * with the configured jitter, unless tracking is stopped, in which case it
     * goes idle on arrival. Does not change m_trackingStopped.
     */
    void commandTarget( double ra /**< [in] right ascension [deg] */,
                        double dec /**< [in] declination [deg] */,
                        double pa /**< [in] position angle [deg] */,
                        const std::string &why /**< [in] what asked for this, for the log */ );

    /// Overlay finite coordinates onto the staged goto_target. Does not slew.
    /** Null pointers or non-finite values leave that axis unchanged. The mount
     * does not move until submitGoto() / the `goto` toggle.
     *
     * \returns 0 always
     */
    int storeGotoCoordinates( const double *ra /**< [in] right ascension [deg], or null to keep */,
                              const double *dec /**< [in] declination [deg], or null to keep */,
                              const double *pa /**< [in] position angle [deg], or null to keep */ );

    /// Publish the staged goto_target coordinates so NaN cannot stick on the server.
    void publishGotoTargetIndi();

    /// Slew to the staged goto_target and track with jitter.
    /** Independent of visitCtrl. Clears stop_tracking. High-rate coordinates
     * continue on the pointing shmim; the 1 Hz INDI `pointing` property is the
     * same boresight, downsampled.
     */
    void submitGoto();

    /// Stop or resume tracking, as the `stop_tracking` toggle does.
    /** Stopping sends a tracking or settling mount to idle, where it drifts at
     * idle_drift_rate with no jitter. A slew in progress finishes, then goes
     * idle. Resuming from idle tracks wherever the mount now points: the target
     * becomes the current pointing and the mount settles, then tracks.
     */
    void setTrackingStopped( bool stopped /**< [in] true to stop tracking, false to resume */ );

    /// Overlay finite drift rates for one mode. Non-finite values are left unchanged.
    void applyDriftRates( bool idle /**< [in] true for idle_drift_rate, false for tracking_drift_rate */,
                          double ra /**< [in] RA rate [arcsec of RA per s], or NaN to keep */,
                          double dec /**< [in] Dec rate [arcsec/s], or NaN to keep */,
                          double pa /**< [in] PA rate [deg/s], or NaN to keep */ );

    /// SET a drift-rate property from members so an ignored NaN element is repaired.
    void publishDriftIndi( bool idle /**< [in] true for idle_drift_rate, false for tracking_drift_rate */ );

    /// Publish the 1 Hz INDI pointing, target, motion flags and status.
    void publishState();

    /// Create the high-rate pointing stream.
    /** \returns 0 on success
     * \returns -1 if the stream cannot be created
     */
    int ensurePointingStream();

    /// Write the current reported pointing (realtime RA/Dec/PA) into the next shmim slice.
    void writePointingShmim();

    /// Thread entry for the pointing worker.
    static void pointingWorkerStart( telescopeSim *s /**< [in] this */ );

    /// Advance simulated time by 1/write_hz, write a pointing slice, optionally pace to wall-clock.
    void pointingWorkerExec();

    /// Overlay live mount-model parameters. Non-finite values are left unchanged.
    /** Invalid values (non-positive rates, negative jitter) are ignored for that
     * field and the previous value is kept. Finiteness is tested with
     * wcc::isFinite because the build's `-ffast-math` removes `std::isfinite`.
     * These are atomics, so a live jitter change does not take the mount mutex
     * or recreate `telpointing`. The pointing worker reads them on the next tick.
     */
    void applySimParameters( double slewRate /**< [in] slew rate [deg/s], or NaN to keep */,
                             double rollRate /**< [in] rotation rate [deg/s], or NaN to keep */,
                             double jitterX /**< [in] focal plane X jitter rms [arcsec], or NaN to keep */,
                             double jitterY /**< [in] focal plane Y jitter rms [arcsec], or NaN to keep */,
                             double jitterRoll /**< [in] roll jitter rms [deg], or NaN to keep */,
                             double settleTime /**< [in] settle time [s], or NaN to keep */,
                             double arriveTol /**< [in] arrival tolerance [arcsec], or NaN to keep */,
                             double jitterTau /**< [in] jitter correlation time [s], or NaN to keep */ );

    /// Overlay live pointing-buffer parameters and recreate the stream if its depth changes.
    /** Non-finite values are left unchanged. A change of `write_hz` is picked up
     * on the next pointing-worker iteration. A change of buffer depth stops the
     * worker, destroys and recreates `telpointing`, then starts the worker again.
     * `wccSim` closes and reopens its mmap when it sees the `write_hz` or
     * `history_s` SET.
     *
     * \returns 0 on success
     * \returns -1 if a live stream could not be recreated
     */
    int applyPointingBufferParameters( double writeHz /**< [in] tick rate [Hz], or NaN to keep */,
                                       double historyS /**< [in] buffer span [s], or NaN to keep */ );

    /// Which live sim number a MagAO-X current/target NEW refers to.
    enum class telSimIndiParam
    {
        slewRate,   ///< sim.slew_rate
        rollRate,   ///< sim.roll_rate
        jitterX,    ///< sim.jitter_x
        jitterY,    ///< sim.jitter_y
        jitterRoll, ///< sim.jitter_roll
        settleTime, ///< sim.settle_time
        arriveTol,  ///< sim.arrive_tol
        jitterTau   ///< sim.jitter_tau
    };

    /// Push current sim members to their INDI `current` / `target` pairs if changed.
    /** Caller must hold m_indiMutex. Non-finite members are not published.
     */
    void syncSimIndi();

    /// Push current pointing-buffer members to `write_hz` and `history_s` if changed.
    /** Caller must hold m_indiMutex.
     */
    void syncPointingCfgIndi();

    /// Write `current` and `target` on a MagAO-X number. Does not lock or send.
    /** Caller must hold m_indiMutex. Non-finite values are ignored so NaN cannot
     * be published.
     */
    static void writeNumberPair( pcf::IndiProperty &prop /**< [in,out] current/target property */,
                                 double value /**< [in] value for both current and target */ );

    /// Lock, write, and SET one MagAO-X standard number from a finite member.
    /** Always sends, so a NaN a client wrote to this property is replaced on the
     * server and in every GUI by the value actually in use.
     */
    void sendNumberPair( pcf::IndiProperty &prop /**< [in,out] current/target property */,
                         double value /**< [in] value for both current and target */ );

    /// Value of the member a live sim number controls.
    /** \returns the current value
     */
    double simParameter( telSimIndiParam which /**< [in] which member */ );

    /// Read `target`, or `current` if target is absent, from a standard number NEW.
    /** Ignores missing, empty, and non-finite values so a client that sends the
     * unused element as NaN does not clobber a live parameter.
     *
     * \returns true if a finite number was present
     */
    static bool readIndiNumberRequest( const pcf::IndiProperty &ip /**< [in] the received property */,
                                       double &out /**< [out] the value */ );

    /// Apply one live sim number NEW and republish that property from members.
    /** Non-finite requests, which MagAO-X GUIs send for unedited elements, are
     * ignored and the property is SET back to the value in use.
     *
     * \returns 0 always
     */
    int handleSimNumberNew( pcf::IndiProperty &prop /**< [in,out] the local current/target property */,
                            const pcf::IndiProperty &ipRecv /**< [in] the received NEW */,
                            telSimIndiParam which /**< [in] which member this property controls */ );

    /// Apply a live write_hz or history_s NEW and republish that property from members.
    /** \returns 0 on success
     * \returns -1 if a live stream could not be recreated
     */
    int handlePointingNumberNew( pcf::IndiProperty &prop /**< [in,out] the local current/target property */,
                                 const pcf::IndiProperty &ipRecv /**< [in] the received NEW */,
                                 bool isWriteHz /**< [in] true for write_hz, false for history_s */ );

    /// Start the pointing worker if the app is live, write_hz is positive, and no worker is running.
    void startPointingWorker();

    /// Stop the pointing worker and join it. Does not hold the stream mutex.
    void stopPointingWorker();

    /// Destroy and recreate the pointing stream at the current depth.
    /** Stops the worker first so ImageStreamIO_destroyIm cannot wait on a writer
     * that is waiting for this mutex.
     *
     * \returns 0 on success
     * \returns -1 if the stream cannot be created
     */
    int recreatePointingStream();

    /// Shared SET callback for the subscribed visitCtrl properties.
    /** \returns 0 always, so an unexpected property is ignored rather than fatal
     */
    int setCallBack_remote( const pcf::IndiProperty &ipRecv /**< [in] the received property */ );

    /// Static trampoline for setCallBack_remote().
    /** \returns the result of setCallBack_remote()
     */
    static int st_setCallBack_remote( void *app /**< [in] the telescopeSim instance */,
                                      const pcf::IndiProperty &ipRecv /**< [in] the received property */ );

    /// Read a numeric element from a received property, tolerating text encoding.
    /** \returns true if the element was present and parsed to a finite number
     */
    static bool elementValue( const pcf::IndiProperty &ip /**< [in] the property */,
                              const std::string &el /**< [in] element name */,
                              double &out /**< [out] the value */ );
};

inline telescopeSim::telescopeSim() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED ), m_rng( 8675309 )
{
    // Pure software: no PDU.
    m_powerMgtEnabled = false;

    // INDI stays at 1 Hz. The pointing worker writes the shmim at pointing.write_hz.
    m_loopPause = wcc::indiLoopPause;

    return;
}

inline telescopeSim::~telescopeSim() noexcept
{
    return;
}

inline void telescopeSim::setupConfig()
{
    config.add( "visit.device", "", "visit.device", argType::Required, "visit", "device", false, "string",
                "INDI device publishing the visit, normally visitCtrl. Empty disables start_visit." );

    config.add( "sim.slew_rate", "", "sim.slew_rate", argType::Required, "sim", "slew_rate", false, "double",
                "Slew rate on the sky [deg/s]." );
    config.add( "sim.roll_rate", "", "sim.roll_rate", argType::Required, "sim", "roll_rate", false, "double",
                "Rotation rate [deg/s]." );
    config.add( "sim.jitter", "", "sim.jitter", argType::Required, "sim", "jitter", false, "double",
                "Legacy: pointing jitter rms [arcsec] for both focal plane axes. jitter_x / jitter_y override." );
    config.add( "sim.jitter_x", "", "sim.jitter_x", argType::Required, "sim", "jitter_x", false, "double",
                "Pointing jitter along focal plane X, rms [arcsec]. 0 gives a perfectly steady axis." );
    config.add( "sim.jitter_y", "", "sim.jitter_y", argType::Required, "sim", "jitter_y", false, "double",
                "Pointing jitter along focal plane Y, rms [arcsec]." );
    config.add( "sim.jitter_roll", "", "sim.jitter_roll", argType::Required, "sim", "jitter_roll", false,
                "double", "Roll (image rotation about the boresight) jitter, rms [deg]." );
    config.add( "sim.tracking_drift_ra", "", "sim.tracking_drift_ra", argType::Required, "sim",
                "tracking_drift_ra", false, "double",
                "RA drift while tracking [arcsec of RA per s, not scaled by cos Dec]. Exercises guiding." );
    config.add( "sim.tracking_drift_dec", "", "sim.tracking_drift_dec", argType::Required, "sim",
                "tracking_drift_dec", false, "double", "Dec drift while tracking [arcsec/s]." );
    config.add( "sim.tracking_drift_pa", "", "sim.tracking_drift_pa", argType::Required, "sim",
                "tracking_drift_pa", false, "double", "PA drift while tracking [deg/s]. Exercises roll guiding." );
    config.add( "sim.idle_drift_ra", "", "sim.idle_drift_ra", argType::Required, "sim", "idle_drift_ra", false,
                "double", "RA drift while not tracking [arcsec of RA per s]. Sidereal is about 15.041." );
    config.add( "sim.idle_drift_dec", "", "sim.idle_drift_dec", argType::Required, "sim", "idle_drift_dec",
                false, "double", "Dec drift while not tracking [arcsec/s]." );
    config.add( "sim.idle_drift_pa", "", "sim.idle_drift_pa", argType::Required, "sim", "idle_drift_pa", false,
                "double", "PA drift while not tracking [deg/s]." );
    config.add( "sim.drift_x", "", "sim.drift_x", argType::Required, "sim", "drift_x", false, "double",
                "Retired: accepted so old configs load, ignored. Use tracking_drift_ra / tracking_drift_dec." );
    config.add( "sim.drift_y", "", "sim.drift_y", argType::Required, "sim", "drift_y", false, "double",
                "Retired: accepted so old configs load, ignored. Use tracking_drift_ra / tracking_drift_dec." );
    config.add( "sim.settle_time", "", "sim.settle_time", argType::Required, "sim", "settle_time", false,
                "double", "Time spent settling after a slew before reporting tracking [s]." );
    config.add( "sim.arrive_tol", "", "sim.arrive_tol", argType::Required, "sim", "arrive_tol", false, "double",
                "Distance below which the mount is considered to have arrived [arcsec]." );
    config.add( "sim.parity", "", "sim.parity", argType::Required, "sim", "parity", false, "double",
                "Handedness of focal plane X against increasing RA, +1 or -1. Must match wccSim." );
    config.add( "sim.ra", "", "sim.ra", argType::Required, "sim", "ra", false, "double",
                "Right ascension at startup [deg]." );
    config.add( "sim.dec", "", "sim.dec", argType::Required, "sim", "dec", false, "double",
                "Declination at startup [deg]." );
    config.add( "sim.pa", "", "sim.pa", argType::Required, "sim", "pa", false, "double",
                "Position angle at startup [deg]." );
    config.add( "sim.seed", "", "sim.seed", argType::Required, "sim", "seed", false, "int",
                "RNG seed for the jitter, so a run is reproducible." );
    config.add( "sim.jitter_tau", "", "sim.jitter_tau", argType::Required, "sim", "jitter_tau", false,
                "double",
                "Jitter correlation time [s]. 0 is white (independent draws each write). A finite value "
                "makes the pointing wander smoothly so a long exposure streaks rather than blobs." );

    config.add( "pointing.shmim", "", "pointing.shmim", argType::Required, "pointing", "shmim", false, "string",
                "ImageStreamIO stream the high-rate pointing is written to. 4x1xN doubles: ra, dec, pa, sim_time." );
    config.add( "pointing.write_hz", "", "pointing.write_hz", argType::Required, "pointing", "write_hz", false,
                "double",
                "Pointing tick rate [Hz of simulated time]. Each write advances the mount by 1/write_hz. "
                "0 disables the stream and updates only at the 1 Hz INDI tick." );
    config.add( "pointing.history_s", "", "pointing.history_s", argType::Required, "pointing", "history_s",
                false, "double",
                "Circular-buffer span [s of simulated time]. Must cover the longest camera exposure." );
    config.add( "pointing.pace_wallclock", "", "pointing.pace_wallclock", argType::Required, "pointing",
                "pace_wallclock", false, "bool",
                "If true, sleep so writes match write_hz in wall-clock time. Default false: the sim is not "
                "required to run in real time, and exposures are correlated by simulated ticks." );
}

inline int telescopeSim::loadConfigImpl( mx::app::appConfigurator &_config )
{
    _config( m_visitDevice, "visit.device" );

    double slewRate = m_slewRate.load();
    double rollRate = m_rollRate.load();
    double jitterX = m_jitterX.load();
    double jitterY = m_jitterY.load();
    double jitterRoll = m_jitterRoll.load();
    double trackDriftRA = m_trackDriftRA.load();
    double trackDriftDec = m_trackDriftDec.load();
    double trackDriftPA = m_trackDriftPA.load();
    double idleDriftRA = m_idleDriftRA.load();
    double idleDriftDec = m_idleDriftDec.load();
    double idleDriftPA = m_idleDriftPA.load();
    double settleTime = m_settleTime.load();
    double arriveTol = m_arriveTol.load();
    double jitterTau = m_jitterTau.load();
    double writeHz = m_writeHz.load();
    double historyS = m_historyS.load();
    bool paceWallclock = m_paceWallclock.load();

    _config( slewRate, "sim.slew_rate" );
    _config( rollRate, "sim.roll_rate" );

    // sim.jitter sets both axes; the per-axis keys override it when present.
    {
        double jitter = std::numeric_limits<double>::quiet_NaN();
        _config( jitter, "sim.jitter" );
        if( wcc::isFinite( jitter ) )
        {
            jitterX = jitter;
            jitterY = jitter;
        }
    }
    _config( jitterX, "sim.jitter_x" );
    _config( jitterY, "sim.jitter_y" );
    _config( jitterRoll, "sim.jitter_roll" );
    _config( trackDriftRA, "sim.tracking_drift_ra" );
    _config( trackDriftDec, "sim.tracking_drift_dec" );
    _config( trackDriftPA, "sim.tracking_drift_pa" );
    _config( idleDriftRA, "sim.idle_drift_ra" );
    _config( idleDriftDec, "sim.idle_drift_dec" );
    _config( idleDriftPA, "sim.idle_drift_pa" );

    {
        double driftX = 0, driftY = 0;
        _config( driftX, "sim.drift_x" );
        _config( driftY, "sim.drift_y" );
        if( driftX != 0 || driftY != 0 )
        {
            log<text_log>( "sim.drift_x / sim.drift_y are retired and ignored; use sim.tracking_drift_ra, "
                           "sim.tracking_drift_dec and sim.tracking_drift_pa",
                           logPrio::LOG_WARNING );
        }
    }

    _config( settleTime, "sim.settle_time" );
    _config( arriveTol, "sim.arrive_tol" );
    _config( m_parity, "sim.parity" );
    _config( m_startRA, "sim.ra" );
    _config( m_startDec, "sim.dec" );
    _config( m_startPA, "sim.pa" );

    {
        int seed = static_cast<int>( m_seed );
        _config( seed, "sim.seed" );
        if( seed > 0 )
        {
            m_seed = static_cast<uint64_t>( seed );
        }
    }

    _config( jitterTau, "sim.jitter_tau" );
    _config( m_pointingShmim, "pointing.shmim" );
    _config( writeHz, "pointing.write_hz" );
    _config( historyS, "pointing.history_s" );
    _config( paceWallclock, "pointing.pace_wallclock" );

    if( !wcc::isFinite( jitterTau ) || jitterTau < 0 )
    {
        jitterTau = 0;
    }

    if( !wcc::isFinite( writeHz ) || writeHz < 0 )
    {
        writeHz = 0;
    }

    if( !wcc::isFinite( historyS ) || historyS <= 0 )
    {
        historyS = wcc::pointingHistorySDefault;
    }

    if( m_pointingShmim.empty() )
    {
        m_pointingShmim = wcc::pointingShmimDefault;
    }

    if( !wcc::isFinite( slewRate ) || slewRate <= 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, "sim.slew_rate must be positive" } );
    }

    if( !wcc::isFinite( rollRate ) || rollRate <= 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, "sim.roll_rate must be positive" } );
    }

    if( !wcc::isFinite( m_startRA ) || !wcc::isFinite( m_startDec ) || !wcc::isFinite( m_startPA ) )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, "sim.ra, sim.dec and sim.pa must be finite" } );
    }

    for( double *nonNeg : { &jitterX, &jitterY, &jitterRoll, &settleTime } )
    {
        if( !wcc::isFinite( *nonNeg ) || *nonNeg < 0 )
        {
            *nonNeg = 0;
        }
    }

    for( double *rate : { &trackDriftRA, &trackDriftDec, &trackDriftPA, &idleDriftRA, &idleDriftDec, &idleDriftPA } )
    {
        if( !wcc::isFinite( *rate ) )
        {
            *rate = 0;
        }
    }

    if( !wcc::isFinite( arriveTol ) || arriveTol <= 0 )
    {
        arriveTol = 1.0;
    }

    m_slewRate.store( slewRate );
    m_rollRate.store( rollRate );
    m_jitterX.store( jitterX );
    m_jitterY.store( jitterY );
    m_jitterRoll.store( jitterRoll );
    m_trackDriftRA.store( trackDriftRA );
    m_trackDriftDec.store( trackDriftDec );
    m_trackDriftPA.store( trackDriftPA );
    m_idleDriftRA.store( idleDriftRA );
    m_idleDriftDec.store( idleDriftDec );
    m_idleDriftPA.store( idleDriftPA );
    m_settleTime.store( settleTime );
    m_arriveTol.store( arriveTol );
    m_jitterTau.store( jitterTau );
    m_writeHz.store( writeHz );
    m_historyS.store( historyS );
    m_paceWallclock.store( paceWallclock );

    m_pointingDepth = wcc::pointingBufferDepth( writeHz, historyS );

    m_rng.seed( m_seed );

    m_baseRA = m_startRA;
    m_baseDec = m_startDec;
    m_basePA = m_startPA;
    m_reportRA = m_startRA;
    m_reportDec = m_startDec;
    m_reportPA = m_startPA;
    m_targetRA = m_startRA;
    m_targetDec = m_startDec;
    m_targetPA = m_startPA;
    m_gotoRA = m_startRA;
    m_gotoDec = m_startDec;
    m_gotoPA = m_startPA;
    m_simTime = 0;
    m_settleStart = 0;
    m_state = telSimState::idle;
    m_trackingStopped = true;
    m_jx = 0;
    m_jy = 0;
    m_jroll = 0;

    return 0;
}

inline void telescopeSim::loadConfig()
{
    if( loadConfigImpl( config ) < 0 )
    {
        m_shutdown = true;
    }
}

inline int telescopeSim::appStartup()
{
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_startVisit, "start_visit" );

    // Multi-element vectors read better as named elements than current/target
    // pairs, which is the choice tcsInterface makes for pyrNudge.
    REG_INDI_NEWPROP( m_indiP_offset, "offset", pcf::IndiProperty::Number );
    m_indiP_offset.add( pcf::IndiElement( "x" ) );
    m_indiP_offset.add( pcf::IndiElement( "y" ) );
    m_indiP_offset.add( pcf::IndiElement( "roll" ) );
    m_indiP_offset["x"].set( 0.0 );
    m_indiP_offset["y"].set( 0.0 );
    m_indiP_offset["roll"].set( 0.0 );

    // Staged command only. The `goto` toggle is what actually slews.
    REG_INDI_NEWPROP( m_indiP_goto, "goto_target", pcf::IndiProperty::Number );
    indi::addNumberElement<double>( m_indiP_goto, "ra", 0, 360, 0, "%0.6f", "RA [deg]" );
    indi::addNumberElement<double>( m_indiP_goto, "dec", -90, 90, 0, "%0.6f", "Dec [deg]" );
    indi::addNumberElement<double>( m_indiP_goto, "pa", -360, 360, 0, "%0.6f", "PA [deg]" );
    m_indiP_goto["ra"].set( m_startRA );
    m_indiP_goto["dec"].set( m_startDec );
    m_indiP_goto["pa"].set( m_startPA );

    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_go, "goto" );

    // Starts On: the mount is not tracking anything until goto / start_visit, or
    // until this is turned Off to track wherever it points.
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_stopTracking, "stop_tracking" );
    m_indiP_stopTracking["toggle"].setSwitchState( pcf::IndiElement::On );

    // Drift rates are applied as soon as they arrive (unlike goto_target). NaN
    // elements are ignored, then the whole vector is SET back from members.
    REG_INDI_NEWPROP( m_indiP_trackDrift, "tracking_drift_rate", pcf::IndiProperty::Number );
    indi::addNumberElement<double>( m_indiP_trackDrift, "ra", -1e6, 1e6, 0, "%0.6f", "RA [arcsec/s]" );
    indi::addNumberElement<double>( m_indiP_trackDrift, "dec", -1e6, 1e6, 0, "%0.6f", "Dec [arcsec/s]" );
    indi::addNumberElement<double>( m_indiP_trackDrift, "pa", -360, 360, 0, "%0.8f", "PA [deg/s]" );

    REG_INDI_NEWPROP( m_indiP_idleDrift, "idle_drift_rate", pcf::IndiProperty::Number );
    indi::addNumberElement<double>( m_indiP_idleDrift, "ra", -1e6, 1e6, 0, "%0.6f", "RA [arcsec/s]" );
    indi::addNumberElement<double>( m_indiP_idleDrift, "dec", -1e6, 1e6, 0, "%0.6f", "Dec [arcsec/s]" );
    indi::addNumberElement<double>( m_indiP_idleDrift, "pa", -360, 360, 0, "%0.8f", "PA [deg/s]" );

    publishDriftIndi( false );
    publishDriftIndi( true );

    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_slewRate, "slew_rate", 1e-6, 1e6, 0.1, "%0.6f", "Slew rate [deg/s]", "sim" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_rollRate, "roll_rate", 1e-6, 1e6, 0.1, "%0.6f", "Roll rate [deg/s]", "sim" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_jitterX, "jitter_x", 0, 100, 0.01, "%0.4f", "Jitter X rms [arcsec]", "sim" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_jitterY, "jitter_y", 0, 100, 0.01, "%0.4f", "Jitter Y rms [arcsec]", "sim" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_jitterRoll, "jitter_roll", 0, 10, 0.0001, "%0.6f", "Roll jitter rms [deg]", "sim" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_settleTime, "settle_time", 0, 3600, 0.1, "%0.3f", "Settle time [s]", "sim" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_arriveTol, "arrive_tol", 1e-6, 3600, 0.1, "%0.4f", "Arrive tol [arcsec]", "sim" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_jitterTau, "jitter_tau", 0, 100, 0.001, "%0.4f", "Jitter tau [s]", "sim" );

    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_writeHz, wcc::pointingWriteHzIndiProperty, 0, 1e6, 1, "%0.3f",
                                "Pointing tick rate [Hz]", "pointing" );
    CREATE_REG_INDI_NEW_NUMBERD( m_indiP_historyS, wcc::pointingHistoryIndiProperty, 0.001, 1e6, 1, "%0.3f",
                                "Pointing history [s]", "pointing" );

    sendNumberPair( m_indiP_slewRate, m_slewRate.load() );
    sendNumberPair( m_indiP_rollRate, m_rollRate.load() );
    sendNumberPair( m_indiP_jitterX, m_jitterX.load() );
    sendNumberPair( m_indiP_jitterY, m_jitterY.load() );
    sendNumberPair( m_indiP_jitterRoll, m_jitterRoll.load() );
    sendNumberPair( m_indiP_settleTime, m_settleTime.load() );
    sendNumberPair( m_indiP_arriveTol, m_arriveTol.load() );
    sendNumberPair( m_indiP_jitterTau, m_jitterTau.load() );
    sendNumberPair( m_indiP_writeHz, m_writeHz.load() );
    sendNumberPair( m_indiP_historyS, m_historyS.load() );

    if( createROIndiNumber( m_indiP_pointing, "pointing", "Current pointing", "telescope" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber pointing" } );
    }
    m_indiP_pointing.add( pcf::IndiElement( "ra" ) );
    m_indiP_pointing.add( pcf::IndiElement( "dec" ) );
    m_indiP_pointing.add( pcf::IndiElement( "pa" ) );
    registerIndiPropertyReadOnly( m_indiP_pointing );

    if( createROIndiNumber( m_indiP_target, "target", "Target pointing", "telescope" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber target" } );
    }
    m_indiP_target.add( pcf::IndiElement( "ra" ) );
    m_indiP_target.add( pcf::IndiElement( "dec" ) );
    m_indiP_target.add( pcf::IndiElement( "pa" ) );
    registerIndiPropertyReadOnly( m_indiP_target );

    // Named to match tcsInterface's teldata so a controller's done detection can
    // be pointed at either with only a configuration change.
    if( createROIndiNumber( m_indiP_teldata, "teldata", "Telescope motion", "telescope" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber teldata" } );
    }
    m_indiP_teldata.add( pcf::IndiElement( "slewing" ) );
    m_indiP_teldata.add( pcf::IndiElement( "settling" ) );
    m_indiP_teldata.add( pcf::IndiElement( "tracking" ) );
    registerIndiPropertyReadOnly( m_indiP_teldata );

    if( createROIndiText( m_indiP_status, "tel_status", "state", "Telescope status", "telescope", "state" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText tel_status" } );
    }
    m_indiP_status.add( pcf::IndiElement( "message" ) );
    registerIndiPropertyReadOnly( m_indiP_status );

    // ---------------------------------------------------- visitCtrl subscription
    if( !m_visitDevice.empty() )
    {
        if( registerIndiPropertySet( m_indiP_visitTarget, m_visitDevice, wcc::visitIndi::propTarget,
                                     st_setCallBack_remote ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__,
                  "failed to subscribe to " + m_visitDevice + "." + wcc::visitIndi::propTarget } );
        }
        m_bindings[m_indiP_visitTarget.createUniqueKey()] = "visit_target";

        if( registerIndiPropertySet( m_indiP_visitStatus, m_visitDevice, wcc::visitIndi::propStatus,
                                     st_setCallBack_remote ) < 0 )
        {
            return log<software_error, -1>(
                { __FILE__, __LINE__,
                  "failed to subscribe to " + m_visitDevice + "." + wcc::visitIndi::propStatus } );
        }
        m_bindings[m_indiP_visitStatus.createUniqueKey()] = "visit_status";

        log<text_log>( "taking visit targets from " + m_visitDevice );
    }
    else
    {
        log<text_log>( "no visit device configured; start_visit is disabled and only goto (from goto_target) "
                       "and offset will move the mount",
                       logPrio::LOG_WARNING );
    }

    m_lastUpdate = mx::sys::get_curr_time();

    m_pointingLive = true;
    if( m_writeHz.load() > 0 )
    {
        if( ensurePointingStream() < 0 )
        {
            return -1;
        }

        startPointingWorker();
    }

    state( stateCodes::READY );

    log<text_log>( "telescopeSim ready at ra " + std::to_string( m_startRA ) + " dec " +
                   std::to_string( m_startDec ) + " pa " + std::to_string( m_startPA ) + ", slew rate " +
                   std::to_string( m_slewRate.load() ) + " deg/s, jitter x " + std::to_string( m_jitterX.load() ) +
                   " y " + std::to_string( m_jitterY.load() ) + " arcsec rms, roll " +
                   std::to_string( m_jitterRoll.load() ) + " deg rms, idle drift ra " +
                   std::to_string( m_idleDriftRA.load() ) + " arcsec/s, tracking stopped until goto, start_visit "
                   "or stop_tracking Off, pointing shmim " +
                   m_pointingShmim + " at " +
                   std::to_string( m_writeHz.load() ) + " Hz of simulated time" +
                   std::string( m_paceWallclock.load() ? ", paced to wall-clock" : "" ) );

    return 0;
}

inline int telescopeSim::appLogic()
{
    const double writeHz = m_writeHz.load();

    if( writeHz <= 0 )
    {
        // No pointing thread: keep the 1 Hz INDI tick as the mount clock.
        updateMount();
    }

    telSimState st;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        st = m_state;
    }

    if( st == telSimState::slewing || st == telSimState::settling )
    {
        state( stateCodes::CONFIGURING );
    }
    else if( st == telSimState::tracking )
    {
        state( stateCodes::OPERATING );
    }
    else
    {
        state( stateCodes::READY );
    }

    // appLogic runs at the 1 Hz INDI rate limit, so publishing here needs no
    // further throttling.
    publishState();

    return 0;
}

inline int telescopeSim::appShutdown()
{
    m_pointingLive = false;
    stopPointingWorker();

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingStreamMutex );

        if( m_pointingStreamOpen )
        {
            ImageStreamIO_destroyIm( &m_pointingStream );
            m_pointingStreamOpen = false;
        }
    }

    return 0;
}

inline void telescopeSim::updateMount()
{
    const double now = mx::sys::get_curr_time();
    const double dt = now - m_lastUpdate;
    m_lastUpdate = now;

    if( dt <= 0 || dt > 60.0 )
    {
        // First call, a clock step, or a long stall: do not integrate a huge slew.
        return;
    }

    advanceMount( dt );
}

inline void telescopeSim::advanceMount( double dt )
{
    if( !wcc::isFinite( dt ) || !( dt > 0 ) )
    {
        return;
    }

    // Snapshot the live parameters. They are only ever stored finite, but a bad
    // value here would poison the OU state and the reported pointing forever, so
    // repair rather than trust.
    auto positive = []( std::atomic<double> &a, double fallback )
    {
        const double v = a.load();
        if( wcc::isFinite( v ) && v > 0 )
        {
            return v;
        }
        a.store( fallback );
        return fallback;
    };

    auto nonNegative = []( std::atomic<double> &a )
    {
        const double v = a.load();
        if( wcc::isFinite( v ) && v >= 0 )
        {
            return v;
        }
        a.store( 0 );
        return 0.0;
    };

    auto finiteOrZero = []( std::atomic<double> &a )
    {
        const double v = a.load();
        if( wcc::isFinite( v ) )
        {
            return v;
        }
        a.store( 0 );
        return 0.0;
    };

    const double slewRate = positive( m_slewRate, 1.0 );
    const double rollRate = positive( m_rollRate, 1.0 );
    const double arriveTol = positive( m_arriveTol, 1.0 );
    const double jitterX = nonNegative( m_jitterX );
    const double jitterY = nonNegative( m_jitterY );
    const double jitterRoll = nonNegative( m_jitterRoll );
    const double settleTime = nonNegative( m_settleTime );
    const double jitterTau = nonNegative( m_jitterTau );
    const double trackDriftRA = finiteOrZero( m_trackDriftRA );
    const double trackDriftDec = finiteOrZero( m_trackDriftDec );
    const double trackDriftPA = finiteOrZero( m_trackDriftPA );
    const double idleDriftRA = finiteOrZero( m_idleDriftRA );
    const double idleDriftDec = finiteOrZero( m_idleDriftDec );
    const double idleDriftPA = finiteOrZero( m_idleDriftPA );

    std::string arrivedLog;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );

    m_simTime += dt;

    // ----------------------------------------------------------------- drift
    // Tracking drift is the residual error guiding has to remove. Idle drift is
    // what the sky does to a mount that is not tracking (sidereal in RA for an
    // equatorial mount). A slew in progress is driven by the slew controller and
    // does not drift.
    {
        double dRA = 0, dDec = 0, dPA = 0;

        if( m_state == telSimState::tracking || m_state == telSimState::settling )
        {
            dRA = trackDriftRA;
            dDec = trackDriftDec;
            dPA = trackDriftPA;
        }
        else if( m_state == telSimState::idle )
        {
            dRA = idleDriftRA;
            dDec = idleDriftDec;
            dPA = idleDriftPA;
        }

        if( dRA != 0 || dDec != 0 || dPA != 0 )
        {
            m_baseRA += dRA * dt * wcc::arcsec2deg;
            m_baseDec += dDec * dt * wcc::arcsec2deg;
            m_basePA += dPA * dt;

            m_baseRA = std::fmod( m_baseRA, 360.0 );
            if( m_baseRA < 0 )
            {
                m_baseRA += 360.0;
            }
            m_baseDec = std::max( -90.0, std::min( 90.0, m_baseDec ) );
        }
    }

    // ------------------------------------------------------------------ slew
    if( m_state == telSimState::slewing )
    {
        const double sep = wcc::angularSeparation( m_baseRA, m_baseDec, m_targetRA, m_targetDec );
        const double paErr = m_targetPA - m_basePA;

        const double maxStep = slewRate * dt;
        const double maxRoll = rollRate * dt;

        // Arrival is whether this step *reached* the target, not whether the mount
        // had to move to get there. A step that covers the whole remaining distance
        // arrives now; treating it as still slewing would cost a full update, and at
        // the 1 Hz reporting rate that is a wasted second on every small guiding
        // correction. Within arrive_tol the mount snaps onto the target: a guiding
        // offset is usually smaller than the tolerance, and declaring arrival
        // without moving would silently drop it.
        bool atPosition = false;

        if( sep <= maxStep || sep <= arriveTol * wcc::arcsec2deg )
        {
            m_baseRA = m_targetRA;
            m_baseDec = m_targetDec;
            atPosition = true;
        }
        else
        {
            // Move a fraction of the way along the great circle. Linear
            // interpolation in RA and Dec is close enough for a simulator at
            // these step sizes and keeps the mount monotonically approaching.
            const double f = maxStep / sep;

            double dra = m_targetRA - m_baseRA;

            // Take the short way around the RA wrap.
            if( dra > 180.0 )
            {
                dra -= 360.0;
            }
            if( dra < -180.0 )
            {
                dra += 360.0;
            }

            m_baseRA += f * dra;
            m_baseDec += f * ( m_targetDec - m_baseDec );
        }

        bool atRoll = ( std::fabs( paErr ) <= 1e-6 );

        if( !atRoll )
        {
            if( std::fabs( paErr ) <= maxRoll )
            {
                m_basePA = m_targetPA;
                atRoll = true;
            }
            else
            {
                m_basePA += ( paErr > 0 ? maxRoll : -maxRoll );
            }
        }

        m_baseRA = std::fmod( m_baseRA, 360.0 );
        if( m_baseRA < 0 )
        {
            m_baseRA += 360.0;
        }

        if( atPosition && atRoll )
        {
            if( m_trackingStopped )
            {
                m_state = telSimState::idle;
                m_message = "arrived; tracking stopped";
                arrivedLog = "arrived at ra " + std::to_string( m_baseRA ) + " dec " + std::to_string( m_baseDec ) +
                             " pa " + std::to_string( m_basePA ) + " with tracking stopped";
            }
            else
            {
                m_state = telSimState::settling;
                m_settleStart = m_simTime;
                m_message = "settling";
            }
        }
    }
    else if( m_state == telSimState::settling )
    {
        if( m_simTime - m_settleStart >= settleTime )
        {
            m_state = telSimState::tracking;
            m_message = "tracking";
            arrivedLog = "arrived at ra " + std::to_string( m_baseRA ) + " dec " +
                         std::to_string( m_baseDec ) + " pa " + std::to_string( m_basePA );
        }
    }

    // ---------------------------------------------------------------- jitter
    // Jitter is applied to the reported pointing, not accumulated into the
    // commanded one, so it is a wander about where the mount actually is rather
    // than a random walk that would run away. A finite correlation time makes
    // successive writes a smooth trail, which is what a long exposure should
    // integrate into a streak; tau = 0 is white, matching the original 1 Hz draws.
    // X and Y are focal plane axes, so their sky direction follows PA; roll is a
    // rotation about the boresight and is independent of both.
    if( m_state == telSimState::tracking || m_state == telSimState::settling )
    {
        auto ouStep = [&]( double &x, double sigma )
        {
            if( !( sigma > 0 ) )
            {
                x = 0;
                return;
            }

            if( !wcc::isFinite( x ) )
            {
                x = 0;
            }

            if( !( jitterTau > 0 ) )
            {
                x = sigma * m_rng.normal();
                return;
            }

            const double a = std::exp( -dt / jitterTau );
            const double s = sigma * std::sqrt( std::max( 0.0, 1.0 - a * a ) );
            x = a * x + s * m_rng.normal();
        };

        ouStep( m_jx, jitterX );
        ouStep( m_jy, jitterY );
        ouStep( m_jroll, jitterRoll );
    }
    else
    {
        m_jx = 0;
        m_jy = 0;
        m_jroll = 0;
    }

    if( !wcc::isFinite( m_jx ) )
    {
        m_jx = 0;
    }
    if( !wcc::isFinite( m_jy ) )
    {
        m_jy = 0;
    }
    if( !wcc::isFinite( m_jroll ) )
    {
        m_jroll = 0;
    }

    // Last line of defence: never let a non-finite base leave this function,
    // because it would stay non-finite and be written to the shmim every tick.
    if( !wcc::isFinite( m_baseRA ) || !wcc::isFinite( m_baseDec ) )
    {
        m_baseRA = m_targetRA;
        m_baseDec = m_targetDec;
    }
    if( !wcc::isFinite( m_basePA ) )
    {
        m_basePA = m_targetPA;
    }

    if( m_jx != 0 || m_jy != 0 )
    {
        wcc::offsetBoresight( m_baseRA, m_baseDec, m_basePA, m_parity, m_jx, m_jy, m_reportRA, m_reportDec );
    }
    else
    {
        m_reportRA = m_baseRA;
        m_reportDec = m_baseDec;
    }

    m_reportPA = m_basePA + m_jroll;

    if( !wcc::isFinite( m_reportRA ) || !wcc::isFinite( m_reportDec ) )
    {
        m_reportRA = m_baseRA;
        m_reportDec = m_baseDec;
    }
    if( !wcc::isFinite( m_reportPA ) )
    {
        m_reportPA = m_basePA;
    }
    }

    if( !arrivedLog.empty() )
    {
        log<text_log>( arrivedLog );
    }
}

inline int telescopeSim::ensurePointingStream()
{
    bool created = false;
    bool failed = false;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingStreamMutex );

        if( m_pointingStreamOpen )
        {
            return 0;
        }

        if( m_pointingDepth < 1 )
        {
            m_pointingDepth = 1;
        }

        uint32_t sizes[3] = { wcc::pointingNAxes, 1, m_pointingDepth };

        if( ImageStreamIO_createIm_gpu( &m_pointingStream, m_pointingShmim.c_str(), 3, sizes, IMAGESTRUCT_DOUBLE,
                                        -1, 1, IMAGE_NB_SEMAPHORE, 0, CIRCULAR_BUFFER | ZAXIS_TEMPORAL, 0 ) !=
            IMAGESTREAMIO_SUCCESS )
        {
            failed = true;
        }
        else
        {
            m_pointingStream.md->cnt1 = m_pointingDepth - 1;
            m_pointingStreamOpen = true;
            created = true;
        }
    }

    if( failed )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to create pointing stream " + m_pointingShmim } );
    }

    if( created )
    {
        log<text_log>( "created pointing stream " + m_pointingShmim + " 4x1x" +
                       std::to_string( m_pointingDepth ) + " double at " + std::to_string( m_writeHz.load() ) +
                       " Hz of simulated time" );
    }

    return 0;
}

inline void telescopeSim::stopPointingWorker()
{
    m_shutdownPointing.store( true );

    try
    {
        if( m_pointingThread.joinable() )
        {
            m_pointingThread.join();
        }
    }
    catch( ... )
    {
    }
}

inline int telescopeSim::recreatePointingStream()
{
    stopPointingWorker();

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingStreamMutex );

        if( m_pointingStreamOpen )
        {
            ImageStreamIO_destroyIm( &m_pointingStream );
            m_pointingStreamOpen = false;
        }
    }

    if( ensurePointingStream() < 0 )
    {
        return -1;
    }

    startPointingWorker();

    return 0;
}

inline void telescopeSim::startPointingWorker()
{
    if( !m_pointingLive )
    {
        return;
    }

    if( !( m_writeHz.load() > 0 ) )
    {
        return;
    }

    if( m_pointingThread.joinable() )
    {
        return;
    }

    if( ensurePointingStream() < 0 )
    {
        return;
    }

    m_shutdownPointing.store( false );
    m_pointingThread = std::thread( pointingWorkerStart, this );
}

inline void telescopeSim::writePointingShmim()
{
    double ra, dec, pa, simTime;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        ra = m_reportRA;
        dec = m_reportDec;
        pa = m_reportPA;
        simTime = m_simTime;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingStreamMutex );

        if( !m_pointingStreamOpen || m_pointingStream.md == nullptr )
        {
            return;
        }

        m_pointingStream.md->write = 1;

        const uint32_t depth = ( m_pointingDepth > 0 ) ? m_pointingDepth : 1;
        const uint64_t slice = ( depth > 1 ) ? ( m_pointingStream.md->cnt1 + 1 ) % depth : 0;

        double *dest = reinterpret_cast<double *>( m_pointingStream.array.raw ) + slice * wcc::pointingNAxes;
        wcc::packPointing( dest, ra, dec, pa, simTime );

        clock_gettime( CLOCK_REALTIME, &m_pointingStream.md->writetime );
        m_pointingStream.md->atime = m_pointingStream.md->writetime;

        if( m_pointingStream.writetimearray != nullptr )
        {
            m_pointingStream.writetimearray[slice] = m_pointingStream.md->writetime;
        }

        m_pointingStream.md->cnt1 = slice;

        ImageStreamIO_UpdateIm( &m_pointingStream );

        m_pointingStream.md->write = 0;
    }
}

inline void telescopeSim::pointingWorkerStart( telescopeSim *s )
{
    s->pointingWorkerExec();
}

inline void telescopeSim::pointingWorkerExec()
{
    double next = mx::sys::get_curr_time();

    while( !m_shutdownPointing.load() && !m_shutdown )
    {
        const double writeHz = m_writeHz.load();
        const bool pace = m_paceWallclock.load();

        if( !( writeHz > 0 ) )
        {
            std::this_thread::yield();
            continue;
        }

        const double dtSim = 1.0 / writeHz;

        // One write is one tick of simulated time, regardless of how fast this
        // loop actually runs. Camera exposures are correlated by tick count /
        // sim-time, not by CLOCK_REALTIME. write_hz is reread every iteration so
        // a live INDI change takes effect on the next tick.
        advanceMount( dtSim );
        writePointingShmim();

        if( !pace )
        {
            std::this_thread::yield();
            continue;
        }

        next += dtSim;
        const double now = mx::sys::get_curr_time();
        const double remain = next - now;

        if( remain > 0 )
        {
            mx::sys::microSleep( static_cast<unsigned>( remain * 1e6 ) );
        }
        else
        {
            // Fell behind: do not try to catch up a backlog of writes.
            next = now;
        }
    }
}

inline void telescopeSim::applySimParameters( double slewRate,
                                             double rollRate,
                                             double jitterX,
                                             double jitterY,
                                             double jitterRoll,
                                             double settleTime,
                                             double arriveTol,
                                             double jitterTau )
{
    std::string warning;

    if( wcc::isFinite( slewRate ) )
    {
        if( slewRate > 0 )
        {
            m_slewRate.store( slewRate );
        }
        else
        {
            warning = "sim.slew_rate must be positive; keeping " + std::to_string( m_slewRate.load() );
        }
    }

    if( wcc::isFinite( rollRate ) )
    {
        if( rollRate > 0 )
        {
            m_rollRate.store( rollRate );
        }
        else
        {
            warning = "sim.roll_rate must be positive; keeping " + std::to_string( m_rollRate.load() );
        }
    }

    if( wcc::isFinite( jitterX ) )
    {
        m_jitterX.store( ( jitterX < 0 ) ? 0 : jitterX );
    }

    if( wcc::isFinite( jitterY ) )
    {
        m_jitterY.store( ( jitterY < 0 ) ? 0 : jitterY );
    }

    if( wcc::isFinite( jitterRoll ) )
    {
        m_jitterRoll.store( ( jitterRoll < 0 ) ? 0 : jitterRoll );
    }

    if( wcc::isFinite( settleTime ) )
    {
        m_settleTime.store( ( settleTime < 0 ) ? 0 : settleTime );
    }

    if( wcc::isFinite( arriveTol ) )
    {
        m_arriveTol.store( ( arriveTol <= 0 ) ? 1.0 : arriveTol );
    }

    if( wcc::isFinite( jitterTau ) )
    {
        m_jitterTau.store( ( jitterTau < 0 ) ? 0 : jitterTau );
    }

    if( !warning.empty() )
    {
        log<text_log>( warning, logPrio::LOG_WARNING );
    }
}

inline void telescopeSim::applyDriftRates( bool idle, double ra, double dec, double pa )
{
    std::atomic<double> &aRA = idle ? m_idleDriftRA : m_trackDriftRA;
    std::atomic<double> &aDec = idle ? m_idleDriftDec : m_trackDriftDec;
    std::atomic<double> &aPA = idle ? m_idleDriftPA : m_trackDriftPA;

    if( wcc::isFinite( ra ) )
    {
        aRA.store( ra );
    }

    if( wcc::isFinite( dec ) )
    {
        aDec.store( dec );
    }

    if( wcc::isFinite( pa ) )
    {
        aPA.store( pa );
    }
}

inline void telescopeSim::publishDriftIndi( bool idle )
{
    pcf::IndiProperty &prop = idle ? m_indiP_idleDrift : m_indiP_trackDrift;
    const double ra = idle ? m_idleDriftRA.load() : m_trackDriftRA.load();
    const double dec = idle ? m_idleDriftDec.load() : m_trackDriftDec.load();
    const double pa = idle ? m_idleDriftPA.load() : m_trackDriftPA.load();

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_indiMutex );

        try
        {
            if( wcc::isFinite( ra ) )
            {
                prop["ra"].set( ra );
            }
            if( wcc::isFinite( dec ) )
            {
                prop["dec"].set( dec );
            }
            if( wcc::isFinite( pa ) )
            {
                prop["pa"].set( pa );
            }

            if( m_indiDriver )
            {
                m_indiDriver->sendSetProperty( prop );
            }
        }
        catch( ... )
        {
        }
    }
}

inline void telescopeSim::setTrackingStopped( bool stopped )
{
    std::string logMsg;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );

        m_trackingStopped = stopped;

        if( stopped )
        {
            if( m_state == telSimState::tracking || m_state == telSimState::settling )
            {
                m_state = telSimState::idle;
                m_message = "tracking stopped";
                logMsg = "tracking stopped at ra " + std::to_string( m_baseRA ) + " dec " +
                         std::to_string( m_baseDec ) + " pa " + std::to_string( m_basePA ) +
                         "; drifting at idle_drift_rate";
            }
            else if( m_state == telSimState::slewing )
            {
                logMsg = "tracking stopped; the slew in progress will finish, then the mount goes idle";
            }
        }
        else if( m_state == telSimState::idle )
        {
            // Track wherever the mount now points, drift included.
            m_targetRA = m_baseRA;
            m_targetDec = m_baseDec;
            m_targetPA = m_basePA;
            m_haveTarget = true;
            m_state = telSimState::settling;
            m_settleStart = m_simTime;
            m_message = "settling";
            logMsg = "tracking resumed at ra " + std::to_string( m_baseRA ) + " dec " + std::to_string( m_baseDec ) +
                     " pa " + std::to_string( m_basePA );
        }
    }

    if( !logMsg.empty() )
    {
        log<text_log>( logMsg );
    }
}

inline int telescopeSim::applyPointingBufferParameters( double writeHz, double historyS )
{
    double hz = m_writeHz.load();
    double hist = m_historyS.load();

    if( wcc::isFinite( writeHz ) )
    {
        if( writeHz < 0 )
        {
            log<text_log>( "write_hz must be >= 0; keeping " + std::to_string( hz ), logPrio::LOG_WARNING );
        }
        else
        {
            hz = writeHz;
        }
    }

    if( wcc::isFinite( historyS ) )
    {
        if( historyS <= 0 )
        {
            log<text_log>( "history_s must be positive; keeping " + std::to_string( hist ),
                           logPrio::LOG_WARNING );
        }
        else
        {
            hist = historyS;
        }
    }

    const uint32_t newDepth = wcc::pointingBufferDepth( hz, hist );
    bool streamOpen = false;
    uint32_t oldDepth = 1;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_pointingStreamMutex );
        streamOpen = m_pointingStreamOpen;
        oldDepth = m_pointingDepth;
    }

    const bool needRecreate = m_pointingLive && streamOpen && newDepth != oldDepth && hz > 0;
    const bool needStop = m_pointingLive && ( needRecreate || ( streamOpen && !( hz > 0 ) ) );

    m_writeHz.store( hz );
    m_historyS.store( hist );

    if( needStop )
    {
        stopPointingWorker();
    }

    if( needRecreate )
    {
        log<text_log>( "recreating pointing stream at " + std::to_string( hz ) + " Hz, history " +
                       std::to_string( hist ) + " s (" + std::to_string( newDepth ) + " slices)" );

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_pointingStreamMutex );

            if( m_pointingStreamOpen )
            {
                ImageStreamIO_destroyIm( &m_pointingStream );
                m_pointingStreamOpen = false;
            }

            m_pointingDepth = newDepth;
        }

        if( ensurePointingStream() < 0 )
        {
            return -1;
        }
    }
    else
    {
        { //mutex scope
            std::lock_guard<std::mutex> lock( m_pointingStreamMutex );
            m_pointingDepth = newDepth;
        }

        if( m_pointingLive && !streamOpen && hz > 0 )
        {
            if( ensurePointingStream() < 0 )
            {
                return -1;
            }
        }
        else if( m_pointingLive && streamOpen && !( hz > 0 ) )
        {
            { //mutex scope
                std::lock_guard<std::mutex> lock( m_pointingStreamMutex );

                if( m_pointingStreamOpen )
                {
                    ImageStreamIO_destroyIm( &m_pointingStream );
                    m_pointingStreamOpen = false;
                }
            }
        }
    }

    if( m_pointingLive && hz > 0 )
    {
        startPointingWorker();
    }

    return 0;
}

inline void telescopeSim::writeNumberPair( pcf::IndiProperty &prop, double value )
{
    if( !wcc::isFinite( value ) )
    {
        return;
    }

    try
    {
        prop["current"].set( value );
        prop["target"].set( value );
    }
    catch( ... )
    {
    }
}

inline void telescopeSim::sendNumberPair( pcf::IndiProperty &prop, double value )
{
    if( !wcc::isFinite( value ) )
    {
        return;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_indiMutex );

        writeNumberPair( prop, value );

        if( m_indiDriver )
        {
            m_indiDriver->sendSetProperty( prop );
        }
    }
}

inline double telescopeSim::simParameter( telSimIndiParam which )
{
    switch( which )
    {
    case telSimIndiParam::slewRate:
        return m_slewRate.load();
    case telSimIndiParam::rollRate:
        return m_rollRate.load();
    case telSimIndiParam::jitterX:
        return m_jitterX.load();
    case telSimIndiParam::jitterY:
        return m_jitterY.load();
    case telSimIndiParam::jitterRoll:
        return m_jitterRoll.load();
    case telSimIndiParam::settleTime:
        return m_settleTime.load();
    case telSimIndiParam::arriveTol:
        return m_arriveTol.load();
    case telSimIndiParam::jitterTau:
        return m_jitterTau.load();
    }

    return std::numeric_limits<double>::quiet_NaN();
}

inline bool telescopeSim::readIndiNumberRequest( const pcf::IndiProperty &ip, double &out )
{
    if( elementValue( ip, "target", out ) )
    {
        return true;
    }

    return elementValue( ip, "current", out );
}

inline int telescopeSim::handleSimNumberNew( pcf::IndiProperty &prop,
                                             const pcf::IndiProperty &ipRecv,
                                             telSimIndiParam which )
{
    const double keep = std::numeric_limits<double>::quiet_NaN();
    double requested = keep;

    if( readIndiNumberRequest( ipRecv, requested ) )
    {
        switch( which )
        {
        case telSimIndiParam::slewRate:
            applySimParameters( requested, keep, keep, keep, keep, keep, keep, keep );
            break;
        case telSimIndiParam::rollRate:
            applySimParameters( keep, requested, keep, keep, keep, keep, keep, keep );
            break;
        case telSimIndiParam::jitterX:
            applySimParameters( keep, keep, requested, keep, keep, keep, keep, keep );
            break;
        case telSimIndiParam::jitterY:
            applySimParameters( keep, keep, keep, requested, keep, keep, keep, keep );
            break;
        case telSimIndiParam::jitterRoll:
            applySimParameters( keep, keep, keep, keep, requested, keep, keep, keep );
            break;
        case telSimIndiParam::settleTime:
            applySimParameters( keep, keep, keep, keep, keep, requested, keep, keep );
            break;
        case telSimIndiParam::arriveTol:
            applySimParameters( keep, keep, keep, keep, keep, keep, requested, keep );
            break;
        case telSimIndiParam::jitterTau:
            applySimParameters( keep, keep, keep, keep, keep, keep, keep, requested );
            break;
        }
    }

    // SET back from the member even when the request was ignored, so a NaN the
    // client wrote is replaced by the value in use.
    sendNumberPair( prop, simParameter( which ) );

    return 0;
}

inline int telescopeSim::handlePointingNumberNew( pcf::IndiProperty &prop,
                                                  const pcf::IndiProperty &ipRecv,
                                                  bool isWriteHz )
{
    const double keep = std::numeric_limits<double>::quiet_NaN();
    double requested = keep;
    int rv = 0;

    if( readIndiNumberRequest( ipRecv, requested ) )
    {
        rv = applyPointingBufferParameters( isWriteHz ? requested : keep, isWriteHz ? keep : requested );
    }

    sendNumberPair( prop, isWriteHz ? m_writeHz.load() : m_historyS.load() );

    return rv;
}

inline void telescopeSim::syncSimIndi()
{
    const std::pair<pcf::IndiProperty *, double> nums[] = {
        { &m_indiP_slewRate, m_slewRate.load() },     { &m_indiP_rollRate, m_rollRate.load() },
        { &m_indiP_jitterX, m_jitterX.load() },       { &m_indiP_jitterY, m_jitterY.load() },
        { &m_indiP_jitterRoll, m_jitterRoll.load() }, { &m_indiP_settleTime, m_settleTime.load() },
        { &m_indiP_arriveTol, m_arriveTol.load() },   { &m_indiP_jitterTau, m_jitterTau.load() } };

    for( const auto &n : nums )
    {
        if( wcc::isFinite( n.second ) )
        {
            updateIfChanged( *n.first, "current", n.second );
            updateIfChanged( *n.first, "target", n.second );
        }
    }
}

inline void telescopeSim::syncPointingCfgIndi()
{
    const double writeHz = m_writeHz.load();
    const double historyS = m_historyS.load();

    if( wcc::isFinite( writeHz ) )
    {
        updateIfChanged( m_indiP_writeHz, "current", writeHz );
        updateIfChanged( m_indiP_writeHz, "target", writeHz );
    }

    if( wcc::isFinite( historyS ) )
    {
        updateIfChanged( m_indiP_historyS, "current", historyS );
        updateIfChanged( m_indiP_historyS, "target", historyS );
    }
}

inline void telescopeSim::commandTarget( double ra, double dec, double pa, const std::string &why )
{
    if( !wcc::isFinite( ra ) || !wcc::isFinite( dec ) || !wcc::isFinite( pa ) )
    {
        log<text_log>( why + ": target ignored because ra, dec or pa is not finite", logPrio::LOG_WARNING );
        return;
    }

    if( dec > 90.0 )
    {
        dec = 90.0;
    }
    if( dec < -90.0 )
    {
        dec = -90.0;
    }

    ra = std::fmod( ra, 360.0 );
    if( ra < 0 )
    {
        ra += 360.0;
    }

    double sep = 0;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );

        m_targetRA = ra;
        m_targetDec = dec;
        m_targetPA = pa;
        m_haveTarget = true;

        sep = wcc::angularSeparation( m_baseRA, m_baseDec, ra, dec );

        m_state = telSimState::slewing;
        m_message = why;
    }

    log<text_log>( why + ": slewing " + std::to_string( sep * 3600.0 ) + " arcsec to ra " +
                   std::to_string( ra ) + " dec " + std::to_string( dec ) + " pa " + std::to_string( pa ) );
}

inline void telescopeSim::publishGotoTargetIndi()
{
    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        ra = m_gotoRA;
        dec = m_gotoDec;
        pa = m_gotoPA;
    }

    try
    {
        std::lock_guard<std::mutex> lock( m_indiMutex );

        m_indiP_goto["ra"].set( ra );
        m_indiP_goto["dec"].set( dec );
        m_indiP_goto["pa"].set( pa );

        if( m_indiDriver )
        {
            m_indiDriver->sendSetProperty( m_indiP_goto );
        }
    }
    catch( ... )
    {
    }
}

inline int telescopeSim::storeGotoCoordinates( const double *ra, const double *dec, const double *pa )
{
    bool any = false;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );

        if( ra != nullptr && wcc::isFinite( *ra ) )
        {
            m_gotoRA = std::fmod( *ra, 360.0 );
            if( m_gotoRA < 0 )
            {
                m_gotoRA += 360.0;
            }
            any = true;
        }

        if( dec != nullptr && wcc::isFinite( *dec ) )
        {
            m_gotoDec = *dec;
            if( m_gotoDec > 90.0 )
            {
                m_gotoDec = 90.0;
            }
            if( m_gotoDec < -90.0 )
            {
                m_gotoDec = -90.0;
            }
            any = true;
        }

        if( pa != nullptr && wcc::isFinite( *pa ) )
        {
            m_gotoPA = *pa;
            any = true;
        }
    }

    if( !any )
    {
        log<text_log>( "goto_target ignored: ra, dec and pa were missing or not finite", logPrio::LOG_WARNING );
    }

    publishGotoTargetIndi();

    return 0;
}

inline void telescopeSim::submitGoto()
{
    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        ra = m_gotoRA;
        dec = m_gotoDec;
        pa = m_gotoPA;
        m_trackingStopped = false;
    }

    commandTarget( ra, dec, pa, "goto" );
}

inline void telescopeSim::publishState()
{
    double ra, dec, pa, tra, tdec, tpa;
    telSimState st;
    std::string msg;
    bool stopped;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        ra = m_reportRA;
        dec = m_reportDec;
        pa = m_reportPA;
        tra = m_targetRA;
        tdec = m_targetDec;
        tpa = m_targetPA;
        st = m_state;
        msg = m_message;
        stopped = m_trackingStopped;
    }

    { //mutex scope
        std::unique_lock<std::mutex> lock( m_indiMutex, std::try_to_lock );

        if( !lock.owns_lock() )
        {
            return;
        }

        if( wcc::isFinite( ra ) )
        {
            updateIfChanged( m_indiP_pointing, "ra", ra );
        }
        if( wcc::isFinite( dec ) )
        {
            updateIfChanged( m_indiP_pointing, "dec", dec );
        }
        if( wcc::isFinite( pa ) )
        {
            updateIfChanged( m_indiP_pointing, "pa", pa );
        }

        if( wcc::isFinite( tra ) )
        {
            updateIfChanged( m_indiP_target, "ra", tra );
        }
        if( wcc::isFinite( tdec ) )
        {
            updateIfChanged( m_indiP_target, "dec", tdec );
        }
        if( wcc::isFinite( tpa ) )
        {
            updateIfChanged( m_indiP_target, "pa", tpa );
        }

        updateIfChanged( m_indiP_teldata, "slewing", ( st == telSimState::slewing ) ? 1.0 : 0.0 );
        updateIfChanged( m_indiP_teldata, "settling", ( st == telSimState::settling ) ? 1.0 : 0.0 );
        updateIfChanged( m_indiP_teldata, "tracking", ( st == telSimState::tracking ) ? 1.0 : 0.0 );

        updateIfChanged( m_indiP_status, "state", telSimStateName( st ) );
        updateIfChanged( m_indiP_status, "message", msg );

        updateSwitchIfChanged( m_indiP_stopTracking, "toggle", stopped ? pcf::IndiElement::On : pcf::IndiElement::Off,
                               stopped ? INDI_IDLE : INDI_OK );

        syncSimIndi();
        syncPointingCfgIndi();
    }
}

inline int telescopeSim::st_setCallBack_remote( void *app, const pcf::IndiProperty &ipRecv )
{
    return static_cast<telescopeSim *>( app )->setCallBack_remote( ipRecv );
}

inline bool telescopeSim::elementValue( const pcf::IndiProperty &ip, const std::string &el, double &out )
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

inline int telescopeSim::setCallBack_remote( const pcf::IndiProperty &ipRecv )
{
    auto it = m_bindings.find( ipRecv.createUniqueKey() );

    if( it == m_bindings.end() )
    {
        return 0;
    }

    if( it->second == "visit_target" )
    {
        double ra = 0, dec = 0, pa = 0;
        const bool haveRA = elementValue( ipRecv, wcc::visitIndi::elRA, ra );
        const bool haveDec = elementValue( ipRecv, wcc::visitIndi::elDec, dec );
        const bool havePA = elementValue( ipRecv, wcc::visitIndi::elRollPA, pa );

        std::lock_guard<std::mutex> lock( m_visitMutex );

        if( haveRA )
        {
            m_visitRA = ra;
        }
        if( haveDec )
        {
            m_visitDec = dec;
        }
        if( havePA )
        {
            m_visitPA = pa;
        }

        if( haveRA && haveDec )
        {
            m_haveVisit = true;
        }

        return 0;
    }

    if( it->second == "visit_status" )
    {
        try
        {
            if( ipRecv.find( wcc::visitIndi::elState ) )
            {
                std::lock_guard<std::mutex> lock( m_visitMutex );
                m_visitState = ipRecv[wcc::visitIndi::elState].getValue();
            }
        }
        catch( ... )
        {
        }

        return 0;
    }

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_startVisit )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_startVisit, ipRecv );

    if( !ipRecv.find( "toggle" ) )
    {
        return 0;
    }

    if( ipRecv["toggle"].getSwitchState() != pcf::IndiElement::On )
    {
        // Toggling off stops tracking the visit and leaves the mount where it is,
        // drifting at idle_drift_rate.
        { //mutex scope
            std::lock_guard<std::mutex> lock( m_mountMutex );
            m_state = telSimState::idle;
            m_trackingStopped = true;
            m_message = "visit stopped";
        }

        updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::Off, INDI_IDLE );
        log<text_log>( "visit stopped; tracking stopped where the mount was" );

        return 0;
    }

    double ra, dec, pa;
    bool have;
    std::string vstate;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        ra = m_visitRA;
        dec = m_visitDec;
        pa = m_visitPA;
        have = m_haveVisit;
        vstate = m_visitState;
    }

    if( m_visitDevice.empty() )
    {
        log<text_log>( "start_visit rejected: no visit device configured", logPrio::LOG_WARNING );
        updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::Off, INDI_ALERT );
        return 0;
    }

    if( !have || vstate != wcc::visitIndi::stateLoaded )
    {
        log<text_log>( "start_visit rejected: " + m_visitDevice + " has not published a loaded visit (state " +
                           ( vstate.empty() ? std::string( "unknown" ) : vstate ) + ")",
                       logPrio::LOG_WARNING );
        updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::Off, INDI_ALERT );
        return 0;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        m_trackingStopped = false;
    }

    commandTarget( ra, dec, pa, "start_visit from " + m_visitDevice );

    updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::On, INDI_BUSY );
    updateSwitchIfChanged( m_indiP_go, "toggle", pcf::IndiElement::Off, INDI_IDLE );

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_goto )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_goto, ipRecv );

    double ra = std::numeric_limits<double>::quiet_NaN();
    double dec = std::numeric_limits<double>::quiet_NaN();
    double pa = std::numeric_limits<double>::quiet_NaN();

    const bool haveRA = elementValue( ipRecv, "ra", ra );
    const bool haveDec = elementValue( ipRecv, "dec", dec );
    const bool havePA = elementValue( ipRecv, "pa", pa );

    return storeGotoCoordinates( haveRA ? &ra : nullptr, haveDec ? &dec : nullptr, havePA ? &pa : nullptr );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_go )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_go, ipRecv );

    if( !ipRecv.find( "toggle" ) )
    {
        return 0;
    }

    if( ipRecv["toggle"].getSwitchState() != pcf::IndiElement::On )
    {
        { //mutex scope
            std::lock_guard<std::mutex> lock( m_mountMutex );
            m_state = telSimState::idle;
            m_trackingStopped = true;
            m_message = "goto stopped";
        }

        updateSwitchIfChanged( m_indiP_go, "toggle", pcf::IndiElement::Off, INDI_IDLE );
        log<text_log>( "goto stopped; tracking stopped where the mount was" );

        return 0;
    }

    submitGoto();

    updateSwitchIfChanged( m_indiP_go, "toggle", pcf::IndiElement::On, INDI_BUSY );
    updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::Off, INDI_IDLE );

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_offset )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_offset, ipRecv );

    double dx = 0, dy = 0, droll = 0;
    elementValue( ipRecv, "x", dx );
    elementValue( ipRecv, "y", dy );
    elementValue( ipRecv, "roll", droll );

    if( dx != 0 || dy != 0 || droll != 0 )
    {
        // Relative to where the mount is, drift included, so a guiding correction
        // removes the error it measured. During a slew the base is in transit, so
        // offset the destination instead of abandoning the slew.
        double ra, dec, pa;

        { //mutex scope
            std::lock_guard<std::mutex> lock( m_mountMutex );

            if( m_state == telSimState::slewing )
            {
                ra = m_targetRA;
                dec = m_targetDec;
                pa = m_targetPA;
            }
            else
            {
                ra = m_baseRA;
                dec = m_baseDec;
                pa = m_basePA;
            }
        }

        double nra = ra, ndec = dec;

        if( dx != 0 || dy != 0 )
        {
            wcc::offsetBoresight( ra, dec, pa, m_parity, dx, dy, nra, ndec );
        }

        commandTarget( nra, ndec, pa + droll,
                       "offset " + std::to_string( dx ) + ", " + std::to_string( dy ) + " arcsec, roll " +
                           std::to_string( droll ) + " deg" );
    }

    // Offsets are relative, so clear the request once applied. Always SET: the
    // local copy is already 0, so updateIfChanged would leave the client's
    // request (or NaN) showing on the server.
    { //mutex scope
        std::lock_guard<std::mutex> lock( m_indiMutex );

        try
        {
            m_indiP_offset["x"].set( 0.0 );
            m_indiP_offset["y"].set( 0.0 );
            m_indiP_offset["roll"].set( 0.0 );

            if( m_indiDriver )
            {
                m_indiDriver->sendSetProperty( m_indiP_offset );
            }
        }
        catch( ... )
        {
        }
    }

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_stopTracking )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_stopTracking, ipRecv );

    if( !ipRecv.find( "toggle" ) )
    {
        return 0;
    }

    const bool stop = ( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On );

    setTrackingStopped( stop );

    if( stop )
    {
        // Stopping ends whatever command was being tracked.
        updateSwitchIfChanged( m_indiP_go, "toggle", pcf::IndiElement::Off, INDI_IDLE );
        updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::Off, INDI_IDLE );
    }

    updateSwitchIfChanged( m_indiP_stopTracking, "toggle", stop ? pcf::IndiElement::On : pcf::IndiElement::Off,
                           stop ? INDI_IDLE : INDI_OK );

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_trackDrift )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_trackDrift, ipRecv );

    double ra = std::numeric_limits<double>::quiet_NaN();
    double dec = ra, pa = ra;
    elementValue( ipRecv, "ra", ra );
    elementValue( ipRecv, "dec", dec );
    elementValue( ipRecv, "pa", pa );

    applyDriftRates( false, ra, dec, pa );
    publishDriftIndi( false );

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_idleDrift )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_idleDrift, ipRecv );

    double ra = std::numeric_limits<double>::quiet_NaN();
    double dec = ra, pa = ra;
    elementValue( ipRecv, "ra", ra );
    elementValue( ipRecv, "dec", dec );
    elementValue( ipRecv, "pa", pa );

    applyDriftRates( true, ra, dec, pa );
    publishDriftIndi( true );

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_slewRate )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_slewRate, ipRecv );
    return handleSimNumberNew( m_indiP_slewRate, ipRecv, telSimIndiParam::slewRate );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_rollRate )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_rollRate, ipRecv );
    return handleSimNumberNew( m_indiP_rollRate, ipRecv, telSimIndiParam::rollRate );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_jitterX )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_jitterX, ipRecv );
    return handleSimNumberNew( m_indiP_jitterX, ipRecv, telSimIndiParam::jitterX );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_jitterY )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_jitterY, ipRecv );
    return handleSimNumberNew( m_indiP_jitterY, ipRecv, telSimIndiParam::jitterY );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_jitterRoll )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_jitterRoll, ipRecv );
    return handleSimNumberNew( m_indiP_jitterRoll, ipRecv, telSimIndiParam::jitterRoll );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_settleTime )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_settleTime, ipRecv );
    return handleSimNumberNew( m_indiP_settleTime, ipRecv, telSimIndiParam::settleTime );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_arriveTol )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_arriveTol, ipRecv );
    return handleSimNumberNew( m_indiP_arriveTol, ipRecv, telSimIndiParam::arriveTol );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_jitterTau )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_jitterTau, ipRecv );
    return handleSimNumberNew( m_indiP_jitterTau, ipRecv, telSimIndiParam::jitterTau );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_writeHz )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_writeHz, ipRecv );
    return handlePointingNumberNew( m_indiP_writeHz, ipRecv, true );
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_historyS )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_historyS, ipRecv );
    return handlePointingNumberNew( m_indiP_historyS, ipRecv, false );
}

} // namespace app
} // namespace MagAOX

#endif // telescopeSim_hpp
