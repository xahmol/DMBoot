# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

DMBoot 128: a boot menu / file browser for the Commodore 128, written in C for **CC65** (`cl65`, target `c128`). It runs as `autostart.128.prg` from the `/usb*/11/` directory of an **Ultimate II+** cartridge and is started by Bart van Leeuwen's **C128 Device Manager ROM**. Both are hard runtime requirements: the program exits if no Ultimate Command Interface is detected, and it calls the DM ROM's extended API (`src/dmapiasm.s`, jump table at `$807B`–`$808F`).

The file browser is derived from DraBrowse (Sascha Bader / doj, `github.com/doj/dracopy`); `ultimate_*_lib.c` come from xlar54's ultimateii-dos-lib. Keep the attribution headers intact.

## Build and deploy

```
make            # builds autostart.128.prg, overlay .prg files, dmb-confupd-3-4.prg, and a timestamped release ZIP
make clean
make deploy     # wput the deploy files over FTP to the Ultimate II+ (ULTHOST in the Makefile, currently 192.168.1.19/usb1/11/)
```

- Requires `cl65` on PATH, plus `zip` and `wput`.
- `make` (default `all`) always creates a new `DMBoot-v391-<date>.zip` in the repo root, and `README.pdf` is packed into it. Historical release ZIPs are committed on purpose because the README links to them. Don't delete them, and don't commit a new ZIP unless a release is intended.
- The version string lives in `ZIP` in the Makefile and in `VERSION_MAJOR/MINOR` in `include/version.h`.
- `c128-ram.o` (the CC65 bank-1 extended-memory driver, from `c128-ram.s`) is prebuilt and linked directly. It is not rebuilt by the Makefile.
- `$(TIME)`/`$(CFG)` targets in the Makefile are leftovers from older multi-program builds and expand to nothing.
- There are no tests. You can only verify changes on real hardware (C128 + U2+ + DM ROM) or in an emulator setup that provides both. The `.vscode/launch.json` VICE config is stale (Windows paths).

## Architecture: overlays

Memory is the main constraint. The README notes that the program is close to full memory. `dmboot-cc65.cfg` is a custom linker config that splits the program into a resident main program plus overlay files, all emitted by the single link of `autostart.128.prg`:

| Segment    | Output file     | Load address       | Contents |
|------------|-----------------|--------------------|----------|
| MAIN       | autostart.128.prg | `$1C01`          | `main.c`, screen/base/ops, VDC, Ultimate libs, DM API asm |
| OVERLAY1   | dmb-fb.prg      | `$C000-OVERLAYSIZE` | file browser (`dir.c`, `db.c`, part of `ops.c`) |
| OVERLAY2   | dmb-menu.prg    | shared overlay area | boot menu (`bootmenu.c`) |
| OVERLAY3   | dmb-util.prg    | shared overlay area | NTP time + config UI (`u-time.c`, `dmbconfig.c`, `configcommon.c`) |
| OVERLAY6   | dmb-exec.prg    | shared overlay area | mounting images / loading REU / launching (`exec.c`) |
| OVERLAY4   | dmb-lowc.prg    | `$1300` (0x900)    | `geosramboot.c` low-memory code |
| OVERLAY5   | dmb-geos.prg    | `$0B00` (0x100)    | `geosramroutine.s` |

- You put code into an overlay with `#pragma code-name ("OVERLAYn")` / `rodata-name` at the top of the source file (or push/pop for part of a file, as in `ops.c`).
- OVERLAY1/2/3/6 share one region of `__OVERLAYSIZE__` bytes (`$1C80`) ending at `$C000`. Only one is resident at a time. `main()` in `src/main.c` calls `loadoverlay("11:dmb-xxx")` before entering each feature and reloads `dmb-menu` afterwards. Code in one of these overlays must never call into another one. Shared helpers belong in MAIN.
- If an overlay grows past `$1C80`, raise `__OVERLAYSIZE__`, but that shrinks MAIN (`$A3F3 - OVERLAYSIZE - STACKSIZE`). Check the `.map` output after changes.
- Adding a new overlay means adding a MEMORY/SEGMENT pair in the cfg (OVL7–9 are spare placeholders), adding the file to `DEPLOYS` in the Makefile, and loading it from `main.c`.

## Data storage

- **Menu slots**: 36 slots (keys 0-9, a-z) of `struct SlotStruct` (`include/defines.h`). They live in C128 **bank 1** and are accessed through the CC65 `em_*` API (`c128_ram` driver) with `getslotfromem`/`putslottoem` in `bootmenu.c`. `std_read`/`std_write` in `main.c` load and save them as a raw binary blob: KERNAL `SETBNK` to bank 1, `$0400`–`$0400+72*256`, file `dmbootconf`, partition 11 of the boot device.
- `Slot.cfgvs` / `CFGVERSION` versions the slot format. If you change `SlotStruct`, bump `CFGVERSION` and write a new `dmb-confupd-X-Y.c` migration tool (see `dmb-confupd-3-4.c`, set as `SOURCESUPD` in the Makefile).
- **Util config** (NTP / GEOS RAM boot settings): file `dmbcfgfile`, read and written by `configcommon.c`.
- **Directory entries** in the file browser are stored in free VDC RAM rather than main memory (`vdc.c` / `vdc_assembly.s`). The browser detects 16 KB vs 64 KB VDC at startup (`VDC_DetectVDCMemSize`), and this sets the maximum number of entries.
- Hard-coded paths assume the program lives in `/usb*/11/` and that partition 11 is the boot partition (`cp11`). Overlay file names are prefixed `11:`.

## Unused files

The Makefile does not build `src/*_old.c`, `include/ops_old.h`, `src/test.c`, or `dmb-confupd-1-2.c`/`2-3.c`. Many `.prg`, `.map`, `.seq`, and `.reu` files in the repo root are committed build artifacts or test data. No `.gitignore` exists, so be careful about accidentally committing `.o`/`.d` files from a build.
