/** \file wccAstrometry.hpp
 * \brief Source detection and astrometric solution for the WCC controller.
 * \author Adam Schilperoort
 *
 * Three stages, each usable on its own:
 *
 * 1. detectSources() finds star like peaks in one sensor frame and centroids
 *    them, working from a clipped background and noise estimate so it does not
 *    need a calibration frame.
 * 2. solveFrame() matches those detections against catalog positions predicted
 *    through a wccSkyWCS and recovers the pixel offset and field rotation of the
 *    frame. Matching is by translation vote, which is robust to a starting error
 *    much larger than the star separation and to unmatched sources on both sides.
 * 3. solveBoresight() combines per sensor solutions from across the array into a
 *    single boresight correction in field angle and roll, which is the quantity
 *    that gets sent to the telescope.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccAstrometry_hpp
#define wccAstrometry_hpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "wccFocalPlane.hpp"
#include "wccSkyWCS.hpp"
#include "wccUnits.hpp"

namespace MagAOX
{
namespace wcc
{

/// One detected source in a frame.
/** \ingroup wccCommon
 */
struct detection
{
    double m_x{ 0 }; ///< Centroid column, 0-based [pixels].

    double m_y{ 0 }; ///< Centroid row, 0-based [pixels].

    double m_flux{ 0 }; ///< Background subtracted sum over the detection window.

    double m_peak{ 0 }; ///< Background subtracted peak pixel value.

    int m_npix{ 0 }; ///< Pixels above threshold contributing to the centroid.
};

/// Tuning for detectSources().
/** \ingroup wccCommon
 */
struct detectConfig
{
    double m_threshold{ 5.0 }; ///< Detection threshold in units of the background sigma.

    int m_boxHalf{ 5 }; ///< Half width of the centroid window [pixels].

    int m_minSeparation{ 6 }; ///< Minimum separation between accepted peaks [pixels].

    size_t m_maxSources{ 200 }; ///< Stop after this many sources, brightest first.

    int m_edgeMargin{ 2 }; ///< Reject peaks within this many pixels of the frame edge.

    int m_minPixels{ 2 }; ///< Reject peaks with fewer than this many pixels above threshold.

    /// Radius over which a bright star suppresses much fainter neighbours [pixels].
    /** A bright star's Airy rings and diffraction spikes appear as genuine local
     * maxima well outside m_minSeparation, and each one would otherwise become a
     * spurious source that steals a match from the real star. Within this radius
     * of an accepted detection, a candidate is only kept if it is at least
     * m_ringSuppressRatio as bright, which discards rings while preserving real
     * close pairs of comparable brightness.
     */
    double m_ringSuppressRadius{ 24.0 };

    /// Brightness fraction a candidate must reach to survive ring suppression.
    double m_ringSuppressRatio{ 0.3 };
};

/// Background level and noise of a frame, from iterative sigma clipping.
/** \ingroup wccCommon
 */
struct backgroundEstimate
{
    double m_level{ 0 }; ///< Background level.

    double m_sigma{ 0 }; ///< Background standard deviation.

    size_t m_nUsed{ 0 }; ///< Pixels surviving the clip.
};

/// Estimate the background level and noise by iterative sigma clipping.
/** Stars occupy a small fraction of a WCC frame, so clipping converges to the
 * sky and read noise floor in a few passes.
 *
 * \returns the estimate
 *
 * \ingroup wccCommon
 */
inline backgroundEstimate estimateBackground( const float *frame /**< [in] frame, row major */,
                                              size_t npix /**< [in] number of pixels */,
                                              int iterations = 3 /**< [in] clipping passes */,
                                              double nSigma = 3.0 /**< [in] clip at this many sigma */,
                                              size_t stride = 1 /**< [in] sample every stride-th pixel */ )
{
    backgroundEstimate out;

    if( frame == nullptr || npix == 0 )
    {
        return out;
    }

    if( stride < 1 )
    {
        stride = 1;
    }

    double lo = -std::numeric_limits<double>::infinity();
    double hi = std::numeric_limits<double>::infinity();

    for( int it = 0; it <= iterations; ++it )
    {
        double sum = 0, sum2 = 0;
        size_t n = 0;

        for( size_t i = 0; i < npix; i += stride )
        {
            const double v = frame[i];

            if( v < lo || v > hi )
            {
                continue;
            }

            sum += v;
            sum2 += v * v;
            ++n;
        }

        if( n < 2 )
        {
            break;
        }

        const double mean = sum / n;
        const double var = std::max( 0.0, sum2 / n - mean * mean );
        const double sd = std::sqrt( var );

        out.m_level = mean;
        out.m_sigma = sd;
        out.m_nUsed = n;

        if( sd == 0 )
        {
            break;
        }

        lo = mean - nSigma * sd;
        hi = mean + nSigma * sd;
    }

    return out;
}

/// Find and centroid star like sources in a frame.
/** Peaks are required to be a strict local maximum in a 3 by 3 neighbourhood and
 * above threshold, then centroided over a square window with the background
 * removed. Detections are returned brightest first and thinned so that no two
 * are closer than m_minSeparation, which suppresses the multiple hits a bright
 * star's diffraction rings would otherwise produce.
 *
 * \returns the number of sources found
 *
 * \ingroup wccCommon
 */
inline size_t detectSources( const float *frame /**< [in] frame, row major */,
                             int w /**< [in] frame width [pixels] */,
                             int h /**< [in] frame height [pixels] */,
                             const detectConfig &cfg /**< [in] detection tuning */,
                             std::vector<detection> &out /**< [out] detections, brightest first */,
                             backgroundEstimate *bkgOut = nullptr /**< [out] background estimate, optional */ )
{
    out.clear();

    if( frame == nullptr || w < 3 || h < 3 )
    {
        return 0;
    }

    const size_t npix = static_cast<size_t>( w ) * static_cast<size_t>( h );

    // Subsample large frames for the background estimate; a few hundred thousand
    // samples pin the level and sigma well enough and keeps this off the critical path.
    const size_t stride = std::max<size_t>( 1, npix / 400000 );
    const backgroundEstimate bkg = estimateBackground( frame, npix, 3, 3.0, stride );

    if( bkgOut != nullptr )
    {
        *bkgOut = bkg;
    }

    // With no measurable noise, fall back to a small fraction of the dynamic
    // range so a noiseless simulated frame still yields detections.
    double sigma = bkg.m_sigma;
    if( !( sigma > 0 ) )
    {
        double mx = frame[0];
        for( size_t i = 1; i < npix; i += stride )
        {
            mx = std::max<double>( mx, frame[i] );
        }
        sigma = std::max( 1e-6, 1e-3 * ( mx - bkg.m_level ) );
    }

    const double thresh = bkg.m_level + cfg.m_threshold * sigma;
    const int margin = std::max( 1, cfg.m_edgeMargin );

    std::vector<detection> cand;

    for( int y = margin; y < h - margin; ++y )
    {
        const float *row = frame + static_cast<size_t>( y ) * static_cast<size_t>( w );
        const float *up = row - w;
        const float *dn = row + w;

        for( int x = margin; x < w - margin; ++x )
        {
            const float v = row[x];

            if( !( v > thresh ) )
            {
                continue;
            }

            // Strict local maximum, with ties broken toward lower index so a flat
            // saturated core produces one detection rather than many.
            if( v < up[x - 1] || v < up[x] || v < up[x + 1] || v < row[x - 1] || v <= row[x + 1] ||
                v < dn[x - 1] || v <= dn[x] || v <= dn[x + 1] )
            {
                continue;
            }

            // Background subtracted centroid over the window.
            const int bh = std::max( 1, cfg.m_boxHalf );
            const int x0 = std::max( 0, x - bh );
            const int y0 = std::max( 0, y - bh );
            const int x1 = std::min( w - 1, x + bh );
            const int y1 = std::min( h - 1, y + bh );

            double sw = 0, sx = 0, sy = 0;
            int nAbove = 0;

            for( int yy = y0; yy <= y1; ++yy )
            {
                const float *r = frame + static_cast<size_t>( yy ) * static_cast<size_t>( w );

                for( int xx = x0; xx <= x1; ++xx )
                {
                    const double d = r[xx] - bkg.m_level;

                    if( d <= 0 )
                    {
                        continue;
                    }

                    if( r[xx] > thresh )
                    {
                        ++nAbove;
                    }

                    sw += d;
                    sx += d * xx;
                    sy += d * yy;
                }
            }

            if( !( sw > 0 ) || nAbove < cfg.m_minPixels )
            {
                continue;
            }

            detection d;
            d.m_x = sx / sw;
            d.m_y = sy / sw;
            d.m_flux = sw;
            d.m_peak = v - bkg.m_level;
            d.m_npix = nAbove;

            cand.push_back( d );
        }
    }

    std::sort( cand.begin(), cand.end(), []( const detection &a, const detection &b ) {
        return a.m_peak > b.m_peak;
    } );

    // Thin by separation and suppress the diffraction structure of bright stars.
    // Candidates arrive brightest first, so any already accepted neighbour is the
    // brighter of the pair.
    const double minSep2 = static_cast<double>( cfg.m_minSeparation ) * cfg.m_minSeparation;
    const double ringR2 = cfg.m_ringSuppressRadius * cfg.m_ringSuppressRadius;

    for( const detection &d : cand )
    {
        bool reject = false;

        for( const detection &k : out )
        {
            const double dx = d.m_x - k.m_x;
            const double dy = d.m_y - k.m_y;
            const double r2 = dx * dx + dy * dy;

            if( r2 < minSep2 )
            {
                reject = true;
                break;
            }

            if( r2 < ringR2 && d.m_peak < cfg.m_ringSuppressRatio * k.m_peak )
            {
                reject = true;
                break;
            }
        }

        if( reject )
        {
            continue;
        }

        out.push_back( d );

        if( out.size() >= cfg.m_maxSources )
        {
            break;
        }
    }

    return out.size();
}

/// A predicted source position, from a catalog projected through a WCS.
/** \ingroup wccCommon
 */
struct prediction
{
    double m_x{ 0 }; ///< Predicted column, 0-based [pixels].

    double m_y{ 0 }; ///< Predicted row, 0-based [pixels].

    double m_mag{ 99 }; ///< Catalog magnitude.

    size_t m_catalogIndex{ 0 }; ///< Index into the catalog this came from.
};

/// The result of matching one frame against predictions.
/** The solution is a rotation about (m_pivotX, m_pivotY) followed by a
 * translation. m_dx and m_dy are therefore only meaningful together with the
 * pivot: the same physical transform yields different translations for different
 * pivots. Callers should use apply() or offsetAt() rather than reading m_dx and
 * m_dy directly, because what matters operationally is where a particular
 * position, such as a guide star's target pixel, actually landed.
 *
 * \ingroup wccCommon
 */
struct frameSolution
{
    bool m_valid{ false }; ///< True when enough sources matched to trust the result.

    double m_dx{ 0 }; ///< Translation along columns about the pivot [pixels].

    double m_dy{ 0 }; ///< Translation along rows about the pivot [pixels].

    double m_rotation{ 0 }; ///< Field rotation, measured minus predicted [deg].

    int m_nMatched{ 0 }; ///< Sources matched.

    double m_rms{ 0 }; ///< RMS residual of the matched pairs [pixels].

    double m_pivotX{ 0 }; ///< Column the rotation is about [pixels].

    double m_pivotY{ 0 }; ///< Row the rotation is about [pixels].

    /// Map a predicted position to where the solution says it actually is.
    void apply( double x /**< [in] predicted column [pixels] */,
                double y /**< [in] predicted row [pixels] */,
                double &mx /**< [out] measured column [pixels] */,
                double &my /**< [out] measured row [pixels] */ ) const;

    /// Offset the solution implies at a given position, measured minus predicted.
    /** This is the pivot independent quantity a control loop wants: how far a
     * particular pixel has actually moved.
     */
    void offsetAt( double x /**< [in] predicted column [pixels] */,
                   double y /**< [in] predicted row [pixels] */,
                   double &ox /**< [out] column offset [pixels] */,
                   double &oy /**< [out] row offset [pixels] */ ) const;
};

inline void frameSolution::apply( double x, double y, double &mx, double &my ) const
{
    const double cr = std::cos( m_rotation * deg2rad );
    const double sr = std::sin( m_rotation * deg2rad );
    const double rx = x - m_pivotX;
    const double ry = y - m_pivotY;

    mx = m_pivotX + cr * rx - sr * ry + m_dx;
    my = m_pivotY + sr * rx + cr * ry + m_dy;
}

inline void frameSolution::offsetAt( double x, double y, double &ox, double &oy ) const
{
    double mx, my;
    apply( x, y, mx, my );

    ox = mx - x;
    oy = my - y;
}

/// Tuning for solveFrame().
/** \ingroup wccCommon
 */
struct solveConfig
{
    /// Largest pointing error the vote will search, in pixels.
    double m_searchRadius{ 400.0 };

    /// Vote bin size [pixels]. Should be a few times the expected centroid scatter.
    double m_voteBin{ 8.0 };

    /// Pairing radius once the coarse offset is known [pixels].
    double m_matchRadius{ 6.0 };

    /// Minimum matched sources for a valid solution.
    int m_minMatched{ 4 };

    /// Solve for field rotation as well as translation. Needs at least 3 matches.
    bool m_solveRotation{ true };

    /// Refinement passes after the first pairing.
    int m_refineIterations{ 2 };
};

/// Match detections against predictions and solve for the frame offset.
/** Stage one histograms every detection to prediction offset within the search
 * radius and takes the modal bin, which survives a large fraction of spurious
 * detections and predictions with no counterpart. Stage two pairs sources using
 * that coarse offset and least squares fits translation, and optionally
 * rotation, about the centroid of the matched predictions.
 *
 * \returns 0 on success
 * \returns -1 if too few sources matched, with the solution marked invalid
 *
 * \ingroup wccCommon
 */
inline int solveFrame( const std::vector<detection> &det /**< [in] detected sources */,
                       const std::vector<prediction> &pred /**< [in] predicted positions */,
                       const solveConfig &cfg /**< [in] solver tuning */,
                       frameSolution &sol /**< [out] the solution */ )
{
    sol = frameSolution();

    if( det.size() < 2 || pred.size() < 2 )
    {
        return -1;
    }

    // ------------------------------------------------- stage one: offset vote
    const double bin = ( cfg.m_voteBin > 0 ) ? cfg.m_voteBin : 8.0;
    const int nb = std::max( 1, static_cast<int>( std::ceil( cfg.m_searchRadius / bin ) ) );
    const int side = 2 * nb + 1;

    const size_t nBins = static_cast<size_t>( side ) * static_cast<size_t>( side );

    std::vector<int> votes( nBins, 0 );

    // The exact offsets are accumulated alongside the counts so that the coarse
    // estimate can be the centroid of the contributing offsets rather than a bin
    // center. Without this the estimate carries up to 1.4 bins of quantization
    // error, which can exceed the pairing radius and strand the solve.
    std::vector<double> voteX( nBins, 0.0 );
    std::vector<double> voteY( nBins, 0.0 );

    for( const detection &d : det )
    {
        for( const prediction &p : pred )
        {
            const double ox = d.m_x - p.m_x;
            const double oy = d.m_y - p.m_y;

            if( std::fabs( ox ) > cfg.m_searchRadius || std::fabs( oy ) > cfg.m_searchRadius )
            {
                continue;
            }

            const int bx = static_cast<int>( std::lround( ox / bin ) ) + nb;
            const int by = static_cast<int>( std::lround( oy / bin ) ) + nb;

            if( bx < 0 || bx >= side || by < 0 || by >= side )
            {
                continue;
            }

            const size_t k = static_cast<size_t>( by ) * static_cast<size_t>( side ) + static_cast<size_t>( bx );

            ++votes[k];
            voteX[k] += ox;
            voteY[k] += oy;
        }
    }

    // Modal bin, summed over its 3 by 3 neighbourhood so a peak straddling a bin
    // boundary is not split, then refined to the centroid of that neighbourhood.
    int bestScore = -1;
    double coarseX = 0, coarseY = 0;

    for( int by = 0; by < side; ++by )
    {
        for( int bx = 0; bx < side; ++bx )
        {
            int score = 0;
            double sx = 0, sy = 0;

            for( int dy = -1; dy <= 1; ++dy )
            {
                for( int dx = -1; dx <= 1; ++dx )
                {
                    const int xx = bx + dx;
                    const int yy = by + dy;

                    if( xx < 0 || xx >= side || yy < 0 || yy >= side )
                    {
                        continue;
                    }

                    const size_t k =
                        static_cast<size_t>( yy ) * static_cast<size_t>( side ) + static_cast<size_t>( xx );

                    score += votes[k];
                    sx += voteX[k];
                    sy += voteY[k];
                }
            }

            if( score > bestScore )
            {
                bestScore = score;
                coarseX = sx / score;
                coarseY = sy / score;
            }
        }
    }

    if( bestScore < cfg.m_minMatched )
    {
        return -1;
    }

    // ------------------------------------- stage two: pair up and least squares
    double dx = coarseX;
    double dy = coarseY;
    double rot = 0;

    const int passes = std::max( 1, cfg.m_refineIterations + 1 );
    double radius = std::max( cfg.m_matchRadius, bin );

    for( int pass = 0; pass < passes; ++pass )
    {
        // Project every prediction through the current model.
        const double cr = std::cos( rot * deg2rad );
        const double sr = std::sin( rot * deg2rad );
        const double r2max = radius * radius;

        std::vector<double> mxv( pred.size() ), myv( pred.size() );

        for( size_t ip = 0; ip < pred.size(); ++ip )
        {
            const double rx = pred[ip].m_x - sol.m_pivotX;
            const double ry = pred[ip].m_y - sol.m_pivotY;
            mxv[ip] = sol.m_pivotX + cr * rx - sr * ry + dx;
            myv[ip] = sol.m_pivotY + sr * rx + cr * ry + dy;
        }

        // Mutual nearest neighbour pairing: a prediction and a detection are only
        // matched if each is the other's closest candidate. A greedy one sided
        // pass lets a spurious detection consume a real star's match and then
        // biases the fit, which mutual matching cannot do.
        std::vector<size_t> nearestDet( pred.size(), det.size() );
        std::vector<double> nearestDetR2( pred.size(), r2max );
        std::vector<size_t> nearestPred( det.size(), pred.size() );
        std::vector<double> nearestPredR2( det.size(), r2max );

        for( size_t ip = 0; ip < pred.size(); ++ip )
        {
            for( size_t id = 0; id < det.size(); ++id )
            {
                const double ex = det[id].m_x - mxv[ip];
                const double ey = det[id].m_y - myv[ip];
                const double e2 = ex * ex + ey * ey;

                if( e2 < nearestDetR2[ip] )
                {
                    nearestDetR2[ip] = e2;
                    nearestDet[ip] = id;
                }

                if( e2 < nearestPredR2[id] )
                {
                    nearestPredR2[id] = e2;
                    nearestPred[id] = ip;
                }
            }
        }

        std::vector<std::pair<size_t, size_t>> pairs;

        for( size_t ip = 0; ip < pred.size(); ++ip )
        {
            const size_t id = nearestDet[ip];

            if( id < det.size() && nearestPred[id] == ip )
            {
                pairs.emplace_back( ip, id );
            }
        }

        if( static_cast<int>( pairs.size() ) < cfg.m_minMatched )
        {
            return -1;
        }

        // Pivot on the centroid of the matched predictions so translation and
        // rotation decouple.
        double px = 0, py = 0;
        for( const auto &pr : pairs )
        {
            px += pred[pr.first].m_x;
            py += pred[pr.first].m_y;
        }
        px /= pairs.size();
        py /= pairs.size();

        // Least squares for a rotation plus translation: minimizing over a
        // similarity transform with unit scale reduces to the mean offset and the
        // Kabsch angle from the cross terms.
        double mdx = 0, mdy = 0;
        for( const auto &pr : pairs )
        {
            mdx += det[pr.second].m_x - pred[pr.first].m_x;
            mdy += det[pr.second].m_y - pred[pr.first].m_y;
        }
        mdx /= pairs.size();
        mdy /= pairs.size();

        double newRot = 0;

        if( cfg.m_solveRotation && pairs.size() >= 3 )
        {
            double sxy = 0, sxx = 0;

            for( const auto &pr : pairs )
            {
                const double ax = pred[pr.first].m_x - px;
                const double ay = pred[pr.first].m_y - py;
                const double bx = det[pr.second].m_x - ( px + mdx );
                const double by = det[pr.second].m_y - ( py + mdy );

                sxy += ax * by - ay * bx;
                sxx += ax * bx + ay * by;
            }

            if( sxx != 0 || sxy != 0 )
            {
                newRot = std::atan2( sxy, sxx ) * rad2deg;
            }
        }

        sol.m_pivotX = px;
        sol.m_pivotY = py;
        dx = mdx;
        dy = mdy;
        rot = newRot;

        // Residuals under the fitted model.
        const double cr2 = std::cos( rot * deg2rad );
        const double sr2 = std::sin( rot * deg2rad );
        double sum2 = 0;

        for( const auto &pr : pairs )
        {
            const double rx = pred[pr.first].m_x - px;
            const double ry = pred[pr.first].m_y - py;
            const double mx = px + cr2 * rx - sr2 * ry + dx;
            const double my = py + sr2 * rx + cr2 * ry + dy;
            const double ex = det[pr.second].m_x - mx;
            const double ey = det[pr.second].m_y - my;

            sum2 += ex * ex + ey * ey;
        }

        sol.m_nMatched = static_cast<int>( pairs.size() );
        sol.m_rms = std::sqrt( sum2 / pairs.size() );

        // Tighten the pairing radius for the next pass now that the model is better.
        radius = std::max( cfg.m_matchRadius, 3.0 * sol.m_rms );
    }

    sol.m_dx = dx;
    sol.m_dy = dy;
    sol.m_rotation = rot;
    sol.m_valid = ( sol.m_nMatched >= cfg.m_minMatched );

    return sol.m_valid ? 0 : -1;
}

/// Project a catalog cone onto a sensor ROI to build the prediction list.
/** \returns the number of predictions inside the frame
 *
 * \ingroup wccCommon
 */
inline size_t predictSources( const skyWCS &wcs /**< [in] the ROI world coordinate system */,
                              int w /**< [in] frame width [pixels] */,
                              int h /**< [in] frame height [pixels] */,
                              const std::vector<double> &ra /**< [in] catalog right ascensions [deg] */,
                              const std::vector<double> &dec /**< [in] catalog declinations [deg] */,
                              const std::vector<double> &mag /**< [in] catalog magnitudes */,
                              std::vector<prediction> &out /**< [out] predictions, brightest first */,
                              double marginPix = 0 /**< [in] accept predictions this far outside the frame */ )
{
    out.clear();

    const size_t n = std::min( ra.size(), std::min( dec.size(), mag.size() ) );

    for( size_t i = 0; i < n; ++i )
    {
        double x, y;

        if( !wcs.world2pix( ra[i], dec[i], x, y ) )
        {
            continue;
        }

        if( x < -marginPix || y < -marginPix || x > w - 1 + marginPix || y > h - 1 + marginPix )
        {
            continue;
        }

        prediction p;
        p.m_x = x;
        p.m_y = y;
        p.m_mag = mag[i];
        p.m_catalogIndex = i;

        out.push_back( p );
    }

    std::sort( out.begin(), out.end(), []( const prediction &a, const prediction &b ) {
        return a.m_mag < b.m_mag;
    } );

    return out.size();
}

/// One sensor's contribution to the boresight solution.
/** \ingroup wccCommon
 */
struct sensorMeasurement
{
    size_t m_sensor{ 0 }; ///< Index into the focal plane model.

    double m_fieldX{ 0 }; ///< Field angle of the solved region on this sensor [arcsec].

    double m_fieldY{ 0 }; ///< Field angle of the solved region on this sensor [arcsec].

    double m_shiftX{ 0 }; ///< Measured field shift along focal plane X [arcsec].

    double m_shiftY{ 0 }; ///< Measured field shift along focal plane Y [arcsec].

    double m_rotation{ 0 }; ///< Measured field rotation on this sensor [deg].

    double m_weight{ 1 }; ///< Relative weight, typically the matched source count.
};

/// A boresight correction solved from several sensors at once.
/** \ingroup wccCommon
 */
struct boresightSolution
{
    bool m_valid{ false }; ///< True when at least one sensor contributed.

    double m_fieldX{ 0 }; ///< Boresight shift along focal plane X [arcsec].

    double m_fieldY{ 0 }; ///< Boresight shift along focal plane Y [arcsec].

    double m_roll{ 0 }; ///< Roll error [deg].

    int m_nSensors{ 0 }; ///< Sensors that contributed.

    double m_rms{ 0 }; ///< RMS residual of the fit [arcsec].
};

/// Combine per sensor field shifts into a single boresight and roll correction.
/** A boresight offset \f$(\Delta X, \Delta Y)\f$ plus a roll \f$\Delta\theta\f$
 * about the boresight produces, at a sensor sitting at field position
 * \f$(X_s, Y_s)\f$, the local shift
 * \f[ (\delta x, \delta y) = (\Delta X, \Delta Y) + \Delta\theta\,(-Y_s,\; X_s). \f]
 * With two or more sensors at different field positions this is an
 * overdetermined three parameter linear system, solved here by weighted least
 * squares. Roll is only separable when the sensors span a range of field
 * positions, so with a single sensor the measured frame rotation is used
 * directly instead.
 *
 * \returns 0 on success
 * \returns -1 if no sensor contributed
 *
 * \ingroup wccCommon
 */
inline int solveBoresight( const std::vector<sensorMeasurement> &meas /**< [in] per sensor measurements */,
                           boresightSolution &sol /**< [out] the boresight correction */ )
{
    sol = boresightSolution();

    if( meas.empty() )
    {
        return -1;
    }

    sol.m_nSensors = static_cast<int>( meas.size() );

    if( meas.size() == 1 )
    {
        // Roll is degenerate with translation from one sensor, so take the
        // rotation the frame solution measured directly.
        sol.m_fieldX = meas[0].m_shiftX;
        sol.m_fieldY = meas[0].m_shiftY;
        sol.m_roll = meas[0].m_rotation;
        sol.m_valid = true;
        sol.m_rms = 0;

        return 0;
    }

    // Normal equations for [dX, dY, dtheta] in radians for the roll term.
    double a[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
    double b[3] = { 0, 0, 0 };

    for( const sensorMeasurement &m : meas )
    {
        const double wt = ( m.m_weight > 0 ) ? m.m_weight : 1.0;

        // Row for the X equation: dX + dtheta * (-Ys) = shiftX
        const double r1[3] = { 1.0, 0.0, -m.m_fieldY };
        // Row for the Y equation: dY + dtheta * ( Xs) = shiftY
        const double r2[3] = { 0.0, 1.0, m.m_fieldX };

        for( int i = 0; i < 3; ++i )
        {
            for( int j = 0; j < 3; ++j )
            {
                a[i][j] += wt * ( r1[i] * r1[j] + r2[i] * r2[j] );
            }

            b[i] += wt * ( r1[i] * m.m_shiftX + r2[i] * m.m_shiftY );
        }
    }

    // Gauss-Jordan on a 3 by 3 with partial pivoting.
    double m3[3][4] = { { a[0][0], a[0][1], a[0][2], b[0] },
                        { a[1][0], a[1][1], a[1][2], b[1] },
                        { a[2][0], a[2][1], a[2][2], b[2] } };

    bool singular = false;

    for( int c = 0; c < 3; ++c )
    {
        int piv = c;
        for( int r = c + 1; r < 3; ++r )
        {
            if( std::fabs( m3[r][c] ) > std::fabs( m3[piv][c] ) )
            {
                piv = r;
            }
        }

        if( std::fabs( m3[piv][c] ) < 1e-12 )
        {
            singular = true;
            break;
        }

        if( piv != c )
        {
            for( int k = 0; k < 4; ++k )
            {
                std::swap( m3[c][k], m3[piv][k] );
            }
        }

        const double d = m3[c][c];
        for( int k = c; k < 4; ++k )
        {
            m3[c][k] /= d;
        }

        for( int r = 0; r < 3; ++r )
        {
            if( r == c )
            {
                continue;
            }

            const double f = m3[r][c];
            for( int k = c; k < 4; ++k )
            {
                m3[r][k] -= f * m3[c][k];
            }
        }
    }

    if( singular )
    {
        // Sensors are collinear or coincident in field: solve translation only
        // and average the per frame rotations.
        double wx = 0, wy = 0, wr = 0, wsum = 0;

        for( const sensorMeasurement &m : meas )
        {
            const double wt = ( m.m_weight > 0 ) ? m.m_weight : 1.0;
            wx += wt * m.m_shiftX;
            wy += wt * m.m_shiftY;
            wr += wt * m.m_rotation;
            wsum += wt;
        }

        if( !( wsum > 0 ) )
        {
            return -1;
        }

        sol.m_fieldX = wx / wsum;
        sol.m_fieldY = wy / wsum;
        sol.m_roll = wr / wsum;
        sol.m_valid = true;
    }
    else
    {
        sol.m_fieldX = m3[0][3];
        sol.m_fieldY = m3[1][3];
        sol.m_roll = m3[2][3] * rad2deg;
        sol.m_valid = true;
    }

    // Residuals.
    const double dth = sol.m_roll * deg2rad;
    double sum2 = 0, wsum = 0;

    for( const sensorMeasurement &m : meas )
    {
        const double wt = ( m.m_weight > 0 ) ? m.m_weight : 1.0;
        const double ex = sol.m_fieldX - dth * m.m_fieldY - m.m_shiftX;
        const double ey = sol.m_fieldY + dth * m.m_fieldX - m.m_shiftY;

        sum2 += wt * ( ex * ex + ey * ey );
        wsum += wt;
    }

    sol.m_rms = ( wsum > 0 ) ? std::sqrt( sum2 / wsum ) : 0.0;

    return 0;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccAstrometry_hpp
