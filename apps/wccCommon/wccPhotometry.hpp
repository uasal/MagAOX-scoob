/** \file wccPhotometry.hpp
 * \brief Conversion of catalog magnitudes to detected photoelectrons.
 * \author Adam Schilperoort
 *
 * The C++ equivalent of the astropy units block in
 * `uasal_star_catalog_simulator_prysm.py`:
 *
 * \code
 * ph_counts = (mag * u.ABmag).to(u.photon / u.s / u.m**2 / u.micron,
 *                                equivalencies=u.spectral_density(6500 * u.AA))
 * ph_counts = ph_counts * bandwidth * aperture_area * exp_time * throughput
 * \endcode
 *
 * \par Derivation
 * An AB magnitude is defined by
 * \f$ m = -2.5\log_{10} f_\nu - 48.60 \f$ with \f$ f_\nu \f$ in
 * erg s\f$^{-1}\f$ cm\f$^{-2}\f$ Hz\f$^{-1}\f$, so
 * \f$ f_\nu = 10^{-0.4(m + 48.60)} \f$. Converting to a photon rate per unit
 * wavelength, \f$ f_\lambda = f_\nu c/\lambda^2 \f$ and each photon carries
 * \f$ hc/\lambda \f$, therefore
 * \f[ N_\lambda = \frac{f_\lambda}{hc/\lambda} = \frac{f_\nu}{h\lambda}. \f]
 * The speed of light cancels, which is why only the Planck constant appears
 * below.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccPhotometry_hpp
#define wccPhotometry_hpp

#include <cmath>

#include "wccUnits.hpp"

namespace MagAOX
{
namespace wcc
{

/// Everything needed to turn a catalog magnitude into a photoelectron count.
/** \ingroup wccCommon
 */
struct photometryConfig
{
    /// Pivot wavelength the AB magnitude is evaluated at [nm]. The Python simulator uses 650 nm.
    double m_pivotWavelength{ 650.0 };

    /// Bandpass width [um]. The Python simulator uses 0.150.
    double m_bandwidth{ 0.150 };

    /// Collecting area [m^2]. Set through apertureArea() or computed from the pupil diameter.
    double m_apertureArea{ 0.0 };

    /// Total optical throughput, dimensionless. The Python simulator hard codes 0.5.
    double m_throughput{ 0.5 };

    /// Detector quantum efficiency, electrons per photon.
    /** The Python simulator folds QE into its throughput and then tags the
     * result as electrons, so 1.0 reproduces it exactly.
     */
    double m_quantumEfficiency{ 1.0 };
};

/// Unobscured collecting area of a circular aperture.
/** \returns the area [m^2]
 *
 * \ingroup wccCommon
 */
inline double apertureArea( double diameter /**< [in] aperture diameter [m] */,
                            double centralObscuration = 0 /**< [in] obscuration as a fraction of the diameter */ )
{
    if( diameter <= 0 )
    {
        return 0;
    }

    const double outer = 0.25 * pi * diameter * diameter;

    if( centralObscuration <= 0 || centralObscuration >= 1 )
    {
        return outer;
    }

    return outer * ( 1.0 - centralObscuration * centralObscuration );
}

/// Photon flux density of an AB magnitude source at a wavelength.
/** \returns the flux density [photons s^-1 m^-2 um^-1]
 *
 * \ingroup wccCommon
 */
inline double abMagToPhotonFluxDensity( double mag /**< [in] AB magnitude */,
                                        double wavelength /**< [in] wavelength [nm] */ )
{
    if( wavelength <= 0 )
    {
        return 0;
    }

    // f_nu in erg s^-1 cm^-2 Hz^-1, then in W m^-2 Hz^-1.
    const double fNu = std::pow( 10.0, -0.4 * ( mag + abZeroPoint ) ) * ergPerCm2ToWPerM2;

    // N_lambda = f_nu / (h * lambda), with lambda in metres, giving
    // photons s^-1 m^-2 m^-1. The trailing 1e-6 converts to per micron.
    return fNu / ( planckH * wavelength * 1e-9 ) * 1e-6;
}

/// Photoelectrons accumulated from an AB magnitude source over an exposure.
/** This is the full chain the Python simulator applies to each catalog star,
 * and is the number the unit sum PSF stamp is scaled by.
 *
 * \returns the accumulated signal [electrons]
 *
 * \ingroup wccCommon
 */
inline double abMagToElectrons( double mag /**< [in] AB magnitude */,
                                double expTime /**< [in] exposure time [s] */,
                                const photometryConfig &cfg /**< [in] bandpass and telescope parameters */ )
{
    if( expTime <= 0 || cfg.m_apertureArea <= 0 || cfg.m_bandwidth <= 0 )
    {
        return 0;
    }

    return abMagToPhotonFluxDensity( mag, cfg.m_pivotWavelength ) * cfg.m_bandwidth * cfg.m_apertureArea * expTime *
           cfg.m_throughput * cfg.m_quantumEfficiency;
}

/// Faintest magnitude that still reaches a requested signal in an exposure.
/** Useful for trimming a catalog to the stars a given camera configuration can
 * actually detect, which keeps the per frame star loop short.
 *
 * \returns the limiting AB magnitude
 * \returns 99 if the configuration cannot produce signal
 *
 * \ingroup wccCommon
 */
inline double limitingMagnitude( double electrons /**< [in] signal threshold [electrons] */,
                                 double expTime /**< [in] exposure time [s] */,
                                 const photometryConfig &cfg /**< [in] bandpass and telescope parameters */ )
{
    if( electrons <= 0 )
    {
        return 99.0;
    }

    // Signal at magnitude zero, then invert the 10^(-0.4 m) scaling.
    const double zeroMagSignal = abMagToElectrons( 0.0, expTime, cfg );

    if( !( zeroMagSignal > 0 ) )
    {
        return 99.0;
    }

    return 2.5 * std::log10( zeroMagSignal / electrons );
}

} // namespace wcc
} // namespace MagAOX

#endif // wccPhotometry_hpp
