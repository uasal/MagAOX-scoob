Vendored SHM-only IEFC slice of the LINA C++ library for MagAOX `iefcCtrl`. It provides shared-memory camera/DM I/O, calibration, closed-loop run, and package orchestration helpers — not the full LINA tree (no ControlModel, props, or CUDA).

`ShmimStream` reads UINT8/INT8/UINT16/INT16/UINT32/INT32/UINT64/INT64/FLOAT/DOUBLE
(MagAO-X cameras are commonly UINT16) and writes FLOAT/DOUBLE (cacao DM channels).
DM `nact` is the square stream size (`size[0]==size[1]`).
