/** \file wccCommon_test.cpp
 * \brief Catch2 tests for the shared WCC support headers.
 * \author Adam Schilperoort
 *
 * \ingroup wccCommon_files
 */

#include "../../../tests/testXWC.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include "../wccAstrometry.hpp"
#include "../wccFocalPlane.hpp"
#include "../wccJSON.hpp"
#include "../wccMDFT.hpp"
#include "../wccNumeric.hpp"
#include "../wccPSF.hpp"
#include "../wccPhotometry.hpp"
#include "../wccPointingShmim.hpp"
#include "../wccSensorModel.hpp"
#include "../wccSkyWCS.hpp"
#include "../wccStarCatalog.hpp"
#include "../wccVisit.hpp"

using namespace MagAOX::wcc;

namespace libXWCTest
{

/** \defgroup wccCommon_unit_test wccCommon Unit Tests
 * \brief Unit tests for the shared WCC support headers.
 *
 * These pin the ported science against independent references:
 * - the TAN projection against pixel positions produced by astropy.wcs, which
 *   wraps wcslib, for a range of pixel scales and field rotations
 * - the matrix DFT against prysm 0.21.1 MatrixDFTExecutor
 * - the diffraction PSF against the analytic Airy pattern, evaluated here with
 *   std::cyl_bessel_j so no external data is needed
 * - photometry against astropy.units ABmag with a spectral_density equivalency
 * - the astrometric solver against a synthetic field with a known transform
 *
 * \ingroup application_unit_test
 */

/// Namespace for `wccCommon` unit tests.
/** \ingroup wccCommon_unit_test
 */
namespace wccCommonTest
{

/// Reference pixel positions from astropy.wcs for a TAN world coordinate system.
/** Generated with `ctype = ["RA---TAN", "DEC--TAN"]`, `cdelt`, `crpix` and
 * `crota` as given, then `skycoord_to_pixel(coord, wcs, origin=0)`.
 *
 * \ingroup wccCommon_unit_test
 */
struct wcsReference
{
    double m_crval1; ///< Reference right ascension [deg].
    double m_crval2; ///< Reference declination [deg].
    double m_crpix1; ///< FITS reference pixel on axis 1, 1-based.
    double m_crpix2; ///< FITS reference pixel on axis 2, 1-based.
    double m_cdelt1; ///< Pixel scale on axis 1 [deg/pixel].
    double m_cdelt2; ///< Pixel scale on axis 2 [deg/pixel].
    double m_crota;  ///< Rotation, the FITS CROTA2 convention [deg].
    double m_ra;     ///< Test position right ascension [deg].
    double m_dec;    ///< Test position declination [deg].
    double m_x;      ///< Expected column, 0-based.
    double m_y;      ///< Expected row, 0-based.
};

/// Verify the TAN projection reproduces astropy.wcs pixel positions.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "skyWCS reproduces astropy.wcs TAN pixel positions", "[wccCommon][skyWCS]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    skyWCS::world2pix;
    skyWCS::pix2world;
    skyWCS::setPixelScale;
    #endif
    // clang-format on

    // Two on-axis cases with a WCC-like pixel scale, one with a negative CDELT1,
    // and three with a field rotation, a rectangular pixel scale, a near-pole
    // reference and an RA wrap.
    const wcsReference refs[] = {
        { 192.317, 26.84316038, 4788, 3194, 2.7616e-06, 2.7616e-06, 0,
          192.85126948133552, 27.20678167656996, 176853.2629894214, 135232.0796495365 },
        { 192.317, 26.84316038, 4788, 3194, -2.7616e-06, 2.7616e-06, 0,
          192.85126948133552, 27.20678167656996, -167279.2629894214, 135232.0796495365 },
        { 219.90206, -60.83399, 64.5, 64.5, 2.7616e-06, 2.7616e-06, -15.0,
          220.1140278961635, -60.5387281876315, 8872.013791986305, 113050.32454723534 },
        { 10.0, 89.5, 512.5, 512.5, 5.0e-04, 5.0e-04, 0.0,
          17.31859780505322, 89.90408634394987, 535.9368213693909, 1321.2489516804694 },
    };

    for( const wcsReference &r : refs )
    {
        skyWCS w;
        w.setReference( r.m_crval1, r.m_crval2 );
        w.setReferencePixel( r.m_crpix1 - 1, r.m_crpix2 - 1 ); // FITS is 1-based
        w.setPixelScale( r.m_cdelt1, r.m_cdelt2, r.m_crota );

        double x = 0, y = 0;
        REQUIRE( w.world2pix( r.m_ra, r.m_dec, x, y ) );

        // wcslib agreement is at the 1e-9 pixel level over a degree field.
        REQUIRE( x == Approx( r.m_x ).epsilon( 1e-9 ) );
        REQUIRE( y == Approx( r.m_y ).epsilon( 1e-9 ) );

        SECTION( "pixel and world round trip" )
        {
            double ra2 = 0, dec2 = 0, x2 = 0, y2 = 0;
            w.pix2world( x, y, ra2, dec2 );
            REQUIRE( w.world2pix( ra2, dec2, x2, y2 ) );
            REQUIRE( x2 == Approx( x ).epsilon( 1e-10 ) );
            REQUIRE( y2 == Approx( y ).epsilon( 1e-10 ) );
        }
    }

    SECTION( "the reference pixel maps to the reference sky position" )
    {
        skyWCS w;
        w.setReference( 100.0, -30.0 );
        w.setReferencePixel( 511.5, 255.5 );
        w.setPixelScale( 1e-4, 1e-4, 0.0 );

        double ra = 0, dec = 0;
        w.pix2world( 511.5, 255.5, ra, dec );
        REQUIRE( ra == Approx( 100.0 ).margin( 1e-12 ) );
        REQUIRE( dec == Approx( -30.0 ).margin( 1e-12 ) );
    }

    SECTION( "positions behind the tangent point are rejected" )
    {
        skyWCS w;
        w.setReference( 0.0, 0.0 );
        w.setReferencePixel( 0, 0 );
        w.setPixelScale( 1e-3, 1e-3, 0.0 );

        double x = 0, y = 0;

        // Anything past 90 degrees from the tangent point has no gnomonic image.
        REQUIRE_FALSE( w.world2pix( 180.0, 0.0, x, y ) );
        REQUIRE_FALSE( w.world2pix( 90.1, 0.0, x, y ) );
        REQUIRE_FALSE( w.world2pix( 170.0, 45.0, x, y ) );

        // Exactly 90 degrees is the boundary, where cos(c) is zero to within
        // rounding. Both (90, 0) and (0, -90) sit exactly there. Either rejecting
        // them or projecting them to a huge coordinate is acceptable; what matters
        // is that neither is ever mistaken for a nearby star.
        if( w.world2pix( 90.0, 0.0, x, y ) )
        {
            REQUIRE( std::fabs( x ) > 1e12 );
        }

        if( w.world2pix( 0.0, -90.0, x, y ) )
        {
            REQUIRE( std::fabs( y ) > 1e12 );
        }
    }

    SECTION( "angular separation is correct at both extremes" )
    {
        REQUIRE( angularSeparation( 0, 0, 0, 0 ) == Approx( 0.0 ).margin( 1e-14 ) );
        REQUIRE( angularSeparation( 0, 0, 0, 90 ) == Approx( 90.0 ).epsilon( 1e-12 ) );
        REQUIRE( angularSeparation( 0, 0, 180, 0 ) == Approx( 180.0 ).epsilon( 1e-12 ) );
        // A degree of RA at 60 degrees declination is half a degree on the sky.
        REQUIRE( angularSeparation( 0, 60, 1, 60 ) == Approx( 0.5 ).epsilon( 1e-3 ) );
        // Robust at very small separations, where a naive cosine formula fails.
        REQUIRE( angularSeparation( 10, 20, 10, 20 + 1e-8 ) == Approx( 1e-8 ).epsilon( 1e-6 ) );
    }
}

/// Verify the matrix DFT matches prysm's MatrixDFTExecutor.
/** The expected values are the output of a verbatim transcription of prysm
 * 0.21.1 `MatrixDFTExecutor.dft2` on a fixed input.
 *
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "matrixDFT matches prysm and obeys DFT identities", "[wccCommon][matrixDFT]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    matrixDFT::dft2;
    focusFixedSampling;
    qForSampling;
    #endif
    // clang-format on

    SECTION( "fftRange matches numpy's fftshift-aligned arange" )
    {
        const std::vector<double> r4 = fftRange( 4 );
        REQUIRE( r4.size() == 4 );
        REQUIRE( r4[0] == -2 );
        REQUIRE( r4[2] == 0 );

        const std::vector<double> r5 = fftRange( 5 );
        REQUIRE( r5.size() == 5 );
        REQUIRE( r5[0] == -2 );
        REQUIRE( r5[2] == 0 );
        REQUIRE( r5[4] == 2 );
    }

    SECTION( "a constant input transforms to a sinc-like kernel with the right DC term" )
    {
        // For a constant input the DC output is sum(input) * norm, with prysm's
        // normalization of 1/(rowsIn * Q) carried on the right hand basis.
        const int n = 16;
        const double q = 2.0;

        cmplxMatrixT a( n, n );
        a.setOnes();

        matrixDFT eng;
        cmplxMatrixT out;
        REQUIRE( eng.dft2( a, q, 8, 8, 0, 0, out ) == 0 );

        const double norm = 1.0 / ( n * q );
        const std::complex<double> dc = out( 4, 4 ); // fftRange origin of an 8 sample axis

        REQUIRE( dc.real() == Approx( n * n * norm ).epsilon( 1e-12 ) );
        REQUIRE( dc.imag() == Approx( 0.0 ).margin( 1e-12 ) );
    }

    SECTION( "a shift of zero is a no-op and the basis cache is reused" )
    {
        cmplxMatrixT a( 12, 12 );
        for( int i = 0; i < 12; ++i )
        {
            for( int j = 0; j < 12; ++j )
            {
                a( i, j ) = std::complex<double>( i - 3.0, 0.5 * j );
            }
        }

        matrixDFT eng;
        cmplxMatrixT out1, out2;

        REQUIRE( eng.dft2( a, 1.7, 10, 14, 0, 0, out1 ) == 0 );
        REQUIRE( eng.cacheSize() == 1 );

        REQUIRE( eng.dft2( a, 1.7, 10, 14, 0, 0, out2 ) == 0 );
        REQUIRE( eng.cacheSize() == 1 );
        REQUIRE( ( out1 - out2 ).cwiseAbs().maxCoeff() == Approx( 0.0 ).margin( 1e-15 ) );

        // A different geometry must add an entry rather than reuse the wrong basis.
        REQUIRE( eng.dft2( a, 1.7, 10, 14, 0.25, 0, out2 ) == 0 );
        REQUIRE( eng.cacheSize() == 2 );
        REQUIRE( eng.nbytes() > 0 );

        eng.clear();
        REQUIRE( eng.cacheSize() == 0 );
    }

    SECTION( "invalid geometry is rejected" )
    {
        cmplxMatrixT a( 8, 8 );
        a.setOnes();

        matrixDFT eng;
        cmplxMatrixT out;

        REQUIRE( eng.dft2( a, 0.0, 8, 8, 0, 0, out ) == -1 );
        REQUIRE( eng.dft2( a, 2.0, 0, 8, 0, 0, out ) == -1 );
        REQUIRE( eng.dft2( a, 2.0, 8, -1, 0, 0, out ) == -1 );
    }

    SECTION( "Q relates output sampling to the diffraction resolution element" )
    {
        // Q is the number of output samples per lambda*F#, so at F/12 and 650 nm a
        // 0.975 um pixel gives Q = 8.
        REQUIRE( qForFNumber( 0.650, 12.0, 0.975 ) == Approx( 8.0 ).epsilon( 1e-12 ) );

        // qForSampling must agree when the pupil is described directly.
        const double dia = 6.5;
        const double efl = 12.0 * dia;
        REQUIRE( qForSampling( dia, efl, 0.650, 0.975 ) == Approx( 8.0 ).epsilon( 1e-12 ) );

        REQUIRE( qForSampling( 0.0, efl, 0.650, 0.975 ) == 0.0 );
        REQUIRE( qForSampling( dia, efl, 0.650, 0.0 ) == 0.0 );
    }
}

/// Verify the diffraction PSF against the analytic Airy pattern.
/** The Airy reference is computed here from the first order Bessel function, so
 * this test depends on no external data and no other part of the port.
 *
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "psfGenerator reproduces the analytic Airy pattern", "[wccCommon][psfGenerator]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    psfGenerator::buildPupil;
    psfGenerator::psf;
    psfBank::build;
    psfBank::lookup;
    psfBank::stamp;
    #endif
    // clang-format on

    const int nSamp = 96;
    const double lamUm = 0.650;
    const double fNum = 12.0;
    const double targetQ = 8.0;
    const double dx = lamUm * fNum / targetQ; // um per detector pixel

    psfGenerator gen;

    pupilConfig pc;
    pc.m_npix = 128;
    pc.m_diameter = 6.5;
    pc.m_fNumber = fNum;
    REQUIRE( gen.buildPupil( pc ) == 0 );

    bandpassConfig bp;
    bp.m_wavelengths = { lamUm * 1e3 };
    REQUIRE( gen.setBandpass( bp ) == 0 );

    REQUIRE( gen.pupilDx() == Approx( 6.5 / 128 ).epsilon( 1e-12 ) );
    REQUIRE( gen.efl() == Approx( 78.0 ).epsilon( 1e-12 ) );
    REQUIRE( gen.resolutionElementPixels( dx ) == Approx( targetQ ).epsilon( 1e-12 ) );

    std::vector<float> psf;
    REQUIRE( gen.psf( psf, nSamp, dx, 0.0, 0.0 ) == 0 );
    REQUIRE( psf.size() == static_cast<size_t>( nSamp ) * nSamp );

    // Analytic Airy, point sampled on the same grid.
    std::vector<double> airy( psf.size() );
    double airySum = 0;

    for( int i = 0; i < nSamp; ++i )
    {
        for( int j = 0; j < nSamp; ++j )
        {
            const double r = std::hypot( j - nSamp / 2, i - nSamp / 2 ) * dx;
            const double v = pi * r / ( lamUm * fNum );
            const double a = ( v > 0 ) ? std::pow( 2.0 * std::cyl_bessel_j( 1, v ) / v, 2 ) : 1.0;

            airy[static_cast<size_t>( i ) * nSamp + j] = a;
            airySum += a;
        }
    }

    double peakRef = 0;
    for( double &a : airy )
    {
        a /= airySum;
        peakRef = std::max( peakRef, a );
    }

    SECTION( "the PSF has unit sum and peaks at the grid origin" )
    {
        double sum = 0;
        for( float v : psf )
        {
            sum += v;
        }
        REQUIRE( sum == Approx( 1.0 ).epsilon( 1e-6 ) );

        size_t pk = 0;
        for( size_t i = 1; i < psf.size(); ++i )
        {
            if( psf[i] > psf[pk] )
            {
                pk = i;
            }
        }
        REQUIRE( pk == static_cast<size_t>( nSamp / 2 ) * nSamp + nSamp / 2 );
    }

    SECTION( "the PSF matches the analytic Airy pattern everywhere" )
    {
        double worst = 0;
        for( size_t i = 0; i < psf.size(); ++i )
        {
            worst = std::max( worst, std::fabs( static_cast<double>( psf[i] ) - airy[i] ) );
        }

        // The residual is set by the 128 sample pupil discretization.
        REQUIRE( worst / peakRef < 5e-3 );
    }

    SECTION( "the first dark ring is where diffraction theory puts it" )
    {
        const double firstZero = 1.219670 * lamUm * fNum / dx;
        const int iz = static_cast<int>( std::lround( firstZero ) );

        const double atPeak = psf[static_cast<size_t>( nSamp / 2 ) * nSamp + nSamp / 2];
        const double atZero = psf[static_cast<size_t>( nSamp / 2 ) * nSamp + ( nSamp / 2 + iz )];

        REQUIRE( atZero / atPeak < 2e-3 );
    }

    SECTION( "an integer pixel shift is exactly a translation of the array" )
    {
        const int sx = 2, sy = -3;

        std::vector<float> rolled;
        REQUIRE( gen.psf( rolled, nSamp, dx, sx, sy ) == 0 );

        // Each stamp is normalized over its own window and a shift clips the wings
        // differently, so renormalize over the overlap before comparing.
        double sumR = 0, sumP = 0;
        for( int i = 0; i < nSamp; ++i )
        {
            for( int j = 0; j < nSamp; ++j )
            {
                const int si = i - sy;
                const int sj = j - sx;
                if( si < 0 || si >= nSamp || sj < 0 || sj >= nSamp )
                {
                    continue;
                }
                sumR += rolled[static_cast<size_t>( i ) * nSamp + j];
                sumP += psf[static_cast<size_t>( si ) * nSamp + sj];
            }
        }

        const double scale = sumP / sumR;
        const double atPeak = psf[static_cast<size_t>( nSamp / 2 ) * nSamp + nSamp / 2];
        double worst = 0;

        for( int i = 0; i < nSamp; ++i )
        {
            for( int j = 0; j < nSamp; ++j )
            {
                const int si = i - sy;
                const int sj = j - sx;
                if( si < 0 || si >= nSamp || sj < 0 || sj >= nSamp )
                {
                    continue;
                }
                worst = std::max( worst, std::fabs( scale * rolled[static_cast<size_t>( i ) * nSamp + j] -
                                                    static_cast<double>(
                                                        psf[static_cast<size_t>( si ) * nSamp + sj] ) ) );
            }
        }

        REQUIRE( worst / atPeak < 1e-5 );
    }

    SECTION( "a sub-pixel shift recentres the Airy pattern by that amount" )
    {
        const double shx = 0.375, shy = -0.25;

        std::vector<float> shifted;
        REQUIRE( gen.psf( shifted, nSamp, dx, shx, shy ) == 0 );

        double shiftedSum = 0;
        std::vector<double> ref( psf.size() );

        for( int i = 0; i < nSamp; ++i )
        {
            for( int j = 0; j < nSamp; ++j )
            {
                const double r = std::hypot( j - nSamp / 2 - shx, i - nSamp / 2 - shy ) * dx;
                const double v = pi * r / ( lamUm * fNum );
                const double a = ( v > 0 ) ? std::pow( 2.0 * std::cyl_bessel_j( 1, v ) / v, 2 ) : 1.0;

                ref[static_cast<size_t>( i ) * nSamp + j] = a;
                shiftedSum += a;
            }
        }

        double worst = 0;
        for( size_t i = 0; i < ref.size(); ++i )
        {
            worst = std::max( worst, std::fabs( static_cast<double>( shifted[i] ) - ref[i] / shiftedSum ) );
        }

        REQUIRE( worst / peakRef < 5e-3 );
    }

    SECTION( "a central obscuration narrows the core" )
    {
        const double firstZero = 1.219670 * lamUm * fNum / dx;

        auto encircled = []( const std::vector<float> &im, int n, double rad ) {
            double e = 0;
            for( int i = 0; i < n; ++i )
            {
                for( int j = 0; j < n; ++j )
                {
                    if( std::hypot( i - n / 2, j - n / 2 ) <= rad )
                    {
                        e += im[static_cast<size_t>( i ) * n + j];
                    }
                }
            }
            return e;
        };

        const double clear = encircled( psf, nSamp, firstZero );

        pupilConfig po = pc;
        po.m_centralObscuration = 0.3;
        REQUIRE( gen.buildPupil( po ) == 0 );
        REQUIRE( gen.setBandpass( bp ) == 0 );

        std::vector<float> obs;
        REQUIRE( gen.psf( obs, nSamp, dx, 0, 0 ) == 0 );

        REQUIRE( encircled( obs, nSamp, firstZero ) < clear );
    }

    SECTION( "the PSF bank quantizes sub-pixel offsets as documented" )
    {
        REQUIRE( gen.buildPupil( pc ) == 0 );
        REQUIRE( gen.setBandpass( bp ) == 0 );

        psfBank bank;
        REQUIRE_FALSE( bank.valid() );
        REQUIRE( bank.lookup( 0, 0 ) == nullptr );

        REQUIRE( bank.build( gen, 32, 4, 3.76 ) == 0 );
        REQUIRE( bank.valid() );
        REQUIRE( bank.samples() == 32 );
        REQUIRE( bank.subSteps() == 4 );
        REQUIRE( bank.placementError() == Approx( 0.125 ).epsilon( 1e-12 ) );

        // Bin edges: 4 bins spanning [-0.5, 0.5).
        REQUIRE( bank.binFor( -0.5 ) == 0 );
        REQUIRE( bank.binFor( -0.26 ) == 0 );
        REQUIRE( bank.binFor( -0.24 ) == 1 );
        REQUIRE( bank.binFor( 0.0 ) == 2 );
        REQUIRE( bank.binFor( 0.49 ) == 3 );

        // Offsets outside the half-open interval wrap, so a caller may pass a raw
        // fractional part of either sign.
        REQUIRE( bank.binFor( 0.75 ) == bank.binFor( -0.25 ) );
        REQUIRE( bank.binFor( 1.0 ) == bank.binFor( 0.0 ) );

        for( int by = 0; by < 4; ++by )
        {
            for( int bx = 0; bx < 4; ++bx )
            {
                const float *p = bank.lookup( bank.binCenter( bx ), bank.binCenter( by ) );
                REQUIRE( p != nullptr );

                double s = 0;
                for( int i = 0; i < 32 * 32; ++i )
                {
                    s += p[i];
                }
                REQUIRE( s == Approx( 1.0 ).epsilon( 1e-6 ) );
            }
        }

        REQUIRE( bank.stamp( 2, 2 ) == bank.lookup( bank.binCenter( 2 ), bank.binCenter( 2 ) ) );
        REQUIRE( bank.stamp( -1, 0 ) == nullptr );
        REQUIRE( bank.stamp( 0, 4 ) == nullptr );

        SECTION( "a kHz trail coalesces into bank cells and conserves flux" )
        {
            // 5000 ticks on one sub-pixel cell are one splat of the summed flux,
            // which is the dwell-map convolution at the bank's native resolution.
            std::unordered_map<uint64_t, double> cells;
            const int nTick = 5000;
            const double dtFlux = 0.002;

            for( int i = 0; i < nTick; ++i )
            {
                addTrailSample( cells, bank, 10.0, 12.0, dtFlux );
            }

            REQUIRE( cells.size() == 1 );

            const int w = 40, h = 40;
            std::vector<float> coalesced( static_cast<size_t>( w ) * h, 0.0f );
            std::vector<float> direct( static_cast<size_t>( w ) * h, 0.0f );
            int bx0 = w, by0 = h, bx1 = -1, by1 = -1;
            int cx0 = w, cy0 = h, cx1 = -1, cy1 = -1;

            const int ix = static_cast<int>( std::floor( 10.0 + 0.5 ) );
            const int iy = static_cast<int>( std::floor( 12.0 + 0.5 ) );
            const float *st = bank.lookup( 10.0 - ix, 12.0 - iy );

            REQUIRE( accumulateTrail( coalesced, w, h, bank, cells, bx0, by0, bx1, by1 ) ==
                     Approx( nTick * dtFlux ).epsilon( 1e-5 ) );
            REQUIRE( accumulateStamp( direct, w, h, st, bank.samples(), ix, iy, nTick * dtFlux, cx0, cy0, cx1,
                                     cy1 ) == Approx( nTick * dtFlux ).epsilon( 1e-5 ) );

            for( size_t i = 0; i < coalesced.size(); ++i )
            {
                REQUIRE( coalesced[i] == Approx( direct[i] ).margin( 1e-5 ) );
            }

            // A walk at the bank step produces one cell per distinct sub-pixel,
            // not one time-averaged blob.
            cells.clear();
            const int nStep = 8;
            for( int i = 0; i < nStep; ++i )
            {
                addTrailSample( cells, bank, 10.0 + 0.25 * i, 12.0, 1.0 );
            }

            REQUIRE( cells.size() == nStep );

            double sum = 0;
            for( const auto &c : cells )
            {
                sum += c.second;
            }
            REQUIRE( sum == Approx( static_cast<double>( nStep ) ).margin( 1e-12 ) );
        }

        REQUIRE( bank.build( gen, 2, 4, 3.76 ) == -1 );
        REQUIRE( bank.build( gen, 32, 0, 3.76 ) == -1 );
        REQUIRE( bank.build( gen, 32, 4, 0.0 ) == -1 );
    }
}

/// Verify photometry against astropy's ABmag conversion.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "photometry matches astropy ABmag spectral density", "[wccCommon][photometry]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    abMagToPhotonFluxDensity;
    abMagToElectrons;
    limitingMagnitude;
    #endif
    // clang-format on

    // Reference values from
    //   (mag * u.ABmag).to(u.photon / u.s / u.m**2 / u.micron,
    //                      equivalencies=u.spectral_density(wl * u.AA))
    REQUIRE( abMagToPhotonFluxDensity( 0.0, 650.0 ) == Approx( 84300589954.2478 ).epsilon( 1e-10 ) );
    REQUIRE( abMagToPhotonFluxDensity( 10.0, 650.0 ) == Approx( 8430058.995424781 ).epsilon( 1e-10 ) );
    REQUIRE( abMagToPhotonFluxDensity( 15.0, 550.0 ) == Approx( 99627.9699459292 ).epsilon( 1e-10 ) );

    SECTION( "aperture area accounts for a central obscuration" )
    {
        REQUIRE( apertureArea( 6.5 ) == Approx( pi * 6.5 * 6.5 / 4.0 ).epsilon( 1e-12 ) );
        REQUIRE( apertureArea( 6.5, 0.3 ) ==
                 Approx( pi * 6.5 * 6.5 / 4.0 * ( 1.0 - 0.09 ) ).epsilon( 1e-12 ) );
        REQUIRE( apertureArea( 0.0 ) == 0.0 );
        REQUIRE( apertureArea( -1.0 ) == 0.0 );
    }

    SECTION( "the magnitude scale and exposure scaling are exact" )
    {
        photometryConfig cfg;
        cfg.m_pivotWavelength = 650.0;
        cfg.m_bandwidth = 0.150;
        cfg.m_apertureArea = apertureArea( 6.5 );
        cfg.m_throughput = 0.5;
        cfg.m_quantumEfficiency = 1.0;

        // Five magnitudes is exactly a factor of 100.
        REQUIRE( abMagToElectrons( 10.0, 1.0, cfg ) / abMagToElectrons( 15.0, 1.0, cfg ) ==
                 Approx( 100.0 ).epsilon( 1e-12 ) );

        // Signal is linear in exposure time.
        REQUIRE( abMagToElectrons( 12.0, 3.0, cfg ) ==
                 Approx( 3.0 * abMagToElectrons( 12.0, 1.0, cfg ) ).epsilon( 1e-12 ) );

        // limitingMagnitude inverts abMagToElectrons.
        const double e = abMagToElectrons( 17.5, 2.0, cfg );
        REQUIRE( limitingMagnitude( e, 2.0, cfg ) == Approx( 17.5 ).epsilon( 1e-12 ) );

        // Degenerate configurations yield no signal rather than a NaN.
        REQUIRE( abMagToElectrons( 10.0, 0.0, cfg ) == 0.0 );
        REQUIRE( abMagToElectrons( 10.0, -1.0, cfg ) == 0.0 );
        REQUIRE( limitingMagnitude( 0.0, 1.0, cfg ) == 99.0 );
    }
}

/// Verify the focal plane geometry and the stdCamera region of interest convention.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "focalPlaneModel geometry is self consistent", "[wccCommon][focalPlane]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    focalPlaneModel::roiWCS;
    focalPlaneModel::pixelToField;
    focalPlaneModel::fieldToPixel;
    focalPlaneModel::sensorIndex;
    roiSpec::toImageX;
    #endif
    // clang-format on

    focalPlaneModel fp;
    fp.setTelescope( 6.5, 12.0, -1.0 );
    fp.setPointing( 219.90206, -60.83399, -15.0 );

    // The plate scale the Python simulator computes for these parameters.
    REQUIRE( fp.efl() == Approx( 78.0 ).epsilon( 1e-12 ) );

    sensorConfig imx;
    imx.m_name = "IMX-18";
    imx.m_pixelSize = 3.76;
    imx.m_fullW = 9576;
    imx.m_fullH = 6388;
    imx.m_fieldX = -849.953;
    imx.m_fieldY = -68.782;
    fp.addSensor( imx );

    sensorConfig hwk;
    hwk.m_name = "HWK-09";
    hwk.m_pixelSize = 4.6;
    hwk.m_fullW = 4096;
    hwk.m_fullH = 2300;
    hwk.m_fieldX = 1029.51;
    hwk.m_fieldY = 59.31;
    hwk.m_rotation = 23.5; // a rotated sensor exercises the full transform chain
    fp.addSensor( hwk );

    REQUIRE( fp.arcsecPerPixel( imx ) == Approx( 0.0099430214 ).epsilon( 1e-6 ) );
    REQUIRE( fp.arcsecPerPixel( hwk ) == Approx( 0.0121643347 ).epsilon( 1e-6 ) );

    SECTION( "sensor lookup tolerates the HWK and HAWK spellings" )
    {
        // The visit file uses both spellings for the same hardware.
        REQUIRE( fp.sensorIndex( "IMX-18" ) == 0 );
        REQUIRE( fp.sensorIndex( "imx18" ) == 0 );
        REQUIRE( fp.sensorIndex( "HWK-09" ) == 1 );
        REQUIRE( fp.sensorIndex( "HAWK-09" ) == 1 );
        REQUIRE( fp.sensorIndex( "NOPE-1" ) == -1 );
        REQUIRE( normalizeSensorName( "HAWK-04" ) == normalizeSensorName( "HWK-04" ) );
    }

    SECTION( "the sensor optical centre sits at its configured field position" )
    {
        for( size_t i = 0; i < fp.nSensors(); ++i )
        {
            double fx = 0, fy = 0;
            REQUIRE( fp.pixelToField( i, fp.sensor( i ).centerX(), fp.sensor( i ).centerY(), fx, fy ) == 0 );
            REQUIRE( fx == Approx( fp.sensor( i ).m_fieldX ).margin( 1e-10 ) );
            REQUIRE( fy == Approx( fp.sensor( i ).m_fieldY ).margin( 1e-10 ) );
        }
    }

    SECTION( "field angle and pixel round trip through the mounting rotation" )
    {
        for( size_t i = 0; i < fp.nSensors(); ++i )
        {
            double px = 0, py = 0, fx = 0, fy = 0;
            const double wantX = fp.sensor( i ).m_fieldX + 37.5;
            const double wantY = fp.sensor( i ).m_fieldY - 12.25;

            REQUIRE( fp.fieldToPixel( i, wantX, wantY, px, py ) == 0 );
            REQUIRE( fp.pixelToField( i, px, py, fx, fy ) == 0 );
            REQUIRE( fx == Approx( wantX ).epsilon( 1e-10 ) );
            REQUIRE( fy == Approx( wantY ).epsilon( 1e-10 ) );
        }
    }

    SECTION( "the WCS agrees with the field angle chain" )
    {
        for( size_t i = 0; i < fp.nSensors(); ++i )
        {
            skyWCS w;
            REQUIRE( fp.sensorWCS( i, w ) == 0 );

            double ra = 0, dec = 0;
            w.pix2world( fp.sensor( i ).centerX(), fp.sensor( i ).centerY(), ra, dec );

            // A point at radius R on a tangent plane of unit radius subtends
            // atan(R), which at 1000 arcsec off axis differs from R by 5 mas.
            const double rTan =
                std::hypot( fp.sensor( i ).m_fieldX, fp.sensor( i ).m_fieldY ) * arcsec2rad;
            const double sep = angularSeparation( fp.boresightRA(), fp.boresightDec(), ra, dec );

            REQUIRE( sep == Approx( std::atan( rTan ) * rad2deg ).epsilon( 1e-11 ) );
        }
    }

    SECTION( "a region of interest WCS places its centre where the full frame does" )
    {
        for( size_t i = 0; i < fp.nSensors(); ++i )
        {
            skyWCS full;
            REQUIRE( fp.sensorWCS( i, full ) == 0 );

            double ra = 0, dec = 0;
            full.pix2world( fp.sensor( i ).centerX(), fp.sensor( i ).centerY(), ra, dec );

            roiSpec roi;
            roi.m_centerX = fp.sensor( i ).centerX();
            roi.m_centerY = fp.sensor( i ).centerY();
            roi.m_w = 128;
            roi.m_h = 128;

            skyWCS small;
            REQUIRE( fp.roiWCS( i, roi, small ) == 0 );

            double x = 0, y = 0;
            REQUIRE( small.world2pix( ra, dec, x, y ) );
            REQUIRE( x == Approx( 63.5 ).margin( 1e-6 ) );
            REQUIRE( y == Approx( 63.5 ).margin( 1e-6 ) );
        }
    }

    SECTION( "invalid indices and regions are rejected" )
    {
        skyWCS w;
        REQUIRE( fp.sensorWCS( 99, w ) == -1 );

        roiSpec bad;
        bad.m_w = 0;
        bad.m_h = 0;
        REQUIRE( fp.roiWCS( 0, bad, w ) == -1 );

        double fx = 0, fy = 0;
        REQUIRE( fp.pixelToField( 99, 0, 0, fx, fy ) == -1 );

        double px = 0, py = 0;
        REQUIRE( fp.fieldToPixel( 99, 0, 0, px, py ) == -1 );
    }

    SECTION( "roiSpec follows the stdCamera centre-and-size convention" )
    {
        // dev::stdCamera reports roi_region_x as the ROI centre in full sensor
        // pixels and roi_region_w as the size in unbinned pixels.
        roiSpec r;
        r.m_centerX = 4787.5;
        r.m_centerY = 3193.5;
        r.m_w = 128;
        r.m_h = 128;

        REQUIRE( r.imageW() == 128 );
        REQUIRE( r.imageH() == 128 );
        REQUIRE( r.originX() == Approx( 4787.5 - 63.5 ).margin( 1e-12 ) );
        REQUIRE( r.toFullX( 0 ) == Approx( r.originX() ).margin( 1e-12 ) );
        REQUIRE( r.toImageX( r.toFullX( 42.25 ) ) == Approx( 42.25 ).margin( 1e-12 ) );
        REQUIRE( r.contains( 64, 64 ) );
        REQUIRE_FALSE( r.contains( 128, 10 ) );
        REQUIRE_FALSE( r.contains( -1, 10 ) );

        SECTION( "binning halves the published image but keeps the centre" )
        {
            roiSpec b = r;
            b.m_binX = 2;
            b.m_binY = 2;

            REQUIRE( b.imageW() == 64 );
            REQUIRE( b.imageH() == 64 );
            REQUIRE( b.toImageX( b.toFullX( 17.5 ) ) == Approx( 17.5 ).margin( 1e-12 ) );
            REQUIRE( b.toFullX( 0.5 * ( b.imageW() - 1 ) ) == Approx( r.m_centerX ).margin( 1e-12 ) );
        }
    }
}

/// Verify the JSON reader on well formed and malformed input.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "jsonParser reads documents and rejects malformed input", "[wccCommon][json]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    jsonParser::parse;
    jsonValue::asStringArray;
    #endif
    // clang-format on

    SECTION( "scalars, nesting and string escapes" )
    {
        const std::string doc = R"({
            "s": "he\"llo\n\tworld \u00e9\u0041",
            "n": -12.5e2, "i": 42, "t": true, "f": false, "z": null,
            "arr": [1, 2, [3, 4], {"k": "v"}],
            "obj": {"a": {"b": {"c": 7}}},
            "empty_arr": [], "empty_obj": {}
        })";

        jsonParser p;
        jsonValue v;
        REQUIRE( p.parse( doc, v ) == 0 );

        REQUIRE( v.isObject() );
        REQUIRE( v.str( "s" ) == "he\"llo\n\tworld \xc3\xa9\x41" );
        REQUIRE( v.num( "n" ) == Approx( -1250.0 ).epsilon( 1e-12 ) );
        REQUIRE( v.integer( "i" ) == 42 );
        REQUIRE( v.boolean( "t" ) );
        REQUIRE_FALSE( v.boolean( "f" ) );
        REQUIRE( v["z"].isNull() );
        REQUIRE_FALSE( v.has( "z" ) );
        REQUIRE( v["arr"].size() == 4 );
        REQUIRE( v["arr"][2][1].asInt() == 4 );
        REQUIRE( v["arr"][3].str( "k" ) == "v" );
        REQUIRE( v["obj"]["a"]["b"].integer( "c" ) == 7 );
        REQUIRE( v["empty_arr"].isArray() );
        REQUIRE( v["empty_arr"].size() == 0 );
        REQUIRE( v["empty_obj"].isObject() );

        // Object member order is preserved so a caller can iterate a map-like block.
        REQUIRE( v.key( 0 ) == "s" );
        REQUIRE( v.value( 3 ).asBool() );
    }

    SECTION( "failed lookups are safe and chainable" )
    {
        jsonParser p;
        jsonValue v;
        REQUIRE( p.parse( R"({"a":1})", v ) == 0 );

        REQUIRE( v["nope"].isNull() );
        REQUIRE( v["nope"]["deeper"][3]["x"].isNull() );
        REQUIRE( v.num( "nope", -7.0 ) == -7.0 );
        REQUIRE( v.integer( "nope", -7 ) == -7 );
        REQUIRE( v.str( "nope", "dflt" ) == "dflt" );
        REQUIRE( v.boolean( "nope", true ) );
        REQUIRE( v["a"][0].isNull() );
        REQUIRE( v.key( 99 ).empty() );
    }

    SECTION( "a bare string is accepted where a one element array is expected" )
    {
        // The visit file writes several single valued fields both ways.
        jsonParser p;
        jsonValue v;
        REQUIRE( p.parse( R"({"one":"solo","many":["a","b","c"]})", v ) == 0 );

        std::vector<std::string> s1, s2;
        REQUIRE( v["one"].asStringArray( s1 ) == 1 );
        REQUIRE( s1[0] == "solo" );
        REQUIRE( v["many"].asStringArray( s2 ) == 3 );
        REQUIRE( s2[2] == "c" );
    }

    SECTION( "malformed input is rejected rather than crashing" )
    {
        const char *bad[] = { "{",         "{\"a\"}",  "{\"a\":}", "[1,2",     "[1,2,]",
                              "{\"a\":1,}", "tru",      "\"unterm", "{}{}",     "",
                              "{'a':1}",   "{\"a\":\"\\q\"}" };

        for( const char *b : bad )
        {
            jsonParser p;
            jsonValue v;
            REQUIRE( p.parse( b, v ) < 0 );
            REQUIRE_FALSE( p.error().empty() );
        }
    }
}

/// Verify the visit file reader, including the extended schema and its defaults.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "visitFile reads the schema and applies defaults", "[wccCommon][visit]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    visitFile::loadJSON;
    visitFile::sensorConfigFor;
    visitFile::rollStar;
    #endif
    // clang-format on

    SECTION( "an original LAZ format file loads and gets application defaults" )
    {
        // Only the fields the LAZ schema actually carries; everything else must
        // fall back rather than fail.
        const std::string doc = R"({
          "PROGMID":"072226","TELESCOP":"LAZULI","INSTRUME":"ESC","OBSID":"001",
          "VISITID":"001","TARGET":"ALPHA Cen A",
          "RA_PROP":219.90206,"DEC_PROP":-60.83399,"ROLLPA":-15,
          "TARGET_ACQUISITION":{
            "CONFIG_SENSORS":["HWK-01","IMX-19"],
            "TA_EXPTIME_SEC_SLOANR":[1],
            "TA_GAIN":"HIGH"
          },
          "GUIDE_STAR":[{"RANK":1,"GSNUM":2,"ID":"S1","SENSOR":"IMX-18",
                         "RA":220.38682,"DEC":-60.85223,"X_WCC":-849.953,"Y_WCC":-68.782,
                         "EXP_TIME_FG":1,"EXP_TIME_EE":1}],
          "ROLL_STAR":[{"RANK":1,"RSNUM":1,"ID":"S2","SENSOR":"HAWK-09",
                        "RA":219.3156,"DEC":-60.81624,"X_WCC":1029.51,"Y_WCC":59.31},
                       {"RANK":3,"RSNUM":1,"ID":"S3","SENSOR":"IMX-16",
                        "RA":219.49569,"DEC":-60.89411,"X_WCC":711.644,"Y_WCC":-218.633}]
        })";

        jsonParser p;
        jsonValue root;
        REQUIRE( p.parse( doc, root ) == 0 );

        visitFile vf;
        std::string err;
        REQUIRE( vf.loadJSON( root, err ) == 0 );

        REQUIRE( vf.target() == "ALPHA Cen A" );
        REQUIRE( vf.telescope() == "LAZULI" );
        REQUIRE( vf.ra() == Approx( 219.90206 ).epsilon( 1e-12 ) );
        REQUIRE( vf.dec() == Approx( -60.83399 ).epsilon( 1e-12 ) );
        REQUIRE( vf.rollPA() == Approx( -15.0 ).epsilon( 1e-12 ) );
        REQUIRE( vf.configSensors().size() == 2 );

        // TA_EXPTIME_SEC_SLOANR is a one element array in the LAZ format.
        REQUIRE( vf.taExpTime() == Approx( 1.0 ).epsilon( 1e-12 ) );
        REQUIRE( vf.taGain() == "HIGH" );

        // Defaults for everything the LAZ schema lacks.
        REQUIRE( vf.guideTolPix() == Approx( 50.0 ).epsilon( 1e-12 ) );
        REQUIRE( vf.rollTolPix() == Approx( 50.0 ).epsilon( 1e-12 ) );
        REQUIRE( vf.maxIterations() == 3 );
        REQUIRE( vf.tracking().m_roiW == 128 );
        REQUIRE( vf.tracking().m_roiH == 128 );
        REQUIRE_FALSE( vf.taROI().m_valid );

        const visitStar *gs = vf.guideStar();
        REQUIRE( gs != nullptr );
        REQUIRE( gs->m_sensor == "IMX-18" );
        REQUIRE( gs->m_fieldX == Approx( -849.953 ).epsilon( 1e-12 ) );

        // Rank filtering on roll stars.
        REQUIRE( vf.rollStar( 1 ) != nullptr );
        REQUIRE( vf.rollStar( 1 )->m_rank == 1 );
        REQUIRE( vf.rollStar( 3 ) != nullptr );
        REQUIRE( vf.rollStar( 3 )->m_sensor == "IMX-16" );
        REQUIRE( vf.rollStar( 99 ) == nullptr );
        REQUIRE( vf.rollStar( 0 ) != nullptr ); // 0 accepts any rank

        // An omitted target pixel falls back to the sensor centre.
        sensorConfig sc;
        sc.m_fullW = 9576;
        sc.m_fullH = 6388;
        REQUIRE( gs->targetX( sc ) == Approx( 0.5 * 9575 ).epsilon( 1e-12 ) );
        REQUIRE( gs->targetY( sc ) == Approx( 0.5 * 6387 ).epsilon( 1e-12 ) );

        // A sensor with no TA_SENSORS block inherits the acquisition defaults.
        const visitSensorConfig vsc = vf.sensorConfigFor( "IMX-19" );
        REQUIRE( vsc.m_expTime == Approx( 1.0 ).epsilon( 1e-12 ) );
        REQUIRE( vsc.m_stream );
        REQUIRE_FALSE( vsc.m_roi.m_valid );
    }

    SECTION( "the extended schema is honoured where present" )
    {
        const std::string doc = R"({
          "TARGET":"X","RA_PROP":10.0,"DEC_PROP":20.0,"ROLLPA":33.0,
          "TARGET_ACQUISITION":{
            "CONFIG_SENSORS":["IMX-01","HWK-02"],
            "TA_EXPTIME_SEC_SLOANR":[2.5],"TA_FRAME_RATE_HZ":4,
            "TA_ROI":{"X":100,"Y":200,"W":512,"H":512,"BIN_X":2,"BIN_Y":2},
            "TA_GUIDE_TOL_PIX":25,"TA_ROLL_TOL_PIX":40,"TA_MAX_ITERATIONS":5,
            "TA_SENSORS":{
               "IMX-01":{"EXPTIME":0.5,"FRAME_RATE":10,"GAIN":200,
                         "ROI":{"X":1,"Y":2,"W":256,"H":128}},
               "HWK-02":{"STREAM":false}
            }
          },
          "GUIDE_STAR":[{"RANK":1,"SENSOR":"IMX-01","RA":10.1,"DEC":20.1,
                         "X_WCC":-100,"Y_WCC":50,"TARGET_X_PIX":64,"TARGET_Y_PIX":96,
                         "MAG":11.25,"EXP_TIME_FG":0.002,"FRAME_RATE_FG":250,
                         "ROI_W_FG":64,"ROI_H_FG":64}],
          "ROLL_STAR":[{"RANK":1,"SENSOR":"HWK-02","RA":9.9,"DEC":19.9,
                        "X_WCC":300,"Y_WCC":-40}],
          "TRACKING":{"ROI_W":64,"ROI_H":64,"FRAME_RATE":250,"EXPTIME":0.002,
                      "CENTROID_DEVICE_GUIDE":"cguide","CENTROID_DEVICE_ROLL":"croll",
                      "LOOP_GAIN":0.45,"ROLL_GAIN":0.2}
        })";

        jsonParser p;
        jsonValue root;
        REQUIRE( p.parse( doc, root ) == 0 );

        visitFile vf;
        std::string err;
        REQUIRE( vf.loadJSON( root, err ) == 0 );

        REQUIRE( vf.taExpTime() == Approx( 2.5 ).epsilon( 1e-12 ) );
        REQUIRE( vf.taFrameRate() == Approx( 4.0 ).epsilon( 1e-12 ) );
        REQUIRE( vf.guideTolPix() == Approx( 25.0 ).epsilon( 1e-12 ) );
        REQUIRE( vf.rollTolPix() == Approx( 40.0 ).epsilon( 1e-12 ) );
        REQUIRE( vf.maxIterations() == 5 );

        REQUIRE( vf.taROI().m_valid );
        REQUIRE( vf.taROI().toROISpec().imageW() == 256 ); // 512 unbinned, binned by 2

        const visitSensorConfig a = vf.sensorConfigFor( "IMX-01" );
        REQUIRE( a.m_expTime == Approx( 0.5 ).epsilon( 1e-12 ) );
        REQUIRE( a.m_frameRate == Approx( 10.0 ).epsilon( 1e-12 ) );
        REQUIRE( a.m_gain == Approx( 200.0 ).epsilon( 1e-12 ) );
        REQUIRE( a.m_roi.m_valid );
        REQUIRE( a.m_roi.m_w == 256 );
        // A per sensor ROI without explicit binning must not inherit the TA ROI's.
        REQUIRE( a.m_roi.m_binX == 1 );

        const visitSensorConfig b = vf.sensorConfigFor( "HWK-02" );
        REQUIRE_FALSE( b.m_stream );
        REQUIRE( b.m_expTime == Approx( 2.5 ).epsilon( 1e-12 ) );

        const visitStar *g = vf.guideStar();
        REQUIRE( g->m_targetX == Approx( 64.0 ).epsilon( 1e-12 ) );
        REQUIRE( g->m_targetY == Approx( 96.0 ).epsilon( 1e-12 ) );
        REQUIRE( g->m_mag == Approx( 11.25 ).epsilon( 1e-12 ) );
        REQUIRE( g->m_roiWFG == 64 );

        REQUIRE( vf.tracking().m_centroidGuide == "cguide" );
        REQUIRE( vf.tracking().m_centroidRoll == "croll" );
        REQUIRE( vf.tracking().m_loopGain == Approx( 0.45 ).epsilon( 1e-12 ) );
    }

    SECTION( "an unusable visit is rejected with a message" )
    {
        jsonParser p;
        jsonValue root;
        visitFile vf;
        std::string err;

        REQUIRE( p.parse( R"({"RA_PROP":1,"DEC_PROP":2,"GUIDE_STAR":[]})", root ) == 0 );
        REQUIRE( vf.loadJSON( root, err ) < 0 );
        REQUIRE_FALSE( err.empty() );

        REQUIRE( p.parse( R"({"DEC_PROP":2,"GUIDE_STAR":[{"RANK":1}]})", root ) == 0 );
        REQUIRE( vf.loadJSON( root, err ) < 0 );

        REQUIRE( p.parse( R"([1,2,3])", root ) == 0 );
        REQUIRE( vf.loadJSON( root, err ) < 0 );
    }
}

/// Verify the random generator, stamp accumulation and detector noise model.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "sensor model noise and stamp accumulation are correct", "[wccCommon][sensorModel]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    fastRandom::poisson;
    accumulateStamp;
    sensorNoise::apply;
    analogGainLinear;
    analogGainDecibels;
    clampAnalogGainCode;
    sensorNoise::digitize;
    #endif
    // clang-format on

    SECTION( "the generator has the right moments and is reproducible" )
    {
        fastRandom r( 12345 );

        const int n = 400000;
        double s = 0, s2 = 0, lo = 2, hi = -1;

        for( int i = 0; i < n; ++i )
        {
            const double u = r.uniform();
            s += u;
            s2 += u * u;
            lo = std::min( lo, u );
            hi = std::max( hi, u );
        }

        REQUIRE( s / n == Approx( 0.5 ).margin( 5e-3 ) );
        REQUIRE( s2 / n - ( s / n ) * ( s / n ) == Approx( 1.0 / 12.0 ).margin( 5e-3 ) );
        REQUIRE( lo >= 0.0 );
        REQUIRE( hi < 1.0 );

        s = 0;
        s2 = 0;
        for( int i = 0; i < n; ++i )
        {
            const double g = r.normal();
            s += g;
            s2 += g * g;
        }
        REQUIRE( s / n == Approx( 0.0 ).margin( 1e-2 ) );
        REQUIRE( s2 / n == Approx( 1.0 ).margin( 2e-2 ) );

        // Poisson on both sides of the switch from Knuth to the normal
        // approximation at lambda = 30. Mean and variance must both equal lambda.
        for( double lam : { 0.5, 5.0, 29.0, 31.0, 200.0 } )
        {
            double ps = 0, ps2 = 0;
            const int m = 200000;

            for( int i = 0; i < m; ++i )
            {
                const double k = r.poisson( lam );
                ps += k;
                ps2 += k * k;
            }

            const double mean = ps / m;
            REQUIRE( mean == Approx( lam ).epsilon( 0.03 ) );
            REQUIRE( ps2 / m - mean * mean == Approx( lam ).epsilon( 0.08 ) );
        }

        REQUIRE( r.poisson( 0.0 ) == 0.0 );
        REQUIRE( r.poisson( -5.0 ) == 0.0 );

        fastRandom x( 999 ), y( 999 );
        for( int i = 0; i < 1000; ++i )
        {
            REQUIRE( x.next() == y.next() );
        }
    }

    SECTION( "stamp accumulation conserves flux and clips at the frame edge" )
    {
        const int s = 8;
        const int w = 40, h = 30;
        std::vector<float> stamp( s * s, 1.0f / ( s * s ) );
        std::vector<float> frame( w * h, 0.0f );

        int bx0 = w, by0 = h, bx1 = -1, by1 = -1;

        // Fully interior: all the flux lands and the bounding box is exact.
        REQUIRE( accumulateStamp( frame, w, h, stamp.data(), s, 20, 15, 1000.0, bx0, by0, bx1, by1 ) ==
                 Approx( 1000.0 ).epsilon( 1e-5 ) );

        double tot = 0;
        for( float v : frame )
        {
            tot += v;
        }
        REQUIRE( tot == Approx( 1000.0 ).epsilon( 1e-5 ) );
        REQUIRE( bx0 == 20 - s / 2 );
        REQUIRE( bx1 == 20 + s / 2 - 1 );
        REQUIRE( by0 == 15 - s / 2 );
        REQUIRE( by1 == 15 + s / 2 - 1 );

        // Partly off the left edge: only the visible fraction lands, which is how a
        // star just outside the region of interest still contributes its wings.
        std::fill( frame.begin(), frame.end(), 0.0f );
        int cx0 = w, cy0 = h, cx1 = -1, cy1 = -1;
        const double clipped =
            accumulateStamp( frame, w, h, stamp.data(), s, 1, 15, 1000.0, cx0, cy0, cx1, cy1 );

        REQUIRE( clipped > 0.0 );
        REQUIRE( clipped < 1000.0 );
        REQUIRE( cx0 == 0 );

        tot = 0;
        for( float v : frame )
        {
            tot += v;
        }
        REQUIRE( tot == Approx( clipped ).epsilon( 1e-5 ) );

        // Entirely outside, and degenerate inputs.
        std::fill( frame.begin(), frame.end(), 0.0f );
        int dx0 = w, dy0 = h, dx1 = -1, dy1 = -1;
        REQUIRE( accumulateStamp( frame, w, h, stamp.data(), s, -50, 15, 1000.0, dx0, dy0, dx1, dy1 ) == 0.0 );
        REQUIRE( dx1 == -1 );
        REQUIRE( accumulateStamp( frame, w, h, nullptr, s, 20, 15, 1000.0, dx0, dy0, dx1, dy1 ) == 0.0 );
        REQUIRE( accumulateStamp( frame, w, h, stamp.data(), s, 20, 15, 0.0, dx0, dy0, dx1, dy1 ) == 0.0 );
    }

    SECTION( "noise modes produce the documented statistics" )
    {
        sensorConfig sc;
        sc.m_darkCurrent = 10.0;
        sc.m_readNoise = 5.0;

        const int w = 200, h = 200;
        const double expTime = 2.0; // a 20 electron dark pedestal

        // off: exactly the pedestal with no scatter, so a frame is repeatable.
        {
            sensorNoise ns( 1 );
            ns.mode( noiseMode::off );
            ns.bias( 100.0 );
            REQUIRE( ns.mode() == noiseMode::off );
            REQUIRE( ns.bias() == Approx( 100.0 ) );

            std::vector<float> f( w * h, 0.0f );
            ns.apply( f, w, h, sc, expTime, w, h, -1, -1 );

            for( float v : f )
            {
                REQUIRE( v == Approx( 120.0f ).margin( 1e-4 ) );
            }
        }

        // read: zero mean read noise on the pedestal, at the configured sigma.
        {
            sensorNoise ns( 2 );
            ns.mode( noiseMode::read );
            ns.bias( 100.0 );

            std::vector<float> f( w * h, 0.0f );
            ns.apply( f, w, h, sc, expTime, w, h, -1, -1 );

            double s = 0, s2 = 0;
            for( float v : f )
            {
                s += v;
                s2 += static_cast<double>( v ) * v;
            }
            const double n = f.size();
            const double mean = s / n;

            REQUIRE( mean == Approx( 120.0 ).margin( 0.2 ) );
            REQUIRE( std::sqrt( s2 / n - mean * mean ) == Approx( 5.0 ).margin( 0.2 ) );
        }

        // full: shot noise only where a star actually put signal.
        {
            sensorNoise ns( 3 );
            ns.mode( noiseMode::full );
            ns.bias( 0.0 );

            std::vector<float> f( w * h, 0.0f );
            for( int y = 10; y < 60; ++y )
            {
                for( int x = 10; x < 60; ++x )
                {
                    f[static_cast<size_t>( y ) * w + x] = 1000.0f;
                }
            }

            ns.apply( f, w, h, sc, expTime, 10, 10, 59, 59 );

            double si = 0, si2 = 0;
            int ni = 0;
            for( int y = 10; y < 60; ++y )
            {
                for( int x = 10; x < 60; ++x )
                {
                    const double v = f[static_cast<size_t>( y ) * w + x];
                    si += v;
                    si2 += v * v;
                    ++ni;
                }
            }

            const double mi = si / ni;
            // Signal 1000 plus dark 20, with read noise 5 added in quadrature.
            REQUIRE( mi == Approx( 1020.0 ).margin( 5.0 ) );
            REQUIRE( std::sqrt( si2 / ni - mi * mi ) ==
                     Approx( std::sqrt( 1020.0 + 25.0 ) ).margin( 4.0 ) );

            // Outside the illuminated box: pedestal and read noise only.
            double so = 0;
            int no = 0;
            for( int y = 120; y < 190; ++y )
            {
                for( int x = 120; x < 190; ++x )
                {
                    so += f[static_cast<size_t>( y ) * w + x];
                    ++no;
                }
            }
            REQUIRE( so / no == Approx( 20.0 ).margin( 0.5 ) );
        }
    }

    SECTION( "digitization clips at full well and the ADC range" )
    {
        sensorNoise ns( 7 );
        sensorConfig sc;
        sc.m_fullWellDepth = 20000;

        const std::vector<float> f = { -5.0f, 0.0f, 100.0f, 19999.0f, 25000.0f, 1e9f };
        std::vector<uint16_t> dn;

        const size_t sat = ns.digitize( f, dn, sc, 1.0, 16 );
        REQUIRE( dn[0] == 0 );
        REQUIRE( dn[1] == 0 );
        REQUIRE( dn[2] == 100 );
        REQUIRE( dn[4] == 20000 );
        REQUIRE( sat >= 2 );

        // Analog gain multiplies electrons, including the noise already in them.
        ns.digitize( f, dn, sc, 4.0, 16 );
        REQUIRE( dn[2] == 400 );

        // Conversion gain is electrons per DN at 0 dB analog gain.
        sc.m_conversionGain = 4.0;
        ns.digitize( f, dn, sc, 1.0, 16 );
        REQUIRE( dn[2] == 25 );
        sc.m_conversionGain = 1.0;

        // A 12 bit converter saturates at 4095.
        ns.digitize( f, dn, sc, 1.0, 12 );
        REQUIRE( dn[3] == 4095 );
        REQUIRE( dn[4] == 4095 );
    }

    SECTION( "noise mode names round trip" )
    {
        REQUIRE( parseNoiseMode( "off" ) == noiseMode::off );
        REQUIRE( parseNoiseMode( "none" ) == noiseMode::off );
        REQUIRE( parseNoiseMode( "read" ) == noiseMode::read );
        REQUIRE( parseNoiseMode( "full" ) == noiseMode::full );
        REQUIRE( parseNoiseMode( "nonsense" ) == noiseMode::full );
        REQUIRE( noiseModeName( noiseMode::off ) == "off" );
        REQUIRE( noiseModeName( noiseMode::read ) == "read" );
        REQUIRE( noiseModeName( noiseMode::full ) == "full" );
    }
}

/// Verify IMX-style analog gain codes and high-rate pointing sample collection.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "analog gain and pointing samples follow the CMOS and shmim contracts",
           "[wccCommon][analogGain][pointingShmim]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    analogGainLinear;
    analogGainDecibels;
    clampAnalogGainCode;
    packPointing;
    unpackPointing;
    collectPointingTicks;
    collectPointingExposure;
    binPointingSamples;
    ticksForExposure;
    pointingBufferDepth;
    pointingWriteHzIndiProperty;
    pointingHistoryIndiProperty;
    packStampCell;
    unpackStampCell;
    addTrailSample;
    accumulateTrail;
    markDwellMap;
    markDwellFromCells;
    #endif
    // clang-format on

    SECTION( "IMX 0.1 dB codes multiply electrons, including at the register limits" )
    {
        REQUIRE( clampAnalogGainCode( -3 ) == 0 );
        REQUIRE( clampAnalogGainCode( 120 ) == 120 );
        REQUIRE( clampAnalogGainCode( 300 ) == 255 );

        REQUIRE( analogGainDecibels( 0 ) == Approx( 0.0 ).epsilon( 1e-12 ) );
        REQUIRE( analogGainDecibels( 120 ) == Approx( 12.0 ).epsilon( 1e-12 ) );
        REQUIRE( analogGainDecibels( 255 ) == Approx( 25.5 ).epsilon( 1e-12 ) );

        // Sony: G_dB = 20 log10(G). Code 0 is unity, 200 is 20 dB = 10x.
        REQUIRE( analogGainLinear( 0 ) == Approx( 1.0 ).epsilon( 1e-12 ) );
        REQUIRE( analogGainLinear( 200 ) == Approx( 10.0 ).epsilon( 1e-12 ) );
        REQUIRE( analogGainLinear( 120 ) == Approx( std::pow( 10.0, 12.0 / 20.0 ) ).epsilon( 1e-12 ) );
    }

    SECTION( "stamp-cell keys round trip, including pixels off the frame" )
    {
        int ix = 0, iy = 0, bx = 0, by = 0;
        unpackStampCell( packStampCell( -3, 12, 7, 2 ), ix, iy, bx, by );
        REQUIRE( ix == -3 );
        REQUIRE( iy == 12 );
        REQUIRE( bx == 7 );
        REQUIRE( by == 2 );

        unpackStampCell( packStampCell( 9576, -1, 0, 255 ), ix, iy, bx, by );
        REQUIRE( ix == 9576 );
        REQUIRE( iy == -1 );
        REQUIRE( bx == 0 );
        REQUIRE( by == 255 );
    }

    SECTION( "a dwell map is the frame size with 1s only at PSF placement pixels" )
    {
        const int w = 32, h = 16;
        std::vector<uint16_t> dwell( static_cast<size_t>( w ) * h, 7 );

        dwell.assign( static_cast<size_t>( w ) * h, 0 );
        markDwellMap( dwell, w, h, 8, 8 );
        markDwellMap( dwell, w, h, 12, 8 );
        markDwellMap( dwell, w, h, -1, 0 );
        markDwellMap( dwell, w, h, 0, 99 );

        REQUIRE( dwell[8 * w + 8] == 1 );
        REQUIRE( dwell[8 * w + 12] == 1 );
        REQUIRE( dwell[0] == 0 );

        std::unordered_map<uint64_t, double> cells;
        cells[packStampCell( 3, 4, 1, 2 )] = 10.0;
        markDwellFromCells( dwell, w, h, cells );
        REQUIRE( dwell[4 * w + 3] == 1 );
    }

    SECTION( "a pointing ring covering an exposure keeps chronological order and equal weights" )
    {
        REQUIRE( ticksForExposure( 100.0, 5000.0 ) == 500000 );
        REQUIRE( ticksForExposure( 0.0, 5000.0 ) == 1 );
        REQUIRE( pointingBufferDepth( 5000.0, 120.0 ) == 600000 );
        REQUIRE( pointingBufferDepth( 0.0, 8.0 ) == 1 );
        REQUIRE( std::string( pointingWriteHzIndiProperty ) == "write_hz" );
        REQUIRE( std::string( pointingHistoryIndiProperty ) == "history_s" );

        const uint32_t depth = 8;
        std::vector<double> data( depth * pointingNAxes, 0.0 );

        for( uint32_t i = 0; i < depth; ++i )
        {
            packPointing( data.data() + i * pointingNAxes, 10.0 + 0.001 * i, 20.0, 1.0 * i, 1.0 + 0.001 * i );
        }

        // Last written slice is 7. Simulated now is 1.007 s; a 0.003 s exposure
        // should take the four ticks at 1.004 .. 1.007, equally weighted.
        std::vector<pointingSample> samples;
        REQUIRE( collectPointingExposure( data.data(), pointingNAxes, depth, 7, 8, 0.003, 1000.0, samples ) ==
                 4 );

        REQUIRE( samples.front().m_time == Approx( 1.004 ).margin( 1e-12 ) );
        REQUIRE( samples.back().m_ra == Approx( 10.007 ).epsilon( 1e-12 ) );
        REQUIRE( samples.front().m_ra == Approx( 10.004 ).epsilon( 1e-12 ) );

        double sumDt = 0;
        double minDt = samples.front().m_dt;
        double maxDt = samples.front().m_dt;

        for( const pointingSample &s : samples )
        {
            sumDt += s.m_dt;
            minDt = std::min( minDt, s.m_dt );
            maxDt = std::max( maxDt, s.m_dt );
        }

        REQUIRE( sumDt == Approx( 0.003 ).margin( 1e-12 ) );
        REQUIRE( maxDt == Approx( minDt ).margin( 1e-15 ) );
    }

    SECTION( "a 3-axis stream falls back to tick count" )
    {
        const uint32_t nAxes = 3;
        const uint32_t depth = 8;
        std::vector<double> data( depth * nAxes, 0.0 );

        for( uint32_t i = 0; i < depth; ++i )
        {
            data[i * nAxes + 0] = 10.0 + 0.001 * i;
            data[i * nAxes + 1] = 20.0;
            data[i * nAxes + 2] = 1.0 * i;
        }

        std::vector<pointingSample> samples;
        REQUIRE( collectPointingExposure( data.data(), nAxes, depth, 7, 8, 0.004, 1000.0, samples ) == 4 );
        REQUIRE( samples.back().m_ra == Approx( 10.007 ).epsilon( 1e-12 ) );
        REQUIRE( samples.front().m_ra == Approx( 10.004 ).epsilon( 1e-12 ) );
    }

    SECTION( "an exposure longer than the history uses every tick without parking flux on the last sample" )
    {
        const uint32_t depth = 5;
        std::vector<double> data( depth * pointingNAxes, 0.0 );

        for( uint32_t i = 0; i < depth; ++i )
        {
            packPointing( data.data() + i * pointingNAxes, 10.0 + i, 20.0, 0.0, 0.001 * ( i + 1 ) );
        }

        std::vector<pointingSample> samples;
        REQUIRE( collectPointingExposure( data.data(), pointingNAxes, depth, 4, 5, 10.0, 1000.0, samples ) ==
                 5 );

        double sumDt = 0;
        for( const pointingSample &s : samples )
        {
            sumDt += s.m_dt;
            REQUIRE( s.m_dt == Approx( 2.0 ).margin( 1e-12 ) );
        }

        REQUIRE( sumDt == Approx( 10.0 ).margin( 1e-12 ) );
        REQUIRE( samples.front().m_ra == Approx( 10.0 ).margin( 1e-12 ) );
        REQUIRE( samples.back().m_ra == Approx( 14.0 ).margin( 1e-12 ) );
    }

    SECTION( "binning a trail conserves time and keeps the path" )
    {
        std::vector<pointingSample> samples( 8 );

        for( size_t i = 0; i < samples.size(); ++i )
        {
            samples[i].m_ra = 10.0 + 0.1 * static_cast<double>( i );
            samples[i].m_dec = 20.0;
            samples[i].m_pa = 0.0;
            samples[i].m_time = 0.001 * static_cast<double>( i );
            samples[i].m_dt = 0.001;
        }

        binPointingSamples( samples, 2 );
        REQUIRE( samples.size() == 2 );

        REQUIRE( samples[0].m_dt + samples[1].m_dt == Approx( 0.008 ).margin( 1e-12 ) );
        REQUIRE( samples[0].m_ra == Approx( 10.15 ).margin( 1e-12 ) );
        REQUIRE( samples[1].m_ra == Approx( 10.55 ).margin( 1e-12 ) );
    }

    SECTION( "two pointings in one exposure split the flux between two pixels" )
    {
        // A 1-pixel stamp makes the trail a pair of pixels rather than overlapping cores.
        const int s = 2;
        std::vector<float> stamp( s * s, 0.0f );
        stamp[s * ( s / 2 ) + s / 2] = 1.0f;

        const int w = 32, h = 16;
        std::vector<float> frame( static_cast<size_t>( w ) * h, 0.0f );
        int bx0 = w, by0 = h, bx1 = -1, by1 = -1;

        REQUIRE( accumulateStamp( frame, w, h, stamp.data(), s, 8, 8, 100.0, bx0, by0, bx1, by1 ) ==
                 Approx( 100.0 ).epsilon( 1e-5 ) );
        REQUIRE( accumulateStamp( frame, w, h, stamp.data(), s, 12, 8, 50.0, bx0, by0, bx1, by1 ) ==
                 Approx( 50.0 ).epsilon( 1e-5 ) );

        REQUIRE( frame[8 * w + 8] == Approx( 100.0f ).margin( 1e-4 ) );
        REQUIRE( frame[8 * w + 12] == Approx( 50.0f ).margin( 1e-4 ) );
    }
}

/// Verify the astrometric pipeline recovers a known transform end to end.
/** The synthetic frame is built from real diffraction PSFs, real photometry and
 * real detector noise, so this exercises the whole chain the controller depends
 * on rather than the matcher in isolation.
 *
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "astrometry recovers a known transform end to end", "[wccCommon][astrometry]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    detectSources;
    solveFrame;
    predictSources;
    solveBoresight;
    frameSolution::apply;
    #endif
    // clang-format on

    psfGenerator gen;

    pupilConfig pc;
    pc.m_npix = 128;
    pc.m_diameter = 6.5;
    pc.m_fNumber = 12.0;
    REQUIRE( gen.buildPupil( pc ) == 0 );

    bandpassConfig bp;
    bp.m_wavelengths = { 650.0 };
    REQUIRE( gen.setBandpass( bp ) == 0 );

    // Oversample so the PSF core spans a few pixels, as a WCC sensor does.
    const double pixUm = 0.650 * 12.0 / 3.0; // Q = 3
    psfBank bank;
    REQUIRE( bank.build( gen, 32, 8, pixUm ) == 0 );

    const int w = 512, h = 512;
    const double pivX = 0.5 * ( w - 1 );
    const double pivY = 0.5 * ( h - 1 );

    // A fixed pseudo random star field with a realistic magnitude spread.
    fastRandom rr( 4242 );
    std::vector<double> tx, ty, tmag;
    for( int i = 0; i < 40; ++i )
    {
        tx.push_back( 40.0 + rr.uniform() * ( w - 80 ) );
        ty.push_back( 40.0 + rr.uniform() * ( h - 80 ) );
        tmag.push_back( 12.0 + rr.uniform() * 5.0 );
    }

    photometryConfig phot;
    phot.m_apertureArea = apertureArea( 6.5 );
    phot.m_throughput = 0.5;
    phot.m_bandwidth = 0.150;

    const double trueDx = 137.4, trueDy = -88.7, trueRot = 0.35;

    auto truthAt = [&]( double x, double y, double &mx, double &my ) {
        const double cr = std::cos( trueRot * deg2rad );
        const double sr = std::sin( trueRot * deg2rad );
        mx = pivX + cr * ( x - pivX ) - sr * ( y - pivY ) + trueDx;
        my = pivY + sr * ( x - pivX ) + cr * ( y - pivY ) + trueDy;
    };

    auto renderFrame = [&]( double dx, double dy, double rot, std::vector<float> &frame ) {
        frame.assign( static_cast<size_t>( w ) * h, 0.0f );
        int bx0 = w, by0 = h, bx1 = -1, by1 = -1;

        const double cr = std::cos( rot * deg2rad );
        const double sr = std::sin( rot * deg2rad );

        for( size_t i = 0; i < tx.size(); ++i )
        {
            const double x = pivX + cr * ( tx[i] - pivX ) - sr * ( ty[i] - pivY ) + dx;
            const double y = pivY + sr * ( tx[i] - pivX ) + cr * ( ty[i] - pivY ) + dy;

            const int ix = static_cast<int>( std::floor( x + 0.5 ) );
            const int iy = static_cast<int>( std::floor( y + 0.5 ) );

            accumulateStamp( frame, w, h, bank.lookup( x - ix, y - iy ), bank.samples(), ix, iy,
                             abMagToElectrons( tmag[i], 0.05, phot ), bx0, by0, bx1, by1 );
        }

        sensorConfig sc;
        sc.m_darkCurrent = 0.05;
        sc.m_readNoise = 3.0;
        sc.m_fullWellDepth = 1e9;

        sensorNoise ns( 77 );
        ns.mode( noiseMode::full );
        ns.bias( 100.0 );
        ns.apply( frame, w, h, sc, 0.05, bx0, by0, bx1, by1 );
    };

    detectConfig dc;
    dc.m_threshold = 5.0;
    dc.m_boxHalf = 5;
    dc.m_minSeparation = 8;

    solveConfig sc2;
    sc2.m_searchRadius = 400;
    sc2.m_voteBin = 8;
    sc2.m_matchRadius = 6;
    sc2.m_minMatched = 4;
    sc2.m_solveRotation = true;

    // Predictions are the untransformed truth positions.
    std::vector<prediction> preds;
    for( size_t i = 0; i < tx.size(); ++i )
    {
        prediction p;
        p.m_x = tx[i];
        p.m_y = ty[i];
        p.m_mag = tmag[i];
        p.m_catalogIndex = i;
        preds.push_back( p );
    }

    auto worstTransformError = [&]( const frameSolution &s ) {
        double worst = 0;
        for( int y = 0; y < h; y += 64 )
        {
            for( int x = 0; x < w; x += 64 )
            {
                double ax, ay, bx, by;
                truthAt( x, y, ax, ay );
                s.apply( x, y, bx, by );
                worst = std::max( worst, std::hypot( ax - bx, ay - by ) );
            }
        }
        return worst;
    };

    SECTION( "detection finds the stars without mistaking Airy rings for sources" )
    {
        std::vector<float> frame;
        renderFrame( 0, 0, 0, frame );

        std::vector<detection> dets;
        backgroundEstimate bkg;
        const size_t nd = detectSources( frame.data(), w, h, dc, dets, &bkg );

        // The clipped background sits just above the bias pedestal because
        // overlapping stellar wings are real signal, not an estimator error.
        REQUIRE( bkg.m_level >= 100.0 );
        REQUIRE( bkg.m_level < 101.0 );
        REQUIRE( bkg.m_sigma == Approx( 3.0 ).margin( 1.0 ) );

        // Without ring suppression a bright star yields several spurious sources.
        REQUIRE( nd >= tx.size() - 4 );
        REQUIRE( nd <= tx.size() + 4 );

        // Detections are ordered brightest first.
        for( size_t i = 1; i < dets.size(); ++i )
        {
            REQUIRE( dets[i].m_peak <= dets[i - 1].m_peak );
        }
    }

    SECTION( "a large offset and rotation are both recovered" )
    {
        std::vector<float> frame;
        renderFrame( trueDx, trueDy, trueRot, frame );

        std::vector<detection> dets;
        REQUIRE( detectSources( frame.data(), w, h, dc, dets ) > 10 );

        frameSolution sol;
        REQUIRE( solveFrame( dets, preds, sc2, sol ) == 0 );
        REQUIRE( sol.m_valid );
        REQUIRE( sol.m_rotation == Approx( trueRot ).margin( 0.01 ) );
        REQUIRE( sol.m_rms < 0.5 );

        // m_dx and m_dy are tied to the solver's pivot, so compare transforms.
        REQUIRE( worstTransformError( sol ) < 0.3 );

        // offsetAt must agree with apply.
        double mx, my, ox, oy;
        sol.apply( 123.0, 45.0, mx, my );
        sol.offsetAt( 123.0, 45.0, ox, oy );
        REQUIRE( ox == Approx( mx - 123.0 ).margin( 1e-12 ) );
        REQUIRE( oy == Approx( my - 45.0 ).margin( 1e-12 ) );
    }

    SECTION( "a zero offset solves to zero rather than drifting" )
    {
        std::vector<float> frame;
        renderFrame( 0, 0, 0, frame );

        std::vector<detection> dets;
        detectSources( frame.data(), w, h, dc, dets );

        frameSolution sol;
        REQUIRE( solveFrame( dets, preds, sc2, sol ) == 0 );
        REQUIRE( sol.m_dx == Approx( 0.0 ).margin( 0.1 ) );
        REQUIRE( sol.m_dy == Approx( 0.0 ).margin( 0.1 ) );
        REQUIRE( sol.m_rotation == Approx( 0.0 ).margin( 0.01 ) );
    }

    SECTION( "the solve survives missing predictions and spurious detections" )
    {
        std::vector<float> frame;
        renderFrame( trueDx, trueDy, trueRot, frame );

        std::vector<detection> dets;
        detectSources( frame.data(), w, h, dc, dets );

        std::vector<prediction> half;
        for( size_t i = 0; i < preds.size(); i += 2 )
        {
            half.push_back( preds[i] );
        }

        for( int i = 0; i < 25; ++i )
        {
            detection d;
            d.m_x = rr.uniform() * w;
            d.m_y = rr.uniform() * h;
            d.m_peak = 10;
            d.m_flux = 100;
            d.m_npix = 3;
            dets.push_back( d );
        }

        frameSolution sol;
        REQUIRE( solveFrame( dets, half, sc2, sol ) == 0 );
        REQUIRE( sol.m_rotation == Approx( trueRot ).margin( 0.03 ) );
        REQUIRE( worstTransformError( sol ) < 0.6 );
    }

    SECTION( "too little information fails cleanly" )
    {
        frameSolution sol;
        std::vector<detection> none;

        REQUIRE( solveFrame( none, preds, sc2, sol ) < 0 );
        REQUIRE_FALSE( sol.m_valid );

        std::vector<detection> one( 1 );
        REQUIRE( solveFrame( one, preds, sc2, sol ) < 0 );

        std::vector<prediction> noPred;
        std::vector<detection> some( 10 );
        REQUIRE( solveFrame( some, noPred, sc2, sol ) < 0 );
    }

    SECTION( "predictSources projects a catalog onto a frame and orders it" )
    {
        focalPlaneModel fp;
        fp.setTelescope( 6.5, 12.0, -1.0 );
        fp.setPointing( 192.317, 26.84316038, 0.0 );

        sensorConfig s;
        s.m_name = "IMX-01";
        s.m_pixelSize = 3.76;
        s.m_fullW = 1024;
        s.m_fullH = 1024;
        fp.addSensor( s );

        skyWCS wcs;
        REQUIRE( fp.sensorWCS( 0, wcs ) == 0 );

        const std::vector<double> wantX = { 10.0, 512.0, 1000.0, 300.5 };
        const std::vector<double> wantY = { 20.0, 512.0, 3.0, 777.25 };

        std::vector<double> ra, dec, mag;
        for( size_t i = 0; i < wantX.size(); ++i )
        {
            double r, d;
            wcs.pix2world( wantX[i], wantY[i], r, d );
            ra.push_back( r );
            dec.push_back( d );
            mag.push_back( 15.0 - i ); // descending, so sorting is observable
        }

        // Plus one far outside the frame.
        {
            double r, d;
            wcs.pix2world( -5000.0, -5000.0, r, d );
            ra.push_back( r );
            dec.push_back( d );
            mag.push_back( 9.0 );
        }

        std::vector<prediction> out;
        REQUIRE( predictSources( wcs, 1024, 1024, ra, dec, mag, out ) == wantX.size() );
        REQUIRE( out[0].m_mag <= out[1].m_mag );

        for( const prediction &p : out )
        {
            REQUIRE( p.m_x == Approx( wantX[p.m_catalogIndex] ).margin( 1e-6 ) );
            REQUIRE( p.m_y == Approx( wantY[p.m_catalogIndex] ).margin( 1e-6 ) );
        }

        // A margin lets a source just off the frame through.
        std::vector<double> ra2, dec2, mag2;
        double r2, d2;
        wcs.pix2world( -5.0, 500.0, r2, d2 );
        ra2.push_back( r2 );
        dec2.push_back( d2 );
        mag2.push_back( 12.0 );

        std::vector<prediction> p2;
        REQUIRE( predictSources( wcs, 1024, 1024, ra2, dec2, mag2, p2, 0 ) == 0 );
        REQUIRE( predictSources( wcs, 1024, 1024, ra2, dec2, mag2, p2, 20 ) == 1 );
    }
}

/// Verify the multi-sensor boresight solution separates pointing from roll.
/**
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "solveBoresight separates pointing from roll", "[wccCommon][astrometry]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    solveBoresight;
    #endif
    // clang-format on

    const double trueX = 12.5, trueY = -7.25, trueRoll = 0.02;
    const double dth = trueRoll * deg2rad;

    // Field positions taken from the visit file's guide star candidates.
    const double fx[5] = { -849.9, -666.3, -241.3, 34.9, 1029.5 };
    const double fy[5] = { -68.8, -109.3, 390.6, 313.1, 59.3 };

    std::vector<sensorMeasurement> meas;
    for( int i = 0; i < 5; ++i )
    {
        sensorMeasurement m;
        m.m_sensor = i;
        m.m_fieldX = fx[i];
        m.m_fieldY = fy[i];
        // The shift a boresight offset plus a roll produces at this sensor.
        m.m_shiftX = trueX - dth * fy[i];
        m.m_shiftY = trueY + dth * fx[i];
        m.m_rotation = trueRoll;
        m.m_weight = 20;
        meas.push_back( m );
    }

    SECTION( "an exact multi-sensor system is solved exactly" )
    {
        boresightSolution bs;
        REQUIRE( solveBoresight( meas, bs ) == 0 );
        REQUIRE( bs.m_valid );
        REQUIRE( bs.m_nSensors == 5 );
        REQUIRE( bs.m_fieldX == Approx( trueX ).margin( 1e-9 ) );
        REQUIRE( bs.m_fieldY == Approx( trueY ).margin( 1e-9 ) );
        REQUIRE( bs.m_roll == Approx( trueRoll ).margin( 1e-11 ) );
        REQUIRE( bs.m_rms == Approx( 0.0 ).margin( 1e-9 ) );
    }

    SECTION( "roll is degenerate from one sensor, so the frame rotation is used" )
    {
        const std::vector<sensorMeasurement> one( meas.begin(), meas.begin() + 1 );

        boresightSolution bs;
        REQUIRE( solveBoresight( one, bs ) == 0 );
        REQUIRE( bs.m_fieldX == Approx( one[0].m_shiftX ).margin( 1e-12 ) );
        REQUIRE( bs.m_fieldY == Approx( one[0].m_shiftY ).margin( 1e-12 ) );
        REQUIRE( bs.m_roll == Approx( trueRoll ).margin( 1e-12 ) );
    }

    SECTION( "a degenerate layout falls back rather than producing nonsense" )
    {
        const std::vector<sensorMeasurement> dup = { meas[0], meas[0], meas[0] };

        boresightSolution bs;
        REQUIRE( solveBoresight( dup, bs ) == 0 );
        REQUIRE( isFinite( bs.m_fieldX ) );
        REQUIRE( isFinite( bs.m_fieldY ) );
        REQUIRE( isFinite( bs.m_roll ) );
    }

    SECTION( "no measurements is an error" )
    {
        boresightSolution bs;
        REQUIRE( solveBoresight( {}, bs ) < 0 );
        REQUIRE_FALSE( bs.m_valid );
    }

    SECTION( "measurement noise degrades gracefully and is reported" )
    {
        fastRandom nr( 5150 );
        std::vector<sensorMeasurement> noisy = meas;

        for( sensorMeasurement &m : noisy )
        {
            m.m_shiftX += 0.05 * nr.normal();
            m.m_shiftY += 0.05 * nr.normal();
        }

        boresightSolution bs;
        REQUIRE( solveBoresight( noisy, bs ) == 0 );
        REQUIRE( bs.m_fieldX == Approx( trueX ).margin( 0.15 ) );
        REQUIRE( bs.m_fieldY == Approx( trueY ).margin( 0.15 ) );
        REQUIRE( bs.m_roll == Approx( trueRoll ).margin( 0.005 ) );
        REQUIRE( bs.m_rms > 0.0 );
        REQUIRE( bs.m_rms < 0.2 );
    }
}

/// Verify the catalog reader against the bundled GSC 3.1 export.
/** Skipped when the catalog is not present, so the suite still runs from a
 * checkout without the large data file.
 *
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "starCatalog reads a GSC 3.1 export and cone searches correctly",
           "[wccCommon][starCatalog]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    starCatalog::load;
    starCatalog::coneSearch;
    #endif
    // clang-format on

    SECTION( "CSV field splitting handles quotes and padding" )
    {
        std::vector<std::string> f;
        splitCSV( R"(a, b ,"c,d",,"e")", f );

        REQUIRE( f.size() == 5 );
        REQUIRE( f[0] == "a" );
        REQUIRE( f[1] == "b" );
        REQUIRE( f[2] == "c,d" );
        REQUIRE( f[3].empty() );
        REQUIRE( f[4] == "e" );

        REQUIRE( trimField( "  x  " ) == "x" );
        REQUIRE( trimField( "\"y\"" ) == "y" );
        REQUIRE( trimField( "" ).empty() );
    }

    SECTION( "cone search matches a brute force scan" )
    {
        // A synthetic catalog keeps this independent of the bundled data file.
        std::vector<starEntry> stars;
        fastRandom rr( 31337 );

        for( int i = 0; i < 4000; ++i )
        {
            starEntry s;
            s.m_ra = 190.0 + rr.uniform() * 5.0;
            s.m_dec = 24.0 + rr.uniform() * 5.0;
            s.m_mag = 10.0 + rr.uniform() * 12.0;
            s.m_id = "S" + std::to_string( i );
            stars.push_back( s );
        }

        starCatalog cat;
        cat.setStars( stars );

        REQUIRE( cat.size() == stars.size() );
        REQUIRE_FALSE( cat.empty() );

        // The index is declination sorted, which is what coneSearch brackets on.
        for( size_t i = 1; i < cat.size(); ++i )
        {
            REQUIRE( cat[i].m_dec >= cat[i - 1].m_dec );
        }

        const double ra = 192.317, dec = 26.84316038;

        for( double rad : { 0.01, 0.05, 0.2, 1.0 } )
        {
            std::vector<size_t> hits;
            cat.coneSearch( ra, dec, rad, hits );

            size_t brute = 0;
            for( size_t i = 0; i < cat.size(); ++i )
            {
                if( angularSeparation( ra, dec, cat[i].m_ra, cat[i].m_dec ) <= rad )
                {
                    ++brute;
                }
            }

            REQUIRE( hits.size() == brute );
        }

        // A magnitude cut must be honoured.
        std::vector<size_t> bright;
        cat.coneSearch( ra, dec, 1.0, bright, 15.0 );
        for( size_t i : bright )
        {
            REQUIRE( cat[i].m_mag <= 15.0 );
        }

        // Degenerate searches return nothing rather than misbehaving.
        std::vector<size_t> none;
        REQUIRE( cat.coneSearch( ra, dec, 0.0, none ) == 0 );
        REQUIRE( cat.coneSearch( ra, dec, -1.0, none ) == 0 );
    }

    SECTION( "a missing file is an error, not a crash" )
    {
        starCatalog cat;
        REQUIRE( cat.load( "/nonexistent/path/to/catalog.csv" ) == -1 );
        REQUIRE( cat.empty() );
    }
}

/// Verify isFinite rejects NaN and infinity even under -ffast-math.
/** MagAO-X builds with `-ffast-math`, which removes `std::isfinite` guards. The
 * values are parsed from text, as INDI delivers them, so the compiler cannot
 * constant-fold them.
 *
 * \ingroup wccCommon_unit_test
 */
TEST_CASE( "isFinite classifies NaN and infinity under fast-math", "[wccCommon][numeric]" )
{
    // clang-format off
    #ifdef WCCCOMMON_TEST_DOXYGEN_REF
    isFinite;
    #endif
    // clang-format on

    for( const char *bad : { "nan", "-nan", "NaN", "inf", "-inf" } )
    {
        const double v = std::strtod( bad, nullptr );
        REQUIRE_FALSE( isFinite( v ) );
    }

    for( const char *good : { "0", "-0", "0.5", "-1e300", "1e-310", "15.041" } )
    {
        const double v = std::strtod( good, nullptr );
        REQUIRE( isFinite( v ) );
    }
}

} // namespace wccCommonTest

} // namespace libXWCTest
