/** \file wccPSF.hpp
 * \brief Diffraction PSF generation and sub-pixel shifted PSF banks for the WCC simulator.
 * \author Adam Schilperoort
 *
 * This is the C++ equivalent of `get_psf()`, `get_telescope_optical_parameters()`
 * and `generate_telescope_data()` in `uasal_star_catalog_simulator_prysm.py`,
 * built on the matrix DFT port in wccMDFT.hpp.
 *
 * \par Why a bank instead of a PSF per star
 * The Python script calls `get_psf()` once per star to get the correct
 * sub-pixel placement, which is roughly two 512x512 by 512x512 complex matrix
 * products per star per wavelength. That is far too slow to run at a camera
 * frame rate. The only thing that changes from star to star is the sub-pixel
 * offset and the total flux, so psfBank precomputes the PSF on a quantized grid
 * of sub-pixel offsets once at startup. Placing a star then costs one lookup and
 * one scaled accumulation. A several-kHz pointing trail is histogrammed into
 * those same bins (`addTrailSample`), which is the dwell-map convolution of the
 * path with the PSF at the bank's native resolution: every tick contributes
 * flux, identical placements splat once. The residual placement error is bounded
 * by half a bank step, which subSteps controls, and psfGenerator is still
 * available for an exact per star computation when fidelity matters more than
 * rate.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccPSF_hpp
#define wccPSF_hpp

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "wccMDFT.hpp"
#include "wccUnits.hpp"

namespace MagAOX
{
namespace wcc
{

/// Geometric description of the telescope pupil.
/** \ingroup wccCommon
 */
struct pupilConfig
{
    /// Samples across the pupil grid, the `npix_pupil` of the Python simulator.
    int m_npix{ 256 };

    /// Clear aperture diameter [m]. From `telescope.optics.m1.aper_clear_OD`.
    double m_diameter{ 6.5 };

    /// Focal ratio, from `telescope.general.f_number`.
    double m_fNumber{ 12.0 };

    /// Central obscuration diameter as a fraction of the clear aperture. 0 disables it.
    double m_centralObscuration{ 0.0 };

    /// Number of spider vanes. 0 disables the spider.
    int m_spiderVanes{ 0 };

    /// Spider vane width [m].
    double m_spiderWidth{ 0.0 };

    /// Clockwise rotation of the spider pattern [deg].
    double m_spiderRotation{ 0.0 };
};

/// Sampled bandpass: the wavelengths a PSF is summed over and their relative weights.
/** The weights play the role of prysm's `Throughput` array, which the Python
 * simulator builds by interpolating mirror coating curves, the detector quantum
 * efficiency and the filter transmission onto its wavelength grid. Only the
 * shape matters here because total flux comes from the photometry.
 *
 * \ingroup wccCommon
 */
struct bandpassConfig
{
    /// Wavelength samples [nm]. A single entry gives a monochromatic PSF.
    std::vector<double> m_wavelengths{ 650.0 };

    /// Relative weight of each wavelength. Empty or mismatched means uniform weighting.
    std::vector<double> m_throughput{};

    /// Human readable filter name, carried through for logging and telemetry.
    std::string m_name{ "default" };
};

/// Broadband diffraction PSF generator.
/** Holds the pupil amplitude and optional optical path difference map, and
 * evaluates the incoherent sum over the bandpass at a requested focal plane
 * sampling and sub-pixel offset. The returned PSF always has unit sum, so the
 * caller multiplies by the photon count from photometry.
 *
 * This class is not thread safe: it owns a matrixDFT basis cache and scratch
 * buffers. Give each worker thread its own instance.
 *
 * \ingroup wccCommon
 */
class psfGenerator
{

    /** \name Pupil - Data
     *@{
     */
  protected:
    pupilConfig m_pupil; ///< Geometry the amplitude map was built from.

    bandpassConfig m_bandpass; ///< Wavelengths and weights the PSF is summed over.

    realMatrixT m_amplitude; ///< Pupil amplitude transmission, built by buildPupil().

    realMatrixT m_opd; ///< Optical path difference over the pupil [nm]. Empty means a flat wavefront.

    bool m_haveOPD{ false }; ///< True when m_opd is populated and matches m_amplitude in shape.
    ///@}

    /** \name Working State - Data
     *@{
     */
  protected:
    matrixDFT m_engine; ///< DFT basis cache, reused across calls.

    cmplxMatrixT m_field; ///< Scratch complex pupil field.

    cmplxMatrixT m_focal; ///< Scratch focal plane complex field.

    std::vector<double> m_accum; ///< Scratch accumulator for the incoherent wavelength sum.
    ///@}

  public:
    /// Build the pupil amplitude map from a geometric description.
    /** Applies the clear aperture, then the central obscuration, then the spider.
     *
     * \returns 0 on success
     * \returns -1 if the configuration is invalid
     */
    int buildPupil( const pupilConfig &cfg /**< [in] pupil geometry */ );

    /// Set the bandpass the PSF is summed over.
    /** \returns 0 on success
     * \returns -1 if the wavelength list is empty
     */
    int setBandpass( const bandpassConfig &cfg /**< [in] wavelengths and weights */ );

    /// Install an optical path difference map over the pupil.
    /** \returns 0 on success
     * \returns -1 if the map does not match the pupil grid, in which case the
     *          wavefront stays flat
     */
    int setOPD( const std::vector<double> &opd /**< [in] OPD [nm], row major, npix by npix */ );

    /// Remove any installed OPD map, returning to a flat wavefront.
    void clearOPD();

    /// Pupil sample spacing implied by the geometry [m].
    double pupilDx() const;

    /// Effective focal length implied by the geometry [m].
    double efl() const;

    /// The pupil configuration in use.
    const pupilConfig &pupil() const;

    /// The bandpass in use.
    const bandpassConfig &bandpass() const;

    /// Diffraction limited resolution element at the mean wavelength [detector pixels].
    /** This is Q at the given detector sampling, so values below 2 mean the PSF
     * core is undersampled by the detector.
     *
     * \returns the number of detector pixels per resolution element
     */
    double resolutionElementPixels( double pixelSize /**< [in] detector pixel pitch [um] */ ) const;

    /// Compute the broadband PSF at a requested detector sampling and sub-pixel offset.
    /** The result is normalized to unit sum and stored row major.
     *
     * \returns 0 on success
     * \returns -1 if the pupil is not built or the propagation fails
     */
    int psf( std::vector<float> &out /**< [out] PSF, samples by samples, row major, unit sum */,
             int samples /**< [in] output stamp size on a side [pixels] */,
             double pixelSize /**< [in] detector pixel pitch [um] */,
             double shiftXPix = 0 /**< [in] offset along columns [detector pixels] */,
             double shiftYPix = 0 /**< [in] offset along rows [detector pixels] */ );

    /// Number of cached DFT basis pairs, for diagnostics.
    size_t dftCacheSize() const;

    /// Heap size of the cached DFT basis matrices [bytes], for diagnostics.
    size_t dftCacheBytes() const;
};

inline int psfGenerator::buildPupil( const pupilConfig &cfg )
{
    if( cfg.m_npix < 8 || cfg.m_diameter <= 0 || cfg.m_fNumber <= 0 )
    {
        return -1;
    }

    m_pupil = cfg;

    realMatrixT x, y, r, t;
    makeXYGrid( cfg.m_npix, cfg.m_diameter, x, y );
    cartToPolar( x, y, r, t );

    circleMask( 0.5 * cfg.m_diameter, r, m_amplitude );

    if( cfg.m_centralObscuration > 0 )
    {
        const double rObs = 0.5 * cfg.m_centralObscuration * cfg.m_diameter;

        for( Eigen::Index i = 0; i < m_amplitude.rows(); ++i )
        {
            for( Eigen::Index j = 0; j < m_amplitude.cols(); ++j )
            {
                if( r( i, j ) <= rObs )
                {
                    m_amplitude( i, j ) = 0.0;
                }
            }
        }
    }

    if( cfg.m_spiderVanes > 0 && cfg.m_spiderWidth > 0 )
    {
        realMatrixT sp;
        spiderMask( cfg.m_spiderVanes, cfg.m_spiderWidth, x, y, sp, cfg.m_spiderRotation );
        m_amplitude = m_amplitude.cwiseProduct( sp );
    }

    // A pupil grid change invalidates any OPD map and every cached basis.
    m_haveOPD = false;
    m_opd.resize( 0, 0 );
    m_engine.clear();

    return 0;
}

inline int psfGenerator::setBandpass( const bandpassConfig &cfg )
{
    if( cfg.m_wavelengths.empty() )
    {
        return -1;
    }

    m_bandpass = cfg;

    if( m_bandpass.m_throughput.size() != m_bandpass.m_wavelengths.size() )
    {
        m_bandpass.m_throughput.assign( m_bandpass.m_wavelengths.size(), 1.0 );
    }

    return 0;
}

inline int psfGenerator::setOPD( const std::vector<double> &opd )
{
    const size_t npix = static_cast<size_t>( m_amplitude.rows() ) * static_cast<size_t>( m_amplitude.cols() );

    if( npix == 0 || opd.size() != npix )
    {
        m_haveOPD = false;
        return -1;
    }

    m_opd.resize( m_amplitude.rows(), m_amplitude.cols() );

    for( Eigen::Index i = 0; i < m_amplitude.rows(); ++i )
    {
        for( Eigen::Index j = 0; j < m_amplitude.cols(); ++j )
        {
            m_opd( i, j ) = opd[static_cast<size_t>( i ) * static_cast<size_t>( m_amplitude.cols() ) +
                                static_cast<size_t>( j )];
        }
    }

    m_haveOPD = true;

    return 0;
}

inline void psfGenerator::clearOPD()
{
    m_haveOPD = false;
    m_opd.resize( 0, 0 );
}

inline double psfGenerator::pupilDx() const
{
    if( m_pupil.m_npix < 1 )
    {
        return 0;
    }

    return m_pupil.m_diameter / m_pupil.m_npix;
}

inline double psfGenerator::efl() const
{
    return m_pupil.m_fNumber * m_pupil.m_diameter;
}

inline const pupilConfig &psfGenerator::pupil() const
{
    return m_pupil;
}

inline const bandpassConfig &psfGenerator::bandpass() const
{
    return m_bandpass;
}

inline double psfGenerator::resolutionElementPixels( double pixelSize ) const
{
    if( m_bandpass.m_wavelengths.empty() )
    {
        return 0;
    }

    double sum = 0;
    for( double w : m_bandpass.m_wavelengths )
    {
        sum += w;
    }

    const double meanNm = sum / m_bandpass.m_wavelengths.size();

    return qForFNumber( meanNm * 1e-3, m_pupil.m_fNumber, pixelSize );
}

inline int psfGenerator::psf( std::vector<float> &out,
                              int samples,
                              double pixelSize,
                              double shiftXPix,
                              double shiftYPix )
{
    if( m_amplitude.rows() < 1 || samples < 1 || pixelSize <= 0 || m_bandpass.m_wavelengths.empty() )
    {
        return -1;
    }

    const size_t npix = static_cast<size_t>( samples ) * static_cast<size_t>( samples );

    m_accum.assign( npix, 0.0 );

    // prysm works in mm for the pupil and mm for the focal length. The pupil
    // here is described in metres, and only the ratio efl/diameter enters Q, so
    // the scale factor cancels. Passing metres for both is therefore correct.
    const double dx = pupilDx();
    const double focal = efl();

    for( size_t w = 0; w < m_bandpass.m_wavelengths.size(); ++w )
    {
        const double wvlUm = m_bandpass.m_wavelengths[w] * 1e-3;

        if( wvlUm <= 0 )
        {
            return -1;
        }

        wavefrontFromAmpAndPhase( m_amplitude, m_haveOPD ? &m_opd : nullptr, wvlUm, m_field );

        if( focusFixedSampling( m_engine,
                                m_field,
                                dx,
                                focal,
                                wvlUm,
                                pixelSize,
                                samples,
                                samples,
                                shiftXPix,
                                shiftYPix,
                                m_focal ) < 0 )
        {
            return -1;
        }

        // Intensity, then normalize this wavelength to unit flux before weighting,
        // matching the `flux /= np.sum(flux)` in the Python get_psf().
        double sum = 0;
        for( Eigen::Index i = 0; i < m_focal.rows(); ++i )
        {
            for( Eigen::Index j = 0; j < m_focal.cols(); ++j )
            {
                sum += std::norm( m_focal( i, j ) );
            }
        }

        if( !( sum > 0 ) )
        {
            return -1;
        }

        const double weight = m_bandpass.m_throughput[w] / sum;

        for( Eigen::Index i = 0; i < m_focal.rows(); ++i )
        {
            for( Eigen::Index j = 0; j < m_focal.cols(); ++j )
            {
                m_accum[static_cast<size_t>( i ) * static_cast<size_t>( samples ) + static_cast<size_t>( j )] +=
                    weight * std::norm( m_focal( i, j ) );
            }
        }
    }

    double total = 0;
    for( double v : m_accum )
    {
        total += v;
    }

    if( !( total > 0 ) )
    {
        return -1;
    }

    out.resize( npix );
    for( size_t i = 0; i < npix; ++i )
    {
        out[i] = static_cast<float>( m_accum[i] / total );
    }

    return 0;
}

inline size_t psfGenerator::dftCacheSize() const
{
    return m_engine.cacheSize();
}

inline size_t psfGenerator::dftCacheBytes() const
{
    return m_engine.nbytes();
}

/// A bank of PSFs precomputed on a quantized grid of sub-pixel offsets.
/** Built once at startup, then queried per star. See the file header for the
 * rationale. Once built the bank is immutable, so it is safe to share a single
 * const bank across all of the simulator worker threads.
 *
 * \ingroup wccCommon
 */
class psfBank
{

    /** \name Bank Geometry - Data
     *@{
     */
  protected:
    int m_samples{ 0 }; ///< Stamp size on a side [pixels].

    int m_subSteps{ 0 }; ///< Sub-pixel offset bins per axis.

    double m_pixelSize{ 0 }; ///< Detector pixel pitch the bank was built for [um].

    /// One unit sum PSF stamp per (row bin, column bin), indexed binY*subSteps + binX.
    std::vector<std::vector<float>> m_stamps;
    ///@}

  public:
    /// Build the bank by evaluating the generator over the sub-pixel offset grid.
    /** \returns 0 on success
     * \returns -1 if a parameter is invalid or the generator fails
     */
    int build( psfGenerator &gen /**< [in,out] configured generator */,
               int samples /**< [in] stamp size on a side [pixels], should be even */,
               int subSteps /**< [in] sub-pixel bins per axis, 1 disables sub-pixel placement */,
               double pixelSize /**< [in] detector pixel pitch [um] */ );

    /// True once build() has succeeded.
    bool valid() const;

    /// Stamp size on a side [pixels].
    int samples() const;

    /// Sub-pixel bins per axis.
    int subSteps() const;

    /// Detector pixel pitch the bank was built for [um].
    double pixelSize() const;

    /// Total heap size of the stamps [bytes].
    size_t nbytes() const;

    /// Worst case sub-pixel placement error of the bank [pixels].
    double placementError() const;

    /// Look up the stamp nearest a requested sub-pixel offset.
    /** Offsets outside [-0.5, 0.5) are wrapped, so the caller may pass a raw
     * fractional part of either sign.
     *
     * \returns a pointer to samples*samples floats with unit sum
     * \returns nullptr if the bank is not built
     */
    const float *lookup( double fracX /**< [in] offset along columns [pixels] */,
                         double fracY /**< [in] offset along rows [pixels] */ ) const;

    /// Stamp at an explicit sub-pixel bin, without wrapping a fractional offset.
    /** \returns a pointer to samples*samples floats with unit sum
     * \returns nullptr if the bank is not built or the bin is out of range
     */
    const float *stamp( int binX /**< [in] column bin, in [0, subSteps) */,
                        int binY /**< [in] row bin, in [0, subSteps) */ ) const;

    /// Bin index for a fractional offset, exposed for testing and diagnostics.
    /** \returns the bin index in [0, subSteps)
     */
    int binFor( double frac /**< [in] fractional offset [pixels] */ ) const;

    /// The sub-pixel offset at the center of a bin [pixels].
    /** \returns the bin center offset, in [-0.5, 0.5)
     */
    double binCenter( int bin /**< [in] bin index */ ) const;
};

inline int psfBank::build( psfGenerator &gen, int samples, int subSteps, double pixelSize )
{
    if( samples < 4 || subSteps < 1 || pixelSize <= 0 )
    {
        return -1;
    }

    m_samples = samples;
    m_subSteps = subSteps;
    m_pixelSize = pixelSize;
    m_stamps.assign( static_cast<size_t>( subSteps ) * static_cast<size_t>( subSteps ), std::vector<float>() );

    for( int by = 0; by < subSteps; ++by )
    {
        for( int bx = 0; bx < subSteps; ++bx )
        {
            const size_t idx = static_cast<size_t>( by ) * static_cast<size_t>( subSteps ) +
                               static_cast<size_t>( bx );

            if( gen.psf( m_stamps[idx], samples, pixelSize, binCenter( bx ), binCenter( by ) ) < 0 )
            {
                m_stamps.clear();
                m_samples = 0;
                m_subSteps = 0;
                return -1;
            }
        }
    }

    return 0;
}

inline bool psfBank::valid() const
{
    return m_samples > 0 && m_subSteps > 0 && !m_stamps.empty();
}

inline int psfBank::samples() const
{
    return m_samples;
}

inline int psfBank::subSteps() const
{
    return m_subSteps;
}

inline double psfBank::pixelSize() const
{
    return m_pixelSize;
}

inline size_t psfBank::nbytes() const
{
    size_t total = 0;

    for( const std::vector<float> &s : m_stamps )
    {
        total += s.size() * sizeof( float );
    }

    return total;
}

inline double psfBank::placementError() const
{
    if( m_subSteps < 1 )
    {
        return 0;
    }

    return 0.5 / m_subSteps;
}

inline int psfBank::binFor( double frac ) const
{
    if( m_subSteps < 1 )
    {
        return 0;
    }

    // Wrap into [0,1), then bin. std::floor handles negative offsets correctly.
    double f = frac + 0.5;
    f -= std::floor( f );

    int b = static_cast<int>( f * m_subSteps );

    if( b < 0 )
    {
        b = 0;
    }
    if( b >= m_subSteps )
    {
        b = m_subSteps - 1;
    }

    return b;
}

inline double psfBank::binCenter( int bin ) const
{
    if( m_subSteps < 1 )
    {
        return 0;
    }

    return ( bin + 0.5 ) / m_subSteps - 0.5;
}

inline const float *psfBank::lookup( double fracX, double fracY ) const
{
    if( !valid() )
    {
        return nullptr;
    }

    const size_t idx = static_cast<size_t>( binFor( fracY ) ) * static_cast<size_t>( m_subSteps ) +
                       static_cast<size_t>( binFor( fracX ) );

    if( idx >= m_stamps.size() || m_stamps[idx].empty() )
    {
        return nullptr;
    }

    return m_stamps[idx].data();
}

inline const float *psfBank::stamp( int binX, int binY ) const
{
    if( !valid() || binX < 0 || binY < 0 || binX >= m_subSteps || binY >= m_subSteps )
    {
        return nullptr;
    }

    const size_t idx = static_cast<size_t>( binY ) * static_cast<size_t>( m_subSteps ) +
                       static_cast<size_t>( binX );

    if( idx >= m_stamps.size() || m_stamps[idx].empty() )
    {
        return nullptr;
    }

    return m_stamps[idx].data();
}

} // namespace wcc
} // namespace MagAOX

#endif // wccPSF_hpp
