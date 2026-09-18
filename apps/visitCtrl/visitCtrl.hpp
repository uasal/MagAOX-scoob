/** \file visitCtrl.hpp
 * \brief The MagAO-X WCC visit file loader.
 * \author Adam Schilperoort
 *
 * \ingroup visitCtrl_files
 */

#ifndef visitCtrl_hpp
#define visitCtrl_hpp

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include "../wccCommon/wccIndiRate.hpp"
#include "../wccCommon/wccVisit.hpp"
#include "../wccCommon/wccVisitIndi.hpp"

/** \defgroup visitCtrl WCC Visit File Loader
 * \brief Reads a WCC visit file and republishes it as INDI properties.
 *
 * This is the only application that parses a visit file. Everything else in the
 * WCC chain, `telescopeSim` for where to point and `wccCtrl` for the acquisition
 * parameters, reads the visit over INDI instead of opening the file itself. That
 * keeps one parser and one interpretation of the schema, and it means an operator
 * can inspect exactly what the system believes the visit says.
 *
 * The interface is two controls: a text property naming the file, and a toggle
 * that reads it. On a successful read every parameter appears on the read-only
 * properties described in wccVisitIndi.hpp.
 *
 * A visit file offers several ranked guide and roll star candidates.
 * `select_rank` chooses which pair is published, and changing it republishes from
 * the already-parsed file without re-reading it.
 *
 * <a href="../handbook/operating/software/apps/visitCtrl.html">Application Documentation</a>
 *
 * \ingroup apps
 */

/** \defgroup visitCtrl_files WCC Visit File Loader Files
 * \ingroup visitCtrl
 */

namespace MagAOX
{
namespace app
{

/// The MagAO-X WCC visit file loader.
/** \ingroup visitCtrl
 */
class visitCtrl : public MagAOXApp<true>
{

    // Give the test harness access.
    friend class visitCtrl_test;

    /** \name Configurable Parameters - Data
     *@{
     */
  protected:
    std::string m_visitPath; ///< Path of the visit file to read.

    /// Whether to read the configured visit file at startup without a load request.
    bool m_loadAtStartup{ false };

    /// Rank to publish. 0 means the highest ranked pair the visit offers.
    int m_selectRank{ 0 };
    ///@}

    /** \name Loader State - Data
     *@{
     */
  protected:
    std::mutex m_visitMutex; ///< Guards the parsed visit and the selection below.

    wcc::visitFile m_visit; ///< The parsed visit file.

    bool m_parsed{ false }; ///< True once a file has parsed successfully.

    wcc::visitSelection m_selection; ///< The currently published selection.

    std::string m_state{ wcc::visitIndi::stateEmpty }; ///< Loader state.

    std::string m_message; ///< Why the loader is in its current state.

    std::string m_loadedPath; ///< Path that was actually read.

    /// Set when a load has been requested and not yet serviced by appLogic.
    /** The read happens in appLogic rather than in the INDI callback so that a
     * large file cannot stall the INDI driver thread.
     */
    std::atomic<bool> m_loadRequest{ false };

    /// Set when the selection changed and needs republishing.
    std::atomic<bool> m_republish{ false };
    ///@}

    /** \name INDI - Data
     *@{
     */
  protected:
    pcf::IndiProperty m_indiP_visitFile; ///< Path of the visit file to read.
    INDI_NEWCALLBACK_DECL( visitCtrl, m_indiP_visitFile );

    pcf::IndiProperty m_indiP_load; ///< Toggle that reads the visit file.
    INDI_NEWCALLBACK_DECL( visitCtrl, m_indiP_load );

    pcf::IndiProperty m_indiP_selectRank; ///< Which ranked star pair to publish.
    INDI_NEWCALLBACK_DECL( visitCtrl, m_indiP_selectRank );

    pcf::IndiProperty m_indiP_status; ///< Loader state, message and path.

    pcf::IndiProperty m_indiP_target; ///< Requested boresight.

    pcf::IndiProperty m_indiP_targetInfo; ///< Target identification.

    pcf::IndiProperty m_indiP_guideStar; ///< Selected guide star.

    pcf::IndiProperty m_indiP_guideStarInfo; ///< Selected guide star identification.

    pcf::IndiProperty m_indiP_rollStar; ///< Selected roll star.

    pcf::IndiProperty m_indiP_rollStarInfo; ///< Selected roll star identification.

    pcf::IndiProperty m_indiP_acqParams; ///< Acquisition parameters.

    pcf::IndiProperty m_indiP_trackParams; ///< Tracking parameters.

    pcf::IndiProperty m_indiP_trackDevices; ///< Centroid controller device names.

    pcf::IndiProperty m_indiP_configSensors; ///< Sensors to solve astrometry on.
    ///@}

  public:
    /// Default c'tor.
    visitCtrl();

    /// D'tor, declared and defined for noexcept.
    ~visitCtrl() noexcept;

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** \returns 0 on success
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] configuration to load from */ );

    virtual void loadConfig();

    virtual int appStartup();

    /// Implementation of the FSM for visitCtrl.
    /** \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
    virtual int appLogic();

    virtual int appShutdown();

  protected:
    /// Read the visit file at m_visitPath and publish the resulting selection.
    /** \returns 0 on success
     * \returns -1 if the file cannot be read or has no usable guide star
     */
    int loadVisitFile();

    /// Resolve the selection at the current rank from the already-parsed visit.
    /** \returns 0 on success
     * \returns -1 if no pair exists at the requested rank
     */
    int applySelection();

    /// Create the read-only properties carrying the visit.
    /** \returns 0 on success
     * \returns -1 on a creation or registration failure
     */
    int createVisitProperties();

    /// Publish the current selection onto the read-only properties.
    void publishSelection();

    /// Publish the loader state, message and path.
    void publishStatus();

    /// Add the standard star elements to a number property.
    static void addStarElements( pcf::IndiProperty &prop /**< [in,out] the property */ );

    /// Publish one star onto a number property and its text companion.
    void publishStar( pcf::IndiProperty &prop /**< [in,out] the number property */,
                      pcf::IndiProperty &info /**< [in,out] the text property */,
                      const wcc::visitStar &star /**< [in] the star to publish */ );
};

inline visitCtrl::visitCtrl() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    // Pure software: no PDU.
    m_powerMgtEnabled = false;

    // Every WCC application holds its INDI traffic to 1 Hz.
    m_loopPause = wcc::indiLoopPause;

    return;
}

inline visitCtrl::~visitCtrl() noexcept
{
    return;
}

inline void visitCtrl::setupConfig()
{
    config.add( "visit.path", "", "visit.path", argType::Required, "visit", "path", false, "string",
                "Path of the visit file to read. May also be set at runtime through the visit_file property." );
    config.add( "visit.load_at_startup", "", "visit.load_at_startup", argType::Required, "visit",
                "load_at_startup", false, "bool",
                "Read the configured visit file at startup rather than waiting for a load request." );
    config.add( "visit.select_rank", "", "visit.select_rank", argType::Required, "visit", "select_rank", false,
                "int", "Which ranked guide and roll star pair to publish. 0 means the highest available." );
}

inline int visitCtrl::loadConfigImpl( mx::app::appConfigurator &_config )
{
    _config( m_visitPath, "visit.path" );
    _config( m_loadAtStartup, "visit.load_at_startup" );
    _config( m_selectRank, "visit.select_rank" );

    if( m_selectRank < 0 )
    {
        m_selectRank = 0;
    }

    return 0;
}

inline void visitCtrl::loadConfig()
{
    loadConfigImpl( config );
}

inline void visitCtrl::addStarElements( pcf::IndiProperty &prop )
{
    prop.add( pcf::IndiElement( wcc::visitIndi::elRank ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elRA ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elDec ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elFieldX ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elFieldY ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elMag ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elTargetX ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elTargetY ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elExpTime ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elFrameRate ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elROIW ) );
    prop.add( pcf::IndiElement( wcc::visitIndi::elROIH ) );
}

inline int visitCtrl::createVisitProperties()
{
    using namespace wcc::visitIndi;

    if( createROIndiText( m_indiP_status, propStatus, elState, "Visit loader status", "visit", elState ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText visit_status" } );
    }
    m_indiP_status.add( pcf::IndiElement( elMessage ) );
    m_indiP_status.add( pcf::IndiElement( elPath ) );
    m_indiP_status[elState].set( m_state );
    registerIndiPropertyReadOnly( m_indiP_status );

    if( createROIndiNumber( m_indiP_target, propTarget, "Requested boresight", "visit" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber target" } );
    }
    m_indiP_target.add( pcf::IndiElement( elRA ) );
    m_indiP_target.add( pcf::IndiElement( elDec ) );
    m_indiP_target.add( pcf::IndiElement( elRollPA ) );
    registerIndiPropertyReadOnly( m_indiP_target );

    if( createROIndiText( m_indiP_targetInfo, propTargetInfo, elName, "Target identification", "visit",
                          elName ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText target_info" } );
    }
    m_indiP_targetInfo.add( pcf::IndiElement( elProgID ) );
    m_indiP_targetInfo.add( pcf::IndiElement( elObsID ) );
    m_indiP_targetInfo.add( pcf::IndiElement( elVisitID ) );
    registerIndiPropertyReadOnly( m_indiP_targetInfo );

    if( createROIndiNumber( m_indiP_guideStar, propGuideStar, "Selected guide star", "guide" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber guide_star" } );
    }
    addStarElements( m_indiP_guideStar );
    registerIndiPropertyReadOnly( m_indiP_guideStar );

    if( createROIndiText( m_indiP_guideStarInfo, propGuideStarInfo, elID, "Guide star identification", "guide",
                          elID ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText guide_star_info" } );
    }
    m_indiP_guideStarInfo.add( pcf::IndiElement( elSensor ) );
    m_indiP_guideStarInfo.add( pcf::IndiElement( elCatalog ) );
    registerIndiPropertyReadOnly( m_indiP_guideStarInfo );

    if( createROIndiNumber( m_indiP_rollStar, propRollStar, "Selected roll star", "roll" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber roll_star" } );
    }
    addStarElements( m_indiP_rollStar );
    registerIndiPropertyReadOnly( m_indiP_rollStar );

    if( createROIndiText( m_indiP_rollStarInfo, propRollStarInfo, elID, "Roll star identification", "roll",
                          elID ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText roll_star_info" } );
    }
    m_indiP_rollStarInfo.add( pcf::IndiElement( elSensor ) );
    m_indiP_rollStarInfo.add( pcf::IndiElement( elCatalog ) );
    registerIndiPropertyReadOnly( m_indiP_rollStarInfo );

    if( createROIndiNumber( m_indiP_acqParams, propAcqParams, "Acquisition parameters", "acq" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber acq_params" } );
    }
    m_indiP_acqParams.add( pcf::IndiElement( elTAExpTime ) );
    m_indiP_acqParams.add( pcf::IndiElement( elTAFrameRate ) );
    m_indiP_acqParams.add( pcf::IndiElement( elGuideTol ) );
    m_indiP_acqParams.add( pcf::IndiElement( elRollTol ) );
    m_indiP_acqParams.add( pcf::IndiElement( elMaxIter ) );
    m_indiP_acqParams.add( pcf::IndiElement( elNSensors ) );
    registerIndiPropertyReadOnly( m_indiP_acqParams );

    if( createROIndiNumber( m_indiP_trackParams, propTrackParams, "Tracking parameters", "track" ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiNumber track_params" } );
    }
    m_indiP_trackParams.add( pcf::IndiElement( elROIW ) );
    m_indiP_trackParams.add( pcf::IndiElement( elROIH ) );
    m_indiP_trackParams.add( pcf::IndiElement( elFrameRate ) );
    m_indiP_trackParams.add( pcf::IndiElement( elExpTime ) );
    m_indiP_trackParams.add( pcf::IndiElement( elLoopGain ) );
    m_indiP_trackParams.add( pcf::IndiElement( elRollGain ) );
    registerIndiPropertyReadOnly( m_indiP_trackParams );

    if( createROIndiText( m_indiP_trackDevices, propTrackDevices, elCentroidGuide,
                          "Centroid controller devices", "track", elCentroidGuide ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText track_devices" } );
    }
    m_indiP_trackDevices.add( pcf::IndiElement( elCentroidRoll ) );
    registerIndiPropertyReadOnly( m_indiP_trackDevices );

    if( createROIndiText( m_indiP_configSensors, propConfigSensors, elList, "Sensors in the solution", "acq",
                          elList ) < 0 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createROIndiText config_sensors" } );
    }
    registerIndiPropertyReadOnly( m_indiP_configSensors );

    return 0;
}

inline int visitCtrl::appStartup()
{
    using namespace wcc::visitIndi;

    CREATE_REG_INDI_NEW_TEXT( m_indiP_visitFile, propVisitFile, "Visit file path", "visit" );
    m_indiP_visitFile["current"].set( m_visitPath );
    m_indiP_visitFile["target"].set( m_visitPath );

    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_load, propLoad );

    CREATE_REG_INDI_NEW_NUMBERI( m_indiP_selectRank, propSelectRank, 0, 99, 1, "%d",
                                 "Ranked star pair to publish", "visit" );
    m_indiP_selectRank["current"].set( m_selectRank );
    m_indiP_selectRank["target"].set( m_selectRank );

    if( createVisitProperties() < 0 )
    {
        return -1;
    }

    if( m_loadAtStartup && !m_visitPath.empty() )
    {
        m_loadRequest = true;
    }

    state( stateCodes::READY );

    log<text_log>( "visitCtrl ready; set " + std::string( propVisitFile ) + " and toggle " +
                   std::string( propLoad ) + " to publish a visit" );

    return 0;
}

inline int visitCtrl::appLogic()
{
    // The file read happens here, not in the INDI callback, so a slow or large
    // file cannot stall the INDI driver thread.
    if( m_loadRequest.exchange( false ) )
    {
        if( loadVisitFile() < 0 )
        {
            // Already logged and reflected in the status property.
            updateSwitchIfChanged( m_indiP_load, "toggle", pcf::IndiElement::Off, INDI_ALERT );
        }
        else
        {
            updateSwitchIfChanged( m_indiP_load, "toggle", pcf::IndiElement::On, INDI_OK );
        }
    }

    if( m_republish.exchange( false ) )
    {
        applySelection();
    }

    bool haveVisit = false;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        haveVisit = m_selection.m_valid;
    }

    if( m_state == wcc::visitIndi::stateError )
    {
        state( stateCodes::ERROR );
    }
    else if( haveVisit )
    {
        state( stateCodes::READY );
    }
    else
    {
        state( stateCodes::NODEVICE );
    }

    // appLogic runs at m_loopPause, which is the 1 Hz INDI rate limit, so
    // publishing unconditionally here is already rate limited.
    publishStatus();
    publishSelection();

    return 0;
}

inline int visitCtrl::appShutdown()
{
    return 0;
}

inline int visitCtrl::loadVisitFile()
{
    std::string path;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        path = m_visitPath;
    }

    if( path.empty() )
    {
        std::lock_guard<std::mutex> lock( m_visitMutex );
        m_state = wcc::visitIndi::stateError;
        m_message = "no visit file path set";
        return log<software_error, -1>( { __FILE__, __LINE__, "no visit file path set" } );
    }

    wcc::visitFile vf;
    std::string err;

    if( vf.load( path, err ) < 0 )
    {
        { //mutex scope
            std::lock_guard<std::mutex> lock( m_visitMutex );
            m_state = wcc::visitIndi::stateError;
            m_message = err;
            m_parsed = false;
            m_selection = wcc::visitSelection();
        }

        return log<software_error, -1>( { __FILE__, __LINE__, "visit load failed: " + err } );
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        m_visit = vf;
        m_parsed = true;
        m_loadedPath = path;
    }

    if( applySelection() < 0 )
    {
        return -1;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        log<text_log>( "loaded visit " + m_selection.m_targetName + " from " + path + ": ra " +
                       std::to_string( m_selection.m_ra ) + " dec " + std::to_string( m_selection.m_dec ) +
                       " rollpa " + std::to_string( m_selection.m_rollPA ) + ", rank " +
                       std::to_string( m_selection.m_rank ) + ", guide star on " +
                       m_selection.m_guide.m_sensor + ", roll star on " + m_selection.m_roll.m_sensor );
    }

    return 0;
}

inline int visitCtrl::applySelection()
{
    std::lock_guard<std::mutex> lock( m_visitMutex );

    if( !m_parsed )
    {
        m_state = wcc::visitIndi::stateError;
        m_message = "no visit parsed";
        return -1;
    }

    wcc::visitSelection sel;
    std::string err;

    if( wcc::selectFromVisit( m_visit, m_selectRank, sel, err ) < 0 )
    {
        m_state = wcc::visitIndi::stateError;
        m_message = err;
        m_selection = wcc::visitSelection();

        return log<software_error, -1>( { __FILE__, __LINE__, err } );
    }

    m_selection = sel;
    m_state = wcc::visitIndi::stateLoaded;
    m_message = "rank " + std::to_string( sel.m_rank ) + " selected";

    return 0;
}

inline void visitCtrl::publishStatus()
{
    using namespace wcc::visitIndi;

    std::string st, msg, path;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        st = m_state;
        msg = m_message;
        path = m_loadedPath;
    }

    updateIfChanged( m_indiP_status, elState, st );
    updateIfChanged( m_indiP_status, elMessage, msg );
    updateIfChanged( m_indiP_status, elPath, path );
}

inline void visitCtrl::publishStar( pcf::IndiProperty &prop,
                                    pcf::IndiProperty &info,
                                    const wcc::visitStar &star )
{
    using namespace wcc::visitIndi;

    updateIfChanged( prop, elRank, static_cast<double>( star.m_rank ) );
    updateIfChanged( prop, elRA, star.m_ra );
    updateIfChanged( prop, elDec, star.m_dec );
    updateIfChanged( prop, elFieldX, star.m_fieldX );
    updateIfChanged( prop, elFieldY, star.m_fieldY );
    updateIfChanged( prop, elMag, star.m_mag );

    // Negative means "use the sensor centre", which only the consumer can resolve
    // because only it knows the sensor geometry. Pass the sentinel through.
    updateIfChanged( prop, elTargetX, star.m_targetX );
    updateIfChanged( prop, elTargetY, star.m_targetY );
    updateIfChanged( prop, elExpTime, star.m_expTimeFG );
    updateIfChanged( prop, elFrameRate, star.m_frameRateFG );
    updateIfChanged( prop, elROIW, static_cast<double>( star.m_roiWFG ) );
    updateIfChanged( prop, elROIH, static_cast<double>( star.m_roiHFG ) );

    updateIfChanged( info, elID, star.m_id );
    updateIfChanged( info, elSensor, star.m_sensor );
    updateIfChanged( info, elCatalog, star.m_catalogName );
}

inline void visitCtrl::publishSelection()
{
    using namespace wcc::visitIndi;

    wcc::visitSelection sel;

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        sel = m_selection;
    }

    if( !sel.m_valid )
    {
        return;
    }

    updateIfChanged( m_indiP_target, elRA, sel.m_ra );
    updateIfChanged( m_indiP_target, elDec, sel.m_dec );
    updateIfChanged( m_indiP_target, elRollPA, sel.m_rollPA );

    updateIfChanged( m_indiP_targetInfo, elName, sel.m_targetName );
    updateIfChanged( m_indiP_targetInfo, elProgID, sel.m_programID );
    updateIfChanged( m_indiP_targetInfo, elObsID, sel.m_obsID );
    updateIfChanged( m_indiP_targetInfo, elVisitID, sel.m_visitID );

    publishStar( m_indiP_guideStar, m_indiP_guideStarInfo, sel.m_guide );
    publishStar( m_indiP_rollStar, m_indiP_rollStarInfo, sel.m_roll );

    updateIfChanged( m_indiP_acqParams, elTAExpTime, sel.m_taExpTime );
    updateIfChanged( m_indiP_acqParams, elTAFrameRate, sel.m_taFrameRate );
    updateIfChanged( m_indiP_acqParams, elGuideTol, sel.m_guideTolPix );
    updateIfChanged( m_indiP_acqParams, elRollTol, sel.m_rollTolPix );
    updateIfChanged( m_indiP_acqParams, elMaxIter, static_cast<double>( sel.m_maxIterations ) );
    updateIfChanged( m_indiP_acqParams, elNSensors, static_cast<double>( sel.m_configSensors.size() ) );

    updateIfChanged( m_indiP_trackParams, elROIW, static_cast<double>( sel.m_tracking.m_roiW ) );
    updateIfChanged( m_indiP_trackParams, elROIH, static_cast<double>( sel.m_tracking.m_roiH ) );
    updateIfChanged( m_indiP_trackParams, elFrameRate, sel.m_tracking.m_frameRate );
    updateIfChanged( m_indiP_trackParams, elExpTime, sel.m_tracking.m_expTime );
    updateIfChanged( m_indiP_trackParams, elLoopGain, sel.m_tracking.m_loopGain );
    updateIfChanged( m_indiP_trackParams, elRollGain, sel.m_tracking.m_rollGain );

    updateIfChanged( m_indiP_trackDevices, elCentroidGuide, sel.m_tracking.m_centroidGuide );
    updateIfChanged( m_indiP_trackDevices, elCentroidRoll, sel.m_tracking.m_centroidRoll );

    updateIfChanged( m_indiP_configSensors, elList, sel.configSensorList() );
}

INDI_NEWCALLBACK_DEFN( visitCtrl, m_indiP_visitFile )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_visitFile, ipRecv );

    std::string target;

    if( indiTargetUpdate( m_indiP_visitFile, target, ipRecv, false ) < 0 )
    {
        return -1;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        m_visitPath = target;
    }

    updateIfChanged( m_indiP_visitFile, "current", target );

    log<text_log>( "visit_file -> " + target + " (toggle load to read it)" );

    return 0;
}

INDI_NEWCALLBACK_DEFN( visitCtrl, m_indiP_load )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_load, ipRecv );

    if( !ipRecv.find( "toggle" ) )
    {
        return 0;
    }

    if( ipRecv["toggle"].getSwitchState() != pcf::IndiElement::On )
    {
        // Toggling off clears the published visit, so a stale selection cannot be
        // mistaken for a current one.
        { //mutex scope
            std::lock_guard<std::mutex> lock( m_visitMutex );
            m_selection = wcc::visitSelection();
            m_parsed = false;
            m_state = wcc::visitIndi::stateEmpty;
            m_message = "unloaded";
            m_loadedPath.clear();
        }

        updateSwitchIfChanged( m_indiP_load, "toggle", pcf::IndiElement::Off, INDI_IDLE );
        log<text_log>( "visit unloaded" );

        return 0;
    }

    // Defer the read to appLogic; see m_loadRequest.
    m_loadRequest = true;

    updateSwitchIfChanged( m_indiP_load, "toggle", pcf::IndiElement::On, INDI_BUSY );

    return 0;
}

INDI_NEWCALLBACK_DEFN( visitCtrl, m_indiP_selectRank )
( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_selectRank, ipRecv );

    int target = m_selectRank;

    if( indiTargetUpdate( m_indiP_selectRank, target, ipRecv, false ) < 0 )
    {
        return -1;
    }

    if( target < 0 )
    {
        target = 0;
    }

    { //mutex scope
        std::lock_guard<std::mutex> lock( m_visitMutex );
        m_selectRank = target;
    }

    updateIfChanged( m_indiP_selectRank, "current", target );

    // Reselecting from the already-parsed file avoids re-reading it.
    m_republish = true;

    log<text_log>( "select_rank -> " + std::to_string( target ) );

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // visitCtrl_hpp
