"""Optical-model bridge for llowfscSimCpp.

The C++ MagAO-X app owns INDI, ImageStreamIO, and loop cadence. This module
creates the esc_llowfsc_sim model once and applies live DM/FSM/camera state
before each M.snap_camsci() / M.snap_camlo(). It does not rebuild the Fraunhofer
model on every frame — that is the slow path we are avoiding.
"""

from __future__ import annotations

import copy
import os
from pathlib import Path

import numpy as np
import tomlkit

_M = None
_xp = None
_ensure_np_array = None
_PREFPM_OPD0 = None
_wfe_modes = None
_nbits = 14
_last_fsm = None
_last_dm = None  # ndarray, _DM_FLAT, or None
_last_opd = None
_DM_FLAT = object()
_base_flux_ph = None
_lyot_c = None
_stream_lres = None
_stream_hres = None
_host_f32 = None
_cupy = None
_props = None
_utils = None


def _map_imx492_gain(raw: float) -> int:
    return 0 if abs(float(raw) - 0.0) < abs(float(raw) - 120.0) else 120


def create(cfg: dict) -> dict:
    """Build esc.single + IMX492 detector. cfg keys match llowfscSim config."""
    global _M, _xp, _ensure_np_array, _PREFPM_OPD0, _wfe_modes, _nbits
    global _last_fsm, _last_dm, _last_opd
    global _base_flux_ph, _lyot_c, _stream_lres, _stream_hres, _cupy, _props, _utils
    global _host_f32
    _last_fsm = None
    _last_dm = None
    _last_opd = None
    _host_f32 = None

    os.environ["CUDA_VISIBLE_DEVICES"] = str(cfg.get("cuda_device", "0"))

    import esc_llowfsc_sim
    from esc_llowfsc_sim import detector, source_flux
    import esc_llowfsc_sim.esc_fraunhofer as esc
    from lina import utils
    from lina.math_module import xp, ensure_np_array
    from lina import props as lina_props
    import astropy.units as u

    _xp = xp
    _ensure_np_array = ensure_np_array
    _utils = utils
    _props = lina_props
    _nbits = int(cfg.get("nbits", 14))

    model_path = Path(esc_llowfsc_sim.path) / "data" / str(cfg.get("model_pack", "1k-256"))
    toml_path = model_path / "model_params.toml"
    if not toml_path.is_file():
        raise FileNotFoundError(f"model_params.toml not found at {toml_path}")

    with open(toml_path, "r") as fp:
        all_params = tomlkit.load(fp).unwrap()
    model_params = dict(all_params.get("model", {}))
    wfe_params = dict(all_params.get("wfe", {}))

    wavelength_c = float(cfg.get("wavelength_c", 633e-9))
    ncamsci = int(cfg.get("ncamsci", 512))
    model_params.update({"wavelength_c": wavelength_c, "ncamsci": ncamsci})
    wfe_params.update(
        {
            "dm_astig_rms": float(cfg.get("dm_astig_rms", 0.0)),
            "compute_flat": bool(cfg.get("compute_flat", True)),
            "plot_flat": bool(cfg.get("plot_flat", False)),
        }
    )

    nwaves = int(cfg.get("nwaves", 51))
    bw = float(cfg.get("bw", 0.02))
    waves = np.linspace(wavelength_c * (1 - bw / 2), wavelength_c * (1 + bw / 2), nwaves)
    mag0_source = source_flux.SOURCE(**source_flux.mag0_source_params, wavelengths=waves)
    entrance_flux = np.sum(mag0_source.calc_fluxes())

    _M = esc.single(**model_params, entrance_flux=entrance_flux)
    _M.init_wfe(**wfe_params)
    _PREFPM_OPD0 = copy.copy(_M.PREFPM_OPD)

    cam = detector.IMX492(
        exp_time=float(cfg.get("default_exp_time", 0.01)),
        gain=_map_imx492_gain(float(cfg.get("default_gain", 120.0))),
        blacklevel=float(cfg.get("default_blacklevel", 10.0)),
        dark_current=float(cfg.get("dark_current", 0.025)),
        nbits=_nbits,
        rebin=int(cfg.get("default_bit_depth", 16)),
        qe=float(cfg.get("qe", 0.75)),
    )
    _M.CAMSCI = cam
    _M.use_vortex = bool(cfg.get("usevortex", True))
    _M.NCAMSCI = 1
    _M.NCAMLO = 1
    _M.camsci_shutter = False

    _wfe_modes = utils.create_zernike_modes(_M.APERTURE, nmodes=10, remove_modes=1)

    area = (_M.total_pupil_diam * u.m / _M.npix) ** 2
    _base_flux_ph = (_M.entrance_flux * area).to_value(u.photon / u.second)
    _lyot_c = _M.LYOTSTOP.astype(complex)

    _cupy = None
    _stream_lres = None
    _stream_hres = None
    xp_mod = getattr(xp, "_srcmodule", None)
    try:
        if xp_mod is not None and hasattr(xp_mod, "cuda"):
            _cupy = xp_mod
            _stream_lres = xp_mod.cuda.Stream(non_blocking=True)
            _stream_hres = xp_mod.cuda.Stream(non_blocking=True)
    except Exception:
        _cupy = None
        _stream_lres = None
        _stream_hres = None

    import sys

    xp_name = getattr(xp_mod, "__name__", None) if xp_mod is not None else None
    if not xp_name:
        xp_name = type(xp).__name__
    fast = []
    if _cupy is not None:
        fast.append("cuda_streams")
    fast.append("flux_cache")

    return {
        "ncamsci": int(_M.ncamsci),
        "nact": int(_M.Nact),
        "model_path": str(model_path),
        "entrance_flux": str(entrance_flux),
        "xp": str(xp_name),
        "esc": str(getattr(esc_llowfsc_sim, "__file__", "")),
        "sys_prefix": str(sys.prefix),
        "fast": ",".join(fast),
    }


def nact() -> int:
    return 0 if _M is None else int(_M.Nact)


def ncamsci() -> int:
    return 0 if _M is None else int(_M.ncamsci)


def set_dm_flat(flat_m: np.ndarray) -> None:
    """Load DM flat in meters (already scaled). Does not set_dm."""
    global _last_dm
    if _M is None:
        raise RuntimeError("optical model not created")
    _M.dm_flat = _xp.array(np.ascontiguousarray(flat_m, dtype=np.float64))
    _last_dm = None


def _calc_wfs_camsci_fast(M):
    """Same math as esc.single.calc_wfs_camsci with cached flux and overlapped vortex.

Keep in sync with esc_fraunhofer.single.calc_wfs_camsci. Low-res FFT and high-res
MFT are independent; two CUDA streams hide some of that latency.
"""
    flux_ph = float(_base_flux_ph) * float(M.flux_scale_factor)
    APERTURE_FLUX = M.APERTURE * np.sqrt(flux_ph)
    if M.use_keystones:
        APERTURE_FLUX *= M.KEY_APERTURE
    if M.use_mb:
        APERTURE_FLUX *= M.MB_APERTURE

    E_EP = APERTURE_FLUX * M.PREFPM_WFE
    E_DM = E_EP * M.M4_PHASOR * M.FSM_PHASOR * M.DM_PHASOR * M.CHROMATIC_WEDGE_PHASOR

    if M.use_vortex:
        if _cupy is not None and _stream_lres is not None:
            # E_DM is produced on the current (usually default) stream.
            # The vortex FFT/MFT use non-blocking streams that do *not* wait
            # for that work. Without this event they can read a half-written
            # E_DM and publish a saturated "flash" frame.
            e_dm_ready = _cupy.cuda.get_current_stream().record()
            with _stream_lres:
                _stream_lres.wait_event(e_dm_ready)
                E_DM_lres = _utils.pad_or_crop(E_DM, M.N_vortex_lres)
                E_FPM_lres = _props.fft(E_DM_lres)
                E_FPM_lres *= M.windowed_vortex_lres
                E_LP_lres = _props.ifft(E_FPM_lres)
                E_LP_lres = _utils.pad_or_crop(E_LP_lres, M.Ndef)
            with _stream_hres:
                _stream_hres.wait_event(e_dm_ready)
                E_FPM_hres = M.Mx_vortex @ E_DM @ M.My_vortex * M.vortex_mft_norm
                E_FPM_hres *= M.windowed_vortex_hres
                E_LP_hres = (
                    M.Mx_vortex_back @ E_FPM_hres @ M.My_vortex_back * M.vortex_mft_norm_back
                )
            _stream_lres.synchronize()
            _stream_hres.synchronize()
            E_LP = E_LP_lres + E_LP_hres
        else:
            E_DM_lres = _utils.pad_or_crop(E_DM, M.N_vortex_lres)
            E_FPM_lres = _props.fft(E_DM_lres)
            E_FPM_lres *= M.windowed_vortex_lres
            E_LP_lres = _props.ifft(E_FPM_lres)
            E_LP_lres = _utils.pad_or_crop(E_LP_lres, M.Ndef)
            E_FPM_hres = M.Mx_vortex @ E_DM @ M.My_vortex * M.vortex_mft_norm
            E_FPM_hres *= M.windowed_vortex_hres
            E_LP_hres = M.Mx_vortex_back @ E_FPM_hres @ M.My_vortex_back * M.vortex_mft_norm_back
            E_LP = E_LP_lres + E_LP_hres
    else:
        E_LP = E_DM

    E_LP = E_LP * M.POSTFPM_WFE
    E_LS = E_LP * _lyot_c
    if M.use_weak_lens:
        E_CAMSCI = _utils.pad_or_crop(E_LS, M.ncamsci)
    else:
        E_CAMSCI = M.camsci_mft_norm * M.Mx_camsci @ E_LS @ M.My_camsci
        E_CAMSCI *= np.sqrt(M.camsci_throughput) / _xp.sqrt(M.Imax_ref)
    return E_CAMSCI


def _to_host_f32(frame, roi_w: int, roi_h: int) -> np.ndarray:
    """GPU float32 (+ optional center crop) then one DtoH into a reused host buffer."""
    global _host_f32
    xp = _xp
    if hasattr(frame, "dtype") and frame.dtype != np.float32 and frame.dtype != xp.float32:
        frame = frame.astype(xp.float32)
    h, w = int(frame.shape[-2]), int(frame.shape[-1])
    rw = int(roi_w) if int(roi_w) > 0 else w
    rh = int(roi_h) if int(roi_h) > 0 else h
    if rh < h or rw < w:
        y0 = max(0, (h - rh) // 2)
        x0 = max(0, (w - rw) // 2)
        frame = frame[y0 : y0 + rh, x0 : x0 + rw]
        h, w = rh, rw
    if _cupy is not None and hasattr(frame, "get"):
        g = _cupy.ascontiguousarray(frame)
        if _host_f32 is None or _host_f32.shape != (h, w):
            _host_f32 = np.empty((h, w), dtype=np.float32)
        g.get(out=_host_f32)
        return _host_f32
    out = np.ascontiguousarray(_ensure_np_array(frame), dtype=np.float32)
    return out


def snap(
    dm=None,
    fsm_x_nm: float = 0.0,
    fsm_y_nm: float = 0.0,
    exp: float = 0.01,
    gain: float = 120.0,
    blacklevel: float = 0.0,
    bitdepth: int = 16,
    shutter_closed: bool = False,
    use_vortex: bool = True,
    vmag: float = 0.0,
    roi_w: int = 0,
    roi_h: int = 0,
    which: str = "camsci",
    opd=None,
) -> np.ndarray:
    """Apply live plant state and snap. `dm` is nact×nact meters (or None → dm_flat).

Does not rebuild esc.single. Camera/FSM/DM/shutter/vortex/OPD updates are
applied to the existing model so the next snap recomputes the image.
"""
    global _last_fsm, _last_dm, _last_opd
    if _M is None:
        raise RuntimeError("optical model not created")

    M = _M
    M.flux_scale_factor = 2.512 ** (-float(vmag))
    M.use_vortex = bool(use_vortex)
    M.camsci_shutter = bool(shutter_closed)

    if opd is not None:
        coeff = np.asarray(opd, dtype=np.float64).ravel()[:10]
        if coeff.size == 10:
            if _last_opd is None or not np.array_equal(coeff, _last_opd):
                wfe_opd = _xp.sum(_xp.array(coeff)[:, None, None] * _wfe_modes, axis=0)
                M.set_prefpm_wfe(_PREFPM_OPD0 + wfe_opd)
                _last_opd = coeff.copy()

    fsm_key = (float(fsm_x_nm), float(fsm_y_nm))
    if fsm_key != _last_fsm:
        M.set_fsm(_xp.array([fsm_key[0] * 1e-9, fsm_key[1] * 1e-9]), RMS=1)
        _last_fsm = fsm_key

    if dm is None:
        if M.dm_flat is not None and _last_dm is not _DM_FLAT:
            M.set_dm(M.dm_flat, channel=0)
            _last_dm = _DM_FLAT
    else:
        cmd = np.ascontiguousarray(np.asarray(dm, dtype=np.float64))
        if (
            not isinstance(_last_dm, np.ndarray)
            or cmd.shape != _last_dm.shape
            or not np.array_equal(cmd, _last_dm)
        ):
            M.set_dm(_xp.array(cmd), channel=0)
            try:
                M.zero_dm(channel=1)
            except Exception:
                pass
            _last_dm = cmd.copy()

    mapped_gain = _map_imx492_gain(gain)
    M.CAMSCI.exp_time = float(exp)
    try:
        M.CAMSCI.gain = mapped_gain
    except ValueError:
        M.CAMSCI.gain = 120
    M.CAMSCI.blacklevel = float(blacklevel)
    M.CAMSCI.nbits = int(_nbits)
    M.CAMSCI.rebin = int(bitdepth)
    M.CAMSCI.adu_max = 2 ** M.CAMSCI.nbits - 1

    if which == "camlo":
        frame = M.snap_camlo()
    else:
        E = _calc_wfs_camsci_fast(M)
        flux = _xp.abs(E) ** 2 * (M.camsci_shutter < 0.5)
        n = int(getattr(M, "NCAMSCI", 1) or 1)
        if M.CAMSCI is not None:
            frame = (
                _xp.mean(M.CAMSCI.add_noise(flux, n), axis=0)
                if n > 1
                else M.CAMSCI.add_noise(flux, n)
            )
        else:
            frame = flux

    return _to_host_f32(frame, roi_w, roi_h)
