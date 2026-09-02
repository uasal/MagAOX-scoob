# nsvCtrlSim

MagAO-X **NSV camera simulator**: `stdCamera` + `frameGrabber` without V4L2 hardware.
Publishes constant max-DN frames into an ImageStreamIO shmim at a mode-limited framerate.

## Build / install

```bash
make -C apps/nsvCtrlSim
sudo make -C apps/nsvCtrlSim install
```

## INDI registration

Add process name (e.g. `nsv455sim`) to proclist and xindiserver `drivers=`, then restart
xindiserver (creates `/opt/MagAOX/drivers/<name> → xindidriver`).

## Config

See `nsvCtrlSim.conf.sample` (based on an `nsv455.conf` ROI table).

Important:

- Each ROI **mode** is an unused INI section with `configFile=dummy` (required by
  `stdCamera::loadCameraConfig`), `centerX/Y`, `sizeX/Y`, and `maxFPS`.
- `camera.startupMode` selects the initial mode.
- `camera.bitDepth` sets the constant fill value `2^bitDepth - 1` (UINT16 shmim).
- `camera.readoutOverhead_ns` (default 100) caps exposure when `fast_cam` is Off:
  `max_exptime = 1/fps − overhead`.

## Operation

1. Start the app (state `CONNECTED`).
2. Toggle INDI `streaming` **On** → state `OPERATING`, frames published at `fps.current`.
3. Switch `mode` to change ROI geometry + `maxFPS` (fps clamped; exptime re-capped).
4. With `fast_cam` **Off** (default): `fps` is limited to the current ROI mode
   `maxFPS` from config, and `exptime` is limited by `1/fps − readout overhead`.
5. Toggle `fast_cam` **On** to bypass those ROI/exposure limits. `fps` may go up
   to **2000 Hz** and `exptime` is independent of fps (absolute max 3600 s).
   INDI `fps.max` / `exptime.max` follow this switch so GUIs match the live
   limits. Cadence follows `fps.current`. Auto-enables `streaming`.
   `llowfscSim` follows `fps.current`. Toggle Off to restore the mode `maxFPS`
   clamp and the fps↔exptime cap.

### Key INDI properties (nsvCtrl-aligned)

| Property | Role |
|----------|------|
| `streaming` | Start/stop publishing frames |
| `fast_cam` | Bypass mode maxFPS and fps↔exptime mutual limits |
| `mode` | ROI mode from config sections |
| `fps` | Framerate (`current` = live cadence; `target` commands it) |
| `exptime` | Exposure [s] (capped by fps only when `fast_cam` is Off) |
| `emgain` | Analog gain (NSV uses this name for CMOS gain) |
| `blacklevel` | Black level offset |
| `bitDepth` | ADC depth; fill DN = `2^bitDepth−1` |
| `roi_*` | Software ROI within the current mode frame |

No noise / shutter / temperature simulation — constant flat frames only.
