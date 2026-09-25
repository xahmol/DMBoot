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

#include <stdbool.h>
#include <petscii.h>
#include <c128/vdc.h>
#include "vdc_win.h"
#include "vdc_core.h"
#include "banking.h"

struct WinStyleStruct winStyle;
struct WinCfgStruct winCfg;
struct WinStruct windows[WIN_MAX_NR];

// Predefined windows border styles: bordercolor plus screencodes for border chars
struct WinStyleStruct winStyles[2] =
	{
		{VDC_DRED, 0x6c, 0x7b, 0x7c, 0x7e, 0x62, 0xe2, 0xe1, 0x61},
		{VDC_LYELLOW, 0x70, 0x6e, 0x6d, 0x7d, 0x40, 0x40, 0x5d, 0x5d}};

static inline void copy_fwd(unsigned sdp, const unsigned ssp, unsigned cdp, const unsigned csp, char n)
// Copy of a line direct from VDC source to VDC destination (for vertical based copy)
{
	// Screen copy
	vdc_block_copy(sdp, ssp, n);
	// Color copy
	vdc_block_copy(cdp, csp, n);
}

static inline void copy_bwd(unsigned sdp, const unsigned ssp, unsigned cdp, const unsigned csp, char n)
// Copy of a line from VDC source via swap memory to VDC destination (for horizontal based copy)
{
	// Check if swap memory is available given screenmode and VDC memory size
	if (!vdc_state.memextended && vdc_state.swap_text > 0x3ff)
	{
		// Screen copy via bank 0 linebuffer
		bnk_cpyfromvdc(BNK_DEFAULT, linebuffer, ssp, n);
		bnk_cpytovdc(sdp, BNK_DEFAULT, linebuffer, n);
		// Color copy via bank 0 linebuffer
		bnk_cpyfromvdc(BNK_DEFAULT, linebuffer, csp, n);
		bnk_cpytovdc(cdp, BNK_DEFAULT, linebuffer, n);
	}
	else
	{
		// Screen copy via swap VDC memory
		vdc_block_copy(vdc_state.swap_text, ssp, n);
		vdc_block_copy(sdp, vdc_state.swap_text, n);
		// Color copy via swap VDC memory
		vdc_block_copy(vdc_state.swap_text, csp, n);
		vdc_block_copy(cdp, vdc_state.swap_text, n);
	}
}

void vdcwin_init(struct VDCWin *win, char sx, char sy, char wx, char wy)
// Initialize the VDCWin structure for the given screen and coordinates, does not clear the window
{
	win->sx = sx;
	win->sy = sy;
	win->wx = wx;
	win->wy = wy;
	win->cx = 0;
	win->cy = 0;
	win->sp = vdc_state.base_text + vdc_coords(sx, sy);
	win->cp = vdc_state.base_attr + vdc_coords(sx, sy);
}

void vdcwin_clear(struct VDCWin *win)
// Clear the window
{
	vdcwin_fill(win, ' ');
}

void vdcwin_fill(struct VDCWin *win, char ch)
// Fill the window with the given character and the active attributes
{
	vdc_clear(win->sx, win->sy, ch, win->wx, win->wy);
}

void vdcwin_cursor_toggle(struct VDCWin *win)
// Show or hide the cursor by setting or clearing the reverse attribute
{
	unsigned cp = vdc_state.base_attr + vdc_coords(win->sx + win->cx, win->sy + win->cy);
	char attr = vdc_mem_read_at(cp);
	attr ^= VDC_A_REVERSE;
	attr ^= VDC_A_BLINK;
	vdc_mem_write_at(cp,attr);
}

void vdcwin_cursor_move(struct VDCWin *win, char cx, char cy)
// Move the cursor to the given location
{
	win->cx = cx;
	win->cy = cy;
}

bool vdcwin_cursor_left(struct VDCWin *win)
// Move the cursor left in the window, returns true if the cursor could be moved
{
	if (win->cx > 0)
	{
		win->cx--;
		return true;
	}

	return false;
}

bool vdcwin_cursor_right(struct VDCWin *win)
// Move the cursor right in the window, returns true if the cursor could be moved
{
	if (win->cx + 1 < win->wx)
	{
		win->cx++;
		return true;
	}

	return false;
}

bool vdcwin_cursor_up(struct VDCWin *win)
// Move the cursor up in the window, returns true if the cursor could be moved
{
	if (win->cy > 0)
	{
		win->cy--;
		return true;
	}

	return false;
}

bool vdcwin_cursor_down(struct VDCWin *win)
// Move the cursor down in the window, returns true if the cursor could be moved
{
	if (win->cy + 1 < win->wy)
	{
		win->cy++;
		return true;
	}

	return false;
}

bool vdcwin_cursor_newline(struct VDCWin *win)
// Go to next line
{
	win->cx = 0;
	if (win->cy + 1 < win->wy)
	{
		win->cy++;
		return true;
	}

	return false;
}

bool vdcwin_cursor_forward(struct VDCWin *win)
// Move cursor position forward
{
	if (win->cx + 1 < win->wx)
	{
		win->cx++;
		return true;
	}
	else if (win->cy + 1 < win->wy)
	{
		win->cx = 0;
		win->cy++;
		return true;
	}

	return false;
}

bool vdcwin_cursor_backward(struct VDCWin *win)
// Move cursor position backward
{
	if (win->cx > 0)
	{
		win->cx--;
		return true;
	}
	else if (win->cy > 0)
	{
		win->cx = win->wx - 1;
		win->cy--;
		return true;
	}

	return false;
}

// PETSCII to and from screencode conversion tables
static char p2smap[] = {0x00, 0x00, 0x40, 0x20, 0x80, 0xc0, 0x80, 0x80};
static char s2pmap[] = {0x40, 0x00, 0x20, 0xc0, 0xc0, 0x80, 0xa0, 0x40};

static inline char p2s(char ch)
// PETSCII to screencode conversion
{
	return ch ^ p2smap[ch >> 5];
}

static inline char s2p(char ch)
// Screencode to PETSCII converion
{
	return ch ^ s2pmap[ch >> 5];
}

void vdcwin_read_string(struct VDCWin *win, char *buffer)
// Read the full window into a string
{
	unsigned sp = win->sp;

	char i = 0;
	for (char y = 0; y < win->wy; y++)
	{
		for (char x = 0; x < win->wx; x++)
		{
			buffer[i++] = s2p(vdc_mem_read_at(sp + x));
		}
		sp += vdc_state.width;
	}
	while (i > 0 && buffer[i - 1] == ' ')
		i--;
	buffer[i] = 0;
}

void vdcwin_write_string(struct VDCWin *win, const char *buffer)
// Write the fill window with the given string
{
	unsigned dp = win->sp;
	for (char y = 0; y < win->wy; y++)
	{
		for (char x = 0; x < win->wx; x++)
		{
			char ch = *buffer;
			if (ch)
			{
				vdc_mem_write_at(dp + x, p2s(ch));
				buffer++;
			}
			else
				vdc_mem_write_at(dp + x, ' ');
		}
		dp += vdc_state.width;
	}
}

void vdcwin_put_char(struct VDCWin *win, char ch)
// Put a single char at the cursor location and advance the cursor
{
	vdcwin_putat_char(win, win->cx, win->cy, ch);
	win->cx++;
	if (win->cx == win->wx)
	{
		win->cx = 0;
		win->cy++;
		if (win->cy == win->wy)
		{
			win->cy--;
			vdcwin_printline(win, "");
		}
	}
}

void vdcwin_put_chars(struct VDCWin *win, const char *chars, char num)
// Put an array of chars at the cursor location and advance the cursor
{
	vdcwin_putat_chars(win, win->cx, win->cy, chars, num);
	win->cx += num;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
		if (win->cy == win->wy)
		{
			win->cy--;
			vdcwin_printline(win, "");
		}
	}
}

char vdcwin_put_string(struct VDCWin *win, const char *str)
// Put a zero terminated string at the cursor location and advance the cursor
{
	char n = vdcwin_putat_string(win, win->cx, win->cy, str);
	win->cx += n;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
		if (win->cy == win->wy)
		{
			win->cy--;
			vdcwin_printline(win, "");
		}
	}
	return n;
}

void vdcwin_put_char_raw(struct VDCWin *win, char ch)
// Put a single raw char at the cursor location and advance the cursor
{
	vdcwin_putat_char_raw(win, win->cx, win->cy, ch);
	win->cx++;
	if (win->cx == win->wx)
	{
		win->cx = 0;
		win->cy++;
		if (win->cy == win->wy)
		{
			win->cy--;
			vdcwin_printline(win, "");
		}
	}
}

void vdcwin_put_chars_raw(struct VDCWin *win, const char *chars, char num)
// Put an array of raw chars at the cursor location and advance the cursor
{
	vdcwin_putat_chars_raw(win, win->cx, win->cy, chars, num);
	win->cx += num;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
		if (win->cy == win->wy)
		{
			win->cy--;
			vdcwin_printline(win, "");
		}
	}
}

char vdcwin_put_string_raw(struct VDCWin *win, const char *str)
// Put a zero terminated raw string at the cursor location and advance the cursor
{
	char n = vdcwin_putat_string_raw(win, win->cx, win->cy, str);
	win->cx += n;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
		if (win->cy == win->wy)
		{
			win->cy--;
			vdcwin_printline(win, "");
		}
	}

	return n;
}

void vdcwin_putat_char(struct VDCWin *win, char x, char y, char ch)
// Put a single char at the given window location
{
	vdc_printc(win->sx + x, win->sy + y, p2s(ch), vdc_state.text_attr);
}

#pragma native(vdcwin_putat_char)

void vdcwin_putat_chars(struct VDCWin *win, char x, char y, const char *chars, char num)
// Put an array of chars at the given window location
{
	for (char i = 0; i < num; i++)
	{
		vdc_printc(win->sx + x + i, win->sy + y, p2s(chars[i]), vdc_state.text_attr);
	}
}

#pragma native(vdcwin_putat_chars)

char vdcwin_putat_string(struct VDCWin *win, char x, char y, const char *str)
// Put a zero terminated string at the given window location
{
	char i = 0;
	char ch;
	while (ch = str[i])
	{
		vdc_printc(win->sx + x + i, win->sy + y, p2s(ch), vdc_state.text_attr);
		i++;
	}

	return i;
}

#pragma native(vdcwin_putat_string)

void vdcwin_putat_char_raw(struct VDCWin *win, char x, char y, char ch)
// Put a single raw char at the given window location
{
	vdc_printc(win->sx + x, win->sy + y, ch, vdc_state.text_attr);
}

#pragma native(vdcwin_putat_char_raw)

void vdcwin_putat_chars_raw(struct VDCWin *win, char x, char y, const char *chars, char num)
// Put an array of raw chars at the given window location
{
	for (char i = 0; i < num; i++)
	{
		vdc_printc(win->sx + x + i, win->sy + y, chars[i], vdc_state.text_attr);
	}
}

#pragma native(vdcwin_putat_chars_raw)

char vdcwin_putat_string_raw(struct VDCWin *win, char x, char y, const char *str)
// Put a zero terminated string at the given window location
{
	char i = 0;
	char ch;
	while (ch = str[i])
	{
		vdc_printc(win->sx + x + i, win->sy + y, ch, vdc_state.text_attr);
		i++;
	}

	return i;
}

#pragma native(vdcwin_putat_string_raw)

char vdcwin_getat_char(struct VDCWin *win, char x, char y)
// Get a single char at the given window location
{
	unsigned sp = win->sp + vdc_coords(x, y);

	return s2p(vdc_mem_read_at(sp));
}

#pragma native(vdcwin_getat_char)

void vdcwin_getat_chars(struct VDCWin *win, char x, char y, char *chars, char num)
// Get an array of chars at the given window location
{
	unsigned sp = win->sp + vdc_coords(x, y);

	for (char i = 0; i < num; i++)
	{
		chars[i] = s2p(vdc_mem_read_at(sp + i));
	}
}

#pragma native(vdcwin_getat_chars)

char vdcwin_getat_char_raw(struct VDCWin *win, char x, char y)
// Get a single char at the given window location
{
	unsigned sp = win->sp + vdc_coords(x, y);

	return vdc_mem_read_at(sp);
}

#pragma native(vdcwin_getat_chars_raw)

void vdcwin_getat_chars_raw(struct VDCWin *win, char x, char y, char *chars, char num)
// Get an array of chars at the given window location
{
	unsigned sp = win->sp + vdc_coords(x, y);

	for (char i = 0; i < num; i++)
	{
		chars[i] = vdc_mem_read_at(sp + i);
	}
}

#pragma native(vdcwin_put_rect_raw)

void vdcwin_put_rect_raw(struct VDCWin *win, char x, char y, char w, char h, char cr, const char *chars)
// Put an array of raw characters into a rectangle in the char win
{
	int offset = vdc_coords(x, y);

	unsigned sp = win->sp + offset;
	unsigned cp = win->cp + offset;

	for (char i = 0; i < h; i++)
	{
		bnk_cpytovdc(sp, cr, chars, w);
		// Fix (DMBoot v5, 2026-09-25): block fill length is zero based
		// (one byte plus length - 1 more); w filled one attribute too many.
		if (w > 1)
		{
			vdc_block_fill(cp, vdc_state.text_attr, w - 1);
		}
		else if (w == 1)
		{
			vdc_mem_write_at(cp, vdc_state.text_attr);
		}
		chars += w;
		sp += vdc_state.width;
		cp += vdc_state.width;
	}
}

#pragma native(vdcwin_put_rect)

void vdcwin_put_rect(struct VDCWin *win, char x, char y, char w, char h, char cr, const char *chars)
// Put an array of characters into a rectangle in the char win
{
	int offset = vdc_coords(x, y);

	unsigned sp = win->sp + offset;
	unsigned cp = win->cp + offset;

	for (char i = 0; i < h; i++)
	{

		for (char j = 0; j < w; j++)
		{
			vdc_mem_write_at(sp + j, p2s(bnk_readb(cr, chars + j)));
			vdc_mem_write_at(cp + j, vdc_state.text_attr);
		}

		chars += w;
		sp += vdc_state.width;
		cp += vdc_state.width;
	}
}

#pragma native(vdcwin_get_rect_raw)

void vdcwin_get_rect_raw(struct VDCWin *win, char x, char y, char w, char h, char cr, char *chars)
// Get an array of raw characters from a rectangle of a char win
{
	int offset = vdc_coords(x, y);

	unsigned sp = win->sp + offset;

	for (char i = 0; i < h; i++)
	{
		bnk_cpyfromvdc(cr, chars, sp, w);
		chars += w;
		sp += vdc_state.width;
	}
}

#pragma native(vdcwin_get_rect)

void vdcwin_get_rect(struct VDCWin *win, char x, char y, char w, char h, char cr, char *chars)
// Get an array of characters from a rectangle of a char win
{
	int offset = vdc_coords(x, y);

	unsigned sp = win->sp + offset;

	for (char i = 0; i < h; i++)
	{

		for (char j = 0; j < w; j++)
		{
			bnk_writeb(cr, chars + j, s2p(vdc_mem_read_at(sp + j)));
		}

		chars += w;
		sp += vdc_state.width;
	}
}

#pragma native(vdcwin_getat_chars_raw)

void vdcwin_insert_char(struct VDCWin *win)
// Insert one space character at the cursor position
{
	char y = win->wy - 1, rx = win->wx - 1;

	unsigned sp = win->sp + vdc_coords(0, y);
	unsigned cp = win->cp + vdc_coords(0, y);

	while (y > win->cy)
	{
		copy_bwd(sp + 1, sp, cp + 1, cp, rx);

		sp -= vdc_state.width;
		cp -= vdc_state.width;
		vdc_mem_write_at(sp + vdc_state.width, vdc_mem_read_at(sp + rx));
		vdc_mem_write_at(cp + vdc_state.width, vdc_mem_read_at(cp + rx));
		y--;
	}

	sp += win->cx;
	cp += win->cx;
	rx -= win->cx;

	if (rx)
	{
		copy_bwd(sp + 1, sp, cp + 1, cp, rx);
	}

	vdc_mem_write_at(sp, ' ');
}

void vdcwin_delete_char(struct VDCWin *win)
// Delete the character at the cursor position
{
	unsigned sp = win->sp + vdc_coords(0, win->cy);
	unsigned cp = win->cp + vdc_coords(0, win->cy);

	char x = win->cx, rx = win->wx - 1;

	copy_fwd(sp + x, sp + x + 1, cp + x, cp + x + 1, rx - x);

	char y = win->cy + 1;
	while (y < win->wy)
	{
		vdc_mem_write_at(sp + rx, vdc_mem_read_at(sp + vdc_state.width));
		vdc_mem_write_at(cp + rx, vdc_mem_read_at(cp + vdc_state.width));

		sp += vdc_state.width;
		cp += vdc_state.width;

		copy_fwd(sp, sp + 1, cp, cp + 1, rx);

		y++;
	}

	vdc_mem_write_at(sp + rx, ' ');
}

int vdcwin_getch(void)
// Get character function
{
	__asm
	{
		L1:
			jsr	0xffe4
			cmp	#0
			beq	L1
			sta	accu
			lda	#0
			sta	accu + 1
	}
}

int vdcwin_checkch(void)
// Check character functions
{
	__asm
	{
		L1:
			jsr	0xffe4
			sta	accu
			lda	#0
			sta	accu + 1
	}
}

bool vdcwin_edit_char(struct VDCWin *win, char ch)
// Edit the window position using the char as the input
{
	switch (ch)
	{
	case 13:
	case 3:
	case 27:
		return true;

	case 19:
		win->cx = 0;
		win->cy = 0;
		return false;

	case 147:
		vdcwin_clear(win);
		win->cx = 0;
		win->cy = 0;
		return false;

	case 17:
		vdcwin_cursor_down(win);
		return false;

	case 145: // CRSR_UP
		vdcwin_cursor_up(win);
		return false;

	case 29:
		vdcwin_cursor_forward(win);
		return false;

	case 157:
		vdcwin_cursor_backward(win);
		return false;

	case 20:
		if (vdcwin_cursor_backward(win))
			vdcwin_delete_char(win);
		return false;

	default:
		if (ch >= 32 && ch < 128 || ch >= 160)
		{
			if (win->cy + 1 < win->wy || win->cx + 1 < win->wx)
			{
				vdcwin_insert_char(win);
				vdcwin_put_char(win, ch);
			}
		}
		return false;
	}
}

char vdcwin_edit(struct VDCWin *win)
// Edit the window using keyboard input, returns the key the exited the edit, either return or stop
{
	for (;;)
	{
		vdcwin_cursor_toggle(win);
		char ch = vdcwin_getch();
		vdcwin_cursor_toggle(win);

		if (vdcwin_edit_char(win, ch))
			return ch;
	}
}

void vdcwin_scroll_left(struct VDCWin *win, char by)
// Scroll the window left, does not fill the new empty space
{
	unsigned sp = win->sp;
	unsigned cp = win->cp;

	char rx = win->wx - by;

	vdc_wait_vblank();
	for (char y = 0; y < win->wy; y++)
	{
		//if(y==0)
		//{
		//	vdc_wait_vblank();
		//}
		//if(y==1)
		//{
		//	vdc_wait_no_vblank();
		//}
		copy_bwd(sp, sp + by, cp, cp + by, rx);
		sp += vdc_state.width;
		cp += vdc_state.width;
	}
	vdc_wait_no_vblank();
}

void vdcwin_scroll_right(struct VDCWin *win, char by)
// Scroll the window right, does not fill the new empty space
{
	unsigned sp = win->sp;
	unsigned cp = win->cp;

	char rx = win->wx - by;

	for (char y = 0; y < win->wy; y++)
	{
		copy_bwd(sp + by, sp, cp + by, cp, rx);
		sp += vdc_state.width;
		cp += vdc_state.width;
	}
}

void vdcwin_scroll_up(struct VDCWin *win, char by)
// Scroll the window up, does not fill the new empty space
{
	unsigned sp = win->sp;
	unsigned cp = win->cp;

	char rx = win->wx;
	int dst = vdc_state.width * by;

	for (char y = 0; y < win->wy - by; y++)
	{
		copy_fwd(sp, sp + dst, cp, cp + dst, rx);
		sp += vdc_state.width;
		cp += vdc_state.width;
	}
}

void vdcwin_scroll_down(struct VDCWin *win, char by)
// Scroll the window down, does not fill the new empty space
{
	unsigned sp = win->sp + vdc_state.width * win->wy;
	unsigned cp = win->cp + vdc_state.width * win->wy;

	char rx = win->wx;

	int dst = vdc_state.width * by;

	for (char y = 0; y < win->wy - by; y++)
	{
		sp -= vdc_state.width;
		cp -= vdc_state.width;
		copy_fwd(sp, sp - dst, cp, cp - dst, rx);
	}
}

void vdcwin_fill_rect_raw(struct VDCWin *win, char x, char y, char w, char h, char ch)
// Fill the given rectangle with the character and the active color
{
	if (w > 0)
	{
		for (char i = 0; i < h; i++)
		{
			vdc_hchar(win->sx + x, win->sy + y + i, ch, vdc_state.text_attr, w);
		}
	}
}

void vdcwin_fill_rect(struct VDCWin *win, char x, char y, char w, char h, char ch)
// Fill the given rectangle with the screen code and color
{
	vdcwin_fill_rect_raw(win, x, y, w, h, p2s(ch));
}

void vdcwin_printline(struct VDCWin *win, const char *str)
// Print string to window and set cursor at next line, scroll if last line
{
	vdcwin_put_string(win, str);
	win->cx = 0;
	if (win->cy < win->wy - 1)
	{
		win->cy++;
	}
	else
	{
		vdcwin_scroll_up(win, 1);
		vdc_clear(win->sx, win->sy + win->wy - 1, ' ', win->wx, 1);
	}
}

void vdcwin_printwrap(struct VDCWin *win, const char *str)
// Print with line wrap in window
// Adappted from
// https://github.com/sgjava/c3l/blob/main/src/conww.c
// Copyright (c) Steven P. Goldsmith. All rights reserved.
{
	/* Screen width should not exceed buffer size +1 */
	char wrapbuffer[81];
	char i = 0, wordLen, len = strlen(str);
	signed wordStart = -1, wordEnd = -1;
	char maxLine = win->wx, buf = 0, maxBuf = sizeof(wrapbuffer) - 1;
	while (i < len && buf < maxBuf)
	{
		/* Load word buffer using space delimiter */
		while (i < len && wordEnd < 0 && buf < maxBuf)
		{
			/* Find first non space char */
			if (str[i] != ' ')
			{
				if (wordStart < 0)
				{
					wordStart = i;
				}
				wrapbuffer[buf++] = str[i];
			}
			else
			{
				/* End of word including space */
				if (wordEnd < 0)
				{
					wrapbuffer[buf++] = str[i];
					wordEnd = i;
				};
			}
			i++;
		}
		if (buf > 0)
		{
			wrapbuffer[buf] = 0x00;
			wordLen = strlen(wrapbuffer);

			while (wordLen > win->wx)
			{
				vdcwin_printline(win, "");
				vdcwin_put_chars(win, wrapbuffer, win->wx);
				strcpy(wrapbuffer, wrapbuffer + win->wx);
				wordLen = strlen(wrapbuffer);
			}

			if (win->cx + wordLen > maxLine)
			{
				vdcwin_printline(win, "");
			}
			vdcwin_put_string(win, wrapbuffer);
			wordStart = -1;
			wordEnd = -1;
			buf = 0;
		}
	}
}

void vdcwin_border_clear(struct VDCWin *win, char border)
// Clear area with border with selected borderstyle
{
	char style = border & 0x0f;
	char color = winStyles[style].color;

	// Check if there is room for left and right borders
	if ((border & WIN_BOR_LE) && win->sx == 0)
	{
		border &= ~WIN_BOR_LE;
	}
	if ((border & WIN_BOR_RI) && win->sx + win->wx + 1 > vdc_state.width)
	{
		border &= ~WIN_BOR_RI;
	}

	// Draw top and bottom borders if room
	if ((border & WIN_BOR_UP) && win->sy > 0)
	{
		if (border & WIN_BOR_LE)
		{
			vdc_printc(win->sx - 1, win->sy - 1, winStyles[style].upcornleft, color);
		}
		vdc_hchar(win->sx, win->sy - 1, winStyles[style].up, color, win->wx);
		if (border & WIN_BOR_RI)
		{
			vdc_printc(win->sx + win->wx, win->sy - 1, winStyles[style].upcornright, color);
		}
	}
	if ((border & WIN_BOR_BO) && win->sy + win->wy < vdc_state.height)
	{
		if (border & WIN_BOR_LE)
		{
			vdc_printc(win->sx - 1, win->sy + win->wy, winStyles[style].bocornleft, color);
		}
		vdc_hchar(win->sx, win->sy + win->wy, winStyles[style].down, color, win->wx);
		if (border & WIN_BOR_RI)
		{
			vdc_printc(win->sx + win->wx, win->sy + win->wy, winStyles[style].bocornright, color);
		}
	}

	// Draw middle section
	for (char i = 0; i < win->wy; i++)
	{
		if (border & WIN_BOR_LE)
		{
			vdc_printc(win->sx - 1, win->sy + i, winStyles[style].left, color);
		}
		vdc_hchar(win->sx, win->sy + i, ' ', vdc_state.text_attr, win->wx);
		if (border & WIN_BOR_RI)
		{
			vdc_printc(win->sx + win->wx, win->sy + i, winStyles[style].right, color);
		}
	}
}

void vdcwin_winstorage_init(char memcr, char *membase, unsigned memsize)
// Initialise window background saving storage
{
	winCfg.memcr = memcr;
	winCfg.membase = membase;
	winCfg.memsize = memsize;
	winCfg.active = 0;
	winCfg.memactive = membase;
}

bool vdcwin_win_new(char border, char xpos, char ypos, char width, char height)
// Create new window with background save
{
	// Local vars
	char iwidth = width, iheight = height, line;
	unsigned size;
	unsigned vdcaddress = vdc_coords(xpos, ypos);

	// Derive width and height including potential border
	if ((border & WIN_BOR_LE) && xpos)
	{
		iwidth++;
		vdcaddress--;
	}
	if ((border & WIN_BOR_RI) && xpos + width < vdc_state.width)
	{
		iwidth++;
	}
	if ((border & WIN_BOR_UP) && ypos)
	{
		iheight++;
		vdcaddress -= vdc_state.width;
	}
	if ((border & WIN_BOR_BO) && ypos + height < vdc_state.height)
	{
		iheight++;
	}

	// Calculate needed memory and return if insufficient memory left
	{
		unsigned long needed = (unsigned long)iwidth * (unsigned long)iheight * 2UL;
		unsigned long limit = (unsigned long)winCfg.membase + (unsigned long)winCfg.memsize - 2UL;
		if (needed > 0xffffUL ||
			(unsigned long)winCfg.memactive + needed > limit)
		{
			return false;
		}
		size = needed;
	}

	// Check if not at window maximum
	if (winCfg.active == WIN_MAX_NR)
	{
		return false;
	}

	// Set window
	winCfg.active++;
	vdcwin_init(&windows[winCfg.active - 1].win, xpos, ypos, width, height);
	windows[winCfg.active - 1].memaddress = winCfg.memactive;
	windows[winCfg.active - 1].border = border;

	// Copy background
	for (line = 0; line < iheight; line++)
	{
		// Text
		bnk_cpyfromvdc(winCfg.memcr, winCfg.memactive, vdc_state.base_text + vdcaddress, iwidth);
		winCfg.memactive += iwidth;
		// Color
		bnk_cpyfromvdc(winCfg.memcr, winCfg.memactive, vdc_state.base_attr + vdcaddress, iwidth);
		winCfg.memactive += iwidth;
		vdcaddress += vdc_state.width;
	}

	// Clear window and draw desired borders
	vdcwin_border_clear(&windows[winCfg.active - 1].win, border);

//	// Debug
//	sprintf(linebuffer,"Active: %2u Base: %4X Size: %4X New: %4X",winCfg.active,winCfg.membase,winCfg.memsize,winCfg.memactive);
//	vdc_prints(0,23,linebuffer);

	return true;
}

void vdcwin_win_free()
// Close window and restore background
{
	// Local vars
	char iwidth, iheight, line, border;
	unsigned vdcaddress;

	// Return if no windows active
	if (!winCfg.active)
	{
		return;
	}

	// Derive width and height including potential border
	vdcaddress = vdc_coords(windows[winCfg.active - 1].win.sx, windows[winCfg.active - 1].win.sy);
	iwidth = windows[winCfg.active - 1].win.wx;
	iheight = windows[winCfg.active - 1].win.wy;
	border = windows[winCfg.active - 1].border;
	if ((border & WIN_BOR_LE) && windows[winCfg.active - 1].win.sx)
	{
		iwidth++;
		vdcaddress--;
	}
	if ((border & WIN_BOR_RI) && windows[winCfg.active - 1].win.sx + windows[winCfg.active - 1].win.wx < vdc_state.width)
	{
		iwidth++;
	}
	if ((border & WIN_BOR_UP) && windows[winCfg.active - 1].win.sy)
	{
		iheight++;
		vdcaddress -= vdc_state.width;
	}
	if ((border & WIN_BOR_BO) && windows[winCfg.active - 1].win.sy + windows[winCfg.active - 1].win.wy < vdc_state.height)
	{
		iheight++;
	}

	// Restore background
	winCfg.memactive = windows[winCfg.active - 1].memaddress;
	for (line = 0; line < iheight; line++)
	{
		// Text
		bnk_cpytovdc(vdc_state.base_text + vdcaddress, winCfg.memcr, winCfg.memactive, iwidth);
		winCfg.memactive += iwidth;
		// Color
		bnk_cpytovdc(vdc_state.base_attr + vdcaddress, winCfg.memcr, winCfg.memactive, iwidth);
		winCfg.memactive += iwidth;
		vdcaddress += vdc_state.width;
	}

	// Make previous window if any active
	winCfg.memactive = windows[winCfg.active - 1].memaddress;
	winCfg.active--;

//	// Debug
//	sprintf(linebuffer,"Active: %2u Base: %4X Size: %4X New: %4X",winCfg.active,winCfg.membase,winCfg.memsize,winCfg.memactive);
//	vdc_prints(0,23,linebuffer);
}

void vdcwin_viewport_init(struct VDCViewport *vp, char sourcebank, char *sourcebase, unsigned sourcewidth, unsigned sourceheight, unsigned viewwidth, unsigned viewheight, char viewsx, char viewsy)
// Initialize a viewport of screen data in memory
// Inpuut: Viewport struct to use, source bank and basem source dimensions, view window start coord and dimensions
{
	vp->sourcebank = sourcebank;
	vp->sourcebase = sourcebase;
	vp->sourcewidth = sourcewidth;
	vp->sourceheight = sourceheight;
	vp->sourcexoffset = 0;
	vp->sourceyoffset = 0;
	vdcwin_init(&vp->view, viewsx, viewsy, viewwidth, viewheight);
}

void vdcwin_cpy_viewport(struct VDCViewport *viewport)
// Function to copy a viewport on the source screen map to the VDC
// Input: Initialised viewport struct
{
	// Charachters
	unsigned vdcbase = viewport->view.sp;
	char *address = viewport->sourcebase + (viewport->sourceyoffset * viewport->sourcewidth) + viewport->sourcexoffset;

	for (char i = 0; i < viewport->view.wy; i++)
	{
		bnk_cpytovdc(vdcbase, viewport->sourcebank, address, viewport->view.wx);
		vdcbase += vdc_state.width;
		address += viewport->sourcewidth;
	}

	// Attributes
	vdcbase = viewport->view.cp;
	address = viewport->sourcebase + (viewport->sourceyoffset * viewport->sourcewidth) + viewport->sourcexoffset + (viewport->sourceheight * viewport->sourcewidth) + 48;

	for (char i = 0; i < viewport->view.wy; i++)
	{
		bnk_cpytovdc(vdcbase, viewport->sourcebank, address, viewport->view.wx);
		vdcbase += vdc_state.width;
		address += viewport->sourcewidth;
	}
}

void vdcwin_viewportscroll(struct VDCViewport *viewport, char direction)
// Function to scroll a viewport on the source screen map on the VDC in the given direction
// Input: Initialised viewport struct and direction
{
	struct VDCViewport vp_fill;

	memcpy(&vp_fill, viewport, sizeof(vp_fill));

	if (direction & SCROLL_LEFT)
	{
		vdcwin_scroll_right(&viewport->view, 1);
		vp_fill.sourcexoffset--;
		viewport->sourcexoffset--;
		vdcwin_init(&vp_fill.view, viewport->view.sx, viewport->view.sy, 1, viewport->view.wy);
	}
	if (direction & SCROLL_RIGHT)
	{
		vdcwin_scroll_left(&viewport->view, 1);
		vp_fill.sourcexoffset += viewport->view.wx;
		viewport->sourcexoffset++;
		vdcwin_init(&vp_fill.view, viewport->view.sx + viewport->view.wx - 1, viewport->view.sy, 1, viewport->view.wy);
	}
	if (direction & SCROLL_UP)
	{
		vdcwin_scroll_down(&viewport->view, 1);
		vp_fill.sourceyoffset--;
		viewport->sourceyoffset--;
		vdcwin_init(&vp_fill.view, viewport->view.sx, viewport->view.sy, viewport->view.wx, 1);
	}
	if (direction & SCROLL_DOWN)
	{
		vdcwin_scroll_up(&viewport->view, 1);
		vp_fill.sourceyoffset += viewport->view.wy;
		viewport->sourceyoffset++;
		vdcwin_init(&vp_fill.view, viewport->view.sx, viewport->view.sy + viewport->view.wy - 1, viewport->view.wx, 1);
	}
	vdcwin_cpy_viewport(&vp_fill);
}
