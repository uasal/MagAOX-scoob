/** \file wccSkyWCS.hpp
 * \brief Gnomonic (TAN) world coordinate system for WCC detector planes.
 * \author Adam Schilperoort
 *
 * C++ equivalent of the `astropy.wcs.WCS` usage in
 * `uasal_star_catalog_simulator_prysm.py`, which builds a two axis WCS with
 * `ctype = ["RA---TAN", "DEC--TAN"]` and then calls
 * `astropy.wcs.utils.skycoord_to_pixel()` to place catalog stars on a detector.
 *
 * Only the TAN (gnomonic) projection is implemented, with a general CD matrix
 * so that detector rotation and telescope position angle can be folded in.
 * Results agree with wcslib/astropy to better than 1e-9 pixel over a one
 * degree field (see wccCommon_test.cpp).
 *
 * \ingroup wccCommon_files
 */

#ifndef wccSkyWCS_hpp
#define wccSkyWCS_hpp

#include <cmath>

#include "wccUnits.hpp"

namespace MagAOX
{
namespace wcc
{

/// Gnomonic (TAN) world coordinate system relating sky coordinates to detector pixels.
/** The pixel convention is 0-based, matching `skycoord_to_pixel(..., origin=0)`
 * and C array indexing. A FITS `CRPIXn` keyword is 1-based, so
 * `crpix0 = CRPIXn - 1`.
 *
 * The transform is the standard FITS chain
 * \f[ (x,y) = \mathrm{CD} \cdot (p - p_\mathrm{ref}) \f]
 * where \f$(x,y)\f$ are intermediate world coordinates in degrees on the
 * tangent plane, followed by the TAN deprojection about the reference sky
 * position.
 *
 * \ingroup wccCommon
 */
class skyWCS
{

    /** \name Reference Position - Data
     *@{
     */
  protected:
    double m_crval1{ 0 }; ///< Reference right ascension, ICRS [deg].

    double m_crval2{ 0 }; ///< Reference declination, ICRS [deg].

    double m_crpix1{ 0 }; ///< Reference pixel on axis 1 (column/x), 0-based.

    double m_crpix2{ 0 }; ///< Reference pixel on axis 2 (row/y), 0-based.
    ///@}

    /** \name Linear Transform - Data
     *@{
     */
  protected:
    double m_cd11{ 1 }; ///< CD matrix element (1,1) [deg/pixel].

    double m_cd12{ 0 }; ///< CD matrix element (1,2) [deg/pixel].

    double m_cd21{ 0 }; ///< CD matrix element (2,1) [deg/pixel].

    double m_cd22{ 1 }; ///< CD matrix element (2,2) [deg/pixel].

    double m_inv11{ 1 }; ///< Inverse CD element (1,1) [pixel/deg]. Kept in sync by updateInverse().

    double m_inv12{ 0 }; ///< Inverse CD element (1,2) [pixel/deg].

    double m_inv21{ 0 }; ///< Inverse CD element (2,1) [pixel/deg].

    double m_inv22{ 1 }; ///< Inverse CD element (2,2) [pixel/deg].

    /// Cached sine of the reference declination. Kept in sync by setReference().
    double m_sinDec0{ 0 };

    /// Cached cosine of the reference declination.
    double m_cosDec0{ 1 };
    ///@}

  public:
    /// Set the reference sky position (FITS CRVAL1/CRVAL2).
    void setReference( double ra /**< [in] reference right ascension [deg] */,
                       double dec /**< [in] reference declination [deg] */ );

    /// Set the reference pixel, 0-based (FITS CRPIX minus one).
    void setReferencePixel( double x /**< [in] reference column, 0-based */,
                            double y /**< [in] reference row, 0-based */ );

    /// Set the CD matrix directly.
    void setCDMatrix( double cd11 /**< [in] element (1,1) [deg/pixel] */,
                      double cd12 /**< [in] element (1,2) [deg/pixel] */,
                      double cd21 /**< [in] element (2,1) [deg/pixel] */,
                      double cd22 /**< [in] element (2,2) [deg/pixel] */ );

    /// Set the CD matrix from a pixel scale plus a rotation, the FITS CDELT/CROTA2 form.
    /** Equivalent to `wcs.wcs.cdelt = [cdelt1, cdelt2]` with `CROTA2 = rotation`.
     * With `rotation = 0` this reduces to the diagonal CD matrix used by the
     * Python simulator.
     */
    void setPixelScale( double cdelt1 /**< [in] scale on axis 1 [deg/pixel] */,
                        double cdelt2 /**< [in] scale on axis 2 [deg/pixel] */,
                        double rotation = 0 /**< [in] rotation of the detector on the sky [deg] */ );

    /// Reference right ascension [deg].
    double crval1() const;

    /// Reference declination [deg].
    double crval2() const;

    /// Reference column, 0-based.
    double crpix1() const;

    /// Reference row, 0-based.
    double crpix2() const;

    /// CD matrix element (1,1) [deg/pixel].
    double cd11() const;

    /// CD matrix element (1,2) [deg/pixel].
    double cd12() const;

    /// CD matrix element (2,1) [deg/pixel].
    double cd21() const;

    /// CD matrix element (2,2) [deg/pixel].
    double cd22() const;

    /// Mean absolute pixel scale implied by the CD matrix [deg/pixel].
    /** Uses the square root of the absolute determinant, so it is the
     * scale of a pixel of equal area.
     */
    double pixelScale() const;

    /// Project a sky position onto the tangent plane.
    /** \returns true if the position is on the visible hemisphere
     * \returns false if the position is at or beyond 90 degrees from the reference, in which case
     *          the outputs are not set
     */
    bool world2intermediate( double ra /**< [in] right ascension [deg] */,
                            double dec /**< [in] declination [deg] */,
                            double &x /**< [out] tangent plane coordinate on axis 1 [deg] */,
                            double &y /**< [out] tangent plane coordinate on axis 2 [deg] */ ) const;

    /// Deproject a tangent plane position back onto the sky.
    void intermediate2world( double x /**< [in] tangent plane coordinate on axis 1 [deg] */,
                             double y /**< [in] tangent plane coordinate on axis 2 [deg] */,
                             double &ra /**< [out] right ascension [deg], in [0,360) */,
                             double &dec /**< [out] declination [deg] */ ) const;

    /// Convert a sky position to a 0-based detector pixel.
    /** The equivalent of `astropy.wcs.utils.skycoord_to_pixel(coord, wcs)`.
     *
     * \returns true on success
     * \returns false if the position is not on the visible hemisphere
     */
    bool world2pix( double ra /**< [in] right ascension [deg] */,
                    double dec /**< [in] declination [deg] */,
                    double &x /**< [out] column, 0-based, may be fractional */,
                    double &y /**< [out] row, 0-based, may be fractional */ ) const;

    /// Convert a 0-based detector pixel to a sky position.
    void pix2world( double x /**< [in] column, 0-based */,
                    double y /**< [in] row, 0-based */,
                    double &ra /**< [out] right ascension [deg], in [0,360) */,
                    double &dec /**< [out] declination [deg] */ ) const;

  protected:
    /// Recompute the cached inverse of the CD matrix.
    void updateInverse();
};

inline void skyWCS::setReference( double ra, double dec )
{
    m_crval1 = ra;
    m_crval2 = dec;
    m_sinDec0 = std::sin( dec * deg2rad );
    m_cosDec0 = std::cos( dec * deg2rad );
}

inline void skyWCS::setReferencePixel( double x, double y )
{
    m_crpix1 = x;
    m_crpix2 = y;
}

inline void skyWCS::setCDMatrix( double cd11, double cd12, double cd21, double cd22 )
{
    m_cd11 = cd11;
    m_cd12 = cd12;
    m_cd21 = cd21;
    m_cd22 = cd22;
    updateInverse();
}

inline void skyWCS::setPixelScale( double cdelt1, double cdelt2, double rotation )
{
    const double c = std::cos( rotation * deg2rad );
    const double s = std::sin( rotation * deg2rad );

    // FITS CDELT + CROTA2 convention, Greisen & Calabretta 2002 eq. 189.
    setCDMatrix( cdelt1 * c, -cdelt2 * s, cdelt1 * s, cdelt2 * c );
}

inline double skyWCS::crval1() const
{
    return m_crval1;
}

inline double skyWCS::crval2() const
{
    return m_crval2;
}

inline double skyWCS::crpix1() const
{
    return m_crpix1;
}

inline double skyWCS::crpix2() const
{
    return m_crpix2;
}

inline double skyWCS::cd11() const
{
    return m_cd11;
}

inline double skyWCS::cd12() const
{
    return m_cd12;
}

inline double skyWCS::cd21() const
{
    return m_cd21;
}

inline double skyWCS::cd22() const
{
    return m_cd22;
}

inline double skyWCS::pixelScale() const
{
    return std::sqrt( std::fabs( m_cd11 * m_cd22 - m_cd12 * m_cd21 ) );
}

inline void skyWCS::updateInverse()
{
    const double det = m_cd11 * m_cd22 - m_cd12 * m_cd21;

    if( det == 0 )
    {
        // Degenerate CD matrix. Leave the inverse as identity so that callers
        // get finite (if meaningless) pixels rather than NaN or a trap.
        m_inv11 = 1;
        m_inv12 = 0;
        m_inv21 = 0;
        m_inv22 = 1;
        return;
    }

    m_inv11 = m_cd22 / det;
    m_inv12 = -m_cd12 / det;
    m_inv21 = -m_cd21 / det;
    m_inv22 = m_cd11 / det;
}

inline bool skyWCS::world2intermediate( double ra, double dec, double &x, double &y ) const
{
    const double dra = ( ra - m_crval1 ) * deg2rad;
    const double sinDec = std::sin( dec * deg2rad );
    const double cosDec = std::cos( dec * deg2rad );
    const double cosDra = std::cos( dra );
    const double sinDra = std::sin( dra );

    // Cosine of the angular distance from the reference position.
    const double cosc = sinDec * m_sinDec0 + cosDec * m_cosDec0 * cosDra;

    if( cosc <= 0 )
    {
        return false;
    }

    x = rad2deg * cosDec * sinDra / cosc;
    y = rad2deg * ( sinDec * m_cosDec0 - cosDec * m_sinDec0 * cosDra ) / cosc;

    return true;
}

inline void skyWCS::intermediate2world( double x, double y, double &ra, double &dec ) const
{
    const double xi = x * deg2rad;
    const double eta = y * deg2rad;

    dec = std::asin( ( m_sinDec0 + eta * m_cosDec0 ) / std::sqrt( 1.0 + xi * xi + eta * eta ) ) * rad2deg;

    ra = m_crval1 + std::atan2( xi, m_cosDec0 - eta * m_sinDec0 ) * rad2deg;

    // Normalize into [0,360).
    ra = std::fmod( ra, 360.0 );
    if( ra < 0 )
    {
        ra += 360.0;
    }
}

inline bool skyWCS::world2pix( double ra, double dec, double &x, double &y ) const
{
    double ix, iy;

    if( !world2intermediate( ra, dec, ix, iy ) )
    {
        return false;
    }

    x = m_crpix1 + m_inv11 * ix + m_inv12 * iy;
    y = m_crpix2 + m_inv21 * ix + m_inv22 * iy;

    return true;
}

inline void skyWCS::pix2world( double x, double y, double &ra, double &dec ) const
{
    const double dx = x - m_crpix1;
    const double dy = y - m_crpix2;

    intermediate2world( m_cd11 * dx + m_cd12 * dy, m_cd21 * dx + m_cd22 * dy, ra, dec );
}

/// Angular separation between two sky positions, using the Vincenty formula.
/** Numerically stable for both very small and very large separations.
 *
 * \returns the separation in degrees
 *
 * \ingroup wccCommon
 */
inline double angularSeparation( double ra1 /**< [in] first right ascension [deg] */,
                                 double dec1 /**< [in] first declination [deg] */,
                                 double ra2 /**< [in] second right ascension [deg] */,
                                 double dec2 /**< [in] second declination [deg] */ )
{
    const double dra = ( ra2 - ra1 ) * deg2rad;
    const double sd1 = std::sin( dec1 * deg2rad );
    const double cd1 = std::cos( dec1 * deg2rad );
    const double sd2 = std::sin( dec2 * deg2rad );
    const double cd2 = std::cos( dec2 * deg2rad );

    const double num1 = cd2 * std::sin( dra );
    const double num2 = cd1 * sd2 - sd1 * cd2 * std::cos( dra );
    const double den = sd1 * sd2 + cd1 * cd2 * std::cos( dra );

    return std::atan2( std::sqrt( num1 * num1 + num2 * num2 ), den ) * rad2deg;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccSkyWCS_hpp
