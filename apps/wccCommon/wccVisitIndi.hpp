/** \file wccVisitIndi.hpp
 * \brief The INDI contract by which visitCtrl publishes a visit and other apps consume it.
 * \author Adam Schilperoort
 *
 * Visit file parsing lives in exactly one place, `visitCtrl`, which reads the JSON
 * and republishes everything the rest of the system needs as INDI properties.
 * `wccCtrl` and `telescopeSim` then read the visit over INDI rather than each
 * opening and interpreting the file themselves.
 *
 * The property and element names live here, as constants shared by the publisher
 * and every consumer, so a rename cannot desynchronize them: the compiler will
 * flag it in all four applications at once. wccSensorConfig.hpp exists for the
 * same reason on the sensor geometry side.
 *
 * \par Property summary
 * All are read-only on `visitCtrl` except `visit_file`, `load` and `select_rank`.
 *
 * | property           | type   | elements                                                        |
 * |--------------------|--------|-----------------------------------------------------------------|
 * | `visit_file`       | text   | `current`, `target` - the path to read                          |
 * | `load`             | switch | `toggle` - read the file and publish everything below           |
 * | `select_rank`      | number | `current`, `target` - which ranked star pair to publish         |
 * | `visit_status`     | text   | `state`, `message`, `path`                                      |
 * | `target`           | number | `ra`, `dec`, `rollpa`                                           |
 * | `target_info`      | text   | `name`, `progid`, `obsid`, `visitid`                            |
 * | `guide_star`       | number | the star elements below                                  |
 * | `guide_star_info`  | text   | `id`, `sensor`, `catalog`                                       |
 * | `roll_star`        | number | the star elements below                                         |
 * | `roll_star_info`   | text   | `id`, `sensor`, `catalog`                                       |
 * | `acq_params`       | number | `ta_exptime`, `ta_frame_rate`, `guide_tol_px`, `roll_tol_px`, `max_iterations`, `n_sensors` |
 * | `track_params`     | number | `roi_w`, `roi_h`, `frame_rate`, `exptime`, `loop_gain`, `roll_gain` |
 * | `track_devices`    | text   | `centroid_guide`, `centroid_roll`                               |
 * | `config_sensors`   | text   | `list` - comma separated sensor names                           |
 *
 * \ingroup wccCommon_files
 */

#ifndef wccVisitIndi_hpp
#define wccVisitIndi_hpp

#include <algorithm>
#include <string>
#include <vector>

#include "wccStarCatalog.hpp" // for trimField()
#include "wccVisit.hpp"

namespace MagAOX
{
namespace wcc
{

/// Names of the INDI properties and elements carrying a visit.
/** \ingroup wccCommon
 */
namespace visitIndi
{

/// Text property holding the path of the visit file to read.
constexpr const char *propVisitFile = "visit_file";

/// Toggle that reads the visit file and publishes everything else.
constexpr const char *propLoad = "load";

/// Number property selecting which ranked guide and roll star pair to publish.
constexpr const char *propSelectRank = "select_rank";

/// Text property reporting the loader state.
constexpr const char *propStatus = "visit_status";

/// Number property carrying the requested boresight.
constexpr const char *propTarget = "target";

/// Text property carrying the target identification.
constexpr const char *propTargetInfo = "target_info";

/// Number property carrying the selected guide star.
constexpr const char *propGuideStar = "guide_star";

/// Text property carrying the selected guide star's identification.
constexpr const char *propGuideStarInfo = "guide_star_info";

/// Number property carrying the selected roll star.
constexpr const char *propRollStar = "roll_star";

/// Text property carrying the selected roll star's identification.
constexpr const char *propRollStarInfo = "roll_star_info";

/// Number property carrying the acquisition parameters.
constexpr const char *propAcqParams = "acq_params";

/// Number property carrying the tracking parameters.
constexpr const char *propTrackParams = "track_params";

/// Text property carrying the centroid controller device names.
constexpr const char *propTrackDevices = "track_devices";

/// Text property carrying the sensors to solve astrometry on.
constexpr const char *propConfigSensors = "config_sensors";

/** \name Element names
 *@{
 */
constexpr const char *elState = "state";     ///< Loader state on propStatus.
constexpr const char *elMessage = "message"; ///< Loader message on propStatus.
constexpr const char *elPath = "path";       ///< File actually read, on propStatus.

constexpr const char *elRA = "ra";         ///< Right ascension [deg].
constexpr const char *elDec = "dec";       ///< Declination [deg].
constexpr const char *elRollPA = "rollpa"; ///< Requested position angle [deg].

constexpr const char *elName = "name";       ///< Target name on propTargetInfo.
constexpr const char *elProgID = "progid";   ///< Program id on propTargetInfo.
constexpr const char *elObsID = "obsid";     ///< Observation id on propTargetInfo.
constexpr const char *elVisitID = "visitid"; ///< Visit id on propTargetInfo.

constexpr const char *elRank = "rank";           ///< Selection rank of a published star.
constexpr const char *elFieldX = "field_x";      ///< X_WCC field angle [arcsec].
constexpr const char *elFieldY = "field_y";      ///< Y_WCC field angle [arcsec].
constexpr const char *elMag = "mag";             ///< Star magnitude.
constexpr const char *elTargetX = "target_x";    ///< Target column, full sensor pixels.
constexpr const char *elTargetY = "target_y";    ///< Target row, full sensor pixels.
constexpr const char *elExpTime = "exptime";     ///< Fine guiding exposure time [s].
constexpr const char *elFrameRate = "frame_rate"; ///< Fine guiding frame rate [Hz].
constexpr const char *elROIW = "roi_w";          ///< Fine guiding ROI width [pixels].
constexpr const char *elROIH = "roi_h";          ///< Fine guiding ROI height [pixels].

constexpr const char *elID = "id";           ///< Catalog identifier of a star.
constexpr const char *elSensor = "sensor";    ///< Sensor a star lands on.
constexpr const char *elCatalog = "catalog";  ///< Catalog a star came from.

constexpr const char *elTAExpTime = "ta_exptime";       ///< Acquisition exposure time [s].
constexpr const char *elTAFrameRate = "ta_frame_rate";  ///< Acquisition frame rate [Hz].
constexpr const char *elGuideTol = "guide_tol_px";      ///< Guide acceptance radius [pixels].
constexpr const char *elRollTol = "roll_tol_px";        ///< Roll acceptance radius [pixels].
constexpr const char *elMaxIter = "max_iterations";     ///< Offset and verify cycle limit.
constexpr const char *elNSensors = "n_sensors";         ///< Sensors in the solution.

constexpr const char *elLoopGain = "loop_gain"; ///< Fast pointing loop gain.
constexpr const char *elRollGain = "roll_gain"; ///< Fast roll loop gain.

constexpr const char *elCentroidGuide = "centroid_guide"; ///< Guide centroid controller device.
constexpr const char *elCentroidRoll = "centroid_roll";   ///< Roll centroid controller device.

constexpr const char *elList = "list"; ///< Comma separated list on propConfigSensors.
///@}

/// Loader states reported on propStatus.
/** \ingroup wccCommon
 */
constexpr const char *stateEmpty = "EMPTY";   ///< No visit loaded.
constexpr const char *stateLoaded = "LOADED"; ///< A visit is loaded and published.
constexpr const char *stateError = "ERROR";   ///< The last load attempt failed.

} // namespace visitIndi

/// One resolved visit: the target plus the selected guide and roll star pair.
/** This is the payload the INDI contract carries. `visitCtrl` fills it from a
 * `visitFile` and publishes it; `wccCtrl` reassembles it from the subscribed
 * properties. Holding it in one struct on both sides means the sequencer does not
 * care which way the data arrived.
 *
 * \ingroup wccCommon
 */
struct visitSelection
{
    /** \name Target
     *@{
     */
    std::string m_targetName; ///< TARGET.

    std::string m_programID; ///< PROGMID.

    std::string m_obsID; ///< OBSID.

    std::string m_visitID; ///< VISITID.

    double m_ra{ 0 }; ///< RA_PROP, requested boresight right ascension [deg].

    double m_dec{ 0 }; ///< DEC_PROP, requested boresight declination [deg].

    double m_rollPA{ 0 }; ///< ROLLPA, requested position angle [deg].
    ///@}

    /** \name Selected Stars
     *@{
     */
    visitStar m_guide; ///< The selected guide star.

    visitStar m_roll; ///< The selected roll star.

    int m_rank{ 0 }; ///< Rank the pair was selected at.
    ///@}

    /** \name Acquisition
     *@{
     */
    double m_taExpTime{ 1.0 }; ///< Default acquisition exposure time [s].

    double m_taFrameRate{ -1 }; ///< Default acquisition frame rate [Hz], negative to leave alone.

    double m_guideTolPix{ 50.0 }; ///< Guide star acceptance radius [pixels].

    double m_rollTolPix{ 50.0 }; ///< Roll star acceptance radius [pixels].

    int m_maxIterations{ 3 }; ///< Offset and verify cycle limit.

    std::vector<std::string> m_configSensors; ///< Sensors to solve astrometry on.
    ///@}

    /** \name Tracking
     *@{
     */
    visitTracking m_tracking; ///< The fast tracking handoff configuration.
    ///@}

    bool m_valid{ false }; ///< True once fully populated.

    /// Serialize the sensor list the way propConfigSensors carries it.
    /** \returns the comma separated list
     */
    std::string configSensorList() const;

    /// Populate m_configSensors from the comma separated form.
    void setConfigSensorList( const std::string &list /**< [in] comma separated names */ );
};

inline std::string visitSelection::configSensorList() const
{
    std::string out;

    for( size_t i = 0; i < m_configSensors.size(); ++i )
    {
        if( i > 0 )
        {
            out += ",";
        }

        out += m_configSensors[i];
    }

    return out;
}

inline void visitSelection::setConfigSensorList( const std::string &list )
{
    m_configSensors.clear();

    std::string cur;

    for( char c : list )
    {
        if( c == ',' )
        {
            const std::string t = trimField( cur );

            if( !t.empty() )
            {
                m_configSensors.push_back( t );
            }

            cur.clear();
            continue;
        }

        cur.push_back( c );
    }

    const std::string t = trimField( cur );

    if( !t.empty() )
    {
        m_configSensors.push_back( t );
    }
}

/// Resolve a visit file into a single selection at a given rank.
/** Rank 0 means take the highest ranked guide star present. A roll star of the
 * same rank is preferred, falling back to the first roll star available, because
 * the visit file does not always carry a matching rank for both.
 *
 * \returns 0 on success
 * \returns -1 if the visit has no guide star, or none at the requested rank
 *
 * \ingroup wccCommon
 */
inline int selectFromVisit( const visitFile &vf /**< [in] the parsed visit */,
                            int rank /**< [in] requested rank, 0 for the best available */,
                            visitSelection &sel /**< [out] the resolved selection */,
                            std::string &err /**< [out] description of any failure */ )
{
    sel = visitSelection();

    if( vf.guideStars().empty() )
    {
        err = "visit has no guide stars";
        return -1;
    }

    const visitStar *gs = nullptr;

    for( const visitStar &cand : vf.guideStars() )
    {
        if( rank == 0 || cand.m_rank == rank )
        {
            gs = &cand;
            break;
        }
    }

    if( gs == nullptr )
    {
        err = "visit has no guide star at rank " + std::to_string( rank );
        return -1;
    }

    // Prefer a roll star of the same rank, then any roll star at all.
    const visitStar *rs = vf.rollStar( gs->m_rank );

    if( rs == nullptr )
    {
        rs = vf.rollStar( 0 );
    }

    sel.m_targetName = vf.target();
    sel.m_programID = vf.programID();
    sel.m_obsID = vf.obsID();
    sel.m_visitID = vf.visitID();
    sel.m_ra = vf.ra();
    sel.m_dec = vf.dec();
    sel.m_rollPA = vf.rollPA();

    sel.m_guide = *gs;
    sel.m_rank = gs->m_rank;

    if( rs != nullptr )
    {
        sel.m_roll = *rs;
    }

    sel.m_taExpTime = vf.taExpTime();
    sel.m_taFrameRate = vf.taFrameRate();
    sel.m_guideTolPix = vf.guideTolPix();
    sel.m_rollTolPix = vf.rollTolPix();
    sel.m_maxIterations = vf.maxIterations();
    sel.m_configSensors = vf.configSensors();

    sel.m_tracking = vf.tracking();

    sel.m_valid = true;

    return 0;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccVisitIndi_hpp
