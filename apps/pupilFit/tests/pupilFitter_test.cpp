/** \file pupilFitter_test.cpp
 * \brief Catch2 tests for the pupilFitter class used by the pupilFit app.
 * \author Claude Code
 *
 * \ingroup pupilFit_files
 */

#include "../../../tests/testXWC.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "../pupilFitter.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/// Namespace for `pupilFit` unit tests.
/** \ingroup pupilFit_unit_test
 */
namespace pupilFitTest
{

/// Background level used in the synthetic pupil images.
constexpr float c_bgLevel = 10.0f;

/// Illuminated-pupil level used in the synthetic pupil images.
constexpr float c_pupLevel = 1000.0f;

/// Radius, in pixels, of the synthetic pupils.
constexpr float c_pupRad = 24.0f;

/// Paint a filled circular pupil into an image.
/** Every pixel whose center lies within \p r of (\p xc, \p yc) is set to \p val.
 */
void addPupil( mx::improc::eigenImage<float> &im, /**< [in.out] the image to paint into */
               float                          xc, /**< [in] the row (x) coordinate of the pupil center */
               float                          yc, /**< [in] the column (y) coordinate of the pupil center */
               float                          r,  /**< [in] the pupil radius in pixels */
               float                          val /**< [in] the value to assign to pupil pixels */
)
{
    for( int i = 0; i < im.rows(); ++i )
    {
        for( int j = 0; j < im.cols(); ++j )
        {
            if( ( i - xc ) * ( i - xc ) + ( j - yc ) * ( j - yc ) <= r * r )
            {
                im( i, j ) = val;
            }
        }
    }
}

/// Build a 120x120 four-pupil image with pupils centered in each 60x60 quadrant.
/** The pupil in each quadrant is offset from the quadrant center (29.5,29.5) by the given shifts.
 *
 * \returns the synthetic image
 */
mx::improc::eigenImage<float> fourPupilImage( float dx0 = 0, /**< [in] [optional] x shift of pupil 0 */
                                              float dy0 = 0  /**< [in] [optional] y shift of pupil 0 */
)
{
    mx::improc::eigenImage<float> im;
    im.resize( 120, 120 );
    im.setConstant( c_bgLevel );

    addPupil( im, 29.5 + dx0, 29.5 + dy0, c_pupRad, c_pupLevel ); // quad 0: i0=0,  j0=0
    addPupil( im, 89.5, 29.5, c_pupRad, c_pupLevel );             // quad 1: i0=60, j0=0
    addPupil( im, 29.5, 89.5, c_pupRad, c_pupLevel );             // quad 2: i0=0,  j0=60
    addPupil( im, 89.5, 89.5, c_pupRad, c_pupLevel );             // quad 3: i0=60, j0=60

    return im;
}

/// Verify setSize() allocates the working and magnified buffers.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter setSize allocates the working buffers", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::setSize( 0, 0 );
    #endif
    // clang-format on

    pupilFitter<float> fitter;

    REQUIRE( fitter.m_rows == 0 );
    REQUIRE( fitter.m_cols == 0 );
    REQUIRE( fitter.m_numPupils == 4 );
    REQUIRE( fitter.m_thresh == Approx( 0.5 ) );
    REQUIRE( fitter.m_bgMedIndex == Approx( 0.1867 ) );
    REQUIRE( fitter.m_pupMedIndex == Approx( 0.6867 ) );

    REQUIRE( fitter.setSize( 60, 50 ) == 0 );

    REQUIRE( fitter.m_rows == 60 );
    REQUIRE( fitter.m_cols == 50 );
    REQUIRE( fitter.m_quad.rows() == 60 );
    REQUIRE( fitter.m_quad.cols() == 50 );
    REQUIRE( fitter.m_circ.rows() == 60 );
    REQUIRE( fitter.m_circ.cols() == 50 );
    REQUIRE( fitter.m_quadMag.rows() == 600 );
    REQUIRE( fitter.m_quadMag.cols() == 500 );
    REQUIRE( fitter.m_circMag.rows() == 600 );
    REQUIRE( fitter.m_circMag.cols() == 500 );
    REQUIRE( fitter.m_pixs.size() == 600 * 500 );
}

/// Verify quadCoords() for the 4-pupil and 3-pupil layouts.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter quadCoords maps quadrant numbers to corners", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::quadCoords( i0, j0, 0 );
    #endif
    // clang-format on

    pupilFitter<float> fitter;
    fitter.setSize( 60, 40 );

    size_t i0 = 99, j0 = 99;

    SECTION( "4 pupils" )
    {
        fitter.m_numPupils = 4;

        REQUIRE( fitter.quadCoords( i0, j0, 0 ) == 0 );
        REQUIRE( i0 == 0 );
        REQUIRE( j0 == 0 );

        fitter.quadCoords( i0, j0, 1 );
        REQUIRE( i0 == 60 );
        REQUIRE( j0 == 0 );

        fitter.quadCoords( i0, j0, 2 );
        REQUIRE( i0 == 0 );
        REQUIRE( j0 == 40 );

        fitter.quadCoords( i0, j0, 3 );
        REQUIRE( i0 == 60 );
        REQUIRE( j0 == 40 );
    }

    SECTION( "3 pupils" )
    {
        fitter.m_numPupils = 3;

        REQUIRE( fitter.quadCoords( i0, j0, 0 ) == 0 );
        REQUIRE( i0 == 0 );
        REQUIRE( j0 == 0 );

        fitter.quadCoords( i0, j0, 1 );
        REQUIRE( i0 == 60 );
        REQUIRE( j0 == 0 );

        // The third pupil is centered in x across the top half
        fitter.quadCoords( i0, j0, 2 );
        REQUIRE( i0 == 30 );
        REQUIRE( j0 == 40 );
    }
}

/// Verify threshold() converts an image to a 1/0 mask at m_thresh.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter threshold produces a binary mask", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::threshold( im );
    #endif
    // clang-format on

    pupilFitter<float> fitter;

    mx::improc::eigenImage<float> im;
    im.resize( 2, 3 );
    im( 0, 0 ) = -1.0;
    im( 0, 1 ) = 0.49;
    im( 0, 2 ) = 0.5;
    im( 1, 0 ) = 0.51;
    im( 1, 1 ) = 2.0;
    im( 1, 2 ) = 0.0;

    SECTION( "default threshold of 0.5 is inclusive" )
    {
        REQUIRE( fitter.threshold( im ) == 0 );

        REQUIRE( im( 0, 0 ) == 0 );
        REQUIRE( im( 0, 1 ) == 0 );
        REQUIRE( im( 0, 2 ) == 1 );
        REQUIRE( im( 1, 0 ) == 1 );
        REQUIRE( im( 1, 1 ) == 1 );
        REQUIRE( im( 1, 2 ) == 0 );
    }

    SECTION( "a custom threshold is honored" )
    {
        fitter.m_thresh = 1.0;
        REQUIRE( fitter.threshold( im ) == 0 );

        REQUIRE( im( 0, 2 ) == 0 );
        REQUIRE( im( 1, 0 ) == 0 );
        REQUIRE( im( 1, 1 ) == 1 );
    }
}

/// Verify getQuad() and putQuad() extract and insert quadrants, and reject wrongly sized images.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter getQuad and putQuad round trip", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::getQuad( quad, im, 0 );
    pupilFitter<float>::putQuad( im, quad, 0 );
    #endif
    // clang-format on

    pupilFitter<float> fitter;
    fitter.setSize( 4, 3 );

    mx::improc::eigenImage<float> im;
    im.resize( 8, 6 );
    for( int i = 0; i < im.rows(); ++i )
    {
        for( int j = 0; j < im.cols(); ++j )
        {
            im( i, j ) = 100 * i + j;
        }
    }

    mx::improc::eigenImage<float> quad;
    quad.resize( 4, 3 );

    SECTION( "getQuad extracts each quadrant" )
    {
        REQUIRE( fitter.getQuad( quad, im, 0 ) == 0 );
        REQUIRE( quad( 0, 0 ) == 0 );
        REQUIRE( quad( 3, 2 ) == 302 );

        REQUIRE( fitter.getQuad( quad, im, 1 ) == 0 );
        REQUIRE( quad( 0, 0 ) == 400 );
        REQUIRE( quad( 3, 2 ) == 702 );

        REQUIRE( fitter.getQuad( quad, im, 2 ) == 0 );
        REQUIRE( quad( 0, 0 ) == 3 );
        REQUIRE( quad( 3, 2 ) == 305 );

        REQUIRE( fitter.getQuad( quad, im, 3 ) == 0 );
        REQUIRE( quad( 0, 0 ) == 403 );
        REQUIRE( quad( 3, 2 ) == 705 );
    }

    SECTION( "putQuad inserts into the right quadrant only" )
    {
        mx::improc::eigenImage<float> out;
        out.resize( 8, 6 );
        out.setZero();

        quad.setConstant( 7 );
        REQUIRE( fitter.putQuad( out, quad, 3 ) == 0 );

        REQUIRE( out.block( 4, 3, 4, 3 ).sum() == Approx( 7 * 12 ) );
        REQUIRE( out.sum() == Approx( 7 * 12 ) );
        REQUIRE( out( 3, 2 ) == 0 );
        REQUIRE( out( 4, 3 ) == 7 );
    }

    SECTION( "round trip reproduces the image" )
    {
        mx::improc::eigenImage<float> out;
        out.resize( 8, 6 );
        out.setZero();

        for( int q = 0; q < 4; ++q )
        {
            REQUIRE( fitter.getQuad( quad, im, q ) == 0 );
            REQUIRE( fitter.putQuad( out, quad, q ) == 0 );
        }

        REQUIRE( ( out - im ).abs().maxCoeff() == 0 );
    }

    SECTION( "wrong image size is rejected" )
    {
        mx::improc::eigenImage<float> bad;
        bad.resize( 7, 6 );
        bad.setZero();

        REQUIRE( fitter.getQuad( quad, bad, 0 ) == -1 );
        REQUIRE( fitter.putQuad( bad, quad, 0 ) == -1 );

        bad.resize( 8, 5 );
        REQUIRE( fitter.getQuad( quad, bad, 0 ) == -1 );
        REQUIRE( fitter.putQuad( bad, quad, 0 ) == -1 );
    }
}

/// Verify fit() measures the centers, diameters, background and median of four synthetic pupils.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter fit measures four synthetic pupils", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::fit( im, edged );
    pupilFitter<float>::outerpix( avgx, avgy, avgr, 0 );
    #endif
    // clang-format on

    pupilFitter<float> fitter;
    fitter.setSize( 60, 60 );

    mx::improc::eigenImage<float> im = fourPupilImage();

    mx::improc::eigenImage<float> edged;
    edged.resize( 120, 120 );
    edged.setZero();

    REQUIRE( fitter.fit( im, edged ) == 0 );

    const float qx[4] = { 29.5, 89.5, 29.5, 89.5 };
    const float qy[4] = { 29.5, 29.5, 89.5, 89.5 };

    for( int q = 0; q < 4; ++q )
    {
        INFO( "quad " << q );

        // magnification by 599/590 and edge discretization bias the absolute values slightly
        CHECK( fitter.m_avgx[q] == Approx( qx[q] ).margin( 1.0 ) );
        CHECK( fitter.m_avgy[q] == Approx( qy[q] ).margin( 1.0 ) );
        CHECK( fitter.m_avgr[q] == Approx( c_pupRad + 0.5 ).margin( 1.0 ) );

        CHECK( fitter.m_bg[q] == Approx( c_bgLevel ).epsilon( 1e-4 ) );
        CHECK( fitter.m_med[q] == Approx( c_pupLevel ).epsilon( 1e-4 ) );
    }

    // Identical pupils give identical fits, offset by exactly the quadrant size
    REQUIRE( fitter.m_avgx[1] - fitter.m_avgx[0] == Approx( 60 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgy[1] - fitter.m_avgy[0] == Approx( 0 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgy[2] - fitter.m_avgy[0] == Approx( 60 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgx[2] - fitter.m_avgx[0] == Approx( 0 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgx[3] - fitter.m_avgx[0] == Approx( 60 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgy[3] - fitter.m_avgy[0] == Approx( 60 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgr[3] == Approx( fitter.m_avgr[0] ).margin( 1e-3 ) );

    // The image is replaced by the de-magnified threshold mask
    REQUIRE( im.maxCoeff() <= 1.0f );
    REQUIRE( im.minCoeff() >= 0.0f );
    REQUIRE( im( 29, 29 ) == Approx( 1.0 ) );
    REQUIRE( im( 89, 89 ) == Approx( 1.0 ) );
    REQUIRE( im( 0, 0 ) == Approx( 0.0 ) );
    REQUIRE( im( 59, 59 ) == Approx( 0.0 ) );

    // Thresholded area approximates the pupil area in each quadrant
    const float area = 3.14159265f * c_pupRad * c_pupRad;
    REQUIRE( im.block( 0, 0, 60, 60 ).sum() == Approx( area ).epsilon( 0.1 ) );

    // The edge image marks the rim but not the interior
    REQUIRE( edged.sum() > 0 );
    REQUIRE( edged( 29, 29 ) == 0 );
    REQUIRE( edged( 0, 0 ) == 0 );
}

/// Verify fit() tracks pupil shifts.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter fit follows a shifted pupil", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::fit( im, edged );
    #endif
    // clang-format on

    pupilFitter<float> fitter;
    fitter.setSize( 60, 60 );

    mx::improc::eigenImage<float> edged;
    edged.resize( 120, 120 );

    mx::improc::eigenImage<float> im = fourPupilImage();
    REQUIRE( fitter.fit( im, edged ) == 0 );

    const float x0  = fitter.m_avgx[0];
    const float y0  = fitter.m_avgy[0];
    const float r0  = fitter.m_avgr[0];
    const float x1  = fitter.m_avgx[1];
    const float y1  = fitter.m_avgy[1];
    const float x3r = fitter.m_avgr[3];

    im = fourPupilImage( 4, -3 );
    REQUIRE( fitter.fit( im, edged ) == 0 );

    // The shifted pupil moves by the shift (scaled by the ~1.015 magnification factor)
    REQUIRE( fitter.m_avgx[0] - x0 == Approx( 4 ).margin( 0.3 ) );
    REQUIRE( fitter.m_avgy[0] - y0 == Approx( -3 ).margin( 0.3 ) );
    REQUIRE( fitter.m_avgr[0] == Approx( r0 ).margin( 0.3 ) );

    // The other pupils are unchanged
    REQUIRE( fitter.m_avgx[1] == Approx( x1 ).margin( 1e-4 ) );
    REQUIRE( fitter.m_avgy[1] == Approx( y1 ).margin( 1e-4 ) );
    REQUIRE( fitter.m_avgr[3] == Approx( x3r ).margin( 1e-4 ) );
}

/// Verify fit() measures a larger pupil as larger.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter fit measures pupil size changes", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::fit( im, edged );
    #endif
    // clang-format on

    pupilFitter<float> fitter;
    fitter.setSize( 60, 60 );

    mx::improc::eigenImage<float> edged;
    edged.resize( 120, 120 );

    mx::improc::eigenImage<float> im;
    im.resize( 120, 120 );
    im.setConstant( c_bgLevel );
    addPupil( im, 29.5, 29.5, 20, c_pupLevel );
    addPupil( im, 89.5, 29.5, 24, c_pupLevel );
    addPupil( im, 29.5, 89.5, 24, c_pupLevel );
    addPupil( im, 89.5, 89.5, 24, c_pupLevel );

    REQUIRE( fitter.fit( im, edged ) == 0 );

    REQUIRE( fitter.m_avgr[1] - fitter.m_avgr[0] == Approx( 4 ).margin( 0.4 ) );
    REQUIRE( fitter.m_avgx[0] == Approx( 29.5 ).margin( 1.0 ) );
}

/// Verify fit() with the 3-pupil layout.
/**
 * \ingroup pupilFit_unit_test
 */
TEST_CASE( "pupilFitter fit measures three synthetic pupils", "[pupilFit][pupilFitter]" )
{
    // clang-format off
    #ifdef PUPILFIT_TEST_DOXYGEN_REF
    pupilFitter<float>::fit( im, edged );
    #endif
    // clang-format on

    pupilFitter<float> fitter;
    fitter.m_numPupils = 3;
    fitter.setSize( 60, 60 );

    mx::improc::eigenImage<float> im;
    im.resize( 120, 120 );
    im.setConstant( c_bgLevel );
    addPupil( im, 29.5, 29.5, c_pupRad, c_pupLevel ); // quad 0: i0=0,  j0=0
    addPupil( im, 89.5, 29.5, c_pupRad, c_pupLevel ); // quad 1: i0=60, j0=0
    addPupil( im, 59.5, 89.5, c_pupRad, c_pupLevel ); // quad 2: i0=30, j0=60

    mx::improc::eigenImage<float> edged;
    edged.resize( 120, 120 );
    edged.setZero();

    // Pre-set the 4th slot so we can see it is not touched
    fitter.m_avgx[3] = -7;

    REQUIRE( fitter.fit( im, edged ) == 0 );

    REQUIRE( fitter.m_avgx[1] - fitter.m_avgx[0] == Approx( 60 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgx[2] - fitter.m_avgx[0] == Approx( 30 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgy[2] - fitter.m_avgy[0] == Approx( 60 ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgr[2] == Approx( fitter.m_avgr[0] ).margin( 1e-3 ) );
    REQUIRE( fitter.m_avgx[2] == Approx( 59.5 ).margin( 1.0 ) );
    REQUIRE( fitter.m_avgy[2] == Approx( 89.5 ).margin( 1.0 ) );

    REQUIRE( fitter.m_avgx[3] == -7 );

    // The unused bottom-right corner is left zeroed
    REQUIRE( im( 119, 119 ) == 0 );
    REQUIRE( im( 59, 89 ) == Approx( 1.0 ) );
}

} // namespace pupilFitTest

} // namespace libXWCTest
