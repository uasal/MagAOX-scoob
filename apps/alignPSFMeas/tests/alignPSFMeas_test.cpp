/** \file alignPSFMeas_test.cpp
  * \brief Catch2 tests for the alignPSFMeas app.
  */
#include "../../../tests/catch2/catch.hpp"

#include "../alignPSFMeas.hpp"

using namespace MagAOX::app;

namespace alignPSFMeas_test
{

class alignPSFMeas_test : public alignPSFMeas
{
public:
   alignPSFMeas_test()
   {
      m_configName = "alignPSFMeas_test";
   }

   // Expose protected members for testing.
   using alignPSFMeas::m_method;
   using alignPSFMeas::m_speckleSep;
   using alignPSFMeas::m_speckleAngle;
   using alignPSFMeas::m_speckleSearchR;
   using alignPSFMeas::m_searchHalfWidth;
   using alignPSFMeas::m_minSpeckles;
   using alignPSFMeas::m_predGain;
   using alignPSFMeas::m_maskCenterX;
   using alignPSFMeas::m_maskCenterY;
   using alignPSFMeas::m_psfEstX;
   using alignPSFMeas::m_psfEstY;
   using alignPSFMeas::m_initialized;
   using alignPSFMeas::findLocalPeak;
   using alignPSFMeas::findFirstOrderSpeckles;
   using alignPSFMeas::loadConfigImpl;

   // Method 3 raster members
   using alignPSFMeas::m_rasterHalfSteps;
   using alignPSFMeas::m_rasterStepSize;
   using alignPSFMeas::m_rasterSettleFrames;
   using alignPSFMeas::m_rasterMeasureFrames;
   using alignPSFMeas::m_rasterCenterRadius;
   using alignPSFMeas::m_rasterState;
   using alignPSFMeas::m_rasterData;
   using alignPSFMeas::m_rasterMinX;
   using alignPSFMeas::m_rasterMinY;
   using alignPSFMeas::measureCentralIntensity;
   using alignPSFMeas::fitRaster;

   // State constants
   using alignPSFMeas::RASTER_IDLE;
   using alignPSFMeas::RASTER_SETTLING;
   using alignPSFMeas::RASTER_MEASURING;
   using alignPSFMeas::RASTER_FITTING;
   using alignPSFMeas::RASTER_DONE;
};

/// Add a 2D Gaussian to an image at position (cx, cy) with sigma and peak amplitude.
static void addGaussian( eigenImage<float> & im,
                          float cx, float cy, float sigma, float amplitude )
{
   for(int r = 0; r < im.rows(); ++r)
      for(int c = 0; c < im.cols(); ++c)
      {
         float dx = r - cx, dy = c - cy;
         im(r, c) += amplitude * std::exp(-(dx*dx + dy*dy) / (2.0f * sigma * sigma));
      }
}

/// Generate a synthetic image with a PSF core and 4 first-order speckles.
/// \param psfX, psfY  PSF centre position (row, col)
/// \param speckleSep  distance from PSF centre to each speckle
/// \param angle       angle of first speckle (degrees)
/// \param psfAmp      amplitude of PSF core Gaussian
/// \param speckleAmp  amplitude of each speckle Gaussian
/// \param psfSigma    sigma of PSF Gaussian
/// \param speckleSigma sigma of each speckle Gaussian
static eigenImage<float> makeSyntheticImage( int nrows, int ncols,
                                              float psfX, float psfY,
                                              float speckleSep, float angle,
                                              float psfAmp, float speckleAmp,
                                              float psfSigma = 5.0f,
                                              float speckleSigma = 2.0f )
{
   eigenImage<float> im(nrows, ncols);
   im.setZero();

   addGaussian(im, psfX, psfY, psfSigma, psfAmp);

   float angRad = angle * M_PI / 180.0f;
   for(int n = 0; n < 4; ++n)
   {
      float a = angRad + n * (M_PI / 2.0f);
      float sx = psfX + speckleSep * std::cos(a);
      float sy = psfY + speckleSep * std::sin(a);
      addGaussian(im, sx, sy, speckleSigma, speckleAmp);
   }

   return im;
}

// ============================================================
// findLocalPeak tests
// ============================================================

SCENARIO( "findLocalPeak finds a Gaussian peak with sub-pixel accuracy", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;
   eigenImage<float> im(64, 64);
   im.setZero();

   GIVEN("a Gaussian peak at (30.0, 25.0)")
   {
      addGaussian(im, 30.0f, 25.0f, 3.0f, 100.0f);

      float fx, fy;
      bool ok = app.findLocalPeak(im, 30.0f, 25.0f, 10, fx, fy);

      REQUIRE(ok == true);
      REQUIRE(fx == Approx(30.0f).margin(0.2f));
      REQUIRE(fy == Approx(25.0f).margin(0.2f));
   }

   GIVEN("a Gaussian at (20.3, 40.7) — sub-pixel position")
   {
      im.setZero();
      addGaussian(im, 20.3f, 40.7f, 3.0f, 100.0f);

      float fx, fy;
      bool ok = app.findLocalPeak(im, 20.0f, 41.0f, 5, fx, fy);

      REQUIRE(ok == true);
      REQUIRE(fx == Approx(20.3f).margin(0.5f));
      REQUIRE(fy == Approx(40.7f).margin(0.5f));
   }

   GIVEN("search centre outside image bounds")
   {
      float fx, fy;
      bool ok = app.findLocalPeak(im, -10.0f, -10.0f, 3, fx, fy);
      REQUIRE(ok == false);
   }
}

SCENARIO( "findLocalPeak handles boundary and degenerate cases", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;

   GIVEN("peak very close to image corner (row=1, col=1)")
   {
      eigenImage<float> im(64, 64);
      im.setZero();
      addGaussian(im, 1.0f, 1.0f, 1.5f, 100.0f);

      float fx, fy;
      bool ok = app.findLocalPeak(im, 1.0f, 1.0f, 5, fx, fy);

      // Search box clips to [0,6] x [0,6]; peak should still be found
      REQUIRE(ok == true);
      REQUIRE(fx == Approx(1.0f).margin(0.5f));
      REQUIRE(fy == Approx(1.0f).margin(0.5f));
   }

   GIVEN("search radius extends well beyond image boundary — clips gracefully")
   {
      eigenImage<float> im(16, 16);
      im.setZero();
      addGaussian(im, 2.0f, 2.0f, 1.0f, 100.0f);

      float fx, fy;
      // radius=20 would extend from (-18,-18) to (22,22), all clamped to [0,15]
      bool ok = app.findLocalPeak(im, 2.0f, 2.0f, 20, fx, fy);

      REQUIRE(ok == true);
      REQUIRE(fx == Approx(2.0f).margin(0.5f));
      REQUIRE(fy == Approx(2.0f).margin(0.5f));
   }

   GIVEN("all-zero image — returns success at max-pixel position without crashing")
   {
      eigenImage<float> im(16, 16);
      im.setZero();

      float fx, fy;
      // sumW==0 branch: foundX/Y fall back to integer max-pixel position
      bool ok = app.findLocalPeak(im, 8.0f, 8.0f, 4, fx, fy);

      REQUIRE(ok == true);  // no crash, no UB
   }

   GIVEN("image with only negative values — max is least-negative, centroid clips to 0")
   {
      eigenImage<float> im(16, 16);
      im.setConstant(-1.0f);
      im(8, 8) = -0.1f;  // least-negative pixel

      float fx, fy;
      bool ok = app.findLocalPeak(im, 8.0f, 8.0f, 4, fx, fy);

      // max found at (8,8), but all 3x3 neighbourhood weights are max(0, negative) = 0
      // so sumW==0 and result falls back to integer peak position
      REQUIRE(ok == true);
      REQUIRE(fx == Approx(8.0f).margin(0.5f));
      REQUIRE(fy == Approx(8.0f).margin(0.5f));
   }
}

// ============================================================
// findFirstOrderSpeckles tests
// ============================================================

SCENARIO( "findFirstOrderSpeckles detects 4 speckles in a synthetic image", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;
   app.m_speckleSep = 30.0f;
   app.m_speckleAngle = 45.0f;

   float cx = 128.0f, cy = 128.0f;

   GIVEN("PSF centred with bright core")
   {
      // PSF core (amp 1000) is brighter than speckles (amp 500).
      // The PSF core creates an extra cluster at the midpoint of the speckle quad;
      // the geometric filter must correctly ignore it.
      auto im = makeSyntheticImage(256, 256, cx, cy, 30.0f, 45.0f,
                                    1000.0f, 500.0f);

      float foundX[4], foundY[4];
      int rv = app.findFirstOrderSpeckles(im, foundX, foundY);

      REQUIRE(rv == 0);

      float sumX = 0, sumY = 0;
      for(int n = 0; n < 4; ++n) { sumX += foundX[n]; sumY += foundY[n]; }
      float detectedCX = sumX / 4.0f;
      float detectedCY = sumY / 4.0f;

      REQUIRE(detectedCX == Approx(cx).margin(1.0f));
      REQUIRE(detectedCY == Approx(cy).margin(1.0f));
   }

   GIVEN("PSF suppressed (no core, only speckles)")
   {
      auto im = makeSyntheticImage(256, 256, cx, cy, 30.0f, 45.0f,
                                    0.0f, 500.0f);

      float foundX[4], foundY[4];
      int rv = app.findFirstOrderSpeckles(im, foundX, foundY);

      REQUIRE(rv == 0);

      float sumX = 0, sumY = 0;
      for(int n = 0; n < 4; ++n) { sumX += foundX[n]; sumY += foundY[n]; }

      REQUIRE(sumX / 4.0f == Approx(cx).margin(1.0f));
      REQUIRE(sumY / 4.0f == Approx(cy).margin(1.0f));
   }

   GIVEN("PSF offset from image centre")
   {
      float offX = 140.0f, offY = 110.0f;
      auto im = makeSyntheticImage(256, 256, offX, offY, 30.0f, 45.0f,
                                    800.0f, 500.0f);

      float foundX[4], foundY[4];
      int rv = app.findFirstOrderSpeckles(im, foundX, foundY);

      REQUIRE(rv == 0);

      float sumX = 0, sumY = 0;
      for(int n = 0; n < 4; ++n) { sumX += foundX[n]; sumY += foundY[n]; }

      REQUIRE(sumX / 4.0f == Approx(offX).margin(1.0f));
      REQUIRE(sumY / 4.0f == Approx(offY).margin(1.0f));
   }

   GIVEN("higher-order speckles present — first-order set must be selected")
   {
      auto im = makeSyntheticImage(256, 256, cx, cy, 30.0f, 45.0f,
                                    1000.0f, 500.0f);

      // Add 4 second-order speckles at 2× separation (fainter).
      float angRad = 45.0f * M_PI / 180.0f;
      for(int n = 0; n < 4; ++n)
      {
         float a = angRad + n * (M_PI / 2.0f);
         float sx = cx + 60.0f * std::cos(a);
         float sy = cy + 60.0f * std::sin(a);
         addGaussian(im, sx, sy, 2.0f, 200.0f);
      }

      float foundX[4], foundY[4];
      int rv = app.findFirstOrderSpeckles(im, foundX, foundY);

      REQUIRE(rv == 0);

      // Each found speckle must be at the first-order radius, not the second-order radius.
      float sumX = 0, sumY = 0;
      for(int n = 0; n < 4; ++n)
      {
         float dist = std::sqrt(std::pow(foundX[n] - cx, 2) + std::pow(foundY[n] - cy, 2));
         REQUIRE(dist == Approx(30.0f).margin(2.0f));
         sumX += foundX[n];
         sumY += foundY[n];
      }
      REQUIRE(sumX / 4.0f == Approx(cx).margin(1.0f));
      REQUIRE(sumY / 4.0f == Approx(cy).margin(1.0f));
   }
}

SCENARIO( "findFirstOrderSpeckles returns -1 when no valid quad can be found", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;
   app.m_speckleSep = 30.0f;
   app.m_speckleAngle = 45.0f;

   GIVEN("only 2 bright clusters present")
   {
      eigenImage<float> im(256, 256);
      im.setZero();

      float angRad = 45.0f * M_PI / 180.0f;
      for(int n = 0; n < 2; ++n)
      {
         float a = angRad + n * (M_PI / 2.0f);
         addGaussian(im, 128.0f + 30.0f * std::cos(a),
                         128.0f + 30.0f * std::sin(a), 2.0f, 500.0f);
      }

      float foundX[4], foundY[4];
      int rv = app.findFirstOrderSpeckles(im, foundX, foundY);

      REQUIRE(rv < 0);
   }

   GIVEN("4 clusters arranged in a line — no square geometry")
   {
      // Place 4 Gaussians in a horizontal line; no pair forms a diagonal of 2*speckleSep=60.
      eigenImage<float> im(256, 256);
      im.setZero();
      for(int n = 0; n < 4; ++n)
         addGaussian(im, 128.0f, 50.0f + n * 50.0f, 2.0f, 500.0f);

      float foundX[4], foundY[4];
      int rv = app.findFirstOrderSpeckles(im, foundX, foundY);

      REQUIRE(rv < 0);
   }

   GIVEN("4 clusters at wrong separation — diagonal does not match 2*speckleSep")
   {
      // Square arrangement but at sep=80, expectedDiag=160 vs app.m_speckleSep*2=60
      eigenImage<float> im(512, 512);
      im.setZero();
      float angRad = 45.0f * M_PI / 180.0f;
      for(int n = 0; n < 4; ++n)
      {
         float a = angRad + n * (M_PI / 2.0f);
         addGaussian(im, 256.0f + 80.0f * std::cos(a),
                         256.0f + 80.0f * std::sin(a), 2.0f, 500.0f);
      }

      float foundX[4], foundY[4];
      int rv = app.findFirstOrderSpeckles(im, foundX, foundY);

      REQUIRE(rv < 0);
   }
}

// ============================================================
// Method 2 multi-frame tracking
// ============================================================

SCENARIO( "Method 2 tracks PSF across multiple frames with simulated corrections", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;
   app.m_method = 2;
   app.m_speckleSep = 30.0f;
   app.m_speckleAngle = 45.0f;
   app.m_speckleSearchR = 8;
   app.m_minSpeckles = 4;
   app.m_maskCenterX = 128.0f;
   app.m_maskCenterY = 128.0f;

   float cx = 128.0f, cy = 128.0f;

   GIVEN("PSF starts offset and loop corrects with gain=0.5, predGain=0.5")
   {
      float psfX = 140.0f, psfY = 135.0f;
      float gain = 0.5f;
      app.m_predGain = gain;

      // Bootstrap: find initial position.
      auto im0 = makeSyntheticImage(256, 256, psfX, psfY, 30.0f, 45.0f,
                                     800.0f, 500.0f);
      float camSpeckleX[4], camSpeckleY[4];
      int rv = app.findFirstOrderSpeckles(im0, camSpeckleX, camSpeckleY);
      REQUIRE(rv == 0);

      float sumX = 0, sumY = 0;
      for(int n = 0; n < 4; ++n) { sumX += camSpeckleX[n]; sumY += camSpeckleY[n]; }
      app.m_psfEstX = sumX / 4.0f - app.m_maskCenterX;
      app.m_psfEstY = sumY / 4.0f - app.m_maskCenterY;
      app.m_initialized = true;

      REQUIRE(app.m_psfEstX == Approx(12.0f).margin(1.0f));  // 140-128
      REQUIRE(app.m_psfEstY == Approx(7.0f).margin(1.0f));   // 135-128

      // Simulate 5 frames of closed-loop correction.
      for(int frame = 0; frame < 5; ++frame)
      {
         float psfCX = app.m_maskCenterX + app.m_psfEstX;
         float psfCY = app.m_maskCenterY + app.m_psfEstY;
         float angRad = app.m_speckleAngle * M_PI / 180.0f;

         float sumDx = 0.0f, sumDy = 0.0f;
         int nFound = 0;
         auto im = makeSyntheticImage(256, 256, psfX, psfY, 30.0f, 45.0f,
                                       800.0f, 500.0f);

         for(int n = 0; n < 4; ++n)
         {
            float a = angRad + n * (M_PI / 2.0f);
            float eX = psfCX + app.m_speckleSep * std::cos(a);
            float eY = psfCY + app.m_speckleSep * std::sin(a);

            float fX, fY;
            if(app.findLocalPeak(im, eX, eY, app.m_speckleSearchR, fX, fY))
            {
               sumDx += fX - eX;
               sumDy += fY - eY;
               ++nFound;
            }
         }

         REQUIRE(nFound >= app.m_minSpeckles);

         float xs = app.m_psfEstX + sumDx / nFound;
         float ys = app.m_psfEstY + sumDy / nFound;

         REQUIRE(xs == Approx(psfX - cx).margin(1.0f));
         REQUIRE(ys == Approx(psfY - cy).margin(1.0f));

         // Simulate FSM correction.
         psfX -= gain * xs;
         psfY -= gain * ys;

         // Apply prediction: next search centres on predicted post-correction position.
         app.m_psfEstX = xs * (1.0f - app.m_predGain);
         app.m_psfEstY = ys * (1.0f - app.m_predGain);
      }

      // After 5 frames with gain=0.5, PSF should converge near centre.
      REQUIRE(psfX == Approx(cx).margin(1.0f));
      REQUIRE(psfY == Approx(cy).margin(1.0f));
   }
}

// ============================================================
// predGain effect on tracking state
// ============================================================

SCENARIO( "predGain controls the next-frame PSF search position", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;
   app.m_speckleSep = 30.0f;
   app.m_speckleAngle = 45.0f;
   app.m_speckleSearchR = 8;
   app.m_maskCenterX = 128.0f;
   app.m_maskCenterY = 128.0f;

   // PSF is offset by (12, 7) pixels from mask centre.
   const float psfX = 140.0f, psfY = 135.0f;
   auto im = makeSyntheticImage(256, 256, psfX, psfY, 30.0f, 45.0f, 800.0f, 500.0f);

   // Helper: run one Method-2 tracking step starting from a known estimate.
   // Returns (xs, ys) — the measured PSF shift — and updates app.m_psfEstX/Y.
   auto runOneFrame = [&](float estX, float estY) -> std::pair<float,float>
   {
      app.m_psfEstX = estX;
      app.m_psfEstY = estY;

      float psfCX = app.m_maskCenterX + estX;
      float psfCY = app.m_maskCenterY + estY;
      float angRad = app.m_speckleAngle * M_PI / 180.0f;

      float sumDx = 0.0f, sumDy = 0.0f;
      int nFound = 0;
      for(int n = 0; n < 4; ++n)
      {
         float a = angRad + n * (M_PI / 2.0f);
         float eX = psfCX + app.m_speckleSep * std::cos(a);
         float eY = psfCY + app.m_speckleSep * std::sin(a);
         float fX, fY;
         if(app.findLocalPeak(im, eX, eY, app.m_speckleSearchR, fX, fY))
         {
            sumDx += fX - eX;
            sumDy += fY - eY;
            ++nFound;
         }
      }
      REQUIRE(nFound == 4);

      float xs = app.m_psfEstX + sumDx / nFound;
      float ys = app.m_psfEstY + sumDy / nFound;

      app.m_psfEstX = xs * (1.0f - app.m_predGain);
      app.m_psfEstY = ys * (1.0f - app.m_predGain);

      return {xs, ys};
   };

   GIVEN("predGain=1.0 — assumes full correction each frame")
   {
      // With predGain=1, the next-frame estimate resets to 0, meaning the search
      // is centred on the mask centre (as if the loop has fully corrected the error).
      app.m_predGain = 1.0f;
      runOneFrame(12.0f, 7.0f);

      REQUIRE(app.m_psfEstX == Approx(0.0f).margin(0.1f));
      REQUIRE(app.m_psfEstY == Approx(0.0f).margin(0.1f));
   }

   GIVEN("predGain=0.0 — assumes no correction (open loop)")
   {
      // With predGain=0, the estimate stays at the last measured shift so the
      // next search is centred on where the PSF currently is (no correction assumed).
      app.m_predGain = 0.0f;
      auto [xs, ys] = runOneFrame(12.0f, 7.0f);

      // xs ≈ 12, ys ≈ 7 (PSF hasn't moved); estimate should equal the measurement.
      REQUIRE(app.m_psfEstX == Approx(xs).margin(0.1f));
      REQUIRE(app.m_psfEstY == Approx(ys).margin(0.1f));
   }

   GIVEN("predGain=0.5 — partial prediction")
   {
      app.m_predGain = 0.5f;
      auto [xs, ys] = runOneFrame(12.0f, 7.0f);

      REQUIRE(app.m_psfEstX == Approx(xs * 0.5f).margin(0.1f));
      REQUIRE(app.m_psfEstY == Approx(ys * 0.5f).margin(0.1f));
   }
}

// ============================================================
// Config validation
// ============================================================

SCENARIO( "loadConfigImpl rejects unsafe parameter combinations", "[alignPSFMeas]" )
{
   GIVEN("searchHalfWidth >= speckleSep/2")
   {
      alignPSFMeas_test app;
      mx::app::appConfigurator config;
      config.set("xcorr.speckleSep", "30.0");
      config.set("xcorr.searchHalfWidth", "15");  // 15 >= 30/2=15 → reject
      config.set("xcorr.speckleSearchR", "5");

      int rv = app.loadConfigImpl(config);
      REQUIRE(rv < 0);
   }

   GIVEN("speckleSearchR >= speckleSep/2")
   {
      alignPSFMeas_test app;
      mx::app::appConfigurator config;
      config.set("xcorr.speckleSep", "30.0");
      config.set("xcorr.searchHalfWidth", "5");
      config.set("xcorr.speckleSearchR", "16");  // 16 >= 30/2=15 → reject

      int rv = app.loadConfigImpl(config);
      REQUIRE(rv < 0);
   }

   GIVEN("minSpeckles = 0 (below minimum of 1)")
   {
      alignPSFMeas_test app;
      mx::app::appConfigurator config;
      config.set("xcorr.speckleSep", "30.0");
      config.set("xcorr.searchHalfWidth", "5");
      config.set("xcorr.speckleSearchR", "8");
      config.set("xcorr.minSpeckles", "0");

      int rv = app.loadConfigImpl(config);
      REQUIRE(rv < 0);
   }

   GIVEN("minSpeckles = 5 (above maximum of 4)")
   {
      alignPSFMeas_test app;
      mx::app::appConfigurator config;
      config.set("xcorr.speckleSep", "30.0");
      config.set("xcorr.searchHalfWidth", "5");
      config.set("xcorr.speckleSearchR", "8");
      config.set("xcorr.minSpeckles", "5");

      int rv = app.loadConfigImpl(config);
      REQUIRE(rv < 0);
   }

   GIVEN("predGain < 0")
   {
      alignPSFMeas_test app;
      mx::app::appConfigurator config;
      config.set("xcorr.speckleSep", "30.0");
      config.set("xcorr.searchHalfWidth", "5");
      config.set("xcorr.speckleSearchR", "8");
      config.set("xcorr.predGain", "-0.1");

      int rv = app.loadConfigImpl(config);
      REQUIRE(rv < 0);
   }

   GIVEN("predGain > 1")
   {
      alignPSFMeas_test app;
      mx::app::appConfigurator config;
      config.set("xcorr.speckleSep", "30.0");
      config.set("xcorr.searchHalfWidth", "5");
      config.set("xcorr.speckleSearchR", "8");
      config.set("xcorr.predGain", "1.1");

      int rv = app.loadConfigImpl(config);
      REQUIRE(rv < 0);
   }

   GIVEN("all parameters within valid ranges")
   {
      alignPSFMeas_test app;
      mx::app::appConfigurator config;
      config.set("xcorr.speckleSep", "30.0");
      config.set("xcorr.searchHalfWidth", "5");
      config.set("xcorr.speckleSearchR", "8");
      config.set("xcorr.minSpeckles", "4");
      config.set("xcorr.predGain", "0.5");

      int rv = app.loadConfigImpl(config);
      REQUIRE(rv == 0);
   }
}

// ============================================================
// measureCentralIntensity
// ============================================================

SCENARIO( "measureCentralIntensity sums pixels inside a circular aperture", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;

   GIVEN("a 64x64 image with a bright central region")
   {
      eigenImage<float> im(64, 64);
      im.setZero();
      // Fill a 5-pixel radius disc around (32,32) with value 1.
      for(int r = 0; r < 64; ++r)
         for(int c = 0; c < 64; ++c)
         {
            float dr = r - 32.0f, dc = c - 32.0f;
            if(dr*dr + dc*dc <= 25.0f) im(r,c) = 1.0f;
         }

      WHEN("aperture exactly matches the disc (radius=5)")
      {
         float total = app.measureCentralIntensity(im, 32.0f, 32.0f, 5.0f);
         // Count pixels inside r<=5 manually: pi*25 ~ 79, actual = 81 for integer grid
         REQUIRE(total > 60.0f);
         REQUIRE(total < 100.0f);
      }

      WHEN("aperture is smaller than the disc (radius=2)")
      {
         float total = app.measureCentralIntensity(im, 32.0f, 32.0f, 2.0f);
         float total5 = app.measureCentralIntensity(im, 32.0f, 32.0f, 5.0f);
         REQUIRE(total < total5);
         REQUIRE(total > 0.0f);
      }

      WHEN("aperture is centred away from the bright region")
      {
         float total = app.measureCentralIntensity(im, 5.0f, 5.0f, 2.0f);
         REQUIRE(total == Approx(0.0f).margin(1e-4f));
      }
   }

   GIVEN("an all-zero image")
   {
      eigenImage<float> im(32, 32);
      im.setZero();

      float total = app.measureCentralIntensity(im, 16.0f, 16.0f, 8.0f);
      REQUIRE(total == Approx(0.0f).margin(1e-6f));
   }

   GIVEN("aperture overlaps the image boundary")
   {
      eigenImage<float> im(32, 32);
      im.setConstant(1.0f);

      // Centre near corner; should not crash and should return >= 0.
      float total = app.measureCentralIntensity(im, 1.0f, 1.0f, 5.0f);
      REQUIRE(total >= 0.0f);
      // Only pixels within the image that are also within radius count.
      float total_centre = app.measureCentralIntensity(im, 16.0f, 16.0f, 5.0f);
      REQUIRE(total < total_centre);
   }
}

// ============================================================
// fitRaster
// ============================================================

SCENARIO( "fitRaster fits a 2D quadratic and finds the minimum", "[alignPSFMeas]" )
{
   alignPSFMeas_test app;
   app.m_rasterHalfSteps = 3;
   app.m_rasterStepSize  = 2.0f;

   GIVEN("perfect bowl data: I(x,y) = x^2 + y^2 with min at (0,0)")
   {
      app.m_rasterData.clear();
      for(int ix = -3; ix <= 3; ++ix)
         for(int iy = -3; iy <= 3; ++iy)
         {
            float x = ix * 2.0f, y = iy * 2.0f;
            app.m_rasterData.push_back({x, y, x*x + y*y});
         }

      int rv = app.fitRaster();
      REQUIRE(rv == 0);
      REQUIRE(app.m_rasterMinX == Approx(0.0f).margin(0.05f));
      REQUIRE(app.m_rasterMinY == Approx(0.0f).margin(0.05f));
   }

   GIVEN("offset bowl: I(x,y) = (x-2)^2 + (y-1)^2, minimum at (2,1)")
   {
      app.m_rasterData.clear();
      for(int ix = -3; ix <= 3; ++ix)
         for(int iy = -3; iy <= 3; ++iy)
         {
            float x = ix * 2.0f, y = iy * 2.0f;
            float dx = x - 2.0f, dy = y - 1.0f;
            app.m_rasterData.push_back({x, y, dx*dx + dy*dy});
         }

      int rv = app.fitRaster();
      REQUIRE(rv == 0);
      // Minimum is at (2,1) — within the scanned range of ±6.
      REQUIRE(app.m_rasterMinX == Approx(2.0f).margin(0.1f));
      REQUIRE(app.m_rasterMinY == Approx(1.0f).margin(0.1f));
   }

   GIVEN("true minimum outside scanned range: I(x,y) = (x-20)^2 + y^2")
   {
      // Minimum at x=20, which exceeds the ±6 clamping range.
      app.m_rasterData.clear();
      for(int ix = -3; ix <= 3; ++ix)
         for(int iy = -3; iy <= 3; ++iy)
         {
            float x = ix * 2.0f, y = iy * 2.0f;
            float dx = x - 20.0f;
            app.m_rasterData.push_back({x, y, dx*dx + y*y});
         }

      int rv = app.fitRaster();
      REQUIRE(rv == 0);
      // Result should be clamped to the scanned range.
      const float maxOffset = app.m_rasterHalfSteps * app.m_rasterStepSize;
      REQUIRE(app.m_rasterMinX <= maxOffset + 1e-4f);
      REQUIRE(app.m_rasterMinX >= -maxOffset - 1e-4f);
   }

   GIVEN("a saddle surface (not positive definite) — falls back to raw argmin")
   {
      // I(x,y) = x^2 - y^2: Hessian has det = 4*(1)*(-1) - 0 = -4 < 0.
      // The raw minimum is at x=0, y=±6 (largest |y| with smallest x^2-y^2).
      app.m_rasterData.clear();
      for(int ix = -3; ix <= 3; ++ix)
         for(int iy = -3; iy <= 3; ++iy)
         {
            float x = ix * 2.0f, y = iy * 2.0f;
            app.m_rasterData.push_back({x, y, x*x - y*y});
         }

      int rv = app.fitRaster();
      REQUIRE(rv == 0); // should not return error, just use argmin fallback

      // Minimum value is at x=0,y=±6 → intensity = 0 - 36 = -36.
      // m_rasterMinX should be 0 (or close), m_rasterMinY ±6.
      REQUIRE(app.m_rasterMinX == Approx(0.0f).margin(0.1f));
      REQUIRE(std::abs(app.m_rasterMinY) == Approx(6.0f).margin(0.1f));
   }

   GIVEN("fewer than 6 data points — returns error")
   {
      app.m_rasterData.clear();
      for(int i = 0; i < 5; ++i)
         app.m_rasterData.push_back({(float)i, (float)i, (float)i});

      int rv = app.fitRaster();
      REQUIRE(rv < 0);
   }
}

// ============================================================
// Config validation — Method 3 raster parameters
// ============================================================

SCENARIO( "loadConfigImpl rejects invalid raster parameters", "[alignPSFMeas]" )
{
   // Helper: build a minimally valid base config (passes all pre-raster checks).
   auto makeValidBase = []() -> mx::app::appConfigurator
   {
      mx::app::appConfigurator cfg;
      cfg.set("xcorr.speckleSep",      "30.0");
      cfg.set("xcorr.searchHalfWidth", "5");
      cfg.set("xcorr.speckleSearchR",  "8");
      cfg.set("xcorr.minSpeckles",     "4");
      cfg.set("xcorr.predGain",        "0.5");
      return cfg;
   };

   GIVEN("rasterHalfSteps = 0 (below minimum of 1)")
   {
      alignPSFMeas_test app;
      auto cfg = makeValidBase();
      cfg.set("xcorr.rasterHalfSteps", "0");
      REQUIRE(app.loadConfigImpl(cfg) < 0);
   }

   GIVEN("rasterStepSize = 0 (not positive)")
   {
      alignPSFMeas_test app;
      auto cfg = makeValidBase();
      cfg.set("xcorr.rasterStepSize", "0.0");
      REQUIRE(app.loadConfigImpl(cfg) < 0);
   }

   GIVEN("rasterStepSize = -1 (negative)")
   {
      alignPSFMeas_test app;
      auto cfg = makeValidBase();
      cfg.set("xcorr.rasterStepSize", "-1.0");
      REQUIRE(app.loadConfigImpl(cfg) < 0);
   }

   GIVEN("rasterSettleFrames = 0 (below minimum of 1)")
   {
      alignPSFMeas_test app;
      auto cfg = makeValidBase();
      cfg.set("xcorr.rasterSettleFrames", "0");
      REQUIRE(app.loadConfigImpl(cfg) < 0);
   }

   GIVEN("rasterMeasureFrames = 0 (below minimum of 1)")
   {
      alignPSFMeas_test app;
      auto cfg = makeValidBase();
      cfg.set("xcorr.rasterMeasureFrames", "0");
      REQUIRE(app.loadConfigImpl(cfg) < 0);
   }

   GIVEN("rasterCenterRadius = 0 (not positive)")
   {
      alignPSFMeas_test app;
      auto cfg = makeValidBase();
      cfg.set("xcorr.rasterCenterRadius", "0.0");
      REQUIRE(app.loadConfigImpl(cfg) < 0);
   }

   GIVEN("all raster parameters valid — passes validation")
   {
      alignPSFMeas_test app;
      auto cfg = makeValidBase();
      cfg.set("xcorr.rasterHalfSteps",    "4");
      cfg.set("xcorr.rasterStepSize",     "1.5");
      cfg.set("xcorr.rasterSettleFrames", "8");
      cfg.set("xcorr.rasterMeasureFrames","3");
      cfg.set("xcorr.rasterCenterRadius", "15.0");
      REQUIRE(app.loadConfigImpl(cfg) == 0);
   }
}

} //namespace alignPSFMeas_test
