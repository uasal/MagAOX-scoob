# llowfscSim

MagAO-X **optical + detector simulator** (Python). Ports
`notebooks/rt-sims/run_camsci_cpp.ipynb` into an `XDevice` app that:

1. Follows a live camera INDI device (`cam_name`, e.g. `nsvCtrlSim`) for
   `exptime`, `emgain`, `blacklevel`, `fps`, `bitDepth` (→ detector `rebin`), and ROI
2. Follows an FSM INDI device (`fsm_name`, e.g. `fsm_sim`) for `val_1.current` / `val_2.current` [nm]
3. Owns **`shutter`** (INDI toggle; On = closed) — replace milk `camscishutter`
4. Owns **`usevortex`** (INDI toggle; On = vortex in) — replace milk `usevortex` shmim
5. Publishes frames to a configurable output shmim (`shm_output`, default `camsci_sim`)

## Dependencies

Install into the MagAO-X conda env (same as the notebook: `lina_cpp310`):

- `esc_llowfsc_sim`, `lina`, `poppy`, GPU stack
- MagAO-X Python (`purepyindi2`, `xconf`, `magaox`)

Optional in `local/python.mk`:

```make
INSTRUMENT_CONDA_ENV=lina_cpp310
```

## Build / install

```bash
make -C apps/llowfscSim install
```

## INDI registration

1. proclist: `llowfscsim  llowfscSim` and `fsmsim  fsmSim`
2. `/opt/MagAOX/config/llowfscsim.conf` (see `llowfscSim.conf.sample`)
3. `is*.conf` `drivers=...,llowfscsim,nsv455sim,fsmsim`
4. Restart xindiserver

## Operation

1. Start `fsmSim`, `nsvCtrlSim` (or real camera), and `llowfscSim`
2. Ensure milk inputs exist: **`dm01disp`** (`shm_dm_total`) and **`dm01disp00`**
   (`shm_dm_flat_channel`). The app **reads** `dm01disp00` into `M.dm_flat` (never
   writes cacao). Each frame matches `run_camsci_cpp.ipynb`:
   `M.set_dm(grab(dm01disp)/1e6, channel=0)` using magpyx's Fortran-view + `.T`
   layout. `vmag` and `opdsim` are optional. Missing streams are retried every frame.
3. Toggle **`llowfscsim.streaming`** On to publish frames at `cam_name.fps`
4. Other apps command shutter via **`llowfscsim.shutter.toggle`** (not a shmim)
5. Toggle **`llowfscsim.usevortex.toggle`** On/Off to insert or remove the vortex
   (default On; next published frame uses the new path — no process restart)
6. Other apps set camera params on **`cam_name`** only, e.g.
   `setINDI nsvsim.exptime.target=0.01` / `nsvsim.emgain.target=...`
   (this app only mirrors `.current` and does not own targets for those props)
7. Other apps command FSM via **`fsm_name.x.target` / `y.target`** [nm]; this app reads **`.current`**

## INDI properties

| Property | Role |
|----------|------|
| `streaming` | Gate frame publication |
| `shutter` | `toggle` On=closed / Off=open (owned here; other apps NEW this) |
| `usevortex` | `toggle` On=vortex in / Off=out (owned here; default On). Next snap uses the new path. |
| `cam_name` | Which camera INDI device to follow |
| `fsm_name` | Which FSM INDI device provides `x/y.current` [nm] |
| `shm_output` | Output ImageStreamIO name |
| `shm_dm_total` | Config: cacao combination shmim (default `dm01disp`), live total each frame |
| `shm_dm_flat_channel` | Config: cacao channel 00 (default `dm01disp00`), **read** as the model flat (never written) |
| `exptime` / `emgain` / `blacklevel` / `fps` / `bitDepth` | RO `current` mirrors of `cam_name` (no local `target`) |

## Migration from milk scalars

| Old milk | New |
|----------|-----|
| `camscishutter` | `llowfscsim.shutter.toggle` INDI (On=closed) |
| `usevortex` shmim | `llowfscsim.usevortex.toggle` INDI (On=vortex in) |
| `camsciexptime` | `cam_name.exptime` INDI |
| `camscigain` | `cam_name.emgain` INDI |
| `dm00disp` (FSM) | `fsm_name.x/y.current` INDI [nm] |
| `camsci` (sim write) | `shm_output` (default `camsci_sim`) |

`darkCtrl` / `iefcCtrl` in this tree talk to those INDI devices instead of milk scalars.
