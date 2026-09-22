/** \file wccPointingShmim.hpp
 * \brief Shared ImageStreamIO contract for high-rate telescope pointing.
 * \author Adam Schilperoort
 *
 * INDI is held to 1 Hz, and the WCC simulators are not required to run in
 * wall-clock time: a camera may be commanded to 100 s exposures at 10 fps, and
 * the renderer may emit frames as fast as the CPU allows. Wall-clock timestamps
 * therefore cannot define an exposure. Each pointing slice is instead one tick of
 * **simulated** time, of duration `1/write_hz`, and an exposure of `T` seconds
 * integrates the most recent `round(T * write_hz)` ticks. Camera output streams
 * still publish at the camera's commanded frame rate; they just accumulate those
 * ticks.
 *
 * The stream is a 4 by 1 by N circular buffer of doubles:
 *
 * | index | axis     | unit                         |
 * |-------|----------|------------------------------|
 * | 0     | RA       | deg                          |
 * | 1     | Dec      | deg                          |
 * | 2     | PA       | deg                          |
 * | 3     | sim time | seconds, monotonically increasing from 0 |
 *
 * N is `write_hz * history_s`. The writer advances simulated time by `1/write_hz`
 * on every write and is not paced to wall-clock unless configured to be.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccPointingShmim_hpp
#define wccPointingShmim_hpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace MagAOX
{
namespace wcc
{

/// Number of values packed into each temporal slice.
/** \ingroup wccCommon
 */
constexpr uint32_t pointingNAxes = 4;

/// Slice column of right ascension [deg].
/** \ingroup wccCommon
 */
constexpr uint32_t pointingIxRA = 0;

/// Slice column of declination [deg].
/** \ingroup wccCommon
 */
constexpr uint32_t pointingIxDec = 1;

/// Slice column of position angle [deg].
/** \ingroup wccCommon
 */
constexpr uint32_t pointingIxPA = 2;

/// Slice column of simulated time [s], counted from writer start.
/** \ingroup wccCommon
 */
constexpr uint32_t pointingIxSimTime = 3;

/// Default ImageStreamIO name for the pointing stream.
/** \ingroup wccCommon
 */
constexpr const char *pointingShmimDefault = "telpointing";

/// Default pointing tick rate [Hz of simulated time].
/** \ingroup wccCommon
 */
constexpr double pointingWriteHzDefault = 5000.0;

/// Default circular-buffer span of simulated time [s].
/** Must cover the longest exposure a camera will integrate. 60 s at 5000 Hz is
 * 300 000 slices, about 10 MB of doubles.
 *
 * \ingroup wccCommon
 */
constexpr double pointingHistorySDefault = 120.0;

/// Default time-decimation cap. 0 keeps every pointing tick.
/** Spatial coalescing into PSF-bank cells is what makes a several-kHz trail
 * cheap, not this. A positive value is an optional debug throttle that averages
 * adjacent ticks in time before rendering.
 *
 * \ingroup wccCommon
 */
constexpr uint32_t pointingMaxSamplesDefault = 0;

/// One pointing sample, with the duration it should contribute to an exposure.
/** \ingroup wccCommon
 */
struct pointingSample
{
    double m_ra{ 0 }; ///< Right ascension [deg].

    double m_dec{ 0 }; ///< Declination [deg].

    double m_pa{ 0 }; ///< Position angle of focal plane +Y, east of north [deg].

    double m_time{ 0 }; ///< Simulated time of this tick [s].

    double m_dt{ 0 }; ///< Integration weight [s]. Summed over an exposure this equals the exposure time.
};

/// Number of pointing ticks that span an exposure at a tick rate.
/** \returns at least 1
 *
 * \ingroup wccCommon
 */
inline uint64_t ticksForExposure( double expTime /**< [in] exposure length [s of simulated time] */,
                                  double writeHz /**< [in] tick rate [Hz of simulated time] */ )
{
    if( !( expTime > 0 ) || !( writeHz > 0 ) )
    {
        return 1;
    }

    const long long n = std::llround( expTime * writeHz );

    return ( n < 1 ) ? 1 : static_cast<uint64_t>( n );
}

/// Number of circular-buffer slices needed to hold a span at a tick rate.
/** \returns at least 1
 *
 * \ingroup wccCommon
 */
inline uint32_t pointingBufferDepth( double writeHz /**< [in] tick rate [Hz] */,
                                     double historyS /**< [in] span to retain [s of simulated time] */ )
{
    const uint64_t n = ticksForExposure( historyS, writeHz );

    return ( n > 0xffffffffULL ) ? 0xffffffffu : static_cast<uint32_t>( n );
}

/// Pack a pointing tick into a slice of pointingNAxes doubles.
/** \ingroup wccCommon
 */
inline void packPointing( double *dst /**< [out] at least pointingNAxes doubles */,
                          double ra /**< [in] right ascension [deg] */,
                          double dec /**< [in] declination [deg] */,
                          double pa /**< [in] position angle [deg] */,
                          double simTime /**< [in] simulated time [s] */ )
{
    if( dst == nullptr )
    {
        return;
    }

    dst[pointingIxRA] = ra;
    dst[pointingIxDec] = dec;
    dst[pointingIxPA] = pa;
    dst[pointingIxSimTime] = simTime;
}

/// Unpack a pointing tick from a slice.
/** `nAxes` may be 3 (legacy RA/Dec/PA) or 4 (including simulated time).
 *
 * \ingroup wccCommon
 */
inline void unpackPointing( const double *src /**< [in] nAxes doubles */,
                            uint32_t nAxes /**< [in] values per slice */,
                            double &ra /**< [out] right ascension [deg] */,
                            double &dec /**< [out] declination [deg] */,
                            double &pa /**< [out] position angle [deg] */,
                            double &simTime /**< [out] simulated time [s], 0 if the slice has no sim-time axis */ )
{
    if( src == nullptr || nAxes < 3 )
    {
        ra = 0;
        dec = 0;
        pa = 0;
        simTime = 0;
        return;
    }

    ra = src[pointingIxRA];
    dec = src[pointingIxDec];
    pa = src[pointingIxPA];
    simTime = ( nAxes > pointingIxSimTime ) ? src[pointingIxSimTime] : 0;
}

/// Collect the most recent `nTicks` pointing samples and weight them equally.
/** Correlation is by sample count, not by the computer clock: an exposure of
 * `expTime` seconds is `ticksForExposure(expTime, writeHz)` ticks, each lasting
 * `expTime / nCollected` so the integrated flux always matches the commanded
 * exposure. If the buffer holds fewer ticks than that, every valid tick is used
 * rather than parking the PSF on the newest sample for the missing time.
 *
 * `cnt1` is the last-written slice (`md->cnt1`). `nWritten` is `md->cnt0`.
 * `nAxes` is `md->size[0]`.
 *
 * \returns the number of samples
 * \returns -1 if the buffer is empty
 *
 * \ingroup wccCommon
 */
inline int collectPointingTicks( const double *data /**< [in] ring of pointing slices */,
                                 uint32_t nAxes /**< [in] values per slice, 3 or 4 */,
                                 uint32_t depth /**< [in] circular buffer length */,
                                 uint64_t cnt1 /**< [in] last-written slice index */,
                                 uint64_t nWritten /**< [in] total writes so far */,
                                 uint64_t nTicks /**< [in] ticks that span the exposure */,
                                 double expTime /**< [in] commanded exposure [s], split equally across ticks */,
                                 std::vector<pointingSample> &out /**< [out] chronological samples */ )
{
    out.clear();

    if( data == nullptr || depth < 1 || nWritten == 0 || nAxes < 3 )
    {
        return -1;
    }

    const uint64_t nValid = std::min<uint64_t>( nWritten, depth );
    const uint64_t nUse = std::min( nTicks < 1 ? uint64_t( 1 ) : nTicks, nValid );

    if( nUse < 1 )
    {
        return -1;
    }

    const double dt = ( expTime > 0 ) ? expTime / static_cast<double>( nUse )
                                      : 1.0 / static_cast<double>( nUse );

    out.resize( static_cast<size_t>( nUse ) );

    for( uint64_t i = 0; i < nUse; ++i )
    {
        // i = 0 is the oldest tick of the window, i = nUse-1 is cnt1.
        const uint64_t age = nUse - 1 - i;
        const uint32_t slice = static_cast<uint32_t>( ( cnt1 + depth - age ) % depth );
        const double *src = data + static_cast<size_t>( slice ) * nAxes;

        pointingSample &s = out[static_cast<size_t>( i )];
        unpackPointing( src, nAxes, s.m_ra, s.m_dec, s.m_pa, s.m_time );
        s.m_dt = dt;
    }

    return static_cast<int>( nUse );
}

/// Collect the pointing ticks that cover an exposure of simulated time.
/** Prefers the sim-time axis: the newest slice's sim-time is "now", and every
 * earlier slice with sim-time in `[now - expTime, now]` is included. That is
 * independent of the computer clock and of how fast the writer actually ran.
 * A 3-axis stream (no sim-time) falls back to the last
 * `ticksForExposure(expTime, writeHz)` slices.
 *
 * \returns the number of samples
 * \returns -1 if the buffer is empty
 *
 * \ingroup wccCommon
 */
inline int collectPointingExposure( const double *data /**< [in] ring of pointing slices */,
                                    uint32_t nAxes /**< [in] values per slice, 3 or 4 */,
                                    uint32_t depth /**< [in] circular buffer length */,
                                    uint64_t cnt1 /**< [in] last-written slice index */,
                                    uint64_t nWritten /**< [in] total writes so far */,
                                    double expTime /**< [in] commanded exposure [s of simulated time] */,
                                    double writeHz /**< [in] tick rate, used only if there is no sim-time axis */,
                                    std::vector<pointingSample> &out /**< [out] chronological samples */ )
{
    out.clear();

    if( data == nullptr || depth < 1 || nWritten == 0 || nAxes < 3 )
    {
        return -1;
    }

    const uint64_t nValid = std::min<uint64_t>( nWritten, depth );
    uint64_t nUse = ticksForExposure( expTime, writeHz );

    if( nAxes > pointingIxSimTime )
    {
        const double *newest = data + static_cast<size_t>( cnt1 % depth ) * nAxes;
        double ra, dec, pa, tNewest;
        unpackPointing( newest, nAxes, ra, dec, pa, tNewest );

        const double tStart = tNewest - std::max( expTime, 0.0 );
        nUse = 0;

        for( uint64_t age = 0; age < nValid; ++age )
        {
            const uint32_t slice = static_cast<uint32_t>( ( cnt1 + depth - age ) % depth );
            const double *src = data + static_cast<size_t>( slice ) * nAxes;
            double t;
            unpackPointing( src, nAxes, ra, dec, pa, t );

            if( age > 0 && t < tStart )
            {
                break;
            }

            nUse = age + 1;
        }
    }

    return collectPointingTicks( data, nAxes, depth, cnt1, nWritten, nUse, expTime, out );
}

/// Reduce a trail to at most `maxN` samples by averaging adjacent ticks.
/** Total `m_dt` is conserved, so the integrated flux is unchanged. Right
 * ascension is unwrapped against the first sample so a cut at 0/360 deg does
 * not average to 180.
 *
 * \ingroup wccCommon
 */
inline void binPointingSamples( std::vector<pointingSample> &samples /**< [in,out] chronological ticks */,
                                uint32_t maxN /**< [in] maximum samples to keep */ )
{
    if( maxN < 1 || samples.size() <= maxN )
    {
        return;
    }

    const size_t n = samples.size();
    const double ra0 = samples.front().m_ra;
    std::vector<pointingSample> out( maxN );

    for( uint32_t i = 0; i < maxN; ++i )
    {
        const size_t a = ( static_cast<size_t>( i ) * n ) / maxN;
        const size_t b = ( static_cast<size_t>( i + 1 ) * n ) / maxN;
        const size_t m = ( b > a ) ? ( b - a ) : 1;

        double sra = 0, sdec = 0, spa = 0, st = 0, sdt = 0;

        for( size_t k = a; k < b && k < n; ++k )
        {
            double dra = samples[k].m_ra - ra0;

            if( dra > 180.0 )
            {
                dra -= 360.0;
            }
            if( dra < -180.0 )
            {
                dra += 360.0;
            }

            sra += dra;
            sdec += samples[k].m_dec;
            spa += samples[k].m_pa;
            st += samples[k].m_time;
            sdt += samples[k].m_dt;
        }

        pointingSample &o = out[i];
        o.m_ra = ra0 + sra / static_cast<double>( m );
        o.m_ra = std::fmod( o.m_ra, 360.0 );

        if( o.m_ra < 0 )
        {
            o.m_ra += 360.0;
        }

        o.m_dec = sdec / static_cast<double>( m );
        o.m_pa = spa / static_cast<double>( m );
        o.m_time = st / static_cast<double>( m );
        o.m_dt = sdt;
    }

    samples.swap( out );
}

} // namespace wcc
} // namespace MagAOX

#endif // wccPointingShmim_hpp
