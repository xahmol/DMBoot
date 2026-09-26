# DMBoot 128 v5: architecture

How the rebuilt DMBoot is put together, as implemented. The reasons behind
the design and the phase history are in [REBUILD_PLAN.md](REBUILD_PLAN.md);
this document describes the result. Numbers are from the build of
2026-09-25 (`build/dmboot.map`); check the map after changes.

## 1. Programs and files

| File on `/usb*/11/` | What it is | Built from |
|---|---|---|
| `autostart.128.prg` | DMBoot, started by the Device Manager ROM | `src/main.c` (Oscar64 `c128e`) |
| `dmblmc.prg` | Low-memory code (LMC), loaded to `$1300` | `#pragma overlay(dmblmc, 1)` in `include/banking.c` |
| `dmbovl1.prg` .. `dmbovl6.prg` | Overlays, loaded once at start-up into their stores | `#pragma overlay(dmbovlN, N+1)` in the overlay sources |
| `dmbupd45.prg` | Upgrade tool v4 -> v5 (standalone) | `src/dmbupd45.c` |
| `dmbconf.cfg` | Configuration (`struct ConfigStruct`, 1203 bytes; new fields are appended, older shorter files still read) | written by DMBoot / dmbupd45 |
| `dmbslots.cfg` | 36 slots (`struct SlotStruct`, 1360 bytes each) | written by DMBoot / dmbupd45 |

## 2. Modules

### Resident (always in memory, `$1C80`-)

| Module | Job |
|---|---|
| `src/main.c` | Start-up, overlay table and loading, main loop |
| `src/core.c` | Error exit, delay, header line, DOS commands, slot keys, IEC bus scan, drive root reset |
| `src/basicexit.c` | Clean return to BASIC 7: zero page save/restore, function key expansion off/on |
| `src/fileio.c` | Config and slot files (UCI, chunked) <-> REU |
| `src/cfgdefaults.c` | Default configuration (shared with dmbupd45) |
| `src/dmpaths.c` | Storage path search (`/usb*/11/` etc.) and file names (identity charmap) |
| `src/petconv.c` | ASCII <-> PETSCII (Ultimate file system <-> screen) |
| `src/slotlist.c` | Slot list drawing and slot picking, shared by overlays 1, 2 and 3 |
| `src/dirparse.c` | Directory line parsing, image names, dirtrace path (browser, host-tested) |
| `src/timeconv.c` | UNIX time to Ultimate RTC time (NTP, host-tested) |
| `include/dualwin.c` | 40/80 column window library (see `DUALWINMANUAL.md`) |
| `include/vdc_core.c`, `vdc_win.c` | VDC library (copy of VDC Screen Editor 2's) |
| `include/reu128.c` | REU DMA at 1 MHz, size detection |
| `include/ultimate_*.c` | Ultimate Command Interface library (see `UCILIBMANUAL.md`) |
| `include/testmode.c` | Test mailbox at `$0B00` (test builds only) |

### Low-memory code (`$1300`-`$1AFF`, common RAM)

Code that must keep running while the MMU maps something else in:
`bnk_memcpy` and the other `bnk_*` routines (banked copies), the Device
Manager API calls and the C64-mode `SYS` entry `dm_run64` (`include/dmapi.c`,
function ROM at `$8000`), and the GEOS RAM boot `geos_boot`
(`include/geosboot.c`, needs 16 KB common RAM). About 0.4 KB of 2 KB used.

### Overlays (one at a time in the load slot `$9800`-`$BFFF`)

| # | File | Source | Contents | Size |
|---|---|---|---|---|
| 1 | `dmbovl1` | `src/slotmenu.c` | Main menu, auto-boot countdown | 1.2 KB |
| 2 | `dmbovl2` | `src/slotedit.c` | Slot editor (F3) | 4.2 KB |
| 3 | `dmbovl3` | `src/browse.c` | File browser (F1), slot picking | 8.0 KB |
| 4 | `dmbovl4` | `src/config.c` | NTP update, configuration (F4), information (F2) | 5.8 KB |
| 5 | `dmbovl5` | `src/exec.c` | Slot start, go 64, exit, GEOS RAM boot (F6) | 4.5 KB |
| 6 | `dmbovl6` | `src/splash.c` | Splash screen (F2), packed data from `assets/splash.petmate` | 2.6 KB |

Sizes include the overlay's own variables. The slot is 10 KB
(`OVERLAYSIZE $2800`).

Rules:
- Overlays never call each other. Shared code is resident (`slotlist.c`,
  `dirparse.c`, ...). Hand-overs between overlays go through resident data,
  e.g. `browsereq` (browser -> slot start).
- Functions called from resident code are `__noinline`.
- `loadoverlay(n)` copies the image from its store; there is no disk
  access after start-up.

The splash screen is designed in Petmate9 (`assets/splash.petmate`, 40 and
80 column screens). `tools/petmate2c.py` packs it (PackBits) into
`src/splashdata.c`; `make` regenerates that file when the Petmate9 file
changes. `tools/splashgen.py` created the first design.

## 3. Memory map

### Bank 0

| Range | Use |
|---|---|
| `$0B00`-`$0BFF` | Test mailbox (test builds) |
| `$1300`-`$1AFF` | LMC |
| `$1C01`-`$1C7F` | BASIC stub and Oscar64 start-up |
| `$1C80`-`$6885` | Resident code and data (~19 KB) |
| `$6886`-`$78BF` | Resident variables (~4 KB) |
| `$78C0`-`$87FF` | Heap (~3.9 KB; used by the UCI library's `malloc`) |
| `$8800`-`$97FF` | Stacks |
| `$9800`-`$BFFF` | Overlay load slot |
| `$C000`-`$E7FF` | Store of overlay 5 (RAM under the KERNAL ROM) |

Common RAM is set to 8 KB at the bottom (`$D506 = $06`) while DMBoot runs
and back to 1 KB on exit.

### Bank 1

| Range | Use |
|---|---|
| `$0000`-`$1FFF` | Not usable: covered by common RAM |
| `$2000`-`$3FFF` | DualWin popup store |
| `$4000`-`$67FF` | Store of overlay 1 |
| `$6800`-`$8FFF` | Store of overlay 2 |
| `$9000`-`$B7FF` | Store of overlay 3 |
| `$B800`-`$DFFF` | Store of overlay 4 |
| `$E000`-`$FEFF` | Store of overlay 6 (small store: at most `OVERLAY_SMALL_SIZE` = `$1F00` bytes, enforced by its region) |

### REU (at least 128 KB, required)

| Address | Use |
|---|---|
| `$00000`-`$0BF3F` | 36 slots (`SLOT_REU_START`) |
| `$10000`- | Directory listing of the browser (linked list, up to the top of the REU); slot backup while re-ordering in the editor (never at the same time) |

A slot's REU image load overwrites all of this. That is why it is the last
step of a slot start, and nothing returns to the menu afterwards.

## 4. Flow of a slot start

`runbootfrommenu` (overlay 5):
1. Switch on Ultimate drive A/B when needed (wait 2 s), mount the images
   (other USB ports are tried when an image is not found).
2. Load the REU image (last).
3. Drive root reset, then the slot's path command (`cd:/...` on the
   hyperspeed drive is relative to the current directory).
4. `execute`: demo mode, Force 8 (Device Manager API), then the start
   line: the slot command, then `RUN"file",U<id>`, `BOOT U<id>`,
   `LOAD"file",<id>,1` or `SYS <dm_run64>` for C64 mode, all joined with
   `:` into one logical screen line (at most 160 characters). After a
   `LOAD ...,1`, `RUN` + RETURN are typed from the keyboard buffer.
5. `exec_to_basic`: prints the start line, puts one RETURN (plus any extra
   keys) in the keyboard buffer (`$034A`, count `$D0`), restores the screen
   (`CINT`), the MMU and BASIC's zero page, and returns to BASIC, which runs
   the line. One line instead of v4's one line per statement: in 40 columns
   BASIC prints a command's output on a new line (in 80 columns on the same
   line), so READY. overwrote the next statement line.

## 5. Conventions

- Every function has a comment block: Title, Description, Syntax, Input,
  Output.
- Strings: `strncpy` + terminator, sizes from `sizeof` or named constants,
  every external input checked for length and range.
- `petscii.h` everywhere that prints. Strings sent to the Ultimate or the
  drives, or compared with their data, use an identity charmap
  (`dmpaths.c`), raw byte arrays, or numeric PETSCII constants
  (`PET_LC()` in `dirparse.c`).
- Credits: third parties only; the author's own projects are referred to
  by their GitHub links.
- Oscar64 pitfalls met in this project are in `oscar64manual.md` (C128
  gotchas): among others zero page and BASIC 7, function key expansion,
  stores in `__asm` not seen by the optimiser, code used only through its
  address.

## 6. Testing

- **On the PC:** `make test` builds the hardware-independent modules with
  gcc as under Oscar64 and checks them (`tests/host/README.md`):
  `timeconv.c`, `v4convert.c` (byte-identical with the Python reference
  `tests/tools/convert_v4_slots.py` on real v4 files), `dirparse.c`.
- **On the C128:** there is no emulator path (VICE has no Ultimate Command
  Interface). `make test-build` adds the test mailbox at `$0B00` for
  c64bridge. Rules: REST memory access only at 1 MHz, keys through the
  keyboard buffer, and ask before resets or writes (plan §7.1, CLAUDE.md).
