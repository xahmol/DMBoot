# VDC Library Manual

Oscar64 C128 VDC libraries by Xander Mol — reference for `vdc_core`, `vdc_win`, `banking`, and related modules.

All library source and headers live in `include/`. Include the header; Oscar64's `#pragma compile()` inside the header automatically pulls in the `.c` file.

---

## Module Overview

| Header | Source | Used by | Purpose |
|---|---|---|---|
| `vdc_core.h` | `vdc_core.c` | Main editor | VDC chip layer with banking |
| `vdc_win.h` | `vdc_win.c` | Main editor | Windowing + viewport with banking |
| `vdc_menu.h` | `vdc_menu.c` | Main editor | Pulldown menu system |
| `banking.h` | `banking.c` | Main editor | Full C128 MMU banking, dir, SID |
| `vdc_nobnk.h` | `vdc_nobnk.c` | Utilities | VDC chip layer, no banking |
| `vdcwin_nobnk.h` | `vdcwin_nobnk.c` | Utilities | Windowing + viewport + soft scroll, no banking |
| `bank_minimal.h` | `bank_minimal.c` | Utilities | Minimal MMU banking (read/write/load/save only) |

**Rule of thumb:** use the `*_nobnk` pair for standalone programs that only access bank 0. Use `vdc_core.h` + `vdc_win.h` + `banking.h` for programs that access bank 1 or need overlays, directory I/O, or SID.

---

## vdc_core / vdc_nobnk

### Color Constants

```c
VDC_BLACK(0)  VDC_DGREY(1)   VDC_DBLUE(2)   VDC_LBLUE(3)
VDC_DGREEN(4) VDC_LGREEN(5)  VDC_DCYAN(6)   VDC_LCYAN(7)
VDC_DRED(8)   VDC_LRED(9)    VDC_DPURPLE(10) VDC_LPURPLE(11)
VDC_DYELLOW(12) VDC_LYELLOW(13) VDC_LGREY(14) VDC_WHITE(15)
```

### Attribute Bit Constants

```c
VDC_A_BLINK     0x10  // bit 4
VDC_A_UNDERLINE 0x20  // bit 5
VDC_A_REVERSE   0x40  // bit 6
VDC_A_ALTCHAR   0x80  // bit 7 — use alternate charset
```

An attribute byte is `color | VDC_A_xxx | ...`, e.g., `VDC_LYELLOW | VDC_A_ALTCHAR`.

### VDCMode Enum

```c
enum VDCMode {
    VDC_TEXT_80x25_PAL,    // 16KB VDC OK
    VDC_TEXT_80x50_PAL,    // needs 64KB VDC (banking ver) / OK on 16KB (nobnk ver)
    VDC_TEXT_80x70_PAL,    // needs 64KB VDC
    VDC_TEXT_80x25_NTSC,
    VDC_TEXT_80x50_NTSC,
    VDC_TEXT_80x60_NTSC    // needs 64KB VDC
};
```

> **Gotcha:** the 80x50 PAL entry has `extmem=1` in `vdc_core.c` but `extmem=0` in `vdc_nobnk.c`. The nobnk version permits 80x50 on 16KB VDC.

### VDCStatus Global

```c
struct VDCStatus vdc_state;  // set by vdc_init(), updated by vdc_set_mode()
```

Key fields:

| Field | Type | Contents |
|---|---|---|
| `memsize` | char | 16 or 64 (KB detected) |
| `memextended` | char | 1 = 64KB VDC mode active |
| `mode` | char | current VDCMode index |
| `width` | unsigned | chars per row (80) |
| `height` | unsigned | rows |
| `text_attr` | char | current default attribute byte |
| `base_text` | unsigned | VDC address of visible text buffer |
| `base_attr` | unsigned | VDC address of visible attribute buffer |
| `swap_text` | unsigned | VDC swap scratch area (used by H-scroll) |
| `char_std` | unsigned | VDC address of standard charset |
| `char_alt` | unsigned | VDC address of alternate charset |
| `disp_skip` | char | extra columns per row (soft scroll virtual screen) |

### Initialization

```c
void vdc_init(char mode, char extmem);
```
Call once at program start. Detects VDC RAM, switches to 80-col if needed, enables 2 MHz, sets mode. `extmem=1` requests 64KB VDC mode if the chip has it.

```c
void vdc_exit();
```
Restore 1 MHz, reset to 80×25 PAL, clear screen, call `bnk_exit()`. Call before returning to BASIC.

### Screen Mode

```c
char vdc_set_mode(char mode);
// Returns 1=success, 0=fail (64KB required but unavailable).
// Updates vdc_state, sets VDC registers, clears screen.

void vdc_set_extended_memsize();
// Enable 64KB VDC. No-op if already set or memsize==16.
// Wipes VDC RAM first to avoid display corruption.

void vdc_set_default_memsize();
// Revert to 16KB VDC. No-op if not currently extended.

void vdc_detect_mem_size();
// Probe VDC RAM size. Sets vdc_state.memsize to 16 or 64.
```

### Display Enable/Disable

```c
void vdc_disable_display();  // VDCR_HSTART = 0x80 (blank)
void vdc_enable_display();   // VDCR_HSTART = 0x7d (normal)
```

Always wrap charset redefinition or VDC memory layout changes in disable/enable to prevent visible artifacts.

### Block Operations (VDC Hardware DMA)

```c
void vdc_block_fill(unsigned address, char value, char length);
// length is zero-based: 0=1 byte, 255=256 bytes.

void vdc_block_copy_page(unsigned dest, unsigned src, char length);
// Single page (max 256 bytes, zero-based length).

void vdc_block_copy(unsigned dest, unsigned src, unsigned length);
// Multi-page: splits into 256-byte pages automatically.

void vdc_scroll_copy(unsigned dest, unsigned src, char lines, char length);
// Copy a rectangle row-by-row. Used by windowing scroll routines.

void vdc_wipe_mem();
// Zero all 64KB VDC RAM. Required before toggling 16K↔64K mode.
```

### Color and Attribute State

```c
void vdc_bgcolor(char color);    // VDC background color (reg 26 bits 0-3)
void vdc_fgcolor(char color);    // VDC foreground color (reg 26 bits 4-7)
void vdc_textcolor(char color);  // Set color nibble of vdc_state.text_attr
void vdc_altchar(char set);      // Set/clear VDC_A_ALTCHAR in text_attr
void vdc_blink(char set);        // Set/clear VDC_A_BLINK in text_attr
void vdc_underline(char set);    // Set/clear VDC_A_UNDERLINE in text_attr
void vdc_reverse(char set);      // Set/clear VDC_A_REVERSE in text_attr
```

Default `text_attr` after `vdc_init`: `VDC_LYELLOW | VDC_A_ALTCHAR`.

### Drawing

```c
void vdc_printc(char x, char y, char val, char attr);
// Write one screencode + attr at absolute column x, row y.

void vdc_prints(char x, char y, const char *string);
// Write PETSCII string at (x,y) using vdc_state.text_attr.

void vdc_prints_attr(char x, char y, const char *string, char attr);
// Same with explicit attr byte.

void vdc_hchar(char x, char y, char val, char attr, char length);
// Horizontal fill: `length` characters starting at (x,y).

void vdc_vchar(char x, char y, char val, char attr, char length);
// Vertical fill: `length` characters downward from (x,y).

void vdc_clear(char x, char y, char val, char length, char lines);
// Fill rectangle (length × lines) with val + text_attr.

void vdc_cls();
// Clear full screen with spaces using text_attr.
```

### Coordinate Helper

```c
unsigned vdc_coords(char x, char y);
// Offset from base address for column x, row y (uses multab[] precomputed table).
// Add vdc_state.base_text / vdc_state.base_attr to get absolute VDC address.
```

### Utility

```c
char pet2screen(char p);        // PETSCII → screencode
char screen_width();            // Returns 40 or 80 (reads zero page $D7)
void screen_setmode(char mode); // Switch between 40-col VIC and 80-col VDC (Kernal $FF5F)
void fastmode(char set);        // 1=2MHz+blank VIC, 0=1MHz+unblank VIC
```

### VSync

```c
void vdc_wait_vblank();      // Spin until VDC status bit 5 set (entering VBLANK)
void vdc_wait_no_vblank();   // Spin until VDC status bit 5 clear (leaving VBLANK)
void vdc_pass_vblank();      // Wait for full VBLANK passage
```

### Charset Management

Banking version (`vdc_core.h`):
```c
void vdc_restore_charsets();
// Copy ROM charset (BNK_CHARROM, 0xD000) to VDC char_std. 512 characters.
```

nobnk version (`vdc_nobnk.h`) adds direct-copy functions:
```c
void vdc_redef_charset(unsigned vdcdest, volatile char *sp, unsigned size);
// Copy charset from bank-0 RAM to VDC. size = number of characters.

void vdc_cpytovdc(unsigned vdcdest, volatile char *sp, unsigned size);
// Copy bytes from bank-0 RAM to VDC.

void vdc_cpyfromvdc(volatile char *dp, unsigned vdcsrc, unsigned size);
// Copy bytes from VDC to bank-0 RAM.
```

---

## vdc_win / vdcwin_nobnk

### VDCWin Struct

```c
struct VDCWin {
    char sx, sy;    // Screen origin (column, row) — absolute VDC screen coords
    char wx, wy;    // Width and height in characters
    char cx, cy;    // Cursor position within window (0-based)
    unsigned sp;    // VDC text buffer address at (sx, sy)
    unsigned cp;    // VDC attr buffer address at (sx, sy)
};
```

### VDCViewport Struct

Banking version (`vdc_win.h`):
```c
struct VDCViewport {
    char sourcebank;        // MMU CR value for source data bank
    char *sourcebase;       // Base pointer in that bank
    unsigned sourcewidth;   // Full width of source screen map
    unsigned sourceheight;  // Full height of source screen map
    unsigned sourcexoffset; // Current horizontal scroll offset
    unsigned sourceyoffset; // Current vertical scroll offset
    struct VDCWin view;     // The visible window on the VDC screen
};
```

nobnk version: identical but no `sourcebank` field (always bank 0).

**Source memory layout:** screencodes occupy `sourcewidth × sourceheight` bytes starting at `sourcebase`. Attribute data starts at `sourcebase + sourcewidth × sourceheight + 48` (48-byte header gap).

### VDCSoftScrollSettings (nobnk only)

```c
struct VDCSoftScrollSettings {
    char *source;       // Source screen data pointer (bank 0)
    char width;         // Virtual screen width
    char height;        // Virtual screen height
    unsigned addr_offset;
    char vscroll, hscroll;
    char vscroll_base, hscroll_base;
    char xoff, yoff;
    char hscroll_def;
};
```

### Window Init and Fill

```c
void vdcwin_init(struct VDCWin *win, char sx, char sy, char wx, char wy);
// Set up VDCWin struct. Does NOT clear or draw anything.

void vdcwin_clear(struct VDCWin *win);         // Fill with spaces, current text_attr
void vdcwin_fill(struct VDCWin *win, char ch); // Fill with ch, current text_attr
```

### Cursor Movement

```c
void vdcwin_cursor_move(struct VDCWin *win, char cx, char cy);
void vdcwin_cursor_toggle(struct VDCWin *win);
// Toggle reverse+blink at cursor position (visual blinking cursor).

bool vdcwin_cursor_left(struct VDCWin *win);     // Returns false at left edge
bool vdcwin_cursor_right(struct VDCWin *win);    // Returns false at right edge
bool vdcwin_cursor_up(struct VDCWin *win);       // Returns false at top
bool vdcwin_cursor_down(struct VDCWin *win);     // Returns false at bottom
bool vdcwin_cursor_forward(struct VDCWin *win);  // Right, wraps to next line
bool vdcwin_cursor_backward(struct VDCWin *win); // Left, wraps to previous line
bool vdcwin_cursor_newline(struct VDCWin *win);  // cx=0, cy++; false at last line
```

### Output at Cursor Position

PETSCII input (auto-converted to screencode on write):
```c
void vdcwin_put_char(struct VDCWin *win, char ch);
void vdcwin_put_chars(struct VDCWin *win, const char *chars, char num);
char vdcwin_put_string(struct VDCWin *win, const char *str); // returns chars written
```

Raw screencode input:
```c
void vdcwin_put_char_raw(struct VDCWin *win, char ch);
void vdcwin_put_chars_raw(struct VDCWin *win, const char *chars, char num);
char vdcwin_put_string_raw(struct VDCWin *win, const char *str);
```

All `put_*` variants advance the cursor and scroll the window up if the last row is exceeded.

### Output at Explicit Position

```c
void  vdcwin_putat_char(struct VDCWin*, char x, char y, char ch);       // PETSCII
void  vdcwin_putat_chars(struct VDCWin*, char x, char y, const char*, char num);
char  vdcwin_putat_string(struct VDCWin*, char x, char y, const char*); // PETSCII
void  vdcwin_putat_char_raw(struct VDCWin*, char x, char y, char ch);   // screencode
void  vdcwin_putat_chars_raw(struct VDCWin*, char x, char y, const char*, char num);
char  vdcwin_putat_string_raw(struct VDCWin*, char x, char y, const char*);
```

### Input from Position

```c
char vdcwin_getat_char(struct VDCWin*, char x, char y);     // Returns PETSCII
char vdcwin_getat_char_raw(struct VDCWin*, char x, char y); // Returns screencode
void vdcwin_getat_chars(struct VDCWin*, char x, char y, char *buf, char num);
void vdcwin_getat_chars_raw(struct VDCWin*, char x, char y, char *buf, char num);
```

### Rectangle Operations

Banking version (source/dest data in a specific bank, `cr` = MMU CR value):
```c
void vdcwin_put_rect_raw(struct VDCWin*, char x, char y, char w, char h, char cr, const char*);
void vdcwin_put_rect(struct VDCWin*, char x, char y, char w, char h, char cr, const char*);
void vdcwin_get_rect_raw(struct VDCWin*, char x, char y, char w, char h, char cr, char*);
void vdcwin_get_rect(struct VDCWin*, char x, char y, char w, char h, char cr, char*);
```

nobnk version (no `cr` parameter, data always in bank 0):
```c
void vdcwin_put_rect_raw(struct VDCWin*, char x, char y, char w, char h, const char*);
void vdcwin_get_rect_raw(struct VDCWin*, char x, char y, char w, char h, char*);
// etc.
```

`_raw` = screencode data. Non-`_raw` = PETSCII (auto-converted on transfer).

### Bulk Text I/O

```c
void vdcwin_read_string(struct VDCWin *win, char *buffer);
// Read full window content as PETSCII string. Trailing spaces stripped. NUL-terminated.

void vdcwin_write_string(struct VDCWin *win, const char *buffer);
// Fill window row-by-row from PETSCII string. Pads remaining cells with spaces.

void vdcwin_printline(struct VDCWin *win, const char *str);
// Print string, advance to next line. Scrolls up if at last row.

void vdcwin_printwrap(struct VDCWin *win, const char *str);
// Print with word wrap (adapted from C3L by Steve Goldsmith).
```

### Keyboard Input

```c
int vdcwin_getch(void);    // Blocking: wait for key, return PETSCII (Kernal GETIN $FFE4)
int vdcwin_checkch(void);  // Non-blocking: return key if pending, else 0
```

### Edit Functions

```c
bool vdcwin_edit_char(struct VDCWin *win, char ch);
// Process one PETSCII character for text editing.
// Returns true when editing should stop (RETURN=13, STOP=3, ESC=27).
// Handles: cursor keys, HOME(19), CLR(147), DEL(20), printable chars with insert.

char vdcwin_edit(struct VDCWin *win);
// Interactive edit loop with blinking cursor. Returns terminating key.
```

### Window Scroll (Content Shift)

```c
void vdcwin_scroll_left(struct VDCWin *win, char by);
void vdcwin_scroll_right(struct VDCWin *win, char by);
void vdcwin_scroll_up(struct VDCWin *win, char by);
void vdcwin_scroll_down(struct VDCWin *win, char by);
```

Shift existing content only; vacated space is not filled.

H-scroll (`left`/`right`) uses VDC swap memory when available (64KB mode, 80x50+); falls back to `linebuffer` via bank 0 for 16KB or 80x25 mode. `scroll_left` wraps inside VBLANK.

### Fill and Border

```c
void vdcwin_fill_rect(struct VDCWin *win, char x, char y, char w, char h, char ch);
// PETSCII ch, current text_attr.

void vdcwin_fill_rect_raw(struct VDCWin *win, char x, char y, char w, char h, char ch);
// Screencode ch, current text_attr.
```

Border flags — OR combination for `border` parameter:
```c
WIN_BOR_UP    0x80  // top
WIN_BOR_LE    0x40  // left
WIN_BOR_RI    0x20  // right
WIN_BOR_BO    0x10  // bottom
WIN_BOR_ALL   0xF0  // all four
WIN_BOR_NOTOP 0x70  // all except top
// Bits 0-3 = style index (0 = dark-red box, 1 = yellow lines)
```

```c
void vdcwin_border_clear(struct VDCWin *win, char border);
// Draw borders and clear interior. Borders drawn outside window bounds.
// If no room (window at edge), that border is silently skipped.
```

### Popup Windows (Background Save)

```c
void vdcwin_winstorage_init(char memcr, char *membase, unsigned memsize);
// Initialize background save arena. Call once before any vdcwin_win_new.
// memcr: MMU CR for the RAM area. membase/memsize: the save buffer.

bool vdcwin_win_new(char border, char xpos, char ypos, char width, char height);
// Save background, clear window, draw border. Returns false if memory full.
// Max WIN_MAX_NR (3) simultaneous windows.

void vdcwin_win_free();
// Restore background of the most-recently-opened window (LIFO — no index).
```

Active window: `windows[winCfg.active-1].win` is the `VDCWin` of the top window.

### Viewport (Character-Resolution Scroll)

Banking version:
```c
void vdcwin_viewport_init(struct VDCViewport *vp,
    char sourcebank, char *sourcebase,
    unsigned sourcewidth, unsigned sourceheight,
    unsigned viewwidth, unsigned viewheight,
    char viewsx, char viewsy);

void vdcwin_cpy_viewport(struct VDCViewport *viewport);
// Blit current viewport from source memory to VDC display.

void vdcwin_viewportscroll(struct VDCViewport *viewport, char direction);
// Scroll by one character in direction. direction = SCROLL_LEFT/RIGHT/UP/DOWN.
// Shifts existing VDC content, then fills the exposed edge from source.
```

nobnk version: same but no `sourcebank` parameter.

Scroll direction constants (defined in `banking.h` / `vdcwin_nobnk.h`):
```c
SCROLL_LEFT  0x01
SCROLL_RIGHT 0x02
SCROLL_DOWN  0x04
SCROLL_UP    0x08
```

### Soft Scroll (nobnk only — sub-character pixel scrolling)

Uses VDC hardware VSCROLL/HSCROLL registers and the ROWINC feature. Requires 64KB VDC for H-scroll (virtual screen must fit with charsets).

```c
char vdc_softscroll_init(struct VDCSoftScrollSettings *settings, char mode);
// Copy source to VDC virtual screen, set ROWINC for wider-than-display virtual width.
// Returns 0 if source too large for available VDC RAM.
// Charsets placed at 0x0000/0x1000; display starts at 0x2000.

void vdc_softscroll_exit(struct VDCSoftScrollSettings *settings, char mode);
// Restore VDC to normal (zero ROWINC, restore VSCROLL/HSCROLL, re-init mode).

void vdc_softscroll_down(struct VDCSoftScrollSettings *settings, char step);
void vdc_softscroll_up(struct VDCSoftScrollSettings *settings, char step);
// Vertical scroll by `step` pixels (typical step=2). Adjusts VDCR_VSCROLL.
// Advances display address when vscroll wraps past 8.

void vdc_softscroll_right(struct VDCSoftScrollSettings *settings, char step);
void vdc_softscroll_left(struct VDCSoftScrollSettings *settings, char step);
// Horizontal scroll by `step` pixels. Adjusts VDCR_HSCROLL.
// Advances display address when hscroll wraps past 8.
// All H-scroll operations sync to VBLANK.
```

---

## vdc_menu

### Include

```c
#include "vdc_menu.h"   // requires vdc_core.h + vdc_win.h + defines.h
```

### Configuration (compile-time)

```c
VDC_MENUBAR_MAXOPTIONS  5   // options in main menu bar
VDC_MENUBAR_MAXLENGTH   12  // max chars per menu bar option name
VDC_PULLDOWN_NUMBER     8   // number of pull-down menus defined
VDC_PULLDOWN_MAXOPTIONS 6   // options per pull-down
VDC_PULLDOWN_MAXLENGTH  17  // max chars per pull-down option (one slot of slack)
```

Keep constants at their minimum needed values — array sizes are the product of all three dimensions, so every unused slot wastes memory.

### Border Mode

Define or leave undefined to select border rendering:

```c
// In vdc_menu.h (or pass via compiler flag -dVDC_MENU_BORDERS):
// #define VDC_MENU_BORDERS
```

| State | Effect |
|---|---|
| Undefined (default) | No visible borders — required when the user can reload custom charsets, because box-drawing screencodes become corrupt after a charset swap |
| `#define VDC_MENU_BORDERS` | Box-drawing borders via `WIN_BOR_ALL` / `WIN_BOR_NOTOP` (Oscar64Test default) |

Can also be toggled at compile time without editing the header: `-dVDC_MENU_BORDERS` on the oscar64 command line.

### Data Tables (must be populated by caller)

```c
struct VDCMenuBar menubar;   // titles[], xstart[], ypos
char pulldown_options[VDC_PULLDOWN_NUMBER];   // option count per pull-down
char pulldown_titles[VDC_PULLDOWN_NUMBER][VDC_PULLDOWN_MAXOPTIONS][VDC_PULLDOWN_MAXLENGTH];
```

Entries in `pulldown_titles` that display runtime values (screen dimensions, current settings) are updated via `sprintf()` before `menu_main()` is called. Strings are padded with spaces to fill `VDC_PULLDOWN_MAXLENGTH` so the pulldown column width stays fixed.

**Shortcut hint convention:** embed keyboard hints using `[X]` notation in item strings where a direct key shortcut also exists in the main loop:

```c
"[S]ave screen  "   // S shortcut active in main loop
"[L]oad screen  "
"Version/credits"   // no shortcut — no brackets
```

The menu system does not parse these brackets; they are display-only hints for the user.

Color globals (set before use):
```c
char mc_mb_normal;    // menu bar normal item color
char mc_mb_select;    // menu bar selected item color
char mc_pd_normal;    // pull-down normal item color
char mc_pd_select;    // pull-down selected item color
char mc_menupopup;    // popup message color
```

### Functions

```c
void menu_placeheader(const char *header);
// Print header string in menubar style at top of screen (row 0).

void menu_placebar(char y);
// Draw the full menu bar at row y. Calculates and stores xstart[] positions.

void menu_placetop(const char *header);
// Combined: cls + draw header at row 0 + draw menu bar at row 1.

char menu_pulldown(char xpos, char ypos, char menunumber, unsigned char escapable);
// Show pull-down menu at (xpos,ypos). Returns selected option (1-based),
// 0=escaped, 18=left-arrow (caller should open previous bar item),
// 19=right-arrow (caller should open next bar item).
// escapable: non-zero = allow ESC/STOP to cancel.

char menu_main();
// Interactive main menu bar + pull-down navigation.
// Returns menubarchoice*10 + menuoptionchoice (e.g. 13 = bar item 1, option 3).
// Returns 99 on ESC/STOP from the bar level.

char menu_areyousure(const char *message);
// Popup with message + "Are you sure?" + Yes/No pulldown (menu index VDC_MENU_YESNO).
// Returns 1=Yes, 2=No.

void menu_fileerrormessage();
// Popup showing "File error!" and the Kernal I/O status byte (krnio_pstatus[1]) in hex.
// Waits for keypress.

void menu_messagepopup(const char *message);
// Popup showing message, waits for keypress.

char menu_option_select(const char *message, char menunumber);
// Generic labeled submenu dialog: popup with message header above any named pulldown.
// Useful for presenting a choice with context — e.g. "Select import mode:" + a 3-option menu.
// Returns pulldown result (1-N selected, 0 if cancelled with ESC).
// Uses 2 of the 3 WIN_MAX_NR window slots (same budget as menu_areyousure).
```

---

## banking / bank_minimal

### Architecture

All banking functions run from the **shared low memory region 0x1300–0x1B00**, mapped as common RAM between both banks via MMU `rcr`. This allows the same function to execute regardless of which bank the main program is in.

`bnk_init()` sets `xmmu.rcr = 0x06` (8KB shared bottom RAM) and loads the `vdcselmc` overlay which places the banking routines at 0x1300. This must be called before any `bnk_*` function.

All functions that live in low memory are `__noinline`. If Oscar64 were to inline them, the code would end up in a segment that may not be visible from both banks.

### MMU Configuration Constants

```c
BNK_DEFAULT  0x0e  // Bank 0, I/O at $D000, Kernal + Basic ROM
BNK_CHARROM  0x01  // Bank 0, Character ROM at $D000 (charset copy from ROM)
BNK_0_FULL   0x3f  // Bank 0, full RAM (no ROM)
BNK_1_FULL   0x7f  // Bank 1, full RAM
BMK_0_IO     0x3e  // Bank 0 + I/O
BNK_1_IO     0x7e  // Bank 1 + I/O (used by SID player)
```

### Init / Exit

```c
void bnk_init();
// Detect boot device ($BA → bootdevice), set MMU rcr=0x06 (8KB shared),
// load vdcselmc overlay into shared low memory.

void bnk_exit();
// Restore MMU rcr=0x04 (1KB shared, C128 default).

char getcurrentdevice();
// Return current IEC device ($BA). Returns 8 if $BA is zero.
```

`vdc_exit()` calls `bnk_exit()` — don't call both.

### Read / Write

```c
__noinline char bnk_readb(char cr, volatile char *p);
__noinline unsigned bnk_readw(char cr, volatile unsigned *p);       // full banking.h only
__noinline unsigned long bnk_readl(char cr, volatile unsigned long *p); // full only
__noinline void bnk_writeb(char cr, volatile char *p, char b);
__noinline void bnk_writew(char cr, volatile unsigned *p, unsigned w);  // full only
__noinline void bnk_writel(char cr, volatile unsigned long *p, unsigned long l); // full only
```

`cr` = MMU CR value. These save/restore the CR around the access. Use `BNK_1_FULL` (0x7f) to access Bank 1.

### Block Memory Operations

```c
__noinline void bnk_memcpy(char dcr, volatile char *dp, char scr, volatile char *sp, unsigned size);
// Copy between two banks. src and dst each have their own CR.

__noinline void bnk_memset(char cr, volatile char *p, char val, unsigned size);
// Fill memory in a specific bank.

__noinline void bnk_cpytovdc(unsigned vdcdest, char scr, volatile char *sp, unsigned size);
// Copy from bank RAM to VDC. (full banking.h only)

__noinline void bnk_cpyfromvdc(char dcr, volatile char *dp, unsigned vdcsrc, unsigned size);
// Copy from VDC to bank RAM. (full banking.h only)

__noinline void bnk_redef_charset(unsigned vdcdest, char scr, volatile char *sp, unsigned size);
// Copy charset from RAM/ROM to VDC. (full banking.h only)
// source: 8 bytes/char. VDC format: 16 bytes/char (8 data + 8 zero padding).
// size = number of characters (512 = full ROM charset copy).
```

### File I/O

```c
__noinline bool bnk_load(char device, char bank, const char *start, const char *fname);
// Load file to bank at start address. bank: 0 or 1. Uses Kernal LOAD ($FFD5).

__noinline bool bnk_save(char device, char bank, const char *start, const char *end, const char *fname);
// Save memory range from bank. Uses krnio_save.

__noinline int bnk_io_read(char fnum, char cr, char *data, int num);
// Sequential read from open Kernal file (fnum) into bank/address. Returns bytes read or -1.
// (full banking.h only)
```

### Overlay Loading

```c
void load_overlay(const char *fname);
// Load PRG from bootdevice at its native load address (krnio_load with SA=1).
// Calls exit(1) on failure with error message.
```

### Directory (full banking.h only)

```c
__noinline char dir_open(char lfn, unsigned char device);
// Open "$" for reading. Returns 0=ok, non-zero=error.

__noinline void dir_close(char lfn);

__noinline char dir_readentry(const char lfn, struct DirEntry *l_dirent);
// Read next entry. Returns 0=ok, 1=end of dir.

void freeDir();
// Free the cwd linked list.

__noinline char dir_validentry(char filter);
// Test current entry against filter: 0=all PRG, 1=.proj files, 2=SEQ files.

__noinline const char *fileTypeToStr(char ft);
// Returns "PRG", "SEQ", "USR", etc.
```

```c
struct DirEntry {
    char name[17];    // NUL-terminated, max 16 chars, PetSCII
    unsigned size;    // blocks
    char type;        // CBM_T_PRG(0x11), CBM_T_SEQ(0x10), CBM_T_USR(0x12),
                      // CBM_T_REL(0x13), CBM_T_DEL(0x00), CBM_T_DIR(0x02),
                      // CBM_T_HEADER(0x05), CBM_T_FREE(100)
    char access;      // CBM_A_RO(1), CBM_A_RW(3)
};
```

### IEC Presence

```c
__noinline bool bnk_iec_active(char device);
// LISTEN + SECOND to device; returns true if it responds.
```

### SID Music Player (full banking.h only)

Assumes a SID file at Bank 1 `$2000`: init=`$2000`, play=`$2003`, zero page `$FC–$FE`.

```c
__noinline void sid_startmusic();
// Hook IRQ vector to sid_interrupt, call SID init at $2000 in Bank 1.

__noinline void sid_stopmusic();
// Restore original IRQ vector, silence SID.

__noinline void sid_resetsid();
// Zero all SID registers, wait 3 raster frames to silence voices.

__noinline void sid_pausemusic();
// Toggle sid_pause flag. Music stops playing but IRQ continues.
```

### bank_minimal.h

Stripped-down version for utilities that don't need directory, VDC copy, or SID. Exports only: `bnk_readb`, `bnk_writeb`, `bnk_memcpy`, `bnk_memset`, `bnk_load`, `bnk_save`.

---

## Key Gotchas

1. **`__noinline` on all bnk_ functions** — they live in shared low memory at 0x1300. Oscar64 must not inline them or the code ends up in the wrong segment.

2. **80x50 PAL extmem difference** — `vdc_core.c` marks it `extmem=1` (needs 64KB VDC), `vdc_nobnk.c` marks it `extmem=0`. Mixing the two in the same project would be wrong.

3. **Block fill/copy length is zero-based** — 0=1 byte, 255=256 bytes. Off-by-one is a common mistake.

4. **`bnk_redef_charset` expands 8→16 bytes per char** — VDC charset slots are 16 bytes (8 pixel rows + 8 zero bytes). The ROM charset is 8 bytes/char. This expansion happens automatically inside `bnk_redef_charset`.

5. **H-scroll fallback for 16KB VDC** — `vdcwin_scroll_left/right` uses swap VDC memory when in 64KB mode; falls back to `linebuffer` (81-byte, bank 0) for 16KB mode, limiting horizontal scroll to 80 characters per call.

6. **Popup window LIFO** — `vdcwin_win_free()` always frees the topmost window. No random-access close.

7. **Viewport source layout has 48-byte gap** — attributes start at `sourcebase + sourcewidth * sourceheight + 48`, not immediately after screencodes. This matches the VDCSEv2 screen map format.

8. **`fastmode(1)` blanks the VIC screen** — required to avoid bus conflicts at 2MHz. The VIC goes dark; the VDC continues running. Restore with `fastmode(0)`.

9. **`vdc_exit()` already calls `bnk_exit()`** — don't call both.

10. **Soft scroll needs VDC RAM headroom** — `vdc_softscroll_init` returns 0 if the virtual screen + charsets won't fit. On 16KB VDC only 8192 bytes are available for the virtual screen (minus charset space at 0x0000/0x1000).

11. **`vdc_fgcolor()`'s reach depends on attribute mode.** Register 26's high nibble is always the *physical border* colour, but whether it *also* drives per-character text colour depends on which text mode is active (`colorlines` field in the mode table): in an attribute-mode text mode (e.g. `VDC_TEXT_80x25_PAL`, colorlines=8), each character's own colour-RAM entry (`vdc_state.text_attr` / `vdc_prints_attr()`'s explicit `attr` byte) overrides the foreground nibble for that character — so `vdc_fgcolor(VDC_BLACK)` blacks only the border, text stays whatever colour its own attribute says. In a non-attribute text mode (e.g. `VDC_TEXT_80x25_Mono_PAL`, colorlines=0 — used by `idi8b_logo_demo()`'s raster-over-text trick specifically because of this), there is no per-character colour RAM at all: the foreground nibble is the *only* colour for every character on screen, so `vdc_fgcolor()` recolors border and text together. Confirmed live in `vdcmaniac`: a black-border/readable-text intro screen needs attribute mode on, not off.

12. **`vdc_hchar()` length 1 and `vdcwin_put_rect_raw()` attributes (fixed 2026-09-25).** `vdc_block_fill(address, value, length)` writes one byte and then a block of `length` more (VDC register 30 is a zero-based count, and 0 means 256). `vdc_hchar()` passed `length - 1`, so a line of length 1 filled 257 text and attribute positions, and length 0 underflowed. Now length 0 does nothing and length 1 uses `vdc_printc()`. `vdcwin_put_rect_raw()` passed `w` instead of `w - 1` and filled one attribute byte too many per row. Both fixed in `vdc_core`/`vdc_nobnk` and `vdc_win`/`vdcwin_nobnk`. VDC Screen Editor 2 itself never hit them: all its `vdc_hchar()` calls use widths far above 1 (vertical borders are drawn with `vdc_printc()`), and it never calls `vdcwin_put_rect`. Found by DMBoot v5's DualWin library, which draws one-character-wide borders with `vdcwin_fill_rect()`.

---

## Minimal Usage Pattern

```c
#include "vdc_core.h"
#include "vdc_win.h"
#include "banking.h"

int main(void)
{
    bnk_init();
    vdc_init(VDC_TEXT_80x25_PAL, 0);

    // Init popup arena (Bank 1 at 0x2000, 8KB)
    vdcwin_winstorage_init(BNK_1_FULL, (char *)0x2000, 0x2000);

    // Popup dialog
    if (vdcwin_win_new(WIN_BOR_ALL, 10, 5, 40, 10)) {
        struct VDCWin *w = &windows[winCfg.active - 1].win;
        vdcwin_putat_string(w, 0, 0, "Hello VDC!");
        vdcwin_edit(w);
        vdcwin_win_free();
    }

    vdc_exit();
    return 0;
}
```
