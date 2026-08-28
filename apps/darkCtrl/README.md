# darkCtrl

MagAO-X app that builds a science-camera **dark library** for use by `iefcCtrl`.

## Build / install

```bash
make -C apps/darkCtrl
sudo make -C apps/darkCtrl install
```

## INDI registration (required)

Like other MagAO-X apps, `darkCtrl` talks to indiserver through **xindiserver**
FIFOs. Three pieces must agree on the process name (e.g. `dark`):

1. **proclist** — e.g. `dark  darkCtrl` in `proclist_*.txt`
2. **config** — `/opt/MagAOX/config/dark.conf` with a `[dark]` section
3. **xindiserver drivers** — add the process name to `is*.conf`:

```ini
# /opt/MagAOX/config/isworkstation.conf
drivers=...,iefc,dark
```

Then **restart xindiserver**. On startup it creates the bridge symlink:

```text
/opt/MagAOX/drivers/dark -> /opt/MagAOX/bin/xindidriver
```

Without that restart, `darkCtrl` can run and create `dark.in`/`dark.out` FIFOs
but never appears in `getINDI` / GUIs. Check with:

```bash
ls -l /opt/MagAOX/drivers/dark
getINDI -d dark  # or your usual INDI client
```

## Role

- Closes shutter via INDI, sweeps `dark_exptimes`, averages `dark_n_images` frames per exposure
- For each exposure:
  - If `cam_name.fast_cam` is **On**: set `exptime` only (leave fps unchanged), wait for `cam_exptime`
  - If `fast_cam` is **Off** or **absent** (real cameras): set `exptime`, wait for `cam_exptime`, then set max `fps` (≈ `1/exptime`). Missing `fast_cam` is not an error.
- Writes `dark_lib_path/dark_NNN.fits` plus `dark_lib_path/dark_metadata.txt` (CSV of camera parameters per dark)
- Stamps metadata: `shm_cam_input` (frame stream), `cam_name` (INDI device), plus live `cam_name` currents: `exptime`, `emgain`, `blacklevel`, `bitDepth`, `roi_region_{x,y,w,h}`
- `ndark` is the number of frames actually averaged
- The build **fails** (does not write `nan`) if those camera currents were never received. Check `getINDI nsvsim.{exptime,emgain,blacklevel,bitDepth,roi_region_*}` (or your `cam_name`).
- `dark_metadata.txt` is rewritten after each dark so a mid-build failure still leaves a usable library.
- When the build finishes (success, stop, or error after the shutter was closed), the shutter is opened (`shutter_device.shutter` Off) and `cam_name.exptime` is restored to the value from before the sweep. If `fast_cam` is Off/absent, fps is restored too.

Rebuild/install `darkCtrl`, `psfRefCtrl`, and `iefcCtrl`, then **regenerate** the dark library.

`iefcCtrl` loads this library via `dark_lib_path` / `reload_dark_lib`. `psfRefCtrl` uses the same path for `take_ref`.

## Sim defaults

| Config | Default | Notes |
|--------|---------|-------|
| `shm_cam_input` | `camsci_sim` | Frame stream published by `llowfscSim` |
| `cam_name` | `nsv455sim` | INDI device for `exptime` / `emgain` / `fps` / `blacklevel` / `bitDepth` / ROI |
| `shutter_device` | `llowfscsim` | Owns `shutter.toggle` (On = closed) |

## INDI (summary)

| Property | Role |
|----------|------|
| `shm_cam_input` | Camera ImageStreamIO name (frames) |
| `cam_name` | INDI camera device to follow; commands go to `cam_name.<prop>.target` |
| `shutter_device` | INDI device owning shutter toggle |
| `shutter` | Local toggle → NEW to `shutter_device.shutter` (On=closed) |
| `dark_exptimes` | CSV exposures |
| `dark_n_images` | Frames averaged per dark |
| `cam_n_frame_delay` | Skip N frames after an exposure change |
| `cam_exptime` | RO `current` from `cam_name.exptime.current` |
| `cam_emgain` | RO `current` from `cam_name.emgain.current` |
| `cam_fps` | RO `current` from `cam_name.fps.current` |
| `cam_blacklevel` | RO `current` from `cam_name.blacklevel.current` |
| `cam_bitdepth` | RO `current` from `cam_name.bitDepth.current` |
| `cam_roi_x` / `cam_roi_y` | RO from `cam_name.roi_region_x/y.current` |
| `cam_roi_width` / `cam_roi_height` | RO from `cam_name.roi_region_w/h.current` |
| `dark_lib_build` | Request: build the library |
| `stop` | Abort in-progress build |

Camera get/set is always on **`cam_name`**, e.g. `getINDI nsvsim.exptime.current` / `setINDI nsvsim.exptime.target=0.01`. This app only mirrors `.current` (no local `target` for those props).

Remote SET subscriptions (startup device names): `cam_name.exptime`, `cam_name.emgain`, `cam_name.fps`, `cam_name.blacklevel`, `cam_name.bitDepth`, `cam_name.roi_region_{x,y,w,h}`, `cam_name.fast_cam`, `shutter_device.shutter`.
