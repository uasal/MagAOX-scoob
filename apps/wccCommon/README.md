# wccCommon

Shared, header-only support code for the WCC applications. **This is not an
application** — there is no `Makefile` and nothing is built here. `wccSim` and
`wccCtrl` include these headers directly and list them in their own
`OTHER_HEADERS`.

The two applications must agree exactly on the sensor array geometry and the sky
projection, otherwise the controller solves astrometry against a different array
than the simulator rendered. Keeping that code in one place is the point of this
directory.

## Contents

| Header | Purpose |
|---|---|
| `wccUnits.hpp` | Physical constants and unit conversions, replacing `astropy.units` |
| `wccNumeric.hpp` | `isFinite()`, a NaN/infinity test that survives the `-ffast-math` MagAO-X builds with (`std::isfinite` does not) |
| `wccSkyWCS.hpp` | Gnomonic (TAN) world coordinate system, replacing `astropy.wcs` |
| `wccMDFT.hpp` | Matrix triple-product DFT, a port of the `prysm` functions the Python simulator uses |
| `wccPSF.hpp` | Pupil construction, broadband PSF generation, and the sub-pixel PSF bank |
| `wccPhotometry.hpp` | AB magnitude to photoelectrons |
| `wccStarCatalog.hpp` | GSC 3.1 style CSV ingest with a declination-sorted cone search |
| `wccFocalPlane.hpp` | Sensor array layout, region of interest conventions, and per-sensor WCS |
| `wccSensorModel.hpp` | PSF stamp accumulation, kHz trail coalescing into PSF-bank cells, detector noise, IMX analog gain, digitization |
| `wccAstrometry.hpp` | Source detection, catalog matching, multi-sensor boresight solution |
| `wccJSON.hpp` | Minimal JSON reader for the visit file |
| `wccVisit.hpp` | The visit file model |
| `wccVisitIndi.hpp` | The INDI contract by which visitCtrl publishes a visit |
| `wccIndiRate.hpp` | The 1 Hz INDI rate limit and a gate that enforces it |
| `wccPointingShmim.hpp` | The 4×1×N RA/Dec/PA/sim-time ImageStreamIO contract for high-rate pointing |
| `wccSensorConfig.hpp` | The shared sensor-section config loader |

Everything except `wccSensorConfig.hpp` depends only on the standard library and
Eigen. `wccSensorConfig.hpp` additionally needs `mx::app::appConfigurator`, which
is why it is separate: the rest can be compiled and tested without any of MagAO-X.

Three of these headers exist specifically so that separate applications cannot drift
apart on a shared convention, and the compiler enforces it:

- `wccSensorConfig.hpp` — the sensor array geometry. If `wccSim` and `wccCtrl`
  disagreed, the controller would solve astrometry against a different array than the
  simulator rendered.
- `wccVisitIndi.hpp` — the property and element names carrying a visit. A rename
  breaks the publisher and all consumers at once rather than silently at runtime.
- `wccPointingShmim.hpp` — the high-rate pointing stream layout. `telescopeSim`
  writes it and `wccSim` reads it; a change in axis order would otherwise silently
  smear every exposure. Correlation is by simulated time packed in the fourth
  axis, not by the computer clock, because the simulators are not required to run
  in real time.
- `offsetBoresight()` in `wccFocalPlane.hpp` — the field-angle offset convention,
  shared by the telescope that must move when commanded and the simulator that must
  render what it now sees.

## What was ported from Python, and where it differs

`data/uasal_star_catalog_simulator_prysm.py` is an offline script built on
`astropy` and `prysm`. The mapping is:

| Python | Here |
|---|---|
| `astropy.wcs.WCS` + `skycoord_to_pixel` | `skyWCS` |
| `prysm.fttools.MatrixDFTExecutor` | `matrixDFT` |
| `prysm.propagation.focus_fixed_sampling` | `focusFixedSampling` |
| `prysm.coordinates.make_xy_grid` / `cart_to_polar` | `makeXYGrid` / `cartToPolar` |
| `prysm.geometry.circle` / `spider` | `circleMask` / `spiderMask` |
| `prysm.polynomials.sum_of_2d_modes` | `sumOf2DModes` |
| `get_psf()` | `psfGenerator::psf` |
| `(mag * u.ABmag).to(...)` | `abMagToPhotonFluxDensity` |
| `load_gaia_subset()` | `starCatalog::load` |
| `np.random.poisson` / `np.random.normal` | `sensorNoise::apply` |

Three deliberate departures, each documented at its site:

1. **Sub-pixel shift units.** The Python passes fractional pixel offsets straight
   into `focus_fixed_sampling(shift=...)`, which interprets `shift` in output
   *microns* and divides by `output_dx` internally. Fractional-pixel shifts are
   therefore attenuated by the pixel size. `focusFixedSampling` takes the shift in
   output pixels, which is what star placement actually wants.

2. **Read noise.** The Python uses
   `np.random.normal(loc=read_noise, scale=read_noise/5)`, which adds a pedestal
   equal to sigma and produces a distribution five times narrower than the stated
   read noise. Here read noise is zero-mean at the configured sigma, and any
   pedestal is an explicit bias level.

3. **Double-counted signal.** The Python plots and writes `im_fits + noisy3`,
   where `noisy3` already contains the signal. Here the signal is counted once.

A performance departure as well: the Python calls `get_psf()` once per star, which
is two 512×512 complex matrix products per star per wavelength. `psfBank`
precomputes the PSF on a quantized grid of sub-pixel offsets once at startup, so
placing a star costs one lookup and one scaled accumulation. Worst-case placement
error is `0.5 / subSteps` pixels, and `psfGenerator` remains available for an
exact per-star computation.

## Verification

`tests/wccCommon_test.cpp` pins the port against independent references rather
than against itself:

- **TAN projection** against pixel positions from `astropy.wcs` (which wraps
  wcslib) for several pixel scales, a negative `CDELT1`, a field rotation, a
  near-pole reference and an RA wrap. Agreement is at the 1e-9 pixel level.
- **Matrix DFT** against a verbatim transcription of prysm 0.21.1
  `MatrixDFTExecutor.dft2`; agreement is at 1e-15 relative, including for
  non-square arrays, non-integer `Q` and non-zero shifts.
- **Diffraction PSF** against the analytic Airy pattern, computed in the test from
  `std::cyl_bessel_j` so no external data is needed. Agreement is 0.13% of peak,
  set by the pupil sampling. An integer-pixel shift reproduces an array
  translation to 1e-5 of peak, and a sub-pixel shift reproduces a recentred Airy
  pattern to the same 0.13%.
- **Photometry** against `astropy.units` `ABmag` with a `spectral_density`
  equivalency.
- **Astrometry** end to end: a synthetic field built from real PSFs, real
  photometry and real noise, with a known offset and rotation injected and
  recovered to better than 0.3 pixels across the frame — including with half the
  predictions missing and 25 spurious detections added.
- **Boresight solution** recovers an injected pointing and roll exactly, and
  degrades gracefully for a single sensor or a degenerate layout.

The suite is 45,827 assertions across 11 test cases. To run it:

```bash
cd tests
make -B -f Makefile.one t=../apps/wccCommon/tests/wccCommon_test.cpp
../apps/wccCommon/tests/wccCommon_test
```

Because these headers do not depend on libMagAOX, the test can also be built
standalone, which is useful when iterating on the science:

```bash
g++ -std=c++17 -O2 -I/usr/local/include/eigen3 -Itests \
    -c tests/testMain.cpp -o /tmp/testMain.o
g++ -std=c++17 -O2 -I/usr/local/include/eigen3 -Itests \
    -c apps/wccCommon/tests/wccCommon_test.cpp -o /tmp/wccCommon_test.o
g++ -o /tmp/wccCommon_test /tmp/testMain.o /tmp/wccCommon_test.o && /tmp/wccCommon_test
```

## Conventions worth knowing before editing

- **Pixels are 0-based.** FITS `CRPIXn` is 1-based, so `crpix0 = CRPIXn - 1`.
- **Region of interest** follows `dev::stdCamera`: `roi_region_x` is the ROI
  *centre* in full-sensor pixels and `roi_region_w` is the size in *unbinned*
  pixels. The published image is `w/bin_x` by `h/bin_y`.
- **Field angles** are boresight-relative, in arcseconds, and focal plane X runs
  opposite to increasing right ascension. This matches the `X_WCC` and `Y_WCC`
  columns of the visit file; the interpretation was confirmed by reprojecting
  those columns and recovering the RA/Dec the file lists, to 0.02 arcsec.
- **Sensor names** are compared through `normalizeSensorName`, which folds case,
  drops punctuation and maps `HAWK` onto `HWK`, because the visit file uses both
  spellings for the same hardware.
- `frameSolution::m_dx` and `m_dy` are meaningless without `m_pivotX` and
  `m_pivotY`. Use `apply()` or `offsetAt()` instead.
- `matrixDFT`, `psfGenerator` and `sensorNoise` are **not** thread safe; each owns
  a cache or RNG state. Give every worker thread its own. A built `psfBank` is
  immutable and can be shared.

## The 1 Hz INDI limit

`wccIndiRate.hpp` carries the interval every WCC application holds its INDI reads
and writes to. The INDI server serializes all property updates through one set of
FIFOs, so several applications each publishing at tens of hertz will saturate it and
start dropping devices.

This is a real constraint on the control loops, not just on status reporting: it puts
a 1 s floor on the fast guiding loop. `clampIndiPeriod()` raises a shorter requested
period rather than letting it through, so the limit shows up in the configuration and
the log instead of as mysterious INDI instability. `rateGate` is for code paths
called more often than that, such as a worker thread, that still need to emit INDI.

## Units

Following prysm, which the PSF code has to match:

- pupil sample spacing and focal length: any single consistent length unit (only
  the ratio enters `Q`)
- wavelength: **microns** in the propagation code, **nanometres** in
  `bandpassConfig` and `sensorConfig`
- detector pixel pitch and output plane sampling: **microns**
- optical path difference: **nanometres**
- field angles: **arcseconds**; sky coordinates and CD matrices: **degrees**
