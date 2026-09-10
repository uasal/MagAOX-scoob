/** \file wccUnits.hpp
 * \brief Physical constants and unit conversions shared by the WCC applications.
 * \author Adam Schilperoort
 *
 * These replace the `astropy.units` and `astropy.constants` machinery used by
 * `uasal_star_catalog_simulator_prysm.py`. Values are the IAU/CODATA numbers
 * astropy itself uses, so photometry computed here matches the Python simulator
 * to round off.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccUnits_hpp
#define wccUnits_hpp

namespace MagAOX
{
namespace wcc
{

/// Ratio of a circle's circumference to its diameter.
constexpr double pi = 3.14159265358979323846264338328;

/// Degrees per radian.
constexpr double rad2deg = 57.295779513082320876798154814105;

/// Radians per degree.
constexpr double deg2rad = 0.017453292519943295769236907684886;

/// Arcseconds per radian, the classic 206264.806 conversion.
constexpr double rad2arcsec = 206264.80624709635515647335733078;

/// Radians per arcsecond.
constexpr double arcsec2rad = 4.8481368110953599358991410235795e-6;

/// Arcseconds per degree.
constexpr double deg2arcsec = 3600.0;

/// Degrees per arcsecond.
constexpr double arcsec2deg = 1.0 / 3600.0;

/// Planck constant [J s], CODATA 2018 exact value.
constexpr double planckH = 6.62607015e-34;

/// Speed of light in vacuum [m/s], exact by definition.
constexpr double lightC = 2.99792458e8;

/// AB magnitude zero point in the \f$ -2.5\log_{10}f_\nu - 48.60 \f$ convention.
/** \f$ f_\nu \f$ is in erg s\f$^{-1}\f$ cm\f$^{-2}\f$ Hz\f$^{-1}\f$. This is the
 * definition behind `astropy.units.ABmag`.
 */
constexpr double abZeroPoint = 48.60;

/// Multiplier taking erg s^-1 cm^-2 Hz^-1 to W m^-2 Hz^-1.
/** 1e-7 for erg to joule, times 1e4 for cm^-2 to m^-2.
 */
constexpr double ergPerCm2ToWPerM2 = 1e-3;

} // namespace wcc
} // namespace MagAOX

#endif // wccUnits_hpp
