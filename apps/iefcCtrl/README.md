# iefcCtrl

MagAO-X INDI front-end for Stream IEFC (ref PSF, dark library, calibrate, run).

All jobs run natively in-process against milk ImageStreamIO shmims via the vendored
lina IEFC library. No external binary is required.

## Build

```bash
cd /opt/MagAOX/source/MagAOX   # or this checkout
make -C apps/iefcCtrl
sudo make -C apps/iefcCtrl install
```

Registered on RTC (`apps_rtc`) and `all_buildable_apps`.

## INDI registration (required)

MagAO-X bridges apps to indiserver via **xindiserver**. Add the process-ID
(the left column in `proclist_*.txt`, e.g. `iefc`) to that machine’s
`is*.conf` `drivers=` list:

```ini
# /opt/MagAOX/config/isworkstation.conf
drivers=...,alignLoop,iefc
```

Then restart xindiserver (or resurrector). Without this, `iefcCtrl` runs and
creates FIFOs but never appears in `getINDI` / GUIs.

## Config

See `iefcCtrl.conf.sample`. Important:

- Shmim names (loaded from config, also writable via INDI group `shmims`):
  `camsci`, `dmChannel`, `fsmChannel`, `shutterShmim` (config key `shutter`),
  `exptime` / `exptimeShmim`, `gain` / `gainShmim`, `camsci_sub_norm`
- paths: `outdir`, `setupdir`, `caldir`
- amplitudes: `calib_probe_amp`, `calib_mode_amp` (calibration), `cl_probe_amp` (closed loop)

## INDI properties (shmims)

| Property | Role |
|----------|------|
| `camsci` | Science-camera ImageStreamIO name |
| `dmChannel` | IEFC DM write channel (e.g. `dm01disp07`) |
| `fsmChannel` | FSM DMcomb channel (e.g. `dm00disp01`) |
| `shutterShmim` | Shutter scalar shmim name (milk: 1=closed, 0=open) |
| `shutter` | Shutter toggle (On=closed, Off=open; matches stdCamera) |
| `exptimeShmim` | Exposure-time scalar shmim (name only) |
| `gainShmim` | Camera-gain scalar shmim (name only) |
| `camsci_sub_norm` | Continuous dark-sub + Imax-normalized camsci stream |
| `contrastAvgShmim` | Scalar stream name for running-average contrast (default `contrast_avg`) |

Changing a shmim name closes open streams; the next job reopens with the new name.
Automation (refPSF, dark library, calibrate, run) syncs the shutter toggle when
writing the shutter milk scalar.

## INDI properties (shared)

| Property | Role |
|----------|------|
| `nFrames` | Frames averaged per camsci grab (refPSF / darks / calibrate / run) |
| `waitFrames` | New camsci frames to skip after DM write before averaging |
| `delay_s` | Optional wall-clock settle after DM write |
| `exptime` | Live exposure [s]; writes milk `exptimeShmim` when idle |
| `psf_exptime` | Exposure [s] for `doRefPsf` (same milk channel; applied at job start) |
| `outdir` / `setupdir` / `caldir` | Paths |

## Requests

| Switch | Action |
|--------|--------|
| `doRefPsf` | Park FSM at `fsmRefTip/Tilt_nm`, poke tip/tilt, dark + PSF → `outdir` |
| `doDarkLibrary` | Dark library at `exptimes` CSV → `outdir/darks/` |
| `doCalibrate` | Native in-process calibration → FITS package in `caldir` (masked + full response, control); matrices cached in memory |
| `doRun` | Closed-loop run using in-memory control/response (loads `caldir` once if cache empty) |
| `clearDm` | Zero the configured `dmChannel` (e.g. `dm01disp07`) |
| `stop` | Abort in-progress job (calibrate/run/refPSF/dark library); restores DM where applicable and returns to idle |

## INDI properties (progress / RO)

| Property | Role |
|----------|------|
| `status` | Text job status |
| `calibMode` | Current calibration mode (1…N while measuring; 0 idle) |
| `nCalibModes` | Total calib modes (Hadamard / FITS cube) for the active job |
| `Imax_ref` | Last ref-PSF peak |
| `contrast` | Last closed-loop contrast |
| `calib_probe_amp` | Probe amplitude used during calibration [m] |
| `calib_mode_amp` | Mode poke amplitude during calibration [m] |
| `cl_probe_amp` | Probe amplitude during closed-loop run [m] |
| `contrast_avg_n` | Frames in running contrast average |
| `contrast_avg` | RO: current running-average contrast |

## Continuous stream

While `iefcCtrl` is running, a dedicated thread waits on the `camsci` semaphore (not
the MagAOX `loopPause` timer — which defaults to 1 s) and for every new frame
dark-subtracts + normalizes by `Imax_ref`, writing `camsci_sub_norm`.

On the same frames, contrast is computed as the mean of positive NI pixels inside the control mask
(calibration mask if available, else `caldir` `wfs_mask`, else the default half-annulus).
A running average over `contrast_avg_n` frames is written to the `contrast_avg` scalar shmim and published as INDI `contrast_avg`.

## Calibration package (FITS)

`doCalibrate` writes:

- `response_matrix.fits` — masked response used for control (`nmodes × nmeas`)
- `control_matrix.fits` — regularized control matrix (Cholesky `beta_reg`, not SVD)
- modes, `wfs_mask.fits`, dark, `config.txt`
- optional `response_full.fits` only if `save_response_full=true` (can be multi-GB)

After the last calib mode, status shows `calibrate: computing control matrix` then
`calibrate: writing package` — the former used to look like a hang with Eigen JacobiSVD.

Masked response/control stay in process memory for subsequent `doRun` calls and are refreshed after each new calibration.
