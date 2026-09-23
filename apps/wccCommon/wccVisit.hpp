/** \file wccVisit.hpp
 * \brief The WCC visit file: observation plan, sensor configuration and guide star selection.
 * \author Adam Schilperoort
 *
 * Models the JSON visit file, using `data/LAZ_072226_001_R001.json` as the
 * baseline schema and adding the per camera parameters an acquisition sequence
 * needs but that file does not yet carry:
 *
 * - `TARGET_ACQUISITION.TA_SENSORS`, a per sensor block of exposure time, frame
 *   rate, gain and region of interest used for the full frame acquisition pass
 * - `TARGET_X_PIX` / `TARGET_Y_PIX` on each guide and roll star, the pixel the
 *   star is to be placed on
 * - `EXP_TIME_FG`, `FRAME_RATE_FG`, `ROI_W_FG`, `ROI_H_FG` for the small ROI
 *   fine guiding configuration
 * - `TRACKING`, the handoff to the centroid controllers and the fast offset loop
 * - acquisition tolerances and iteration limits
 *
 * Every added field has a default, so an original LAZ format visit file still
 * loads and runs with the application defaults.
 *
 * \par Field angle convention
 * `X_WCC` and `Y_WCC` are boresight relative field angles in arcseconds, with
 * `X_WCC` running opposite to increasing right ascension. See wccFocalPlane.hpp.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccVisit_hpp
#define wccVisit_hpp

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "wccFocalPlane.hpp"
#include "wccJSON.hpp"
#include "wccNumeric.hpp"

namespace MagAOX
{
namespace wcc
{

/// A region of interest as written in a visit file.
/** \ingroup wccCommon
 */
struct visitROI
{
    bool m_valid{ false }; ///< True once a ROI block was present and parsed.

    double m_x{ 0 }; ///< ROI center column in full sensor pixels.

    double m_y{ 0 }; ///< ROI center row in full sensor pixels.

    int m_w{ 0 }; ///< ROI width in unbinned pixels.

    int m_h{ 0 }; ///< ROI height in unbinned pixels.

    int m_binX{ 1 }; ///< Binning along columns.

    int m_binY{ 1 }; ///< Binning along rows.

    /// Populate from a JSON object, leaving m_valid false if it is absent.
    void load( const jsonValue &j /**< [in] the ROI object */ );

    /// Convert to the geometry model's ROI type.
    roiSpec toROISpec() const;
};

inline void visitROI::load( const jsonValue &j )
{
    if( !j.isObject() )
    {
        return;
    }

    m_x = j.num( "X", m_x );
    m_y = j.num( "Y", m_y );
    m_w = j.integer( "W", m_w );
    m_h = j.integer( "H", m_h );
    m_binX = std::max( 1, j.integer( "BIN_X", m_binX ) );
    m_binY = std::max( 1, j.integer( "BIN_Y", m_binY ) );

    m_valid = ( m_w > 0 && m_h > 0 );
}

inline roiSpec visitROI::toROISpec() const
{
    roiSpec r;
    r.m_centerX = m_x;
    r.m_centerY = m_y;
    r.m_w = m_w;
    r.m_h = m_h;
    r.m_binX = m_binX;
    r.m_binY = m_binY;

    return r;
}

/// Per sensor acquisition configuration from `TARGET_ACQUISITION.TA_SENSORS`.
/** \ingroup wccCommon
 */
struct visitSensorConfig
{
    std::string m_name; ///< Logical sensor name.

    double m_expTime{ -1 }; ///< Exposure time [s]. Negative means fall back to the acquisition default.

    double m_frameRate{ -1 }; ///< Frame rate [Hz]. Negative means fall back to the acquisition default.

    double m_gain{ -1 }; ///< Camera gain. Negative means leave the camera at its current gain.

    visitROI m_roi; ///< Region of interest. Invalid means use the sensor full frame.

    bool m_stream{ true }; ///< Whether this sensor participates in the astrometric solution.
};

/// One guide or roll star candidate from the visit file.
/** \ingroup wccCommon
 */
struct visitStar
{
    int m_rank{ 0 }; ///< Selection rank, 1 is preferred.

    int m_num{ 0 }; ///< GSNUM for a guide star or RSNUM for a roll star.

    std::string m_id; ///< Catalog identifier.

    std::string m_sensor; ///< Logical sensor the star is expected to land on.

    std::string m_catalogName; ///< Guide star catalog the entry came from.

    double m_ra{ 0 }; ///< Right ascension, ICRS [deg].

    double m_dec{ 0 }; ///< Declination, ICRS [deg].

    double m_fieldX{ 0 }; ///< X_WCC, field angle along focal plane X [arcsec].

    double m_fieldY{ 0 }; ///< Y_WCC, field angle along focal plane Y [arcsec].

    double m_mag{ 99 }; ///< Magnitude, if the visit file supplies one.

    /// Target column on the sensor, in full sensor pixels. Negative means the sensor center.
    double m_targetX{ -1 };

    /// Target row on the sensor, in full sensor pixels. Negative means the sensor center.
    double m_targetY{ -1 };

    double m_expTimeFG{ -1 }; ///< Fine guiding exposure time [s]. Negative means use the tracking default.

    double m_frameRateFG{ -1 }; ///< Fine guiding frame rate [Hz]. Negative means use the tracking default.

    int m_roiWFG{ -1 }; ///< Fine guiding ROI width [pixels]. Negative means use the tracking default.

    int m_roiHFG{ -1 }; ///< Fine guiding ROI height [pixels]. Negative means use the tracking default.

    /// Populate from a JSON object.
    void load( const jsonValue &j /**< [in] one GUIDE_STAR or ROLL_STAR element */ );

    /// Target column, resolving the negative sentinel against a sensor.
    double targetX( const sensorConfig &s /**< [in] the sensor this star lands on */ ) const;

    /// Target row, resolving the negative sentinel against a sensor.
    double targetY( const sensorConfig &s /**< [in] the sensor this star lands on */ ) const;
};

inline void visitStar::load( const jsonValue &j )
{
    m_rank = j.integer( "RANK", m_rank );
    m_num = j.integer( "GSNUM", j.integer( "RSNUM", m_num ) );
    m_id = j.str( "ID", m_id );
    m_sensor = j.str( "SENSOR", m_sensor );
    m_catalogName = j.str( "CATALOG_NAME", m_catalogName );
    m_ra = j.num( "RA", m_ra );
    m_dec = j.num( "DEC", m_dec );
    m_fieldX = j.num( "X_WCC", m_fieldX );
    m_fieldY = j.num( "Y_WCC", m_fieldY );
    m_mag = j.num( "MAG", m_mag );
    m_targetX = j.num( "TARGET_X_PIX", m_targetX );
    m_targetY = j.num( "TARGET_Y_PIX", m_targetY );
    m_expTimeFG = j.num( "EXP_TIME_FG", m_expTimeFG );
    m_frameRateFG = j.num( "FRAME_RATE_FG", m_frameRateFG );
    m_roiWFG = j.integer( "ROI_W_FG", m_roiWFG );
    m_roiHFG = j.integer( "ROI_H_FG", m_roiHFG );
}

inline double visitStar::targetX( const sensorConfig &s ) const
{
    return ( m_targetX >= 0 ) ? m_targetX : s.centerX();
}

inline double visitStar::targetY( const sensorConfig &s ) const
{
    return ( m_targetY >= 0 ) ? m_targetY : s.centerY();
}

/// The fast tracking handoff configuration from the `TRACKING` block.
/** \ingroup wccCommon
 */
struct visitTracking
{
    int m_roiW{ 128 }; ///< Fine guiding ROI width [pixels].

    int m_roiH{ 128 }; ///< Fine guiding ROI height [pixels].

    double m_frameRate{ 100 }; ///< Fine guiding frame rate [Hz].

    double m_expTime{ 0.01 }; ///< Fine guiding exposure time [s].

    std::string m_centroidGuide; ///< INDI device of the centroid controller on the guide sensor.

    std::string m_centroidRoll; ///< INDI device of the centroid controller on the roll sensor.

    double m_loopGain{ 0.3 }; ///< Proportional gain of the fast pointing offset loop.

    double m_rollGain{ 0.3 }; ///< Proportional gain of the fast roll offset loop.

    /// Populate from a JSON object, leaving defaults for absent members.
    void load( const jsonValue &j /**< [in] the TRACKING object */ );
};

inline void visitTracking::load( const jsonValue &j )
{
    if( !j.isObject() )
    {
        return;
    }

    m_roiW = std::max( 4, j.integer( "ROI_W", m_roiW ) );
    m_roiH = std::max( 4, j.integer( "ROI_H", m_roiH ) );
    m_frameRate = j.num( "FRAME_RATE", m_frameRate );
    m_expTime = j.num( "EXPTIME", m_expTime );
    m_centroidGuide = j.str( "CENTROID_DEVICE_GUIDE", m_centroidGuide );
    m_centroidRoll = j.str( "CENTROID_DEVICE_ROLL", m_centroidRoll );
    m_loopGain = j.num( "LOOP_GAIN", m_loopGain );
    m_rollGain = j.num( "ROLL_GAIN", m_rollGain );
}

/// A parsed WCC visit file.
/** \ingroup wccCommon
 */
class visitFile
{

    /** \name Observation Header - Data
     *@{
     */
  protected:
    std::string m_path; ///< File this was loaded from.

    std::string m_programID; ///< PROGMID.

    std::string m_telescope; ///< TELESCOP.

    std::string m_instrument; ///< INSTRUME.

    std::string m_obsID; ///< OBSID.

    std::string m_visitID; ///< VISITID.

    std::string m_target; ///< TARGET, the object name.

    double m_ra{ 0 }; ///< RA_PROP, the proposed boresight right ascension [deg].

    double m_dec{ 0 }; ///< DEC_PROP, the proposed boresight declination [deg].

    double m_rollPA{ 0 }; ///< ROLLPA, the requested position angle [deg].
    ///@}

    /** \name Target Acquisition - Data
     *@{
     */
  protected:
    /// CONFIG_SENSORS: the sensors to configure and solve astrometry on.
    std::vector<std::string> m_configSensors;

    double m_taExpTime{ 1.0 }; ///< Default acquisition exposure time [s].

    double m_taFrameRate{ -1 }; ///< Default acquisition frame rate [Hz]. Negative leaves the camera alone.

    std::string m_taGain; ///< TA_GAIN, a symbolic gain setting such as HIGH or LOW.

    visitROI m_taROI; ///< Default acquisition ROI. Invalid means full frame.

    /// Per sensor overrides of the acquisition configuration.
    std::vector<visitSensorConfig> m_taSensors;

    double m_guideTolPix{ 50.0 }; ///< Acceptance radius for the guide star [pixels].

    double m_rollTolPix{ 50.0 }; ///< Acceptance radius for the roll star [pixels].

    int m_maxIterations{ 3 }; ///< Maximum offset and verify cycles before declaring failure.
    ///@}

    /** \name Guide and Roll Stars - Data
     *@{
     */
  protected:
    std::vector<visitStar> m_guideStars; ///< GUIDE_STAR entries, sorted by rank.

    std::vector<visitStar> m_rollStars; ///< ROLL_STAR entries, sorted by rank.

    visitTracking m_tracking; ///< The fast tracking handoff configuration.
    ///@}

  public:
    /// Read and parse a visit file.
    /** \returns 0 on success
     * \returns -1 if the file cannot be read, does not parse, or is missing a
     *          required field, with err describing the problem
     */
    int load( const std::string &path /**< [in] path to the visit file */,
              std::string &err /**< [out] description of any failure */ );

    /// Populate from an already parsed JSON document.
    /** \returns 0 on success
     * \returns -1 if a required field is missing
     */
    int loadJSON( const jsonValue &root /**< [in] the visit document root */,
                  std::string &err /**< [out] description of any failure */ );

    /** \name Observation Header
     *@{
     */
    /// File this was loaded from.
    const std::string &path() const;

    /// PROGMID, the program identifier.
    const std::string &programID() const;

    /// TELESCOP.
    const std::string &telescope() const;

    /// INSTRUME.
    const std::string &instrument() const;

    /// OBSID.
    const std::string &obsID() const;

    /// VISITID.
    const std::string &visitID() const;

    /// TARGET, the object name.
    const std::string &target() const;

    /// RA_PROP, the proposed boresight right ascension [deg].
    double ra() const;

    /// DEC_PROP, the proposed boresight declination [deg].
    double dec() const;

    /// ROLLPA, the requested position angle [deg].
    double rollPA() const;
    ///@}

    /** \name Target Acquisition
     *@{
     */
    /// The sensors to configure and solve astrometry on.
    const std::vector<std::string> &configSensors() const;

    /// Default acquisition exposure time [s].
    double taExpTime() const;

    /// Default acquisition frame rate [Hz], negative to leave the camera alone.
    double taFrameRate() const;

    /// TA_GAIN, a symbolic gain setting.
    const std::string &taGain() const;

    /// Default acquisition ROI.
    const visitROI &taROI() const;

    /// Acceptance radius for the guide star [pixels].
    double guideTolPix() const;

    /// Acceptance radius for the roll star [pixels].
    double rollTolPix() const;

    /// Maximum offset and verify cycles before declaring failure.
    int maxIterations() const;

    /// Resolve the acquisition configuration for one sensor, applying defaults.
    /** \returns the effective configuration
     */
    visitSensorConfig sensorConfigFor( const std::string &name /**< [in] logical sensor name */ ) const;
    ///@}

    /** \name Guide and Roll Stars
     *@{
     */
    /// GUIDE_STAR entries, sorted by rank.
    const std::vector<visitStar> &guideStars() const;

    /// ROLL_STAR entries, sorted by rank.
    const std::vector<visitStar> &rollStars() const;

    /// The highest ranked guide star.
    /** \returns a pointer to the star
     * \returns nullptr if the visit file has no guide stars
     */
    const visitStar *guideStar() const;

    /// The highest ranked roll star, optionally restricted to a rank.
    /** The visit file lists several roll stars per rank, so the rank of the
     * selected guide star is the natural filter.
     *
     * \returns a pointer to the star
     * \returns nullptr if no roll star matches
     */
    const visitStar *rollStar( int rank = 0 /**< [in] required rank, 0 accepts any */ ) const;

    /// The fast tracking handoff configuration.
    const visitTracking &tracking() const;
    ///@}
};

inline int visitFile::load( const std::string &path, std::string &err )
{
    jsonValue root;

    if( jsonParseFile( path, root, err ) < 0 )
    {
        return -1;
    }

    m_path = path;

    return loadJSON( root, err );
}

inline int visitFile::loadJSON( const jsonValue &root, std::string &err )
{
    if( !root.isObject() )
    {
        err = "visit file root is not a JSON object";
        return -1;
    }

    m_programID = root.str( "PROGMID" );
    m_telescope = root.str( "TELESCOP" );
    m_instrument = root.str( "INSTRUME" );
    m_obsID = root.str( "OBSID" );
    m_visitID = root.str( "VISITID" );
    m_target = root.str( "TARGET" );

    if( !root.has( "RA_PROP" ) || !root.has( "DEC_PROP" ) )
    {
        err = "visit file is missing RA_PROP or DEC_PROP";
        return -1;
    }

    m_ra = root.num( "RA_PROP" );
    m_dec = root.num( "DEC_PROP" );
    m_rollPA = root.num( "ROLLPA", 0.0 );

    if( !isFinite( m_ra ) || !isFinite( m_dec ) )
    {
        err = "visit file RA_PROP or DEC_PROP is not a finite number";
        return -1;
    }

    // ---------------------------------------------------------- acquisition
    const jsonValue &ta = root["TARGET_ACQUISITION"];

    m_configSensors.clear();
    ta["CONFIG_SENSORS"].asStringArray( m_configSensors );

    // TA_EXPTIME_SEC_SLOANR is written as a one element array in the LAZ file.
    {
        const jsonValue &et = ta["TA_EXPTIME_SEC_SLOANR"];
        if( et.isArray() && et.size() > 0 )
        {
            m_taExpTime = et[0].asDouble( m_taExpTime );
        }
        else if( !et.isNull() )
        {
            m_taExpTime = et.asDouble( m_taExpTime );
        }
        m_taExpTime = ta.num( "TA_EXPTIME", m_taExpTime );
    }

    m_taFrameRate = ta.num( "TA_FRAME_RATE_HZ", m_taFrameRate );
    m_taGain = ta.str( "TA_GAIN", m_taGain );
    m_taROI.load( ta["TA_ROI"] );

    m_guideTolPix = ta.num( "TA_GUIDE_TOL_PIX", m_guideTolPix );
    m_rollTolPix = ta.num( "TA_ROLL_TOL_PIX", m_rollTolPix );
    m_maxIterations = std::max( 1, ta.integer( "TA_MAX_ITERATIONS", m_maxIterations ) );

    m_taSensors.clear();
    {
        const jsonValue &ts = ta["TA_SENSORS"];
        for( size_t i = 0; i < ts.size(); ++i )
        {
            const jsonValue &sv = ts.value( i );

            visitSensorConfig sc;
            sc.m_name = ts.key( i );
            sc.m_expTime = sv.num( "EXPTIME", -1 );
            sc.m_frameRate = sv.num( "FRAME_RATE", -1 );
            sc.m_gain = sv.num( "GAIN", -1 );
            sc.m_stream = sv.boolean( "STREAM", true );
            sc.m_roi.load( sv["ROI"] );

            m_taSensors.push_back( sc );
        }
    }

    // -------------------------------------------------------------- stars
    auto loadStars = []( const jsonValue &arr, std::vector<visitStar> &out ) {
        out.clear();

        for( size_t i = 0; i < arr.size(); ++i )
        {
            visitStar s;
            s.load( arr[i] );
            out.push_back( s );
        }

        std::stable_sort( out.begin(), out.end(), []( const visitStar &a, const visitStar &b ) {
            if( a.m_rank != b.m_rank )
            {
                return a.m_rank < b.m_rank;
            }
            return a.m_num < b.m_num;
        } );
    };

    loadStars( root["GUIDE_STAR"], m_guideStars );
    loadStars( root["ROLL_STAR"], m_rollStars );

    m_tracking = visitTracking();
    m_tracking.load( root["TRACKING"] );

    if( m_guideStars.empty() )
    {
        err = "visit file has no GUIDE_STAR entries";
        return -1;
    }

    return 0;
}

inline const std::string &visitFile::path() const
{
    return m_path;
}

inline const std::string &visitFile::programID() const
{
    return m_programID;
}

inline const std::string &visitFile::telescope() const
{
    return m_telescope;
}

inline const std::string &visitFile::instrument() const
{
    return m_instrument;
}

inline const std::string &visitFile::obsID() const
{
    return m_obsID;
}

inline const std::string &visitFile::visitID() const
{
    return m_visitID;
}

inline const std::string &visitFile::target() const
{
    return m_target;
}

inline double visitFile::ra() const
{
    return m_ra;
}

inline double visitFile::dec() const
{
    return m_dec;
}

inline double visitFile::rollPA() const
{
    return m_rollPA;
}

inline const std::vector<std::string> &visitFile::configSensors() const
{
    return m_configSensors;
}

inline double visitFile::taExpTime() const
{
    return m_taExpTime;
}

inline double visitFile::taFrameRate() const
{
    return m_taFrameRate;
}

inline const std::string &visitFile::taGain() const
{
    return m_taGain;
}

inline const visitROI &visitFile::taROI() const
{
    return m_taROI;
}

inline double visitFile::guideTolPix() const
{
    return m_guideTolPix;
}

inline double visitFile::rollTolPix() const
{
    return m_rollTolPix;
}

inline int visitFile::maxIterations() const
{
    return m_maxIterations;
}

inline visitSensorConfig visitFile::sensorConfigFor( const std::string &name ) const
{
    visitSensorConfig out;
    out.m_name = name;
    out.m_expTime = m_taExpTime;
    out.m_frameRate = m_taFrameRate;
    out.m_roi = m_taROI;

    const std::string want = normalizeSensorName( name );

    for( const visitSensorConfig &sc : m_taSensors )
    {
        if( normalizeSensorName( sc.m_name ) != want )
        {
            continue;
        }

        if( sc.m_expTime >= 0 )
        {
            out.m_expTime = sc.m_expTime;
        }
        if( sc.m_frameRate >= 0 )
        {
            out.m_frameRate = sc.m_frameRate;
        }
        if( sc.m_gain >= 0 )
        {
            out.m_gain = sc.m_gain;
        }
        if( sc.m_roi.m_valid )
        {
            out.m_roi = sc.m_roi;
        }

        out.m_stream = sc.m_stream;
        break;
    }

    return out;
}

inline const std::vector<visitStar> &visitFile::guideStars() const
{
    return m_guideStars;
}

inline const std::vector<visitStar> &visitFile::rollStars() const
{
    return m_rollStars;
}

inline const visitStar *visitFile::guideStar() const
{
    if( m_guideStars.empty() )
    {
        return nullptr;
    }

    return &m_guideStars.front();
}

inline const visitStar *visitFile::rollStar( int rank ) const
{
    for( const visitStar &s : m_rollStars )
    {
        if( rank == 0 || s.m_rank == rank )
        {
            return &s;
        }
    }

    return nullptr;
}

inline const visitTracking &visitFile::tracking() const
{
    return m_tracking;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccVisit_hpp
