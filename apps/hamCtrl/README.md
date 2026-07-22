# Hamamatsu Camera Control

Python MagAO-X app for controlling Hamamatsu qCMOS / ORCA cameras via DCAM.
Requires the Hamamatsu DCAM SDK and DCAM-API Lite for Linux.

## Install

```bash
cd /opt/MagAOX/source/MagAOX/apps/hamCtrl
make install
```

This uses the standard MagAO-X Python app Makefile (`Make/pythonApp.mk`) and installs
the `xapp.hamCtrl` package plus a `/opt/MagAOX/bin/hamCtrl` symlink.

## Config

| Field | Purpose |
| --- | --- |
| `camera_id` | Substring matched against DCAM `CAMERAID` or `BUS` (e.g. `"000027"`, `"S/N: 000027"`, or a USB path). Required when more than one Hamamatsu camera is connected. |
| `shmim_name` | ImageStream name. Defaults to the INDI device name (`-n`) when empty. |
| `data_dir` | Raw image directory for saving the camera stream. Defaults to `/opt/MagAOX/rawimages/<device>` when empty. |
| `exptime`, `binning`, `hpos`, `vpos`, `hsize`, `vsize`, `defectCorrect_Mode` | read bounds and update indi after the selected camera is opened. |

## Frame rate

`frame_rate.current` is read-only INDI, with min/max from DCAM `INTERNALFRAMERATE`
(`prop_getattr` valuemin/valuemax). Bounds refresh after open and whenever ROI size
(`hsize`/`vsize`), binning, or exposure time changes.

depends on exposure time + ROI/readout. Backend `set_frame_rate()` remains for cameras where DCAM
marks the property writable, but no INDI `target` is published.

## Related Docs

- [ORCA-Fire C16240-20UP](https://www.hamamatsu.com/us/en/product/cameras/cmos-cameras/C16240-20UP.html)
- [Driver Software (DCAM)](https://www.hamamatsu.com/jp/en/product/cameras/software/driver-software.html)
- [Catalog ORCA-Quest 2 qCMOS Camera](https://www.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/sys/SCAS0166E_C15550-22UP.pdf)
