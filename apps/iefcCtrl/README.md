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
- `cam_name` — INDI science-camera device. Set/query `cam_name.exptime` / `cam_name.emgain` there (`*.target` to command, `*.current` is what iefc stores for dark matching).
- `shm_cam_input` — ImageStreamIO stream; also the dark-library match key
- `dark_lib_path` / `reload_dark_lib` — load darks built by **darkCtrl** (`dark_metadata.txt` + `dark_NNN.fits`) for `shm_cam_input`
- Ref-PSF: acquisition now in **psfRefCtrl** (`take_ref`); iefc has `reload_psf_ref` to load packages
- Paths:
  - `psf_dir` — ref-PSF / Imax package (from psfRefCtrl; loaded by `reload_psf_ref` / calibrate / `cl_run`)
  - `cal_dir` — calibration package (response/control matrices)
  - `dm_cmd_path` — closed-loop DM command FITS archive (`{shm_dm}_cl_{N}.fits`)
  - `dark_lib_path` — dark library from darkCtrl
- Camera settle (mutually exclusive): `cam_n_frame_delay` **or** `cam_r_delay`
- Shared: `n_images` — frames averaged for `calibrate` grabs, `cl_run` grabs, and contrast / `shm_cam_sub_norm`
- Calibration: `cal_probe_amp`, `cal_mode_amp`, `cal_reg_cond`
- Closed-loop (`cl_*`): `cl_probe_amp`, `cl_iters`, `cl_loop_gain`, `cl_leakage`, `cl_run`, `cl_index`, `dm_reset_index`

## INDI properties (shmims)

| Property | Role |
|----------|------|
| `shm_cam_input` | Science-camera ImageStreamIO name (default `camsci`; use `camsci_sim` with llowfscSim). Dark-library match key. |
| `shm_dm` | IEFC DM write channel (e.g. `dm01disp07`) |
| `shm_cam_sub_norm` | Block-averaged dark-sub + normalized image (cadence = `n_images`) |
| `shm_contrast_avg` | Scalar stream name for running-average contrast (default milk `contrast_avg`) |
| `shm_dh_mask` | Binary WFS/control (DH) mask image stream (default milk `iefc_mask`) |
| `shm_sat_mask` | Binary saturation-check mask image stream (default milk `iefc_sat_mask`) |

Changing a shmim name closes open streams; the next job reopens with the new name.
## INDI properties (shared)

| Property | Role |
|----------|------|
| `cam_n_frame_delay` | Skip N new camera frames after DM write (XOR `cam_r_delay`) |
| `cam_r_delay` | Wall-clock settle [s] after DM write (used only if `cam_n_frame_delay==0`) |
| `n_images` | Frames averaged for `calibrate` grabs, `cl_run` grabs, and contrast / `shm_cam_sub_norm` |
| `cam_name` | INDI science-camera device. Command `cam_name.exptime.target` / `emgain.target`; iefc reads `.current` |
| `cal_dir` | Calibration package directory |
| `dm_cmd_path` | Directory for closed-loop DM command FITS (`{shm_dm}_cl_{N}.fits`) |
| `psf_dir` | Ref-PSF / dark / Imax package directory |
| `dh_mask_path` | External FITS path for `dh_mask_reload` (control+contrast; empty → `cal_dir/wfs_mask.fits`) |
| `sat_mask_path` | FITS region for raw-ADU saturation checks during calibrate |
| `sat_thresh` | Raw ADU threshold (≥ logs a warning, does not abort); default 55000 |
| `psf_max_ref` | Ref-PSF peak / NI scale (writable; calibrate/`reload_psf_ref` override when finished) |

## Requests

| Switch | Action |
|--------|--------|
| `reload_psf_ref` | Load ref-PSF package from `psf_dir` (from psfRefCtrl) → recompute peak → update `psf_max_ref` / live norm; warn if peak ≥ `sat_thresh` |
| `reload_dark_lib` | Reload/validate `dark_lib_path` (`dark_metadata.txt`) for `shm_cam_input` |
| `calibrate` | Native in-process calibration → FITS package in `cal_dir`; matrices cached in memory |
| `cal_reload` | Load existing `cal_dir` package (response/control/modes/mask) into memory for `cl_run` |
| `cl_run` | Toggle: On starts closed loop (FSM OPERATING); Off aborts. Auto-Off when the run finishes. |
| `dm_reset` | Load `{dm_cmd_path}/{shm_dm}_cl_{dm_reset_index}.fits` onto `shm_dm` and set `cl_index` to that index. Index 0 is the zero flat. Later `cl_run` writes overwrite newer files. |
| `dm_reset_index` | Archive index restored by `dm_reset` (0 = `{shm_dm}_cl_0.fits`) |
| `dh_mask_reload` | Load FITS mask as **control+contrast**; write `cal_dir/wfs_mask.fits`; remask + rebuild control when cal data exists; publish `shm_dh_mask` |
| `stop` | Abort in-progress job; restores DM where applicable and returns to idle |

## Closed-loop DM command archive

Every command published to `shm_dm` during `cl_run` is also written under `dm_cmd_path` as
`{shm_dm}_cl_{N}.fits` (same units as the shmim). Probe pokes are not archived.

- `{shm_dm}_cl_0.fits` is the zero flat, written when a run starts at `cl_index=0`.
- Each closed-loop update increments `cl_index` and writes `{shm_dm}_cl_1.fits`,
  `{shm_dm}_cl_2.fits`, …
- `cl_index` (RO INDI) is the last archived or restored index.
- `dm_reset` loads `{shm_dm}_cl_{dm_reset_index}.fits` onto `shm_dm` and sets `cl_index`
  to that value. The next `cl_run` continues from there (`cl_{index+1}`, …), overwriting
  newer files if they exist. `dm_reset_index=0` restores the zero flat
  (`dm01disp07_cl_0.fits` when `shm_dm=dm01disp07`).

## INDI properties (progress / RO)

| Property | Role |
|----------|------|
| `status` | Text job status |
| `cal_mode` | Current calibration mode (1…N while measuring; 0 idle) |
| `n_cal_modes` | Total calib modes for the active job |
| `contrast` | Last closed-loop contrast |
| `cal_probe_amp` / `cal_mode_amp` | Calibration amps [m] |
| `cl_probe_amp` / `cl_iters` / `cl_loop_gain` / `cl_leakage` | Closed-loop run parameters |
| `cl_index` | RO: last archived / restored DM command index (`{shm_dm}_cl_{N}.fits`) |
| `contrast_avg` | RO: contrast of that block-averaged NI image (mean of mask ∩ NI>0) |
| `contrast_pos_pixels` | RO: % of DH-mask pixels with NI>0 on that same block-averaged image |

## Continuous stream

While `iefcCtrl` is running, a dedicated thread waits on the `shm_cam_input` semaphore.
Each new frame is dark-subtracted and normalized by `psf_max_ref` once, then accumulated.
Every `n_images` frames that block mean is published to `shm_cam_sub_norm`, and
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
