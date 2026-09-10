# wccCtrl

The WCC high-level logic controller. It ingests a visit file, configures the sensor
array, solves astrometry across many cameras at once, offsets the telescope until
the guide star lands on its target pixel, checks and corrects roll, then narrows
the guide and roll cameras to a small region of interest and hands off to the
centroid controllers for fast guiding.

## The sequence

Each stage is a state published on `acq_state`, so an operator can always see
where the sequence is and why.

| State | What happens |
|---|---|
| `IDLE` | No visit loaded |
| `LOADED` | Visit parsed, guide and roll stars resolved onto configured sensors |
| `CONFIGURING` | ROI, frame rate, exposure time, gain commanded; `streaming` enabled |
| `CONFIRMING` | Waits for each camera to *report back* the commanded values |
| `ACQUIRING` | One fresh frame grabbed from every participating sensor |
| `SOLVING` | Sources detected and matched to the catalog on each sensor; results combined |
| `OFFSETTING` | Pointing offset sent to the telescope |
| `WAIT_TELESCOPE` | Waits for the telescope to report the move complete |
| `VERIFYING` | Re-acquire, re-solve, check the guide star against its tolerance |
| `ROLLING` | Roll maneuver sent |
| `VERIFY_ROLL` | Re-acquire, re-solve, check the roll star |
| `RECONFIGURING` | Guide and roll cameras narrowed to the tracking ROI and sped up |
| `TRACKING_START` | Centroid controllers enabled |
| `TRACKING` | Fast pointing and roll loop closed on published centroids |
| `FAILED` | Gave up; `acq_state.message` says why |

The sequence runs on its own thread. `appLogic` only publishes status, so the app
stays responsive to INDI throughout.

## Why the guide star drives the offset, not the array average

The array solution (`solveBoresight`) is computed and published every pass, and it
is what separates a pointing error from a roll error — you cannot do that from one
sensor, because a translation and a rotation are degenerate at a single field
position. But the *offset command* is driven from the guide star specifically,
because the guide star landing on its target pixel is the actual requirement. An
array-average correction would leave a residual at the guide star whenever the
solution has any rotation or plate-scale error in it.

The guide star's position is taken from the astrometric solution applied to its
catalog position, not from the nearest detection. That matters: the guide star is
frequently not the brightest thing in the frame, and picking the wrong source
would send the telescope somewhere confidently wrong.

## Sign conventions

A star moves opposite to the boresight. To move a star by `(target - actual)`, the
boresight must move by `(actual - target)`, and that displacement is what
`measureStarError` returns as a field-angle offset. The derivation is in the code
comment at that function.

Roll is recovered from the tangential component of the residual at the roll star's
field radius. For a boresight roll `dθ`, a star at field position `r` moves by
`dθ·(Yr, -Xr)` to first order; dotting with the tangential unit vector gives
`-dθ|r|`, so `dθ = -(S·t̂)/|r|` for a required star motion `S`. Both the
acquisition and the fast loop use this.

Because real telescope interfaces disagree about axis labelling, the outbound
offset is adjustable without code changes: `telescope.offset_sign` and
`telescope.offset_swap_xy`. `tcsInterface`'s `pyrNudge` needs `swap_xy=true`,
because its `x` and `y` are the wavefront sensor axes rather than the focal plane
axes — the same crossing `psfAcq` does.

## Telescope interface

Fully configurable, so the same binary drives a simulator or a real TCS:

| Config | `wccSim` as authority | `tcsInterface` |
|---|---|---|
| `device` | `wccsim` | `tcsi` |
| `offset_property` | `offset` | `pyrNudge` |
| `offset_swap_xy` | `false` | `true` |
| `roll_property` | `offset` | *(empty — no roll offset exposed)* |
| `done_property` | *(empty — use `settle_time`)* | `teldata` |
| `done_elements` | | `slewing,guider_moving` |
| `pos_property` | `pointing` | `telpos` |
| `pa_element` | `pa` | `rotoff` |

Move completion is detected by watching `done_property` until every element in
`done_elements` reads zero, with a one-second grace period first so a fast poll
cannot see the pre-move idle state and return immediately. With no `done_property`
configured it falls back to a fixed `settle_time`, which is what `psfAcq` does
after a nudge.

## Confirming, rather than assuming

`CONFIRMING` exists because commanding a camera and having a camera reconfigure
are different events. After commanding, the reported values are cleared and the
controller waits for fresh reports that match within tolerance. On timeout it
names exactly which sensor and which parameter did not confirm:

```
cameras did not confirm within 15 s: IMX-19(ROI 9576x6388 want 128x128) HWK-08(no fps report)
```

`dev::stdCamera` only applies a staged region of interest on `roi_set`, so the
controller stages `roi_region_*` and then sends the request switch.

## Visit file

`wcc_visit.sample.json` is the annotated reference. It uses
`data/LAZ_072226_001_R001.json` as the baseline schema and adds what an
acquisition sequence needs but that file does not carry:

- `TARGET_ACQUISITION.TA_SENSORS` — per sensor exposure time, frame rate, gain and
  ROI for the full-frame pass, plus `STREAM: false` to drop a sensor from the
  solution without removing it from the array
- `TARGET_X_PIX` / `TARGET_Y_PIX` on each star — the pixel it is to be placed on
- `EXP_TIME_FG`, `FRAME_RATE_FG`, `ROI_W_FG`, `ROI_H_FG` — the fine-guiding
  configuration for that star
- `TRACKING` — the centroid controller device names and the fast loop gains
- `TA_GUIDE_TOL_PIX`, `TA_ROLL_TOL_PIX`, `TA_MAX_ITERATIONS`

Every added field has a default, so an unmodified LAZ-format file still loads and
runs on the configured defaults.

**Star selection degrades rather than failing.** The visit file offers several
ranked guide and roll star candidates. `wccCtrl` walks the ranks and takes the
first whose guide star *and* a roll star both land on sensors this controller
actually has configured. A partially populated array therefore falls back to a
lower-ranked but usable pair instead of refusing to observe.

## Handoff to centroidCtrl

Once both stars are inside tolerance, the guide and roll cameras are narrowed to
`TRACKING.ROI_W` × `ROI_H` (128×128 by default) centred on each target pixel, with
the window clamped to stay on the detector, and their frame rate raised. The
controller then enables each `centroidCtrl` instance and subscribes to its
`centroid` property.

The fast loop converts the guide centroid error to a field-angle offset, applies
`LOOP_GAIN`, and sends it. Roll is taken from the roll centroid error *after*
subtracting the common pointing error measured on the guide sensor, so the two
loops do not fight each other. `track.max_offset_arcsec` rejects implausible
single corrections, and `track.deadband_px` stops it chattering on noise.

`centroidCtrl` is not part of this change; the interface it must satisfy is a
`centroid` property with `x` and `y` elements in ROI pixels, and an optional
enable toggle. All three names are configurable.

## INDI interface

| Property | Type | Purpose |
|---|---|---|
| `visit_file` | text | Path to the visit file |
| `load` | request | Parse the visit file |
| `start` | request | Begin the acquisition sequence |
| `abort` | request | Stop the sequence, return to `LOADED` |
| `acq_state` | text (RO) | `state`, `message` |
| `acq_status` | number (RO) | `iteration`, `guide_err_px`, `roll_err_px`, `n_solved`, `boresight_x`, `boresight_y`, `roll_err_deg`, `solve_rms` |
| `visit` | text (RO) | `target`, `guide_sensor`, `roll_sensor`, `ra`, `dec`, `rollpa` |
| `track_status` | number (RO) | `guide_x/y`, `guide_err_px`, `roll_x/y`, `roll_err_px`, `corrections` |

## Trying it out

```bash
/opt/MagAOX/bin/wccCtrl -n wccctrl
xindi wccctrl.visit_file.target=/opt/MagAOX/config/wcc_visit.json
xindi wccctrl.load.request=On
xindi wccctrl.start.request=On
# watch it go
xindi -m wccctrl.acq_state wccctrl.acq_status
```

## Known limitations

- Astrometric matching solves translation and rotation only. Plate scale and
  higher-order distortion are taken from configuration and not fitted, which is
  correct for a rigid array but would need extending for on-sky plate-scale
  calibration.
- The fast loop is proportional only. There is no integrator and no feed-forward,
  so it will show a steady-state lag under a constant drift rate.
- Roll during acquisition assumes the pointing error has already been removed,
  which is why the roll check runs after the guide star converges. A large
  simultaneous pointing and roll error will need more than one pass.
- The visit file's `NPOSANG`, `ROLCOUNT`, `DA01`-style dither blocks and the
  `ESC_PARAMETERS` / `LLOWFWS` sections are parsed but unused; this app covers
  target acquisition and guiding only.
- INDI subscriptions cannot be withdrawn, so loading a second visit that names
  different centroid controllers adds subscriptions rather than replacing them.
  Harmless, but a long-running process that cycles through many visits will
  accumulate them.
