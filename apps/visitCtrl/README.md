# visitCtrl

Reads a WCC visit file and republishes it as INDI properties.

This is the **only** application that parses a visit file. `telescopeSim` reads
where to point from it, and `wccCtrl` reads the acquisition and tracking parameters
from it, both over INDI. One parser means one interpretation of the schema, and it
means an operator can inspect exactly what the system believes the visit says
instead of inferring it from behaviour.

## Interface

Two controls:

```bash
xindi visitctrl.visit_file.target=/opt/MagAOX/config/wcc_visit.json
xindi visitctrl.load.toggle=On
```

Toggling `load` off clears everything, so a stale selection can never be mistaken
for a current one. Everything else is read-only.

| property | type | elements |
|---|---|---|
| `visit_file` | text | `current`, `target` — the path to read |
| `load` | switch | `toggle` — read it, or clear |
| `select_rank` | number | `current`, `target` — which ranked pair to publish |
| `visit_status` | text | `state` (`EMPTY`/`LOADED`/`ERROR`), `message`, `path` |
| `target` | number | `ra`, `dec`, `rollpa` |
| `target_info` | text | `name`, `progid`, `obsid`, `visitid` |
| `guide_star` | number | `rank`, `ra`, `dec`, `field_x`, `field_y`, `mag`, `target_x`, `target_y`, `exptime`, `frame_rate`, `roi_w`, `roi_h` |
| `guide_star_info` | text | `id`, `sensor`, `catalog` |
| `roll_star` | number | same elements as `guide_star` |
| `roll_star_info` | text | `id`, `sensor`, `catalog` |
| `acq_params` | number | `ta_exptime`, `ta_frame_rate`, `guide_tol_px`, `roll_tol_px`, `max_iterations`, `n_sensors` |
| `track_params` | number | `roi_w`, `roi_h`, `frame_rate`, `exptime`, `loop_gain`, `roll_gain` |
| `track_devices` | text | `centroid_guide`, `centroid_roll` |
| `config_sensors` | text | `list` — comma separated sensor names |

The names are constants in `../wccCommon/wccVisitIndi.hpp`, shared by the publisher
and every consumer, so a rename cannot desynchronize them — the compiler flags it
in all three applications at once.

## Rank selection

A visit file offers several ranked guide and roll star candidates. `visitCtrl` owns
that choice: `select_rank` picks which pair is published, and `0` means the highest
ranked pair available. A roll star of the same rank is preferred, falling back to
any roll star, because the file does not always carry a matching rank for both.

Changing `select_rank` republishes from the already-parsed file without re-reading
it, so trying a different pair is immediate.

This matters when the array is only partly populated: if `wccCtrl` reports that a
star is on an unconfigured sensor, move `select_rank` to a pair that fits the
sensors you actually have.

## Notes

- The file is read in `appLogic`, not in the INDI callback, so a large or
  slow-to-read file cannot stall the INDI driver thread.
- All INDI traffic is held to 1 Hz. `appLogic` runs at that rate and publishing is
  driven from it, so no additional throttling is needed here.
- A failed load leaves `visit_status.state` at `ERROR` with the reason in `message`
  and publishes nothing, rather than leaving a half-populated selection behind.
- Per-sensor overrides from the visit file's `TA_SENSORS` block are **not**
  republished; the acquisition parameters apply to every participating sensor. That
  is the simplification moving the parser out bought. If per-sensor acquisition
  configuration is needed later, `TA_SENSORS` is still parsed by
  `wccCommon/wccVisit.hpp` and only needs new properties to carry it.

## Tests

```bash
cd tests
make -B -f Makefile.one t=../apps/visitCtrl/tests/visitCtrl_test.cpp
../apps/visitCtrl/tests/visitCtrl_test
```

Covers a file becoming a published selection, rank selection picking the pair asked
for, the comma separated sensor list round trip, and the failure modes: missing
file, empty path, malformed JSON, and a rank the visit does not have.
