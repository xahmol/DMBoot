/*
Oscar64 VDC function library

Written in 2024 by Xander Mol

https://github.com/xahmol/Oscar64Test

https://www.idreamtin8bits.com/

Code and resources from others used:

-   Oscar64 cross compiler

    https://github.com/drmortalwombat/oscar64

    Many thanks also to https://github.com/drmortalwombat to provide extrordinary support and tips for making this and adapting Oscar64 to my needs faster than I could ask it.

-   Screens used in the demo made with my own VDC Screen Editor.

    https://github.com/xahmol/VDCScreenEdit

-   Commodore logo charset created using CharPad Pro.

    https://subchristsoftware.itch.io/c64-pro-editions

-   C128 Programmers Reference Guide: For the basic VDC register routines and VDC code inspiration

    http://www.zimmers.net/anonftp/pub/cbm/manuals/c128/C128_Programmers_Reference_Guide.pdf

-   Tokra: For the optimal VDC registry settings for 80x50 and 80x70 textmodes

-   Scott Hutter - VDC Core functions inspiration:

    https://github.com/Commodore64128/vdc_gui/blob/master/src/vdc_core.c

    (used as starting point)

-   Scott Robison for teaching me how o create a C128 disk boot sector

-   Francesco Sblendorio - Screen Utility: used for inspiration:

    https://github.com/xlar54/ultimateii-dos-lib/blob/master/src/samples/screen_utility.c

-   DevDef: Commodore 128 Assembly - Part 3: The 80-column (8563) chip

    https://devdef.blogspot.com/2018/03/commodore-128-assembly-part-3-80-column.html

-   Tips and Tricks for C128: VDC

    http://commodore128.mirkosoft.sk/vdc.html

-   Steve Goldsmith - C3L Commodore 128 CP/M C Library

    https://github.com/sgjava/c3l

    (Used for inspiration and for the text wrap and random sentence generator functions)

-   Bart van Leeuwen: For inspiration and advice while coding. Also for providing the excellent Device Manager ROM to make testing on real hardware very easy

-   Original windowing system code on Commodore 128 by unknown author.
   
-   Tested using real hardware (C128D and C128DCR) plus VICE.

The code can be used freely as long as you retain a notice describing original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
*/

#ifndef VDC_WIN_H
#define VDC_WIN_H

// Define window settings
#define WIN_MAX_NR 3      // Maximum number of windows (of memory allows)
#define WIN_MEMORY 0x2000 // Maximum memory available for windows (now 4 KB)

// Border flags
#define WIN_BOR_UP 0x80    // Bit 7: set top border on (1) or off (0)
#define WIN_BOR_LE 0x40    // Bit 6: set left border on (1) or off (0)
#define WIN_BOR_RI 0x20    // Bit 5: set right border on (1) or off (0)
#define WIN_BOR_BO 0x10    // Bit 4: set bottom border on (1) or off (0)
                           // Bit 0-3: style number (0-15 or max defined)
#define WIN_BOR_ALL 0xF0   // All borders on
#define WIN_BOR_NOTOP 0x70 // All borders expect for top

// Structs
#include "vdcwin_types.h"
struct VDCViewport
{
    char sourcebank;
    char *sourcebase;
    unsigned sourcewidth;
    unsigned sourceheight;
    unsigned sourcexoffset;
    unsigned sourceyoffset;
    struct VDCWin view;
};
struct WinStyleStruct
{
    char color;
    char upcornleft;
    char upcornright;
    char bocornleft;
    char bocornright;
    char up;
    char down;
    char left;
    char right;
};
struct WinCfgStruct
{
    char memcr;
    char *membase;
    unsigned memsize;
    char active;
    char *memactive;
};
extern struct WinCfgStruct winCfg;
struct WinStruct
{
    char *memaddress;
    char border;
    struct VDCWin win;
};
extern struct WinStruct windows[WIN_MAX_NR];

// Function prototypes

// Initialize the VDCWin structure for the given screen and coordinates, does not clear the window
void vdcwin_init(struct VDCWin *win, char sx, char sy, char wx, char wy);

// Clear the window
void vdcwin_clear(struct VDCWin *win);

// Fill the window with the given character and color
void vdcwin_fill(struct VDCWin *win, char ch);

// Show or hide the cursor by setting or clearing the reverse attribute
void vdcwin_cursor_toggle(struct VDCWin *win);

// Move the cursor to the given location
void vdcwin_cursor_move(struct VDCWin *win, char cx, char cy);

// Move the cursor in the window, returns true if the cursor could be moved
bool vdcwin_cursor_left(struct VDCWin *win);
bool vdcwin_cursor_right(struct VDCWin *win);
bool vdcwin_cursor_up(struct VDCWin *win);
bool vdcwin_cursor_down(struct VDCWin *win);
bool vdcwin_cursor_forward(struct VDCWin *win);
bool vdcwin_cursor_backward(struct VDCWin *win);
bool vdcwin_cursor_newline(struct VDCWin *win);

// Read the full window into a string
void vdcwin_read_string(struct VDCWin *win, char *buffer);

// Write the fill window with the given string
void vdcwin_write_string(struct VDCWin *win, const char *buffer);

// Put a single char at the cursor location and advance the cursor
void vdcwin_put_char(struct VDCWin *win, char ch);

// Put an array of chars at the cursor location and advance the cursor
void vdcwin_put_chars(struct VDCWin *win, const char *chars, char num);

// Put a zero terminated string at the cursor location and advance the cursor
char vdcwin_put_string(struct VDCWin *win, const char *str);

// Put a single raw char at the cursor location and advance the cursor
void vdcwin_put_char_raw(struct VDCWin *win, char ch);

// Put an array of raw chars at the cursor location and advance the cursor
void vdcwin_put_chars_raw(struct VDCWin *win, const char *chars, char num);

// Put a zero terminated raw string at the cursor location and advance the cursor
char vdcwin_put_string_raw(struct VDCWin *win, const char *str);

// Put a single char at the given window location
void vdcwin_putat_char(struct VDCWin *win, char x, char y, char ch);

// Put an array of chars at the given window location
void vdcwin_putat_chars(struct VDCWin *win, char x, char y, const char *chars, char num);

// Put a zero terminated string at the given window location
char vdcwin_putat_string(struct VDCWin *win, char x, char y, const char *str);

// Put a single raw char at the given window location
void vdcwin_putat_char_raw(struct VDCWin *win, char x, char y, char ch);

// Put an array of raw chars at the given window location
void vdcwin_putat_chars_raw(struct VDCWin *win, char x, char y, const char *chars, char num);

// Put a zero terminated string at the given window location
char vdcwin_putat_string_raw(struct VDCWin *win, char x, char y, const char *str);

// Get a single char at the given window location
char vdcwin_getat_char(struct VDCWin *win, char x, char y);

// Get an array of chars at the given window location
void vdcwin_getat_chars(struct VDCWin *win, char x, char y, char *chars, char num);

// Get a single char at the given window location
char vdcwin_getat_char_raw(struct VDCWin *win, char x, char y);

// Get an array of chars at the given window location
void vdcwin_getat_chars_raw(struct VDCWin *win, char x, char y, char *chars, char num);

// Put an array of characters into a rectangle in the char win
void vdcwin_put_rect_raw(struct VDCWin *win, char x, char y, char w, char h, char cr, const char *chars);
void vdcwin_put_rect(struct VDCWin *win, char x, char y, char w, char h, char cr, const char *chars);

// Get an array of characters from a rectangle of a char win
void vdcwin_get_rect_raw(struct VDCWin *win, char x, char y, char w, char h, char cr, char *chars);
void vdcwin_get_rect(struct VDCWin *win, char x, char y, char w, char h, char cr, char *chars);

// Insert one space character at the cursor position
void vdcwin_insert_char(struct VDCWin *win);

// Delete the character at the cursor position
void vdcwin_delete_char(struct VDCWin *win);

// Get character functions
int vdcwin_getch(void);
int vdcwin_checkch(void);

// Edit the window position using the char as the input
bool vdcwin_edit_char(struct VDCWin *win, char ch);

// Edit the window using keyboard input, returns the key the exited the edit, either return or stop
char vdcwin_edit(struct VDCWin *win);

// Scroll the window in the given direction, does not fill the new empty space
void vdcwin_scroll_left(struct VDCWin *win, char by);
void vdcwin_scroll_right(struct VDCWin *win, char by);
void vdcwin_scroll_up(struct VDCWin *win, char by);
void vdcwin_scroll_down(struct VDCWin *win, char by);

// Fill the given rectangle with the character and color
inline void vdcwin_fill_rect(struct VDCWin *win, char x, char y, char w, char h, char ch);

// Fill the given rectangle with the screen code and color
void vdcwin_fill_rect_raw(struct VDCWin *win, char x, char y, char w, char h, char ch);

// Print string to window and set cursor at next line, scroll if last line
void vdcwin_printline(struct VDCWin *win, const char *str);

// Print with line wrap in window
void vdcwin_printwrap(struct VDCWin *win, const char *str);

// Clear area with border with selected borderstyle
void vdcwin_border_clear(struct VDCWin *win, char border);

// Initialise window background saving storage
void vdcwin_winstorage_init(char memcr, char *membase, unsigned memsize);

// Create new window with background save
bool vdcwin_win_new(char border, char xpos, char ypos, char width, char height);

// Close window and restore background
void vdcwin_win_free();

// Initialize a viewport of screen data in memory
void vdcwin_viewport_init(struct VDCViewport *vp, char sourcebank, char *sourcebase, unsigned sourcewidth, unsigned sourceheight, unsigned viewwidth, unsigned viewheight, char viewsx, char viewsy);

// Copy a viewport on the source screen map to the VDC
void vdcwin_cpy_viewport(struct VDCViewport *viewport);

// Scroll a viewport on the source screen map on the VDC in the given direction
void vdcwin_viewportscroll(struct VDCViewport *viewport, char direction);

#pragma compile("vdc_win.c")

#endif
