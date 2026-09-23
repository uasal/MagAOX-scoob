# telescopeSim

Simulates a telescope mount: takes a target from the visit, slews there at a finite
rate, then tracks with configurable jitter.

It is the **authority on where the telescope is pointed**. The current pointing is
written to the `telpointing` ImageStreamIO stream at a configurable tick rate
(5000 Hz of simulated time by default) so `wccSim` can integrate mount motion
during a camera exposure. Each write is one tick of duration `1/write_hz`; the
writer is not paced to the computer clock unless `pointing.pace_wallclock` is set.
The same pointing is published on INDI at 1 Hz for operators and `wccCtrl`.
`wccCtrl` sends corrections to `offset` and watches `teldata` to know when a move
is done.

## Where it sits

```
   visit .json ──▶ visitCtrl ──(target ra/dec/rollpa)──▶ telescopeSim
                       │                                     │
                       │ (acq + tracking params)             │ telpointing shmim (sim ticks)
                       ▼                                     ▼
                    wccCtrl ──(offset x/y/roll)────────▶ telescopeSim
                       ▲                                     │
                       │ frames (camera fps)                 │ pointing INDI (1 Hz)
                    wccSim ◀───────(pointing history)─────────┘
```

`wccCtrl` never talks to `wccSim`; it commands the telescope, the telescope reports
where it now points, and the simulator renders that. The loop closes through
physics rather than through a back channel.

## Interface

| property | type | purpose |
|---|---|---|
| `start_visit` | toggle | Slew to the target `visitCtrl` is publishing |
| `pointing` | number (RO) | Current pointing: `ra`, `dec`, `pa`. 1 Hz INDI status |
| `target` | number (RO) | Where it is going: `ra`, `dec`, `pa` |
| `teldata` | number (RO) | `slewing`, `settling`, `tracking` — done detection |
| `tel_status` | text (RO) | `state`, `message` |
| `offset` | number | Relative correction: `x`, `y` [arcsec, focal plane], `roll` [deg]. Self-clearing |
| `goto_target` | number | Staged slew: `ra`, `dec`, `pa`. Does not move the mount |
| `goto` | toggle | Submit a slew to `goto_target`, then track with jitter |
| `stop_tracking` | toggle | On: stop tracking, drift at `idle_drift_rate`. Off: track wherever the mount points |
| `tracking_drift_rate` | number | Drift while tracking: `ra`, `dec` [arcsec/s], `pa` [deg/s] |
| `idle_drift_rate` | number | Drift while not tracking: `ra`, `dec` [arcsec/s], `pa` [deg/s] |
| `jitter_x` | number | Live jitter rms along focal plane X [arcsec]. `current` / `target` |
| `jitter_y` | number | Live jitter rms along focal plane Y [arcsec] |
| `jitter_roll` | number | Live roll (image rotation) jitter rms [deg] |
| `slew_rate` | number | Live slew rate [deg/s] |
| `roll_rate` | number | Live rotation rate [deg/s] |
| `settle_time` | number | Live settle time [s] |
| `arrive_tol` | number | Live arrival tolerance [arcsec] |
| `jitter_tau` | number | Live jitter correlation time [s] |
| `write_hz` | number | Live pointing tick rate [Hz of simulated time] |
| `history_s` | number | Live pointing buffer span [s of simulated time] |

`teldata` is named to match `tcsInterface`'s property of the same name, so pointing
a controller's done-detection at either is a configuration change rather than a code
change.

`start_visit` is refused unless `visitCtrl` reports `visit_status.state == LOADED`,
so it cannot slew to a half-delivered or stale target. `goto_target` plus the
`goto` toggle do not need a visit. Set `ra` / `dec` / `pa` on `goto_target` to
stage a command; that does not move the mount. Turning `goto` on submits the
slew to those coordinates and then tracks with the configured jitter. Turning
`goto` off stops tracking where the mount is.

`stop_tracking` is the mount's tracking switch. It is On at startup, because the
mount has not been told to track anything yet. Turning it On while tracking sends
the mount to `IDLE`, where it drifts at `idle_drift_rate` (sidereal, 15.041 arcsec/s
in RA, for a stopped equatorial mount) with no jitter. A slew in progress finishes,
then goes idle instead of settling. Turning it Off from idle tracks wherever the
mount now points, drift included: the target becomes the current pointing, the
mount settles, then reports `TRACKING`. `goto` and `start_visit` turn it Off;
turning either of those off turns it On. `offset` leaves it alone, so an offset
while stopped moves the mount and it stays stopped.

`tracking_drift_rate` is the residual error guiding must remove; `idle_drift_rate`
is what the sky does to a mount that is not tracking. Both are per axis. The `ra`
rate is in arcsec **of right ascension** per second, not scaled by cos(Dec), so
sidereal is 15.041 at any declination. No drift is applied during a slew. Each
element applies as soon as it arrives; `nan` elements are ignored and the vector is
SET back from the values in use.

### Offsets and the focal-plane to sky conversion

`offset.x` / `offset.y` are in focal-plane arcsec and `offset.roll` is degrees
about the boresight. An external app (normally `wccCtrl`) writes them; they are
applied relative to where the mount is now (drift included), or to the slew
destination if a slew is in progress, then cleared back to 0. `nan` elements
are ignored. With parity `p` and position angle `PA` (focal plane +Y, east of
north), a focal-plane offset `(x, y)` becomes tangent-plane east/north offsets

```
xi  (east)  =  p*x*cos(PA) + y*sin(PA)
eta (north) = -p*x*sin(PA) + y*cos(PA)
```

so for small offsets `dRA ≈ xi / cos(Dec)` and `dDec ≈ eta`. Roll adds directly
to PA. The exact gnomonic inverse is `wcc::offsetBoresight()` in
`wccFocalPlane.hpp`, which `wccSim` also uses, so the telescope and the rendered
sky cannot disagree. `jitter_x` / `jitter_y` go through the same conversion, so
they are the image-plane directions on the camera; `jitter_roll` is a rotation
and moves stars in proportion to their distance from the boresight.

The 1 Hz INDI `pointing` property (`ra`, `dec`, `pa`) is the current boresight
for operators and `wccCtrl`. The same coordinates are written at `write_hz` into
the `telpointing` shmim, which is what `wccSim` integrates over a camera exposure.

The high-rate pointing stream is a 4×1×N circular buffer of doubles named
`telpointing` by default (configurable as `pointing.shmim`). Axes are RA, Dec, PA
in degrees, and simulated time in seconds. `pointing.write_hz` is the tick rate of
**simulated** time — not wall-clock — and `pointing.history_s` sizes the buffer so
the longest camera exposure can be reconstructed from ticks that have already been
written. `wccSim` correlates an exposure of `T` seconds with the ticks whose
sim-time falls in `[now - T, now]`, where `now` is the newest slice. The computer
clock is not involved, which is required because the camera simulators are allowed
to run faster than real time (a 100 s exposure at 10 fps is a valid command).

Each of `jitter_x`, `jitter_y`, `jitter_roll`, `slew_rate`, `roll_rate`,
`settle_time`, `arrive_tol`, `jitter_tau`, `write_hz` and `history_s` is a MagAO-X
standard number (`current` / `target`). A finite `target` (or `current` if there
is no finite target) is applied; missing, empty, and `nan` values are ignored, and
the property is SET back to the value in use. MagAO-X GUIs send `nan` for every
unedited element, so this matters. Setting `telescopesim.jitter_x.target=0.5` or
`telescopesim.write_hz.target=2000` updates the running mount on the next tick
without stopping INDI or the pointing worker.

NaN checks use `wcc::isFinite()` (`wccCommon/wccNumeric.hpp`), which tests the
IEEE exponent bits. MagAO-X builds with `-ffast-math`, under which GCC deletes
`std::isfinite()` checks as always-true; that is how `nan` from a GUI used to
reach `jitter_roll`, `jitter_tau` and `settle_time`, and through the jitter model,
`pointing.pa`.
A change of `write_hz` or `history_s` that changes the buffer depth stops the
worker, recreates `telpointing`, and starts the worker again; `wccSim` closes and
reopens its mmap when it sees that SET.

## The mount model

States are `IDLE → SLEWING → SETTLING → TRACKING`.

Motion is integrated in **simulated** time. The pointing worker advances the mount
by `1/write_hz` on every write. It does not sleep to match that rate in wall-clock
time unless `pointing.pace_wallclock` is true. INDI stays at 1 Hz.
A step that covers the whole remaining distance arrives on that step, and so does
a target within `arrive_tol`: the mount snaps onto it. (Before, a target within
tolerance was declared arrived without moving, which silently dropped every
guiding offset of 1 arcsec or less.)

Jitter is applied to the **reported** pointing, not accumulated into the commanded
one. `jitter_x` and `jitter_y` are independent focal-plane axes; `jitter_roll` is a
separate rotation about the boresight. With `jitter_tau` > 0 each is an
Ornstein-Uhlenbeck wander (smooth at kHz, the configured rms in the long run),
which a long exposure integrates into a streak. `jitter_tau = 0` is white. The unit
tests assert that the base pointing never moves while jitter is active, that each
axis has its configured rms, and that roll jitter does not translate the
boresight. No jitter is applied while idle.

Drift is separate and *does* accumulate into the base pointing, which is the
point: `tracking_drift_rate` gives the guiding loop a systematic error to chase.
Note the loop is proportional only, so a constant drift leaves a steady-state lag.

## Configuration

See `telescopeSim.conf.sample`, which is annotated. The parameters that matter most:

- `jitter_x` / `jitter_y` — pointing jitter rms along focal plane X and Y, in
  arcsec. Should be a few times the guide tolerance to be interesting but well
  inside the acquisition search radius. Set to 0 when debugging the astrometric
  solution itself. The older `jitter` key still works and sets both axes; the
  per-axis keys override it.
- `jitter_roll` — roll (image rotation) jitter rms, in degrees.
- `tracking_drift_ra` / `_dec` / `_pa`, `idle_drift_ra` / `_dec` / `_pa` — drift
  rates [arcsec/s, arcsec/s, deg/s]. `idle_drift_ra=15.041` is sidereal.
  `drift_x` / `drift_y` are retired: still accepted so old configs load, but
  ignored (with a warning if nonzero).
- `jitter_tau` — jitter correlation time in seconds. 0 is white; ~0.05 s streaks
  a 1 s exposure instead of turning it into a blob.
- `pointing.write_hz` — pointing tick rate in Hz of simulated time. 5000 Hz by
  default. 0 disables the stream and updates only at the 1 Hz INDI tick.
- `pointing.history_s` — circular-buffer span in seconds of simulated time. Must
  cover the longest camera exposure.
- `pointing.pace_wallclock` — if true, sleep so writes match `write_hz` in real
  time. Default false.
- `slew_rate` — raise it well above a real mount's if you want simulated
  acquisition sequences to finish quickly.
- `settle_time` — `wccCtrl` waits for this before it re-solves, so it should
  reflect real mount settling.
- `parity` — **must match `wccSim` and `wccCtrl`**, otherwise a commanded correction
  moves the sky the wrong way and the loop diverges instead of converging.

## Running a visit

```bash
xindi visitctrl.visit_file.target=/opt/MagAOX/config/wcc_visit.json
xindi visitctrl.load.toggle=On
xindi telescopesim.start_visit.toggle=On    # slews to the visit target
xindi telescopesim.jitter_x.target=0.5      # live mount-model change
xindi telescopesim.write_hz.target=2000
xindi wccsim.streaming.toggle=On            # images follow the mount
xindi wccctrl.start.request=On               # acquire and hand off to guiding
```

Toggling `start_visit` off stops tracking where the mount is rather than returning
it anywhere.

Stop and resume tracking, and set drift:

```bash
xindi telescopesim.stop_tracking.toggle=On          # drift at idle_drift_rate
xindi telescopesim.idle_drift_rate.ra=15.041        # sidereal
xindi telescopesim.stop_tracking.toggle=Off         # track wherever it now points
xindi telescopesim.tracking_drift_rate.dec=0.05     # 0.05 arcsec/s for guiding to chase
xindi telescopesim.offset.y=1.0                     # 1 arcsec along focal plane +Y
```

Without a visit, stage a target then submit the slew:

```bash
xindi telescopesim.goto_target.ra=192.5
xindi telescopesim.goto_target.dec=26.84
xindi telescopesim.goto_target.pa=0
xindi telescopesim.goto.toggle=On
```

Changing `goto_target` while the mount is already moving does not start a new
slew. Turn `goto` off then on again to submit the staged coordinates. Turning
`goto` off stops tracking where the mount is.

## Known limitations

- The slew interpolates linearly in RA and Dec rather than along a great circle.
  At simulator step sizes the difference is negligible and the mount still
  approaches monotonically, but it is not a faithful mount trajectory.
- There is no mount model beyond rate limits: no acceleration, no backlash, no
  pointing model residuals, no field rotation tracking error.
- Only the offset convention is shared with `wccSim`, through
  `wccCommon::offsetBoresight`. Everything else about the two apps' geometry is
  independently configured and has to be kept consistent by hand — `parity` in
  particular.
- With `pace_wallclock=false` (the default) simulated time can run far ahead of
  wall-clock, so consecutive camera frames integrate disjoint stretches of mount
  motion. Set `pace_wallclock=true` if you want writes to track real time.

## Tests

```bash
cd tests
make -B -f Makefile.one t=../apps/telescopeSim/tests/telescopeSim_test.cpp
../apps/telescopeSim/tests/telescopeSim_test
```

Covers the slew taking the right number of steps and not overshooting, the settle
being waited out, roll slewing at its own rate, a target within `arrive_tol` being
snapped onto, a clock step being ignored rather than integrated into a teleport,
per-axis jitter amplitude and its non-accumulation, roll jitter not translating,
the field-angle offset convention including the parity flip and pole clamping, the
`offset` callback (sub-tolerance offsets, `nan` elements, offsets during a slew),
`stop_tracking` and both drift rates, and the reported bug: a `jitter` NEW with
`nan` siblings leaving `jitter_roll`, `jitter_tau`, `settle_time` and
`pointing.pa` finite. The test build uses the same `-ffast-math` as the apps, so
it exercises the NaN guards the way production does.
