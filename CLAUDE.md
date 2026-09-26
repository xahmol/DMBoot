# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

DMBoot 128 v5: boot menu / file browser for the Commodore 128, being rebuilt from scratch in **Oscar64** (C, target `c128e`) on branch `Oscar64Rebuild`. It runs as `autostart.128.prg` from `/usb*/11/` on an **Ultimate II+** and is autostarted by Bart van Leeuwen's **C128 Device Manager ROM**. It is a normal PRG plus overlay files, not a cartridge. The previous cc65 version (called v4, builds named `v391-*`) is preserved on branch `legacy-cc65`.

**Read `docs/REBUILD_PLAN.md` first.** It holds the full architecture, the memory model, all design decisions, the phase plan, and the hardware test rules. Phase status (2026-09-25): Phases 0-2 hardware-verified (80 columns); Phases 3-6 (slot editor, file browser, configuration/NTP/GEOS, upgrader dmbupd45) implemented but not yet hardware-tested; Phase 7 (docs, release) open. See the plan's phase status section.

Sibling/reference projects (all by the same author): UBoot64-v2 (`/home/xahmol/git/UBoot64-v2`, C64 cartridge version of the same boot menu: **prefer its routines over v4 legacy code**), VDC Screen Editor 2 (`/home/xahmol/VDCScreenEditor2`, overlay/banking/VDC library pattern), vdcmaniac (`/home/xahmol/git/vdcmaniac`).

This is an Oscar64 project: use `oscar64manual.md` as the compiler reference (see the global instructions for keeping it updated). The 40/80 column screen layer is the project's own **DualWin** library (`include/dualwin.c/h`, manual `DUALWINMANUAL.md`): all UI output goes through `dwin_*`, never `printf` after start-up. Other references in the repo root: `UCILIBMANUAL.md` (Ultimate Command Interface library), `vdclib_manual.md` (VDC library suite).

## Build, deploy, test

```
make build        # release build -> build/autostart.128.prg, dmblmc.prg, dmbovl*.prg
make test-build   # same file names, with -dTESTMODE (test mailbox at $0B00, stays at 1 MHz)
make deploy       # FTP to ftp://$(ULTIP1)/Usb1/11/ (ULTIP1 in gitignored .env; test machine 192.168.1.237)
make deploy2      # same to the second machine: ULTIP2/ULTUSB2 in .env (192.168.1.23, stick /USB1/, used for 40-column tests)
make test         # host tests (gcc): timeconv, v4convert, dirparse (see tests/host/README.md)
make docs / zip / clean
```

- Oscar64 at `/home/xahmol/oscar64/bin/oscar64`. `MAIN_SRCS` in the Makefile must list every `.c`/`.h` reached through `#pragma compile`, or make won't rebuild.
- `make deploy` **overwrites `autostart.128.prg`** in the Device Manager boot directory. The v4 original is backed up on the stick as `autostart.128.v4.prg` and locally in `tests/data/`.
- There is no emulator path (VICE has no UCI). Testing is on the real C128 through c64bridge (`u2` backend). **Rules** (plan §7.1):
  - Only read or write C128 memory over REST while the C128 runs at **1 MHz**. DMA at 2 MHz crashes it.
  - Only access memory while mailbox `idle == 1`. After a test key that runs at 2 MHz, wait instead of polling.
  - Inject keys via `$034A` (buffer) + `$D0` (count), not c64bridge's C64 key helpers.
  - Ask the user before any reset, memory write or drive command.

## Architecture (see plan §3-§5 for detail)

- **Memory:**
  - Resident program `$1C80`–`$97FF` (region `dmboot`).
  - Overlay load slot `$9800`–`$BFFF` (`OVERLAYSIZE $2800`).
  - Low-memory code (LMC) at `$1300`–`$1AFF` in 8 KB common RAM (`xmmu.rcr = 0x06`, restored by `bnk_exit()`).
- **Overlays (VDCSE pattern):**
  - Each overlay source starts with `#pragma overlay(dmbovlN, N+1)` + section/region pragmas.
  - All overlay files are loaded once at startup, then copied to stores in bank 1 (`$4000`+) or in bank 0 under ROM (`$C000`). `loadoverlay(n)` copies the image into the slot, with no disk access.
  - Overlays must never call each other. Functions called from resident code are `__noinline`.
- **LMC:** the `bnk_*` banked access routines and the Device Manager ROM API (`dmapi.c`). The API runs with `$FF00 = $2A`, where only RAM below `$8000` is visible, so those routines must not touch memory at `$8000` or above.
- **REU:** required (at least 128 KB). All DMA goes through `reu128_load`/`reu128_store`, which drop to 1 MHz. Size detection uses the probe barrier.
- **UCI library** in `include/ultimate_*`, taken from UBoot64-v2.
- **VDC library suite** copy in `include/vdc_core.*`/`vdc_win.*` (from VDC Screen Editor 2). Keep it byte-identical to the canonical VDCSE files; fix bugs in both (see DUALWINMANUAL.md §9).

## Code conventions (mandatory)

- Every function has a comment block above it: **Title, Description, Syntax, Input, Output**.
- **Overflow-safe strings:** `strncpy(dst, src, sizeof(dst) - 1); dst[sizeof(dst) - 1] = 0;` Sizes come from `sizeof` or named constants. Validate every external input (UCI, files, IEC, keyboard) for length and range.
- Structs for grouped state and layouts. Named constants, no magic numbers.
- `petscii.h` in every file that prints; string literals are PETSCII. Use proper capitalisation in UI text. Wire-protocol strings (UCI/DOS) may need an identity charmap override (UBoot64 ARCHITECTURE.md §12.10).
- Debug/test hooks that compile to nothing in release must still evaluate their arguments (`((void)(x))`).
- **Credits:**
  - Third parties only (Oscar64, ultimateii-dos-lib, DraBrowse/doj, Device Manager ROM/GEOS routine by Bart van Leeuwen).
  - Never credit the author's own work. Refer to his GitHub projects as his where relevant.
- Apply the Oscar64 quirk workarounds from UBoot64-v2 `ARCHITECTURE.md` §12.
