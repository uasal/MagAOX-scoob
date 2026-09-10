/** \file wccMDFT.hpp
 * \brief Matrix triple-product DFT diffraction propagation, a C++ port of the prysm
 *        functions used by the WCC star catalog simulator.
 * \author Adam Schilperoort
 *
 * `uasal_star_catalog_simulator_prysm.py` builds each star image from a
 * diffraction PSF computed by prysm. This header reimplements exactly the prysm
 * surface that script touches, so that the simulator can run inside a real time
 * MagAO-X application with no Python interpreter:
 *
 * | prysm                                   | here                     |
 * |-----------------------------------------|--------------------------|
 * | `prysm.fttools.fftrange`                | fftRange()               |
 * | `prysm.coordinates.make_xy_grid`        | makeXYGrid()             |
 * | `prysm.coordinates.cart_to_polar`       | cartToPolar()            |
 * | `prysm.geometry.circle`                 | circleMask()             |
 * | `prysm.geometry.spider`                 | spiderMask()             |
 * | `prysm.polynomials.sum_of_2d_modes`     | sumOf2DModes()           |
 * | `prysm.propagation.Q_for_sampling`      | qForSampling()           |
 * | `prysm.fttools.MatrixDFTExecutor`       | matrixDFT                |
 * | `Wavefront.from_amp_and_phase`          | wavefrontFromAmpAndPhase()|
 * | `Wavefront.focus_fixed_sampling`        | focusFixedSampling()     |
 *
 * The matrix DFT follows R. Soummer et al., Optics Express 15, 15935 (2007),
 * eqs. (10)-(11): the transform is evaluated as a matrix triple product
 * \f$ \mathrm{out} = E_\mathrm{out} \cdot A \cdot E_\mathrm{in} \f$, which lets
 * the output plane sampling and extent be specified directly rather than being
 * dictated by an FFT grid. Basis matrices are cached, so repeated calls at the
 * same geometry only pay for the two matrix products.
 *
 * \par Unit conventions
 * These match prysm and must be respected by callers:
 * - pupil sample spacing and propagation distance: any single consistent length unit
 *   (prysm documents mm, and only the dimensionless ratio efl/diameter enters Q)
 * - wavelength: microns
 * - output plane sample spacing: microns
 * - optical path difference: nanometres
 *
 * \par Deviation from the Python simulator
 * The Python script passes fractional pixel offsets straight into
 * `focus_fixed_sampling(shift=...)`, which interprets its `shift` argument in
 * output plane physical units (microns) and divides by `output_dx` internally.
 * Fractional pixel shifts are therefore attenuated by the pixel size in the
 * Python version. focusFixedSampling() here takes the shift in output pixels,
 * which is what the star placement code actually wants.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccMDFT_hpp
#define wccMDFT_hpp

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include "wccUnits.hpp"

namespace MagAOX
{
namespace wcc
{

/// Complex matrix type used for the DFT basis matrices and fields.
typedef Eigen::Matrix<std::complex<double>, Eigen::Dynamic, Eigen::Dynamic> cmplxMatrixT;

/// Real matrix type used for pupil amplitude and OPD maps.
typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> realMatrixT;

/// FFT aligned index range, the equivalent of `prysm.fttools.fftrange`.
/** Returns `-(n/2) ... -(n/2)+n-1` with integer division, which places the
 * origin at index `n/2` exactly as numpy's `fftshift` convention does.
 *
 * \ingroup wccCommon
 */
inline std::vector<double> fftRange( int n /**< [in] number of samples */ )
{
    std::vector<double> r;
    r.reserve( static_cast<size_t>( std::max( n, 0 ) ) );

    const int start = -( n / 2 );
    for( int i = 0; i < n; ++i )
    {
        r.push_back( static_cast<double>( start + i ) );
    }

    return r;
}

/// Build cartesian coordinate grids over a square array, the equivalent of `prysm.coordinates.make_xy_grid`.
/** The grid is FFT aligned and spans a full width of `diameter`, so the sample
 * spacing is `diameter/samples`.
 *
 * \ingroup wccCommon
 */
inline void makeXYGrid( int samples /**< [in] samples across one axis */,
                        double diameter /**< [in] full width of the grid [length] */,
                        realMatrixT &x /**< [out] x coordinate of each sample [length] */,
                        realMatrixT &y /**< [out] y coordinate of each sample [length] */ )
{
    const double dx = ( samples > 0 ) ? diameter / samples : 0;
    const std::vector<double> r = fftRange( samples );

    x.resize( samples, samples );
    y.resize( samples, samples );

    for( int i = 0; i < samples; ++i )
    {
        for( int j = 0; j < samples; ++j )
        {
            // Row index is y, column index is x, matching numpy's (row, col) order.
            x( i, j ) = r[static_cast<size_t>( j )] * dx;
            y( i, j ) = r[static_cast<size_t>( i )] * dx;
        }
    }
}

/// Convert cartesian grids to polar, the equivalent of `prysm.coordinates.cart_to_polar`.
/** \ingroup wccCommon
 */
inline void cartToPolar( const realMatrixT &x /**< [in] x coordinates */,
                         const realMatrixT &y /**< [in] y coordinates */,
                         realMatrixT &r /**< [out] radial coordinate, same units as x and y */,
                         realMatrixT &t /**< [out] azimuthal coordinate [rad], in (-pi,pi] */ )
{
    r.resize( x.rows(), x.cols() );
    t.resize( x.rows(), x.cols() );

    for( Eigen::Index i = 0; i < x.rows(); ++i )
    {
        for( Eigen::Index j = 0; j < x.cols(); ++j )
        {
            r( i, j ) = std::hypot( x( i, j ), y( i, j ) );
            t( i, j ) = std::atan2( y( i, j ), x( i, j ) );
        }
    }
}

/// Circular aperture mask, the equivalent of `prysm.geometry.circle`.
/** \ingroup wccCommon
 */
inline void circleMask( double radius /**< [in] aperture radius, same units as r */,
                        const realMatrixT &r /**< [in] radial coordinate grid */,
                        realMatrixT &mask /**< [out] 1 inside the radius, 0 outside */ )
{
    mask.resize( r.rows(), r.cols() );

    for( Eigen::Index i = 0; i < r.rows(); ++i )
    {
        for( Eigen::Index j = 0; j < r.cols(); ++j )
        {
            mask( i, j ) = ( r( i, j ) <= radius ) ? 1.0 : 0.0;
        }
    }
}

/// Spider vane obscuration mask, the equivalent of `prysm.geometry.spider`.
/** The result is 0 on a vane and 1 elsewhere, so it multiplies an aperture.
 *
 * \ingroup wccCommon
 */
inline void spiderMask( int vanes /**< [in] number of vanes radiating from the center */,
                        double width /**< [in] vane width, same units as x and y */,
                        const realMatrixT &x /**< [in] x coordinate grid */,
                        const realMatrixT &y /**< [in] y coordinate grid */,
                        realMatrixT &mask /**< [out] 0 on a vane, 1 elsewhere */,
                        double rotation = 0 /**< [in] clockwise rotation of the vane pattern [deg] */,
                        double centerX = 0 /**< [in] x of the point the vanes emanate from */,
                        double centerY = 0 /**< [in] y of the point the vanes emanate from */ )
{
    mask.resize( x.rows(), x.cols() );
    mask.setOnes();

    if( vanes <= 0 || width <= 0 )
    {
        return;
    }

    const double halfWidth = 0.5 * width;
    const double rot = rotation * deg2rad;
    const double step = 2.0 * pi / vanes;

    for( Eigen::Index i = 0; i < x.rows(); ++i )
    {
        for( Eigen::Index j = 0; j < x.cols(); ++j )
        {
            const double dx = x( i, j ) - centerX;
            const double dy = y( i, j ) - centerY;
            const double rr = std::hypot( dx, dy );
            const double pp = std::atan2( dy, dx ) - rot;

            for( int v = 0; v < vanes; ++v )
            {
                const double ang = pp + step * v;
                const double xr = rr * std::cos( ang );
                const double yr = rr * std::sin( ang );

                if( xr > 0 && std::fabs( yr ) < halfWidth )
                {
                    mask( i, j ) = 0.0;
                    break;
                }
            }
        }
    }
}

/// Weighted sum of a stack of equally shaped 2D modes, the equivalent of `prysm.polynomials.sum_of_2d_modes`.
/** \returns 0 on success
 * \returns -1 if the mode stack is empty or the weight count does not match
 *
 * \ingroup wccCommon
 */
inline int sumOf2DModes( const std::vector<std::vector<double>> &modes /**< [in] stack of modes, each of size npix */,
                         const std::vector<double> &weights /**< [in] one weight per mode */,
                         std::vector<double> &out /**< [out] weighted sum, sized to one mode */ )
{
    if( modes.empty() || modes.size() != weights.size() )
    {
        return -1;
    }

    const size_t npix = modes[0].size();
    out.assign( npix, 0.0 );

    for( size_t m = 0; m < modes.size(); ++m )
    {
        if( modes[m].size() != npix )
        {
            return -1;
        }

        const double w = weights[m];
        for( size_t i = 0; i < npix; ++i )
        {
            out[i] += w * modes[m][i];
        }
    }

    return 0;
}

/// Value of the prysm sampling parameter Q for a requested output sampling.
/** The equivalent of `prysm.propagation.Q_for_sampling`. Q is the number of
 * output samples per diffraction resolution element, so Q = 2 is Nyquist.
 *
 * \returns the required Q
 *
 * \ingroup wccCommon
 */
inline double qForSampling( double inputDiameter /**< [in] pupil diameter [length] */,
                            double propDist /**< [in] propagation distance, same unit as inputDiameter */,
                            double wavelength /**< [in] wavelength [um] */,
                            double outputDx /**< [in] output plane sample spacing [um] */ )
{
    if( inputDiameter == 0 || outputDx == 0 )
    {
        return 0;
    }

    const double resolutionElement = ( wavelength * propDist ) / inputDiameter;

    return resolutionElement / outputDx;
}

/// Value of Q expressed through the focal ratio.
/** Q depends on the pupil only through efl/diameter, so this is an equivalent
 * and more convenient entry point when the focal ratio is the known quantity.
 *
 * \returns the required Q
 *
 * \ingroup wccCommon
 */
inline double qForFNumber( double wavelength /**< [in] wavelength [um] */,
                           double fNumber /**< [in] focal ratio, efl divided by diameter */,
                           double outputDx /**< [in] output plane sample spacing [um] */ )
{
    if( outputDx == 0 )
    {
        return 0;
    }

    return wavelength * fNumber / outputDx;
}

/// Matrix triple-product DFT engine, the equivalent of `prysm.fttools.MatrixDFTExecutor`.
/** Basis matrices are cached on the full transform geometry, which is the
 * tuple (Q, input shape, output shape, shift). A star field simulator evaluates
 * the same geometry at a small number of quantized sub-pixel shifts, so after
 * warm up every transform is two matrix products with no basis construction.
 *
 * This class is not thread safe. Give each worker thread its own instance.
 *
 * \ingroup wccCommon
 */
class matrixDFT
{

    /** \name Basis Cache - Data
     *@{
     */
  protected:
    /// One cached pair of DFT basis matrices along with the geometry that produced it.
    struct basisEntry
    {
        double m_qRows{ 0 };  ///< Q applied along the row (slow) axis.
        double m_qCols{ 0 };  ///< Q applied along the column (fast) axis.
        int m_rowsIn{ 0 };    ///< Input rows, Na in Soummer's notation.
        int m_colsIn{ 0 };    ///< Input columns, Ma.
        int m_rowsOut{ 0 };   ///< Output rows, Nb.
        int m_colsOut{ 0 };   ///< Output columns, Mb.
        double m_shiftX{ 0 }; ///< Output shift along the column axis [output samples].
        double m_shiftY{ 0 }; ///< Output shift along the row axis [output samples].

        cmplxMatrixT m_eOut; ///< Left basis, shape (rowsOut, rowsIn).
        cmplxMatrixT m_eIn;  ///< Right basis, shape (colsIn, colsOut), carries the normalization.
    };

    /// Cached basis matrices. Linear search is fine because the working set is tiny.
    std::vector<basisEntry> m_cache;

    /// Maximum number of cached basis pairs before the cache is cleared.
    size_t m_maxCache{ 64 };
    ///@}

  public:
    /// Forward two dimensional matrix DFT.
    /** Equivalent to `mdft.dft2(ary, Q, samples, shift)`, and to
     * `ifftshift(fft2(fftshift(ary)))` up to the output sampling that Q selects.
     *
     * \returns 0 on success
     * \returns -1 if the requested geometry is invalid
     */
    int dft2( const cmplxMatrixT &in /**< [in] input field, shape (rowsIn, colsIn) */,
              double q /**< [in] sampling parameter, applied to both axes */,
              int rowsOut /**< [in] output rows */,
              int colsOut /**< [in] output columns */,
              double shiftX /**< [in] output shift along the column axis [output samples] */,
              double shiftY /**< [in] output shift along the row axis [output samples] */,
              cmplxMatrixT &out /**< [out] transformed field, shape (rowsOut, colsOut) */ );

    /// Drop all cached basis matrices to release memory.
    void clear();

    /// Total heap size of the cached basis matrices [bytes].
    size_t nbytes() const;

    /// Number of cached basis pairs.
    size_t cacheSize() const;

    /// Set the maximum number of cached basis pairs.
    void maxCache( size_t n /**< [in] maximum entries, must be at least 1 */ );

  protected:
    /// Find, or build and cache, the basis pair for a transform geometry.
    /** \returns a pointer to the cache entry
     * \returns nullptr if the geometry is invalid
     */
    const basisEntry *bases( double qRows /**< [in] Q on the row axis */,
                             double qCols /**< [in] Q on the column axis */,
                             int rowsIn /**< [in] input rows */,
                             int colsIn /**< [in] input columns */,
                             int rowsOut /**< [in] output rows */,
                             int colsOut /**< [in] output columns */,
                             double shiftX /**< [in] shift on the column axis */,
                             double shiftY /**< [in] shift on the row axis */ );
};

inline int matrixDFT::dft2( const cmplxMatrixT &in,
                            double q,
                            int rowsOut,
                            int colsOut,
                            double shiftX,
                            double shiftY,
                            cmplxMatrixT &out )
{
    const int rowsIn = static_cast<int>( in.rows() );
    const int colsIn = static_cast<int>( in.cols() );

    const basisEntry *e = bases( q, q, rowsIn, colsIn, rowsOut, colsOut, shiftX, shiftY );

    if( e == nullptr )
    {
        return -1;
    }

    out.noalias() = e->m_eOut * in * e->m_eIn;

    return 0;
}

inline void matrixDFT::clear()
{
    m_cache.clear();
}

inline size_t matrixDFT::nbytes() const
{
    size_t total = 0;

    for( const basisEntry &e : m_cache )
    {
        total += static_cast<size_t>( e.m_eOut.size() + e.m_eIn.size() ) * sizeof( std::complex<double> );
    }

    return total;
}

inline size_t matrixDFT::cacheSize() const
{
    return m_cache.size();
}

inline void matrixDFT::maxCache( size_t n )
{
    m_maxCache = ( n < 1 ) ? 1 : n;
}

inline const matrixDFT::basisEntry *matrixDFT::bases( double qRows,
                                                      double qCols,
                                                      int rowsIn,
                                                      int colsIn,
                                                      int rowsOut,
                                                      int colsOut,
                                                      double shiftX,
                                                      double shiftY )
{
    if( rowsIn < 1 || colsIn < 1 || rowsOut < 1 || colsOut < 1 || qRows == 0 || qCols == 0 )
    {
        return nullptr;
    }

    for( const basisEntry &e : m_cache )
    {
        if( e.m_qRows == qRows && e.m_qCols == qCols && e.m_rowsIn == rowsIn && e.m_colsIn == colsIn &&
            e.m_rowsOut == rowsOut && e.m_colsOut == colsOut && e.m_shiftX == shiftX && e.m_shiftY == shiftY )
        {
            return &e;
        }
    }

    if( m_cache.size() >= m_maxCache )
    {
        m_cache.clear();
    }

    basisEntry e;
    e.m_qRows = qRows;
    e.m_qCols = qCols;
    e.m_rowsIn = rowsIn;
    e.m_colsIn = colsIn;
    e.m_rowsOut = rowsOut;
    e.m_colsOut = colsOut;
    e.m_shiftX = shiftX;
    e.m_shiftY = shiftY;

    // Soummer's "zoom" factors, the reciprocals of Q.
    const double mn = 1.0 / qRows;
    const double mm = 1.0 / qCols;

    std::vector<double> X = fftRange( colsIn );  // input columns
    std::vector<double> Y = fftRange( rowsIn );  // input rows
    std::vector<double> U = fftRange( colsOut ); // output columns
    std::vector<double> V = fftRange( rowsOut ); // output rows

    if( shiftX != 0 )
    {
        for( double &v : X )
        {
            v -= shiftX;
        }
        for( double &v : U )
        {
            v -= shiftX;
        }
    }

    if( shiftY != 0 )
    {
        for( double &v : Y )
        {
            v -= shiftY;
        }
        for( double &v : V )
        {
            v -= shiftY;
        }
    }

    const double kRow = -2.0 * pi / rowsIn * mn;
    const double kCol = -2.0 * pi / colsIn * mm;

    // prysm applies the whole 1/(Na*Q) normalization to the right hand basis.
    const double norm = 1.0 / ( static_cast<double>( rowsIn ) * qRows );

    e.m_eOut.resize( rowsOut, rowsIn );
    for( int b = 0; b < rowsOut; ++b )
    {
        for( int a = 0; a < rowsIn; ++a )
        {
            const double ph = kRow * Y[static_cast<size_t>( a )] * V[static_cast<size_t>( b )];
            e.m_eOut( b, a ) = std::complex<double>( std::cos( ph ), std::sin( ph ) );
        }
    }

    e.m_eIn.resize( colsIn, colsOut );
    for( int a = 0; a < colsIn; ++a )
    {
        for( int b = 0; b < colsOut; ++b )
        {
            const double ph = kCol * X[static_cast<size_t>( a )] * U[static_cast<size_t>( b )];
            e.m_eIn( a, b ) = std::complex<double>( norm * std::cos( ph ), norm * std::sin( ph ) );
        }
    }

    m_cache.push_back( std::move( e ) );

    return &m_cache.back();
}

/// Build a complex pupil field from amplitude and optical path difference.
/** The equivalent of `prysm.propagation.Wavefront.from_amp_and_phase`. The
 * factor of 1e3 converts the OPD from nanometres to the microns the wavelength
 * is given in.
 *
 * \ingroup wccCommon
 */
inline void wavefrontFromAmpAndPhase( const realMatrixT &amplitude /**< [in] pupil amplitude transmission */,
                                      const realMatrixT *opd /**< [in] OPD map [nm], or nullptr for a flat wavefront */,
                                      double wavelength /**< [in] wavelength [um] */,
                                      cmplxMatrixT &field /**< [out] complex pupil field */ )
{
    field.resize( amplitude.rows(), amplitude.cols() );

    if( opd == nullptr || opd->rows() != amplitude.rows() || opd->cols() != amplitude.cols() )
    {
        for( Eigen::Index i = 0; i < amplitude.rows(); ++i )
        {
            for( Eigen::Index j = 0; j < amplitude.cols(); ++j )
            {
                field( i, j ) = std::complex<double>( amplitude( i, j ), 0.0 );
            }
        }

        return;
    }

    const double prefix = 2.0 * pi / ( wavelength * 1e3 );

    for( Eigen::Index i = 0; i < amplitude.rows(); ++i )
    {
        for( Eigen::Index j = 0; j < amplitude.cols(); ++j )
        {
            const double ph = prefix * ( *opd )( i, j );
            field( i, j ) = std::complex<double>( amplitude( i, j ) * std::cos( ph ),
                                                  amplitude( i, j ) * std::sin( ph ) );
        }
    }
}

/// Propagate a pupil field to a focal plane with a directly specified output sampling.
/** The equivalent of `prysm.propagation.focus_fixed_sampling` with
 * `method='mdft'`, except that the shift is given in output pixels rather than
 * output physical units. See the file header for why.
 *
 * \returns 0 on success
 * \returns -1 on an invalid geometry
 *
 * \ingroup wccCommon
 */
inline int focusFixedSampling( matrixDFT &engine /**< [in,out] DFT engine supplying the cached bases */,
                               const cmplxMatrixT &pupil /**< [in] complex pupil field */,
                               double inputDx /**< [in] pupil sample spacing [length] */,
                               double propDist /**< [in] focal length, same unit as inputDx */,
                               double wavelength /**< [in] wavelength [um] */,
                               double outputDx /**< [in] focal plane sample spacing [um] */,
                               int rowsOut /**< [in] focal plane rows */,
                               int colsOut /**< [in] focal plane columns */,
                               double shiftXPix /**< [in] focal plane shift along columns [output pixels] */,
                               double shiftYPix /**< [in] focal plane shift along rows [output pixels] */,
                               cmplxMatrixT &out /**< [out] focal plane complex field */ )
{
    const double dia = static_cast<double>( pupil.rows() ) * inputDx;
    const double q = qForSampling( dia, propDist, wavelength, outputDx );

    if( q == 0 )
    {
        return -1;
    }

    return engine.dft2( pupil, q, rowsOut, colsOut, shiftXPix, shiftYPix, out );
}

} // namespace wcc
} // namespace MagAOX

#endif // wccMDFT_hpp
