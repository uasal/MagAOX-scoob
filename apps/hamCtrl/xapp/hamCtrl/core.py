# Experimental State app / still has items to be added potentially but being used for testing camera.
## Last version date- 2025-03-03, SFR
## Separate Tool from Dcam API needed to control fan to be turned off currently (DCAM-API_Lite_for_Linux_v24.12.6898)
import logging
import os
import threading
import time
from typing import Optional

import ImageStreamIOWrap as ISIO
import numpy as np
import xconf
from magaox.indi.device import BaseConfig, XDevice
from magpyx.utils import ImageStream
from purepyindi2 import constants, properties
from purepyindi2.messages import DefNumber, DefSwitch, DefText

from .dcam import Dcam, Dcamapi
from .dcamapi4 import (
    DCAM_IDPROP,
    DCAM_IDSTR,
    DCAM_PIXELTYPE,
    DCAMCAP_STATUS,
    DCAMPROP,
    DCAMPROP_OPTION,
)

log = logging.getLogger(__name__)


EXTERNAL_RECORDED_PROPERTIES = {
    'tcsi.catalog.object': 'OBJECT',
    'tcsi.catdata.ra': None,
    'tcsi.catdata.dec': None,
    'tcsi.catdata.epoch': None,
    'observers.current_observer.full_name': 'OBSERVER',
    'tcsi.teldata.pa': 'PARANG',
    'flipacq.presetName.in': None,
}

CAMERA_CONNECT_RETRY_SEC = 5


@xconf.config
class HamCamConfig(BaseConfig):
    """Python INDI device config for the Hamamatsu C15550-22UP camera.

    Loaded from `/opt/MagAOX/config/<device>.conf` when started with `-n <device>`.
    Set `camera_id` to a DCAM CAMERAID substring (e.g. `"000027"` or `"S/N: 000027"`)
    or a BUS/USB path fragment so multiple hamCtrl instances can target different cameras.
    """

    camera_id: str = xconf.field(
        default="",
        help='DCAM CAMERAID / serial / BUS substring used to select the camera (e.g. "000027").',
    )
    shmim_name: str = xconf.field(
        default="",
        help="ImageStream shared-memory name. Defaults to the INDI device name (-n) when empty.",
    )
    data_dir: str = xconf.field(
        default="",
        help="Directory for raw images. Defaults to /opt/MagAOX/rawimages/<device> when empty.",
    )
    exptime: float = xconf.field(default=0.008653964, help='Exposure time in seconds')
    binning: float = xconf.field(default=1.0, help='Binning:[1]- 1x1, [2]- 2x2, [4]- 4x4')
    hpos: int = xconf.field(default=0, help='Subarray horizontal position in pixels (SUBARRAYHPOS)')
    vpos: int = xconf.field(default=0, help='Subarray vertical position in pixels (SUBARRAYVPOS)')
    hsize: int = xconf.field(default=4432, help='Subarray width in pixels (SUBARRAYHSIZE)')
    vsize: int = xconf.field(default=2368, help='Subarray height in pixels (SUBARRAYVSIZE)')
    defectCorrect_Mode: float = xconf.field(default=2.0, help='1.0 = off, 2.0 = on')
    bitdepth: int = xconf.field(
        default=0,
        help=(
            "Output bit depth (e.g. 8 or 16). 0 = leave camera default. "
            "Mapped via DCAM BITSPERCHANNEL and/or IMAGE_PIXELTYPE."
        ),
    )


class HamCam(XDevice):
    config: HamCamConfig

    data_dir: str = ""
    cancel: bool = False
    exp_start: float = 0
    shmim: ISIO.Image
    frame: np.ndarray
    shmim_name: str = ""
    last_image: Optional[str] = None
    start_telem: Optional[dict] = None
    exptime: Optional[float] = None
    vpos: Optional[int] = None
    hpos: Optional[int] = None
    vsize: Optional[int] = None
    hsize: Optional[int] = None
    binning: Optional[float] = None
    height: Optional[int] = None
    width: Optional[int] = None
    cam: Optional[Dcam] = None
    temp: Optional[float] = None
    temp_status: Optional[float] = None
    frame_rate: Optional[float] = None
    camstream: Optional[ImageStream] = None
    fan_status: Optional[float] = None
    protect_status: Optional[float] = None
    hotpixel_lvl: Optional[float] = None
    hp_status: Optional[str] = None
    dc_mode: Optional[str] = None
    tmp_state: Optional[str] = None
    defectCorrect_Mode: Optional[float] = None
    frame_rate_min: float = 0.0
    frame_rate_max: float = 10000.0
    frame_rate_writable: bool = False
    cooler_writable: bool = False
    cooler_fan_writable: bool = False
    cooler_capability: str = "unknown"
    cooler_value: Optional[float] = None
    cooler_fan_value: Optional[float] = None
    bitdepth: Optional[int] = None
    bitdepth_min: int = 8
    bitdepth_max: int = 16
    bitdepth_options: Optional[list] = None
    bitdepth_writable: bool = False
    frame_dtype: type = np.uint16
    _stream_lock: Optional[threading.RLock] = None
    _stream_stop: Optional[threading.Event] = None
    th: Optional[threading.Thread] = None
    streaming: bool = False

    @staticmethod
    def _as_pixel_int(value) -> Optional[int]:
        """Convert a DCAM/INDI numeric pixel quantity to int, or None if unavailable."""
        if value is False or value is None:
            return None
        return int(round(float(value)))

    def emit_telem_hamcam(self):
        self.log.info(f"In emit_telem_hamcam")
        print("In emit_telem_hamcam")
        w = self.width
        h = self.height
        x = self.hpos
        y = self.vpos
        self.telem("telem_hamcam", {
            "roi": {
                "xcen": x,
                "ycen": y,
                "w": w,
                "h": h,
                "xbin": 1,
                "ybin": 1,
            },
            "exptime": self.exptime,
            "frame_rate": self.frame_rate,
            "adcSpeed": -1,
            "shutter": {"statusStr": None, "state": None},
            "synchro": 0,
            "vshift": -1,
            "cropMode": 0
        })

    @staticmethod
    def _dcam_string(value) -> str:
        if value is False or value is None:
            return ""
        return str(value)

    def _enumerate_dcam_devices(self):
        """Return list of (index, model, cameraid, bus) for connected DCAM devices."""
        devices = []
        count = Dcamapi.get_devicecount()
        if count is False or count <= 0:
            return devices

        for index in range(count):
            probe = Dcam(index)
            model = self._dcam_string(probe.dev_getstring(DCAM_IDSTR.MODEL))
            cameraid = self._dcam_string(probe.dev_getstring(DCAM_IDSTR.CAMERAID))
            bus = self._dcam_string(probe.dev_getstring(DCAM_IDSTR.BUS))
            devices.append((index, model, cameraid, bus))
            self.log.info(
                "DCAM device [%d]: MODEL=%s, CAMERAID=%s, BUS=%s",
                index,
                model or "?",
                cameraid or "?",
                bus or "?",
            )
        return devices

    def _select_device_index(self, devices):
        """Pick a DCAM device index from config.camera_id (CAMERAID or BUS substring)."""
        camera_id = (self.config.camera_id or "").strip()
        if not devices:
            return None

        if not camera_id:
            if len(devices) == 1:
                index, model, cameraid, bus = devices[0]
                self.log.warning(
                    "camera_id is empty; using sole DCAM device [%d] CAMERAID=%s BUS=%s",
                    index,
                    cameraid or "?",
                    bus or "?",
                )
                return index
            self.log.error(
                "camera_id is empty but %d DCAM devices are present; set camera_id in config",
                len(devices),
            )
            return None

        needle = camera_id.lower()
        matches = []
        for index, model, cameraid, bus in devices:
            haystacks = (cameraid, bus)
            if any(needle in value.lower() for value in haystacks if value):
                matches.append((index, model, cameraid, bus))

        if not matches:
            self.log.error(
                "No DCAM device matched camera_id=%r; available: %s",
                camera_id,
                ", ".join(
                    f"[{i}] CAMERAID={cid or '?'} BUS={bus or '?'}"
                    for i, _model, cid, bus in devices
                ),
            )
            return None

        if len(matches) > 1:
            self.log.error(
                "camera_id=%r matched multiple DCAM devices: %s",
                camera_id,
                ", ".join(
                    f"[{i}] CAMERAID={cid or '?'} BUS={bus or '?'}"
                    for i, _model, cid, bus in matches
                ),
            )
            return None

        index, model, cameraid, bus = matches[0]
        self.log.info(
            "Selected DCAM device [%d] for camera_id=%r (MODEL=%s, CAMERAID=%s, BUS=%s)",
            index,
            camera_id,
            model or "?",
            cameraid or "?",
            bus or "?",
        )
        return index

    def _prop_attr(self, idprop):
        """Return DCAMPROP_ATTR for an open-camera property, or None if unavailable."""
        if self.cam is None:
            return None
        attr = self.cam.prop_getattr(idprop)
        if attr is False:
            return None
        return attr

    def _walk_prop_values(self, idprop):
        """Return sorted unique numeric values supported by a DCAM property."""
        if self.cam is None:
            return []
        attr = self._prop_attr(idprop)
        if attr is None or not attr.is_effective():
            return []

        values = []
        # Prefer walking with QUERY_NEXT from below valuemin.
        start = float(attr.valuemin) - 1.0 if attr.valuemin == attr.valuemin else 0.0
        cur = self.cam.prop_queryvalue(idprop, start, DCAMPROP_OPTION.NEXT)
        guard = 0
        while cur is not False and guard < 64:
            values.append(float(cur))
            nxt = self.cam.prop_queryvalue(idprop, float(cur), DCAMPROP_OPTION.NEXT)
            if nxt is False or float(nxt) == float(cur):
                break
            cur = nxt
            guard += 1

        if values:
            return sorted(set(int(round(v)) for v in values))

        # Fallback: sample known bit-depth / pixel-type candidates.
        candidates = (
            float(DCAM_PIXELTYPE.MONO8),
            float(DCAM_PIXELTYPE.MONO16),
            8.0,
            10.0,
            12.0,
            14.0,
            16.0,
        )
        for cand in candidates:
            q = self.cam.prop_queryvalue(idprop, cand, DCAMPROP_OPTION.NEAREST)
            if q is not False and abs(float(q) - cand) < 1e-6:
                values.append(float(q))
        return sorted(set(int(round(v)) for v in values))

    @staticmethod
    def _pixeltype_to_bitdepth(pixeltype) -> Optional[int]:
        """Map DCAM IMAGE_PIXELTYPE enum to an output bit depth."""
        if pixeltype is False or pixeltype is None:
            return None
        pt = int(round(float(pixeltype)))
        if pt == int(DCAM_PIXELTYPE.MONO8):
            return 8
        if pt == int(DCAM_PIXELTYPE.MONO16):
            return 16
        return None

    @staticmethod
    def _bitdepth_to_pixeltype(bitdepth: int) -> Optional[int]:
        """Map requested bit depth to DCAM IMAGE_PIXELTYPE (8→MONO8, else MONO16)."""
        if bitdepth <= 8:
            return int(DCAM_PIXELTYPE.MONO8)
        return int(DCAM_PIXELTYPE.MONO16)

    def _frame_dtype_for_bitdepth(self, bitdepth: Optional[int]):
        """NumPy dtype used for the shmim / capture buffer at the given bit depth."""
        if bitdepth is not None and int(bitdepth) <= 8:
            return np.uint8
        return np.uint16

    def _read_bitdepth_from_camera(self) -> Optional[int]:
        """Read effective output bit depth from BITSPERCHANNEL and/or IMAGE_PIXELTYPE."""
        if self.cam is None:
            return None

        pixeltype = self.cam.prop_getvalue(DCAM_IDPROP.IMAGE_PIXELTYPE)
        pt_bits = self._pixeltype_to_bitdepth(pixeltype)
        if pt_bits == 8:
            return 8

        bpc = self.cam.prop_getvalue(DCAM_IDPROP.BITSPERCHANNEL)
        if bpc is not False and bpc is not None:
            return int(round(float(bpc)))

        return pt_bits

    def _probe_bitdepth_capabilities(self):
        """Discover which output bit depths this camera can produce.

        Prefer writable BITSPERCHANNEL values when available. Also include depths
        implied by IMAGE_PIXELTYPE (MONO8→8, MONO16→16). On C16240-20UP,
        BITSPERCHANNEL is fixed at 16 (read-only) while IMAGE_PIXELTYPE is
        switchable between MONO8 and MONO16.
        """
        options = set()
        bits_writable = False
        pixel_writable = False

        bpc_attr = self._prop_attr(DCAM_IDPROP.BITSPERCHANNEL)
        if bpc_attr is not None and bpc_attr.is_effective():
            bits_writable = bool(bpc_attr.is_writable())
            for v in self._walk_prop_values(DCAM_IDPROP.BITSPERCHANNEL):
                if v in (8, 10, 12, 14, 16):
                    options.add(v)
            # Range fallback when walk returns nothing.
            if not options and bpc_attr.valuemin == bpc_attr.valuemin:
                lo = int(round(float(bpc_attr.valuemin)))
                hi = int(round(float(bpc_attr.valuemax)))
                step = int(round(float(bpc_attr.valuestep))) if bpc_attr.valuestep else 0
                if lo == hi:
                    options.add(lo)
                elif step > 0:
                    options.update(range(lo, hi + 1, step))

        pt_attr = self._prop_attr(DCAM_IDPROP.IMAGE_PIXELTYPE)
        if pt_attr is not None and pt_attr.is_effective():
            pixel_writable = bool(pt_attr.is_writable())
            for v in self._walk_prop_values(DCAM_IDPROP.IMAGE_PIXELTYPE):
                mapped = self._pixeltype_to_bitdepth(v)
                if mapped is not None:
                    options.add(mapped)

        if not options:
            options = {16}

        self.bitdepth_options = sorted(options)
        self.bitdepth_min = min(self.bitdepth_options)
        self.bitdepth_max = max(self.bitdepth_options)
        self.bitdepth_writable = bool(bits_writable or pixel_writable)
        self.bitdepth = self._read_bitdepth_from_camera()
        if self.bitdepth is None:
            self.bitdepth = self.bitdepth_max
        self.frame_dtype = self._frame_dtype_for_bitdepth(self.bitdepth)

        self.log.info(
            "Bit depth capabilities: options=%s current=%s writable=%s "
            "(BITSPERCHANNEL writable=%s, IMAGE_PIXELTYPE writable=%s)",
            self.bitdepth_options,
            self.bitdepth,
            self.bitdepth_writable,
            bits_writable,
            pixel_writable,
        )

    def _apply_bitdepth(self, bitdepth: int) -> bool:
        """Configure camera output packing / channel depth for ``bitdepth``.

        Sets BITSPERCHANNEL when writable, then IMAGE_PIXELTYPE (MONO8 for 8-bit,
        MONO16 otherwise) when that property exists. Capture must be stopped.
        """
        if self.cam is None:
            self.log.warning("Cannot set bitdepth: camera not open")
            return False

        requested = int(bitdepth)
        if self.bitdepth_options and requested not in self.bitdepth_options:
            self.log.warning(
                "Unsupported bitdepth=%s; available=%s",
                requested,
                self.bitdepth_options,
            )
            return False

        bpc_attr = self._prop_attr(DCAM_IDPROP.BITSPERCHANNEL)
        if bpc_attr is not None and bpc_attr.is_effective() and bpc_attr.is_writable():
            if not self.cam.prop_setvalue(DCAM_IDPROP.BITSPERCHANNEL, float(requested)):
                self.log.error(
                    "Failed to set BITSPERCHANNEL=%s: %s",
                    requested,
                    self.cam.lasterr(),
                )
                return False
            self.log.info("Set BITSPERCHANNEL=%s", requested)

        pt_attr = self._prop_attr(DCAM_IDPROP.IMAGE_PIXELTYPE)
        if pt_attr is not None and pt_attr.is_effective() and pt_attr.is_writable():
            pixeltype = self._bitdepth_to_pixeltype(requested)
            if pixeltype is not None:
                if not self.cam.prop_setvalue(DCAM_IDPROP.IMAGE_PIXELTYPE, float(pixeltype)):
                    self.log.error(
                        "Failed to set IMAGE_PIXELTYPE=%s for bitdepth=%s: %s",
                        pixeltype,
                        requested,
                        self.cam.lasterr(),
                    )
                    return False
                self.log.info(
                    "Set IMAGE_PIXELTYPE=%s for bitdepth=%s",
                    pixeltype,
                    requested,
                )

        actual = self._read_bitdepth_from_camera()
        if actual is None:
            self.log.error("bitdepth set but readback failed")
            return False

        self.bitdepth = int(actual)
        self.frame_dtype = self._frame_dtype_for_bitdepth(self.bitdepth)
        if self.bitdepth != requested:
            self.log.warning(
                "bitdepth requested=%s actual=%s (camera may remap)",
                requested,
                self.bitdepth,
            )
        else:
            self.log.info("bitdepth now %s (dtype=%s)", self.bitdepth, np.dtype(self.frame_dtype))
        return True

    def _publish_bitdepth(self, redefine=False):
        """Publish bitdepth.current/target (and optionally redefine min/max)."""
        if "bitdepth" not in self.properties or self.bitdepth is None:
            return
        for elem_name in ("current", "target"):
            elem = self.properties["bitdepth"]._elements[elem_name]
            elem.min = float(self.bitdepth_min)
            elem.max = float(self.bitdepth_max)
            self.properties["bitdepth"][elem_name] = int(self.bitdepth)
        if redefine:
            self.define_property(self.properties["bitdepth"])
        else:
            self.update_property(self.properties["bitdepth"])

    def _read_frame_rate_bounds(self):
        """Query INTERNALFRAMERATE valuemin/valuemax from the open camera."""
        attr = self._prop_attr(DCAM_IDPROP.INTERNALFRAMERATE)
        if attr is None:
            return None
        self.frame_rate_writable = bool(attr.is_effective() and attr.is_writable())
        fr_min = float(attr.valuemin)
        fr_max = float(attr.valuemax)
        if fr_max < fr_min:
            fr_min, fr_max = fr_max, fr_min
        return fr_min, fr_max

    def _refresh_frame_rate_bounds(self, redefine=True):
        """Update cached/INDI frame_rate bounds from the camera (ROI-dependent)."""
        bounds = self._read_frame_rate_bounds()
        if bounds is None:
            self.log.debug("Could not read INTERNALFRAMERATE bounds from camera")
            return False

        self.frame_rate_min, self.frame_rate_max = bounds
        value = self.cam.prop_getvalue(DCAM_IDPROP.INTERNALFRAMERATE)
        if value is not False:
            self.frame_rate = value

        self.log.info(
            "Frame rate from camera: min=%s max=%s current=%s writable=%s",
            self.frame_rate_min,
            self.frame_rate_max,
            self.frame_rate,
            self.frame_rate_writable,
        )
        if not self.frame_rate_writable:
            self.log.info(
                "INTERNALFRAMERATE is read-only on this camera/firmware; "
                "achieved rate is set by exposure time, ROI, and readout. "
                "Use exptime (and ROI) to change frame rate."
            )

        if "frame_rate" not in self.properties:
            return True

        elem = self.properties["frame_rate"]._elements["current"]
        elem.min = self.frame_rate_min
        elem.max = self.frame_rate_max

        if self.frame_rate is not None:
            self.properties["frame_rate"]["current"] = self.frame_rate

        if redefine:
            # setNumberVector does not carry min/max; re-define to publish new bounds.
            self.define_property(self.properties["frame_rate"])
        else:
            self.update_property(self.properties["frame_rate"])
        return True

    def set_frame_rate(self, existing_property, new_message):
        """Set INTERNALFRAMERATE when the camera exposes it as writable.

        On C16240-20UP this property is readable but not writable (NOTWRITABLE).
        """
        if 'target' not in new_message:
            self.update_property(existing_property)
            return False

        if self.cam is None:
            self.log.warning("Cannot set frame_rate: camera not open")
            self.update_property(existing_property)
            return False

        # Re-check writability; ROI changes can alter attribute flags on some models.
        attr = self._prop_attr(DCAM_IDPROP.INTERNALFRAMERATE)
        if attr is not None:
            self.frame_rate_writable = bool(attr.is_effective() and attr.is_writable())

        if not self.frame_rate_writable:
            self.log.warning(
                "INTERNALFRAMERATE is not writable on this camera; "
                "ignoring target=%s (current=%s). Change exptime/ROI instead.",
                new_message['target'],
                self.frame_rate,
            )
            if self.frame_rate is not None:
                existing_property['current'] = self.frame_rate
                existing_property['target'] = self.frame_rate
            self.update_property(existing_property)
            return False

        requested = float(new_message['target'])
        self.pause_stream()
        if not self.cam.prop_setvalue(DCAM_IDPROP.INTERNALFRAMERATE, requested):
            self.log.error(
                "Failed to set INTERNALFRAMERATE=%s: %s",
                requested,
                self.cam.lasterr(),
            )
            self._maybe_restart_stream()
            self.update_property(existing_property)
            return False

        actual = self.cam.prop_getvalue(DCAM_IDPROP.INTERNALFRAMERATE)
        if actual is False:
            self.log.error("INTERNALFRAMERATE set succeeded but readback failed: %s", self.cam.lasterr())
            self._maybe_restart_stream()
            self.update_property(existing_property)
            return False

        self.frame_rate = float(actual)
        existing_property['current'] = self.frame_rate
        existing_property['target'] = self.frame_rate
        self.update_property(existing_property)
        self.log.info(
            "INTERNALFRAMERATE requested=%s actual=%s",
            requested,
            self.frame_rate,
        )
        self._refresh_frame_rate_bounds(redefine=True)
        self._maybe_restart_stream()
        return True

    def _probe_cooler_capabilities(self):
        """Detect whether SENSORCOOLER / SENSORCOOLERFAN exist and are writable.

        On C16240-20UP (ORCA-Fire), both property IDs return INVALIDPROPERTYID:
        Peltier cooling is always active and fan speed is not exposed via DCAM
        (manual documents fan speed only through DCAM Configurator).
        """
        self.cooler_writable = False
        self.cooler_fan_writable = False
        self.cooler_value = None
        self.cooler_fan_value = None

        cooler_attr = self._prop_attr(DCAM_IDPROP.SENSORCOOLER)
        fan_attr = self._prop_attr(DCAM_IDPROP.SENSORCOOLERFAN)
        status_attr = self._prop_attr(DCAM_IDPROP.SENSORCOOLERSTATUS)

        parts = []
        if cooler_attr is None:
            parts.append("SENSORCOOLER=unavailable")
        else:
            self.cooler_writable = bool(cooler_attr.is_effective() and cooler_attr.is_writable())
            val = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLER)
            self.cooler_value = None if val is False else float(val)
            parts.append(
                f"SENSORCOOLER=effective writable={self.cooler_writable} value={self.cooler_value}"
            )

        if fan_attr is None:
            parts.append("SENSORCOOLERFAN=unavailable")
        else:
            self.cooler_fan_writable = bool(fan_attr.is_effective() and fan_attr.is_writable())
            val = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLERFAN)
            self.cooler_fan_value = None if val is False else float(val)
            parts.append(
                f"SENSORCOOLERFAN=effective writable={self.cooler_fan_writable} value={self.cooler_fan_value}"
            )

        if status_attr is None:
            parts.append("SENSORCOOLERSTATUS=unavailable")
        else:
            parts.append(
                f"SENSORCOOLERSTATUS=RO value={self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLERSTATUS)}"
            )

        if self.cooler_writable or self.cooler_fan_writable:
            self.cooler_capability = "controllable"
        elif status_attr is not None:
            self.cooler_capability = "status_only"
        else:
            self.cooler_capability = "unsupported"

        self.log.info("Cooler capability probe: %s (%s)", self.cooler_capability, "; ".join(parts))
        if self.cooler_capability == "status_only":
            self.log.info(
                "This camera exposes cooler status/temperature only; "
                "SENSORCOOLER and SENSORCOOLERFAN are not available via DCAM "
                "(cooling is managed by camera firmware)."
            )

    def _sync_cooler_indi(self):
        """Push cooler capability / values into INDI properties if present."""
        if "cooler_capability" in self.properties:
            self.properties["cooler_capability"]["status"] = self.cooler_capability
            self.update_property(self.properties["cooler_capability"])

        if "cooler" in self.properties and self.cooler_value is not None:
            self.properties["cooler"]["current"] = self.cooler_value
            self.properties["cooler"]["target"] = self.cooler_value
            self.update_property(self.properties["cooler"])

        if "cooler_fan" in self.properties and self.cooler_fan_value is not None:
            self.properties["cooler_fan"]["current"] = self.cooler_fan_value
            self.properties["cooler_fan"]["target"] = self.cooler_fan_value
            self.update_property(self.properties["cooler_fan"])

    def _apply_config_camera_settings(self):
        """Apply configured exposure / ROI / binning / defect mode after open."""
        if self.cam is None:
            return False

        # ROI properties require subarray mode off while being written.
        subarray_on = self.check_subarray()
        if subarray_on:
            self.switch_subarray()

        settings = (
            (DCAM_IDPROP.EXPOSURETIME, float(self.config.exptime), "exptime"),
            (DCAM_IDPROP.BINNING, float(self.config.binning), "binning"),
            (DCAM_IDPROP.SUBARRAYHPOS, float(int(self.config.hpos)), "hpos"),
            (DCAM_IDPROP.SUBARRAYVPOS, float(int(self.config.vpos)), "vpos"),
            (DCAM_IDPROP.SUBARRAYHSIZE, float(int(self.config.hsize)), "hsize"),
            (DCAM_IDPROP.SUBARRAYVSIZE, float(int(self.config.vsize)), "vsize"),
            (DCAM_IDPROP.DEFECTCORRECT_MODE, float(self.config.defectCorrect_Mode), "defectCorrect_Mode"),
        )
        for prop_id, value, label in settings:
            if not self.cam.prop_setvalue(prop_id, value):
                self.log.warning(
                    "Failed to set %s=%s: %s",
                    label,
                    value,
                    self.cam.lasterr(),
                )
            else:
                self.log.info("Applied config %s=%s", label, value)

        if not self.check_subarray():
            self.switch_subarray()

        # Probe bit-depth options after ROI so mode queries reflect current sensor mode.
        self._probe_bitdepth_capabilities()
        if int(self.config.bitdepth) > 0:
            if not self._apply_bitdepth(int(self.config.bitdepth)):
                self.log.warning(
                    "Failed to apply config bitdepth=%s; leaving camera default",
                    self.config.bitdepth,
                )
        return True

    def _read_camera_state(self):
        """Cache commonly used property values from the open camera."""
        self.exptime = self.cam.prop_getvalue(DCAM_IDPROP.EXPOSURETIME)
        self.hsize = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHSIZE))
        self.vsize = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVSIZE))
        self.hpos = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHPOS))
        self.vpos = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVPOS))
        self.binning = self.cam.prop_getvalue(DCAM_IDPROP.BINNING)
        self.temp = self.cam.prop_getvalue(DCAM_IDPROP.SENSORTEMPERATURE)
        self.temp_status = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLERSTATUS)
        self.fan_status = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLER)
        self.frame_rate = self.cam.prop_getvalue(DCAM_IDPROP.INTERNALFRAMERATE)
        self.width = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.IMAGE_WIDTH))
        self.height = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.IMAGE_HEIGHT))
        self.defectCorrect_Mode = self.cam.prop_getvalue(DCAM_IDPROP.DEFECTCORRECT_MODE)
        self.hotpixel_lvl = self.cam.prop_getvalue(DCAM_IDPROP.HOTPIXELCORRECT_LEVEL)
        self.bitdepth = self._read_bitdepth_from_camera()
        self.frame_dtype = self._frame_dtype_for_bitdepth(self.bitdepth)

        self.log.info(
            "Camera state: exptime=%s hsize=%s vsize=%s hpos=%s vpos=%s binning=%s "
            "temp=%sC temp_status=%s frame_rate=%s width=%s height=%s defect=%s "
            "hotpixel=%s bitdepth=%s",
            self.exptime,
            self.hsize,
            self.vsize,
            self.hpos,
            self.vpos,
            self.binning,
            self.temp,
            self.temp_status,
            self.frame_rate,
            self.width,
            self.height,
            self.defectCorrect_Mode,
            self.hotpixel_lvl,
            self.bitdepth,
        )

    def _init_camera(self):
        self.log.info("Initializing Hamamatsu")
        self.cam = None

        if not Dcamapi.init():
            self.log.info("-NG: Dcamapi.init() fails with error {}".format(Dcamapi.lasterr()))
            Dcamapi.uninit()
            return False

        devices = self._enumerate_dcam_devices()
        i_device = self._select_device_index(devices)
        if i_device is None:
            Dcamapi.uninit()
            return False

        self.cam = Dcam(i_device)
        if not self.cam.dev_open():
            self.log.info(
                "-NG: Dcam.dev_open() fails with error {}".format(self.cam.lasterr())
            )
            self.cam = None
            Dcamapi.uninit()
            return False

        model = self._dcam_string(self.cam.dev_getstring(DCAM_IDSTR.MODEL))
        cameraid = self._dcam_string(self.cam.dev_getstring(DCAM_IDSTR.CAMERAID))
        bus = self._dcam_string(self.cam.dev_getstring(DCAM_IDSTR.BUS))
        self.log.info("Opened MODEL=%s, CAMERAID=%s, BUS=%s", model, cameraid, bus)

        self._apply_config_camera_settings()
        self._read_camera_state()
        self._probe_cooler_capabilities()
        self._refresh_frame_rate_bounds(redefine=("frame_rate" in self.properties))
        self._publish_bitdepth(redefine=("bitdepth" in self.properties))
        self._sync_cooler_indi()
        self.get_hotpixel_status()
        self.get_defectCorrect_status()
        self.get_tempstatus()
        self.log.info("TEMP STATE: %s", self.tmp_state)

        # Do not auto-start streaming; wait for streaming.toggle = On.
        self.streaming = False
        self._publish_streaming_toggle()
        self.log.info("Camera ready; set streaming.toggle=On to start frame acquisition")
        return True

    def _init_properties(self):
        """
        Setup / initialize indi properties to be used.
        magaox/indi/device.py -> imports properties from purepyindi2 and is within the XDevice class

        """

        self.log.info(f"Hamamatsu was configured! {self.config=}")
        fsmstate = properties.TextVector(name="fsm")
        fsmstate.add_element(DefText(name="state", _value="NODEVICE"))
        self.add_property(fsmstate)

        # MagAOX-standard toggle: On starts capture into the shmim, Off stops it.
        sv = properties.SwitchVector(
            name="streaming",
            rule=constants.SwitchRule.AT_MOST_ONE,
            perm=constants.PropertyPerm.READ_WRITE,
        )
        sv.add_element(DefSwitch(
            name="toggle",
            label="Stream frames",
            _value=constants.SwitchState.ON if self.streaming else constants.SwitchState.OFF,
        ))
        self.add_property(sv, callback=self.handle_streaming)

        # Exposure Time ---------------------------------------------------------------------
        nv = properties.NumberVector(name='exptime', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='Exposure time (sec)', format='%3.1f',
            min=0.000007309, max=10.000005818, step=0.00000001,
            _value=self.exptime if self.exptime is not None else self.config.exptime,
        ))
        nv.add_element(DefNumber(
            name='target', label="Requested exposure time(sec)", format="%3.1f",
            min=0.000007309, max=10.000005818, step=0.00000001,
            _value=self.exptime if self.exptime is not None else self.config.exptime,
        ))
        self.add_property(nv, callback=self.set_exptime)

        # Bit depth (output packing / BITSPERCHANNEL) --------------------------------------
        bitdepth = (
            self.bitdepth
            if self.bitdepth is not None
            else (int(self.config.bitdepth) if int(self.config.bitdepth) > 0 else 16)
        )
        nv = properties.NumberVector(name="bitdepth", perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name="current",
            label="Bit depth",
            format="%d",
            min=float(self.bitdepth_min),
            max=float(self.bitdepth_max),
            step=1,
            _value=int(bitdepth),
        ))
        nv.add_element(DefNumber(
            name="target",
            label="Requested bit depth",
            format="%d",
            min=float(self.bitdepth_min),
            max=float(self.bitdepth_max),
            step=1,
            _value=int(bitdepth),
        ))
        self.add_property(nv, callback=self.set_bitdepth)

        # Last DCAM Error -------------------------------------------------------------------
        #tv = properties.TextVector(name='error')
        #tv.add_element(DefText(
        #    name='current', label="Last Dcam Error", _value=self.lasterr
        #))
        #self.add_property(tv)

        # Gain ------------------------------------------------------------------------------
        # nv = properties.NumberVector(name='gain', perm=constants.PropertyPerm.READ_WRITE)
        # nv.add_element(DefNumber(
        #     name='current', label='Gain', format='%d',
        #     min=0, max=100, step=1, _value=self.gain
        # ))
        # nv.add_element(DefNumber(
        #     name='target', label='Requested gain', format='%d',
        #     min=0, max=100, step=1, _value=self.gain
        # ))
        # self.add_property(nv, callback=self.set_gain)

        # ALU Settings
        ## Defect Correct Mode --------------------------------------------------------------
        ### Current Mode
        defect_mode = (
            self.defectCorrect_Mode
            if self.defectCorrect_Mode is not None
            else self.config.defectCorrect_Mode
        )
        nv = properties.NumberVector(name='defectCorrect_Mode', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='defectCorrect_Mode', format='%3.1f',
            min=1.0, max=2.0, step=1, _value=defect_mode
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested defectCorrect_Mode', format='%3.1f',
            min=1.0, max=2.0, step=1, _value=defect_mode
        ))
        self.add_property(nv, callback=self.set_defectCorrect_Mode)

        tv = properties.TextVector(name="defectMode")
        tv.add_element(DefText(
            name='mode', label='Defect Correction Mode', _value=self.dc_mode or "N/A",
        ))
        self.add_property(tv)

        ## Hot Pixel Correction Level
        nv = properties.NumberVector(name='hotpixel_lvl', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='hotpixel_lvl', format='%3.1f',
            min=1.0, max=3.0, step=1, _value=self.hotpixel_lvl if self.hotpixel_lvl is not None else 1.0
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested hotpixel_lvl', format='%3.1f',
            min=1.0, max=3.0, step=1, _value=self.hotpixel_lvl if self.hotpixel_lvl is not None else 1.0
        ))
        self.add_property(nv, callback=self.set_hotpixel_lvl)

        tv = properties.TextVector(name="hotpixel_status")
        tv.add_element(DefText(
            name='status', label='Hot Pixel Status', _value=self.hp_status or "N/A",
        ))
        self.add_property(tv)

        # Binning ----------------------------------------------------------------------------
        binning = self.binning if self.binning is not None else self.config.binning
        nv = properties.NumberVector(name='binning', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='Binning', format='%3.1f',
            min=1, max=4, step=1, _value=binning
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested binning', format='%3.1f',
            min=1, max=4, step=1, _value=binning
        ))
        self.add_property(nv, callback=self.set_binning)

        # ROI
        ## HPosition ------------------------------------------------------------------------
        hpos = self.hpos if self.hpos is not None else int(self.config.hpos)
        nv = properties.NumberVector(name='hpos', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='hpos (pixels)', format='%d',
            min=0, max=4432, step=4, _value=hpos
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested hpos (pixels)', format='%d',
            min=0, max=4432, step=4, _value=hpos
        ))
        self.add_property(nv, callback=self.set_hpos)

        ## VPosition -----------------------------------------------------------------------
        vpos = self.vpos if self.vpos is not None else int(self.config.vpos)
        nv = properties.NumberVector(name='vpos', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='vpos (pixels)', format='%d',
            min=0, max=2364, step=4, _value=vpos
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested vpos (pixels)', format='%d',
            min=0, max=2364, step=4, _value=vpos
        ))
        self.add_property(nv, callback=self.set_vpos)

        ## HSize ----------------------------------------------------------------------------
        hsize = self.hsize if self.hsize is not None else int(self.config.hsize)
        nv = properties.NumberVector(name='hsize', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='hsize (pixels)', format='%d',
            min=4, max=4432, step=4, _value=hsize
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested hsize (pixels)', format='%d',
            min=4, max=4432, step=4, _value=hsize
        ))
        self.add_property(nv, callback=self.set_hsize)

        ## VSize ----------------------------------------------------------------------------
        vsize = self.vsize if self.vsize is not None else int(self.config.vsize)
        nv = properties.NumberVector(name='vsize', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='vsize (pixels)', format='%d',
            min=4, max=2368, step=4, _value=vsize
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested vsize (pixels)', format='%d',
            min=4, max=2368, step=4, _value=vsize
        ))
        self.add_property(nv, callback=self.set_vsize)

        ## END ROI---------------------------------------------------------------------------

        # Temperature -----------------------------------------------------------------------
        nv = properties.NumberVector(name='temp')
        nv.add_element(DefNumber(
            name='current', label='Current Temperature (deg C)', format='%3.1f',
            min=-50, max=100, step=0.1, _value=self.temp if self.temp is not None else 0.0
        ))
        self.add_property(nv)

        # Temp Status -----------------------------------------------------------------------
        nv = properties.NumberVector(name='temp_status')
        nv.add_element(DefNumber(
            name='current', label='Cooler status code (SENSORCOOLERSTATUS)', format='%3.1f',
            min=-5, max=5, step=1, _value=self.temp_status if self.temp_status is not None else 0.0
        ))
        self.add_property(nv)

        tv = properties.TextVector(name="temp_state")
        tv.add_element(DefText(
            name='status', label='Temperature State', _value=self.tmp_state or "N/A",
        ))
        self.add_property(tv)

        # Cooler capability / optional controls ---------------------------------------------
        tv = properties.TextVector(name="cooler_capability")
        tv.add_element(DefText(
            name='status',
            label='Cooler DCAM capability (unsupported|status_only|controllable)',
            _value=self.cooler_capability,
        ))
        self.add_property(tv)

        # Always expose cooler / fan as RW vectors; setters no-op with a log if unsupported.
        nv = properties.NumberVector(name='cooler', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='SENSORCOOLER (1=OFF, 2=ON, 4=MAX)', format='%3.1f',
            min=1.0, max=4.0, step=1.0,
            _value=self.cooler_value if self.cooler_value is not None else 2.0,
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested SENSORCOOLER', format='%3.1f',
            min=1.0, max=4.0, step=1.0,
            _value=self.cooler_value if self.cooler_value is not None else 2.0,
        ))
        self.add_property(nv, callback=self.set_cooler)

        nv = properties.NumberVector(name='cooler_fan', perm=constants.PropertyPerm.READ_WRITE)
        nv.add_element(DefNumber(
            name='current', label='SENSORCOOLERFAN (if supported by camera)', format='%3.1f',
            min=1.0, max=4.0, step=1.0,
            _value=self.cooler_fan_value if self.cooler_fan_value is not None else 2.0,
        ))
        nv.add_element(DefNumber(
            name='target', label='Requested SENSORCOOLERFAN', format='%3.1f',
            min=1.0, max=4.0, step=1.0,
            _value=self.cooler_fan_value if self.cooler_fan_value is not None else 2.0,
        ))
        self.add_property(nv, callback=self.set_cooler_fan)

        # Internal Frame rate (1/sec) -------------------------------------------------------
        # Read-only INDI exposure. Backend set_frame_rate() remains for models where
        # INTERNALFRAMERATE is writable, but we do not publish a target element here.
        fr = self.frame_rate if self.frame_rate is not None else 0.0
        nv = properties.NumberVector(name="frame_rate", perm=constants.PropertyPerm.READ_ONLY)
        nv.add_element(DefNumber(
            name='current', label='Current Frame Rate (1/sec)', format='%3.3f',
            min=self.frame_rate_min, max=self.frame_rate_max, step=0.0, _value=fr,
        ))
        self.add_property(nv)

        # Shmim Info ------------------------------------------------------------------------
        tv = properties.TextVector(name="fg_shmimName")
        tv.add_element(DefText(
            name='name', label='Shmim Name', _value=self.shmim_name,
        ))
        self.add_property(tv)

        nv = properties.NumberVector(name="fg_framesize")
        nv.add_element(DefNumber(
            name='height', label='Frame size height', format='%d',
            min=4, max=2368, step=4,
            _value=self.height if self.height is not None else int(self.config.vsize),
        ))
        nv.add_element(DefNumber(
            name='width', label='Frame size width', format='%d',
            min=4, max=4432, step=4,
            _value=self.width if self.width is not None else int(self.config.hsize),
        ))
        self.add_property(nv)

        # End of properties -------------------------------------------------
        self.log.info("Set up properties complete")


    def setup(self):
        self._stream_lock = threading.RLock()
        self._stream_stop = threading.Event()
        self.th = None
        self.shmim_name = self.config.shmim_name or self.name
        self.data_dir = self.config.data_dir or f"/opt/MagAOX/rawimages/{self.name}"
        self.log.info(
            "Using shmim_name=%s data_dir=%s camera_id=%r",
            self.shmim_name,
            self.data_dir,
            self.config.camera_id,
        )
        os.makedirs(self.data_dir, exist_ok=True)
        while self.client.status is not constants.ConnectionStatus.CONNECTED:
            self.log.info(f"Connecting to INDI as a client to get {list(EXTERNAL_RECORDED_PROPERTIES.keys())}")
            time.sleep(1)
        self.log.info(f"INDI client connection: {self.client.status}")

        self._init_properties()
        self.properties['fsm']['state'] = 'NOTCONNECTED'
        self.log.info("Set FSM properties")
        self.update_property(self.properties['fsm'])
        self.log.info("Set up complete")


    def update_from_camera(self):
        # If no camera is detected
        #print("Updating from camera")
        if self.cam is None:
            self.log.debug("No camera detected. Cannot update from camera.")
            return
        # Otherwise get values from camera / This can be done better / temp
        self.exptime = self.cam.prop_getvalue(DCAM_IDPROP.EXPOSURETIME)
        #self.gain = self.cam.prop_getvalue(DCAM_IDPROP.CONTRASTGAIN)
        self.hsize = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHSIZE))
        self.vsize = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVSIZE))
        self.hpos = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHPOS))
        self.vpos = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVPOS))
        self.temp = self.cam.prop_getvalue(DCAM_IDPROP.SENSORTEMPERATURE)
        #self.temp_target = self.cam.prop_getvalue(DCAM_IDPROP.SENSORTEMPERATURETARGET)
        self.defectCorrect_Mode = self.cam.prop_getvalue(DCAM_IDPROP.DEFECTCORRECT_MODE)
        self.hotpixel_lvl = self.cam.prop_getvalue(DCAM_IDPROP.HOTPIXELCORRECT_LEVEL)
        #self.temp_status = self.cam.prop_getvalue(DCAM_IDPROP.SENSORTEMPERATURE_STATUS) #This one works but not editable
        self.temp_status = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLERSTATUS) #SENSORCOOLERSTATUS
        #self.lasterr = self.cam.lasterr() # testing for getting last error for quicker view
        self.binning = self.cam.prop_getvalue(DCAM_IDPROP.BINNING)
        self.frame_rate = self.cam.prop_getvalue(DCAM_IDPROP.INTERNALFRAMERATE)
        self.width = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.IMAGE_WIDTH))
        self.height = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.IMAGE_HEIGHT))
        self.bitdepth = self._read_bitdepth_from_camera()
        self.frame_dtype = self._frame_dtype_for_bitdepth(self.bitdepth)
        self.log.debug(
            "Read from camera: exptime=%s hsize=%s vsize=%s hpos=%s vpos=%s temp=%s "
            "binning=%s defect_correction=%s frame_rate=%s bitdepth=%s",
            self.exptime,
            self.hsize,
            self.vsize,
            self.hpos,
            self.vpos,
            self.temp,
            self.binning,
            self.defectCorrect_Mode,
            self.frame_rate,
            self.bitdepth,
        )

    def refresh_properties(self):
        
        self.update_from_camera()

        self.properties['hsize']['current'] = self.hsize
        self.properties['hsize']['target'] = self.hsize
        self.update_property(self.properties['hsize'])

        self.properties['vsize']['current'] = self.vsize
        self.properties['vsize']['target'] = self.vsize
        self.update_property(self.properties['vsize'])

        self.properties['hpos']['current'] = self.hpos
        self.properties['hpos']['target'] = self.hpos
        self.update_property(self.properties['hpos'])

        self.properties['vpos']['current'] = self.vpos
        self.properties['vpos']['target'] = self.vpos
        self.update_property(self.properties['vpos'])

        self.properties['exptime']['current'] = self.exptime
        self.properties['exptime']['target'] = self.exptime
        self.update_property(self.properties['exptime'])

        if self.bitdepth is not None and "bitdepth" in self.properties:
            self.properties["bitdepth"]["current"] = int(self.bitdepth)
            self.properties["bitdepth"]["target"] = int(self.bitdepth)
            self.update_property(self.properties["bitdepth"])

        #self.properties['error']['current'] = self.lasterr # testing

        #self.properties['gain']['current'] = self.gain
        #self.properties['gain']['target'] = self.gain
        #self.update_property(self.properties['gain'])

        self.properties['defectCorrect_Mode']['current'] = self.defectCorrect_Mode
        self.properties['defectCorrect_Mode']['target'] = self.defectCorrect_Mode
        self.update_property(self.properties['defectCorrect_Mode'])

        self.properties['defectMode']['mode'] = self.dc_mode
        self.update_property(self.properties['defectMode'])

        self.properties['hotpixel_lvl']['current'] = self.hotpixel_lvl
        self.properties['hotpixel_lvl']['target'] = self.hotpixel_lvl
        self.update_property(self.properties['hotpixel_lvl'])

        self.properties['hotpixel_status']['status'] = self.hp_status
        self.update_property(self.properties['hotpixel_status'])

        self.properties['binning']['current'] = self.binning
        self.properties['binning']['target'] = self.binning
        self.update_property(self.properties['binning'])

        self.properties['temp']['current'] = self.temp
        #self.properties['temp']['target'] = self.temp_target
        self.update_property(self.properties['temp'])

        self.properties['frame_rate']['current'] = self.frame_rate
        self.update_property(self.properties['frame_rate'])

        self.properties['temp_status']['current'] = self.temp_status
        self.update_property(self.properties['temp_status'])

        self.properties['temp_state']['status'] = self.tmp_state
        self.update_property(self.properties['temp_state'])

        if "cooler_capability" in self.properties:
            self.properties["cooler_capability"]["status"] = self.cooler_capability
            self.update_property(self.properties["cooler_capability"])

        self.properties['fg_framesize']['width'] = self.width
        self.properties['fg_framesize']['height'] = self.height
        self.update_property(self.properties['fg_framesize'])

    # Testing Item
    def teardown(self):
        self.log.info('Shutting down.')
        self.pause_stream()
        if self.cam is not None:
            self.cam.dev_close()
            self.cam = None
        Dcamapi.uninit()

    def _sync_image_dims(self):
        """Refresh width/height from the camera after ROI / binning changes."""
        if self.cam is None:
            return
        self.width = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.IMAGE_WIDTH))
        self.height = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.IMAGE_HEIGHT))
        self.hsize = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHSIZE))
        self.vsize = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVSIZE))
        self.hpos = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHPOS))
        self.vpos = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVPOS))

    def _create_cam_shmim(self, name, shape, np_dtype):
        """Create an ImageStreamIO shmim with a buffer matching ``np_dtype``."""
        img = ISIO.Image()
        img.create(name, np.zeros(shape, dtype=np_dtype))

    def _ensure_camstream(self):
        """Open or recreate the ImageStream to match current image dimensions/dtype."""
        width = int(self.width if self.width is not None else self.hsize)
        height = int(self.height if self.height is not None else self.vsize)
        # milk / ImageStream size convention used elsewhere in this app: (width, height)
        shmim_shape = (width, height)
        np_dtype = self.frame_dtype if self.frame_dtype is not None else np.uint16

        if self.camstream is not None:
            try:
                curshape = list(self.camstream.md.size)
                curdtype = getattr(self.camstream, "dtype", None)
                needs_recreate = (
                    list(curshape) != list(shmim_shape)
                    or np.dtype(curdtype) != np.dtype(np_dtype)
                )
                if not needs_recreate:
                    return True
                self.log.info(
                    "Recreating shmim %s: shape %s→%s dtype %s→%s",
                    self.shmim_name,
                    curshape,
                    shmim_shape,
                    curdtype,
                    np.dtype(np_dtype),
                )
                try:
                    self.camstream.destroy()
                except Exception:
                    pass
                try:
                    self.camstream.close()
                except Exception:
                    self.log.exception("Failed to close previous shmim")
                self.camstream = None
            except Exception:
                self.log.exception("Failed to inspect existing shmim; recreating")
                self.camstream = None

        try:
            self.camstream = ImageStream(
                self.shmim_name,
                expected_shape=shmim_shape,
                dtype=np_dtype,
            )
            if np.dtype(self.camstream.dtype) != np.dtype(np_dtype):
                self.log.info(
                    "Existing shmim dtype %s != %s; recreating",
                    self.camstream.dtype,
                    np.dtype(np_dtype),
                )
                try:
                    self.camstream.destroy()
                except Exception:
                    pass
                try:
                    self.camstream.close()
                except Exception:
                    pass
                self.camstream = None
                raise RuntimeError("shmim dtype mismatch")
            return True
        except (RuntimeError, ValueError) as exc:
            self.log.info(
                "Failed to open shmim %s (%s). Creating shape=%s dtype=%s",
                self.shmim_name,
                exc,
                shmim_shape,
                np.dtype(np_dtype),
            )
            self._create_cam_shmim(self.shmim_name, shmim_shape, np_dtype)
            self.camstream = ImageStream(
                self.shmim_name,
                expected_shape=shmim_shape,
                dtype=np_dtype,
            )
            return True

    def _publish_streaming_toggle(self):
        """Publish the MagAOX-standard streaming.toggle switch state."""
        if "streaming" not in self.properties:
            return
        self.properties["streaming"]["toggle"] = (
            constants.SwitchState.ON if self.streaming else constants.SwitchState.OFF
        )
        self.update_property(self.properties["streaming"])

    def _maybe_restart_stream(self):
        """Restart capture only when streaming.toggle is On."""
        if self.streaming:
            return self.start_stream()
        return True

    def handle_streaming(self, existing_property, new_message):
        """INDI callback for streaming.toggle (start/stop frame acquisition)."""
        if "toggle" not in new_message:
            self.update_property(existing_property)
            return

        want_on = new_message["toggle"] is constants.SwitchState.ON
        if want_on:
            if self.cam is None:
                self.log.warning("Cannot start streaming: camera not open")
                self.streaming = False
            else:
                self.log.info("Streaming toggle ON — starting frame acquisition")
                self.streaming = True
                if not self.start_stream():
                    self.log.error("Failed to start streaming")
                    self.streaming = False
        else:
            self.log.info("Streaming toggle OFF — stopping frame acquisition")
            self.streaming = False
            self.pause_stream()

        existing_property["toggle"] = (
            constants.SwitchState.ON if self.streaming else constants.SwitchState.OFF
        )
        self.update_property(existing_property)

    def start_stream(self):
        """Allocate buffers and start the capture thread for the current ROI."""
        if self._stream_lock is None:
            self._stream_lock = threading.RLock()
        with self._stream_lock:
            self.log.info("Starting stream for hamamatsu")
            if self.cam is None:
                self.log.warning("Cannot start stream: camera not open")
                return False

            # Ensure any previous capture thread is fully stopped before reallocating.
            self._pause_stream_locked()
            self._sync_image_dims()
            if not self._ensure_camstream():
                return False

            if self._stream_stop is None:
                self._stream_stop = threading.Event()
            self._stream_stop.clear()

            if not self.cam.buf_alloc(10):
                self.log.error("buf_alloc(10) failed: %s", self.cam.lasterr())
                try:
                    self.cam.buf_release()
                except Exception:
                    pass
                return False

            self.th = threading.Thread(target=self.stream_thread, name=f"{self.name}-stream", daemon=True)
            self.th.start()
            return True

    def stream_thread(self):
        """Capture frames until pause_stream()/cap_stop() ends BUSY state."""
        self.log.info("In stream thread")
        # Short timeout so pause_stream can join quickly after cap_stop().
        timeout_ms = 500
        if self.cam is None:
            return

        if not self.cam.cap_start():
            self.log.error("cap_start failed: %s", self.cam.lasterr())
            return

        expected_dtype = np.dtype(self.frame_dtype if self.frame_dtype is not None else np.uint16)
        self.log.info(
            "cam.cap_start working (status=%s dtype=%s)",
            self.cam.cap_status(),
            expected_dtype,
        )
        while self._stream_stop is None or not self._stream_stop.is_set():
            cam_status = self.cam.cap_status()
            if cam_status != DCAMCAP_STATUS.BUSY:
                self.log.info("Capture left BUSY state (status=%s lasterr=%s)", cam_status, self.cam.lasterr())
                break

            if not self.cam.wait_capevent_frameready(timeout_ms):
                err = self.cam.lasterr()
                if err.is_timeout():
                    continue
                self.log.info("wait_capevent_frameready failed: %s", err)
                break

            data = self.cam.buf_getlastframedata()
            if data is False or not hasattr(data, "dtype"):
                continue
            if data.dtype != expected_dtype:
                self.log.warning(
                    "Unexpected frame dtype %s (expected %s); skipping",
                    data.dtype,
                    expected_dtype,
                )
                continue

            height = int(self.height if self.height is not None else self.vsize)
            width = int(self.width if self.width is not None else self.hsize)
            expected = height * width
            if data.size != expected:
                self.log.error(
                    "Frame size mismatch: got %s pixels, expected %s (%sx%s); skipping frame",
                    data.size,
                    expected,
                    height,
                    width,
                )
                continue

            try:
                rawframe = np.frombuffer(data, dtype=expected_dtype).reshape(height, width)
                if self.camstream is not None:
                    self.camstream.write(rawframe)
            except Exception:
                self.log.exception("Failed to write frame to shmim")
                # Bail out; pause/restart will recover with a fresh stream.
                break

    def set_bitdepth(self, existing_property, new_message):
        """INDI callback: set output bit depth via BITSPERCHANNEL / IMAGE_PIXELTYPE."""
        if "target" not in new_message:
            self.update_property(existing_property)
            return False

        if self.cam is None:
            self.log.warning("Cannot set bitdepth: camera not open")
            self.update_property(existing_property)
            return False

        requested = int(round(float(new_message["target"])))
        if self.bitdepth is not None and requested == int(self.bitdepth):
            existing_property["current"] = int(self.bitdepth)
            existing_property["target"] = int(self.bitdepth)
            self.update_property(existing_property)
            return True

        if self.bitdepth_options and requested not in self.bitdepth_options:
            self.log.warning(
                "Rejected bitdepth=%s; available options=%s",
                requested,
                self.bitdepth_options,
            )
            if self.bitdepth is not None:
                existing_property["current"] = int(self.bitdepth)
                existing_property["target"] = int(self.bitdepth)
            self.update_property(existing_property)
            return False

        self.pause_stream()
        ok = self._apply_bitdepth(requested)
        if not ok:
            self._probe_bitdepth_capabilities()
            if self.bitdepth is not None:
                existing_property["current"] = int(self.bitdepth)
                existing_property["target"] = int(self.bitdepth)
            self.update_property(existing_property)
            self._maybe_restart_stream()
            return False

        existing_property["current"] = int(self.bitdepth)
        existing_property["target"] = int(self.bitdepth)
        for elem_name in ("current", "target"):
            elem = existing_property._elements[elem_name]
            elem.min = float(self.bitdepth_min)
            elem.max = float(self.bitdepth_max)
        self.define_property(existing_property)
        self._maybe_restart_stream()
        return True

    def pause_stream(self):
        """Stop capture, join the stream thread, and release DCAM buffers."""
        if self._stream_lock is None:
            self._stream_lock = threading.RLock()
        with self._stream_lock:
            self._pause_stream_locked()

    def _pause_stream_locked(self):
        """Internal pause; caller must hold `_stream_lock`."""
        self.log.info("Pausing Stream")
        if self._stream_stop is None:
            self._stream_stop = threading.Event()
        self._stream_stop.set()

        if self.cam is not None:
            try:
                self.cam.cap_stop()
            except Exception:
                self.log.exception("cap_stop failed")

        th = self.th
        if th is not None and th.is_alive():
            th.join(timeout=2.0)
            if th.is_alive():
                self.log.warning("Stream thread did not exit within 2s")
        self.th = None

        if self.cam is not None:
            try:
                self.cam.buf_release()
            except Exception:
                self.log.exception("buf_release failed")

    # Not needed anymore but was for initial tests
    def show_framedata(self, data, status):
        """
        Testing this
        """
        self.log.info("Trying to show framedata")
        if data.dtype == np.uint16:
            imax = np.amax(data)
            if imax > 0:
                imul = int(65535/imax)
                data = data * imul
            return 1
        else:
            self.log.info("Issues with showing framedata")

    # Dependent on hamamatsu camera type / model checking not implemented yet
    def check_fan(self):
        """Report cooler-fan availability for this camera model/firmware.

        Returns:
            True if SENSORCOOLERFAN is present and currently ON-like (>OFF),
            False if present and OFF, None if the property is unavailable.
        """
        self._probe_cooler_capabilities()
        if not self._prop_attr(DCAM_IDPROP.SENSORCOOLERFAN):
            self.log.info(
                "SENSORCOOLERFAN is not available on this camera "
                "(cooler_capability=%s)",
                self.cooler_capability,
            )
            return None
        self.fan_status = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLERFAN)
        self.log.info("SENSORCOOLERFAN value: %s", self.fan_status)
        if self.fan_status is False:
            return None
        return float(self.fan_status) != float(DCAMPROP.MODE.OFF)

    def set_cooler(self, existing_property, new_message):
        """Set SENSORCOOLER if the camera exposes a writable cooler property.

        Modes: OFF=1, ON=2, MAX=4. On C16240-20UP this property does not exist.
        """
        if 'target' not in new_message:
            self.update_property(existing_property)
            return False

        if self.cam is None:
            self.log.warning("Cannot set cooler: camera not open")
            self.update_property(existing_property)
            return False

        if not self.cooler_writable:
            self._probe_cooler_capabilities()
        if not self.cooler_writable:
            self.log.warning(
                "SENSORCOOLER is not writable/available on this camera "
                "(cooler_capability=%s); ignoring target=%s",
                self.cooler_capability,
                new_message['target'],
            )
            self._sync_cooler_indi()
            self.update_property(existing_property)
            return False

        requested = float(new_message['target'])
        if not self.cam.prop_setvalue(DCAM_IDPROP.SENSORCOOLER, requested):
            self.log.error("Failed to set SENSORCOOLER=%s: %s", requested, self.cam.lasterr())
            self.update_property(existing_property)
            return False

        actual = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLER)
        self.cooler_value = None if actual is False else float(actual)
        existing_property['current'] = self.cooler_value
        existing_property['target'] = self.cooler_value
        self.update_property(existing_property)
        self.log.info("SENSORCOOLER set to %s", self.cooler_value)
        return True

    def set_cooler_fan(self, existing_property, new_message):
        """Set SENSORCOOLERFAN if available.

        On C16240-20UP this property does not exist via DCAM; fan speed is
        configured through Hamamatsu's DCAM Configurator instead.
        """
        if 'target' not in new_message:
            self.update_property(existing_property)
            return False

        if self.cam is None:
            self.log.warning("Cannot set cooler fan: camera not open")
            self.update_property(existing_property)
            return False

        if not self.cooler_fan_writable:
            self._probe_cooler_capabilities()
        if not self.cooler_fan_writable:
            self.log.warning(
                "SENSORCOOLERFAN is not writable/available on this camera "
                "(cooler_capability=%s); ignoring target=%s",
                self.cooler_capability,
                new_message['target'],
            )
            self._sync_cooler_indi()
            self.update_property(existing_property)
            return False

        requested = float(new_message['target'])
        if not self.cam.prop_setvalue(DCAM_IDPROP.SENSORCOOLERFAN, requested):
            self.log.error(
                "Failed to set SENSORCOOLERFAN=%s: %s",
                requested,
                self.cam.lasterr(),
            )
            self.update_property(existing_property)
            return False

        actual = self.cam.prop_getvalue(DCAM_IDPROP.SENSORCOOLERFAN)
        self.cooler_fan_value = None if actual is False else float(actual)
        existing_property['current'] = self.cooler_fan_value
        existing_property['target'] = self.cooler_fan_value
        self.update_property(existing_property)
        self.log.info("SENSORCOOLERFAN set to %s", self.cooler_fan_value)
        return True

    def set_defectCorrect_Mode(self, existing_property, new_message):
        """ For selecting if camera is in defect Correction mode or not
            DCAM_IDPROP_DEFECTCORRECT_MODE
                OFF (1.0)
                ON (2.0) (default)
        Args:
            existing_property (float): Current status
            new_message (float): Target / requested status change
        """
        #self.pause_stream()
        self.log.debug(f"Setting defect correction mode")
        if self.cam is None:
            self.log.debug('-NG: Dcamcon is not opened')
            return False
        # prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
        mode_requested = float(new_message['target'])
        self.log.info(f"Mode requested: {mode_requested}")
        mode_check = self.cam.prop_getvalue(DCAM_IDPROP.DEFECTCORRECT_MODE)
        self.log.info(f"Current mode is: {mode_check}")
        if mode_requested == 2.0:
            self.log.info(f'Setting mode to {mode_requested} (Defect Correction mode is being turned on)')
            self.cam.prop_setvalue(DCAM_IDPROP.DEFECTCORRECT_MODE, 2.0)
            if mode_check == DCAMPROP.MODE.ON:
                self.log.info("DCAM_IDPROP.DEFECTCORRECT_MODE is == to DCAMPROP.MODE.ON")
        elif mode_requested == 1.0:
            self.log.info(f'Setting mode to {mode_requested} (Defect Correction mode is being turned off)')    
            self.cam.prop_setvalue(DCAM_IDPROP.DEFECTCORRECT_MODE, 1.0)
            if mode_check == DCAMPROP.MODE.OFF:
                self.log.info("DCAM_IDPROP.DEFECTCORRECT_MODE is == to DCAMPROP.MODE.OFF")
        else:
            self.log.info("mode requested is neither 1.0 or 2.0?")

        mode_actual = float(self.cam.prop_getvalue(DCAM_IDPROP.DEFECTCORRECT_MODE))
        self.log.info(f'Went to a defect correction mode of {mode_actual}')

        if mode_requested != mode_actual:
            self.log.info(f"Mode request does not = actual mode.")
        else:
            existing_property['current'] = new_message['target']
            existing_property['target'] = new_message['target']
            self.defectCorrect_Mode = mode_actual
            self.update_property(existing_property)

        #self.start_stream()
        self.get_defectCorrect_status()
        self.update_property(existing_property)
        return True

    def get_defectCorrect_status(self):
        """
        Adding indicator / string for what the defect correction mode float translates too
        
        DCAM_IDPROP_DEFECTCORRECT_MODE
            OFF (1.0)
            ON (2.0) (default)

        Sets dc_mode to the string value of the float translation for the mode

        """

        if self.defectCorrect_Mode == 1.0:
            mode = "OFF"
        elif self.defectCorrect_Mode == 2.0:
            mode = "ON"
        else:
            self.log.info("Defect Correction Mode set out of bounds or is not an available feature.")
            mode = "N/A"
        
        self.dc_mode = mode
        
        return True

    def set_hotpixel_lvl(self, existing_property, new_message):
        """
        Setting level for hot pixel settings

        DCAM_IDPROP_HOTPIXELCORRECT_LEVEL
            STANDARD (default) = 1
            MINIMUM = 2
            AGGRESSIVE = 3
        """
        if 'target' in new_message and new_message['target'] != existing_property['current']:
            # Adding indicator in logs for what the float translates to
            if float(new_message['target']) == 1.0:
                level = "STANDARD"
            elif float(new_message['target']) == 2.0:
                level = "MINIMUM"
            else:
                level = "AGGRESSIVE"
            self.log.info(f"Setting hotpixel correction level to {level}")

            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            else:# prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
                requested = float(new_message['target'])
                self.log.info(f'Requested value: {requested}')
                self.cam.prop_setvalue(DCAM_IDPROP.HOTPIXELCORRECT_LEVEL, requested)
                actual = self.cam.prop_getvalue(DCAM_IDPROP.HOTPIXELCORRECT_LEVEL)
                self.log.info(f'Actual: {actual}')
                if requested != actual:
                    self.log.info(f"Requested != actual.")
                else:
                    existing_property['current'] = new_message['target']
                    existing_property['target'] = new_message['target']
                    self.hotpixel_lvl = actual
                    self.update_property(existing_property)
        else:
            self.log.info(f"Target requested is equal to current set value")

        self.get_hotpixel_status()
        self.update_property(existing_property)
        return True


    def get_hotpixel_status(self):
        """
        Adding indicator / string for what the hot pixel level float translates too
        
        DCAM_IDPROP_HOTPIXELCORRECT_LEVEL
            STANDARD (default) = 1
            MINIMUM = 2
            AGGRESSIVE = 3

        Sets the hp_status to the string translation of the hotpixel correction level

        """

        if self.hotpixel_lvl == 1.0:
            lvl = "STANDARD"
        elif self.hotpixel_lvl == 2.0:
            lvl = "MINIMUM"
        elif self.hotpixel_lvl == 3.0:
            lvl = "AGGRESSIVE"
        else:
            self.log.info("Hot Pixel level set out of bounds or is not an available feature.")
            lvl = "N/A"
        
        self.hp_status = lvl
        
        return True


    def set_hpos(self, existing_property, new_message):
        """
        22-UP / Orca
        DCAM_IDPROP_SUBARRAYHPOS
            0 to 4096, step 4, default 0
                For DCAMPROP_SENSORMODE_AREA or PHOTONNUMBERRESOLVING
            0 to 4096, step 1, default 0
                For DCAMPROP_SENSORMODE_PROGRESS
        20-UP / Fire
            0 to 4428 , step 4 , default 0
                For DCAMPROP_SENSORMODE_AREA
            0 to 4431 , step 1 , default 0
                For DCAMPROP_SENSORMODE__PROGRESSIVE
        """
        ## Need to turn off subarray before modifying
        self.pause_stream()
        subarray = self.check_subarray()
        if subarray == True:
            self.switch_subarray()

        if 'target' in new_message and new_message['target'] != existing_property['current']:
            self.log.info(f"Setting hpos (x position)")
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            else:# prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
                hpos_requested = self._as_pixel_int(new_message['target'])
                self.log.info(f'Setting horizontal position to {hpos_requested} pixels')
                self.cam.prop_setvalue(DCAM_IDPROP.SUBARRAYHPOS, float(hpos_requested))
                hpos_actual = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHPOS))
                self.log.info(f'Went to an actual hpos of {hpos_actual} pixels')
                if hpos_requested != hpos_actual:
                    self.log.info(f"Hpos request does not = hpos actual.")
                else:
                    existing_property['current'] = hpos_actual
                    existing_property['target'] = hpos_actual
                    self.hpos = hpos_actual
                    self.update_property(existing_property)
        else:
            self.log.info(f"Target requested is equal to current set value for hpos")

        self.switch_subarray()  
        self.update_property(existing_property)
        self._maybe_restart_stream()
        return True
    
    def set_hsize(self, existing_property, new_message):
        """
        DCAM_IDPROP_SUBARRAYHSIZE
            4 to 4096, step 4, default 4096
                For DCAMPROP_SENSORMODE_AREA or PHOTONNUMBERRESOLVING
            1 to 4096, step 1, default 4096
                For DCAMPROP_SENSORMODE_PROGRESS
        20-UP (Fire)
            4 to 4432 , step 4 , default 4432
                For DCAMPROP_SENSORMODE_AREA
            1 to 4432 , step 1 , default 4432
                For DCAMPROP_SENSORMODE_PROGRESS
        """

        ## Need to turn off subarray before modifying
        self.pause_stream()
        subarray = self.check_subarray()
        if subarray == True:
            self.switch_subarray()

        self.log.debug(f"Setting hsize (width)")
        if 'target' in new_message and new_message['target'] != existing_property['current']:
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            else:
                hsize_requested = self._as_pixel_int(new_message['target'])
                self.log.info(f'Setting horizontal size to {hsize_requested} pixels')
                self.cam.prop_setvalue(DCAM_IDPROP.SUBARRAYHSIZE, float(hsize_requested))
                hsize_actual = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHSIZE))
                self.log.info(f'Went to an actual hsize of {hsize_actual} pixels')
                if hsize_requested != hsize_actual:
                    self.log.info(f"Hsize request does not = hsize actual.")
                else:
                    existing_property['current'] = hsize_actual
                    existing_property['target'] = hsize_actual
                    self.hsize = hsize_actual
                    self.update_property(existing_property)
        else:
            self.log.info(f"Target requested is equal to current set value for hsize")

        self.switch_subarray()
        self.update_property(existing_property)
        self._refresh_frame_rate_bounds(redefine=True)
        self._maybe_restart_stream()
        return True


    def set_vpos(self, existing_property, new_message):
        """
        DCAM_IDPROP_SUBARRAYVPOS
            0 to 2300, step 4, default 0
        """
        ## Need to turn off subarray before modifying
        self.pause_stream()
        subarray = self.check_subarray()
        if subarray == True:
            self.switch_subarray()

        self.log.info(f"Setting vpos (y position)")
        if 'target' in new_message and new_message['target'] != existing_property['current']:
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            else:# prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
                vpos_requested = self._as_pixel_int(new_message['target'])
                self.log.info(f'Setting vertical position to {vpos_requested} pixels')
                self.cam.prop_setvalue(DCAM_IDPROP.SUBARRAYVPOS, float(vpos_requested))
                vpos_actual = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVPOS))
                self.log.info(f'Went to an actual vpos of {vpos_actual} pixels')
                if vpos_requested != vpos_actual:
                    self.log.info(f"Vpos request does not = vpos actual.")
                else:
                    existing_property['current'] = vpos_actual
                    existing_property['target'] = vpos_actual
                    self.vpos = vpos_actual
                    self.update_property(existing_property)
        else:
            self.log.info(f"Target requested is equal to current set value for vpos")

        self.switch_subarray()  
        self.update_property(existing_property)
        self._maybe_restart_stream()
        return True
    
    
    def set_vsize(self, existing_property, new_message):
        """
        DCAM_IDPROP_SUBARRAYVSIZE
            4 to 2304, step 4, default 2304
        """
        ## Need to turn off subarray before modifying
        self.pause_stream()
        subarray = self.check_subarray()
        if subarray == True:
            self.switch_subarray()

        self.log.debug(f"Setting vsize (height)")
        if 'target' in new_message and new_message['target'] != existing_property['current']:
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            else:
                # prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
                vsize_requested = self._as_pixel_int(new_message['target'])
                self.log.info(f'Setting vertical size to {vsize_requested} pixels')
                self.cam.prop_setvalue(DCAM_IDPROP.SUBARRAYVSIZE, float(vsize_requested))
                vsize_actual = self._as_pixel_int(self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVSIZE))
                self.log.info(f'Went to an actual vsize of {vsize_actual} pixels')
                if vsize_requested != vsize_actual:
                    self.log.info(f"Height request does not = actual height.")
                else:
                    existing_property['current'] = vsize_actual
                    existing_property['target'] = vsize_actual
                    self.vsize = vsize_actual
                    self.update_property(existing_property)
        else:
            self.log.info(f"Target requested is equal to current set value for vsize")

        self.switch_subarray()
        self.update_property(existing_property)
        self._refresh_frame_rate_bounds(redefine=True)
        self._maybe_restart_stream()
        return True


    # Options dependent on model / no model checking yet
    def set_binning(self, existing_property, new_message):
        """
        Available Binning Options:
            [1]- 1x1 (DCAMPROP_BINNING_1)
            [2]- 2x2 (DCAMPROP_BINNING_2)
            [4]- 4x4 (DCAMPROP_BINNING_4)
            [8]- 8x8 (DCAMPROP_BINNING_8)
            [9]- 16x16 (DCAMPROP_BINNING_16)
        """
        self.pause_stream()
        self.log.debug(f"Setting binning")
        if 'target' in new_message and new_message['target'] != existing_property['current']:
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            # prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
            bin_requested = float(new_message['target'])
            self.log.info(f'Setting binning to {bin_requested}')                
            self.cam.prop_setvalue(DCAM_IDPROP.BINNING, bin_requested)
            bin_actual = self.cam.prop_getvalue(DCAM_IDPROP.BINNING)
            self.log.info(f'Went to a binning of {bin_actual}')
            if bin_requested != bin_actual:
                self.log.info(f"Binning request does not = actual binning.")
            else:
                existing_property['current'] = new_message['target']
                existing_property['target'] = new_message['target']
                self.binning = bin_actual
                self.update_property(existing_property)
        else:
            self.log.info(f"Target requested is equal to current set value for binning")
        self._refresh_frame_rate_bounds(redefine=True)
        self._maybe_restart_stream()


    def check_subarray(self):
        """
        ROI Mode Setting:
            DCAM_IDPROP_SUBARRAYMODE_ON / OFF
            Default- OFF
            Returns- bool value if subarraymode is on or not. True = ON, False = OFF
        """

        subarraymode = self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYMODE)
        self.log.info(f"In subarrarymode check: {subarraymode}")

        if subarraymode == DCAMPROP.MODE.ON:
            self.log.info(f"DCAM_IDPROP.SUBARRAYMODE is turned on")
            return True
        elif subarraymode == DCAMPROP.MODE.OFF:
            self.log.info(f"DCAM_IDPROP.SUBARRAYMODE is turned off")
            return False
        else:
            # Error happened
            self.log.info(f"Issues with detecting subarraymode")
            return False
    
    def switch_subarray(self):
        """
        ROI Mode Setting:
            DCAM_IDPROP_SUBARRAYMODE_ON / OFF
            Default- OFF
            Function- inverse / switch subarray setting based on initial value
            Return- bool value if successful
        """
        initialmode = self.check_subarray()

        if initialmode == True:
            self.cam.prop_setvalue(DCAM_IDPROP.SUBARRAYMODE, 1.0)
            newmode = self.check_subarray()
            if newmode == False:
                self.log.info(f"Successfully changed to ROI mode / Subarraymode")
                return True
            else:
                self.log.info(f"Issues with changing subarraymode off")
                return False
        elif initialmode == False:
            self.cam.prop_setvalue(DCAM_IDPROP.SUBARRAYMODE, 2.0)
            newmode = self.check_subarray()
            if newmode == True:
                self.log.info(f"Successfully changed to ROI mode / Subarraymode")
                return True
            else:
                self.log.info(f"Issues with changing subarraymode on")
                return False
        else:
            # Error happened
            self.log.info(f"Issues with detecting subarraymode")
            return False

    # Dependent on model type / model checking in app not implemented
    def set_temp(self, existing_property, new_message):
        """
        Target Temperature:
            DCAM_IDPROP_SENSORTEMPERATURETARGET
        """
        self.log.info("TRYING TO SET TARGET TEMP")

        if 'target' in new_message and new_message['target'] != existing_property['current']:
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            # prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
            temp_requested = float(new_message['target'])
            self.log.info(f'Setting TEMP to {temp_requested}')
            self.cam.prop_setvalue(DCAM_IDPROP.SENSORTEMPERATURETARGET, temp_requested)
            temp_actual = self.cam.prop_getvalue(DCAM_IDPROP.SENSORTEMPERATURETARGET)
            self.log.info(f"TEMP actually set too: {self.temp_status}")
            if temp_requested != temp_actual:
                self.log.info(f"Temp status not = to actual.")
                existing_property['target'] = new_message['target']
                self.update_property(existing_property)
            else:
                existing_property['current'] = new_message['target']
                existing_property['target'] = new_message['target']
                self.temp_status = temp_actual
                self.update_property(existing_property)

            print(temp_actual, temp_requested)
    
    
    # Dependent on model type / model checking not implemented
    def set_tempstatus(self, existing_property, new_message):
        """Legacy callback name: forward cooler requests to set_cooler()."""
        self.log.info("set_tempstatus redirected to set_cooler()")
        return self.set_cooler(existing_property, new_message)

    def get_tempstatus(self):
        """
        Map SENSORCOOLERSTATUS to a human-readable string in tmp_state.

            ERROR = (-4) - (-1)
            NONE = 0
            OFF = 1
            READY = 2
            BUSY = 3
            ALWAYS = 4
            WARNING = 5
        """
        status = self.temp_status

        match status:
            case -1.0 | -2.0 | -3.0 | -4.0:
                self.tmp_state = "ERROR"
            case 0.0:
                self.tmp_state = "NONE"
            case 1.0:
                self.tmp_state = "OFF"
            case 2.0:
                self.tmp_state = "READY"
            case 3.0:
                self.tmp_state = "BUSY"
            case 4.0:
                self.tmp_state = "ALWAYS"
            case 5.0:
                self.tmp_state = "WARNING"
            case _:
                self.tmp_state = "N/A"

        self.log.debug("Temperature Status: %s", self.tmp_state)

        return True


    # Dependent on camera model / model checking not implemented
    def set_gain(self, existing_property, new_message):
        """
        Setting Gain Value:
            DCAM_IDPROP.CONTRASTGAIN
        """
        self.pause_stream()
        self.log.debug("Gain requested!")
        if 'target' in new_message and new_message['target'] != existing_property['current']:
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            # prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
            gain_requested = float(new_message['target'])
            self.log.info(f'Setting exposure time to {gain_requested}')
            self.cam.prop_setvalue(DCAM_IDPROP.CONTRASTGAIN, gain_requested)
            gain_actual = self.cam.prop_getvalue(DCAM_IDPROP.CONTRASTGAIN)
            self.log.info(f'Went to an actual exposure time of {gain_actual}')
            if gain_requested != gain_actual:
                self.log.info(f"Exposure time request does not = gain actual.")
            else:
                existing_property['current'] = new_message['target']
                existing_property['target'] = new_message['target']
                self.gain = gain_actual
                self.update_property(existing_property)

            print(gain_actual, gain_requested)
        self._maybe_restart_stream()


    def set_exptime(self, existing_property, new_message):
        """
        Set the Exposure Time in seconds
            DCAM_IDPROP.EXPOSURETIME
            22-UP (Ora)
            0.000033949 to 1800.000015185, step 0.00000001, default 0.0082944
                DCAMPROP_SENSORMODE_AREA and DCAM_IDPROP_READOUTSPEED=1 or
                DCAMPROP_SENSORMODE_PHOTONNUMBERRESOLVING
                Depends on SUBARRAY properties
            0.0000072 to 1800.0, step 0.00000001, default 0.0082944
                DCAMPROP_SENSORMODE_AREA and DCAM_IDPROP_READOUTSPEED=2
                Depends on SUBARRAY properties
            0.0000072 to 0.0082944, step 0.00000001, default 0.0082944
                DCAMPROP_SENSORMODE_PROGRESSIVE 
                Depends on INTERNALLINESPEED 
                and INTERNAL_LINEINTERVAL, SUBARRY properties
            20-UP (Fire)
            0.000007309 to 10.000005818 , step 0.00000001 , default 0.008653964
                @ DCAMPROP_SENSORMODE__AREA
            0.000007309 to 0.008653964 , step 0.00000001 , default 0.008653964
                @ DCAMPROP_SENSORMODE__PROGRESSIVE
        """
        self.pause_stream()
        self.log.debug(f"Setting exposure time")
        if 'target' in new_message and new_message['target'] != existing_property['current']:
            if self.cam is None:
                self.log.debug('-NG: Dcamcon is not opened')
                return False
            # prop_setvalue(self, idprop: DCAM_IDPROP, fValue)
            exptime_requested = float(new_message['target'])
            self.log.info(f'Setting exposure time to {exptime_requested}')
            self.cam.prop_setvalue(DCAM_IDPROP.EXPOSURETIME, exptime_requested)
            exptime_actual = self.cam.prop_getvalue(DCAM_IDPROP.EXPOSURETIME)
            self.log.info(f'Went to an actual exposure time of {exptime_actual}')
            if exptime_requested != exptime_actual:
                self.log.info(f"Exposure time request does not = exptime actual.")
            else:
                existing_property['current'] = new_message['target']
                existing_property['target'] = new_message['target']
                self.exptime = exptime_actual
                self.update_property(existing_property)
        self._refresh_frame_rate_bounds(redefine=True)
        self._maybe_restart_stream()

    def loop(self):
        if self.cam is None:
            self.log.debug("Initializing camera...")
            success = self._init_camera()
            if not success:
                self.log.debug("No camera found yet, retrying on next loop")
                #self.properties['fsm']['state'] = 'NODEVICE' # Probably don't need this
                #self.update_property(self.properties['fsm']) # Probably don't need this
                return
            self.log.debug(f"Have camera: {self.cam}")
            self.properties['fsm']['state'] = 'CONNECTED'
            self.update_property(self.properties['fsm'])
        self.refresh_properties()

    # Placeholder if desired to be used
    def _gather_metadata(self):
        meta = {
            #'GAIN': self.cam.prop_getvalue(DCAM_IDPROP.CONTRASTGAIN),
            'EXPTIME': self.cam.prop_getvalue(DCAM_IDPROP.EXPOSURETIME),
            'HPOS': self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHPOS),
            'VPOS': self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVPOS),
            'HSIZE': self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYHSIZE),
            'VSIZE': self.cam.prop_getvalue(DCAM_IDPROP.SUBARRAYVSIZE),
            'BINNING': self.cam.prop_getvalue(DCAM_IDPROP.BINNING),
        }
        return meta

# Not fully up yet
class CameraStreamThread(threading.Thread):
    '''
    Camera stream thread to enable pausing and resume while setting parameters

    Needed for changing ROIs and some other settings.

    '''    
    # stolen from https://stackoverflow.com/a/15734837

    def __init__(self, cam, shmim, height, width):
        super(CameraStreamThread, self).__init__()
        self.iterations = 0
        self.daemon = True  # Allow main to exit even if still running.
        self.paused = True  # Start out paused.
        self.state = threading.Condition()

        self.cam = cam
        self.shmim = shmim
        self.height = height
        self.width = width

    def run(self):
        self.resume()
