/** \file wccFocalPlane.hpp
 * \brief Geometric model of the WCC sensor array on the telescope focal plane.
 * \author Adam Schilperoort
 *
 * The Python simulator models a single monolithic detector centered on the
 * telescope boresight. The WCC is instead an array of sensors at known field
 * positions, each with its own pixel pitch, mounting rotation and filter, and
 * each reading out an independently commanded region of interest. This header
 * carries that layout and turns it into a per sensor, per ROI wccSkyWCS.
 *
 * \par Field angle convention
 * Sensor positions are given as boresight relative field angles in arcseconds,
 * matching the `X_WCC` / `Y_WCC` columns of the visit file. Comparing the visit
 * file entries against their own RA/Dec confirms that `X_WCC` runs opposite to
 * increasing right ascension and `Y_WCC` along increasing declination, which is
 * the parity of -1 default below.
 *
 * \par Transform chain
 * For sensor \f$s\f$ with field position \f$(X_s, Y_s)\f$, mounting rotation
 * \f$\phi_s\f$, and plate scale \f$k_s\f$ arcsec per output pixel, an output
 * pixel \f$p\f$ maps to the tangent plane by
 * \f[ (\xi, \eta) = R(\mathrm{PA})\, P\, \left[ (X_s, Y_s) + R(\phi_s)\, k_s (p - p_c) \right] \f]
 * where \f$P = \mathrm{diag}(\mathrm{parity}, 1)\f$ and \f$p_c\f$ is the sensor
 * optical center. That is exactly a FITS CD matrix plus a reference pixel, so
 * the whole chain collapses into a wccSkyWCS with
 * \f[ \mathrm{CD} = k_s\, R(\mathrm{PA})\, P\, R(\phi_s), \qquad
 *     p_\mathrm{ref} = p_c - R(\phi_s)^{-1} (X_s, Y_s) / k_s. \f]
 *
 * \ingroup wccCommon_files
 */

#ifndef wccFocalPlane_hpp
#define wccFocalPlane_hpp

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "wccSkyWCS.hpp"
#include "wccUnits.hpp"

namespace MagAOX
{
namespace wcc
{

/// A commanded region of interest, in the MagAO-X dev::stdCamera convention.
/** stdCamera reports the ROI position as the *center* in full sensor pixels
 * (`roi_region_x` / `roi_region_y`) and the size in unbinned pixels
 * (`roi_region_w` / `roi_region_h`). The published image is
 * `w/bin_x` by `h/bin_y` pixels.
 *
 * \ingroup wccCommon
 */
struct roiSpec
{
    double m_centerX{ 0 }; ///< ROI center column in full sensor pixels, 0-based.

    double m_centerY{ 0 }; ///< ROI center row in full sensor pixels, 0-based.

    int m_w{ 0 }; ///< ROI width in unbinned sensor pixels.

    int m_h{ 0 }; ///< ROI height in unbinned sensor pixels.

    int m_binX{ 1 }; ///< Binning along columns.

    int m_binY{ 1 }; ///< Binning along rows.

    /// Width of the published image [pixels].
    int imageW() const;

    /// Height of the published image [pixels].
    int imageH() const;

    /// Full sensor column of the center of published column 0.
    double originX() const;

    /// Full sensor row of the center of published row 0.
    double originY() const;

    /// Convert a published column to a full sensor column.
    double toFullX( double x /**< [in] published column, 0-based */ ) const;

    /// Convert a published row to a full sensor row.
    double toFullY( double y /**< [in] published row, 0-based */ ) const;

    /// Convert a full sensor column to a published column.
    double toImageX( double x /**< [in] full sensor column, 0-based */ ) const;

    /// Convert a full sensor row to a published row.
    double toImageY( double y /**< [in] full sensor row, 0-based */ ) const;

    /// True if a published pixel lies inside the image.
    bool contains( double x /**< [in] published column */, double y /**< [in] published row */ ) const;
};

inline int roiSpec::imageW() const
{
    const int b = ( m_binX > 0 ) ? m_binX : 1;
    return ( m_w > 0 ) ? m_w / b : 0;
}

inline int roiSpec::imageH() const
{
    const int b = ( m_binY > 0 ) ? m_binY : 1;
    return ( m_h > 0 ) ? m_h / b : 0;
}

inline double roiSpec::originX() const
{
    const int b = ( m_binX > 0 ) ? m_binX : 1;

    // Unbinned pixels span centerX +- (w-1)/2. The center of the first binned
    // pixel sits (b-1)/2 further in, so the two halves combine to (w-b)/2.
    return m_centerX - 0.5 * ( m_w - b );
}

inline double roiSpec::originY() const
{
    const int b = ( m_binY > 0 ) ? m_binY : 1;
    return m_centerY - 0.5 * ( m_h - b );
}

inline double roiSpec::toFullX( double x ) const
{
    const int b = ( m_binX > 0 ) ? m_binX : 1;
    return originX() + x * b;
}

inline double roiSpec::toFullY( double y ) const
{
    const int b = ( m_binY > 0 ) ? m_binY : 1;
    return originY() + y * b;
}

inline double roiSpec::toImageX( double x ) const
{
    const int b = ( m_binX > 0 ) ? m_binX : 1;
    return ( x - originX() ) / b;
}

inline double roiSpec::toImageY( double y ) const
{
    const int b = ( m_binY > 0 ) ? m_binY : 1;
    return ( y - originY() ) / b;
}

inline bool roiSpec::contains( double x, double y ) const
{
    return x >= 0 && y >= 0 && x <= imageW() - 1 && y <= imageH() - 1;
}

/// Static description of one WCC sensor.
/** Everything here comes from configuration and does not change while the
 * application runs. Live quantities such as the commanded ROI, frame rate and
 * exposure time are tracked separately by the application.
 *
 * \ingroup wccCommon
 */
struct sensorConfig
{
    std::string m_name; ///< Logical sensor name as used by the visit file, e.g. "IMX-18".

    std::string m_indiDevice; ///< INDI device supplying the live ROI, fps and exposure time.

    std::string m_shmimOut; ///< ImageStreamIO stream the simulator publishes this sensor's frames to.

    std::string m_shmimIn; ///< ImageStreamIO stream a controller reads this sensor's frames from.

    std::string m_filter{ "SLOANR" }; ///< Filter in the beam, selecting the bandpass.

    double m_pixelSize{ 3.76 }; ///< Detector pixel pitch [um].

    int m_fullW{ 9576 }; ///< Full frame width [pixels].

    int m_fullH{ 6388 }; ///< Full frame height [pixels].

    double m_fieldX{ 0 }; ///< Field angle of the sensor optical center along focal plane X [arcsec].

    double m_fieldY{ 0 }; ///< Field angle of the sensor optical center along focal plane Y [arcsec].

    double m_rotation{ 0 }; ///< Rotation of the sensor about its own optical center [deg].

    /// Column of the sensor optical center, 0-based. Negative means use (m_fullW-1)/2.
    double m_centerX{ -1 };

    /// Row of the sensor optical center, 0-based. Negative means use (m_fullH-1)/2.
    double m_centerY{ -1 };

    double m_darkCurrent{ 0.05 }; ///< Dark current [e-/pixel/s].

    double m_readNoise{ 0.28 }; ///< Read noise [e- rms].

    double m_quantumEfficiency{ 1.0 }; ///< Detector quantum efficiency [e-/photon].

    double m_bandwidth{ 0.150 }; ///< Bandpass width through the filter [um].

    double m_pivotWavelength{ 650.0 }; ///< Bandpass pivot wavelength [nm].

    double m_fullWellDepth{ 65535.0 }; ///< Saturation level [e-].

    /// Column of the sensor optical center, resolving the negative sentinel.
    double centerX() const;

    /// Row of the sensor optical center, resolving the negative sentinel.
    double centerY() const;

    /// Full frame ROI for this sensor.
    roiSpec fullFrameROI() const;
};

inline double sensorConfig::centerX() const
{
    return ( m_centerX >= 0 ) ? m_centerX : 0.5 * ( m_fullW - 1 );
}

inline double sensorConfig::centerY() const
{
    return ( m_centerY >= 0 ) ? m_centerY : 0.5 * ( m_fullH - 1 );
}

inline roiSpec sensorConfig::fullFrameROI() const
{
    roiSpec r;
    r.m_centerX = 0.5 * ( m_fullW - 1 );
    r.m_centerY = 0.5 * ( m_fullH - 1 );
    r.m_w = m_fullW;
    r.m_h = m_fullH;
    r.m_binX = 1;
    r.m_binY = 1;

    return r;
}

/// The telescope plus the sensor array layout, and the current pointing.
/** \ingroup wccCommon
 */
class focalPlaneModel
{

    /** \name Telescope - Data
     *@{
     */
  protected:
    double m_diameter{ 6.5 }; ///< Clear aperture diameter [m].

    double m_fNumber{ 12.0 }; ///< Focal ratio.

    /// Handedness of focal plane X relative to increasing right ascension.
    /** -1 for the WCC, which matches the X_WCC sign in the visit file.
     */
    double m_parity{ -1.0 };
    ///@}

    /** \name Pointing - Data
     *@{
     */
  protected:
    double m_boresightRA{ 0 }; ///< Boresight right ascension, ICRS [deg].

    double m_boresightDec{ 0 }; ///< Boresight declination, ICRS [deg].

    /// Position angle of focal plane +Y, measured east of north [deg].
    double m_positionAngle{ 0 };
    ///@}

    /** \name Sensor Array - Data
     *@{
     */
  protected:
    std::vector<sensorConfig> m_sensors; ///< Sensors in the array, in configuration order.
    ///@}

  public:
    /// Set the telescope optical parameters.
    void setTelescope( double diameter /**< [in] clear aperture diameter [m] */,
                       double fNumber /**< [in] focal ratio */,
                       double parity = -1.0 /**< [in] handedness of focal plane X, +1 or -1 */ );

    /// Set the current boresight pointing and roll.
    void setPointing( double ra /**< [in] boresight right ascension [deg] */,
                      double dec /**< [in] boresight declination [deg] */,
                      double positionAngle /**< [in] position angle of focal plane +Y, east of north [deg] */ );

    /// Clear aperture diameter [m].
    double diameter() const;

    /// Focal ratio.
    double fNumber() const;

    /// Handedness of focal plane X relative to increasing right ascension.
    double parity() const;

    /// Effective focal length [m].
    double efl() const;

    /// Boresight right ascension [deg].
    double boresightRA() const;

    /// Boresight declination [deg].
    double boresightDec() const;

    /// Position angle of focal plane +Y, east of north [deg].
    double positionAngle() const;

    /// Plate scale of the telescope [arcsec/mm].
    double arcsecPerMM() const;

    /// Plate scale at a sensor, per unbinned detector pixel [arcsec/pixel].
    double arcsecPerPixel( const sensorConfig &s /**< [in] sensor of interest */ ) const;

    /// Add a sensor to the array.
    void addSensor( const sensorConfig &s /**< [in] sensor to add */ );

    /// Number of sensors in the array.
    size_t nSensors() const;

    /// Access a sensor by index.
    const sensorConfig &sensor( size_t i /**< [in] index, must be less than nSensors() */ ) const;

    /// Mutable access to a sensor by index.
    sensorConfig &sensor( size_t i /**< [in] index, must be less than nSensors() */ );

    /// The sensor array.
    const std::vector<sensorConfig> &sensors() const;

    /// Find a sensor by its logical name.
    /** Comparison is case insensitive and tolerates the HWK / HAWK spelling
     * difference seen between the sensor list and the guide star entries of the
     * visit file.
     *
     * \returns the sensor index
     * \returns -1 if no sensor matches
     */
    int sensorIndex( const std::string &name /**< [in] logical sensor name */ ) const;

    /// Build the world coordinate system of a sensor ROI at the current pointing.
    /** The resulting wccSkyWCS maps published ROI pixels, 0-based, to ICRS.
     *
     * \returns 0 on success
     * \returns -1 if the sensor index or ROI is invalid
     */
    int roiWCS( size_t iSensor /**< [in] sensor index */,
                const roiSpec &roi /**< [in] commanded ROI */,
                skyWCS &wcs /**< [out] the ROI world coordinate system */ ) const;

    /// Build the world coordinate system of a full sensor frame at the current pointing.
    /** \returns 0 on success
     * \returns -1 if the sensor index is invalid
     */
    int sensorWCS( size_t iSensor /**< [in] sensor index */,
                   skyWCS &wcs /**< [out] the full frame world coordinate system */ ) const;

    /// Field angle of a full sensor pixel, relative to the boresight.
    /** \returns 0 on success
     * \returns -1 if the sensor index is invalid
     */
    int pixelToField( size_t iSensor /**< [in] sensor index */,
                      double x /**< [in] full sensor column, 0-based */,
                      double y /**< [in] full sensor row, 0-based */,
                      double &fieldX /**< [out] field angle along focal plane X [arcsec] */,
                      double &fieldY /**< [out] field angle along focal plane Y [arcsec] */ ) const;

    /// Full sensor pixel corresponding to a field angle.
    /** The pixel may fall outside the sensor, which the caller can test with
     * sensorContainsField() or by bounds checking.
     *
     * \returns 0 on success
     * \returns -1 if the sensor index is invalid
     */
    int fieldToPixel( size_t iSensor /**< [in] sensor index */,
                      double fieldX /**< [in] field angle along focal plane X [arcsec] */,
                      double fieldY /**< [in] field angle along focal plane Y [arcsec] */,
                      double &x /**< [out] full sensor column, 0-based */,
                      double &y /**< [out] full sensor row, 0-based */ ) const;

    /// True if a field angle lands within a sensor's active area.
    bool sensorContainsField( size_t iSensor /**< [in] sensor index */,
                             double fieldX /**< [in] field angle along focal plane X [arcsec] */,
                             double fieldY /**< [in] field angle along focal plane Y [arcsec] */ ) const;

    /// Find the sensor a field angle lands on.
    /** \returns the sensor index
     * \returns -1 if the field angle falls in a gap between sensors
     */
    int sensorForField( double fieldX /**< [in] field angle along focal plane X [arcsec] */,
                        double fieldY /**< [in] field angle along focal plane Y [arcsec] */ ) const;

    /// Angular radius of the circle circumscribing a sensor ROI [deg].
    /** Sized for a cone search that is guaranteed to cover the ROI footprint,
     * with a margin so that stars just outside can still scatter light in.
     *
     * \returns the search radius [deg]
     */
    double roiSearchRadius( size_t iSensor /**< [in] sensor index */,
                            const roiSpec &roi /**< [in] commanded ROI */,
                            double marginPix = 0 /**< [in] extra margin [published pixels] */ ) const;

  protected:
    /// Rotation matrix elements for the sensor mounting rotation and position angle chain.
    /** Fills the CD matrix, in degrees per published pixel, and the reference
     * pixel in published ROI coordinates.
     */
    void chain( const sensorConfig &s /**< [in] sensor of interest */,
                const roiSpec &roi /**< [in] commanded ROI */,
                double &cd11 /**< [out] CD element (1,1) [deg/pixel] */,
                double &cd12 /**< [out] CD element (1,2) [deg/pixel] */,
                double &cd21 /**< [out] CD element (2,1) [deg/pixel] */,
                double &cd22 /**< [out] CD element (2,2) [deg/pixel] */,
                double &crpix1 /**< [out] reference column in published pixels, 0-based */,
                double &crpix2 /**< [out] reference row in published pixels, 0-based */ ) const;
};

inline void focalPlaneModel::setTelescope( double diameter, double fNumber, double parity )
{
    m_diameter = diameter;
    m_fNumber = fNumber;
    m_parity = ( parity < 0 ) ? -1.0 : 1.0;
}

inline void focalPlaneModel::setPointing( double ra, double dec, double positionAngle )
{
    m_boresightRA = ra;
    m_boresightDec = dec;
    m_positionAngle = positionAngle;
}

inline double focalPlaneModel::diameter() const
{
    return m_diameter;
}

inline double focalPlaneModel::fNumber() const
{
    return m_fNumber;
}

inline double focalPlaneModel::parity() const
{
    return m_parity;
}

inline double focalPlaneModel::efl() const
{
    return m_diameter * m_fNumber;
}

inline double focalPlaneModel::boresightRA() const
{
    return m_boresightRA;
}

inline double focalPlaneModel::boresightDec() const
{
    return m_boresightDec;
}

inline double focalPlaneModel::positionAngle() const
{
    return m_positionAngle;
}

inline double focalPlaneModel::arcsecPerMM() const
{
    const double f = efl();

    if( f <= 0 )
    {
        return 0;
    }

    // 1 mm at the focal plane subtends 1e-3/efl radians.
    return 1e-3 / f * rad2arcsec;
}

inline double focalPlaneModel::arcsecPerPixel( const sensorConfig &s ) const
{
    const double f = efl();

    if( f <= 0 )
    {
        return 0;
    }

    return s.m_pixelSize * 1e-6 / f * rad2arcsec;
}

inline void focalPlaneModel::addSensor( const sensorConfig &s )
{
    m_sensors.push_back( s );
}

inline size_t focalPlaneModel::nSensors() const
{
    return m_sensors.size();
}

inline const sensorConfig &focalPlaneModel::sensor( size_t i ) const
{
    return m_sensors[i];
}

inline sensorConfig &focalPlaneModel::sensor( size_t i )
{
    return m_sensors[i];
}

inline const std::vector<sensorConfig> &focalPlaneModel::sensors() const
{
    return m_sensors;
}

/// Normalize a sensor name for tolerant comparison.
/** Upper cases, drops non alphanumeric characters, and collapses the HAWK
 * spelling onto HWK so that "HAWK-04" and "HWK-04" compare equal. The visit
 * file uses both spellings for the same hardware.
 *
 * \returns the normalized name
 *
 * \ingroup wccCommon
 */
inline std::string normalizeSensorName( const std::string &name /**< [in] raw sensor name */ )
{
    std::string out;

    for( char c : name )
    {
        if( c >= 'a' && c <= 'z' )
        {
            out.push_back( static_cast<char>( c - 'a' + 'A' ) );
        }
        else if( ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) )
        {
            out.push_back( c );
        }
    }

    const std::string hawk = "HAWK";
    size_t pos = out.find( hawk );
    while( pos != std::string::npos )
    {
        out.replace( pos, hawk.size(), "HWK" );
        pos = out.find( hawk, pos + 3 );
    }

    return out;
}

inline int focalPlaneModel::sensorIndex( const std::string &name ) const
{
    const std::string want = normalizeSensorName( name );

    for( size_t i = 0; i < m_sensors.size(); ++i )
    {
        if( normalizeSensorName( m_sensors[i].m_name ) == want )
        {
            return static_cast<int>( i );
        }
    }

    return -1;
}

inline void focalPlaneModel::chain( const sensorConfig &s,
                                    const roiSpec &roi,
                                    double &cd11,
                                    double &cd12,
                                    double &cd21,
                                    double &cd22,
                                    double &crpix1,
                                    double &crpix2 ) const
{
    const int bx = ( roi.m_binX > 0 ) ? roi.m_binX : 1;
    const int by = ( roi.m_binY > 0 ) ? roi.m_binY : 1;

    // Plate scale per published pixel. Binning is usually symmetric; when it is
    // not, the two axes carry different scales and the CD matrix absorbs that.
    const double kx = arcsecPerPixel( s ) * bx;
    const double ky = arcsecPerPixel( s ) * by;

    const double cphi = std::cos( s.m_rotation * deg2rad );
    const double sphi = std::sin( s.m_rotation * deg2rad );
    const double cpa = std::cos( m_positionAngle * deg2rad );
    const double spa = std::sin( m_positionAngle * deg2rad );

    // M = R(PA) * P * R(phi), with R(PA) mapping focal plane to (east, north)
    // and P = diag(parity, 1).
    const double p = m_parity;

    const double m11 = cpa * p * cphi + spa * sphi;
    const double m12 = cpa * p * -sphi + spa * cphi;
    const double m21 = -spa * p * cphi + cpa * sphi;
    const double m22 = -spa * p * -sphi + cpa * cphi;

    // Scale by the plate scale of each pixel axis, then arcsec to degrees.
    cd11 = arcsec2deg * m11 * kx;
    cd12 = arcsec2deg * m12 * ky;
    cd21 = arcsec2deg * m21 * kx;
    cd22 = arcsec2deg * m22 * ky;

    // Reference pixel: the sensor pixel that sits on the boresight. Invert the
    // mounting rotation only, because the sensor offset is defined in focal
    // plane coordinates.
    const double sx = arcsecPerPixel( s );

    double refFullX = s.centerX();
    double refFullY = s.centerY();

    if( sx > 0 )
    {
        // R(phi)^-1 applied to the sensor field offset, converted to pixels.
        const double ix = ( cphi * s.m_fieldX + sphi * s.m_fieldY ) / sx;
        const double iy = ( -sphi * s.m_fieldX + cphi * s.m_fieldY ) / sx;

        refFullX -= ix;
        refFullY -= iy;
    }

    crpix1 = roi.toImageX( refFullX );
    crpix2 = roi.toImageY( refFullY );
}

inline int focalPlaneModel::roiWCS( size_t iSensor, const roiSpec &roi, skyWCS &wcs ) const
{
    if( iSensor >= m_sensors.size() || roi.imageW() < 1 || roi.imageH() < 1 )
    {
        return -1;
    }

    double cd11, cd12, cd21, cd22, crpix1, crpix2;
    chain( m_sensors[iSensor], roi, cd11, cd12, cd21, cd22, crpix1, crpix2 );

    wcs.setReference( m_boresightRA, m_boresightDec );
    wcs.setReferencePixel( crpix1, crpix2 );
    wcs.setCDMatrix( cd11, cd12, cd21, cd22 );

    return 0;
}

inline int focalPlaneModel::sensorWCS( size_t iSensor, skyWCS &wcs ) const
{
    if( iSensor >= m_sensors.size() )
    {
        return -1;
    }

    return roiWCS( iSensor, m_sensors[iSensor].fullFrameROI(), wcs );
}

inline int focalPlaneModel::pixelToField( size_t iSensor,
                                          double x,
                                          double y,
                                          double &fieldX,
                                          double &fieldY ) const
{
    if( iSensor >= m_sensors.size() )
    {
        return -1;
    }

    const sensorConfig &s = m_sensors[iSensor];
    const double k = arcsecPerPixel( s );

    const double cphi = std::cos( s.m_rotation * deg2rad );
    const double sphi = std::sin( s.m_rotation * deg2rad );

    const double u = k * ( x - s.centerX() );
    const double v = k * ( y - s.centerY() );

    fieldX = s.m_fieldX + cphi * u - sphi * v;
    fieldY = s.m_fieldY + sphi * u + cphi * v;

    return 0;
}

inline int focalPlaneModel::fieldToPixel( size_t iSensor,
                                          double fieldX,
                                          double fieldY,
                                          double &x,
                                          double &y ) const
{
    if( iSensor >= m_sensors.size() )
    {
        return -1;
    }

    const sensorConfig &s = m_sensors[iSensor];
    const double k = arcsecPerPixel( s );

    if( k <= 0 )
    {
        return -1;
    }

    const double cphi = std::cos( s.m_rotation * deg2rad );
    const double sphi = std::sin( s.m_rotation * deg2rad );

    const double dx = fieldX - s.m_fieldX;
    const double dy = fieldY - s.m_fieldY;

    x = s.centerX() + ( cphi * dx + sphi * dy ) / k;
    y = s.centerY() + ( -sphi * dx + cphi * dy ) / k;

    return 0;
}

inline bool focalPlaneModel::sensorContainsField( size_t iSensor, double fieldX, double fieldY ) const
{
    double x, y;

    if( fieldToPixel( iSensor, fieldX, fieldY, x, y ) < 0 )
    {
        return false;
    }

    const sensorConfig &s = m_sensors[iSensor];

    return x >= 0 && y >= 0 && x <= s.m_fullW - 1 && y <= s.m_fullH - 1;
}

inline int focalPlaneModel::sensorForField( double fieldX, double fieldY ) const
{
    for( size_t i = 0; i < m_sensors.size(); ++i )
    {
        if( sensorContainsField( i, fieldX, fieldY ) )
        {
            return static_cast<int>( i );
        }
    }

    return -1;
}

inline double focalPlaneModel::roiSearchRadius( size_t iSensor, const roiSpec &roi, double marginPix ) const
{
    if( iSensor >= m_sensors.size() )
    {
        return 0;
    }

    const sensorConfig &s = m_sensors[iSensor];
    const double k = arcsecPerPixel( s );

    const double halfW = 0.5 * roi.m_w + marginPix * std::max( roi.m_binX, 1 );
    const double halfH = 0.5 * roi.m_h + marginPix * std::max( roi.m_binY, 1 );

    // Half diagonal of the ROI, in arcsec, then to degrees.
    return std::hypot( halfW, halfH ) * k * arcsec2deg;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccFocalPlane_hpp
