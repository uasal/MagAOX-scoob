# telescopeSim

Simulates a telescope mount: takes a target from the visit, slews there at a finite
rate, then tracks with configurable jitter.

It is the **authority on where the telescope is pointed**. `wccSim` reads its
`pointing` property to decide what the sky looks like, and `wccCtrl` sends
corrections to `offset` and watches `teldata` to know when a move is done. That
makes the whole acquisition and guiding loop runnable with no hardware.

## Where it sits

```
   visit .json ──▶ visitCtrl ──(target ra/dec/rollpa)──▶ telescopeSim
                       │                                     │
                       │ (acq + tracking params)             │ pointing (current)
                       ▼                                     ▼
                    wccCtrl ──(offset x/y/roll)────────▶ telescopeSim
                       ▲                                     │
                       │ frames                              │
                    wccSim ◀───────(current pointing)─────────┘
```

`wccCtrl` never talks to `wccSim`; it commands the telescope, the telescope reports
where it now points, and the simulator renders that. The loop closes through
physics rather than through a back channel.

## Interface

| property | type | purpose |
|---|---|---|
| `start_visit` | toggle | Slew to the target `visitCtrl` is publishing |
| `pointing` | number (RO) | Current pointing: `ra`, `dec`, `pa`. **This is what wccSim reads** |
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

## The mount model

States are `IDLE → SLEWING → SETTLING → TRACKING`.

Motion is integrated against **measured** elapsed time, not against the tick rate,
so the 1 Hz INDI limit sets how often the pointing is reported rather than how
faithfully the slew is simulated. A step that covers the whole remaining distance
arrives on that step — reporting arrival a tick late would waste a full second on
every small guiding correction, which at 1 Hz is the whole correction interval.

Jitter is applied to the **reported** pointing, not accumulated into the commanded
one. That makes it a wander about where the mount actually is rather than a random
walk that would drift away without bound. The unit tests assert exactly this: the
base pointing never moves while jitter is active, and the radial offset has the
mean and rms two independent Gaussian axes imply.

`drift_x` / `drift_y` are separate and *do* accumulate, which is the point: they
give the guiding loop a systematic error to chase. Note the loop is proportional
only, so a constant drift leaves a steady-state lag.

## Configuration

See `telescopeSim.conf.sample`, which is annotated. The parameters that matter most:

- `jitter` — pointing jitter rms per axis, in arcsec. Should be a few times the
  guide tolerance to be interesting but well inside the acquisition search radius.
  Set to 0 when debugging the astrometric solution itself.
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
