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
- `exptime` has no maximum (INDI `exptime.max` is unbounded). A minimum of 1 µs is still enforced.
- `camera.readoutOverhead_ns` is accepted for config compatibility; it is **not** used to cap exposure.

## Operation

1. Start the app (state `CONNECTED`).
2. Toggle INDI `streaming` **On** → state `OPERATING`, frames published at `fps.current`.
3. Switch `mode` to change ROI geometry + `maxFPS` (fps clamped when `fast_cam` is Off).
4. With `fast_cam` **Off** (default): `fps` is limited to the current ROI mode
   `maxFPS` from config. `exptime` is independent of fps.
5. Toggle `fast_cam` **On** to bypass the ROI fps cap. `fps` may go up to **2000 Hz**.
   INDI `fps.max` follows this switch. Cadence follows `fps.current`.
   Auto-enables `streaming`. `llowfscSim` follows `fps.current`. Toggle Off to
   restore the mode `maxFPS` clamp.

### Key INDI properties (nsvCtrl-aligned)

| Property | Role |
|----------|------|
| `streaming` | Start/stop publishing frames |
| `fast_cam` | Bypass mode maxFPS (2000 Hz ceiling when On) |
| `mode` | ROI mode from config sections |
| `fps` | Framerate (`current` = live cadence; `target` commands it) |
| `exptime` | Exposure [s] (no maximum; independent of fps) |
| `emgain` | Analog gain (NSV uses this name for CMOS gain) |
| `blacklevel` | Black level offset |
| `bitDepth` | ADC depth; fill DN = `2^bitDepth−1` |
| `roi_*` | Software ROI within the current mode frame |

No noise / shutter / temperature simulation — constant flat frames only.
