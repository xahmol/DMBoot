# Host tests

Tests for DMBoot's hardware-independent C modules, built with gcc on the PC
so they behave as under Oscar64 on the C128 (32-bit `long` via `host.h`, unsigned `char`,
packed structures via `-fpack-struct=1`, stub `petscii.h` and `c64/vic.h`).

Run with `make test` (or `python3 tests/host/run_tests.py`).

| Test | Module | Checked against |
|---|---|---|
| `test_timeconv.c` | `src/timeconv.c` (NTP epoch to Ultimate RTC time) | Python `datetime`, fixed cases (leap days, year ends, UTC offsets ±14 h) and 3000 random times up to 2105 |
| `test_dirparse.c` | `src/dirparse.c` (file browser: directory lines, image names, dirtrace) | Hand-made lines in 1541/SoftIEC format (header, all types, locked, splat, shifted spaces, 60-character names, blocks free), image extensions in both cases, dirtrace with a 32-byte buffer for the bounds |
| `test_v4convert.c` | `src/v4convert.c` (upgrade tool dmbupd45) | `tests/tools/convert_v4_slots.py` byte for byte, on both real v4 slot files in `tests/data`; converted settings printed |

Only modules without hardware access, overlays or charmap-dependent
literals can be tested this way. Keep new pure logic in such modules.
