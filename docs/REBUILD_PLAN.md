# DMBoot 128 v5: design and decisions

Branch: `main` · Version 5.0.0 (alpha 1 published)

DMBoot v5 is a rebuild from scratch in Oscar64 for the C128. It carries the
functionality and conventions of the sibling project **UBoot64-v2**
(<https://github.com/xahmol/UBoot64-v2>) and the C128 techniques of **VDC
Screen Editor 2** (overlays, banking, VDC library) and **vdcmaniac** (build
chain, low-memory code). This document holds the decisions and the reasons
behind them; [ARCHITECTURE.md](ARCHITECTURE.md) describes the result module
by module.

---

## 1. Decisions

| Topic | Decision |
|---|---|
| Compiler | Oscar64, **official release tag** (currently `v1.32.273`), target `-tm=c128e`. Not `main`: `main` at `546b627` broke the start-up console scrolling on hardware. A newer release is adopted only after a hardware test. |
| Start mechanism | A normal PRG `autostart.128.prg` in the `11` directory of the USB stick, autostarted by the Device Manager (DM) ROM. Not a cartridge (the big difference from UBoot64). |
| Display | Both 40 columns (VIC) and 80 columns (VDC). The screen active at start-up is used; F8 in the main menu switches. |
| REU | **Required** (≥ 128 KB), as in UBoot64. Slots and directory listings live in the REU. |
| Slots | 36 (keys `0`-`9`, `a`-`z`). 80 columns: two columns of 18. 40 columns: two pages of 18. |
| File location | The DM ROM's `11` directory: `/usb*/11/` first, then `/usb0/11/`, `/usb1/11/`, `/usb2/11/` (several sticks; the U2+ has no SD card). |
| Config files | `dmbconf.cfg` + `dmbslots.cfg`: new names, so the v4 files stay intact next to them. |
| Migration | Standalone upgrade tool `dmbupd45.prg` (v4 -> v5). |
| Version | v5.0.0, build string `v5.0.0-YYYYMMDD-HHMM`. "v4" means the published `v391-*` cc65 builds. |
| Legacy code | The complete cc65 tree and the historic release ZIPs are on branch `legacy-cc65`. |
| File browser | **IEC only**, as in v4 (no UCI browse mode: the DM ROM's SoftIEC hyperspeed drive is always there). |
| Main menu keys | v4 layout kept (F1 browser, F2 information, F3 edit, F4 configuration, F5 go 64, F6 GEOS, F7 exit); F8 added for the 40/80 switch. |
| Firmware 3.15 | SoftIEC partitions supported (§9). They wait for a DM ROM that runs on 3.15; the DM-specific partition use is kept in a few named places. |
| Credits | Third parties only. Code from the author's own projects (UBoot64-v2, VDC Screen Editor 2, vdcmaniac) gets no attribution comment. |

---

## 2. Shared with UBoot64, and different

| Aspect | UBoot64-v2 (C64) | DMBoot v5 (C128) |
|---|---|---|
| Container | FC3 16 KB × 4-bank cartridge | PRG plus overlay PRGs in the `11` directory |
| Code swapping | `fc3_call(bank, fn)` | Oscar64 overlays: loaded once at start-up into stores in bank 1 and bank 0, copied into one load slot on demand (§4) |
| Exit to BASIC | `fc3_exit()` + keyboard buffer | Keyboard buffer (`$034A`, count `$D0`) after restoring screen, MMU and BASIC's zero page |
| Screen | VIC, Oscar64 `CharWin` | VIC and VDC behind one library, DualWin (§6) |
| Extra platform | - | DM ROM API (hyperspeed ID, drive types, run in C64 mode), GEOS 128 RAM boot, FAST mode, `GO 64`, 64 KB VDC |
| Browser | UCI and IEC mode | IEC mode only; host paths from the drive on firmware 3.15+ (§9) |
| UCI library | `include/ultimate_*` | The same library (malloc-free, with the SoftIEC target, see `UCILIBMANUAL.md`) |

---

## 3. Memory model

### 3.1 MMU and CPU while DMBoot runs

- **Common RAM: 8 KB at the bottom** (`$D506 = $06`, `bnk_init()`), so the
  low-memory code at `$1300` is visible whichever bank is mapped in.
  `bnk_exit()` restores 1 KB before every exit.
- **CPU speed:** 2 MHz in 80 columns (release build), with the VIC display
  blanked (`$D011` bit 4, as BASIC's `FAST`), because the VIC gets no memory
  cycles at 2 MHz. 1 MHz in 40 columns and on every exit. Test builds stay
  at 1 MHz (§7.1).
- **REU DMA always at 1 MHz** through `reu128_load`/`reu128_store`
  (`__noinline`, §11.3). DMA targets bank 0.
- **BASIC 7 zero page:** Oscar64's zero page (`$02-$26`, `$43-$62`,
  `$F7-$FF`) overlaps BASIC's work storage. It is saved at the start of
  `main()` and restored on exit (`basicexit.c`).
- **Function keys:** the screen editor's function key expansion is switched
  off while DMBoot runs (key store vector `$033C`) and restored on exit.

### 3.2 Bank 0

| Address | Use |
|---|---|
| `$0000`-`$03FF` | Zero page, stack, KERNAL/BASIC work area (keyboard buffer `$034A`, count `$D0`) |
| `$0400`-`$07E7` | VIC screen |
| `$0B00`-`$0BFF` | Test mailbox (test builds only) |
| `$0C00`-`$12FF` | KERNAL/BASIC: not touched |
| `$1300`-`$1AFF` | Low-memory code `dmblmc` (common RAM) |
| `$1C01`-`$1C7F` | BASIC stub and Oscar64 start-up |
| `$1C80`-`$97FF` | Resident program: code, data, variables, heap (unused), stacks |
| `$9800`-`$BFFF` | Overlay load slot (`OVERLAYSIZE = $2800`) |
| `$C000`-`$E7FF` | Store of overlay 5 (RAM under the KERNAL ROM) |
| `$E800`-`$FEFF` | Free |

### 3.3 Bank 1 (reached through `bnk_*` only)

| Address | Use |
|---|---|
| `$0000`-`$1FFF` | Covered by the common RAM |
| `$2000`-`$3FFF` | DualWin popup background store |
| `$4000`-`$67FF` / `$6800`-`$8FFF` / `$9000`-`$B7FF` / `$B800`-`$DFFF` | Stores of overlays 1 / 2 / 3 / 4 |
| `$E000`-`$FEFF` | Store of overlay 6 (small store, at most `$1F00` bytes) |

After exit, BASIC takes bank 1 back; nothing of DMBoot is needed there then.

### 3.4 VDC RAM

| VDC address | Use |
|---|---|
| `$0000`-`$07CF` | Text screen 80×25 |
| `$0800`-`$0FCF` | Attributes |
| `$1000`-`$1FFF` | Swap/scratch buffer of the VDC library |
| `$2000`-`$3FFF` | Character sets (upper and lower case) |
| `$4000`-`$FFFF` | 64 KB VDC only: not used |

A 64 KB VDC is detected at start-up and switched to 64 KB addressing
(register 28 bit 4, as v4 did): left in 16 KB mode, a 64 KB VDC showed a
corrupted screen. On exit to BASIC it goes back to 16 KB addressing, the
power-on state that the KERNAL and programs such as CP/M expect. The GEOS
RAM boot keeps 64 KB mode (MegaPatch needs it).

### 3.5 REU

| REU address | Use |
|---|---|
| `$00000`-`$0BF3F` | 36 slots (36 × 1360 bytes) |
| `$10000`- top | Directory listing of the browser (linked list); slot backup while re-ordering in the editor (never both at once) |

The REU is used from address 0 and overwritten at start-up. A slot that
needs particular REU contents loads its own REU image when it starts.
Rules: a slot start mounts A, then B, then loads the REU image **last**;
after that nothing returns to the menu (errors exit to BASIC), because the
slots in the REU are gone. The GEOS RAM boot follows the same rule.

---

## 4. Overlays

| # | File | Store | Contents |
|---|---|---|---|
| LMC | `dmblmc` -> `$1300` | stays resident | `bnk_*` banked copies, DM ROM API calls, `dm_run64` (C64 mode), GEOS RAM boot |
| 1 | `dmbovl1` | bank 1 `$4000` | Main menu, auto-boot countdown |
| 2 | `dmbovl2` | bank 1 `$6800` | Slot editor |
| 3 | `dmbovl3` | bank 1 `$9000` | File browser, partitions |
| 4 | `dmbovl4` | bank 1 `$B800` | Configuration, NTP time sync, information |
| 5 | `dmbovl5` | bank 0 `$C000` | Slot start, go 64, exit, GEOS RAM boot |
| 6 | `dmbovl6` | bank 1 `$E000` | Splash screen (small store) |

- Start-up loads every overlay file once from partition 11 of the boot
  drive (`DM_PARTITION_PREFIX "11:"`) into the load slot and copies it to
  its store. `loadoverlay(n)` copies the image back (copy size per overlay);
  no disk access after start-up (v4 went back to disk on every switch).
- Each overlay source starts with `#pragma overlay(dmbovlN, N+1)`, its own
  sections and a region over the load slot (overlay 6: only `$1F00` bytes,
  so the linker refuses a larger one).
- Overlays never call each other; shared code is resident. Functions called
  from resident code are `__noinline`.
- `OVERLAYSIZE` stays `$2800`: the largest overlay (browser) uses 9 KB.

---

## 5. Modules

See [ARCHITECTURE.md](ARCHITECTURE.md) §2 for the complete list. In short:
resident `main`, `core`, `basicexit`, `fileio`, `cfgdefaults`, `dmpaths`,
`petconv`, `slotlist`, `dirparse`, `timeconv`, the DualWin and VDC libraries,
`reu128`, the UCI library and `testmode`; overlays `slotmenu`, `slotedit`,
`browse`, `config`, `exec`, `splash`; LMC `banking`, `dmapi`, `geosboot`;
the upgrade tool `dmbupd45` with `v4convert`.

---

## 6. 40/80 columns: DualWin

A reusable library (`include/dualwin.c/h`, manual `DUALWINMANUAL.md`) over
Oscar64's VIC `CharWin` and the VDC window layer of the VDC library suite.
All screens are written once against `dwin_*`:
- Mode detection from `$D7` bit 7; a state-only VDC set-up (no register
  writes except the memory size), `dwin_exit` = back to 16 KB VDC
  addressing + KERNAL `CINT`, keeping the screen the user switched to.
- Windows, clipped output, console output with scrolling, popups with
  background save in bank 1, key input through KERNAL `GETIN`, text input.
- One logical colour palette (C64/VIC colour numbers) in the configuration,
  mapped to VDC colours by an overridable table.
- `dwin_swap_screen` (KERNAL `SWAPPER`, Oscar64 zero page `$F7-$F9` kept
  around it) and `dwin_vic_charset` (VIC upper/lower case set through the
  KERNAL, whose interrupt rewrites the register from a shadow copy).
- Screens are drawn in full once and then updated incrementally: after a
  key press only the changed parts are redrawn.

Layouts: main menu 2 × 18 slots (80) or 2 pages of 18 (40, cursor
left/right); browser two columns of 19 entries (80) or one (40).

---

## 7. UCI library, build and testing

**UCI library:** taken from UBoot64-v2 and completed against firmware 3.15a
(every command usable on an Ultimate II+, the SoftIEC target in
`ultimate_softiec_lib`; the HTTP target is left out, DMBoot only uses the
network for NTP). No dynamic allocation: one shared command buffer
(`uii_command_buffer`, 520 bytes), status `99` for a command that does not
fit. `UCILIBMANUAL.md` has the full reference.

**Build** (`Makefile`): `make build` (release), `make test-build` (same file
names, `-dTESTMODE`), `make test` (host tests), `make deploy` / `deploy2`
(FTP to the test machines, addresses in the gitignored `.env`), `make docs`,
`make zip`. `MAIN_SRCS` lists every file reached through `#pragma compile`.
`src/splashdata.c` is generated from `assets/splash.petmate` by
`tools/petmate2c.py`.

### 7.1 Hardware testing

VICE has no Ultimate Command Interface, so testing is on the real C128 with
Ultimate II+ and DM ROM, through `make deploy` and c64bridge (`u2` backend:
memory read/write over REST, drive and mount status, FTP, reset).

Test machines: `.237` (C128D, 8563 with 16 KB VDC RAM) and `.23` (flat C128,
8563 with 64 KB VDC RAM, used for 40 columns).

Test builds (`make test-build`) add a mailbox at `$0B00` (`struct
TestMailbox`: magic `DMB5`, `idle`, heartbeat, screen, active overlay,
40/80, speed, boot device, REU size, DM API, last key and message).

Rules:
- **REST memory access is DMA: only while the C128 runs at 1 MHz.** DMA at
  2 MHz crashes the C128 (the same reason REU DMA needs 1 MHz). Test builds
  therefore stay at 1 MHz.
- **Only access memory while the mailbox reports `idle == 1`**, never during
  boot, loading or drive activity; watch progress with drive status and FTP.
- **A REST read sees the MMU configuration of that moment:** above `$3FFF`
  it can return BASIC/KERNAL ROM. Trust reads below `$4000` (screen,
  mailbox, common RAM), or read twice and compare.
- **Keys** go into the KERNAL keyboard buffer (`$034A`, count `$D0`), not
  through c64bridge's C64 key helpers.
- **Ask the user before any reset, memory write or drive command.**
- The VDC screen cannot be read over REST; 40-column screens can (`$0400`).

---

## 8. Data formats (CFGVERSION `0x05`)

**`struct SlotStruct`** (1360 bytes, `dmbslots.cfg` holds 36):

| Field | Type | Note |
|---|---|---|
| `cfgvs` | char | `0x05` |
| `path` | char[256] | DOS command to the program's directory (`cd:/...` on SoftIEC, `cd//...` on other drives) |
| `menu` | char[31] | Name in the menu |
| `file` | char[51] | Program file; empty for BOOT, mount-only and command-only slots |
| `cmd` | char[81] | User command, run before the program on the same BASIC line |
| `reu_image` / `reu_path` / `reusize` | char[51] / char[256] / char | REU image (Ultimate path) and size index 0-7 |
| `runboot` | char | `EXEC_MOUNT 0x01`, `EXEC_FRC8 0x02`, `EXEC_RUN64 0x04`, `EXEC_FAST 0x08`, `EXEC_BOOT 0x10` (v4 values), `EXEC_COMMA1 0x20`, `EXEC_DEMO 0x40` |
| `device` | char | IEC device of the program |
| `command` | char | `COMMAND_CMD 0x01`, `COMMAND_REU 0x02`, `COMMAND_IMGA 0x04`, `COMMAND_IMGB 0x08` |
| `image_a_path` / `image_a_file` / `image_a_id` | char[256] / char[51] / char | Drive A mount (Ultimate path) |
| `image_b_path` / `image_b_file` / `image_b_id` | char[256] / char[51] / char | Drive B mount |
| `isdefault` | char | 1 = auto-boot default slot |
| `partition` | char | Partition to select before the path, 0 = none (§9) |
| `padding` | char[11] | Zero |

**`struct ConfigStruct`** (1203 bytes, `dmbconf.cfg`):
`version`, `timeon`, `host[81]`, `secondsfromutc` (long), `verbose`
(silent / messages / messages + wait), `colors` (11 logical colours),
`timeoutidx` (auto-boot 0/1/3/5/10 s), `iec_root_partition`, `geos`
(GEOS RAM boot: REU image, path and size, drive A and B images and IDs),
`reserved[16]`, `host2[81]`, `host3[81]`. New fields are appended; a shorter
older file is read with the new fields zero.

---

## 9. Firmware 3.15: partitions

The DM ROM does not run on firmware 3.15 yet; DMBoot's partition support is
in place for when it does, and is inactive on older firmware.

- **Device Manager partition use** is kept in named places, because it
  changes with a DM ROM for 3.15 (`cp11` selects the firmware's partition 11
  there, which is what breaks): `DM_PARTITION_PREFIX "11:"` (overlay
  loading, `defines.h`), `drive_root_reset()` (`cp11`, `cd`, `cp0`, `cd`)
  and `drive_select_dmboot()` in `core.c`, and the storage paths in
  `dmpaths.c`. Nothing else uses the DM numbering.
- **Partition list (browser F4):** CMD style `"$=P"` (upper case P, an
  identity-charmap string), header line skipped, entry size = partition
  number. Works for the 3.15 SoftIEC drive, CMD HD and SD2IEC. A drive
  without partitions returns its files for `"$=P"` (the 3.14 SoftIEC reads
  it as a filter): an entry with a file type means "no partition list";
  partition 0 is skipped. RETURN sends `cp<n>`; DEL at the partition root
  shows the list again.
- **Host paths:** `SOFTIEC_CMD_GET_FATNAME` (`$05 $22 <channel> "$"`,
  firmware 3.15+, added together with the partitions) returns the host path
  of the SoftIEC drive's current directory in any partition. It is the only
  source of the path: 3.15a lists partition *names*, and `G-P` only returns
  the name. Older firmware answers "unknown command".
- **Slots on the SoftIEC drive with firmware 3.15+:** partition 254
  (DMBoot's own, at `/`, created with `uii_add_partition`; not kept in flash,
  so created again when needed) plus `cd:` + the host path. Independent of
  the user's partitions and without a dirtrace path. Mounts and REU images
  get the host path too. The check (host path available, 254 free or
  DMBoot's, create accepted) runs once per device in the browser. Fallback
  (older firmware, other drives, 254 in use by the user): the browsed
  partition plus the dirtrace path.
- **Configuration F8 "Root partition":** the browser starts in partition 254.
- **Slot start:** root reset, then (partition 254: create again) `cp<n>`,
  then the path.
- Partitions cannot be deleted through UCI (the firmware does not remove
  them); DMBoot does not try.

---

## 10. Upgrade tool `dmbupd45.prg` (v4 -> v5)

- Reads `dmbootconf.prg` (36 × 512 bytes: each slot is two 256-byte pages,
  page 1 `path` to `cfgvs`, page 2 the image fields) and `DMBCFGFILE`
  (328 bytes) through UCI from the DMBoot directory.
- Converts each slot: `runboot` kept; the `cd:` prefix stripped from mount
  and REU paths only where present, which become ASCII Ultimate paths;
  `path` keeps it (IEC command); image flags cleared when no image file is
  stored; names 20 -> 30 characters; empty slots get the format version;
  padding zero; `partition` and `isdefault` 0. Same rules as the reference
  converter `tests/tools/convert_v4_slots.py` (host-tested, byte-identical
  on real v4 files and on the hardware run).
- Configuration: NTP on/off and UTC offset kept; a v4 NTP server chosen by
  the user becomes server 1; GEOS settings kept (an image without a path
  gets the DMBoot directory); default colours and start-up mode.
- Asks before overwriting existing v5 files; the v4 files stay as a backup.
- DMBoot v5 with only the v4 files asks whether to start with empty slots.
  N selects the DMBoot directory on the boot drive (partition 0 + absolute
  `cd:`, because BASIC 7 puts `0:` before every file name, also before
  `11:name`) and exits to BASIC with `run"dmbupd45",u<id>` under the cursor.

---

## 11. Features

### 11.1 Kept from DMBoot v4

36 slots; the v4 main menu keys; DM API (hyperspeed drive as browser start
device, drive types, Force 8, run in C64 mode); FAST and BOOT; drive A/B
mounts and REU image per slot; user commands; run from a mounted image (M);
the v4 browser keys and 80-column two-column listing; GEOS RAM boot;
NTP time sync; the v4 directory root reset before a slot start.

### 11.2 UBoot64 v2.x and v3.x features in DMBoot v5

| UBoot64 | Feature | DMBoot v5 |
|---|---|---|
| 2.0.0 | Oscar64 rebuild, directory listing in the REU, long names (50) and paths (255) | ✅ (IEC listings show 16-character names; long paths are the gain) |
| 2.0.0 | Configurable colours | ✅ One logical palette for both screens |
| 2.0.0 | Verbose or silent start-up | ✅ Silent, messages, or messages + wait |
| 2.0.0 | Splash screen | ✅ PETSCII art in 40 and 80 columns, overlay 6 (F2) |
| 2.0.0 | Configuration upgrade tool | ✅ `dmbupd45` |
| 2.1.0 | Default slot + auto-boot countdown | ✅ Starts the slot directly, never through a synthesised key |
| 2.1.0 | Mount-only and command-only slots, warning when replacing a program with a drive A mount | ✅ |
| 2.1.0 | REU detection probe barrier | ✅ |
| 2.1.1 | Several storage locations | ✅ USB only: `/usb*/11/`, then `/usb0-2/11/` |
| 2.2.0 | USB port reroute for images and REU files, "insert USB stick" prompt | ✅ Before the REU load |
| 3.0.0 | UCI auto-enable (firmware 3.15 unlock) | ✅ Only when the UCI is not found |
| 3.0.0 | Go up one directory on the rewritten SoftIEC parser | ✅ |
| 3.0.0 | Partition list (F4), root partition option (F8), slots with a partition | ✅ §9, with host paths instead of partition-relative paths |
| 3.0.1 | `cd://` for absolute paths on non-SoftIEC drives, correct "go up" on SD2IEC | ✅ |
| 3.0.1 | Root-partition conflict check for 3.15 and 3.15a, one shared name constant | ✅ |
| 2.x | UCI browse mode | ❌ Not ported: the DM ROM's SoftIEC drive is always there |
| 2.x | Browser keys `↑`, `T`/`E`, `Q`, `1` (`,1` load), `O` (demo mode) | ✅ |
| 2.x | Info screen sprite logo | ❌ Replaced by the PETSCII splash |
| 2.x/3.x | FC3 banking and cartridge start-up | ❌ Not relevant |

### 11.3 Code conventions

- Every function has a comment block: Title, Description, Syntax, Input,
  Output.
- `strncpy` + explicit terminator on every copy; sizes from `sizeof` or
  named constants; every external input (UCI, files, IEC, keyboard) checked
  for length and range.
- Structs for grouped state and layouts; named constants.
- No dynamic allocation.
- `petscii.h` everywhere that prints; strings sent to the Ultimate or the
  drives, or compared with their data, use an identity charmap, raw byte
  arrays or numeric PETSCII constants.
- Redraw only what changed.
- Oscar64 pitfalls (details in `oscar64manual.md`): stores inside `__asm`
  not seen by the optimiser (`volatile`); code used only through its address
  dropped (`#pragma reference`); REU load/store kept `__noinline` so reads
  of loaded data are not moved before the DMA; the REU size probe uses
  inline DMA (register-parameter bug in loops); the Oscar64 zero page and
  BASIC 7.

---

## 12. Status

| Phase | Content | Status |
|---|---|---|
| 0 | Skeleton: branch split, Makefile, overlays through the stores, REU, DM API, c64bridge | Done, verified on hardware |
| 1 | Platform: DualWin, UCI library, file I/O, storage search, start-up | Done, verified on hardware |
| 2 | Main menu and slot start | Done, verified on hardware (40 and 80 columns, both test machines) |
| 3 | Slot editor and auto-boot | Done, verified on hardware |
| 4 | File browser | Done, verified on hardware |
| 5 | Configuration, NTP, information, splash, GEOS RAM boot | Done, verified on hardware |
| 6 | Upgrade tool | Done, verified on hardware |
| 7 | Screenshots and release | Waits for the firmware 3.15 functions to be testable (DM ROM for 3.15) |

Open points:
- The firmware 3.15 paths (§9) are regression-tested on 3.14d only.
- Screenshots in the README are still from v4.
- Publishing v5 as a GitHub Release, as UBoot64 does, is to be decided at
  release.
