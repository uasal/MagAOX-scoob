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
   star catalog ─────▶ │              │ ◀── roi / fps / exptime / emgain
    (gsc31 CSV)         │              │     from nsvCtrlSim, nsvCtrl, or any
                       │    wccSim    │     dev::stdCamera camera
   telpointing shmim ─▶ │              │
   from telescopeSim   │              │ ──▶ nsv18sim, hwk09sim, ...
                       └──────────────┘     (uint16 shmim per sensor, at camera fps)
```

Three inputs, and it needs all three to render a frame: the **star catalog** for
what is out there, each **camera's** commanded ROI and exposure time for how that
sensor is currently reading out, and the **telescope pointing history** for where
the array was looking during the exposure. Change any one and the images follow.

For every configured sensor, `wccSim` subscribes to that camera's `fps`,
`exptime`, `emgain`, `bitDepth` and `roi_region_*` properties. `emgain` is an
IMX455-style analog gain **code**, 0 to 255, at 0.1 dB per step. It multiplies
collected electrons and the noise already in them (`G = 10^(0.1 * code / 20)`).
When a camera is reconfigured — for instance when `wccCtrl` narrows it from full
frame to 128×128 for guiding — the simulated frames follow it with no operator
action.

The camera being simulated does not have to be simulated itself: pointing `wccSim`
at a real `nsvCtrl` works identically, which is how the same simulator can inject
synthetic sky into a real camera's configuration.

## Pointing

High-rate pointing is a shmim, not INDI. `telescopeSim` writes `telpointing` (a
4×1×N circular buffer of RA, Dec, PA in degrees and simulated time in seconds) at
`pointing.write_hz` ticks of simulated time. When `wccSim` publishes a camera frame
it collects every tick whose sim-time falls inside `[now - exptime, now]`, where
`now` is the newest slice — not the computer clock. Each tick contributes flux at
that boresight. Ticks that land in the same PSF-bank sub-pixel cell are coalesced
into one splat, which is the dwell-map convolution of the pointing path with the
PSF at the bank's native resolution (`psf_substeps`, 8 by default, 0.0625 px). A
1 s exposure at 5000 Hz therefore uses all 5000 samples; the renderer does not
time-average them down to 512 placements. `pointing.max_samples` is an optional
debug throttle (0, the default, keeps every tick). Camera output streams still
publish at the camera's own commanded frame rate.

INDI pointing is 1 Hz status, and a fallback if the shmim is not yet open.
`pointing.tel_device` / local `pointing` and `offset` commands work as before when
`pointing.shmim` is left empty. `wccSim` also subscribes to `tel_device.write_hz`
and `tel_device.history_s`. A SET of either closes the pointing mmap so the next frame
reopens the (possibly resized) `telpointing` stream and, if `write_hz` is present,
updates the tick-rate fallback used when the stream has no sim-time axis.

The offset conversion is shared with `telescopeSim` through
`wccCommon::offsetBoresight`, so a commanded correction and the resulting image
motion cannot disagree.

## Frames, and what limits the rate

Each sensor has its own worker thread. Per frame it snapshots the live camera
parameters, reads the pointing history for the exposure, builds that ROI's world
coordinate system at each sample, cone searches once, histograms each star's
trail into PSF-bank cells, splats each occupied cell, applies the noise model,
digitizes with analog gain, and publishes.

The published stream is created exactly as `dev::frameGrabber` creates one —
`naxis` 3, `size[0]` = width, uint16, temporal circular buffer — so downstream
consumers and `rtimv` cannot tell it from a real camera. A second stream,
`shmim_out` + `dwell`, is the same size with a 1 at every detector pixel a PSF
stamp landed on and 0 elsewhere, for overlaying the trail.

Cost is dominated by the noise model at large ROIs, because that is the only
per-pixel work: star placement is proportional to the number of occupied
PSF-bank cells along each trail, not to the pointing write rate and not to the
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
| `cam_<SENSOR>` | number (RO) | Per sensor: `fps`, `achieved_fps`, `exptime`, `roi_*`, `nstars`, `gain_code`, `analog_gain`, `render_ms`, `frames`, `saturated` |

Sensor names contain hyphens, which are not valid in INDI property names, so
`cam_IMX-18` is published as `cam_IMX_18`.

## Configuration

See `wccSim.conf.sample`, which is annotated. The shape is a `[telescope]`,
`[catalog]`, `[pointing]` and `[sim]` block, then one section per sensor named
exactly as it appears in `sim.sensors`.

**The sensor sections must match `wccCtrl`'s.** The geometry keys are read by the
shared loader in `../wccCommon/wccSensorConfig.hpp` precisely so the two cannot
drift, but the two config files still have to carry the same numbers.

The catalog is instrument calibration data rather than source, so it is not
carried in this repository. Install it under `/opt/MagAOX/calib/wcc/` and point
`catalog.path` at it.

### Where `star_catalog_simulator.toml` went

The Python simulator was driven by a TOML file. Everything in it has a home in
the MagAO-X `.conf` format, mostly because a value that used to be global is now
either per sensor or supplied live over INDI:

| TOML | `.conf` equivalent |
|---|---|
| `catalog.pointing_ra` / `pointing_dec` | `pointing.ra` / `pointing.dec`, then live on the `pointing` property |
| `catalog.gaia_catalog_file` | `catalog.path` |
| `catalog.nrows` | `catalog.mag_limit` — trimming by brightness beats trimming by row order |
| `observation.exp_time` | per sensor, from each camera's `exptime` over INDI |
| `observation.throughput` | `telescope.throughput` |
| `observation.sensor` | one config section per sensor, so a mixed IMX and HWK array is described directly rather than one type at a time |
| `sensor.<TYPE>.dark_current` / `read_noise` | `dark_current` / `read_noise` in that sensor's section |
| `sensor.<TYPE>.nominal_temp` | not carried: the calibration-file interpolation the Python did for it is not ported, so the noise values are given directly |
| `simulator.psf_arr_size` | `sim.psf_samples` |
| `simulator.npix_pupil` | `sim.npix_pupil` |

Values that vary per observation rather than per installation — the pointing, and
on the controller side the tolerances and tracking configuration — are not
duplicated in the `.conf` at all. They come from the visit file and are
republished on INDI once it loads, so the running configuration is always
visible. See `wccCtrl`'s `visit` and `visit_params` properties.

## Trying it out

With a camera simulator and a controller:

```bash
# start telescopeSim, then a camera per sensor, then the simulator
/opt/MagAOX/bin/telescopeSim -n telesim
/opt/MagAOX/bin/wccSim -n wccsim
xindi wccsim.streaming.toggle=On
```

Look at `nsv18sim` (or whatever `shmim_out` is), not the camera device stream.
A same-size occupancy stream `nsv18simdwell` is published beside it: 0 everywhere
except 1 at each detector pixel a PSF stamp was placed, so you can overlay it on
the image and see the trail. `sim.start_streaming=true` skips the toggle if you
want frames immediately.

Camera ROI / fps / exptime still arrive over INDI at 1 Hz. Pointing does not: it
is the `telpointing` shmim, correlated by simulated time at `pointing.write_hz`.
The per-sensor render threads run at whatever frame rate each camera asks for.

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
