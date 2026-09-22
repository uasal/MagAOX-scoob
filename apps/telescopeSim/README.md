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
| `offset` | number | Relative correction: `x`, `y` [arcsec], `roll` [deg]. Self-clearing |
| `goto_target` | number | Absolute command: `ra`, `dec`, `pa` |

`teldata` is named to match `tcsInterface`'s property of the same name, so pointing
a controller's done-detection at either is a configuration change rather than a code
change.

`start_visit` is refused unless `visitCtrl` reports `visit_status.state == LOADED`,
so it cannot slew to a half-delivered or stale target.

The high-rate pointing stream is a 4×1×N circular buffer of doubles named
`telpointing` by default (configurable as `pointing.shmim`). Axes are RA, Dec, PA
in degrees, and simulated time in seconds. `pointing.write_hz` is the tick rate of
**simulated** time — not wall-clock — and `pointing.history_s` sizes the buffer so
the longest camera exposure can be reconstructed from ticks that have already been
written. `wccSim` correlates an exposure of `T` seconds with the ticks whose
sim-time falls in `[now - T, now]`, where `now` is the newest slice. The computer
clock is not involved, which is required because the camera simulators are allowed
to run faster than real time (a 100 s exposure at 10 fps is a valid command).

## The mount model

States are `IDLE → SLEWING → SETTLING → TRACKING`.

Motion is integrated in **simulated** time. The pointing worker advances the mount
by `1/write_hz` on every write. It does not sleep to match that rate in wall-clock
time unless `pointing.pace_wallclock` is true. INDI stays at 1 Hz.
A step that covers the whole remaining distance arrives on that step.

Jitter is applied to the **reported** pointing, not accumulated into the commanded
one. With `jitter_tau` > 0 it is an Ornstein-Uhlenbeck wander (smooth at kHz, the
configured rms in the long run), which a long exposure integrates into a streak.
`jitter_tau = 0` is white, matching the original independent 1 Hz draws. The unit
tests assert that the base pointing never moves while jitter is active, and that
the radial offset has the mean and rms two independent Gaussian axes imply.

`drift_x` / `drift_y` are separate and *do* accumulate, which is the point: they
give the guiding loop a systematic error to chase. Note the loop is proportional
only, so a constant drift leaves a steady-state lag.

## Configuration

See `telescopeSim.conf.sample`, which is annotated. The parameters that matter most:

- `jitter` — pointing jitter rms per axis, in arcsec. Should be a few times the
  guide tolerance to be interesting but well inside the acquisition search radius.
  Set to 0 when debugging the astrometric solution itself.
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
xindi wccsim.streaming.toggle=On            # images follow the mount
xindi wccctrl.start.request=On               # acquire and hand off to guiding
```

Toggling `start_visit` off parks the mount where it is rather than returning it
anywhere.

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
being waited out, roll slewing at its own rate, a clock step being ignored rather
than integrated into a teleport, jitter amplitude and its non-accumulation, and the
field-angle offset convention including the parity flip and pole clamping.
