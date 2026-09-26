/*
DualWin - 40/80 column character window library for the Commodore 128

Written in 2026 by Xander Mol
https://github.com/xahmol/DMBoot
https://www.idreamtin8bits.com/

Code and resources from others used:
-   Oscar64 cross compiler and its CharWin library
    https://github.com/drmortalwombat/oscar64
-   DraCopy / DraBrowse text input routine, Sascha Bader (2009), version
    adapted by Dirk Jagdmann (doj), https://github.com/doj/dracopy
    (used by dwin_input, see there)
*/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <petscii.h>
#include <c64/vic.h>
#include <c128/vdc.h>
#include "banking.h"
#include "dualwin.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// C128 system locations
#define DWIN_ZP_MODE        0xd7    // Bit 7 set: 80 column screen active
#define DWIN_MODE_80_FLAG   0x80
#define DWIN_SWAPPER        0xff5f  // KERNAL SWAPPER: switch the active screen
#define DWIN_PALNTSC        0x0a03  // KERNAL PAL/NTSC flag, $FF = PAL
#define DWIN_PAL_FLAG       0xff
#define DWIN_VIC_SCREEN     0x0400  // VIC text screen
#define DWIN_VIC_COLORRAM   0xd800  // VIC colour RAM
#define DWIN_VIC_WIDTH      40
#define DWIN_VDC_WIDTH      80
#define DWIN_SCREEN_HEIGHT  25

// Characters
#define DWIN_CHR_LOWERCASE  0x0e    // CHROUT: switch to lower/upper case charset
#define DWIN_CHR_UPPERCASE  0x8e    // CHROUT: switch to upper case/graphics charset
#define DWIN_CHR_SPACE      0x20
#define DWIN_CHR_INSERT     0x94    // Shift-DEL
#define DWIN_CHR_NEWLINE_LF 0x0a
#define DWIN_CHR_NEWLINE_CR 0x0d
#define DWIN_VIC_REVERSE    0x80    // Reverse bit in a VIC screen code

// PETSCII box drawing characters for popup borders
#define DWIN_BOX_UL         0xb0
#define DWIN_BOX_UR         0xae
#define DWIN_BOX_LL         0xad
#define DWIN_BOX_LR         0xbd
#define DWIN_BOX_H          0xc0
#define DWIN_BOX_V          0xdd

// Popups need at least a border and one inner character
#define DWIN_POPUP_MIN      3

// Library state
struct DWinState dwin_state;

// Logical (C64/VIC) colour number -> VDC colour. May be changed by the
// application; index = VCOL_* value.
char dwin_vdc_colors[DWIN_COLORS] = {
    VDC_BLACK,   // VCOL_BLACK
    VDC_WHITE,   // VCOL_WHITE
    VDC_DRED,    // VCOL_RED
    VDC_LCYAN,   // VCOL_CYAN
    VDC_LPURPLE, // VCOL_PURPLE
    VDC_DGREEN,  // VCOL_GREEN
    VDC_DBLUE,   // VCOL_BLUE
    VDC_LYELLOW, // VCOL_YELLOW
    VDC_DYELLOW, // VCOL_ORANGE
    VDC_DYELLOW, // VCOL_BROWN
    VDC_LRED,    // VCOL_LT_RED
    VDC_DGREY,   // VCOL_DARK_GREY
    VDC_LGREY,   // VCOL_MED_GREY
    VDC_LGREEN,  // VCOL_LT_GREEN
    VDC_LBLUE,   // VCOL_LT_BLUE
    VDC_LGREY    // VCOL_LT_GREY
};

// ===========================================================================
// Internal helpers
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       KERNAL character out
// Description: Sends one character to the active screen via KERNAL CHROUT.
// Syntax:      void dwin_chrout(char ch);
// Input:       ch - PETSCII character or control code
// Output:      None
// ---------------------------------------------------------------------------
static void dwin_chrout(char ch)
{
    __asm
    {
        lda ch
        jsr $ffd2
    }
}

// ---------------------------------------------------------------------------
// Title:       Set VDC text attribute
// Description: Sets the VDC attribute used by the VDC window functions for
//              a logical colour, with the alternate (lower case) charset.
// Syntax:      void dwin_vdc_attr(char color);
// Input:       color - logical colour 0-15 (higher bits are ignored)
// Output:      None (vdc_state.text_attr)
// ---------------------------------------------------------------------------
static void dwin_vdc_attr(char color)
{
    vdc_state.text_attr = dwin_vdc_colors[color & (DWIN_COLORS - 1)] | VDC_A_ALTCHAR;
}

// ---------------------------------------------------------------------------
// Title:       Clip a width to a window
// Description: Limits a number of characters starting at (x, y) so that it
//              stays inside the window.
// Syntax:      char dwin_clip(const struct DWin *win, char x, char y,
//                             char len);
// Input:       win - window
//              x, y - start position inside the window
//              len  - requested number of characters
// Output:      Number of characters that fit (0 when x or y is outside)
// ---------------------------------------------------------------------------
static char dwin_clip(const struct DWin *win, char x, char y, char len)
{
    if (x >= win->wx || y >= win->wy)
    {
        return 0;
    }

    char available = win->wx - x;
    return len < available ? len : available;
}

// ---------------------------------------------------------------------------
// Title:       Bounded string length
// Description: Returns the length of a string, but never more than a limit,
//              so an unterminated or very long string is never over-read
//              beyond what will be printed.
// Syntax:      char dwin_strnlen(const char *str, char limit);
// Input:       str   - string
//              limit - maximum length to report
// Output:      min(strlen(str), limit)
// ---------------------------------------------------------------------------
static char dwin_strnlen(const char *str, char limit)
{
    char len = 0;
    while (len < limit && str[len])
    {
        len++;
    }
    return len;
}

// ---------------------------------------------------------------------------
// Title:       Put characters (backend)
// Description: Writes PETSCII characters at a window position on the active
//              backend. The caller has already clipped num.
// Syntax:      void dwin_putat_chars(struct DWin *win, char x, char y,
//                                    const char *chars, char num, char color);
// Input:       win   - window
//              x, y  - position inside the window
//              chars - PETSCII characters
//              num   - number of characters (already clipped)
//              color - logical colour
// Output:      None
// ---------------------------------------------------------------------------
static void dwin_putat_chars(struct DWin *win, char x, char y, const char *chars, char num, char color)
{
    if (!num)
    {
        return;
    }

    if (dwin_state.mode == DWIN_MODE_VDC)
    {
        dwin_vdc_attr(color);
        vdcwin_putat_chars(&win->backend.vdcwin, x, y, chars, num);
    }
    else
    {
        cwin_putat_chars(&win->backend.vicwin, x, y, chars, num, color);
    }
}

// ---------------------------------------------------------------------------
// Title:       Set or clear reverse on one character
// Description: Sets or clears the reverse state of the character at a
//              window position (VIC: bit 7 of the screen code, VDC: the
//              reverse attribute bit).
// Syntax:      void dwin_set_reverse(struct DWin *win, char x, char y,
//                                    bool reverse);
// Input:       win     - window
//              x, y    - position inside the window (must be inside)
//              reverse - true to set, false to clear
// Output:      None
// ---------------------------------------------------------------------------
static void dwin_set_reverse(struct DWin *win, char x, char y, bool reverse)
{
    if (dwin_state.mode == DWIN_MODE_VDC)
    {
        unsigned address = vdc_state.base_attr + vdc_coords(win->sx + x, win->sy + y);
        char attr = vdc_mem_read_at(address);
        attr = reverse ? (attr | VDC_A_REVERSE) : (attr & ~VDC_A_REVERSE);
        vdc_mem_write_at(address, attr);
    }
    else
    {
        char *sp = win->backend.vicwin.sp + (unsigned)y * DWIN_VIC_WIDTH + x;
        *sp = reverse ? (*sp | DWIN_VIC_REVERSE) : (*sp & ~DWIN_VIC_REVERSE);
    }
}

// ---------------------------------------------------------------------------
// Title:       Initialise the VDC state only
// Description: Fills vdc_state (addresses and sizes of the 80x25 screen the
//              KERNAL has already set up) for the VDC window functions,
//              WITHOUT writing any VDC register. Reprogramming the VDC
//              (vdc_set_mode) shifted the picture after the exit to BASIC,
//              and VDC registers cannot be saved first: many are write-only
//              and read back as $FF (seen on hardware, 2026-09-25).
// Syntax:      void dwin_vdc_state_init(char mode);
// Input:       mode - VDC_TEXT_80x25_PAL or VDC_TEXT_80x25_NTSC
// Output:      None (vdc_state, multiplication table)
// ---------------------------------------------------------------------------
static void dwin_vdc_state_init(char mode)
{
    vdc_state.mode = mode;
    vdc_state.width = vdc_modes[mode].width;
    vdc_state.height = vdc_modes[mode].height;
    vdc_state.base_text = vdc_modes[mode].base_text;
    vdc_state.base_attr = vdc_modes[mode].base_attr;
    vdc_state.swap_text = vdc_modes[mode].swap_text;
    vdc_state.swap_attr = vdc_modes[mode].swap_attr;
    vdc_state.char_std = vdc_modes[mode].char_std;
    vdc_state.char_alt = vdc_modes[mode].char_alt;
    vdc_state.extended = vdc_modes[mode].extended;
    vdc_state.text_attr = VDC_LYELLOW + VDC_A_ALTCHAR;
    vdc_state.dispaddr_offset = 0;
    vdc_state.disp_skip = 0;
    vdc_set_multab();
}

// ===========================================================================
// Set-up
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Active KERNAL screen
// Description: Reads which screen the KERNAL screen editor has active.
// Syntax:      static char dwin_kernal_mode(void);
// Input:       None
// Output:      DWIN_MODE_VDC or DWIN_MODE_VIC
// ---------------------------------------------------------------------------
static char dwin_kernal_mode(void)
{
    return (*(volatile char *)DWIN_ZP_MODE & DWIN_MODE_80_FLAG) ? DWIN_MODE_VDC : DWIN_MODE_VIC;
}

// ---------------------------------------------------------------------------
// Title:       KERNAL screen swap
// Description: Calls the KERNAL SWAPPER, which switches the screen editor
//              to the other screen. SWAPPER exchanges the editor variables
//              $E0-$F9 with its store at $0A40; $F7-$F9 are also Oscar64's
//              automatic zero page, so they are kept around the call.
// Syntax:      static void dwin_swapper(void);
// Input:       None
// Output:      None (zero page $D7 bit 7 toggled)
// ---------------------------------------------------------------------------
static void dwin_swapper(void)
{
    __asm
    {
        lda $f7
        pha
        lda $f8
        pha
        lda $f9
        pha
        jsr DWIN_SWAPPER
        pla
        sta $f9
        pla
        sta $f8
        pla
        sta $f7
    }
}

// ---------------------------------------------------------------------------
// Title:       Set up DualWin
// Description: Detects the active screen (40 or 80 columns) and PAL/NTSC,
//              switches to the lower/upper case charset, initialises the
//              VDC state, switches a 64 KB VDC to 64 KB addressing (needs
//              the LMC loaded) and registers the banked RAM
//              area used to save popup backgrounds.
// Syntax:      void dwin_setup(char storecr, char *storebase,
//                              unsigned storesize);
// Input:       storecr   - MMU $FF00 value for the storage (e.g. BNK_1_FULL)
//              storebase - start address of the storage area
//              storesize - size of the storage area in bytes
// Output:      None (dwin_state)
// ---------------------------------------------------------------------------
void dwin_setup(char storecr, char *storebase, unsigned storesize)
{
    dwin_state.mode = dwin_kernal_mode();
    dwin_state.pal = (*(volatile char *)DWIN_PALNTSC == DWIN_PAL_FLAG) ? 1 : 0;
    dwin_state.height = DWIN_SCREEN_HEIGHT;
    dwin_state.storecr = storecr;
    dwin_state.storebase = storebase;
    dwin_state.storesize = storesize;
    dwin_state.popups = 0;

    // The KERNAL switches the charset of the active screen
    dwin_chrout(DWIN_CHR_LOWERCASE);

    // VDC state in both modes: the memory size switch below clears the
    // VDC screen through it
    dwin_vdc_state_init(dwin_state.pal ? VDC_TEXT_80x25_PAL : VDC_TEXT_80x25_NTSC);

    // 64 KB VDC RAM: switch the VDC to 64 KB addressing (register 28 bit
    // 4). With 64 KB chips in 16 KB mode the screen was corrupted on
    // hardware (SaRuMan, header and cleared lines). dwin_exit switches
    // back to the power-on 16 KB mode.
    vdc_detect_mem_size();
    vdc_set_extended_memsize();

    dwin_state.width = (dwin_state.mode == DWIN_MODE_VDC) ? DWIN_VDC_WIDTH : DWIN_VIC_WIDTH;
}

// ---------------------------------------------------------------------------
// Title:       Set screen colours
// Description: Sets the border and background colour (VIC), or the
//              background colour (VDC, which has no separate border).
// Syntax:      void dwin_screen_colors(char border, char background);
// Input:       border     - logical border colour
//              background - logical background colour
// Output:      None
// ---------------------------------------------------------------------------
void dwin_screen_colors(char border, char background)
{
    if (dwin_state.mode == DWIN_MODE_VDC)
    {
        vdc_bgcolor(dwin_vdc_colors[background & (DWIN_COLORS - 1)]);
    }
    else
    {
        vic.color_border = border;
        vic.color_back = background;
    }
}

// ---------------------------------------------------------------------------
// Title:       Switch to the other screen
// Description: Makes the other screen (40 or 80 columns) the active one:
//              KERNAL SWAPPER, then the DualWin mode and width, and the
//              lower/upper case charset on the new screen. The caller
//              re-initialises its windows (their size depends on the
//              width), sets the screen colours and redraws; open popups
//              are dropped. The CPU speed is not changed: set 1 MHz before
//              drawing on the 40 column screen.
// Syntax:      void dwin_swap_screen(void);
// Input:       None
// Output:      None (dwin_state)
// ---------------------------------------------------------------------------
void dwin_swap_screen(void)
{
    dwin_swapper();
    dwin_state.mode = dwin_kernal_mode();
    dwin_state.width = (dwin_state.mode == DWIN_MODE_VDC) ? DWIN_VDC_WIDTH : DWIN_VIC_WIDTH;
    dwin_state.popups = 0;
    dwin_chrout(DWIN_CHR_LOWERCASE);
}

// ---------------------------------------------------------------------------
// Title:       VIC character set
// Description: Switches the 40 column screen between the lower/upper case
//              and the upper case/graphics character set, through the
//              KERNAL (its interrupt rewrites the VIC charset register from
//              a shadow copy, so a direct write does not last). Only for the
//              40 column screen: DualWin text needs the lower case set, so
//              switch back after a graphics screen.
// Syntax:      void dwin_vic_charset(bool lower);
// Input:       lower - true: lower/upper case, false: upper case/graphics
// Output:      None
// ---------------------------------------------------------------------------
void dwin_vic_charset(bool lower)
{
    dwin_chrout(lower ? DWIN_CHR_LOWERCASE : DWIN_CHR_UPPERCASE);
}

// ---------------------------------------------------------------------------
// Title:       Hand the screen back to the KERNAL
// Description: Re-initialises the KERNAL screen editor and both screens
//              (KERNAL CINT): colours, character sets, cleared screens. Call
//              before exiting to
//              BASIC: DualWin reprograms the VDC and writes screen memory
//              directly, which the KERNAL editor does not know about (seen
//              on hardware: shifted rows and garbage in BASIC after exit).
//              A 64 KB VDC is first switched back to the power-on 16 KB
//              addressing (programs such as CP/M from an REU image expect
//              it; left in 64 KB mode, CP/M's 80 column screen was garbled).
//              CINT picks the screen from the 40/80 key; when DualWin was
//              using the other one (dwin_swap_screen), that screen is made
//              active again, so BASIC continues where the user was.
// Syntax:      void dwin_exit(void);
// Input:       None
// Output:      None (the popup stack is emptied)
// ---------------------------------------------------------------------------
void dwin_exit(void)
{
    dwin_state.popups = 0;
    vdc_set_default_memsize();
    __asm
    {
        jsr $ff81
    }
    if (dwin_kernal_mode() != dwin_state.mode)
    {
        dwin_swapper();
    }
}

// ---------------------------------------------------------------------------
// Title:       Is 80 column mode active
// Description: Tells whether DualWin is using the 80 column VDC screen.
// Syntax:      bool dwin_is80(void);
// Input:       None
// Output:      true in 80 column mode, false in 40 column mode
// ---------------------------------------------------------------------------
bool dwin_is80(void)
{
    return dwin_state.mode == DWIN_MODE_VDC;
}

// ===========================================================================
// Windows
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Initialise a window
// Description: Sets up a window at a screen position. The window is clipped
//              to the screen and is at least 1x1. The window is not cleared.
// Syntax:      void dwin_init(struct DWin *win, char sx, char sy, char wx,
//                             char wy);
// Input:       win    - window to initialise
//              sx, sy - top left screen position
//              wx, wy - width and height in characters
// Output:      None
// ---------------------------------------------------------------------------
void dwin_init(struct DWin *win, char sx, char sy, char wx, char wy)
{
    if (sx >= dwin_state.width)
    {
        sx = dwin_state.width - 1;
    }
    if (sy >= dwin_state.height)
    {
        sy = dwin_state.height - 1;
    }
    if (!wx || wx > dwin_state.width - sx)
    {
        wx = dwin_state.width - sx;
    }
    if (!wy || wy > dwin_state.height - sy)
    {
        wy = dwin_state.height - sy;
    }

    win->sx = sx;
    win->sy = sy;
    win->wx = wx;
    win->wy = wy;
    win->cx = 0;
    win->cy = 0;

    if (dwin_state.mode == DWIN_MODE_VDC)
    {
        vdcwin_init(&win->backend.vdcwin, sx, sy, wx, wy);
    }
    else
    {
        cwin_init(&win->backend.vicwin, (char *)DWIN_VIC_SCREEN, sx, sy, wx, wy);
    }
}

// ---------------------------------------------------------------------------
// Title:       Fill a rectangle
// Description: Fills a rectangle inside a window with a PETSCII character
//              and colour. The rectangle is clipped to the window.
// Syntax:      void dwin_fill_rect(struct DWin *win, char x, char y, char w,
//                                  char h, char ch, char color);
// Input:       win   - window
//              x, y  - top left position inside the window
//              w, h  - width and height
//              ch    - PETSCII fill character
//              color - logical colour
// Output:      None
// ---------------------------------------------------------------------------
void dwin_fill_rect(struct DWin *win, char x, char y, char w, char h, char ch, char color)
{
    w = dwin_clip(win, x, y, w);
    if (!w)
    {
        return;
    }
    if (h > win->wy - y)
    {
        h = win->wy - y;
    }

    if (dwin_state.mode == DWIN_MODE_VDC)
    {
        dwin_vdc_attr(color);
        vdcwin_fill_rect(&win->backend.vdcwin, x, y, w, h, ch);
    }
    else
    {
        cwin_fill_rect(&win->backend.vicwin, x, y, w, h, ch, color);
    }
}

// ---------------------------------------------------------------------------
// Title:       Clear a window
// Description: Fills the window with spaces and moves the cursor home.
// Syntax:      void dwin_clear(struct DWin *win);
// Input:       win - window
// Output:      None
// ---------------------------------------------------------------------------
void dwin_clear(struct DWin *win)
{
    dwin_fill_rect(win, 0, 0, win->wx, win->wy, DWIN_CHR_SPACE, VCOL_WHITE);
    win->cx = 0;
    win->cy = 0;
}

// ---------------------------------------------------------------------------
// Title:       Scroll a window up
// Description: Scrolls the window content up one line and clears the
//              bottom line.
// Syntax:      void dwin_scroll_up(struct DWin *win, char color);
// Input:       win   - window
//              color - logical colour for the cleared bottom line
// Output:      None
// ---------------------------------------------------------------------------
void dwin_scroll_up(struct DWin *win, char color)
{
    if (win->wy > 1)
    {
        if (dwin_state.mode == DWIN_MODE_VDC)
        {
            vdcwin_scroll_up(&win->backend.vdcwin, 1);
        }
        else
        {
            cwin_scroll_up(&win->backend.vicwin, 1);
        }
    }
    dwin_fill_rect(win, 0, win->wy - 1, win->wx, 1, DWIN_CHR_SPACE, color);
}

// ===========================================================================
// Output
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Put a character at a position
// Description: Writes one PETSCII character at a window position, if the
//              position is inside the window. The cursor does not move.
// Syntax:      void dwin_putat_char(struct DWin *win, char x, char y,
//                                   char ch, char color);
// Input:       win   - window
//              x, y  - position inside the window
//              ch    - PETSCII character
//              color - logical colour
// Output:      None
// ---------------------------------------------------------------------------
void dwin_putat_char(struct DWin *win, char x, char y, char ch, char color)
{
    dwin_putat_chars(win, x, y, &ch, dwin_clip(win, x, y, 1), color);
}

// ---------------------------------------------------------------------------
// Title:       Put a string at a position
// Description: Writes a PETSCII string at a window position, clipped at the
//              right edge of the window. The cursor does not move.
// Syntax:      char dwin_putat_string(struct DWin *win, char x, char y,
//                                     const char *str, char color);
// Input:       win   - window
//              x, y  - position inside the window
//              str   - 0-terminated PETSCII string
//              color - logical colour
// Output:      Number of characters written
// ---------------------------------------------------------------------------
char dwin_putat_string(struct DWin *win, char x, char y, const char *str, char color)
{
    char num = dwin_clip(win, x, y, dwin_strnlen(str, DWIN_LINE_MAX));
    dwin_putat_chars(win, x, y, str, num, color);
    return num;
}

// ---------------------------------------------------------------------------
// Title:       Put a reverse string at a position
// Description: As dwin_putat_string, but shown in reverse.
// Syntax:      char dwin_putat_string_reverse(struct DWin *win, char x,
//                                             char y, const char *str,
//                                             char color);
// Input:       win   - window
//              x, y  - position inside the window
//              str   - 0-terminated PETSCII string
//              color - logical colour
// Output:      Number of characters written
// ---------------------------------------------------------------------------
char dwin_putat_string_reverse(struct DWin *win, char x, char y, const char *str, char color)
{
    char num = dwin_putat_string(win, x, y, str, color);
    dwin_reverse_rect(win, x, y, num, 1);
    return num;
}

// ---------------------------------------------------------------------------
// Title:       Reverse a rectangle
// Description: Sets reverse on every character of a rectangle inside the
//              window (clipped to the window), e.g. for a selection bar.
// Syntax:      void dwin_reverse_rect(struct DWin *win, char x, char y,
//                                     char w, char h);
// Input:       win  - window
//              x, y - top left position inside the window
//              w, h - width and height
// Output:      None
// ---------------------------------------------------------------------------
void dwin_reverse_rect(struct DWin *win, char x, char y, char w, char h)
{
    w = dwin_clip(win, x, y, w);
    if (h > win->wy - y)
    {
        h = win->wy - y;
    }
    for (char row = 0; row < h; row++)
    {
        for (char column = 0; column < w; column++)
        {
            dwin_set_reverse(win, x + column, y + row, true);
        }
    }
}

// ===========================================================================
// Cursor and console output
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Move the cursor
// Description: Moves the window cursor; positions outside the window are
//              clamped to the last column/row.
// Syntax:      void dwin_cursor_move(struct DWin *win, char cx, char cy);
// Input:       win    - window
//              cx, cy - new cursor position inside the window
// Output:      None
// ---------------------------------------------------------------------------
void dwin_cursor_move(struct DWin *win, char cx, char cy)
{
    win->cx = cx < win->wx ? cx : win->wx - 1;
    win->cy = cy < win->wy ? cy : win->wy - 1;
}

// ---------------------------------------------------------------------------
// Title:       Cursor to a new line
// Description: Moves the cursor to the start of the next line, scrolling
//              the window up when the cursor is on the last line.
// Syntax:      void dwin_cursor_newline(struct DWin *win, char color);
// Input:       win   - window
//              color - logical colour for a line cleared by scrolling
// Output:      None
// ---------------------------------------------------------------------------
void dwin_cursor_newline(struct DWin *win, char color)
{
    win->cx = 0;
    if (win->cy + 1 < win->wy)
    {
        win->cy++;
    }
    else
    {
        dwin_scroll_up(win, color);
    }
}

// ---------------------------------------------------------------------------
// Title:       Show or hide the cursor
// Description: Shows the cursor as a reverse character at the cursor
//              position, or hides it again.
// Syntax:      void dwin_cursor_show(struct DWin *win, bool show);
// Input:       win  - window
//              show - true to show, false to hide
// Output:      None
// ---------------------------------------------------------------------------
void dwin_cursor_show(struct DWin *win, bool show)
{
    dwin_set_reverse(win, win->cx, win->cy, show);
}

// ---------------------------------------------------------------------------
// Title:       Put a character at the cursor
// Description: Writes one PETSCII character at the cursor and advances the
//              cursor, wrapping and scrolling at the window edges. A newline
//              (LF or CR) moves to the next line.
// Syntax:      void dwin_put_char(struct DWin *win, char ch, char color);
// Input:       win   - window
//              ch    - PETSCII character or newline
//              color - logical colour
// Output:      None
// ---------------------------------------------------------------------------
void dwin_put_char(struct DWin *win, char ch, char color)
{
    if (ch == DWIN_CHR_NEWLINE_LF || ch == DWIN_CHR_NEWLINE_CR)
    {
        dwin_cursor_newline(win, color);
        return;
    }

    dwin_putat_chars(win, win->cx, win->cy, &ch, 1, color);
    win->cx++;
    if (win->cx >= win->wx)
    {
        dwin_cursor_newline(win, color);
    }
}

// ---------------------------------------------------------------------------
// Title:       Put a string at the cursor
// Description: Writes a PETSCII string at the cursor with console
//              behaviour (wrap, newline, scroll).
// Syntax:      void dwin_put_string(struct DWin *win, const char *str,
//                                   char color);
// Input:       win   - window
//              str   - 0-terminated PETSCII string
//              color - logical colour
// Output:      None
// ---------------------------------------------------------------------------
void dwin_put_string(struct DWin *win, const char *str, char color)
{
    while (*str)
    {
        dwin_put_char(win, *str++, color);
    }
}

// ---------------------------------------------------------------------------
// Title:       Formatted console output
// Description: printf-style output at the cursor with console behaviour.
//              The formatted text must fit in DWIN_PRINTF_MAX - 1
//              characters: Oscar64 has no vsnprintf, so print strings of
//              unknown length (file names, user input) with dwin_put_string
//              or dwin_putat_string instead of through %s.
// Syntax:      void dwin_printf(struct DWin *win, char color,
//                               const char *fmt, ...);
// Input:       win   - window
//              color - logical colour
//              fmt   - printf format string (PETSCII)
// Output:      None
// ---------------------------------------------------------------------------
void dwin_printf(struct DWin *win, char color, const char *fmt, ...)
{
    char buffer[DWIN_PRINTF_MAX];
    va_list args;

    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);

    buffer[DWIN_PRINTF_MAX - 1] = 0;
    dwin_put_string(win, buffer, color);
}

// ===========================================================================
// Popups
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Popup storage offset
// Description: Returns where the next popup's saved screen data starts,
//              right after the data of the popup below it.
// Syntax:      char *dwin_popup_next_store(void);
// Input:       None
// Output:      Address in the storage bank
// ---------------------------------------------------------------------------
static char *dwin_popup_next_store(void)
{
    if (!dwin_state.popups)
    {
        return dwin_state.storebase;
    }

    const struct DWinPopup *previous = &dwin_state.popup[dwin_state.popups - 1];
    return previous->store + (unsigned)previous->w * previous->h * 2;
}

// ---------------------------------------------------------------------------
// Title:       Save or restore a screen area
// Description: Copies the text and colour (VIC) or attribute (VDC) memory
//              of a popup area to or from its storage.
// Syntax:      void dwin_popup_copy(const struct DWinPopup *popup,
//                                   bool save);
// Input:       popup - popup area and storage address
//              save  - true: screen to storage, false: storage to screen
// Output:      None
// ---------------------------------------------------------------------------
static void dwin_popup_copy(const struct DWinPopup *popup, bool save)
{
    unsigned area = (unsigned)popup->w * popup->h;

    for (char row = 0; row < popup->h; row++)
    {
        char *textstore = popup->store + (unsigned)row * popup->w;
        char *colorstore = textstore + area;

        if (dwin_state.mode == DWIN_MODE_VDC)
        {
            unsigned offset = vdc_coords(popup->x, popup->y + row);
            if (save)
            {
                bnk_cpyfromvdc(dwin_state.storecr, textstore, vdc_state.base_text + offset, popup->w);
                bnk_cpyfromvdc(dwin_state.storecr, colorstore, vdc_state.base_attr + offset, popup->w);
            }
            else
            {
                bnk_cpytovdc(vdc_state.base_text + offset, dwin_state.storecr, textstore, popup->w);
                bnk_cpytovdc(vdc_state.base_attr + offset, dwin_state.storecr, colorstore, popup->w);
            }
        }
        else
        {
            unsigned offset = (unsigned)(popup->y + row) * DWIN_VIC_WIDTH + popup->x;
            char *screen = (char *)DWIN_VIC_SCREEN + offset;
            char *colors = (char *)DWIN_VIC_COLORRAM + offset;
            if (save)
            {
                bnk_memcpy(dwin_state.storecr, textstore, BNK_DEFAULT, screen, popup->w);
                bnk_memcpy(dwin_state.storecr, colorstore, BNK_DEFAULT, colors, popup->w);
            }
            else
            {
                bnk_memcpy(BNK_DEFAULT, screen, dwin_state.storecr, textstore, popup->w);
                bnk_memcpy(BNK_DEFAULT, colors, dwin_state.storecr, colorstore, popup->w);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Open a popup
// Description: Saves the screen area of a popup (including its border),
//              draws a border and clears the inside, and initialises a
//              window for the inside of the popup.
// Syntax:      bool dwin_popup_open(struct DWin *win, char x, char y,
//                                   char w, char h, char bordercolor,
//                                   char color);
// Input:       win         - window to initialise for the popup inside
//              x, y        - top left screen position of the border
//              w, h        - outer width and height, including the border
//                            (at least 3x3)
//              bordercolor - logical colour of the border
//              color       - logical colour of the inside
// Output:      true when the popup was opened; false when too many popups
//              are open, the area is outside the screen or the storage is
//              too small (nothing is changed then)
// ---------------------------------------------------------------------------
bool dwin_popup_open(struct DWin *win, char x, char y, char w, char h, char bordercolor, char color)
{
    if (dwin_state.popups >= DWIN_POPUP_MAX ||
        w < DWIN_POPUP_MIN || h < DWIN_POPUP_MIN ||
        x >= dwin_state.width || y >= dwin_state.height ||
        w > dwin_state.width - x || h > dwin_state.height - y)
    {
        return false;
    }

    char *store = dwin_popup_next_store();
    unsigned needed = (unsigned)w * h * 2;
    unsigned used = (unsigned)(store - dwin_state.storebase);
    if (used + needed > dwin_state.storesize)
    {
        return false;
    }

    struct DWinPopup *popup = &dwin_state.popup[dwin_state.popups];
    popup->x = x;
    popup->y = y;
    popup->w = w;
    popup->h = h;
    popup->store = store;
    dwin_popup_copy(popup, true);
    dwin_state.popups++;

    // Border, drawn through a temporary window over the whole popup
    struct DWin frame;
    dwin_init(&frame, x, y, w, h);
    dwin_putat_char(&frame, 0, 0, DWIN_BOX_UL, bordercolor);
    dwin_putat_char(&frame, w - 1, 0, DWIN_BOX_UR, bordercolor);
    dwin_putat_char(&frame, 0, h - 1, DWIN_BOX_LL, bordercolor);
    dwin_putat_char(&frame, w - 1, h - 1, DWIN_BOX_LR, bordercolor);
    dwin_fill_rect(&frame, 1, 0, w - 2, 1, DWIN_BOX_H, bordercolor);
    dwin_fill_rect(&frame, 1, h - 1, w - 2, 1, DWIN_BOX_H, bordercolor);
    dwin_fill_rect(&frame, 0, 1, 1, h - 2, DWIN_BOX_V, bordercolor);
    dwin_fill_rect(&frame, w - 1, 1, 1, h - 2, DWIN_BOX_V, bordercolor);

    dwin_init(win, x + 1, y + 1, w - 2, h - 2);
    dwin_fill_rect(win, 0, 0, win->wx, win->wy, DWIN_CHR_SPACE, color);
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Close a popup
// Description: Restores the screen area under the most recently opened
//              popup. Does nothing when no popup is open.
// Syntax:      void dwin_popup_close(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void dwin_popup_close(void)
{
    if (!dwin_state.popups)
    {
        return;
    }

    dwin_state.popups--;
    dwin_popup_copy(&dwin_state.popup[dwin_state.popups], false);
}

// ===========================================================================
// Keyboard
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Check for a key
// Description: Reads one key from the KERNAL keyboard buffer without
//              waiting and without character conversion.
// Syntax:      char dwin_checkch(void);
// Input:       None
// Output:      Raw PETSCII key code, DWIN_KEY_NONE when no key is waiting
// ---------------------------------------------------------------------------
char dwin_checkch(void)
{
    return __asm {
        jsr $ffe4
        sta accu
    };
}

// ---------------------------------------------------------------------------
// Title:       Wait for a key
// Description: Waits until a key is available and returns it raw.
// Syntax:      char dwin_getch(void);
// Input:       None
// Output:      Raw PETSCII key code
// ---------------------------------------------------------------------------
char dwin_getch(void)
{
    char key;
    do
    {
        key = dwin_checkch();
    } while (key == DWIN_KEY_NONE);
    return key;
}

// ---------------------------------------------------------------------------
// Title:       Is a key printable
// Description: Tells whether a PETSCII key code is a printable character
//              (no control code).
// Syntax:      bool dwin_printable(char key);
// Input:       key - raw PETSCII key code
// Output:      true for printable characters
// ---------------------------------------------------------------------------
static bool dwin_printable(char key)
{
    return (key >= 0x20 && key < 0x80) || key >= 0xa0;
}

// ---------------------------------------------------------------------------
// Title:       Draw an input field
// Description: Draws the visible part of the string of an input field and
//              the cursor.
// Syntax:      void dwin_input_draw(struct DWin *win, char x, char y,
//                                   const char *buffer, char index,
//                                   char offset, char width, char color);
// Input:       win    - window
//              x, y   - field position inside the window
//              buffer - edited string
//              index  - cursor index in the string
//              offset - index of the first visible character
//              width  - field width in characters
//              color  - logical colour
// Output:      None
// ---------------------------------------------------------------------------
static void dwin_input_draw(struct DWin *win, char x, char y, const char *buffer, char index, char offset, char width, char color)
{
    char visible[DWIN_LINE_MAX + 1];
    char len = strlen(buffer + offset);

    if (len > width)
    {
        len = width;
    }
    memcpy(visible, buffer + offset, len);
    visible[len] = 0;

    dwin_fill_rect(win, x, y, width, 1, DWIN_CHR_SPACE, color);
    dwin_putat_string(win, x, y, visible, color);
    dwin_set_reverse(win, x + index - offset, y, true);
}

// ---------------------------------------------------------------------------
// Title:       Edit a string in an input field
// Description: Lets the user edit a string in a one-line field. The field
//              scrolls horizontally when the string is longer than the
//              field. Keys: RETURN accepts, RUN/STOP cancels, DEL deletes
//              left of the cursor, SHIFT-DEL inserts a space, cursor
//              left/right move, HOME goes to the start. Only printable
//              characters are inserted. The string never exceeds size - 1
//              characters, so the buffer can never overflow.
//              Based on the text input of DraCopy/DraBrowse by Sascha
//              Bader, as adapted in my UBoot64-v2 project
//              (https://github.com/xahmol/UBoot64-v2). Adapted: size is the
//              buffer size (sizeof) instead of the maximum length, all
//              indexes are bounds-checked, output through DualWin.
// Syntax:      int dwin_input(struct DWin *win, char x, char y,
//                             char *buffer, char size, char width,
//                             char color);
// Input:       win    - window
//              x, y   - field position inside the window
//              buffer - 0-terminated string to edit (may be empty)
//              size   - size of buffer in bytes (sizeof(buffer)), >= 2
//              width  - field width in characters (clipped to the window)
//              color  - logical colour
// Output:      Length of the accepted string, or DWIN_INPUT_CANCEL when
//              the input was cancelled (buffer keeps the edits)
// ---------------------------------------------------------------------------
int dwin_input(struct DWin *win, char x, char y, char *buffer, char size, char width, char color)
{
    if (size < 2)
    {
        return DWIN_INPUT_CANCEL;
    }

    char maxlen = size - 1;
    width = dwin_clip(win, x, y, width);
    if (width < 2)
    {
        return DWIN_INPUT_CANCEL;
    }

    buffer[maxlen] = 0;
    char len = strlen(buffer);
    char index = len;

    while (true)
    {
        // Keep the cursor inside the visible part (last column is kept for
        // the cursor behind the string)
        char offset = (index >= width) ? index - width + 1 : 0;
        dwin_input_draw(win, x, y, buffer, index, offset, width, color);

        char key = dwin_getch();
        switch (key)
        {
        case DWIN_KEY_STOP:
            dwin_input_draw(win, x, y, buffer, 0, 0, width, color);
            dwin_set_reverse(win, x, y, false);
            return DWIN_INPUT_CANCEL;

        case DWIN_KEY_RETURN:
            dwin_input_draw(win, x, y, buffer, 0, 0, width, color);
            dwin_set_reverse(win, x, y, false);
            return len;

        case DWIN_KEY_DEL:
            if (index > 0)
            {
                // Shift the tail (including the terminator) one left
                memmove(buffer + index - 1, buffer + index, len - index + 1);
                index--;
                len--;
            }
            break;

        case DWIN_CHR_INSERT:
            if (len < maxlen && index < len)
            {
                memmove(buffer + index + 1, buffer + index, len - index + 1);
                buffer[index] = DWIN_CHR_SPACE;
                len++;
            }
            break;

        case DWIN_KEY_LEFT:
            if (index > 0)
            {
                index--;
            }
            break;

        case DWIN_KEY_RIGHT:
            if (index < len)
            {
                index++;
            }
            break;

        case DWIN_KEY_HOME:
            index = 0;
            break;

        default:
            if (dwin_printable(key))
            {
                if (index < len)
                {
                    // Overwrite
                    buffer[index++] = key;
                }
                else if (len < maxlen)
                {
                    // Append
                    buffer[index++] = key;
                    len++;
                    buffer[len] = 0;
                }
            }
            break;
        }
    }
}
