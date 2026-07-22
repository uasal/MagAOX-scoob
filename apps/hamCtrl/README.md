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

## Working Controls
_List of controls via cursesINDI interface and there rough estimated status._

| INDI Prop | Short Name | Description | Value Type | Units | Range |  Working Status | 
| ------------- | ------ | ----------- | ------------ | ------ | ------------ | -------- |
| exptime | Exposure Time | Controls setting and viewing Exposure Time. | float | sec | 0.000033949 - 1800.000015185, step = 0.00000001 |Working | 
| hpos | Horizontal Position | ROI horizontal position (~x center pixel of window).  | float | - | 0-4092, step=4 | Working | 
| vpos | Vertical Position | ROI vertical position (~y center pixel of window).  | float | - | 0-2300, step=4 | Working | 
| hsize | Horizontal Size | The 'width' for the ROI | float | - | 4-4096, step=4 | Working |
| vsize | Vertical Size | The 'height' for the ROI | float | - | 4-2304, step=4 | Working |
| binning | Binning | Sets the binning. A higher range is potential but this feature has been 'picky' sometimes. | float | - | 1.0 = 1x1, 2.0 = 2x2, 4.0 = 4x4 | Partly? |
| gain | Contrast Gain | Set the contrast gain | float | - | - | Not Working |
| temp | Temperature | Current temperature of the sensor. | float | C | -50-100 | Working | 
 
_Range values are assuming Dcam is set to `SENSORMODE_AREA`. Refer to the HTML docs for the 15550-22UP property information for more details._

## Related Docs

- [ORCA-Fire C16240-20UP](https://www.hamamatsu.com/us/en/product/cameras/cmos-cameras/C16240-20UP.html)
- [Driver Software (DCAM)](https://www.hamamatsu.com/jp/en/product/cameras/software/driver-software.html)
- [Hamamatsu Listing Overview](https://www.hamamatsu.com/jp/en/product/cameras/qcmos-cameras/C15550-22UP.html)
- [Catalog ORCA-Quest 2 qCMOS Camera](https://www.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/sys/SCAS0166E_C15550-22UP.pdf)
