# wccSim

Real-time all-sky simulator for the WCC sensor array. It converts the offline
`data/uasal_star_catalog_simulator_prysm.py` star field simulator into a MagAO-X
application that publishes one ImageStreamIO stream per sensor, each at that
sensor's own commanded frame rate, exposure time and region of interest.

This is to the WCC what `llowfscSimCpp` is to the LLOWFSC bench, with one
significant difference: `llowfscSimCpp` embeds CPython and calls back into the
Python optical model, whereas `wccSim` has no Python dependency at all. The
diffraction propagation, sky projection and photometry are ported to C++ in
[`../wccCommon`](../wccCommon/README.md) and pinned against `astropy`, `prysm` and
the analytic Airy pattern by that directory's test suite.

## How it fits together

```
                    ┌──────────────┐
   star catalog ───▶ │              │  roi/fps/exptime/emgain (INDI SET)
   (gsc31 CSV)      │    wccSim    │ ◀────────────────  nsvCtrlSim / nsvCtrl / any
                    │              │                    dev::stdCamera camera
   pointing ──────▶ │              │ ──────────────▶  nsv18sim, hwk09sim, ...
   (INDI or local)   └──────────────┘                    (uint16 shmim per sensor)
```

For every configured sensor, `wccSim` subscribes to that camera's `fps`,
`exptime`, `emgain`, `bitDepth` and `roi_region_*` properties. When a camera is
reconfigured — for instance when `wccCtrl` narrows it from full frame to 128×128
for guiding — the simulated frames follow on the next frame, resizing the output
stream as needed. Nothing has to be told twice.

The camera being simulated does not have to be simulated itself: pointing `wccSim`
at a real `nsvCtrl` works identically, which is how the same simulator can inject
synthetic sky into a real camera's configuration.

## Pointing

Two modes, selected by whether `pointing.tel_device` is set:

- **Empty (default).** `wccSim` owns the pointing. It accepts absolute
  `pointing` (`ra`, `dec`, `pa`) and relative `offset` (`x`, `y` in arcsec,
  `roll` in degrees) commands. This is what makes the whole acquisition loop
  demonstrable with no telescope present — `wccCtrl` sends its offsets straight
  to `wccSim` and the star field moves accordingly.
- **Set.** The pointing is slaved to that device's pointing property, and local
  `pointing`/`offset` commands are rejected with a warning. Use this against
  `tcsInterface` or a telescope simulator, where the telescope is the authority
  and `wccSim` merely reports what the sky looks like from where it is pointed.

## Frames, and what limits the rate

Each sensor has its own worker thread. Per frame it snapshots the live camera
parameters and the boresight, builds that ROI's world coordinate system, cone
searches the catalog over the ROI footprint plus a margin, adds a flux-scaled PSF
stamp per star, applies the noise model, digitizes, and publishes.

The published stream is created exactly as `dev::frameGrabber` creates one —
`naxis` 3, `size[0]` = width, uint16, temporal circular buffer — so downstream
consumers and `rtimv` cannot tell it from a real camera.

Cost is dominated by the noise model at large ROIs, because that is the only
per-pixel work: star placement is proportional to the number of stars, not to the
frame size. A full 61-megapixel IMX frame costs roughly 0.3 s in `full` noise
mode. If a worker cannot keep up it says so once per second, naming the achieved
rate and what to change:

```
IMX-18 cannot keep up: 2.9 of 4.0 Hz requested, 340 ms per frame at 9576x6388.
Reduce the ROI, the frame rate, or sim.noise_mode.
```

`sim.noise_mode` trades fidelity for rate: `full` (Poisson on illuminated pixels
plus read noise), `read` (read noise only), `off` (pedestals only, fully
repeatable). Shot noise is evaluated only inside the bounding box the star stamps
actually touched, so a sparse field in a large ROI is much cheaper than the frame
size suggests.

## Startup cost

The PSF bank is built at startup, which is the slow part: `psf_substeps²` PSFs per
distinct (pixel pitch, bandpass) combination, each a matrix DFT from an
`npix_pupil²` pupil. Sensors sharing a pitch and bandpass share one bank, so an
array of identical detectors pays once. `sim_status.banks_ready` goes to 1 when
the banks are done; the app stays `NOTCONNECTED` until then and publishes nothing.

Startup time scales as `npix_pupil² × psf_samples² × psf_substeps²`. The defaults
(256, 48, 8) take a few seconds per bank. Raising `psf_substeps` to 16 quadruples
that for a 2× improvement in placement accuracy, which is rarely the right trade.

## INDI interface

| Property | Type | Purpose |
|---|---|---|
| `streaming` | toggle | Gates frame publication on all sensors |
| `pointing` | number `ra`, `dec`, `pa` | Absolute boresight; also reports the current value |
| `offset` | number `x`, `y`, `roll` | Relative offset in arcsec and degrees; self-clearing |
| `catalog` | number (RO) | `nsources`, `mag_min`, `mag_max` |
| `sim_status` | number (RO) | `nsensors`, `banks_ready`, `frames` |
| `cam_<SENSOR>` | number (RO) | Per sensor: `fps`, `achieved_fps`, `exptime`, `roi_*`, `nstars`, `render_ms`, `frames`, `saturated` |

Sensor names contain hyphens, which are not valid in INDI property names, so
`cam_IMX-18` is published as `cam_IMX_18`.

## Configuration

See `wccSim.conf.sample`, which is annotated. The shape is a `[telescope]`,
`[catalog]`, `[pointing]` and `[sim]` block, then one section per sensor named
exactly as it appears in `sim.sensors`.

**The sensor sections must match `wccCtrl`'s.** The geometry keys are read by the
shared loader in `../wccCommon/wccSensorConfig.hpp` precisely so the two cannot
drift, but the two config files still have to carry the same numbers.

## Trying it out

With a camera simulator and a controller:

```bash
# start a camera per sensor (nsvCtrlSim, one instance per sensor)
# then the simulator
/opt/MagAOX/bin/wccSim -n wccsim
# turn on frame publication
xindi wccsim.streaming.toggle=On
# move the boresight and watch the field shift
xindi wccsim.offset.x=30 wccsim.offset.y=-10
```

`sim.start_streaming=true` skips the toggle if you want frames immediately.

## Known limitations

- The optical path is diffraction plus an optional static OPD map. There is no
  atmosphere, no field-dependent aberration and no distortion beyond the tangent
  plane projection. The array is modelled as ideal sensors on a perfect tangent
  plane.
- `setOPD` exists and is honoured, but nothing in the app populates it yet; the
  Python simulator's PSD-based mirror surface maps (`psd_utils`) are not ported.
  The hook is there when they are needed.
- The bandpass is sampled at five wavelengths with uniform weights rather than
  interpolating real coating, QE and filter curves. Since total flux comes from
  the photometry, this affects only the chromatic PSF shape.
- Full-frame at high rate is not achievable in `full` noise mode; see above.
- Proper motion is ignored: catalog positions are used at their stated epoch.
