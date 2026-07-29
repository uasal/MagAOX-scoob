# iefcCtrl

MagAO-X INDI front-end for Stream IEFC (ref PSF, dark library, calibrate, run).

## Build

```bash
cd /opt/MagAOX/source/MagAOX   # or this checkout
make -C apps/iefcCtrl
sudo make -C apps/iefcCtrl install
```

Registered on RTC (`apps_rtc`) and `all_buildable_apps`.

## Config

See `iefcCtrl.conf.sample`. Important:

- `iefc.camsciOnceBin` — absolute path to `iefc_camsci_once` for calibrate/run
- paths: `outdir`, `setupdir`, `caldir`

## INDI properties (shared)

| Property | Role |
|----------|------|
| `nFrames` | Frames averaged per camsci grab (refPSF / darks / calibrate / run) |
| `waitFrames` | New camsci frames to skip after DM write before averaging |
| `delay_s` | Optional wall-clock settle after DM write |
| `exptime` | If set, written to `camsciexptime` for calibrate/run |
| `outdir` / `setupdir` / `caldir` | Paths |

## Requests

| Switch | Action |
|--------|--------|
| `doRefPsf` | Park FSM at `fsmRefTip/Tilt_nm`, poke tip/tilt, dark + PSF → `outdir` |
| `doDarkLibrary` | Dark library at `exptimes` CSV → `outdir/darks/` |
| `doCalibrate` | Spawn `iefc_camsci_once calibrate …` |
| `doRun` | Spawn `iefc_camsci_once run …` |
| `stop` | Request stop of in-progress native job |

Ref-PSF / dark-library are native milk SHM. Calibrate / run reuse the existing lina binary until the matrix path is inlined.
