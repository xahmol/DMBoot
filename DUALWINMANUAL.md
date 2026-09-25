# DualWin: 40/80 column window library for the Commodore 128

DualWin gives one character-window API for both C128 text screens. You write a screen once and it runs in 40 columns (VIC-IIe) and in 80 columns (VDC 8563). DualWin picks the backend at run time from the screen that is active when the program starts.

- 40 columns: Oscar64's `CharWin` (`<c64/charwin.h>`).
- 80 columns: `vdc_win`/`vdc_core` from the VDC library suite of my VDC Screen Editor 2 project (https://github.com/xahmol/VDCScreenEditor2, see `vdclib_manual.md`).

Written for DMBoot 128 v5 (Oscar64, target `c128e`). Files: `include/dualwin.h`, `include/dualwin.c`.

Status (2026-09-25): 80 column mode is verified on a real C128. 40 column mode is not yet verified on hardware.

---

## Contents

1. [Concepts](#1-concepts)
2. [Requirements and set-up](#2-requirements-and-set-up)
3. [Colours](#3-colours)
4. [API reference](#4-api-reference)
5. [Popups](#5-popups)
6. [Text input](#6-text-input)
7. [Safety rules and limits](#7-safety-rules-and-limits)
8. [Example](#8-example)
9. [Notes on the VDC library copy](#9-notes-on-the-vdc-library-copy)

---

## 1. Concepts

| Concept | Meaning |
|---|---|
| Screen | The active C128 text screen: 40x25 (VIC) or 80x25 (VDC). `dwin_state.width` / `height`. |
| Window (`struct DWin`) | A rectangle on the screen with its own cursor. All positions passed to window functions are relative to the window. |
| Clipping | Output never leaves the window. Strings are cut at the right edge. Rectangles are cut to the window. Positions outside the window write nothing. |
| Logical colour | A C64/VIC colour number 0-15 (`VCOL_*` from `<c64/vic.h>`), mapped to a VDC colour in 80 columns. |
| Characters | PETSCII, as produced by string literals when `<petscii.h>` is included. DualWin switches the screen to the lower/upper case charset at set-up. |
| Popup | A bordered window whose screen area is saved on open and restored on close (LIFO stack). |

DualWin keeps its own cursor (`cx`, `cy` in `struct DWin`). Because the cursor logic (newline, wrap, scroll) is the library's own, console output behaves the same on both screens.

## 2. Requirements and set-up

- Oscar64, target `c128e` (or `c128`).
- The banking layer (`banking.h`/`banking.c`) with `bnk_memcpy`, `bnk_cpytovdc` and `bnk_cpyfromvdc`. These must run from common RAM, because they switch the MMU to the popup storage bank.
- The VDC library files `vdc_core.c/h`, `vdc_win.c/h`, `vdcwin_types.h`, `peekpoke.h`.
- A banked RAM area for popup backgrounds. DMBoot uses bank 1 `$2000`–`$3FFF`.

```c
#include <petscii.h>
#include <c64/vic.h>
#include "banking.h"
#include "dualwin.h"

bnk_init();                                              // LMC with the bnk_* routines
dwin_setup(BNK_1_FULL, (char *)0x2000, 0x2000);          // popup storage in bank 1
dwin_screen_colors(VCOL_BLACK, VCOL_BLACK);
```

`dwin_setup()` does the following:
- detects the active screen (zero page `$D7` bit 7) and PAL/NTSC (`$0A03`);
- sends `CHR$(14)` so the KERNAL switches the active screen to lower/upper case;
- in 80 columns, detects the VDC RAM size and sets `vdc_state` for 80x25 (PAL or NTSC).

It does **not** switch screens or change the CPU speed (unlike the application-specific `vdc_init()` of the VDC suite).

## 3. Colours

All functions take logical colours: C64/VIC colour numbers 0-15.

- **40 columns:** the colour is written to colour RAM unchanged.
- **80 columns:** the colour is translated through `dwin_vdc_colors[16]`, then combined with the alternate charset attribute (`VDC_A_ALTCHAR`, lower/upper case).

Default mapping (an application may overwrite entries):

| Logical colour | VDC colour | Logical colour | VDC colour |
|---|---|---|---|
| 0 black | black | 8 orange | dark yellow |
| 1 white | white | 9 brown | dark yellow |
| 2 red | dark red | 10 light red | light red |
| 3 cyan | light cyan | 11 dark grey | dark grey |
| 4 purple | light purple | 12 grey | light grey |
| 5 green | dark green | 13 light green | light green |
| 6 blue | dark blue | 14 light blue | light blue |
| 7 yellow | light yellow | 15 light grey | light grey |

An application therefore needs only one palette, whatever the screen.

## 4. API reference

### Set-up

| Function | Description |
|---|---|
| `void dwin_setup(char storecr, char *storebase, unsigned storesize)` | Detects the screen and PAL/NTSC, switches to the lower case charset, initialises the VDC state, and registers popup storage (MMU `$FF00` value, start, size). |
| `void dwin_screen_colors(char border, char background)` | VIC: border and background. VDC: background (the VDC has no separate border). |
| `bool dwin_is80(void)` | true in 80 column mode. |

### Windows

| Function | Description |
|---|---|
| `void dwin_init(struct DWin *win, char sx, char sy, char wx, char wy)` | Defines a window at screen position `sx,sy` of `wx` x `wy` characters. It is clipped to the screen, and 0 for `wx`/`wy` means "to the screen edge". The window is not cleared. |
| `void dwin_clear(struct DWin *win)` | Fills the window with spaces and moves the cursor home. |
| `void dwin_fill_rect(win, x, y, w, h, ch, color)` | Fills a rectangle with a PETSCII character. |
| `void dwin_scroll_up(win, color)` | Scrolls one line up and clears the bottom line in `color`. |

### Output (cursor does not move)

| Function | Description |
|---|---|
| `void dwin_putat_char(win, x, y, ch, color)` | One PETSCII character. |
| `char dwin_putat_string(win, x, y, str, color)` | String, clipped at the right edge. Returns the number of characters written. |
| `char dwin_putat_string_reverse(win, x, y, str, color)` | Same, shown in reverse. |
| `void dwin_reverse_rect(win, x, y, w, h)` | Sets reverse on a rectangle, for example a selection bar. |

### Cursor and console output

| Function | Description |
|---|---|
| `void dwin_cursor_move(win, cx, cy)` | Moves the cursor (clamped to the window). |
| `void dwin_cursor_newline(win, color)` | Next line. Scrolls when on the last line. |
| `void dwin_cursor_show(win, show)` | Shows or hides the cursor as a reverse character. |
| `void dwin_put_char(win, ch, color)` | Character at the cursor, then advances. LF or CR gives a newline. Wraps at the edge. |
| `void dwin_put_string(win, str, color)` | String with console behaviour. Any length. |
| `void dwin_printf(win, color, fmt, ...)` | printf-style console output. See the limit in §7. |

### Popups

| Function | Description |
|---|---|
| `bool dwin_popup_open(win, x, y, w, h, bordercolor, color)` | Saves the area, draws a border, clears the inside, and initialises `win` for the inside. `x,y,w,h` is the outer rectangle including the border (at least 3x3). |
| `void dwin_popup_close(void)` | Restores the area of the last opened popup. |

### Keyboard

| Function | Description |
|---|---|
| `char dwin_checkch(void)` | Raw key from KERNAL GETIN, or `DWIN_KEY_NONE`. Does not wait. |
| `char dwin_getch(void)` | Waits for a raw key. |
| `int dwin_input(win, x, y, buffer, size, width, color)` | One-line text input. See §6. |

Keys are read raw from the KERNAL keyboard buffer (`$034A`/`$D0`), with no character conversion. Keys a test harness writes into that buffer therefore work too. Do not use Oscar64's `kbhit()` on the C128: it reads the C64 buffer count at `$C6`.

## 5. Popups

- Storage per popup: `w * h * 2` bytes (text plus colour RAM on the VIC, or text plus attributes on the VDC), including the border.
- Popups are stacked. Each one is stored right after the previous one in the storage area.
- At most `DWIN_POPUP_MAX` (4) popups are open at a time.
- `dwin_popup_open()` returns false (and changes nothing) when:
  - the maximum is reached;
  - the area is outside the screen;
  - the area is smaller than 3x3;
  - the storage area is too small.
- The border uses PETSCII box characters (`$B0 $AE $AD $BD $C0 $DD`), which look the same in both charsets.
- Close popups in reverse order of opening. `dwin_popup_close()` always restores the most recent one.

## 6. Text input

```c
char name[31] = "Edit me";            // up to 30 characters
int len = dwin_input(&win, 1, 3, name, sizeof(name), 20, VCOL_WHITE);
if (len == DWIN_INPUT_CANCEL) { /* RUN/STOP pressed */ }
```

- **`size` is the size of the buffer** (`sizeof`). The string never grows beyond `size - 1` characters, so the buffer cannot overflow. Existing content is kept and the cursor starts at its end.
- **`width` is the visible field width** (clipped to the window). Longer strings scroll horizontally.
- **Keys:**

  | Key | Action |
  |---|---|
  | RETURN | Accept |
  | RUN/STOP | Cancel (the edits stay in the buffer) |
  | DEL | Delete left of the cursor |
  | SHIFT-DEL | Insert a space |
  | Cursor left/right | Move |
  | HOME | Go to the start |
  | Printable characters | Overwrite in the middle of the string, append at the end |

- **Return value:** the string length, or `DWIN_INPUT_CANCEL` (-1).

Based on the DraCopy/DraBrowse text input by Sascha Bader (https://github.com/doj/dracopy), as adapted in my UBoot64-v2 project. Unlike that version, `size` is the buffer size and every index is bounds-checked.

## 7. Safety rules and limits

- **Clipping everywhere:** no output function writes outside its window, so an over-long string (a file name, user input, data from a device) cannot corrupt other screen areas.
- **`dwin_printf` buffer:** Oscar64 has no `vsnprintf`, so `dwin_printf` formats into a `DWIN_PRINTF_MAX` (200) byte buffer with `vsprintf`. Only use it for output of known, bounded length. Print strings of unknown length (file names, user input, UCI data) with `dwin_put_string` or `dwin_putat_string` instead of through `%s`.
- **`printf` precision:** Oscar64's printf has no precision field (`%.10s`). Truncate first with `strncpy` + explicit terminator.
- **Reversed areas:** filling with PETSCII `$A0` (shifted space) does **not** give reverse spaces: both backends convert it to screen code `$60`, a normal blank. Fill with spaces, write the text, then call `dwin_reverse_rect()` over the area (as `headertext()` in DMBoot does).
- **Macro names:** do not name variables or struct members `vic` or `vdc`. Both are macros in the Oscar64 headers (`<c64/vic.h>`, `<c128/vdc.h>`).
- **Popup storage bank:** the storage must be reachable with the given MMU value. The `bnk_*` routines must be in common RAM.

## 8. Example

```c
struct DWin screen, console, popup;

dwin_setup(BNK_1_FULL, (char *)0x2000, 0x2000);
dwin_screen_colors(VCOL_BLACK, VCOL_BLACK);

dwin_init(&screen, 0, 0, 0, 0);                 // full screen
dwin_clear(&screen);
dwin_putat_string_reverse(&screen, 0, 0, " Title ", VCOL_YELLOW);

dwin_init(&console, 0, 9, 0, 0);                // rows 9 to the bottom
dwin_printf(&console, VCOL_LT_GREEN, "%u columns\n", dwin_state.width);

char x = (dwin_state.width - 30) / 2;           // centred in 40 and 80 columns
if (dwin_popup_open(&popup, x, 3, 30, 7, VCOL_YELLOW, VCOL_LT_BLUE))
{
    dwin_putat_string(&popup, 1, 1, "Press a key", VCOL_WHITE);
    dwin_getch();
    dwin_popup_close();
}
```

## 9. Notes on the VDC library copy

DMBoot carries its own copy of the VDC library suite, taken from VDC Screen Editor 2 and kept identical to it. Two bugs found while building DualWin were fixed in both (VDC Screen Editor 2 `vdclib_manual.md`, gotcha 12):

1. **`vdc_hchar()` with length 1 filled 257 positions.** `vdc_block_fill()` writes one byte plus a block of `length - 1` bytes, and a VDC block count of 0 means 256. Now length 1 plots a single character and length 0 plots nothing. Found on hardware: one-character-wide popup borders filled large parts of the screen.
2. **`vdcwin_put_rect_raw()` filled one attribute byte too many per row.** It passed `w` instead of the zero-based `w - 1` to `vdc_block_fill()`.

VDC Screen Editor 2 itself never hit either bug: its `vdc_hchar()` calls always use widths far above 1 (vertical borders are drawn with `vdc_printc()`), and it never calls `vdcwin_put_rect`.
