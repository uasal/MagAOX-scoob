"""llowfscSim — optical + detector simulator driven by a MagAO-X camera INDI device.

Ports ``notebooks/rt-sims/run_camsci_cpp.ipynb`` into a MagAO-X Python app:

* Reads ``exptime`` / ``emgain`` / ``blacklevel`` / ``fps`` / ``bitDepth`` (and ROI)
  from a camera device (e.g. ``nsvCtrlSim``) via INDI — RO ``current`` mirrors only.
  Camera commands go to ``cam_name.<prop>.target`` (not this app).
* Owns the shutter (INDI toggle); other apps command this device, not a milk scalar.
* Writes simulated frames to a configurable output shmim (not the camera's own stream).
"""

from __future__ import annotations

import copy
import logging
import os
import time
from pathlib import Path
from typing import Optional

import numpy as np
import tomlkit
import xconf
from purepyindi2 import constants, properties
from purepyindi2.messages import DefNumber, DefSwitch, DefText

from magaox.indi.device import BaseConfig, XDevice
from magaox.shmim import Image

import ImageStreamIOWrap

log = logging.getLogger(__name__)


def create_if_not_exist_shmim(name: str, shape, dtype=np.float32):
    """Create or recreate a milk ImageStreamIO shmim if missing / wrong shape."""
    img = ImageStreamIOWrap.Image()
    want_dt = (
        ImageStreamIOWrap.ImageStreamIODataType.FLOAT
        if dtype == np.float32
        else ImageStreamIOWrap.ImageStreamIODataType.DOUBLE
    )
    if img.open(name) == 40:
        img.create(name, np.zeros(shape, dtype=dtype))
    else:
        if not (
            img.md.size[0] == shape[0]
            and img.md.size[1] == shape[1]
            and img.md.datatype == want_dt
        ):
            img.destroy()
            img.create(name, np.zeros(shape, dtype=dtype))
    img.close()


def _safe_float(val, default: float) -> float:
    try:
        if val is None:
            return default
        return float(val)
    except (TypeError, ValueError):
        return default


def _safe_int(val, default: int) -> int:
    try:
        if val is None:
            return default
        return int(val)
    except (TypeError, ValueError):
        return default


def _map_imx492_gain(raw: float) -> int:
    """IMX492 only allows gain 0 or 120; snap to nearest."""
    return 0 if abs(raw - 0.0) < abs(raw - 120.0) else 120


@xconf.config
class LlowfscSimConfig(BaseConfig):
    """Optical LLOWFSC / CAMSCI simulator configuration."""

    # --- camera / FSM INDI sources ---
    cam_name: str = xconf.field(
        default="nsv455sim",
        help="INDI device name of the camera providing exptime/emgain/fps/blacklevel/bitDepth/ROI.",
    )
    fsm_name: str = xconf.field(
        default="fsmsim",
        help="INDI device for FSM tip/tilt currents (fsm_name.x.current / y.current [nm]).",
    )

    # --- output stream (not the camera's own shmim) ---
    shm_output: str = xconf.field(
        default="camsci_sim",
        help="ImageStreamIO name for the simulated camera stream this app publishes.",
    )

    # --- optical model ---
    model_pack: str = xconf.field(
        default="1k-256",
        help="esc_llowfsc_sim data pack under esc_llowfsc_sim/data/<pack>/.",
    )
    wavelength_c: float = xconf.field(default=633e-9, help="Central wavelength [m].")
    ncamsci: int = xconf.field(default=512, help="Science-camera array size in the optical model.")
    nwaves: int = xconf.field(default=51, help="Number of wavelengths for entrance-flux integration.")
    bw: float = xconf.field(default=0.02, help="Fractional bandpass for entrance-flux integration.")
    cuda_device: str = xconf.field(default="0", help="CUDA_VISIBLE_DEVICES value.")
    dm_astig_rms: float = xconf.field(default=0.0, help="WFE override: DM astigmatism RMS [m].")
    compute_flat: bool = xconf.field(default=True, help="Compute DM flat during init_wfe.")
    plot_flat: bool = xconf.field(default=False, help="Plot DM flat during init_wfe (needs display).")

    # --- detector model (static bits; live params come from cam_name) ---
    dark_current: float = xconf.field(default=0.025, help="Detector dark current [e-/pix/s].")
    nbits: int = xconf.field(
        default=14,
        help="Internal ADC bit depth before rebin scaling (rebin comes from cam bitDepth).",
    )
    qe: float = xconf.field(default=0.75, help="Quantum efficiency.")
    default_exp_time: float = xconf.field(default=0.01, help="Fallback exposure [s] before camera INDI is available.")
    default_gain: float = xconf.field(default=120.0, help="Fallback gain before camera INDI is available.")
    default_blacklevel: float = xconf.field(default=10.0, help="Fallback blacklevel.")
    default_fps: float = xconf.field(default=200.0, help="Fallback loop rate [Hz] before camera fps is available.")
    default_bit_depth: int = xconf.field(default=16, help="Fallback rebin (= output bit depth).")

    # --- input shmims (DM / atmosphere / misc; FSM is via fsm_name INDI) ---
    shm_dm_total: str = xconf.field(default="dm01disp", help="Total DM command shmim (µm).")
    shm_dm_flat_channel: str = xconf.field(
        default="dm01disp00",
        help="Cacao DM channel 00 (µm). Read as the optical-model flat; never written.",
    )
    shm_vmag: str = xconf.field(default="vmag", help="Vega magnitude scalar shmim.")
    shm_opdsim: str = xconf.field(default="opdsim", help="10 Zernike coefficient shmim.")
    usevortex: bool = xconf.field(
        default=True,
        help="Insert the vortex coronagraph (INDI usevortex.toggle; On=in).",
    )
    dm_scale: float = xconf.field(default=1e-6, help="Multiply milk DM commands by this to get meters.")
    plant_dm_flat: bool = xconf.field(
        default=False,
        help="Deprecated/ignored. The app never writes the model flat onto cacao; it reads shm_dm_flat_channel.",
    )
    zero_other_dm_channels: bool = xconf.field(
        default=False,
        help="If true, zero dmXXXdisp01..11 at setup (can disturb live AO).",
    )

    # BaseConfig.sleep_interval_sec is unused for cadence; loop self-times from camera fps.
    sleep_interval_sec: float = xconf.field(
        default=0.0,
        help="XDevice outer sleep after each loop (keep 0; cadence is camera fps).",
    )


class LlowfscSim(XDevice):
    config: LlowfscSimConfig

    def setup(self):
        self._streaming = False
        self._shutter_closed = False
        self._use_vortex = bool(self.config.usevortex)
        self._cam_name = self.config.cam_name
        self._fsm_name = self.config.fsm_name
        self._shm_output = self.config.shm_output
        self._fps = float(self.config.default_fps)
        self._t0 = time.perf_counter()
        self._time_counter = 0.0
        self._fsm_x_nm = 0.0
        self._fsm_y_nm = 0.0
        # Last-known camera params (mirrored to our INDI; polled every loop tick).
        self._cam_exp = float(self.config.default_exp_time)
        self._cam_gain = float(self.config.default_gain)
        self._cam_blacklevel = float(self.config.default_blacklevel)
        self._cam_bitdepth = int(self.config.default_bit_depth)
        self._cam_roi_w = int(self.config.ncamsci)
        self._cam_roi_h = int(self.config.ncamsci)
        self._last_status = None  # (exp, gain, bl, fps, bitdepth) last published
        self._planted_flat = False
        self._logged_dm_missing = False
        self._dm_flat = None

        os.environ["CUDA_VISIBLE_DEVICES"] = str(self.config.cuda_device)

        self._init_indi_props()
        self._init_optical_model()
        self._init_shmims()
        self._subscribe_camera()
        self._subscribe_fsm()

        self.log.info(
            "llowfscSim ready: cam_name=%s fsm_name=%s output=%s ncamsci=%d usevortex=%s (toggle streaming to publish)",
            self._cam_name,
            self._fsm_name,
            self._shm_output,
            int(self._M.ncamsci),
            "ON" if self._use_vortex else "OFF",
        )

    # ------------------------------------------------------------------ INDI
    def _init_indi_props(self):
        # streaming
        # Note: do not set Vector.label — purepyindi2 rejects label/group on Set*Vector.
        sv = properties.SwitchVector(
            name="streaming",
            rule=constants.SwitchRule.ANY_OF_MANY,
            perm=constants.PropertyPerm.READ_WRITE,
        )
        sv.add_element(
            DefSwitch(name="toggle", label="Publish frames", _value=constants.SwitchState.OFF)
        )
        self.add_property(sv, callback=self.handle_streaming)

        # shutter (1/On = closed, matching milk polarity)
        sv = properties.SwitchVector(
            name="shutter",
            rule=constants.SwitchRule.ANY_OF_MANY,
            perm=constants.PropertyPerm.READ_WRITE,
        )
        sv.add_element(
            DefSwitch(name="toggle", label="Shutter (On=closed)", _value=constants.SwitchState.OFF)
        )
        self.add_property(sv, callback=self.handle_shutter)

        # usevortex (On = vortex in the beam; optical model uses M.use_vortex)
        sv = properties.SwitchVector(
            name="usevortex",
            rule=constants.SwitchRule.ANY_OF_MANY,
            perm=constants.PropertyPerm.READ_WRITE,
        )
        sv.add_element(
            DefSwitch(
                name="toggle",
                label="Vortex (On=in)",
                _value=(
                    constants.SwitchState.ON
                    if self._use_vortex
                    else constants.SwitchState.OFF
                ),
            )
        )
        self.add_property(sv, callback=self.handle_usevortex)

        # cam_name (retarget which camera INDI device to follow)
        tv = properties.TextVector(name="cam_name", perm=constants.PropertyPerm.READ_WRITE)
        tv.add_element(DefText(name="current", _value=self._cam_name))
        tv.add_element(DefText(name="target", _value=self._cam_name))
        self.add_property(tv, callback=self.handle_cam_name)

        # fsm_name (retarget which FSM INDI device provides x/y.current)
        tv = properties.TextVector(name="fsm_name", perm=constants.PropertyPerm.READ_WRITE)
        tv.add_element(DefText(name="current", _value=self._fsm_name))
        tv.add_element(DefText(name="target", _value=self._fsm_name))
        self.add_property(tv, callback=self.handle_fsm_name)

        # output shmim name (read-mostly; changing requires restart for size)
        tv = properties.TextVector(name="shm_output", perm=constants.PropertyPerm.READ_WRITE)
        tv.add_element(DefText(name="current", _value=self._shm_output))
        tv.add_element(DefText(name="target", _value=self._shm_output))
        self.add_property(tv, callback=self.handle_shm_output)

        # Mirrored camera params (read-only current from cam_name; no local target).
        # To change the camera, NEW to cam_name.<prop>.target (e.g. nsvsim.exptime.target).
        for name, label, fmt, val, vmin, vmax, vstep in (
            ("exptime", "Camera exposure [s] (from cam_name)", "%0.6f", self.config.default_exp_time, 0.0, 1e6, 1e-6),
            ("emgain", "Camera gain mapped (from cam_name)", "%0.1f", self.config.default_gain, 0.0, 1000.0, 1.0),
            ("blacklevel", "Camera blacklevel (from cam_name)", "%0.1f", self.config.default_blacklevel, -1e6, 1e6, 1.0),
            ("fps", "Camera fps / loop rate (from cam_name)", "%0.2f", self.config.default_fps, 0.0, 1e4, 0.01),
            ("bitDepth", "Camera bitDepth → rebin (from cam_name)", "%d", float(self.config.default_bit_depth), 8.0, 16.0, 1.0),
        ):
            nv = properties.NumberVector(name=name, perm=constants.PropertyPerm.READ_ONLY)
            nv.add_element(
                DefNumber(
                    name="current",
                    label=label,
                    format=fmt,
                    min=vmin,
                    max=vmax,
                    step=vstep,
                    _value=float(val),
                )
            )
            self.add_property(nv)

    def handle_streaming(self, existing_property, new_message):
        if "toggle" not in new_message:
            return
        on = new_message["toggle"] == constants.SwitchState.ON
        self._streaming = on
        existing_property["toggle"] = (
            constants.SwitchState.ON if on else constants.SwitchState.OFF
        )
        self.update_property(existing_property)
        if on:
            self._t0 = time.perf_counter()
            self._time_counter = 0.0
        self.log.info("streaming %s", "ON" if on else "OFF")

    def handle_shutter(self, existing_property, new_message):
        if "toggle" not in new_message:
            return
        closed = new_message["toggle"] == constants.SwitchState.ON
        self._shutter_closed = closed
        existing_property["toggle"] = (
            constants.SwitchState.ON if closed else constants.SwitchState.OFF
        )
        # Publish SET immediately so GUIs / darkCtrl see the toggle state.
        self.update_property(existing_property)
        if self._M is not None:
            self._M.camsci_shutter = closed
        self.log.info("shutter %s (toggle=%s)", "CLOSED" if closed else "OPEN",
                      "On" if closed else "Off")

    def handle_usevortex(self, existing_property, new_message):
        if "toggle" not in new_message:
            return
        on = new_message["toggle"] == constants.SwitchState.ON
        self._use_vortex = on
        existing_property["toggle"] = (
            constants.SwitchState.ON if on else constants.SwitchState.OFF
        )
        self.update_property(existing_property)
        if self._M is not None:
            self._M.use_vortex = bool(on)
        self.log.info(
            "usevortex %s (next snap %s vortex)",
            "ON" if on else "OFF",
            "with" if on else "without",
        )

    def handle_cam_name(self, existing_property, new_message):
        if "target" not in new_message:
            return
        name = str(new_message["target"]).strip()
        if not name:
            return
        self._cam_name = name
        existing_property["target"] = name
        existing_property["current"] = name
        self.update_property(existing_property)
        self._subscribe_camera()
        self.log.info("cam_name → %s", name)

    def handle_fsm_name(self, existing_property, new_message):
        if "target" not in new_message:
            return
        name = str(new_message["target"]).strip()
        if not name:
            return
        self._fsm_name = name
        existing_property["target"] = name
        existing_property["current"] = name
        self.update_property(existing_property)
        self._subscribe_fsm()
        self.log.info("fsm_name → %s", name)

    def handle_shm_output(self, existing_property, new_message):
        if "target" not in new_message:
            return
        name = str(new_message["target"]).strip()
        if not name:
            return
        existing_property["target"] = name
        existing_property["current"] = name
        self._shm_output = name
        self.update_property(existing_property)
        # Recreate / reopen output stream at current model size.
        n = int(self._M.ncamsci)
        create_if_not_exist_shmim(name, (n, n), dtype=np.float32)
        self._out = Image(name)
        self.log.info("shm_output → %s", name)

    # ----------------------------------------------------------- optical model
    def _init_optical_model(self):
        import esc_llowfsc_sim
        from esc_llowfsc_sim import detector, source_flux
        import esc_llowfsc_sim.esc_fraunhofer as esc
        from lina import utils
        from lina.math_module import xp

        self._xp = xp
        self._utils = utils
        self._ensure_np_array = __import__(
            "lina.math_module", fromlist=["ensure_np_array"]
        ).ensure_np_array

        model_path = Path(esc_llowfsc_sim.path) / "data" / self.config.model_pack
        toml_path = model_path / "model_params.toml"
        if not toml_path.is_file():
            raise FileNotFoundError(f"model_params.toml not found at {toml_path}")

        with open(toml_path, "r") as fp:
            all_params = tomlkit.load(fp).unwrap()
        model_params = dict(all_params.get("model", {}))
        wfe_params = dict(all_params.get("wfe", {}))

        model_params.update(
            {
                "wavelength_c": float(self.config.wavelength_c),
                "ncamsci": int(self.config.ncamsci),
            }
        )
        wfe_params.update(
            {
                "dm_astig_rms": float(self.config.dm_astig_rms),
                "compute_flat": bool(self.config.compute_flat),
                "plot_flat": bool(self.config.plot_flat),
            }
        )

        waves = np.linspace(
            model_params["wavelength_c"] * (1 - self.config.bw / 2),
            model_params["wavelength_c"] * (1 + self.config.bw / 2),
            int(self.config.nwaves),
        )
        mag0_source = source_flux.SOURCE(
            **source_flux.mag0_source_params,
            wavelengths=waves,
        )
        # Keep astropy Quantity units (ph / (s m^2)); esc.single expects that, not a bare float.
        entrance_flux = np.sum(mag0_source.calc_fluxes())
        self.log.info("entrance flux (Vega mag0 bandpass) = %s", entrance_flux)

        self._M = esc.single(**model_params, entrance_flux=entrance_flux)
        self._M.init_wfe(**wfe_params)
        self._PREFPM_OPD0 = copy.copy(self._M.PREFPM_OPD)

        cam = detector.IMX492(
            exp_time=float(self.config.default_exp_time),
            gain=_map_imx492_gain(self.config.default_gain),
            blacklevel=float(self.config.default_blacklevel),
            dark_current=float(self.config.dark_current),
            nbits=int(self.config.nbits),
            rebin=int(self.config.default_bit_depth),
            qe=float(self.config.qe),
        )
        self._M.CAMSCI = cam
        self._M.use_vortex = bool(self._use_vortex)
        self._M.NCAMSCI = 1
        self._M.NCAMLO = 1
        self._M.camsci_shutter = False

        self._wfe_modes = utils.create_zernike_modes(
            self._M.APERTURE, nmodes=10, remove_modes=1
        )
        self.log.info("optical model ready (%s)", model_path)

    # --------------------------------------------------------------- shmims
    def _try_open_shmim(self, name: str) -> Optional[Image]:
        try:
            return Image(name)
        except FileNotFoundError:
            return None
        except Exception:
            self.log.exception("failed to open shmim %s", name)
            return None

    def _plant_dm_flat(self) -> bool:
        """Deprecated: never write onto cacao. Load the flat from shm instead."""
        return self._load_dm_flat_from_shm()

    def _as_nact_command(self, arr, nact: int):
        a = np.asarray(arr, dtype=np.float64).squeeze()
        if a.size != nact * nact:
            return None
        if a.shape == (nact, nact):
            return np.ascontiguousarray(a)
        return np.ascontiguousarray(a.reshape((nact, nact)))

    def _load_dm_flat_from_shm(self) -> bool:
        """Read shm_dm_flat_channel (µm) into M.dm_flat (meters). Does not set_dm."""
        if self._dm_flat is None:
            self._dm_flat = self._try_open_shmim(self.config.shm_dm_flat_channel)
        if self._dm_flat is None:
            return False
        raw = self._grab_array(self._dm_flat)
        if raw is None:
            return False
        nact = int(self._M.Nact)
        cmd = self._as_nact_command(raw, nact)
        if cmd is None:
            self.log.warning(
                "shm_dm_flat_channel=%s shape %s != Nact=%d — not loading flat",
                self.config.shm_dm_flat_channel,
                np.asarray(raw).shape,
                nact,
            )
            return False
        flat_m = self._xp.array(cmd) * self.config.dm_scale
        self._M.dm_flat = flat_m
        self.log.info(
            "loaded DM flat from %s shape=%s dtype=%s (µm→m via dm_scale=%g, magpyx F.T layout)",
            self.config.shm_dm_flat_channel,
            cmd.shape,
            raw.dtype,
            self.config.dm_scale,
        )
        return True

    def _zero_other_dm_channels(self):
        """Zero dmXXXdisp01..11 if shm_dm_flat_channel looks like dmXXXdisp00."""
        name = self.config.shm_dm_flat_channel
        if not name.endswith("00"):
            self.log.warning(
                "zero_other_dm_channels: cannot infer siblings from %s", name
            )
            return
        prefix = name[:-2]
        for i in range(1, 12):
            ch = f"{prefix}{i:02d}"
            img = self._try_open_shmim(ch)
            if img is None:
                continue
            try:
                ny, nx = int(img.md.size[0]), int(img.md.size[1])
                img.write(np.zeros((ny, nx), dtype=np.float32))
                self.log.info("zeroed DM channel %s", ch)
            except Exception:
                self.log.exception("failed to zero DM channel %s", ch)

    def _ensure_input_shmims(self):
        """Open DM / vmag / opd shmims if they appeared after setup."""
        if self._dm is None:
            self._dm = self._try_open_shmim(self.config.shm_dm_total)
            if self._dm is not None:
                self.log.info("opened DM total shmim %s", self.config.shm_dm_total)
                self._logged_dm_missing = False
        if self._dm_flat is None:
            self._dm_flat = self._try_open_shmim(self.config.shm_dm_flat_channel)
            if self._dm_flat is not None:
                self.log.info("opened DM flat shmim %s", self.config.shm_dm_flat_channel)
        if self._vmag is None:
            self._vmag = self._try_open_shmim(self.config.shm_vmag)
        if self._opd is None:
            self._opd = self._try_open_shmim(self.config.shm_opdsim)

    def _init_shmims(self):
        n = int(self._M.ncamsci)
        create_if_not_exist_shmim(self._shm_output, (n, n), dtype=np.float32)
        self._out = Image(self._shm_output)

        self._dm = self._try_open_shmim(self.config.shm_dm_total)
        self._dm_flat = self._try_open_shmim(self.config.shm_dm_flat_channel)
        self._vmag = self._try_open_shmim(self.config.shm_vmag)
        self._opd = self._try_open_shmim(self.config.shm_opdsim)

        for name, img in (
            (self.config.shm_dm_total, self._dm),
            (self.config.shm_dm_flat_channel, self._dm_flat),
            (self.config.shm_vmag, self._vmag),
            (self.config.shm_opdsim, self._opd),
        ):
            if img is None:
                self.log.warning("input shmim %s not found at setup — will retry each frame", name)
            else:
                self.log.info("opened input shmim %s", name)

        if not self._load_dm_flat_from_shm():
            self.log.warning(
                "could not load DM flat from %s — using optical-model compute_flat",
                self.config.shm_dm_flat_channel,
            )

        if self.config.plant_dm_flat:
            self.log.warning(
                "plant_dm_flat is ignored; llowfscSim reads %s and never writes the model flat onto cacao",
                self.config.shm_dm_flat_channel,
            )

        if self.config.zero_other_dm_channels:
            self._zero_other_dm_channels()

    # -------------------------------------------------------- camera INDI
    def _subscribe_camera(self):
        try:
            # Non-blocking: camera may start after us; loop() re-reads as props arrive.
            self.client.get_properties(self._cam_name)
            self.log.info("requested properties for camera INDI device %s", self._cam_name)
        except Exception:
            self.log.exception("get_properties(%s) failed", self._cam_name)

    def _subscribe_fsm(self):
        try:
            self.client.get_properties(self._fsm_name)
            self.log.info("requested properties for FSM INDI device %s", self._fsm_name)
        except Exception:
            self.log.exception("get_properties(%s) failed", self._fsm_name)

    def _read_fsm_current_nm(self):
        """Pull fsm_name.x/y.current [nm]; keep last values if unavailable."""
        try:
            dev = self.client[self._fsm_name]
        except Exception:
            return self._fsm_x_nm, self._fsm_y_nm

        try:
            if "x" in dev:
                self._fsm_x_nm = _safe_float(dev["x"]["current"], self._fsm_x_nm)
        except Exception:
            pass
        try:
            if "y" in dev:
                self._fsm_y_nm = _safe_float(dev["y"]["current"], self._fsm_y_nm)
        except Exception:
            pass
        return self._fsm_x_nm, self._fsm_y_nm

    def _read_camera_params(self):
        """Pull live camera params from INDI; keep last-known values if unavailable."""
        exp = self._cam_exp
        gain = self._cam_gain
        bl = self._cam_blacklevel
        fps = self._fps
        bitdepth = self._cam_bitdepth
        roi_w = self._cam_roi_w
        roi_h = self._cam_roi_h

        try:
            dev = self.client[self._cam_name]
        except Exception:
            return exp, gain, bl, fps, bitdepth, roi_w, roi_h

        try:
            if "exptime" in dev:
                exp = _safe_float(dev["exptime"]["current"], exp)
        except Exception:
            pass
        try:
            if "emgain" in dev:
                gain = _safe_float(dev["emgain"]["current"], gain)
            elif "gain" in dev:
                gain = _safe_float(dev["gain"]["current"], gain)
        except Exception:
            pass
        try:
            if "blacklevel" in dev:
                bl = _safe_float(dev["blacklevel"]["current"], bl)
        except Exception:
            pass
        try:
            if "fps" in dev:
                fps = _safe_float(dev["fps"]["current"], fps)
        except Exception:
            pass
        try:
            if "bitDepth" in dev:
                bitdepth = _safe_int(dev["bitDepth"]["current"], bitdepth)
        except Exception:
            pass
        try:
            if "roi_region_w" in dev:
                roi_w = max(1, _safe_int(dev["roi_region_w"]["current"], roi_w))
            if "roi_region_h" in dev:
                roi_h = max(1, _safe_int(dev["roi_region_h"]["current"], roi_h))
        except Exception:
            pass

        if fps <= 0:
            fps = float(self.config.default_fps)

        self._cam_exp = exp
        self._cam_gain = gain
        self._cam_blacklevel = bl
        self._fps = fps
        self._cam_bitdepth = bitdepth
        self._cam_roi_w = roi_w
        self._cam_roi_h = roi_h
        return exp, gain, bl, fps, bitdepth, roi_w, roi_h

    def _update_status_indi(self, exp, gain, bl, fps, bitdepth):
        """Mirror cam_name params onto our RO INDI props (current only)."""
        key = (float(exp), float(gain), float(bl), float(fps), float(bitdepth))
        if self._last_status == key:
            return
        try:
            updates = {
                "exptime": float(exp),
                "emgain": float(gain),
                "blacklevel": float(bl),
                "fps": float(fps),
                "bitDepth": float(bitdepth),
            }
            for prop_name, value in updates.items():
                prop = self.properties[prop_name]
                prop["current"] = value
                self.update_property(prop)
            self._last_status = key
            self.log.debug(
                "mirrored cam params → INDI exptime=%s emgain=%s fps=%s",
                exp,
                gain,
                fps,
            )
        except Exception:
            self.log.exception("status INDI update failed")

    # -------------------------------------------------------------- frame
    def _copy_shmim(self, img: Image):
        """Non-blocking snapshot matching magpyx.utils.ImageStream.grab_latest().

        magpyx (run_camsci_cpp.ipynb) keeps a Fortran view of the milk buffer
        and transposes it::

            self.buffer = np.array(self, copy=False, order='F').T
            grab_latest() -> np.array(self.buffer, copy=True)

        The optical model is fed that C-order array. MagAO-X ``Image.copy()``
        is the same ``super().copy().T``. Do not skip the transpose.
        """
        try:
            view = np.array(img, copy=False, order="F")
            return np.squeeze(np.array(view.T, copy=True))
        except Exception:
            return np.squeeze(np.asarray(ImageStreamIOWrap.Image.copy(img)).T)

    def _grab_scalar(self, img: Optional[Image], default: float = 0.0) -> float:
        if img is None:
            return default
        try:
            return float(self._copy_shmim(img).ravel()[0])
        except Exception:
            if not getattr(self, "_logged_grab_fail", False):
                self.log.exception("failed to read scalar shmim")
                self._logged_grab_fail = True
            return default

    def _grab_array(self, img: Optional[Image]):
        if img is None:
            return None
        try:
            return self._copy_shmim(img)
        except Exception:
            if not getattr(self, "_logged_grab_fail", False):
                self.log.exception("failed to copy shmim array")
                self._logged_grab_fail = True
            return None

    def _apply_dm_total(self, M, xp):
        """Match run_camsci_cpp.ipynb: cacao total → model channel 0.

        Notebook::

            total_dm_command = DMT_STREAM.grab_latest()  # magpyx F-order .T
            M.set_dm(xp.array(total_dm_command)/1e6, channel=0)

        ``dm01disp00`` is still *read* into ``M.dm_flat`` (never written). The
        live command is the cacao combination on channel 0 only, so iefc/AO
        pokes are included without splitting onto channel 1.
        """
        nact = int(M.Nact)
        scale = float(self.config.dm_scale)

        tot_raw = self._grab_array(self._dm)
        tot_cmd = self._as_nact_command(tot_raw, nact) if tot_raw is not None else None
        if tot_cmd is None:
            if not self._logged_dm_missing:
                self.log.warning(
                    "no data from shm_dm_total=%s — optical model using M.dm_flat only",
                    self.config.shm_dm_total,
                )
                self._logged_dm_missing = True
            if M.dm_flat is not None:
                M.set_dm(M.dm_flat, channel=0)
            return

        M.set_dm(xp.array(tot_cmd) * scale, channel=0)
        try:
            M.zero_dm(channel=1)
        except Exception:
            pass
        if self._logged_dm_missing:
            self.log.info(
                "applying shm_dm_total=%s on model channel 0 (magpyx F.T layout, /1e6)",
                self.config.shm_dm_total,
            )
            self._logged_dm_missing = False

    def _rt_snap(self, exp, gain, bl, bitdepth, roi_w, roi_h):
        M = self._M
        xp = self._xp
        self._ensure_input_shmims()

        vmag = self._grab_scalar(self._vmag, 0.0)
        M.flux_scale_factor = 2.512 ** (-vmag)

        M.use_vortex = bool(self._use_vortex)

        opd = self._grab_array(self._opd)
        if opd is not None:
            coeff = np.asarray(opd).ravel()[:10]
            if coeff.size == 10:
                wfe_opd = xp.sum(
                    xp.array(coeff)[:, None, None] * self._wfe_modes, axis=0
                )
                M.set_prefpm_wfe(self._PREFPM_OPD0 + wfe_opd)

        fsm_x_nm, fsm_y_nm = self._read_fsm_current_nm()
        # Optical model set_fsm(..., RMS=True) expects meters RMS.
        M.set_fsm(xp.array([fsm_x_nm * 1e-9, fsm_y_nm * 1e-9]), RMS=1)

        self._apply_dm_total(M, xp)

        # Apply camera detector params
        mapped_gain = _map_imx492_gain(gain)
        M.CAMSCI.exp_time = float(exp)
        try:
            M.CAMSCI.gain = mapped_gain
        except ValueError:
            M.CAMSCI.gain = 120
            mapped_gain = 120
        M.CAMSCI.blacklevel = float(bl)
        M.CAMSCI.nbits = int(self.config.nbits)
        M.CAMSCI.rebin = int(bitdepth)
        M.CAMSCI.adu_max = 2 ** M.CAMSCI.nbits - 1
        M.camsci_shutter = bool(self._shutter_closed)

        frame = M.snap_camsci()
        frame_np = self._ensure_np_array(frame).astype(np.float32, copy=False)

        # Optional center-crop to camera ROI (when smaller than optical array).
        h, w = frame_np.shape[-2], frame_np.shape[-1]
        if roi_h < h or roi_w < w:
            y0 = max(0, (h - roi_h) // 2)
            x0 = max(0, (w - roi_w) // 2)
            frame_np = frame_np[y0 : y0 + roi_h, x0 : x0 + roi_w]

        if self._streaming:
            # Recreate output if ROI size changed.
            out_shape = frame_np.shape
            if (
                self._out.md.size[0] != out_shape[0]
                or self._out.md.size[1] != out_shape[1]
            ):
                create_if_not_exist_shmim(self._shm_output, out_shape, dtype=np.float32)
                self._out = Image(self._shm_output)
            self._out.write(frame_np)

        return mapped_gain

    def loop(self):
        # Always poll cam_name / publish mirrors — do not wait on optical snaps.
        exp, gain, bl, fps, bitdepth, roi_w, roi_h = self._read_camera_params()
        mapped_gain = _map_imx492_gain(gain)
        self._update_status_indi(exp, mapped_gain, bl, fps, bitdepth)

        if self._streaming:
            try:
                self._rt_snap(exp, gain, bl, bitdepth, roi_w, roi_h)
            except Exception:
                self.log.exception("rt_snap failed; turning streaming OFF")
                self._streaming = False
                try:
                    prop = self.properties["streaming"]
                    prop["toggle"] = constants.SwitchState.OFF
                    self.update_property(prop)
                except Exception:
                    pass
                time.sleep(0.05)
                return
            # Hold cadence to camera fps while publishing.
            period = 1.0 / max(fps, 1e-6)
            self._time_counter += period
            now = time.perf_counter()
            target = self._t0 + self._time_counter
            remain = target - now
            if remain > 0:
                time.sleep(remain)
            elif remain < -1.0:
                # Severely behind — reset timeline so we don't busy-spin catch-up.
                self._t0 = time.perf_counter()
                self._time_counter = 0.0
        else:
            # Idle poll: stay responsive to shutter / camera INDI (~20 Hz).
            time.sleep(0.05)


main = LlowfscSim.console_app
