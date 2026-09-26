# DMBoot 128 v5 — Oscar64 Rebuild Plan

Branch: `Oscar64Rebuild` · Status: **plan only, nothing implemented yet** · Written 2026-09-25

This plan rebuilds DMBoot from scratch in Oscar64 for the C128. It ports the
functionality and conventions of the sibling project **UBoot64-v2**
(`/home/xahmol/git/UBoot64-v2`) and reuses the C128 techniques proven in
**VDC Screen Editor 2** (`/home/xahmol/VDCScreenEditor2`: overlays, banking,
VDC library) and **vdcmaniac** (`/home/xahmol/git/vdcmaniac`: build chain,
low-memory code).

---

## 1. Decisions taken

| Topic | Decision |
|---|---|
| Compiler | Oscar64 (`/home/xahmol/oscar64/bin/oscar64`), target `-tm=c128e` (program `$1C01`–`$C000`) |
| Start mechanism | Unchanged: a normal PRG `autostart.128.prg` in `/usb*/11/`, autostarted by the Device Manager (DM) ROM. It is not a cartridge (the big difference from UBoot64). |
| Display | **Both 40 columns (VIC) and 80 columns (VDC)**. The mode is detected at start, the same way the DM ROM leaves it. |
| REU | **Required**, as in UBoot64. Slots and directory listings live in the REU. Without an enabled REU, DMBoot exits with a clear message. |
| Slots | **36** (keys `0`–`9`, `a`–`z`). 80 columns shows two columns of 18. 40 columns shows two pages of 18. |
| File location | **`/usb*/11/`** (DM ROM convention) for program, overlays, config and slots. `/usb0-2/11/` are fallbacks for when several USB sticks are connected (UBoot64 2.1.1 idea, USB only: the U2+ has no SD card). All path conventions sit in one module so the change needed for firmware 3.15 lands in one place (see §9). |
| UBoot64 parity | Everything UBoot64 gained in v2.0.0–v3.0.1 that applies to the DM ROM target is included (matrix in §11.2). Partition features are prepared but deferred. |
| Firmware 3.15 | Partition support is **deferred** until the DM ROM is updated for 3.15. The data formats and code hooks are prepared now (§9). |
| Migration | New standalone upgrade tool `dmbupd45.prg` (v4 → v5) that converts the v4 `dmbootconf` + `dmbcfgfile` into the v5 formats. |
| Version | **v5.0.0** (major bumped, minor and patch set to 0). Build string `v5.0.0-YYYYMMDD-HHMM`. The current `v391-*` builds were published as the v4 (alpha) line, so the Oscar64 rebuild is v5. In this plan "v4" means those existing `v391-*` builds. |
| Legacy code | Everything from the cc65 era (sources, artifacts, release ZIPs) moves to branch **`legacy-cc65`** and is removed from this branch (§7). |
| REU layout | Starts at **address 0**, as in UBoot64. Slots that need particular REU contents load their own REU image when they run (§3.5). |
| Config files | `dmbconf.cfg` + `dmbslots.cfg` in the `11` directory, so the v4 files stay intact |
| 40-column menu | Two pages of 18 slots. Every slot key works on both pages. |
| Overlay overflow | Rebalance or split first, REU fallback second (§4.5) |
| Splash | Simple text logo built in code for v5.0.0. PETSCII art may replace it later. |
| File browser | **IEC mode only** on the DM ROM hyperspeed device, as in v4. No UCI browse mode. |
| Main menu keys | v4 layout kept (F1 browser, F2 info, F3 edit, F4 config, F5 go 64, F6 GEOS, F7 exit). Keys change only when a new function needs one. |

---

## 2. Architecture: what is shared with UBoot64 and what differs

| Aspect | UBoot64-v2 (C64) | DMBoot v5 (C128) |
|---|---|---|
| Container | FC3 16 KB × 4-bank cartridge (`.crt`) | Normal PRG plus overlay PRGs on `/usb*/11/` |
| Code swapping | `fc3_call(bank, fn)` ROM bank switching | Oscar64 `#pragma overlay`: overlay files loaded once at start, stored in bank 0 and bank 1 RAM, then copied into one overlay slot on demand (VDCSE pattern, §4) |
| Autostart | Cartridge cold start | DM ROM loads `autostart.128.prg` |
| Exit to BASIC | `fc3_exit()` + keyboard buffer | Keyboard buffer (`$034A`, count `$D0`) + `exit()`, after restoring the MMU/RCR state |
| Screen | VIC 40 columns, Oscar64 `CharWin` | VIC 40 columns **and** VDC 80 columns behind one `ui_*` layer (§6) |
| Extra platform | — | DM ROM extended API (`$807B` jump table): hyperspeed ID get/set, drive type, `run64`. GEOS 128 RAM boot. C128 FAST mode. `go 64`. |
| Slot storage | REU, 18 slots | REU, 36 slots |
| Directory listing | REU linked list | Same code, ported (replaces v4's VDC-RAM listing) |
| UCI library | `include/ultimate_*` | Same library, taken over (§7) |

Code ported from UBoot64-v2, VDCSE or vdcmaniac gets **no** "based on"
attribution comment, because these are the author's own repositories. Third-party
credits stay: DraBrowse (Sascha Bader / doj), ultimateii-dos-lib (Scott
Hutter / Francesco Sblendorio), the DM ROM API and GEOS RAM boot routine
(Bart van Leeuwen).

---

## 3. Complete memory model

### 3.1 MMU configuration while DMBoot runs

- **Common RAM: 8 KB at the bottom** (`$0000`–`$1FFF`). Set with `xmmu.rcr = 0x06` in `bnk_init()`, as VDCSE/vdcmaniac do. Low-memory code at `$1300` is then visible regardless of which bank is mapped in.
- **Restored to the default** (`rcr = 0x04`, 1 KB common) by `bnk_exit()` before any exit to BASIC, `go 64`, `dm_run64` or GEOS boot.
- **CPU speed:** in 80-column mode DMBoot runs at 2 MHz (`FAST`). In 40-column mode it stays at 1 MHz, because VIC output breaks at 2 MHz.
- **REU DMA always runs at 1 MHz.** (Phase 0 hardware result, U2+ emulated REU: a CPU-triggered REU transfer also passed 32/32 at 2 MHz without the wrapper, unlike asynchronous REST DMA which crashes at 2 MHz. The wrapper stays as a cheap safety margin for other REU hardware and for Bart's documented GEOS-boot crash case.) All `reu_load`/`reu_store` calls go through wrappers that clear `$D030` bit 0 around the transfer and restore it afterwards. Bart's GEOS routine documents the problem: at 2 MHz the machine often crashes after DMA completes.
- **REU DMA target bank:** on the C128 this is set by MMU RCR bit 6 (`$D506`). The wrappers keep bit 6 = 0 (bank 0). Slot and directory buffers are always in bank 0.

### 3.2 Bank 0

| Address | Size | Use |
|---|---|---|
| `$0000`–`$00FF` | 256 B | Zero page: KERNAL + Oscar64 registers |
| `$0100`–`$01FF` | 256 B | CPU stack |
| `$0200`–`$03FF` | | KERNAL/BASIC workspace. Keyboard buffer `$034A`, `$D0` count. GEOS reset handler target `$03E4`. |
| `$0400`–`$07E7` | 1000 B | VIC screen (40-column mode) |
| `$0800`–`$0AFF` | | KERNAL/BASIC workspace |
| `$0B00`–`$0BFF` | 256 B | TESTMODE mailbox for c64bridge (§7.1); unused in release builds (v4 put the GEOS overlay here; it moves into LMC) |
| `$0C00`–`$12FF` | | RS232 buffers, key definitions, DOS/VSP, BASIC variables: **do not touch** |
| `$1300`–`$1AFF` | 2 KB | **Low-memory code overlay `dmblmc`** (§4.3), in common RAM |
| `$1B00`–`$1C7F` | | BASIC stub `$1C01` + gap (same as VDCSE) |
| `$1C80`–`$97FF` | ~31 KB | **Resident program**: `code, data, bss, heap, stack` (`#pragma region(dmboot, 0x1c80, OVERLAYLOAD, …)`) |
| `$9800`–`$BFFF` | 10 KB | **Overlay load slot** `OVERLAYLOAD` (`OVERLAYSIZE = $2800`) |
| `$C000`–`$E7FF` | 10 KB | **Overlay store** (RAM under the KERNAL ROM, reached through `bnk_*`), overlay 5 |
| `$E800`–`$FEFF` | ~5.8 KB | Spare (reserve for overlay growth / extra text) |
| `$FF00`–`$FF04` | | MMU load-configuration registers |
| `$D000`–`$DFFF` | | I/O when mapped: VIC, SID, MMU `$D500`, VDC `$D600`, colour RAM `$D800`, CIA, REU `$DF00`, UCI `$DF1C` |

`OVERLAYSIZE = $2800` is a **starting estimate**. Phase 1 measures the
real sizes (map file). If an overlay does not fit, see §4.5.

### 3.3 Bank 1 (reached only through `bnk_*`, never mapped as the CPU default)

| Address | Size | Use |
|---|---|---|
| `$0000`–`$1FFF` | | Shadowed by the 8 KB common RAM, not separately usable |
| `$2000`–`$3FFF` | 8 KB | Background save for popup windows (`vdc_win` `WINDOWBASEADDRESS`), also used by the 40-column backend |
| `$4000`–`$67FF` | 10 KB | Overlay store: overlay 1 |
| `$6800`–`$8FFF` | 10 KB | Overlay store: overlay 2 |
| `$9000`–`$B7FF` | 10 KB | Overlay store: overlay 3 |
| `$B800`–`$DFFF` | 10 KB | Overlay store: overlay 4 |
| `$E000`–`$FEFF` | ~7.7 KB | Spare |

When DMBoot exits, BASIC takes bank 1 back for variables. That is harmless,
because overlays are not needed after exit (`dm_run64` and the GEOS
trampoline live in LMC in bank 0).

### 3.4 VDC RAM (16 KB minimum, 64 KB detected)

| VDC address | Size | Use |
|---|---|---|
| `$0000`–`$07CF` | 2000 B | Text screen 80×25 |
| `$0800`–`$0FCF` | 2000 B | Attributes |
| `$1000`–`$1FFF` | 4 KB | Swap/scratch buffer (library soft-scroll area) |
| `$2000`–`$3FFF` | 8 KB | Standard + alternate charset |
| `$4000`–`$FFFF` | 48 KB | 64 KB VDC only: **not used** (the directory listing moved to the REU). Stays free for later use. |

In 40-column mode DMBoot does not touch the VDC.

### 3.5 REU (required, 128 KB minimum)

DMBoot uses the REU **from address 0**, as UBoot64 does. Overwriting whatever
is in the REU at boot is acceptable: a slot that needs specific REU contents
loads its own REU image as part of the slot options, so the right REU state
is put in place when the slot runs.

| REU address | Size | Use |
|---|---|---|
| `$00000`–`$0BFFF` | 48 KB | Slots: 36 × `SLOTSIZE` (≤ 1360 B → 48,960 B), `SLOT_REU_START = 0` |
| `$0C000`–`$0C5FF` | 1.5 KB | Scratch slot (swap buffer for reorder, saves bank 0 RAM) |
| `$0C600`–`$0FFFF` | ~14.5 KB | Reserved: overflow overlay store, only used if §4.5 step 3 is needed |
| `$10000`–top of REU | ≥ 64 KB | Slot backup while re-ordering in the slot editor (`SLOT_REU_BACKUP`, 48 KB; editor and browser are never active together). Directory listing heap: DirElement linked list, bounded by `maxreuaddress = reudetected × 64 KB − 1` (~970 entries on a 128 KB REU, far more on larger ones) |

Rules:
- A slot boot does the steps in this order: mount A → mount B → run command setup → **REU image load last**. After the REU load nothing may return to the menu. Every later error goes to `errorexit()` to BASIC, because the slot data (and any overflow overlays) in the REU has been overwritten.
- GEOS RAM boot (F6) follows the same rule: once the REU image is loaded, control passes to the GEOS rboot loader and never returns.
- README says that DMBoot overwrites REU contents at every boot, and that slots should load their own REU image when a program needs one.

### 3.6 Where each piece of state lives

| Data | Location |
|---|---|
| `cfg` (ConfigStruct), `Slot` working buffer, `path[]`, execute buffers | Bank 0 resident BSS |
| 36 slots | REU `$00000` |
| Directory listing | REU `$10000`+, current element in bank 0 |
| Overlay images | Bank 1 (1–4) and bank 0 under ROM (5) |
| Popup backgrounds | Bank 1 `$2000` |
| Banking routines, DM API trampolines, `dm_run64` + filename, GEOS boot trampoline | LMC `$1300` |

---

## 4. Overlay system (VDCSE pattern)

### 4.1 Overlay list

| # | Oscar64 overlay (file) | Store | Contents |
|---|---|---|---|
| LMC | `dmblmc` → `$1300` | loaded once, stays resident | `bnk_*` routines, DM API asm (`dm_getapiversion`, `dm_getdevicetype`, `dm_get/sethsid`, `dm_run64` + `dm_prgnam`), GEOS RAM boot trampoline (Bart's routine, moved here from v4's `$0B00` overlay; needs the low shared RAM) |
| 1 | `dmbovl1` | bank 1 `$4000` | Main menu: slot rendering (2×18 / 2 pages), `mainmenu`, `autobootcountdown`, `pickmenuslot` |
| 2 | `dmbovl2` | bank 1 `$6800` | Slot editing: rename, reorder, delete, user command, default slot |
| 3 | `dmbovl3` | bank 1 `$9000` | File browser (IEC mode only, dirtrace, REU listing, sort, paging) |
| 4 | `dmbovl4` | bank 1 `$B800` | Configuration (NTP, verbose, timeout, colour palette, GEOS settings), NTP time sync, information/credits, splash |
| 5 | `dmbovl5` | bank 0 `$C000` | Exec: `runbootfrommenu`, mount/REU load with USB reroute, `execute()` keyboard-buffer build, `go 64`, GEOS boot preparation |

### 4.2 Pragmas (per overlay source file)

```c
#pragma overlay(dmbovl1, 2)
#pragma section(codeovl1, 0)
#pragma section(dataovl1, 0)
#pragma section(bssovl1, 0)
#pragma region(ovl1, OVERLAYLOAD, 0xC000, , 2, { codeovl1, dataovl1, bssovl1 })
#pragma code(codeovl1)
#pragma data(dataovl1)
#pragma bss(bssovl1)
```

Main program region: `#pragma region(dmboot, 0x1c80, OVERLAYLOAD, , , {code, data, bss, heap, stack})`.
LMC region in `banking.c`: `#pragma overlay(dmblmc, 1)` + `#pragma region(bank1, 0x1300, 0x1b00, , 1, {...})`.

### 4.3 Load and swap flow

1. `main()` → `bnk_init()`: record `bootdevice`, set RCR 8 KB common, `load_overlay("dmblmc")` to `$1300`.
2. For each overlay 1–5: KERNAL load (`krnio_setbnk(0,0)`, `11:dmbovlN` from `bootdevice`) into `OVERLAYLOAD`, then `bnk_memcpy()` to its store address in bank 1 or bank 0 and record it in `overlaydata[]`. This happens once, with verbose/spinner feedback.
3. `loadoverlay(n)`: if `n != overlay_active`, `bnk_memcpy(BNK_DEFAULT, OVERLAYLOAD, overlaydata[n].bank, overlaydata[n].address, OVERLAYSIZE)`. No disk access after startup (v4 went back to disk on every switch).

### 4.4 Rules

- Overlays never call each other. Anything two overlays need is resident.
- Functions called from resident code into an overlay are `__noinline` (VDCSE rule, UBoot64 §12.2).
- `#pragma code/data/bss` comes after all includes and before any definition (UBoot64 §12.6).
- Overlay load names go through one helper, `dm_filename()` (§9), which adds the `11:` prefix today.

### 4.5 If the budget does not fit

In order of preference:
1. Move shared helpers into the resident region, or out of it, based on the `.map` file.
2. Split the file browser into two overlays (`browse` / `select+actions`), using the spare bank 0 `$E800` or bank 1 `$E000` area.
3. Store overlays that don't fit in the reserved REU area `$0C600`–`$0FFFF` (`reu_load` straight into `OVERLAYLOAD`). The REU-load-last rule in §3.5 already covers this: after a slot's REU image is loaded, no overlay is needed any more. (Decided: split first, REU fallback second.)

---

## 5. Module map (planned)

| File | Where | Role |
|---|---|---|
| `src/main.c` | resident | Regions, globals, startup, main dispatch loop, `loadoverlay` |
| `src/core.c` | resident | `errorexit`, `delay`, `getkey`, `textInput`, IEC helpers (`cmd`, `dosCommand`, `iec_present`, device scan), `pathconcat`, `headertext`, spinner |
| `src/ui.c` / `ui.h` | resident | 40/80 screen abstraction (§6) |
| `src/fileio.c` | resident | REU wrappers (1 MHz), REU detect (UBoot64 probe-barrier fix), slots REU↔UCI in 500 B chunks, config read/write |
| `src/dmpaths.c` | resident | All `/usb*/11/`, `11:` and `cp11` conventions (§9) |
| `src/petscii_ascii.c` | ovl 3 / 5, only if needed | ASCII↔PETSCII. UBoot64 needed it for UCI-mode listings; the IEC-only browser gets PETSCII names directly. It is kept only if converting IEC paths to UCI paths for mounts or REU files needs it. |
| `src/slotmenu.c` | ovl 1 | Menu + pick slot |
| `src/slotedit.c` | ovl 2 | Edit functions |
| `src/filebrowse.c` | ovl 3 | Browser |
| `src/config.c`, `src/time.c`, `src/info.c` | ovl 4 | Config/NTP/info/splash |
| `src/exec.c` | ovl 5 | Boot execution |
| `include/banking.c/h` | LMC + resident | From VDCSE (`bnk_*`, `load_overlay`, `bnk_init/exit`) |
| `include/dmapi.c/h` | LMC | DM ROM API in Oscar64 `__asm {}` (ported from `dmapiasm.s`) |
| `include/geosboot.c/h` | LMC | GEOS RAM boot trampoline (from `geosramroutine.s`) |
| `include/vdc_core.c/h`, `vdc_win.c/h` | resident | VDC library (VDCSE suite, see `~/.claude/vdclib_c128.md`) |
| `include/ultimate_*.c/h` | resident | UCI library (§7) |
| `include/defines.h` | — | Constants, structs, externs |
| `src/dmbupd45.c` | standalone PRG | v4 → v5 migration (§10) |

---

## 6. Dual 40/80-column UI

**Decided (2026-09-25): a new, reusable library with its own manual.** It
builds on the two existing CharWin-style window implementations, Oscar64's
`c64/charwin` (VIC, 40 columns) and the VDC window layer of the VDCSE library
suite (`vdc_core`/`vdc_win`, 80 columns). It gives one API over both, so
DMBoot (and later projects) write screens once. Files live in `include/`,
documented in a manual next to `vdclib_manual.md` in the repo root. The
library is created in Phase 1, before any UI screens are ported.

- **Detection:** at start, check `$D7` bit 7 (80-column active). Set `SCREENW`, `slotcols`, `DIRW`, `MENUX`, and speed (FAST only in 80 columns).
- **`ui_*` API** used by all screens: `ui_init`, `ui_clear`, `ui_putat(x,y,str,role)`, `ui_putat_reverse`, `ui_fill_rect`, `ui_getch`, `ui_checkch`, `ui_textinput`, `ui_popup_open/close`, `ui_cursor`. Colours are set by **role** (header1, header2, text, input, key, dir normal/select, error, ok), never by raw colour number.
- **Backends:**
  - VDC: `vdc_core` + `vdc_win` (VDCSE suite, banking variant).
  - VIC: a small CharWin-style implementation on `$0400`/`$D800`, taken from UBoot64 `cw` usage.
  - Selected through a mode variable with simple `if` dispatch, not function pointers (Oscar64 optimisation rule).
- **One logical colour palette** in `ConfigStruct` (C64/VIC colour numbers). *Changed in Phase 1:* DualWin maps logical colours to VDC colours through an overridable table (`dwin_vdc_colors[]`), so one palette serves both screens (see `DUALWINMANUAL.md`).
- **Layouts:**
  - Main menu: 80 columns uses 2 columns × 18 slots. 40 columns uses 1 column × 18 slots with 2 pages (cursor left/right or `,`/`.` to switch; slot keys `0`–`z` work regardless of the visible page).
  - Browser: 80 columns uses the v4 two-column listing. 40 columns uses one column.
  - Popups use the same logical sizes in both modes, centred.

---

## 7. Ultimate library and build chain

**UCI library:** `UBoot64-v2/include/ultimate_*` plus `UCILIBMANUAL.md` is the **authoritative base** (decided 2026-09-25: most up to date, bug-fixed, includes the firmware 3.15 additions); where it conflicts with the `UltimateDemo2026` copy, UBoot64's names and semantics win. On request it was then **completed** (2026-09-25) against released firmware 3.15a: every command usable on an Ultimate II+ is wrapped (new: REU load/save at an address, REU preload, file size, finish capture, decode track, EasyFlash erase, load config, the whole SoftIEC target in `ultimate_softiec_lib`, storage media helpers), with bounds-checked name handling. The **HTTP target** (new in 3.15) is **deferred**: DMBoot only uses the network for NTP. TCP listener commands are left out (not in released firmware). `UCILIBMANUAL.md` §17 has the complete coverage table. The additions were copied back into UBoot64-v2 (2026-09-25, uncommitted there; bank usage unchanged), so UBoot64-v2 remains the most complete copy.

**Makefile** (per `~/.claude/makefile_conventions.md` and UBoot64/VDCSE):
- `SYS = c128e`, `CC = /home/xahmol/oscar64/bin/oscar64`, `VERSION_MAJOR/MINOR/PATCH = 5/0/0`, timestamped `VERSION`.
- `CFLAGS = -i=include -tm=c128e -O2 -dNOFLOAT -dHEAPCHECK -dVERSION=...`, `-g` for `.map/.lbl/.asm` in `build/`.
- Targets:
  - `all`: `autostart.128.prg`, `dmblmc.prg`, `dmbovl1..5.prg`, `dmbupd45.prg`, `README.pdf`, ZIP in `build/`.
  - `clean`, `docs` (pandoc, warn-and-skip), `check-deploy`, `deploy`.
  - `test-build` (`-g -dTESTMODE`, same output names so `deploy` ships it; `make` restores the release build, as in VDCSE).
- Deploy: `wput` to `ftp://$(ULTIP1)/$(ULTUSB)/11/`. The IP comes from the gitignored `.env`.
- `MAIN_SRCS` lists every transitively compiled `.c`/`.h`, because make can't see `#pragma compile` chains.

**Repo hygiene on this branch:**
- New `.gitignore` covering `build/`, `.env`, release ZIPs, `*.o`, `*.d`.
- **Branch `legacy-cc65`** is created from `main` at `2aef5ab` (the last cc65 commit, v391-20231011-1210) and keeps the complete cc65 tree as it is: sources, linker config, `c128-ram.*`, built `.prg/.map/.seq` files, `geosram1571.reu`, `slotlayout.xlsx` and all historic release ZIPs.
- On `Oscar64Rebuild`, **all cc65-era files are removed**:
  - `src/*`, `include/*`, `dmboot-cc65.cfg`, `c128-ram.*`
  - built artifacts, `geosram1571.reu`, `test.prg`, `slotlayout.xlsx`, `.vscode/dryrun.log`
  - every `DMBoot-v*.zip(p)` / `autostart.128.prg_*` release file
- Kept and rewritten on this branch: `README.md`/`README.pdf` and `LICENSE.MD`. The `pictures/` screenshots are replaced in Phase 7.
- README changelog: the download links for the old versions change from `raw/main/…` to `raw/legacy-cc65/…`, so they keep working after this branch is merged into `main`. v5 release ZIPs are built into `build/` (gitignored), not committed. Publishing them as GitHub Releases, as UBoot64 does, is to be confirmed in Phase 7.
- `.vscode/c_cpp_properties.json` + `settings.json` per `~/.claude/vscode_intellisense.md`.
- Copies of `oscar64manual.md` (per global instructions), `UCILIBMANUAL.md` and `vdclib_manual.md` in the repo root.
- New `ARCHITECTURE.md` (UBoot64 structure). `CLAUDE.md` rewritten for v5.

**Testing:** VICE has no UCI emulation, so DMBoot exits at `uii_detect()`.
Verification is on real hardware (C128 + U2+ + DM ROM) through `make deploy`
plus c64bridge (§7.1). Each phase ends with the hardware checklist for that
phase, run in **both 40 and 80 columns**.

### 7.1 Hardware testing with c64bridge

**Target:** the C128 test machine's Ultimate II+ at **`192.168.1.237`**. It
has REST API v0.1 (no `/v1/info`), FTP with `/Usb1/11/`, and the v4 install
present. The IP is in the gitignored `.env` (`ULTIP1`) for `make deploy`, and
in `~/.c64bridge.json` as the `u2` backend profile. Select it with
`c64_select_backend u2` after the MCP server has been reconnected.

**What the `u2` backend offers here, and what it doesn't:**
- Offered: config read/write, drive and mount status, file transfer, memory read/write (DMA), reset and reboot.
- Not offered: keyboard input (`machine:input` does not exist on U2), streaming, or debug register access.
- `read_menu_screen` needs firmware 3.15.
- c64bridge is written for the C64. Its `key`/`write_text` helpers target the C64 keyboard buffer (`$0277`/`$C6`), and its screen reader assumes a C64 at `$0400`. On the C128 we only use the low-level memory operations.

**Test hooks built into DMBoot (`-dTESTMODE` build, `make test-build`):**
- **Key injection without keyboard input:** write keys into the C128 KERNAL keyboard buffer (`$034A`–`$0353`, count at `$D0`) with c64bridge memory write. DMBoot reads keys through KERNAL `GETIN` in both UI backends (a design requirement for the `ui` layer), so injected keys drive every menu exactly like typed keys.
- **Test mailbox at `$0B00`–`$0BFF`** (the page that is free in the §3.2 map). TESTMODE writes a fixed-layout status block there:
  - magic bytes + version, current screen/overlay id, `overlay_active`
  - last UCI status string, last error code/text
  - detected REU size, VDC RAM size, 40/80 flag, DM API version, firmware version
  - resident free bytes (heap/stack high-water mark)
  - a heartbeat counter that the main loop increments
- **80-column screen readback:** the VDC's RAM cannot be read over DMA (it is only reachable through the `$D600/$D601` register pair). On a mailbox command byte, TESTMODE copies the VDC text screen to a bank 0 buffer that c64bridge can read. 40-column screens are read directly from `$0400`.
- **Symbols:** `-g` builds produce `build/*.lbl`, so tests address globals (`Slot`, `cfg`, `overlay_active`, …) by symbol rather than by hard-coded address.

**Test flow per phase:**
1. `make test-build deploy` (FTP to `/Usb1/11/`).
2. c64bridge `reset`. The DM ROM autostarts `autostart.128.prg`.
3. Poll the mailbox heartbeat and status until the expected screen id appears.
4. Inject keys and check the mailbox, screen dump, drive/mount status (`c64_drive`) and written config files (FTP download of `dmbconf.cfg`/`dmbslots.cfg`, compared with the expected structs).
5. Repeat in 40 and 80 columns. Switching needs the physical 40/80 key, so this step is done by the user, or the test covers the mode the machine is in and records which one.

**Safety rules (learned 2026-09-25, see `tests/crash/`):**
- Every REST memory read or write **stops the C128 CPU briefly** (DMA; firmware `C64::peek`). A `c64bridge reset` followed by reads every 2 s during boot crashed the machine within about 5 s. The DM ROM crash handler reported `BREAK AT $DC03 IN IO SPACE` with `$FF00=$2A` (the DM function-ROM configuration), loaded its `SNAPSHOT128` tool at `$1C01`, and wrote `/Temp/crashdump*.prg`. A reset from the crash menu then booted DMBoot v4 normally.
- **Root cause (found in Phase 0): REST memory access is DMA, and the C128 crashes on DMA while it runs at 2 MHz** (same reason REU DMA needs 1 MHz). Both crashes happened with the CPU in FAST mode (80 columns). TESTMODE builds therefore stay at 1 MHz; a test that switches to 2 MHz restores 1 MHz before going idle, and the harness waits (does not poll) for its maximum duration.
- **REST reads show the memory configuration of that moment (found 2026-09-26).** A DMA read sees what the MMU maps at the instant of the read, not always RAM: DMBoot switches the ROMs in for KERNAL calls (also in its key wait loop), so a read of `$4000`-`$FFFF` can return BASIC/KERNAL ROM instead of DMBoot's RAM. A 32-byte read of the heap at `$7938` showed BASIC 7 code (byte-identical to the C128 `basiclo` ROM at that address); a 4 KB read a minute later showed the intact heap. Reads below `$4000` (screen `$0400`, mailbox `$0B00`, common RAM) are always RAM. For data above `$3FFF`, read twice and compare, or let TESTMODE copy it into the mailbox page.
- **Never read or write memory over REST while the machine is booting, loading, or using the drives** (DM autostart, overlay preload, directory reads, mounts, REU loads). Wait for the TESTMODE mailbox to report idle (menu loop heartbeat) first. Until then, watch progress with drive status (`/v1/drives`) and FTP, which do not touch the C128 bus.
- Before any state-changing c64bridge operation (reset, reboot, memory write, drive command), confirm with the user, who is at the machine.
- Confirmed: REST DMA reads see **bank 0 RAM**, not I/O (`$D500` reads `$FF`). v4's bank 1 slot data was not visible.

**Phase 0 must verify c64bridge on C128 first:**
- which RAM bank REST DMA memory reads and writes see while DMBoot runs (expected: bank 0 at the CPU's current MMU config, so the mailbox and keyboard buffer in common/low RAM are safe choices)
- ~~that a REST reset leads to a clean DM ROM autostart~~ **Verified 2026-09-25:** `c64_system reset` followed by 30 s without memory access autostarts DMBoot cleanly. The earlier crash was caused by the memory reads during boot, not by the reset.
- which REST endpoints firmware v0.1 actually supports

The findings go into `CLAUDE.md`.

**Upgrade tool testing (Phase 6):** download the real v4 files from `/Usb1/11/` (`dmbootconf.prg` 18,434 B, `DMBCFGFILE` 328 B) **read-only** to `tests/data/`, write field-level expectations, run `dmbupd45` on the machine, then download and compare the new files. Never delete or overwrite the v4 files on the device.

**What stays manual:** visual checks (layout, colours, 80-column attributes), physical 40/80 switching, GEOS boot and program launches that leave DMBoot (after exit only drive/mount state and screen RAM can be checked).

---

## 8. Data formats (CFGVERSION `0x05`)

**`SlotStruct`** (UBoot64 v3 layout plus the DMBoot fields). Target ≤ 1360 B, padded to a multiple of 16, zero padding.

| Field | Type | Note |
|---|---|---|
| `cfgvs` | char | `0x05` (v4 files carry `0x01`) |
| `menu` | char[31] | 30 characters, fits the 80-column two-column layout (40 − key label) |
| `path` / `file` | char[256] / char[51] | Program path (IEC/hyperspeed path or UCI path) and file |
| `cmd` | char[81] | User command |
| `reu_path` / `reu_image` / `reusize` | char[256] / char[51] / char | REU preload. In v4 this reused `image_a_path`; v5 has its own field. |
| `image_a_path/file/id`, `image_b_path/file/id` | | Mounts |
| `runboot` | char | `EXEC_MOUNT 0x01`, `EXEC_FRC8 0x02`, `EXEC_RUN64 0x04`, `EXEC_FAST 0x08`, `EXEC_BOOT 0x10`: **all v4 values unchanged** (so the upgrader copies `runboot` as-is). Extended with `EXEC_COMMA1 0x20` and `EXEC_DEMO 0x40` for the new UBoot64 features. These are the next free bits: UBoot64's own values `0x02`/`0x10` are already taken by Force 8 and BOOT in DMBoot. The two projects never share slot files, so different values are harmless. |
| `device` | char | IEC device |
| `command` | char | `COMMAND_CMD/REU/IMGA/IMGB` |
| `isdefault` | char | Autoboot default slot (compared `== 1`) |
| `partition` | char | **3.15 preparation**: 0 = none. Stored now, only used later. |
| `padding` | char[] | **Zero-filled** (UBoot64 lesson: no watermark in padding) |

**`ConfigStruct`:**
- `version`, `timeon`, `host[81]`, `secondsfromutc` (long), `verbose`, `timeoutidx`
- `colors` (one logical palette, C64/VIC colour numbers)
- GEOS RAM boot: `geos_reu_path[256]`, `geos_reu_image[51]`, `geos_reusize`, `geos_image_a/b_path/file/id`
- `iec_root_partition` (reserved, 0)
- `reserved[16]` (zero)

Old files that are shorter are zero-filled on read (UBoot64 pattern).

**Files in `/usb*/11/`:** `dmbslots.cfg` (36 × SlotStruct) and `dmbconf.cfg`.
These are new names, so the v4 files `dmbootconf`/`dmbcfgfile` stay intact.
Old version remains runnable, and the upgrade tool can read the old files and write the new ones side by side.

---

## 9. Firmware 3.15 preparation (no partition functionality yet)

- **`dmpaths.c` is the only place that knows about the DM layout**:
  - `dm_config_dir()` returns `/usb*/11/`.
  - `dm_filename(name)` returns `11:name`.
  - `dm_select_boot_partition()` sends `cp11` / `cd:←`.
  - One compile switch, `DM_LAYOUT_LEGACY11`, selects this.
  - When the DM ROM defines its 3.15 convention, only this module changes.
- **Firmware identification** at startup (`uii_identify()` → `fw_major/minor`, flag `fw_315plus`) is stored and shown in verbose mode and on the info screen. No behaviour depends on it yet.
- **Slot `partition` field and config `iec_root_partition`** exist in the v5 formats from day one, so enabling 3.15 support needs no CFGVERSION bump or migration tool.
- **Browser structure** keeps UBoot64's `partition_depth`/`CH_DEL` hook points and a reserved key (F4 in IEC mode, as in UBoot64). `iec_read_partitions`, `dir_read_partition_list` and `uii_add_partition` usage are **not** ported yet.
- **Boot path hook:** `exec.c` calls `iec_select_partition()` only when `Slot.partition != 0`. Until then, v5 never writes a non-zero value.
- **`pathconcat()` uses `cd://…` (double slash) from the start.** This is the UBoot64 v3.0.1 fix; it is also correct before 3.15.
- **UCI auto-enable** (`uii_enable()`, `$D038/$D036` unlock) is independent of the DM ROM, so it is **enabled from v5.0.0** (see §11.2).
- **Storage search** (`/usb*/11/`, then `/usb0-2/11/`) also lives in `dmpaths.c`, so the 3.15 DM layout replaces one candidate list.
- Reference for later: `UltimateDemo2026/FIRMWARE315UPGRADEPLAN.md` and UBoot64 `ARCHITECTURE.md` §11 "SoftIEC Partition Browsing" (listing name change in 3.15a, partitions not persistent, delete not working, `"$=P"` charmap trap).

---

## 10. Upgrade tool `dmbupd45.prg` (v4 → v5)

Standalone `c128e` PRG (uses `bank_minimal` + UCI library), placed in `/usb*/11/`:

1. Load the old `dmbootconf` (raw 36 × 512 B blob, originally saved from bank 1 `$0400`) with KERNAL LOAD into bank 1 (`krnio_setbnk(1,0)`), as v4 did.
2. Read `dmbcfgfile` (328 B) through UCI, using v4 `configcommon.c` offsets (REU path 60, image name 20, …).
3. Convert each slot:
   - Keep the `runboot` flags.
   - `reu_path` ← old `image_a_path` when `COMMAND_REU` is set; when that is empty (seen in the real v4 file), derive it from `path`.
   - Layout: each 512-byte slot is two 256-byte pages (v4 `getslotfromem`). Page 1 holds `path`…`cfgvs` (246 bytes used); page 2 holds the image fields from offset 256.
   - The 3-byte prefix is `cd:` (verified on real v4 files), but not always present: OHG 64 has `/usb1/...` without it, which v4's `+3` broke. Strip `cd:` only when present, and convert mount/REU paths to ASCII. `path` keeps it (IEC command).
   - Clear `COMMAND_IMGA`/`IMGB` when no image file name is stored (seen in the real v4 file).
   - Reference implementation: `tests/tools/convert_v4_slots.py`.
   - Menu name 20 → 30.
   - Zero the padding.
   - `partition = 0`, `isdefault = 0`.
4. Build the ConfigStruct: NTP/GEOS fields from the old file, default palette, `verbose = 1`.
5. Write `dmbslots.cfg` / `dmbconf.cfg` in 500 B UCI chunks. Leave the old files untouched as a backup.
6. v5 behaviour when the new files are missing but `dmbootconf` exists: show "run dmbupd45 first" instead of silently creating empty defaults.

---

## 11. Features

### 11.1 Kept from DMBoot v4 (`v391-*` builds)

- 36 slots
- Main menu: F1 file browser, F2 info, F3 edit, F4 config, F5 go 64, F6 GEOS RAM boot, F7 exit (`scnclr:new`)
- DM API: hyperspeed ID as browser start device, device-type detection, Force 8 (`8`), run in C64 mode (`6`)
- FAST flag, BOOT flag (`boot u<n>`, browser F5)
- Mount A/B + REU per slot, user commands, run-from-mounted-image (`M`)
- Browser keys `+`/`-`, `D`, `A`/`B`, `M`, `8`, `6`, `U`/`P`, F5, F1 refresh
- 80-column two-column directory listing

### 11.2 UBoot64 v2.x and v3.x features: status in DMBoot v5

UBoot64 v1 was itself a port of DMBoot, so this table covers everything UBoot64 added from v2.0.0 to v3.0.1.

| UBoot64 version | Feature / fix | DMBoot v5 |
|---|---|---|
| 2.0.0 | Oscar64 rebuild | ✅ This plan |
| 2.0.0 | Directory listing in REU (large directories) | ✅ §3.5, overlay 3 |
| 2.0.0 | Long filenames (50) and paths (255) | ✅ SlotStruct §8. **Note for IEC-only browsing:** long *paths* (deep directory trees, up to 255 characters in total) are the real gain. *Filenames* shown through the IEC directory listing are limited to 16 characters by the CBM directory format. The 51-character fields cost nothing in the REU and are kept for the 3.15 layout. |
| 2.0.0 | Configurable colour scheme | ✅ One logical palette (C64/VIC colour numbers), shown on the VDC through DualWin's colour mapping |
| 2.0.0 | Verbose or silent (spinner) startup | ✅ Covers overlay preloading, REU detect, config/slot load, NTP |
| 2.0.0 | Splash screen in Information | ✅ Adapted: **simple text logo built in code** for v5.0.0, laid out for both 40×25 and 80×25, shown with F2 (and optionally at startup in silent mode), in overlay 4. The splash is kept behind one function so PETSCII art (40×25 VIC + 80×25 VDC) can replace it later without other changes. |
| 2.0.0 | Configuration upgrade tool | ✅ `dmbupd45.prg` §10 |
| 2.1.0 | Default boot slot + auto-boot timeout (Off/1/3/5/10 s), countdown screen, any key cancels | ✅ The DM ROM autostarts DMBoot, then DMBoot's countdown boots the default slot, so a fully unattended power-on-to-program works. Keeps UBoot64's fix: call `runbootfrommenu()` directly, never a synthesised keypress (the letter-slot endless-loop bug). This matters even more with 26 letter slots. |
| 2.1.0 | Mount/command-only slots (empty file), created via browser `A`/`B` or edit-menu command on an empty slot | ✅ |
| 2.1.0 | Warning when adding a drive A image to a slot that already launches a program | ✅ |
| 2.1.0 | Fix: REU detection miscompile (probe barrier) | ✅ `fileio.c` REU detect |
| 2.1.0 | Fix: garbled side panel (`optimize(0)` workaround) | ✅ Same workaround ready, applied when the symptom appears |
| 2.1.1 | Config/slots on SD as well as USB, search order SD → USB0 → USB1 → USB2 | ⚠️ **SD part not applicable:** the Ultimate II+ has no SD card slot. The USB part is adapted to the DM convention: candidates are the `11` directories, `/usb*/11/` first (current v4 behaviour), then `/usb0/11/`, `/usb1/11/`, `/usb2/11/`, which covers several USB sticks being connected at once. The first one that holds `dmbconf.cfg` wins, otherwise the first that holds `autostart.128.prg`. It lives in `dmpaths.c` (§9). `dmbupd45` uses the same search. |
| 2.2.0 | USB port auto-reroute for mount images and REU files (real operation used as the probe, `82,FILE NOT FOUND` = keep searching) | ✅ Overlay 5 |
| 2.2.0 | "Insert USB stick" prompt with retry / F7 to BASIC when not found on any port | ✅ Happens before the REU load, so returning to the menu is still safe (§3.5) |
| 3.0.0 | UCI auto-enable (firmware 3.15+ unlock `$D038/$D036`, then wait for `uii_detect()`) | ✅ **Enabled** (not deferred): it does not depend on the DM ROM. Only runs when `uii_detect()` fails. Phase 0 checks that the writes are harmless on the C128's VIC-IIe (unused register range) and on older firmware. |
| 3.0.0 | IEC "go up one directory" adapts to the rewritten SoftIEC DOS parser | ✅ Browser IEC mode |
| 3.0.0 | SoftIEC/CMD partition list (F4 in IEC mode), DEL at partition root returns to the list | ⏸ Deferred (§9), **including** CMD-HD / SD2IEC partition browsing (decided: everything partition-related comes together with SoftIEC 3.15 support). F4 is reserved in the browser. |
| 3.0.0 | "SoftIEC root partition" config toggle (F8) + auto-provisioning | ⏸ Deferred (§9), config field reserved |
| 3.0.0 | Slots record and restore a partition at boot | ⏸ Field present (§8), boot hook present, value always 0 for now |
| 3.0.0 | Format change with partition fields | ✅ Already in the v5 format, so no later migration is needed |
| 3.0.1 | `cd://path` double slash for absolute paths on SoftIEC/VICE | ✅ `pathconcat()` |
| 3.0.1 | DEL on non-Ultimate IEC devices with CMD-HD-style partitions (SD2IEC) sent an invalid command, so display and drive disagreed | ✅ Browser IEC mode |
| 3.0.1 | Root-partition conflict check for firmware 3.15a / charmap-consistent `UBOOT_PARTITION_NAME` | ⏸ Comes with partitions. The general lesson (one shared constant for wire-protocol strings, identity charmap) applies to all UCI/DOS literals from the start. |
| 3.0.1 | Partition delete prompt removed (firmware doesn't delete) | ⏸ Not implemented at all |
| 2.x | UCI browse mode (native USB filesystem) + F3 UCI/IEC toggle | ❌ **Not ported (decided).** UBoot64 needed UCI mode because it has to work without SoftIEC. DMBoot always has the DM ROM's SoftIEC hyperspeed drive (it is how DMBoot gets autostarted), so the browser stays **IEC only**, as in v4. F3 stays free in the browser. |
| 2.x | Browser keys: `↑` root, `T`/`E` first/last item, cursor-right enter / cursor-left parent (40-column single-column mode only), `Q` quit | ✅ Merged key set. None clash with the v4 keys. In 80-column two-column mode, cursor left/right keep v4's sideways movement. |
| 2.x | `1` toggle `,1` load | ✅ `EXEC_COMMA1`: `load"x",u<n>,1` + `run`. Useful for ML programs in C128 mode; ignored for `run64` (DM ROM loads those itself). |
| 2.x | `O` demo mode: power down Ultimate drives not on ID 8 | ✅ `EXEC_DEMO` |
| 2.x | Info screen sprite logo | ❌ Not ported. C64 VIC sprite in the cassette buffer; optional later, 40 columns only. |
| 2.x/3.x | FC3 banking, cartridge init fixes | ❌ Not relevant (PRG + overlays) |

### 11.3 Code conventions taken over

- `strncpy` + explicit NUL on every copy
- ASCII↔PETSCII conversion only at the UCI boundary
- No dynamic allocation
- All Oscar64 quirk workarounds from UBoot64 `ARCHITECTURE.md` §12: `volatile` REU args, `for`/`continue`, `unsigned long` in conditions, no printf precision, charmap for wire strings, REU probe barrier, `optimize(0)` fallback

---

## 12. Phases

Each phase ends with a build, a deploy to hardware (`192.168.1.237`), a c64bridge-driven check of the exit criterion where it can be automated (§7.1), and a manual 40 + 80 column visual check.

| Phase | Content | Exit criterion |
|---|---|---|
| **0. Skeleton** | Create `legacy-cc65`, remove the cc65 files from this branch, `.gitignore`, Makefile (v5.0.0), docs copies, VSCode config. Resident `main` + LMC + two dummy overlays through the bank 1 / bank 0 store. REU detect + 1 MHz DMA wrapper. DM API call from LMC. | The DM ROM autostarts it from `/usb*/11/`. Overlays swap without disk access. REU round-trip works at 2 MHz. DM API version is printed. **c64bridge `u2` backend works on the C128:** reset → autostart, mailbox readable, key injection through `$034A`/`$D0` drives the skeleton, supported REST endpoints recorded. |
| **1. Platform** | `ui` layer (VIC + VDC), UCI library merge, `fileio` (config + slots ↔ REU ↔ UCI), `dmpaths`, `core`, startup flow with verbose/spinner, firmware identification. **Measure memory budget (map) and freeze `OVERLAYSIZE`.** | Config and slots survive a save → power-cycle → load. Resident region has ≥ 2 KB headroom. |
| **2. Menu + exec** | Overlays 1 and 5: 36-slot menu (2×18 / 2 pages), slot run with mount/reroute/REU-last, Force 8, run64, FAST, BOOT, commands, go 64, F7 exit with MMU restore. | Every `runboot` flag combination boots correctly. After a slot's REU image load, nothing returns to the menu. |
| **3. Edit** | Overlay 2 + autoboot countdown | Rename, reorder, delete, command, default slot and timeout work. |
| **4. File browser** | Overlay 3: IEC mode only, dirtrace, REU listing, sort, paging, selection → `pickmenuslot`, hyperspeed start device | Large directories (300+ entries) work. All add-to-slot paths produce slots that boot. |
| **5. Config + GEOS + info** | Overlay 4 + GEOS F6 from LMC trampoline | NTP sync, colour palette in both modes, GEOS boots from REU image. |
| **6. Upgrade tool** | `dmbupd45.prg` | Real v4 config files convert with no loss (compare field by field). |
| **7. Docs + release** | README (v5 changelog, install, upgrade from v4, REU usage note, 3.15 status), ARCHITECTURE.md, CLAUDE.md, README.pdf, ZIP | `make all` produces the release ZIP. Docs match the code. |

---

### Phase status (2026-09-25)

- **Phase 0: done**, verified on hardware.
- **Phase 1: done except items that need later phases or the user:**
  - Done:
    - DualWin (verified in 80 columns).
    - Complete UCI library.
    - `dmpaths`, `fileio`, core helpers and the startup flow, verified on the C128. First boot created `dmbconf.cfg` (1041 B) and `dmbslots.cfg` (48,960 B) with correct contents; `W` rewrote them; the next boot read them without rewriting.
  - Open:
    - 40-column hardware test (user, weekend).
    - Freezing `OVERLAYSIZE`: the overlays are still Phase 0 dummies, so this moves to Phase 2 when the first real overlays exist.
- **Phase 2: in progress.**
  - Verified on the C128 (80 columns):
    - The 36-slot menu.
    - Slot start with path + command + RUN on the hyperspeed drive (`cd:/usb1/11/`, `print"cmd ok"`, `RUN"DMBTEST",U11` → `RUN OK`).
    - The startup root reset `drive_root_reset()` (v4 sequence `cp11`, `cd:`+`$FF`, `cp0`, `cd:`+`$FF`).
  - Slot paths follow v4:
    - On SoftIEC/Ultimate drives, an absolute `cd:/usb1/...`.
    - On other drives, `cd//...` relative to the partition root.
  - Slot start (2026-09-26): all statements on one line joined with `:` and one RETURN; verified in 40 and 80 columns on .23 (`PRINT"CMD OK":RUN"DMBTEST",U11`). Replaces v4's line-per-statement layout, which broke in 40 columns (READY. overwrote the RUN line).
  - 64 KB VDC RAM (2026-09-26, .23 with a SaRuMan VDC): in 16 KB mode the 80 column screen was corrupted (header partly unreversed, slot lines missing, cleared lines keeping old text). `dwin_setup` now detects the VDC RAM size and sets register 28 bit 4 on a 64 KB VDC, as v4 did (not reset on exit, also as v4). Verified: the 80 column main menu is correct on .23.
  - VDC back to 16 KB addressing on exit (2026-09-26): left in 64 KB mode (as v4 did), slot U (CP/M from an REU image) showed a garbled 80 column screen on .23. `dwin_exit` now switches back before CINT; GEOS RAM boot keeps 64 KB mode. Verified: U works on .23.
  - Browser list walk and REU size probe (2026-09-26): REU load/store wrappers `__noinline` (inlined, reads of loaded data were moved before the DMA: cursor down hung), size probe with inline DMA (Oscar64 register-parameter bug in loops). Oscar64 updated to 546b627. Verified on .23: REU detected, browser navigation and directories, re-order still works.
  - Main menu F8 (2026-09-26): switch to the other screen (KERNAL SWAPPER, Oscar64 zero page $F7-$F9 kept around it); the screen left behind says which screen to switch to; `dwin_exit` keeps the chosen screen after CINT. At 2 MHz the VIC display is blanked (as BASIC FAST), on again at 1 MHz and on every exit. Verified on .23 with the release build.
  - Also verified on .23 (2026-09-26): C64 mode slots and F5 go 64; the three start-up modes (silent, messages, messages + wait).
  - Earlier (superseded) cosmetic issue, same as v4: output of a user command starts on the command's own row and overwrites its tail. Probably because the screen editor echoes no CR after a line entered from the keyboard buffer (not investigated). Harmless.
  - Verified with slots converted from the real v4 file (`tests/tools/convert_v4_slots.py`):
    - Plain run from a disk image folder: 5 Tristam Island, 7 Risen f.oblivion.
    - REU image load + run: U ZP/M+ (`/usb1/cpm/cpm.reu`).
    - 8 GEOS128 Ramboot starts v4's `geosramboot`. That program then fails on image B: the v4 `DMBCFGFILE` has image B `GEOSAPP.D81` with an empty path, and the file is missing. v4 config issue. For Phase 5/6: an empty GEOS image path means `/usb*/11/`; warn about missing images.
    - Second, less stale stick (`tests/data/stick2`), slot 0 MegaPatch 3.3 128 US: REU load (16 MB) + image A on 8 + image B on 9 + run from the mounted image works. Needed a fix: power on Ultimate drives only when off, then wait 2 s (UBoot64 approach); an immediate mount gave `90,drive not present`. MegaPatch itself then fails because the test C128 has only 16 KB VDC RAM (MP3 needs 64 KB), not a DMBoot issue.
    - BASIC loaders with variables/strings crashed after a slot start (BREAK, PC `$1005B`): Oscar64 zero page overlaps BASIC 7 work storage. Fixed by saving `$02-$26`, `$43-$62`, `$F7-$FF` at the start of `main()` and restoring them in `dmb_exit`. Verified: X GeckOS 2 (Force 8 + FAST, BASIC loader) works.
    - F-keys were expanded by the screen editor (F7 = LIST started slot L). Fixed via the key store vector `$033C` → `$C6B7` (cc65 approach), restored on exit. Verified: F7 quits to BASIC.
    - Also works: 2 U-term128, V Oxford Pascal.
    - BOOT: F Superbase (Force 8 + FAST + BOOT, v4 style: cd into the image, BOOT on the same drive) works after two fixes. The path is now also sent without a file name, and `execute()` now builds the BOOT command without a file name (both as v4).
    - A Colour Spectrum boots the same way but hangs in Krill's loader: that loader needs real drive emulation (drive code upload) and a single drive on the bus. Slot changed by hand to "mount on drive A (ID 8) + demo mode + BOOT" (runboot `0x51`): works. Demo mode + mount + BOOT verified.
  - Compared with v4 on the same stick: R Keynes shows nothing on the 80-column screen in v4 too (a 40-column program). A Colour Spectrum hangs in Krill's loader in v4 (cd + BOOT); v5 runs it with mount + demo mode. Everything that worked in v4 works in v5.
  - To test: drive power-on from off; C64 mode (C, D, F5) later.
- **Phase 3: implemented, not yet hardware-tested.**
  - Slot list drawing and slot picking moved to the resident `slotlist.c`, shared by the main menu (overlay 1) and the editor (overlay 2, `slotedit.c`, 4.3 KB).
  - Editor (F3 in the main menu): F1 rename, F2 command (an empty slot becomes a command-only slot), F3 re-order with cursor keys (wrap-around, cancel restores from an REU backup at `$10000`), F4 auto-boot timeout (off/1/3/5/10 s), F5 delete, F6 default slot, F7 back (saves slots and/or config when changed).
  - UBoot64 bug not carried over: `editmenuoptions` overwrote its "changes made" flag per action, so an earlier edit could stay unsaved.
- **Phase 4: implemented, not yet hardware-tested.** Overlay 3 `browse.c` (9.0 KB of 10 KB including buffers):
  - IEC only. Starts on the Device Manager hyperspeed drive (else the first active device); `+`/`-` step through the active devices from `iec_scan`.
  - Directory as a linked list in the REU from `$10000` to the top of the REU. Storage is isolated in `dir_load`/`dir_store_meta`, so the list could move (for example to bank 1) by changing only those.
  - One index-based navigation routine for cursor, page (`P`/`U`), top/end (`T`/HOME, `E`) and the 80-column column switch. 80 columns: two columns of 19 entries.
  - Keys as v4 + UBoot64 (IEC mode): F1 refresh, `S` sort, RETURN run/enter, DEL/`↑` up/root, `D` dirtrace, F5 boot, `6` C64 mode, `8` Force 8, `F` FAST, `1` `,1` load, `O` demo, `A`/`B` add mount, `M` run from the traced image, REU image via RETURN, F7/`Q` quit.
  - Dirtrace off: RETURN/F5/`6` start directly (request in `browsereq`, started by overlay 5 `exec_browse`). Dirtrace on: the choice goes to a slot (`browse_pick`, as UBoot64 `pickmenuslot`).
  - Mounts, run-from-image and REU images need the dirtrace on the SoftIEC/hyperspeed drive, because slots store them as Ultimate paths (`/` + trace, PETSCII to ASCII).
  - UBoot64 bug not carried over: block counts were stored in a `char` (sizes shown modulo 256).
- **Phase 5: implemented except the GEOS RAM boot (F6), not yet hardware-tested.** Overlay 4 `config.c` (4.7 KB):
  - NTP time sync at start-up (after the drive detection, before the optional key wait), as UBoot64 `get_ntp_time`; the converted time is kept in a static buffer (UBoot64 returned a pointer to a local array).
  - Configuration (main menu F4): NTP on/off, start-up feedback (silent / show messages / show messages + wait), UTC offset (validated, ±14 h), auto-boot timeout, NTP server (stored in ASCII, edited in PETSCII), colour editor with live preview and undo from a copy taken on entry.
  - Information (main menu F2): text logo (one function, replaceable by PETSCII art), version, Ultimate/REU/DM API info, credits.
  - `pet2asc` moved to the resident `core.c` (browser and configuration use it).
  - GEOS RAM boot (F6): Bart van Leeuwen's routine in the LMC (`geosboot.c`), started by `exec_geos` in overlay 5 after mounting A/B and loading the GEOS REU image last; settings in the configuration (F8), ASCII stored. The v4 "no REU" error path left the ROMs switched in; fixed.
- Found while reviewing: C64 mode typed `SYS 0` since Phase 2 (Oscar64 dropped the address-only `__asm dm_run64` and folded its address to 0). Fixed; see the Oscar64 manual. Needs a hardware test of C/D/F5.
- Slot start sends v4's drive root reset before the slot path (`cd:/...` is relative on the SoftIEC drive; the browser may leave the drive in a subdirectory). v4 did this before every overlay load.
- **Phase 6: implemented, not yet hardware-tested.** `dmbupd45.prg` (7.4 KB, built with `make build`/`test-build`, deployed with the rest):
  - Reads `dmbootconf.prg` (36 × 512 B, two 256-byte pages per slot) and `DMBCFGFILE` (328 B) from the DMBoot directory, asks before overwriting existing v5 files, writes `dmbslots.cfg` and `dmbconf.cfg`; the v4 files stay as backup.
  - Same rules as `tests/tools/convert_v4_slots.py`; GEOS images without a path get the DMBoot directory (with a note).
  - DMBoot v5 without v5 files but with `dmbootconf.prg`: asks whether to start with empty slots or to run the upgrader first (plan §10 point 6).
  - Shared modules for this: `petconv.c`, `cfgdefaults.c`, `basicexit.c` (clean return to BASIC 7 for both programs).

## 13. Risks and verification items

1. **Memory budget.** 31 KB resident has to hold both UI backends, the VDC library, the UCI library and globals. Measure in Phase 1 before building features. Fallbacks are in §4.5.
2. **Filebrowser size** (~2,100 lines in UBoot64, which had a 16 KB ROM bank for it). Split early if it exceeds `OVERLAYSIZE`.
3. **Memory the DM ROM uses at runtime.** v4 already used `$1300` and bank 1, but `$C000`+ RAM under ROM in bank 0 and bank 1 `$4000`+ must be checked while started from the DM ROM.
4. **REU DMA on C128 at 2 MHz, and the bank 1 target.** Handled by the wrappers. Test explicitly in Phase 0.
5. **`11:` prefix + KERNAL load through hyperspeed** for overlays from Oscar64 `krnio_*`. Check in Phase 0.
6. **Oscar64 `#pragma overlay` with 5 overlays + LMC on c128e.** Use the VDCSE naming and bank numbering. Check that the overlay file names come out as expected.
7. **v4 `+3` path prefix and REU path in `image_a_path`.** Found in v4 `pathconcat()`: the stored path begins with `cd:` (SoftIEC/VICE) or `cd/` (other devices), and `+3` strips it to give the UCI path. v5 stores the mount/REU path **without** this prefix, and the upgrader strips it. The REU path stored in `image_a_path` still has to be checked against real config files.
8. **c64bridge on a C128 is untested.** The tool is C64-oriented and the device runs REST API v0.1. If DMA does not see the expected bank, or needed endpoints are missing, fall back to FTP deploy + manual checks, with the mailbox still readable through plain REST `/v1/machine:readmem` calls from a small script.
9. **Firmware 3.15 on hardware before the DM ROM update.** The `/11` convention breaks. Out of scope, but README must say so.
