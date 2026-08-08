# iefcCtrl

MagAO-X INDI front-end for Stream IEFC (ref PSF, dark library, calibrate, closed loop).

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
  `shm_cam_input`, `shm_dm`, `shm_cam_sub_norm`, `shm_contrast_avg`,
  `shm_dh_mask`, `shm_sat_mask`
- Camera targets: `cam_exp`, `cam_gain` (NEW to `cam_name.exptime` / `emgain`; `current` tracks remote SET)
- `cam_name` — INDI science-camera device (exptime/emgain + dark-library filter)
- `dark_lib_path` / `dark_lib_load` — load darks built by **darkCtrl** (no in-app dark build)
- Ref-PSF: acquisition now in **psfRefCtrl** (`take_ref`); iefc has `reload_psf_ref` to load packages
- Paths:
  - `psf_dir` — ref-PSF / Imax package (from psfRefCtrl; loaded by `reload_psf_ref` / calibrate / `cl_run`)
  - `cal_dir` — calibration package (response/control matrices)
  - `dark_lib_path` — dark library from darkCtrl
- Camera settle (mutually exclusive): `cam_n_frame_delay` **or** `cam_r_delay`
- Calibration: `cal_n_images`, `cal_probe_amp`, `cal_mode_amp`, `cal_reg_cond`
- Closed-loop (`cl_*`): `cl_probe_amp`, `cl_iters`, `cl_loop_gain`, `cl_leakage`, `cl_contrast_avg_n`, `cl_run`

## INDI properties (shmims)

| Property | Role |
|----------|------|
| `shm_cam_input` | Science-camera ImageStreamIO name (default `camsci`; use `camsci_sim` with llowfscSim) |
| `shm_dm` | IEFC DM write channel (e.g. `dm01disp07`) |
| `shm_cam_sub_norm` | Block-averaged dark-sub + normalized image (cadence = `cl_contrast_avg_n`) |
| `shm_contrast_avg` | Scalar stream name for running-average contrast (default milk `contrast_avg`) |
| `shm_dh_mask` | Binary WFS/control (DH) mask image stream (default milk `iefc_mask`) |
| `shm_sat_mask` | Binary saturation-check mask image stream (default milk `iefc_sat_mask`) |

Changing a shmim name closes open streams; the next job reopens with the new name.
## INDI properties (shared)

| Property | Role |
|----------|------|
| `cam_n_frame_delay` | Skip N new camera frames after DM write (XOR `cam_r_delay`) |
| `cam_r_delay` | Wall-clock settle [s] after DM write (used only if `cam_n_frame_delay==0`) |
| `cam_exp` | Target exposure [s]; NEW to `cam_name.exptime`. `current` mirrors remote SET |
| `cam_gain` | Target gain; NEW to `cam_name.emgain`. `current` mirrors remote SET |
| `cam_name` | INDI science-camera device name |
| `cal_dir` | Calibration package directory |
| `psf_dir` | Ref-PSF / dark / Imax package directory |
| `dh_mask_path` | External FITS path for `dh_mask_reload` (control+contrast; empty → `cal_dir/wfs_mask.fits`) |
| `sat_mask_path` | FITS region for raw-ADU saturation checks during calibrate |
| `sat_thresh` | Raw ADU threshold (≥ logs a warning, does not abort); default 55000 |
| `cal_n_images` | Frames averaged per grab during `calibrate` |
| `psf_max_ref` | Ref-PSF peak / NI scale (writable; calibrate/`reload_psf_ref` override when finished) |

## Requests

| Switch | Action |
|--------|--------|
| `reload_psf_ref` | Load ref-PSF package from `psf_dir` (from psfRefCtrl) → recompute peak → update `psf_max_ref` / live norm; warn if peak ≥ `sat_thresh` |
| `dark_lib_load` | Validate/index `dark_lib_path` entries for `cam_name` |
| `calibrate` | Native in-process calibration → FITS package in `cal_dir`; matrices cached in memory |
| `cal_reload` | Load existing `cal_dir` package (response/control/modes/mask) into memory for `cl_run` |
| `cl_run` | Closed-loop run using in-memory control/response (loads `cal_dir` once if cache empty) |
| `dm_reset` | Zero the configured `shm_dm` |
| `dh_mask_reload` | Load FITS mask as **control+contrast**; write `cal_dir/wfs_mask.fits`; remask + rebuild control when cal data exists; publish `shm_dh_mask` |
| `stop` | Abort in-progress job; restores DM where applicable and returns to idle |

## INDI properties (progress / RO)

| Property | Role |
|----------|------|
| `status` | Text job status |
| `cal_mode` | Current calibration mode (1…N while measuring; 0 idle) |
| `n_cal_modes` | Total calib modes for the active job |
| `contrast` | Last closed-loop contrast |
| `cal_probe_amp` / `cal_mode_amp` | Calibration amps [m] |
| `cl_probe_amp` / `cl_iters` / `cl_loop_gain` / `cl_leakage` | Closed-loop run parameters |
| `cl_contrast_avg_n` | NI frames averaged for contrast / `shm_cam_sub_norm` and for `cl_run` grabs |
| `contrast_avg` | RO: contrast of that block-averaged NI image (mean of mask ∩ NI>0) |
| `contrast_pos_pixels` | RO: % of DH-mask pixels with NI>0 on that same block-averaged image |

## Continuous stream

While `iefcCtrl` is running, a dedicated thread waits on the `shm_cam_input` semaphore.
Each new frame is dark-subtracted and normalized by `psf_max_ref` once, then accumulated.
Every `cl_contrast_avg_n` frames that block mean is published to `shm_cam_sub_norm`, and
contrast is computed from **the same** mean image into `contrast_avg`. The fraction of
DH-mask pixels with NI>0 on that image is published as `contrast_pos_pixels` (percent).

`dh_mask_reload` sets the live control (+ contrast) mask, writes `cal_dir/wfs_mask.fits`,
publishes `shm_dh_mask`, and remasks/rebuilds control when `cal_dir` has `response_full`.
`calibrate` uses a previously loaded mask instead of the default annulus.

`reload_psf_ref` loads a ref-PSF package created by **psfRefCtrl** and updates `psf_max_ref`,
the live normalization, and contrast accumulator.

During calibrate, raw frames are checked against `sat_mask_path` / `sat_thresh` when a
sat mask is loaded; saturation warns with `cal_mode` and continues. The mask is published
to `shm_sat_mask`.

Setting `cal_reg_cond` loads or rebuilds `cal_dir/control_matrix_reg_<tag>.fits`.
`cl_run` uses the same path. A new `calibrate` clears prior `control_matrix_reg_*.fits`.

After a restart, use `cal_reload` to restore response/control/modes/mask from `cal_dir`
(uses current `cal_reg_cond` to pick or rebuild the tagged control matrix). Then `cl_run`
can proceed without re-calibrating.

## Calibration package (FITS)

`calibrate` writes:

- `response_matrix.fits` — masked response (`nmodes × nmeas`)
- `control_matrix_reg_<±X.XXXXXX>.fits` — control for that `cal_reg_cond`
- `control_matrix.fits` — legacy alias
- modes, `wfs_mask.fits`, dark, `config.txt` (includes `psf_max_ref`; also writes legacy `Imax_ref`)
- optional `response_full.fits` when `save_response_full=true` (needed to remask after restart)

Masked response/control stay in memory for subsequent `cl_run` calls.
