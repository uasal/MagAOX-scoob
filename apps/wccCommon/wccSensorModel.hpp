/** \file wccSensorModel.hpp
 * \brief Star stamp accumulation, detector noise and digitization for the WCC simulator.
 * \author Adam Schilperoort
 *
 * The C++ equivalent of the star placement and noise block of
 * `uasal_star_catalog_simulator_prysm.py`:
 *
 * \code
 * im_fits_2[start_x:start_x+n, start_y:start_y+n] += psf
 * noisy1 = np.random.poisson(im_fits)
 * noisy2 = noisy1 + np.random.normal(loc=read_noise, scale=read_noise/5, size=...)
 * noisy3 = noisy2 + dark_current * exp_time
 * \endcode
 *
 * Two behaviours differ deliberately from the Python:
 *
 * - The Python centers the read noise *on* the read noise value, which adds a
 *   spurious pedestal equal to sigma and gives a distribution five times
 *   narrower than the stated sigma. Here read noise is zero mean with the
 *   configured sigma, and any pedestal is the explicit bias level.
 * - The Python adds the noisy frame back onto the noiseless one
 *   (`im_fits + noisy3`), double counting the signal. Here the signal is counted
 *   once.
 *
 * \par Performance
 * A full frame IMX sensor is 61 megapixels, so per pixel work matters. Poisson
 * noise is only evaluated on pixels a star stamp actually touched, tracked by
 * bounding box, and the Gaussian read noise generator is a xoshiro256+ RNG with
 * a two-at-a-time polar Box-Muller. noiseMode lets an operator trade fidelity
 * against frame rate.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccSensorModel_hpp
#define wccSensorModel_hpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "wccFocalPlane.hpp"

namespace MagAOX
{
namespace wcc
{

/// How much of the detector noise model to evaluate.
/** \ingroup wccCommon
 */
enum class noiseMode
{
    off,  ///< Signal plus bias and dark pedestal only. Fastest, fully repeatable.
    read, ///< Adds zero mean Gaussian read noise over the whole frame.
    full  ///< Adds Poisson shot noise on illuminated pixels as well.
};

/// Parse a noise mode name from configuration.
/** Unrecognized names fall back to full.
 *
 * \returns the parsed mode
 *
 * \ingroup wccCommon
 */
inline noiseMode parseNoiseMode( const std::string &s /**< [in] one of off, read, full */ )
{
    if( s == "off" || s == "none" )
    {
        return noiseMode::off;
    }

    if( s == "read" )
    {
        return noiseMode::read;
    }

    return noiseMode::full;
}

/// Name of a noise mode, for logging and INDI.
/** \returns the mode name
 *
 * \ingroup wccCommon
 */
inline std::string noiseModeName( noiseMode m /**< [in] the mode */ )
{
    switch( m )
    {
    case noiseMode::off:
        return "off";
    case noiseMode::read:
        return "read";
    default:
        return "full";
    }
}

/// Default IMX455-style analog gain register limits and step.
/** The camera `emgain` property is a code 0 to 255. Each step is 0.1 dB, and
 * the linear electron multiplier follows the Sony relation
 * \f$ G_{\mathrm{dB}} = 20\log_{10} G \f$, so both signal and the noise already
 * present in electrons are multiplied.
 *
 * \ingroup wccCommon
 */
constexpr int analogGainCodeMin = 0;

/// Inclusive maximum analog gain code for an 8-bit IMX-style register.
/** \ingroup wccCommon
 */
constexpr int analogGainCodeMax = 255;

/// Analog gain register step [dB per code] for IMX455.
/** \ingroup wccCommon
 */
constexpr double analogGainStepDb = 0.1;

/// Clamp a camera gain code into a valid analog-gain register value.
/** \returns the clamped code
 *
 * \ingroup wccCommon
 */
inline int clampAnalogGainCode( double v /**< [in] raw code, often from INDI emgain */,
                                int lo = analogGainCodeMin /**< [in] inclusive minimum */,
                                int hi = analogGainCodeMax /**< [in] inclusive maximum */ )
{
    int c = static_cast<int>( std::lround( v ) );

    if( lo > hi )
    {
        std::swap( lo, hi );
    }

    return std::max( lo, std::min( hi, c ) );
}

/// Analog gain in decibels for a register code.
/** \returns code times the step, never negative
 *
 * \ingroup wccCommon
 */
inline double analogGainDecibels( int code /**< [in] analog gain register code */,
                                  double stepDb = analogGainStepDb /**< [in] dB per code */ )
{
    if( code < 0 )
    {
        code = 0;
    }

    if( !( stepDb > 0 ) )
    {
        stepDb = analogGainStepDb;
    }

    return static_cast<double>( code ) * stepDb;
}

/// Linear electron multiplier for an IMX-style analog gain code.
/** \returns \f$ 10^{0.1\,\mathrm{code}/20} \f$ at the default 0.1 dB step
 *
 * \ingroup wccCommon
 */
inline double analogGainLinear( int code /**< [in] analog gain register code */,
                                double stepDb = analogGainStepDb /**< [in] dB per code */ )
{
    return std::pow( 10.0, analogGainDecibels( code, stepDb ) / 20.0 );
}

/// xoshiro256+ pseudo random generator with Gaussian and Poisson draws.
/** Chosen over std::mt19937 because a full frame needs tens of millions of
 * draws per frame. Each simulator worker thread owns one instance, so no
 * synchronization is needed and each sensor's noise is independent and
 * reproducible from its seed.
 *
 * \ingroup wccCommon
 */
class fastRandom
{
    /** \name Generator State - Data
     *@{
     */
  protected:
    uint64_t m_s[4]{ 0x9E3779B97F4A7C15ULL, 0xBF58476D1CE4E5B9ULL, 0x94D049BB133111EBULL,
                     0x2545F4914F6CDD1DULL }; ///< xoshiro256+ state.

    double m_spare{ 0 }; ///< Second Gaussian from the last polar Box-Muller pair.

    bool m_haveSpare{ false }; ///< True when m_spare holds an unused Gaussian.
    ///@}

  public:
    /// Construct with a seed.
    explicit fastRandom( uint64_t seed = 1 /**< [in] seed, mixed through splitmix64 */ );

    /// Reseed the generator, discarding any buffered Gaussian.
    void seed( uint64_t s /**< [in] seed, mixed through splitmix64 */ );

    /// Next raw 64 bit value.
    uint64_t next();

    /// Uniform double in [0,1).
    double uniform();

    /// Standard normal draw, mean 0 and sigma 1.
    double normal();

    /// Poisson draw.
    /** Uses Knuth's product method below 30, where it is cheap and exact, and a
     * normal approximation above, where the relative error of the approximation
     * is under a percent and Knuth would loop tens of times.
     *
     * \returns the draw
     */
    double poisson( double lambda /**< [in] mean */ );
};

inline fastRandom::fastRandom( uint64_t s )
{
    seed( s );
}

inline void fastRandom::seed( uint64_t s )
{
    // splitmix64 to spread a small seed over the whole state.
    for( int i = 0; i < 4; ++i )
    {
        s += 0x9E3779B97F4A7C15ULL;
        uint64_t z = s;
        z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ULL;
        z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBULL;
        m_s[i] = z ^ ( z >> 31 );
    }

    m_haveSpare = false;
}

inline uint64_t fastRandom::next()
{
    const uint64_t result = m_s[0] + m_s[3];
    const uint64_t t = m_s[1] << 17;

    m_s[2] ^= m_s[0];
    m_s[3] ^= m_s[1];
    m_s[1] ^= m_s[2];
    m_s[0] ^= m_s[3];
    m_s[2] ^= t;
    m_s[3] = ( m_s[3] << 45 ) | ( m_s[3] >> 19 );

    return result;
}

inline double fastRandom::uniform()
{
    // Top 53 bits give a uniform double with full mantissa resolution.
    return ( next() >> 11 ) * 0x1.0p-53;
}

inline double fastRandom::normal()
{
    if( m_haveSpare )
    {
        m_haveSpare = false;
        return m_spare;
    }

    double u, v, s;
    do
    {
        u = 2.0 * uniform() - 1.0;
        v = 2.0 * uniform() - 1.0;
        s = u * u + v * v;
    } while( s >= 1.0 || s == 0.0 );

    const double f = std::sqrt( -2.0 * std::log( s ) / s );

    m_spare = v * f;
    m_haveSpare = true;

    return u * f;
}

inline double fastRandom::poisson( double lambda )
{
    if( !( lambda > 0 ) )
    {
        return 0;
    }

    if( lambda < 30.0 )
    {
        const double limit = std::exp( -lambda );
        double p = uniform();
        int k = 0;

        while( p > limit && k < 1000 )
        {
            p *= uniform();
            ++k;
        }

        return k;
    }

    const double v = lambda + std::sqrt( lambda ) * normal();

    return ( v < 0 ) ? 0.0 : v;
}

/// Accumulate a unit sum PSF stamp into a frame, scaled by a flux.
/** The stamp is placed with its FFT origin, index `samples/2`, on the integer
 * pixel `(ix, iy)`; the sub-pixel part of the position is already baked into the
 * stamp by psfBank. Portions falling outside the frame are clipped, so stars
 * just off the edge still contribute their wings, which is what the Python
 * simulator achieves with an oversized array and a crop.
 *
 * The touched bounding box is unioned into the four box arguments so that the
 * caller can restrict Poisson noise to illuminated pixels.
 *
 * \returns the accumulated flux that actually landed inside the frame [electrons]
 *
 * \ingroup wccCommon
 */
inline double accumulateStamp( std::vector<float> &frame /**< [in,out] frame, row major, w by h */,
                               int w /**< [in] frame width [pixels] */,
                               int h /**< [in] frame height [pixels] */,
                               const float *stamp /**< [in] unit sum PSF stamp */,
                               int samples /**< [in] stamp size on a side [pixels] */,
                               int ix /**< [in] frame column the stamp origin lands on */,
                               int iy /**< [in] frame row the stamp origin lands on */,
                               double flux /**< [in] total signal to distribute [electrons] */,
                               int &bx0 /**< [in,out] bounding box min column */,
                               int &by0 /**< [in,out] bounding box min row */,
                               int &bx1 /**< [in,out] bounding box max column, inclusive */,
                               int &by1 /**< [in,out] bounding box max row, inclusive */ )
{
    if( stamp == nullptr || samples < 1 || w < 1 || h < 1 || !( flux > 0 ) )
    {
        return 0;
    }

    const int half = samples / 2;

    // Stamp index s maps to frame index ix + (s - half).
    const int sx0 = std::max( 0, half - ix );
    const int sy0 = std::max( 0, half - iy );
    const int sx1 = std::min( samples, w - ix + half );
    const int sy1 = std::min( samples, h - iy + half );

    if( sx0 >= sx1 || sy0 >= sy1 )
    {
        return 0;
    }

    bx0 = std::min( bx0, ix + sx0 - half );
    by0 = std::min( by0, iy + sy0 - half );
    bx1 = std::max( bx1, ix + sx1 - 1 - half );
    by1 = std::max( by1, iy + sy1 - 1 - half );

    const float f = static_cast<float>( flux );
    double landed = 0;

    for( int sy = sy0; sy < sy1; ++sy )
    {
        const int fy = iy + sy - half;
        float *row = frame.data() + static_cast<size_t>( fy ) * static_cast<size_t>( w );
        const float *srow = stamp + static_cast<size_t>( sy ) * static_cast<size_t>( samples );

        for( int sx = sx0; sx < sx1; ++sx )
        {
            const float v = f * srow[sx];
            row[ix + sx - half] += v;
            landed += v;
        }
    }

    return landed;
}

/// Detector noise and digitization applied to an accumulated electron frame.
/** \ingroup wccCommon
 */
class sensorNoise
{
    /** \name Noise State - Data
     *@{
     */
  protected:
    fastRandom m_rng; ///< Generator for this sensor's noise, seeded per sensor.

    noiseMode m_mode{ noiseMode::full }; ///< How much of the model to evaluate.

    double m_bias{ 100.0 }; ///< Bias pedestal added to every pixel [electrons].
    ///@}

  public:
    /// Construct with a seed.
    explicit sensorNoise( uint64_t seed = 1 /**< [in] RNG seed */ );

    /// Set the noise mode.
    void mode( noiseMode m /**< [in] the mode */ );

    /// The noise mode in use.
    noiseMode mode() const;

    /// Set the bias pedestal [electrons].
    void bias( double b /**< [in] pedestal */ );

    /// The bias pedestal [electrons].
    double bias() const;

    /// Reseed the generator.
    void seed( uint64_t s /**< [in] seed */ );

    /// Apply the noise model in place to a frame of accumulated electrons.
    /** Poisson noise is applied only inside the supplied bounding box, which the
     * caller accumulates from accumulateStamp(). Dark current is added as a
     * pedestal and, in full mode, is itself shot noise limited.
     */
    void apply( std::vector<float> &frame /**< [in,out] electrons, row major, w by h */,
                int w /**< [in] frame width [pixels] */,
                int h /**< [in] frame height [pixels] */,
                const sensorConfig &sensor /**< [in] detector parameters */,
                double expTime /**< [in] exposure time [s] */,
                int bx0 /**< [in] illuminated bounding box min column */,
                int by0 /**< [in] illuminated bounding box min row */,
                int bx1 /**< [in] illuminated bounding box max column, inclusive */,
                int by1 /**< [in] illuminated bounding box max row, inclusive */ );

    /// Convert an electron frame to digital numbers, clipping at full well and the ADC range.
    /** Analog gain multiplies the electron image — signal, shot noise, dark and
     * read noise — the way a CMOS PGA does. Conversion gain then maps the
     * amplified charge to DN. Full well is applied in electrons before the PGA;
     * the ADC clips after it.
     *
     * \returns the number of pixels that saturated
     */
    size_t digitize( const std::vector<float> &frame /**< [in] electrons, row major */,
                     std::vector<uint16_t> &out /**< [out] digital numbers, sized to frame */,
                     const sensorConfig &sensor /**< [in] detector parameters */,
                     double analogGain /**< [in] linear electron multiplier; values at or below 0 mean unity */,
                     int bitDepth /**< [in] ADC bit depth, 8 to 16 */ );
};

inline sensorNoise::sensorNoise( uint64_t s ) : m_rng( s )
{
}

inline void sensorNoise::mode( noiseMode m )
{
    m_mode = m;
}

inline noiseMode sensorNoise::mode() const
{
    return m_mode;
}

inline void sensorNoise::bias( double b )
{
    m_bias = b;
}

inline double sensorNoise::bias() const
{
    return m_bias;
}

inline void sensorNoise::seed( uint64_t s )
{
    m_rng.seed( s );
}

inline void sensorNoise::apply( std::vector<float> &frame,
                                int w,
                                int h,
                                const sensorConfig &sensor,
                                double expTime,
                                int bx0,
                                int by0,
                                int bx1,
                                int by1 )
{
    if( w < 1 || h < 1 || frame.size() < static_cast<size_t>( w ) * static_cast<size_t>( h ) )
    {
        return;
    }

    const double dark = std::max( 0.0, sensor.m_darkCurrent * std::max( 0.0, expTime ) );

    // Shot noise on the illuminated pixels only. Dark current is folded into the
    // Poisson mean there so that its own shot noise is captured too.
    if( m_mode == noiseMode::full )
    {
        const int x0 = std::max( 0, bx0 );
        const int y0 = std::max( 0, by0 );
        const int x1 = std::min( w - 1, bx1 );
        const int y1 = std::min( h - 1, by1 );

        for( int y = y0; y <= y1; ++y )
        {
            float *row = frame.data() + static_cast<size_t>( y ) * static_cast<size_t>( w );

            for( int x = x0; x <= x1; ++x )
            {
                row[x] = static_cast<float>( m_rng.poisson( row[x] + dark ) - dark );
            }
        }
    }

    // Pedestals and read noise over the whole frame.
    const double pedestal = m_bias + dark;
    const double rn = std::max( 0.0, sensor.m_readNoise );

    if( m_mode == noiseMode::off || rn == 0 )
    {
        for( int y = 0; y < h; ++y )
        {
            float *row = frame.data() + static_cast<size_t>( y ) * static_cast<size_t>( w );

            for( int x = 0; x < w; ++x )
            {
                row[x] += static_cast<float>( pedestal );
            }
        }

        return;
    }

    for( int y = 0; y < h; ++y )
    {
        float *row = frame.data() + static_cast<size_t>( y ) * static_cast<size_t>( w );

        for( int x = 0; x < w; ++x )
        {
            row[x] += static_cast<float>( pedestal + rn * m_rng.normal() );
        }
    }
}

inline size_t sensorNoise::digitize( const std::vector<float> &frame,
                                     std::vector<uint16_t> &out,
                                     const sensorConfig &sensor,
                                     double analogGain,
                                     int bitDepth )
{
    const int bits = std::min( 16, std::max( 8, bitDepth ) );
    const double adcMax = ( bits >= 16 ) ? 65535.0 : static_cast<double>( ( 1u << bits ) - 1u );
    const double wellMax = ( sensor.m_fullWellDepth > 0 ) ? sensor.m_fullWellDepth : adcMax;
    const double g = ( analogGain > 0 ) ? analogGain : 1.0;
    const double conv = ( sensor.m_conversionGain > 0 ) ? sensor.m_conversionGain : 1.0;

    out.resize( frame.size() );

    size_t saturated = 0;

    for( size_t i = 0; i < frame.size(); ++i )
    {
        double e = frame[i];

        if( e > wellMax )
        {
            e = wellMax;
            ++saturated;
        }

        // PGA multiplies the electron image, including the noise already in it.
        double dn = ( e * g ) / conv;

        if( dn <= 0 )
        {
            out[i] = 0;
            continue;
        }

        if( dn >= adcMax )
        {
            out[i] = static_cast<uint16_t>( adcMax );
            ++saturated;
            continue;
        }

        out[i] = static_cast<uint16_t>( dn + 0.5 );
    }

    return saturated;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccSensorModel_hpp
