# psfRefCtrl

MagAO-X app that takes PSF references with FSM pokes for use by `iefcCtrl`.

## Build / install

```bash
make -C apps/psfRefCtrl
sudo make -C apps/psfRefCtrl install
```

## INDI registration (required)

Like other MagAO-X apps, `psfRefCtrl` talks to indiserver through **xindiserver**
FIFOs. Three pieces must agree on the process name (e.g. `psfref`):

1. **proclist** — e.g. `psfref  psfRefCtrl` in `proclist_*.txt`
2. **config** — `/opt/MagAOX/config/psfref.conf` with a `[ref]` section
3. **xindiserver drivers** — add the process name to `is*.conf`:

```ini
# /opt/MagAOX/config/isworkstation.conf
drivers=...,iefc,dark,psfref
```

Then **restart xindiserver**. On startup it creates the bridge symlink:

```text
/opt/MagAOX/drivers/psfref -> /opt/MagAOX/bin/xindidriver
```

Without that restart, `psfRefCtrl` can run and create `psfref.in`/`psfref.out` FIFOs
but never appears in `getINDI` / GUIs. Check with:

```bash
ls -l /opt/MagAOX/drivers/psfref
getINDI -d psfref  # or your usual INDI client
```

## Role

- Takes reference PSF with FSM pokes for PSF centering
- Parks FSM at reference position, then pokes to reference + poke offsets
- Picks matching dark from `dark_lib_path` (via `lina::pick_dark_from_library`)
- Saves package directory with:
  - `dark_avg.fits` (copied from library)
  - `ref_psf_avg.fits` (raw averaged frames)
  - `ref_psf_dark_sub.fits` (dark-subtracted)
  - `config.txt` (all metadata for iefcCtrl)
- Updates INDI `max_ref` property with peak intensity
- Warns if any pixel >= `sat_thresh` (does not abort)

## Requirements

- `dark_lib_path` must be set and contain `dark_metadata.txt` + `dark_NNN.fits` (built by `darkCtrl`)
- `fsm_name` must respond to `x.target` / `y.target` [nm]
- `cam_name` must publish `exptime`, `emgain`, `blacklevel` (SET subscriptions)

## Sim defaults

| Config | Default | Notes |
|--------|---------|-------|
| `shm_cam_input` | `camsci_sim` | Frame stream published by `llowfscSim` |
| `cam_name` | `nsvsim` | INDI device for `exptime` / `emgain` / `blacklevel` |
| `fsm_name` | `fsm_sim` | INDI FSM for tip/tilt positioning |
| `dir` | `./ref_psf` | Package output directory |

## INDI (summary)

| Property | Role |
|----------|------|
| `shm_cam_input` | Camera ImageStreamIO name (frames + dark-library match key) |
| `cam_name` | INDI camera device for exptime/emgain/blacklevel |
| `fsm_name` | INDI FSM device for tip/tilt positioning [nm] |
| `dark_lib_path` | Directory containing `dark_metadata.txt` + `dark_NNN.fits` (REQUIRED) |
| `dir` | Package output directory |
| `n_frames` | Frames averaged for reference PSF |
| `fsm_poke_tip` | FSM poke tip offset [nm] |
| `fsm_poke_tilt` | FSM poke tilt offset [nm] |
| `fsm_ref_tip` | FSM reference tip position [nm] |
| `fsm_ref_tilt` | FSM reference tilt position [nm] |
| `max_ref` | Max reference intensity (RW, updated after take_ref) |
| `sat_thresh` | Saturation threshold (warn only) |
| `settle_s` | Wall-clock settle after FSM park/poke [s] |
| `cam_n_frame_delay` | Skip N frames after FSM move |
| `take_ref` | Request: take reference PSF |
| `stop` | Abort in-progress take_ref |

Remote SET subscriptions (startup device names): `cam_name.exptime`, `cam_name.emgain`, `cam_name.blacklevel`.

## Workflow

1. User triggers `take_ref` via INDI
2. App parks FSM at `(fsm_ref_tip, fsm_ref_tilt)`, waits `settle_s`
3. App pokes FSM to `(fsm_ref_tip + fsm_poke_tip, fsm_ref_tilt + fsm_poke_tilt)`, waits `settle_s`
4. App reads live camera params from INDI SET subscriptions
5. App picks closest dark from `dark_lib_path` using `lina::pick_dark_from_library` with filter on `shm_cam_input` and `gain` (if finite)
6. App copies dark to `dir/dark_avg.fits`
7. App grabs `n_frames` from `shm_cam_input`, averages
8. If any pixel >= `sat_thresh`, logs WARNING (does not abort)
9. App dark-subtracts, computes peak max
10. App saves `ref_psf_avg.fits`, `ref_psf_dark_sub.fits`
11. App writes `config.txt` with all metadata
12. App updates INDI `max_ref` to peak
13. App restores FSM to `(fsm_ref_tip, fsm_ref_tilt)`

## Config.txt keys

The generated `config.txt` includes (for `iefcCtrl` compatibility):

- `camsci`, `fsm_name`, `cam_name`
- `exptime`, `exposure`, `emgain`, `blacklevel` (from live INDI)
- `psf_exptime`, `psf_gain`, `cam_exp`, `cam_gain`, `gain` (aliases)
- `width`, `height` (image size)
- `psf_max_ref`, `Imax_ref`, `peak_dark_sub` (aliases for peak intensity)
- `tip_nm`, `tilt_nm` (poke offsets)
- `ref_tip_nm`, `ref_tilt_nm` (reference position)
- `dark_lib_path`, `npsf` (frame count)
- `dark_file`, `ref_psf_file`, `ref_psf_dark_sub_file` (relative paths)
