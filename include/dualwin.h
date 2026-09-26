/*
DualWin - 40/80 column character window library for the Commodore 128

Written in 2026 by Xander Mol
https://github.com/xahmol/DMBoot
https://www.idreamtin8bits.com/

One window API for both C128 text screens. The backend is chosen at run
time from the active screen:
- 40 columns (VIC-IIe): Oscar64's CharWin (<c64/charwin.h>)
- 80 columns (VDC 8563): the VDC window layer vdc_win / vdc_core of the VDC
  library suite of my VDC Screen Editor 2 project
  (https://github.com/xahmol/VDCScreenEditor2)

Code and resources from others used:
-   Oscar64 cross compiler and its CharWin library
    https://github.com/drmortalwombat/oscar64

Design (see DUALWINMANUAL.md for the full manual):
- Colours are logical C64/VIC colour numbers (0-15, VCOL_* in <c64/vic.h>).
  In 80 columns they are translated to VDC colours by dwin_vdc_colors[],
  which an application may change.
- Characters and strings are PETSCII (as produced with <petscii.h>).
- Every output function clips to the window: nothing is ever written
  outside it, whatever the string length.
- The library keeps its own cursor, so console output (newline, scroll)
  behaves the same on both screens.
- Popups save the screen area they cover (text and colour/attribute
  memory, including a one-character border) in a caller-supplied banked
  RAM area and restore it on close, last opened first closed.
- Keys are read raw with KERNAL GETIN (keyboard buffer at $034A), so
  keys injected into the buffer by a test harness work too.

Requires the banking layer (banking.h): bnk_memcpy, bnk_cpytovdc and
bnk_cpyfromvdc, which must be resident in common RAM.
*/

#ifndef DUALWIN_H
#define DUALWIN_H

#include <c64/charwin.h>
#include "vdc_core.h"
#include "vdc_win.h"

// Screen modes
#define DWIN_MODE_VIC       0       // 40 column VIC-IIe screen
#define DWIN_MODE_VDC       1       // 80 column VDC screen

// Limits
#define DWIN_LINE_MAX       80      // Longest possible window line
#define DWIN_PRINTF_MAX     200     // Buffer for dwin_printf output (see manual)
#define DWIN_POPUP_MAX      4       // Maximum number of open popups
#define DWIN_COLORS         16      // Number of logical colours

// Special key codes returned by dwin_getch / dwin_input
#define DWIN_KEY_NONE       0x00
#define DWIN_KEY_RETURN     0x0d
#define DWIN_KEY_STOP       0x03
#define DWIN_KEY_DEL        0x14
#define DWIN_KEY_LEFT       0x9d
#define DWIN_KEY_RIGHT      0x1d
#define DWIN_KEY_HOME       0x13

// Return value of dwin_input when the input was cancelled
#define DWIN_INPUT_CANCEL   -1

// A character window on either screen
struct DWin
{
    char sx, sy;            // Top left position on the screen
    char wx, wy;            // Width and height in characters
    char cx, cy;            // Cursor position inside the window
    union
    {
        CharWin vicwin;     // Backend window, 40 column mode (not "vic": macro in <c64/vic.h>)
        struct VDCWin vdcwin; // Backend window, 80 column mode (not "vdc": macro in <c128/vdc.h>)
    } backend;
};

// Saved screen area of an open popup
struct DWinPopup
{
    char x, y, w, h;        // Saved area on the screen (including border)
    char *store;            // Start of the saved data in the storage bank
};

// Library state
struct DWinState
{
    char mode;              // DWIN_MODE_VIC or DWIN_MODE_VDC
    char width;             // Screen width in characters (40 or 80)
    char height;            // Screen height in characters
    char pal;               // 1 = PAL, 0 = NTSC
    char storecr;           // MMU $FF00 value of the popup storage area
    char *storebase;        // Start of the popup storage area
    unsigned storesize;     // Size of the popup storage area in bytes
    char popups;            // Number of open popups
    struct DWinPopup popup[DWIN_POPUP_MAX];
};

extern struct DWinState dwin_state;
extern char dwin_vdc_colors[DWIN_COLORS];

// Set-up
void dwin_setup(char storecr, char *storebase, unsigned storesize);
void dwin_screen_colors(char border, char background);
bool dwin_is80(void);
void dwin_swap_screen(void);
void dwin_vic_charset(bool lower);
void dwin_exit(void);

// Windows
void dwin_init(struct DWin *win, char sx, char sy, char wx, char wy);
void dwin_clear(struct DWin *win);
void dwin_fill_rect(struct DWin *win, char x, char y, char w, char h, char ch, char color);
void dwin_scroll_up(struct DWin *win, char color);

// Output
void dwin_putat_char(struct DWin *win, char x, char y, char ch, char color);
char dwin_putat_string(struct DWin *win, char x, char y, const char *str, char color);
char dwin_putat_string_reverse(struct DWin *win, char x, char y, const char *str, char color);
void dwin_reverse_rect(struct DWin *win, char x, char y, char w, char h);

// Cursor and console output
void dwin_cursor_move(struct DWin *win, char cx, char cy);
void dwin_cursor_newline(struct DWin *win, char color);
void dwin_cursor_show(struct DWin *win, bool show);
void dwin_put_char(struct DWin *win, char ch, char color);
void dwin_put_string(struct DWin *win, const char *str, char color);
void dwin_printf(struct DWin *win, char color, const char *fmt, ...);

// Popups
bool dwin_popup_open(struct DWin *win, char x, char y, char w, char h, char bordercolor, char color);
void dwin_popup_close(void);

// Keyboard
char dwin_getch(void);
char dwin_checkch(void);
int dwin_input(struct DWin *win, char x, char y, char *buffer, char size, char width, char color);

#pragma compile("dualwin.c")

#endif // DUALWIN_H
