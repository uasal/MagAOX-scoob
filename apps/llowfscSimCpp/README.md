# llowfscSimCpp

C++ MagAO-X conversion based on llowfscSim python app utilizing the **same** `esc_llowfsc_sim` optical model. 
Sets up core XWCTk functionality, shmim management, INDI setup, app loop in C++ however bridges the python optical model.

How embedded CPython works: a dedicated worker thread
creates `esc.single` once (Python) and then calls `M.snap_camsci()` /
`M.snap_camlo()` at the camera fps cadence. INDI, ImageStreamIO, and the loop
timer stay in C++.

Fraunhofer snap is slow. `snap_camsci` is still GPU Python (`calc_wfs_camsci`). 
This is the limitation for going faster 
than 500Hz currently. 

`optical_bridge.snap()` is the Python `_rt_snap` path. Applies live state (reloaded from 
camera parameters, fsm location, etc.), then
propagates. 

## Layout (compare with Python `llowfscSim`)

| | Python `llowfscSim` | `llowfscSimCpp` |
|--|--|--|
| Process / INDI name | `llowfscsim` | `llowfscsimcpp` (choose in proclist) |
| Optical model | `esc_llowfsc_sim` | same, via `optical_bridge.py` |
| Host | MagAO-X `XDevice` | MagAO-X C++ `MagAOXApp` |
| Output shmim | `shm_output` (default `camsci_sim`) | same |
| Shutter owner | this device | this device (`darkCtrl` `shutter_device`) |
| FSM follow | Python currently polls `val_1`/`val_2` | same: **`fsm_sim.val_1` / `val_2` only** |

## Build / install

Needs the MagAO-X **base** conda env (`/opt/conda`) with `esc_llowfsc_sim`,
`lina`, `numpy`, `tomlkit` — the same interpreter MagAO-X Python apps use
(`/opt/conda/bin/python`). 

after 'make install' 
Installs the binary and `/opt/MagAOX/python/llowfscSimCpp/optical_bridge.py`.

After install, the running log should show `Python prefix=/opt/conda` (base),
matching MagAO-X Python apps. Optional `sim.python_prefix=/opt/conda` in
`llowfscsimcpp.conf`. The `optical model ready` line should include `xp=cupy`
if the GPU stack is in that env.

Optimizations done by the C++ to gain slightly faster computation:

- Overlap vortex low-res FFT and high-res MFT on two CUDA streams
- Cache the astropy flux conversion (it ran every frame)
- Cast to float32 on GPU and reuse a host buffer for DtoH
- Removed milk transpose on every frame due to added compute time
