Vendored SHM-only IEFC slice of the LINA C++ library for MagAOX `iefcCtrl`. It provides shared-memory camera/DM I/O, calibration, closed-loop run, and package orchestration helpers — not the full LINA tree (no ControlModel, props, or CUDA).

`ShmimStream` reads UINT8/INT8/UINT16/INT16/UINT32/INT32/UINT64/INT64/FLOAT/DOUBLE
(MagAO-X cameras are commonly UINT16) and writes FLOAT/DOUBLE (cacao DM channels).
DM `nact` is the square stream size (`size[0]==size[1]`). Calibrate/run take separate
mode and probe streams (same square size); the physical command is their cacao sum.

`grab_mean` timeout is `(wait_frames + nframes + 3) * frame_period_s` (1 in-progress
exposure + 2 extra frames), floored at 5 s. A missing camera frame period is an
error (no timeout fallback).

