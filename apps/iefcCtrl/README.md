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

- Shmim names (config + INDI group `shmims`):
  `shm_cam_input`, `dm_shmim`, `fsm_shmim`, `shutterShmim` (config key `shutter`),
  `exptime` / `exptimeShmim`, `gain` / `gainShmim`, `shm_cam_sub_norm`
- Paths (`dir_*`):
  - `dir_psf` — ref-PSF / dark / Imax package (doRefPsf / doDarkLibrary write here; calibrate/run read it)
  - `dir_cal` — calibration package (response/control matrices)
- Camera settle (mutually exclusive): `cam_n_frame_delay` **or** `cam_r_delay`
- Calibration amps: `cal_probe_amp`, `cal_mode_amp`, `cal_reg_cond`
- Closed-loop (`cl_*`): `cl_probe_amp`, `cl_iters`, `cl_loop_gain`, `cl_leakage`

## INDI properties (shmims)

| Property | Role |
|----------|------|
| `shm_cam_input` | Science-camera ImageStreamIO name (default milk value `camsci`) |
| `dm_shmim` | IEFC DM write channel (e.g. `dm01disp07`) |
| `fsm_shmim` | FSM DMcomb channel (e.g. `dm00disp01`) |
| `shutterShmim` | Shutter scalar shmim name (milk: 1=closed, 0=open) |
| `shutter` | Shutter toggle (On=closed, Off=open; matches stdCamera) |
| `exptimeShmim` | Exposure-time scalar shmim (name only) |
| `gainShmim` | Camera-gain scalar shmim (name only) |
| `shm_cam_sub_norm` | Continuous dark-sub + Imax-normalized camera stream |
| `contrastAvgShmim` | Scalar stream name for running-average contrast (default `contrast_avg`) |

Changing a shmim name closes open streams; the next job reopens with the new name.
Automation (refPSF, dark library, calibrate, run) syncs the shutter toggle when
writing the shutter milk scalar.

## INDI properties (shared)

| Property | Role |
|----------|------|
| `nFrames` | Frames averaged per camera grab (refPSF / darks / calibrate / run) |
| `cam_n_frame_delay` | Skip N new camera frames after DM write (XOR `cam_r_delay`) |
| `cam_r_delay` | Wall-clock settle [s] after DM write (used only if `cam_n_frame_delay==0`) |
| `exptime` | Live exposure [s]; writes milk `exptimeShmim` when idle |
| `psf_exptime` | Exposure [s] for `doRefPsf` (same milk channel; applied at job start) |
| `dir_cal` | Calibration package directory |
| `dir_psf` | Ref-PSF / dark / Imax package directory |

## Requests

| Switch | Action |
|--------|--------|
| `doRefPsf` | Park FSM at `fsmRefTip/Tilt_nm`, poke tip/tilt, dark + PSF → `dir_psf` |
| `doDarkLibrary` | Dark library at `exptimes` CSV → `dir_psf/darks/` |
| `doCalibrate` | Native in-process calibration → FITS package in `dir_cal` (masked + full response, control); matrices cached in memory |
| `doRun` | Closed-loop run using in-memory control/response (loads `dir_cal` once if cache empty) |
| `clearDm` | Zero the configured `dm_shmim` (e.g. `dm01disp07`) |
| `stop` | Abort in-progress job (calibrate/run/refPSF/dark library); restores DM where applicable and returns to idle |

## INDI properties (progress / RO)

| Property | Role |
|----------|------|
| `status` | Text job status |
| `cal_mode` | Current calibration mode (1…N while measuring; 0 idle) |
| `n_cal_modes` | Total calib modes (Hadamard / FITS cube) for the active job |
| `Imax_ref` | Last ref-PSF peak |
| `contrast` | Last closed-loop contrast |
| `cal_probe_amp` | Probe amplitude used during calibration [m] |
| `cal_mode_amp` | Mode poke amplitude during calibration [m] |
| `cl_probe_amp` | Probe amplitude during closed-loop run [m] |
| `cl_iters` / `cl_loop_gain` / `cl_leakage` | Closed-loop run parameters |
| `contrast_avg_n` | Frames in running contrast average |
| `contrast_avg` | RO: current running-average contrast |

## Continuous stream

While `iefcCtrl` is running, a dedicated thread waits on the `shm_cam_input` semaphore (not
the MagAOX `loopPause` timer — which defaults to 1 s) and for every new frame
dark-subtracts + normalizes by `Imax_ref`, writing `shm_cam_sub_norm`.

On the same frames, contrast is computed as the mean of positive NI pixels inside the control mask
(calibration mask if available, else `dir_cal` `wfs_mask`, else the default half-annulus).
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
