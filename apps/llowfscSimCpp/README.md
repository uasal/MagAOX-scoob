# llowfscSimCpp

C++ MagAO-X host for the **same** `esc_llowfsc_sim` optical model used by
`apps/llowfscSim`. Side-by-side prototype — keep the Python app until you are
ready to switch proclist / xindiserver.

## Is the coworker's approach possible?

Yes. Embedding CPython in a MagAO-X `MagAOXApp` works: a dedicated worker thread
creates `esc.single` once (Python) and then calls `M.snap_camsci()` /
`M.snap_camlo()` at the camera fps cadence. INDI, ImageStreamIO, and the loop
timer stay in C++.

What this does **not** do: make a single Fraunhofer snap faster. `snap_camsci`
is still GPU Python (`calc_wfs_camsci`). If one snap takes 20 ms you cannot hold
200 Hz. Moving MagAO-X bookkeeping out of `XDevice.loop()` only helps if that
Python host was adding extra delay around the snap.

## Live plant vs rebuilding `esc.single`

Camera / FSM / DM changes **must** change the next image. They do **not**
require reconstructing the optical model.

| Change | What we do | Cost |
|--|--|--|
| DM command (`dm01disp`) | `M.set_dm(...)` then snap | cheap + snap |
| FSM `val_1`/`val_2` [nm] (`fsm_sim`) | `M.set_fsm([x,y]*1e-9, RMS=1)` then snap | cheap + snap |
| exptime / emgain / blacklevel / bitDepth / ROI | write `M.CAMSCI.*` then snap | cheap + snap |
| shutter / vortex | `M.camsci_shutter` / `M.use_vortex` then snap | cheap + snap |
| magnitude (INDI) / 10-Zernike OPD | `flux_scale_factor` / `set_prefpm_wfe` then snap | cheap + snap |
| pack, `ncamsci`, wavelength, bandpass | rebuild `esc.single` + `init_wfe` | **slow** (startup only) |

`optical_bridge.snap()` is the Python `_rt_snap` path: apply live state, then
propagate. Rebuilding `esc.single` every FSM or DM tick would be much slower
than the snap itself.

## Layout (compare with Python `llowfscSim`)

| | Python `llowfscSim` | `llowfscSimCpp` |
|--|--|--|
| Process / INDI name | `llowfscsim` | `llowfscsimcpp` (choose in proclist) |
| Optical model | `esc_llowfsc_sim` | same, via `optical_bridge.py` |
| Host | MagAO-X `XDevice` | MagAO-X C++ `MagAOXApp` |
| Output shmim | `shm_output` (default `camsci_sim`) | same |
| Shutter owner | this device | this device (`darkCtrl` `shutter_device`) |
| FSM follow | Python currently polls `val_1`/`val_2` | same: **`fsm_sim.val_1` / `val_2` only** |

Do **not** run both against the same `shm_output` at once.

`lina` C++ stays available for other apps (`iefcCtrl`, `psfRefCtrl`, …). This
sim does not reimplement the Fraunhofer model in C++.

## Build / install

Needs the MagAO-X **base** conda env (`/opt/conda`) with `esc_llowfsc_sim`,
`lina`, `numpy`, `tomlkit` — the same interpreter MagAO-X Python apps use
(`/opt/conda/bin/python`). Do not point this at a named env such as
`lina_cpp310`.

This clone's `Make/common.mk` requires GCC ≥ 14. On the instrument, copy this
folder into `/opt/MagAOX/source/MagAOX/apps/llowfscSimCpp` (same as other apps)
and build there:

```bash
sudo cp -a apps/llowfscSimCpp /opt/MagAOX/source/MagAOX/apps/
sudo make -C /opt/MagAOX/source/MagAOX/apps/llowfscSimCpp
sudo make -C /opt/MagAOX/source/MagAOX/apps/llowfscSimCpp install
```

Installs the binary and `/opt/MagAOX/python/llowfscSimCpp/optical_bridge.py`.

After install, the running log should show `Python prefix=/opt/conda` (base),
matching MagAO-X Python apps. Optional `sim.python_prefix=/opt/conda` in
`llowfscsimcpp.conf`. The `optical model ready` line should include `xp=cupy`
if the GPU stack is in that env.

### Vortex image vs Python `llowfscSim`

`usevortex` On still means `M.use_vortex = True` (same as Python). Two layout
bugs made the C++ host disagree with the Python app even when the flag was
right:

1. **DM grab** must be magpyx `np.array(milk, order='F').T`, not a raw memcpy.
   A swapped 34×34 command on a vortex null looks like junk, not a coronagraph.
2. **Output shmim** must match `magaox.shmim.Image.write`: milk `size[0]=h`,
   F-contiguous copy of the C-order `(h,w)` frame. For a square 512×512 those
   size labels look identical, but a C-order memcpy is a 90° transpose.

### Frame rate

Embedding CPython does **not** make `M.snap_camsci()` faster, and compiling this
C++ app with nvcc will not either: the Fraunhofer path already runs in **cupy
CUDA**. 412 Hz is ~2.43 ms/frame — the same as the notebook `rt_snap` timing
on this GPU. Vortex-on is a 1024² FFT plus two MFTs plus the 512² science MFT;
those kernels dominate.

What this host *can* do (and now does):

- Overlap vortex low-res FFT and high-res MFT on two CUDA streams
- Cache the astropy flux conversion (it ran every frame)
- Cast to float32 on GPU and reuse a host buffer for DtoH
- Blocked C→milk transpose (naive 512² loop was a large slice of 2.4 ms)

Logs `frame mean … Hz grab= snap= pub=` every 256 frames. If `snap=` is still
~2.3 ms after rebuild, you are at the optical-model wall: next levers are a
smaller `ncamsci`, vortex off, or changing `esc_llowfsc_sim` itself (kernel
fusion / CUDA graphs), not more C++ around the snap.

If `xp=numpy` in the ready log, you are on CPU and will not match GPU Python
speed or (exactly) the PSF.

### Setuid vs conda CUDA

`magAOXApp.mk` installs `4755 root`. MagAO-X Python apps are **not** setuid
(they symlink to conda). A setuid binary ignores `LD_LIBRARY_PATH` (AT_SECURE),
which can break `cupy`/conda `.so` loads even with rpath on `libpython`.

If the worker dies at `import esc_llowfsc_sim` or the first snap with a CUDA /
`libstdc++` error, drop setuid for this prototype only:

```bash
sudo chmod 755 /opt/MagAOX/bin/llowfscSimCpp
```

If `import esc_llowfsc_sim` fails with `No module named 'tomlkit'`, install it
into base conda (not a named env) so setuid / other-user launches work:

```bash
sudo -H /opt/conda/bin/pip install tomlkit
```

## INDI registration

1. proclist: `llowfscsimcpp  llowfscSimCpp` (leave `llowfscsim  llowfscSim` for A/B)
2. `/opt/MagAOX/config/llowfscsimcpp.conf` from `llowfscSimCpp.conf.sample`
3. `is*.conf` `drivers=...,llowfscsimcpp`
4. Restart xindiserver

Point `darkCtrl` `shutter_device` at this process name if you switch over.

## Operation

Same as the Python app: start `fsm_sim` + `nsvCtrlSim`, toggle `streaming` On,
command `nsvsim.exptime` / `fsm_sim.val_1`/`val_2`, close `shutter` for darks.
Set `llowfscsimcpp.magnitude.target` for Vega mag (default 0).

### Occasional bright flash

The vortex FFT and MFT run on two CUDA streams. Those streams used to consume
`E_DM` before the default stream finished writing it, which produced a
saturated frame. `optical_bridge.py` now records an event on the producing
stream and waits on both vortex streams before the overlap.
