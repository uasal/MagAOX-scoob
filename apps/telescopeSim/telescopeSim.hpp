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
 * written to an ImageStreamIO stream at a configurable rate (1000 Hz by default)
 * so `wccSim` can integrate mount motion during a camera exposure. The same
 * pointing is also published on the INDI `pointing` property at 1 Hz, which is
 * what `wccCtrl` and operators read. `wccCtrl` sends corrections to `offset` and
 * watches `teldata` to know when a move is done. The property and element names
 * match what `wccCtrl` expects from a real `tcsInterface` closely enough that
 * pointing it at either is a configuration change rather than a code change.
 *
 * \par Interface summary
 * | property      | type   | purpose                                              |
 * |---------------|--------|------------------------------------------------------|
 * | `start_visit` | toggle | Slew to the target `visitCtrl` is publishing         |
 * | `pointing`    | number | Current pointing: `ra`, `dec`, `pa` (1 Hz INDI)      |
 * | `target`      | number | Where it is going: `ra`, `dec`, `pa`                 |
 * | `teldata`     | number | `slewing`, `tracking`, `settling`. Done detection    |
 * | `tel_status`  | text   | `state`, `message`                                   |
 * | `offset`      | number | Relative correction: `x`, `y` [arcsec], `roll` [deg] |
 * | `goto_target` | number | Absolute command: `ra`, `dec`, `pa`                  |
 *
 * High-rate pointing lives on the `telpointing` shmim (configurable): a 3×1×N
 * circular buffer of doubles, axes RA, Dec, PA in degrees, written at
 * `pointing.write_hz`.
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
    idle,     ///< Parked, no target commanded.
    slewing,  ///< Moving toward the target.
    settling, ///< Arrived, waiting out the settle time.
    tracking  ///< On target, wandering by the configured jitter.
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

    double m_slewRate{ 1.0 }; ///< Slew rate on the sky [deg/s].

    double m_rollRate{ 1.0 }; ///< Rotation rate [deg/s].

    double m_jitter{ 0.2 }; ///< Pointing jitter, rms per axis [arcsec].

    double m_jitterRoll{ 0.002 }; ///< Roll jitter, rms [deg].

    double m_drift{ 0.0 }; ///< Systematic drift along focal plane X [arcsec/min].

    double m_driftY{ 0.0 }; ///< Systematic drift along focal plane Y [arcsec/min].

    double m_settleTime{ 2.0 }; ///< Time spent settling after a slew [s].

    /// Distance below which the mount is considered to have arrived [arcsec].
    double m_arriveTol{ 1.0 };

    /// Handedness of focal plane X relative to increasing right ascension. Must match wccSim.
    double m_parity{ -1.0 };

    double m_startRA{ 0 }; ///< Pointing at startup [deg].

    double m_startDec{ 0 }; ///< Pointing at startup [deg].

    double m_startPA{ 0 }; ///< Position angle at startup [deg].

    uint64_t m_seed{ 8675309 }; ///< RNG seed for the jitter.

    /// ImageStreamIO stream the high-rate pointing is written to.
    std::string m_pointingShmim{ wcc::pointingShmimDefault };

    double m_writeHz{ wcc::pointingWriteHzDefault }; ///< Pointing shmim write rate [Hz].

    double m_historyS{ wcc::pointingHistorySDefault }; ///< Circular-buffer span [s].

    /// Correlation time of the pointing jitter [s]. 0 is white (independent draws).
    double m_jitterTau{ 0.05 };
    ///@}

    /** \name Mount State - Data
     *@{
     */
  protected:
    std::mutex m_mountMutex; ///< Guards the mount state below.

    telSimState m_state{ telSimState::idle }; ///< What the mount is doing.

    std::string m_message; ///< Why the mount is in its current state.

    /// Commanded pointing, before jitter is added.
    double m_baseRA{ 0 };

    double m_baseDec{ 0 }; ///< Commanded declination, before jitter [deg].

    double m_basePA{ 0 }; ///< Commanded position angle, before jitter [deg].

    double m_targetRA{ 0 }; ///< Target right ascension [deg].

    double m_targetDec{ 0 }; ///< Target declination [deg].

    double m_targetPA{ 0 }; ///< Target position angle [deg].

    bool m_haveTarget{ false }; ///< True once a target has been commanded.

    /// Reported pointing, which is the commanded pointing plus jitter.
    double m_reportRA{ 0 };

    double m_reportDec{ 0 }; ///< Reported declination [deg].

    double m_reportPA{ 0 }; ///< Reported position angle [deg].

    double m_settleStart{ 0 }; ///< When settling began [s].

    double m_lastUpdate{ 0 }; ///< Time of the last motion update [s].

    wcc::fastRandom m_rng; ///< Jitter generator.

    /// Instantaneous jitter state [arcsec], evolved as an Ornstein-Uhlenbeck process.
    double m_jx{ 0 };

    double m_jy{ 0 }; ///< Instantaneous Y jitter [arcsec].

    double m_jroll{ 0 }; ///< Instantaneous roll jitter [deg].
    ///@}

    /** \name Pointing Stream - Data
     *@{
     */
  protected:
    IMAGE m_pointingStream{}; ///< High-rate pointing ImageStreamIO stream.

    bool m_pointingStreamOpen{ false }; ///< True while m_pointingStream is created.

    uint32_t m_pointingDepth{ 1 }; ///< Circular buffer length of the pointing stream.

    std::thread m_pointingThread; ///< Worker that advances the mount and writes the shmim.

    std::atomic<bool> m_shutdownPointing{ false }; ///< Tells the pointing thread to exit.
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

    pcf::IndiProperty m_indiP_goto; ///< Absolute pointing command.
    INDI_NEWCALLBACK_DECL( telescopeSim, m_indiP_goto );

    pcf::IndiProperty m_indiP_pointing; ///< Current pointing. This is what wccSim reads.

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
    /// Advance the mount toward its target and apply jitter.
    /** Called from the pointing worker at the shmim write rate, and from unit
     * tests. Elapsed time is measured rather than assumed, so the write rate
     * sets the jitter bandwidth rather than the slew fidelity.
     */
    void updateMount();

    /// Command an absolute target and begin slewing.
    void commandTarget( double ra /**< [in] right ascension [deg] */,
                        double dec /**< [in] declination [deg] */,
                        double pa /**< [in] position angle [deg] */,
                        const std::string &why /**< [in] what asked for this, for the log */ );

    /// Publish the pointing, target, motion flags and status.
    void publishState();

    /// Create the high-rate pointing stream.
    /** \returns 0 on success
     * \returns -1 if the stream cannot be created
     */
    int ensurePointingStream();

    /// Write the current reported pointing into the next circular-buffer slice.
    void writePointingShmim();

    /// Thread entry for the pointing worker.
    static void pointingWorkerStart( telescopeSim *s /**< [in] this */ );

    /// Advance the mount and write the pointing shmim at m_writeHz.
    void pointingWorkerExec();

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
                "Pointing jitter, rms per axis [arcsec]. 0 gives a perfectly steady mount." );
    config.add( "sim.jitter_roll", "", "sim.jitter_roll", argType::Required, "sim", "jitter_roll", false,
                "double", "Roll jitter, rms [deg]." );
    config.add( "sim.drift_x", "", "sim.drift_x", argType::Required, "sim", "drift_x", false, "double",
                "Systematic drift along focal plane X [arcsec/min]. Exercises the guiding loop." );
    config.add( "sim.drift_y", "", "sim.drift_y", argType::Required, "sim", "drift_y", false, "double",
                "Systematic drift along focal plane Y [arcsec/min]." );
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
                "ImageStreamIO stream the high-rate pointing is written to. 3x1xN doubles: ra, dec, pa." );
    config.add( "pointing.write_hz", "", "pointing.write_hz", argType::Required, "pointing", "write_hz", false,
                "double", "Pointing shmim write rate [Hz]. 0 disables the stream and updates only at 1 Hz." );
    config.add( "pointing.history_s", "", "pointing.history_s", argType::Required, "pointing", "history_s",
                false, "double",
                "Circular-buffer span [s]. Must cover the longest camera exposure wccSim will integrate." );
}

inline int telescopeSim::loadConfigImpl( mx::app::appConfigurator &_config )
{
    _config( m_visitDevice, "visit.device" );

    _config( m_slewRate, "sim.slew_rate" );
    _config( m_rollRate, "sim.roll_rate" );
    _config( m_jitter, "sim.jitter" );
    _config( m_jitterRoll, "sim.jitter_roll" );
    _config( m_drift, "sim.drift_x" );
    _config( m_driftY, "sim.drift_y" );
    _config( m_settleTime, "sim.settle_time" );
    _config( m_arriveTol, "sim.arrive_tol" );
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

    _config( m_jitterTau, "sim.jitter_tau" );
    _config( m_pointingShmim, "pointing.shmim" );
    _config( m_writeHz, "pointing.write_hz" );
    _config( m_historyS, "pointing.history_s" );

    if( m_jitterTau < 0 )
    {
        m_jitterTau = 0;
    }

    if( m_writeHz < 0 )
    {
        m_writeHz = 0;
    }

    if( m_historyS <= 0 )
    {
        m_historyS = wcc::pointingHistorySDefault;
    }

    if( m_pointingShmim.empty() )
    {
        m_pointingShmim = wcc::pointingShmimDefault;
    }

    m_pointingDepth = wcc::pointingBufferDepth( m_writeHz, m_historyS );

    if( m_slewRate <= 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, "sim.slew_rate must be positive" } );
    }

    if( m_rollRate <= 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, "sim.roll_rate must be positive" } );
    }

    if( m_jitter < 0 )
    {
        m_jitter = 0;
    }

    if( m_jitterRoll < 0 )
    {
        m_jitterRoll = 0;
    }

    if( m_arriveTol <= 0 )
    {
        m_arriveTol = 1.0;
    }

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

    REG_INDI_NEWPROP( m_indiP_goto, "goto_target", pcf::IndiProperty::Number );
    m_indiP_goto.add( pcf::IndiElement( "ra" ) );
    m_indiP_goto.add( pcf::IndiElement( "dec" ) );
    m_indiP_goto.add( pcf::IndiElement( "pa" ) );
    m_indiP_goto["ra"].set( m_startRA );
    m_indiP_goto["dec"].set( m_startDec );
    m_indiP_goto["pa"].set( m_startPA );

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
        log<text_log>( "no visit device configured; start_visit is disabled and only goto_target and offset "
                       "will move the mount",
                       logPrio::LOG_WARNING );
    }

    m_lastUpdate = mx::sys::get_curr_time();

    if( m_writeHz > 0 )
    {
        if( ensurePointingStream() < 0 )
        {
            return -1;
        }

        m_shutdownPointing = false;
        m_pointingThread = std::thread( pointingWorkerStart, this );
    }

    state( stateCodes::READY );

    log<text_log>( "telescopeSim ready at ra " + std::to_string( m_startRA ) + " dec " +
                   std::to_string( m_startDec ) + " pa " + std::to_string( m_startPA ) + ", slew rate " +
                   std::to_string( m_slewRate ) + " deg/s, jitter " + std::to_string( m_jitter ) +
                   " arcsec rms, pointing shmim " + m_pointingShmim + " at " + std::to_string( m_writeHz ) +
                   " Hz" );

    return 0;
}

inline int telescopeSim::appLogic()
{
    if( m_writeHz <= 0 )
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
    m_shutdownPointing = true;

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

    if( m_pointingStreamOpen )
    {
        ImageStreamIO_destroyIm( &m_pointingStream );
        m_pointingStreamOpen = false;
    }

    return 0;
}

inline void telescopeSim::updateMount()
{
    const double now = mx::sys::get_curr_time();

    std::lock_guard<std::mutex> lock( m_mountMutex );

    const double dt = now - m_lastUpdate;
    m_lastUpdate = now;

    if( dt <= 0 || dt > 60.0 )
    {
        // First call, a clock step, or a long stall: do not integrate a huge slew.
        return;
    }

    // ----------------------------------------------------------------- drift
    if( m_state == telSimState::tracking && ( m_drift != 0 || m_driftY != 0 ) )
    {
        const double dx = m_drift * dt / 60.0;
        const double dy = m_driftY * dt / 60.0;

        double nra, ndec;
        wcc::offsetBoresight( m_baseRA, m_baseDec, m_basePA, m_parity, dx, dy, nra, ndec );

        m_baseRA = nra;
        m_baseDec = ndec;
    }

    // ------------------------------------------------------------------ slew
    if( m_state == telSimState::slewing )
    {
        const double sep = wcc::angularSeparation( m_baseRA, m_baseDec, m_targetRA, m_targetDec );
        const double paErr = m_targetPA - m_basePA;

        const double maxStep = m_slewRate * dt;
        const double maxRoll = m_rollRate * dt;

        // Arrival is whether this step *reached* the target, not whether the mount
        // had to move to get there. A step that covers the whole remaining distance
        // arrives now; treating it as still slewing would cost a full update, and at
        // the 1 Hz reporting rate that is a wasted second on every small guiding
        // correction.
        bool atPosition = ( sep <= m_arriveTol * wcc::arcsec2deg );

        if( !atPosition )
        {
            if( sep <= maxStep )
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
            m_state = telSimState::settling;
            m_settleStart = now;
            m_message = "settling";
        }
    }
    else if( m_state == telSimState::settling )
    {
        if( now - m_settleStart >= m_settleTime )
        {
            m_state = telSimState::tracking;
            m_message = "tracking";
            log<text_log>( "arrived at ra " + std::to_string( m_baseRA ) + " dec " +
                           std::to_string( m_baseDec ) + " pa " + std::to_string( m_basePA ) );
        }
    }

    // ---------------------------------------------------------------- jitter
    // Jitter is applied to the reported pointing, not accumulated into the
    // commanded one, so it is a wander about where the mount actually is rather
    // than a random walk that would run away. A finite correlation time makes
    // successive writes a smooth trail, which is what a long exposure should
    // integrate into a streak; tau = 0 is white, matching the original 1 Hz draws.
    if( m_state == telSimState::tracking || m_state == telSimState::settling )
    {
        auto ouStep = [&]( double &x, double sigma ) {
            if( !( sigma > 0 ) )
            {
                x = 0;
                return;
            }

            if( m_jitterTau <= 0 )
            {
                x = sigma * m_rng.normal();
                return;
            }

            const double a = std::exp( -dt / m_jitterTau );
            const double s = sigma * std::sqrt( std::max( 0.0, 1.0 - a * a ) );
            x = a * x + s * m_rng.normal();
        };

        ouStep( m_jx, m_jitter );
        ouStep( m_jy, m_jitter );
        ouStep( m_jroll, m_jitterRoll );
    }
    else
    {
        m_jx = 0;
        m_jy = 0;
        m_jroll = 0;
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
}

inline int telescopeSim::ensurePointingStream()
{
    if( m_pointingStreamOpen )
    {
        return 0;
    }

    if( m_pointingDepth < 1 )
    {
        m_pointingDepth = 1;
    }

    uint32_t sizes[3] = { wcc::pointingNAxes, 1, m_pointingDepth };

    if( ImageStreamIO_createIm_gpu( &m_pointingStream, m_pointingShmim.c_str(), 3, sizes, IMAGESTRUCT_DOUBLE, -1,
                                    1, IMAGE_NB_SEMAPHORE, 0, CIRCULAR_BUFFER | ZAXIS_TEMPORAL, 0 ) !=
        IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>(
            { __FILE__, __LINE__, "failed to create pointing stream " + m_pointingShmim } );
    }

    m_pointingStream.md->cnt1 = m_pointingDepth - 1;
    m_pointingStreamOpen = true;

    log<text_log>( "created pointing stream " + m_pointingShmim + " 3x1x" + std::to_string( m_pointingDepth ) +
                   " double at " + std::to_string( m_writeHz ) + " Hz" );

    return 0;
}

inline void telescopeSim::writePointingShmim()
{
    if( !m_pointingStreamOpen || m_pointingStream.md == nullptr )
    {
        return;
    }

    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        ra = m_reportRA;
        dec = m_reportDec;
        pa = m_reportPA;
    }

    m_pointingStream.md->write = 1;

    const uint64_t slice =
        ( m_pointingDepth > 1 ) ? ( m_pointingStream.md->cnt1 + 1 ) % m_pointingDepth : 0;

    double *dest = reinterpret_cast<double *>( m_pointingStream.array.raw ) + slice * wcc::pointingNAxes;
    wcc::packPointing( dest, ra, dec, pa );

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

inline void telescopeSim::pointingWorkerStart( telescopeSim *s )
{
    s->pointingWorkerExec();
}

inline void telescopeSim::pointingWorkerExec()
{
    const double period = ( m_writeHz > 0 ) ? 1.0 / m_writeHz : 0.001;
    double next = mx::sys::get_curr_time();

    while( !m_shutdownPointing.load() && !m_shutdown )
    {
        updateMount();
        writePointingShmim();

        next += period;
        const double now = mx::sys::get_curr_time();
        const double remain = next - now;

        if( remain > 0 )
        {
            mx::sys::microSleep( static_cast<unsigned>( remain * 1e6 ) );
        }
        else
        {
            // Fell behind: do not try to catch up a backlog of writes, which
            // would burst-fill the buffer with stale timestamps.
            next = now;
        }
    }
}

inline void telescopeSim::commandTarget( double ra, double dec, double pa, const std::string &why )
{
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

inline void telescopeSim::publishState()
{
    double ra, dec, pa, tra, tdec, tpa;
    telSimState st;
    std::string msg;

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
    }

    updateIfChanged( m_indiP_pointing, "ra", ra );
    updateIfChanged( m_indiP_pointing, "dec", dec );
    updateIfChanged( m_indiP_pointing, "pa", pa );

    updateIfChanged( m_indiP_target, "ra", tra );
    updateIfChanged( m_indiP_target, "dec", tdec );
    updateIfChanged( m_indiP_target, "pa", tpa );

    updateIfChanged( m_indiP_teldata, "slewing", ( st == telSimState::slewing ) ? 1.0 : 0.0 );
    updateIfChanged( m_indiP_teldata, "settling", ( st == telSimState::settling ) ? 1.0 : 0.0 );
    updateIfChanged( m_indiP_teldata, "tracking", ( st == telSimState::tracking ) ? 1.0 : 0.0 );

    updateIfChanged( m_indiP_status, "state", telSimStateName( st ) );
    updateIfChanged( m_indiP_status, "message", msg );
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

        if( end == s.c_str() || !std::isfinite( v ) )
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
        // Toggling off stops tracking the visit but leaves the mount where it is.
        { //mutex scope
            std::lock_guard<std::mutex> lock( m_mountMutex );
            m_state = telSimState::idle;
            m_message = "visit stopped";
        }

        updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::Off, INDI_IDLE );
        log<text_log>( "visit stopped; mount parked where it was" );

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

    commandTarget( ra, dec, pa, "start_visit from " + m_visitDevice );

    updateSwitchIfChanged( m_indiP_startVisit, "toggle", pcf::IndiElement::On, INDI_BUSY );

    return 0;
}

INDI_NEWCALLBACK_DEFN( telescopeSim, m_indiP_goto )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_goto, ipRecv );

    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        ra = m_targetRA;
        dec = m_targetDec;
        pa = m_targetPA;
    }

    elementValue( ipRecv, "ra", ra );
    elementValue( ipRecv, "dec", dec );
    elementValue( ipRecv, "pa", pa );

    commandTarget( ra, dec, pa, "goto_target" );

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

    if( dx == 0 && dy == 0 && droll == 0 )
    {
        return 0;
    }

    // The offset is relative to where the mount currently is, not to the previous
    // target, so a correction applied while still settling does the right thing.
    double ra, dec, pa;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_mountMutex );
        ra = m_baseRA;
        dec = m_baseDec;
        pa = m_basePA;
    }

    double nra = ra, ndec = dec;

    if( dx != 0 || dy != 0 )
    {
        wcc::offsetBoresight( ra, dec, pa, m_parity, dx, dy, nra, ndec );
    }

    commandTarget( nra, ndec, pa + droll,
                   "offset " + std::to_string( dx ) + ", " + std::to_string( dy ) + " arcsec, roll " +
                       std::to_string( droll ) + " deg" );

    // Offsets are relative, so clear the request once applied.
    updateIfChanged( m_indiP_offset, "x", 0.0 );
    updateIfChanged( m_indiP_offset, "y", 0.0 );
    updateIfChanged( m_indiP_offset, "roll", 0.0 );

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // telescopeSim_hpp
