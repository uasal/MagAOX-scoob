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
  `cam_exp_shmim`, `cam_gain_shmim`, `shm_cam_sub_norm`, `contrast_avg`, `iefc_mask`, `iefc_sat_mask`
- Live camera: `cam_exp`, `cam_gain` (write the milk channels above)
- Ref-PSF (`doRefPsf`): `cal_psf_exp`, `cal_psf_gain` (same milk channels; applied at job start)
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
| `cam_exp_shmim` | Exposure-time scalar shmim (name only) |
| `cam_gain_shmim` | Camera-gain scalar shmim (name only) |
| `shm_cam_sub_norm` | Continuous dark-sub + Imax-normalized camera stream |
| `contrastAvgShmim` | Scalar stream name for running-average contrast (default `contrast_avg`) |
| `iefcMaskShmim` | Binary WFS/control mask image stream (default `iefc_mask`) |
| `iefcSatMaskShmim` | Binary saturation-check mask image stream (default `iefc_sat_mask`) |

Changing a shmim name closes open streams; the next job reopens with the new name.
Automation (refPSF, dark library, calibrate, run) syncs the shutter toggle when
writing the shutter milk scalar.

## INDI properties (shared)

| Property | Role |
|----------|------|
| `nFrames` | Frames averaged per camera grab (refPSF / darks / calibrate / run) |
| `cam_n_frame_delay` | Skip N new camera frames after DM write (XOR `cam_r_delay`) |
| `cam_r_delay` | Wall-clock settle [s] after DM write (used only if `cam_n_frame_delay==0`) |
| `cam_exp` | Live exposure [s]; writes milk `cam_exp_shmim` when idle |
| `cam_gain` | Live gain; writes milk `cam_gain_shmim` when idle |
| `cal_psf_exp` | Exposure [s] for `doRefPsf` (writes `cam_exp_shmim` at job start) |
| `cal_psf_gain` | Gain for `doRefPsf` (writes `cam_gain_shmim` at job start) |
| `dir_cal` | Calibration package directory |
| `dir_psf` | Ref-PSF / dark / Imax package directory |
| `wfs_mask_path` | External FITS path for `loadWfsMask` (control+contrast; empty → `dir_cal/wfs_mask.fits`) |
| `sat_mask_path` | FITS region for raw-ADU saturation checks during calibrate |
| `sat_thresh` | Raw ADU threshold inside `sat_mask` (≥ logs a warning, does not abort); default 55000 |

## Requests

| Switch | Action |
|--------|--------|
| `doRefPsf` | Park FSM at `fsmRefTip/Tilt_nm`, poke tip/tilt, dark + PSF → `dir_psf` |
| `doDarkLibrary` | Dark library at `exptimes` CSV → `dir_psf/darks/` |
| `doCalibrate` | Native in-process calibration → FITS package in `dir_cal` (masked + full response, control); matrices cached in memory |
| `doRun` | Closed-loop run using in-memory control/response (loads `dir_cal` once if cache empty) |
| `clearDm` | Zero the configured `dm_shmim` (e.g. `dm01disp07`) |
| `loadWfsMask` | Load FITS mask as **control+contrast**; write `dir_cal/wfs_mask.fits`; remask `response_full` + rebuild control when cal data exists; publish `iefc_mask` |
| `stop` | Abort in-progress job (calibrate/run/refPSF/dark library); restores DM where applicable and returns to idle |

## INDI properties (progress / RO)

| Property | Role |
|----------|------|
| `status` | Text job status |
| `cal_mode` | Current calibration mode (1…N while measuring; 0 idle) |
| `n_cal_modes` | Total calib modes (Hadamard / FITS cube) for the active job |
| `Imax_ref` | Ref-PSF peak / NI scale (writable target; cal/refPSF still override when finished) |
| `contrast` | Last closed-loop contrast |
| `cal_probe_amp` | Probe amplitude used during calibration [m] |
| `cal_mode_amp` | Mode poke amplitude during calibration [m] |
| `cl_probe_amp` | Probe amplitude during closed-loop run [m] |
| `cl_iters` / `cl_loop_gain` / `cl_leakage` | Closed-loop run parameters |
| `contrast_avg_n` | NI frames averaged into one image before contrast is computed (sets publish cadence) |
| `contrast_avg` | RO: contrast of that block-averaged NI image (mean of mask ∩ NI>0) |

## Continuous stream

While `iefcCtrl` is running, a dedicated thread waits on the `shm_cam_input` semaphore (not
the MagAOX `loopPause` timer — which defaults to 1 s) and for every new frame
dark-subtracts + normalizes by `Imax_ref`, writing `shm_cam_sub_norm`.

`loadWfsMask` sets the live **control** mask (and contrast mask), writes
`dir_cal/wfs_mask.fits`, publishes `iefc_mask`, and — when `dir_cal` has a full-frame
response (`response_full` in memory or `response_full.fits` on disk) — remasks that
response and rebuilds the control matrix for the current `cal_reg_cond`. If `dir_cal`
does not exist yet, a warning is logged and the mask is kept for the next `doCalibrate`.
`doCalibrate` uses a previously loaded mask instead of the default annulus.

During calibrate, raw `camsci` frames (before dark/normalize/mask) are checked against
`sat_mask_path` / `sat_thresh` when a sat mask is loaded; saturation logs a warning with
`cal_mode` and continues. The mask is also published to `iefc_sat_mask`. `Imax_ref`
accepts an INDI target for manual NI scale changes (updates live `shm_cam_sub_norm` /
`contrast_avg` immediately and is preserved across dark reloads); refPSF/calibrate still
overwrite it when they finish.

Every `contrast_avg_n` of those NI frames are averaged into one image; contrast is then
the mean of pixels that are inside the **WFS/control** mask **and** strictly greater than zero
(divisor = that positive count only). The result is written to the `contrast_avg` scalar
shmim and published as INDI `contrast_avg`. Changing `contrast_avg_n` resets the block
accumulator and changes the update cadence.

Setting INDI `cal_reg_cond` (when a response is available in memory or under `dir_cal`)
queues a worker job that **loads** `dir_cal/control_matrix_reg_<tag>.fits` if it exists,
otherwise runs `beta_reg`, writes that file, and caches it. Switching back to a previous
`cal_reg_cond` reloads the saved matrix (no recompute). Status shows
`loading matrix for <value> reg param` or `gen. matrix for <value> reg param`.
`doRun` uses the same load-or-build path. A new `doCalibrate` clears prior
`control_matrix_reg_*.fits` in that package (response changed).

## Calibration package (FITS)

`doCalibrate` writes:

- `response_matrix.fits` — masked response used for control (`nmodes × nmeas`)
- `control_matrix_reg_<±X.XXXXXX>.fits` — control matrix for that `cal_reg_cond` (Cholesky `beta_reg`)
- `control_matrix.fits` — legacy alias of the most recently written reg
- modes, `wfs_mask.fits`, dark, `config.txt`
- optional `response_full.fits` when `save_response_full=true` (multi-GB; **required** to remask via `loadWfsMask` after restart). Calibrate always keeps full response in memory for same-session remask.

After the last calib mode, status shows `calibrate: computing control matrix` then
`calibrate: writing package` — the former used to look like a hang with Eigen JacobiSVD.

Masked response/control stay in process memory for subsequent `doRun` calls and are refreshed after each new calibration.
