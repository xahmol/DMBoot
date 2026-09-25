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

#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <petscii.h>
#include <c64/kernalio.h>
#include <c128/vdc.h>
#include "vdc_core.h"
#include "banking.h"
#include "peekpoke.h"

char linebuffer[81];
struct VDCStatus vdc_state;
unsigned multab[72];

// VDC mode settings. Credits to Tokra.
struct VDCModeSet vdc_modes[6] =
    {
        {80, 25, 0, 0x0000, 0x0800, 0x1000, 0x1800, 0x2000, 0x3000, 0x4000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x26, VDCR_VADJUST, 0xe0, VDCR_VDISPLAY, 0x19, VDCR_VSYNC, 0x20, VDCR_LACE, 0xfc, VDCR_CSIZE, 0xe7, VDCR_REFRESH, 0x7e, 255}},
        {80, 50, 1, 0x0000, 0x1000, 0x4000, 0x5000, 0x2000, 0x3000, 0x6000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x4d, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x32, VDCR_VSYNC, 0x40, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        {80, 70, 1, 0x0000, 0x1800, 0x6000, 0x7800, 0x4000, 0x5000, 0x9000, {VDCR_HTOTAL, 0x7f, VDCR_VTOTAL, 0x4d, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x46, VDCR_VSYNC, 0x48, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        {80, 25, 0, 0x0000, 0x0800, 0x1000, 0x1800, 0x2000, 0x3000, 0x4000, {VDCR_HTOTAL, 0x7e, VDCR_VTOTAL, 0x20, VDCR_VADJUST, 0xe0, VDCR_VDISPLAY, 0x19, VDCR_VSYNC, 0x1d, VDCR_LACE, 0xfc, VDCR_CSIZE, 0xe7, VDCR_REFRESH, 0xf5, 255}},
        {80, 50, 1, 0x0000, 0x1000, 0x4000, 0x5000, 0x2000, 0x3000, 0x6000, {VDCR_HTOTAL, 0x7e, VDCR_VTOTAL, 0x41, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x32, VDCR_VSYNC, 0x3b, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}},
        {80, 60, 1, 0x0000, 0x1800, 0x6000, 0x7800, 0x4000, 0x5000, 0x9000, {VDCR_HTOTAL, 0x7e, VDCR_VTOTAL, 0x41, VDCR_VADJUST, 0x00, VDCR_VDISPLAY, 0x3c, VDCR_VSYNC, 0x3d, VDCR_LACE, 0x03, VDCR_CSIZE, 0x07, VDCR_REFRESH, 0x00, 255}}};

char screen_width()
// Return screenwidth 40 or 80
{
    if (*(volatile char *)0xd7 >= 128)
    {
        return 80;
    }
    else
    {
        return 40;
    }
}

void screen_setmode(char mode)
// Set mode of text screen: inpout 40 or 80 for repp. VIC-II or VDC mode
{
    if (mode != screen_width())
    {
        __asm
        {
            jsr 0xff5f
        }
    }
}

void fastmode(char set)
// Set (1) or disable (0) fast 2 MHz mode. Set blanks VIC.
{
    if (set)
    {
        POKE(0xd011, PEEK(0xd011) & (~(1 << 4))); // Disable the 5th bit of the SCROLY register to blank VIC screen
        POKE(0xd011, PEEK(0xd011) & (~(1 << 7))); // Disable the 8th bit of the SCROLY register to avoid accidentally setting raster interrupt to high
        POKE(0xd030, 1);                          // Set C128 speed to FAST (2 Mhz)
    }
    else
    {
        POKE(0xd030, 0);                          // Switch to 1Mhz mode
        POKE(0xd011, PEEK(0xd011) | (1 << 4));    // Enable the 5th bit of the SCROLY register to blank VIC screen
        POKE(0xd011, PEEK(0xd011) & (~(1 << 7))); // Disable the 8th bit of the SCROLY register to avoid accidentally setting raster interrupt to high
    }
}

char pet2screen(char p)
// Convert Petscii values to screen code values
{
    if (p < 32)
        p = p + 128;
    else if (p > 63 && p < 96)
        p = p - 64;
    else if (p > 95 && p < 128)
        p = p - 32;
    else if (p > 127 && p < 160)
        p = p + 64;
    else if (p > 159 && p < 192)
        p = p - 64;
    else if (p > 191 && p < 255)
        p = p - 128;
    else if (p == 255)
        p = 94;

    return p;
}

void vdc_set_disp_address(unsigned text, unsigned attr)
// Function to set the VDC display addresses for text and attributes
{
    // Text
    vdc_reg_write(VDCR_DISP_ADDRH, text >> 8); // Set high byte of text address
    vdc_reg_write(VDCR_DISP_ADDRL, text);      // Set low byte of text address
    // Attribute
    vdc_reg_write(VDCR_ATTR_ADDRH, attr >> 8); // Set high byte of attribute address
    vdc_reg_write(VDCR_ATTR_ADDRL, attr);      // Set low byte of attribute address
}

void vdc_set_charset_address(unsigned address)
// Function to set the VDC display addresses for text and attributes
{
    vdc_reg_write(VDCR_CHAR_ADDRH, ((vdc_reg_read(VDCR_CHAR_ADDRH) & 0x10) | ((address >> 8) & 0xe0)));
}

void vdc_set_multab()
// Set multiplication table for screen width
{
    unsigned val = 0;
    for (char index = 0; index < vdc_state.height; index++)
    {
        multab[index] = val;
        val += vdc_state.width + vdc_state.disp_skip;
    }
}

char vdc_set_mode(char mode)
// Function to set one of the preset VDC modes. Returns 1=succes, 0=fail.
{
    char index = 0;
    unsigned val;

    // Check if extended memory is required and available. If not, return with code 0.
    if (vdc_modes[mode].extmem && vdc_state.memsize == 16)
    {
        return 0;
    }

    // Set screen state
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
    vdc_state.text_attr = VDC_LYELLOW+VDC_A_ALTCHAR;
    vdc_state.dispaddr_offset = 0;
    vdc_state.disp_skip = 0;

    // Set multiplication table for screen width
    vdc_set_multab();

    // Set VDC addresses
    vdc_disable_display();
    vdc_set_disp_address(vdc_modes[mode].base_text, vdc_modes[mode].base_attr);
    vdc_set_charset_address(vdc_modes[mode].char_std);
    vdc_restore_charsets();

    index = 0;
    do
    {
        vdc_reg_write(vdc_modes[mode].regset[index++], vdc_modes[mode].regset[index++]);
    } while (vdc_modes[mode].regset[index] != 255);

    // Check if extended memory is required and not yet set. If so, set.
    if (vdc_modes[mode].extmem && !vdc_state.memextended)
    {
        vdc_set_extended_memsize();
    }

    vdc_cls();
    vdc_enable_display();
    return 1;
}

void vdc_init(char mode, char extmem)
// Initialize VDC screen
{
    // Function key values
    char functionkeys[20] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        133, 137, 134, 138, 135, 139, 136, 140, 10, 140};

    // Init function keys definitions
    memcpy((char*)0x1000,functionkeys,20);
    
    // Init screen colors
    vdc_bgcolor(VDC_BLACK);
    vdc_fgcolor(VDC_LYELLOW);
    vdc_state.text_attr = VDC_LYELLOW + VDC_A_ALTCHAR;

    // Detect VDC memsize and set to extended if 64 KB
    vdc_detect_mem_size();

    // Give message if 40 column screen is active and wait on key before switching to 80
    if (screen_width() == 40)
    {
        printf("switch to 80 column screen\nand press key.\n");
        getch();
        clrscr();
        screen_setmode(80);
    }

    // Set 2 Mhz mode
    fastmode(1);

    // Set screen mode
    vdc_set_mode(mode);

    // If 64 KB VDC and extmem is requested, enable it.
    if (vdc_state.memsize == 64 && extmem)
    {
        vdc_set_extended_memsize();
    }
}

void vdc_exit()
// Return to normal state of VDC
{
    fastmode(0);                      // Disable fast mode
    vdc_set_mode(VDC_TEXT_80x25_PAL); // Set default mode
    vdc_cls();                        // Clear screen
    bnk_exit();                       // Reset shared memory to default
}

unsigned vdc_coords(char x, char y)
// Function returns a VDC memory address for given x,y coords. To be added to base address for text or attributes.
{
    return multab[y] + x;
}

void vdc_restore_charsets()
// Restore charsets from ROM
{
    bnk_redef_charset(vdc_state.char_std, BNK_CHARROM, (char *)0xd000, 512);
}

void vdc_detect_mem_size()
// Function to detect VDC memory size. Returns size in KB in glbal variable.
{
    // Reading register 28, safeguarding value, setting bit 4 and storing back to register 28
    char ramtype = vdc_reg_read(VDCR_CHAR_ADDRH);
    vdc_reg_write(VDCR_CHAR_ADDRH, ramtype | 0x10);

    // Writing a $00 value to VDC $1fff
    vdc_mem_write_at(0x1fff, 0x00);

    // Writing a $ff value to VDC $9fff
    vdc_mem_write_at(0x9fff, 0xff);

    // Reading back value of VDC $1fff and comparing value with $00 to see if 64KB address could be read
    // If value has remained 0, then 64 KB VDC mem is detected, else 16.
    vdc_state.memsize = (vdc_mem_read_at(0x1fff) == 0x00) ? 64 : 16;

    // Restore bit 4 of register 28
    vdc_reg_write(VDCR_CHAR_ADDRH, ramtype);

    // Do a clear screen
    vdc_cls();
}

void vdc_disable_display()
// Function to disable VDC display
{
    vdc_reg_write(VDCR_HSTART, 0x80);
}

void vdc_enable_display()
// Function to enable VDC display
{
    vdc_reg_write(VDCR_HSTART, 0x7d);
}

void vdc_block_fill(unsigned address, char value, char length)
// Function to flll VDC area with blockfill
// Input:   address =   start address
//          value   =   value to fill area with
//          length  =   number of positions to fill, zero based, max 255
{
    vdc_mem_addr(address);                                          // Set VDC address
    vdc_write(value);                                               // Write value to data register
    vdc_reg_write(VDCR_VSCROLL, vdc_reg_read(VDCR_VSCROLL) & 0x7f); // Clear copy bit (bit 7) of register 24
    vdc_reg_write(VDCR_DSIZE, length);                              // Set block copy length
}

void vdc_block_copy_page(unsigned dest, unsigned src, char length)
// Function to copy maximum a page within VDC memory using fast block copy
// Input: Destination (dest) amd source (src) addresses, length max 255 zero based
{
    // Set base addresses
    vdc_mem_addr(dest);                                             // Set VDC destination address
    vdc_reg_write(VDCR_VSCROLL, vdc_reg_read(VDCR_VSCROLL) | 0x80); // Set copy bit (bit 7) of registerv 24
    vdc_reg_write(VDCR_BLOCK_ADDRH, src >> 8);                      // Set high byte of source address
    vdc_reg_write(VDCR_BLOCK_ADDRL, src);                           // Set low byte of source address
    vdc_reg(VDCR_DATA);                                             // Write to VDC

    // Set length
    vdc_reg_write(VDCR_DSIZE, length); // Set length in register 30
}

void vdc_block_copy(unsigned dest, unsigned src, unsigned length)
// Function to copy multiple pages within VDC memory using fast block copy
// Input: Destination (dest) amd source (src) addresses, length zero based
{
    char pages = length / 256;    // Calculate number of pages
    char lastpage = length % 256; // Calculate length left after doing all full pages

    // Copy full pages
    for (char page = 0; page < pages; page++)
    {
        vdc_block_copy_page(dest, src, 255);
        dest += 256;
        src += 256;
    }

    // Copy length left
    vdc_block_copy_page(dest, src, lastpage);
}

void vdc_scroll_copy(unsigned dest, unsigned src, char lines, char length)
// Function to copy a window of lines by length within VDC memory to another location
// Source address is address of upper left corner
{
    for (char line = 0; line < lines; line++)
    {
        vdc_block_copy_page(dest, src, length);
        src += vdc_state.width;
        dest += vdc_state.width;
    }
}

void vdc_wipe_mem()
// Function to wipe VDC memory to avoid visible screen corruption on VDC mem lauout change
{
    unsigned address = 0;

    for (char x = 0; x < 255; x++)
    {
        vdc_block_fill(address, 0, 255);
        address += 256;
    }
    vdc_block_fill(address, 0, 255);
}

void vdc_set_extended_memsize()
// Function to set VDC in 64k memory configuration
{
    // Check if 64 KB VDC, return if 16. Also return if already set.
    if (vdc_state.memsize == 16 || vdc_state.memextended)
    {
        return;
    }

    vdc_disable_display();                                                // Disable display to not show artifacts
    vdc_wipe_mem();                                                       // Wipe memory to avoid artifacts
    vdc_reg_write(VDCR_CHAR_ADDRH, vdc_reg_read(VDCR_CHAR_ADDRH) | 0x10); // Setting memory mode to 64KB by setting bit 4 of register 28
    vdc_restore_charsets();                                               // Restore charsets from ROM
    vdc_cls();                                                            // CLear VDC screen with spaces in color ywllow
    vdc_enable_display();                                                 // Enable display again
    vdc_state.memextended = 1;                                            // Set state flag
}

void vdc_set_default_memsize()
// Function to set VDC in default memory configuration
{
    // Check if already in default mode
    if (!vdc_state.memextended)
    {
        return;
    }

    vdc_disable_display();                                                // Disable display to not show artifacts
    vdc_wipe_mem();                                                       // Wipe memory to avoid artifacts
    vdc_reg_write(VDCR_CHAR_ADDRH, vdc_reg_read(VDCR_CHAR_ADDRH) & 0xef); // Setting memory mode to 64KB by clearing bit 4 of register 28
    vdc_restore_charsets();                                               // Restore charsets from ROM
    vdc_cls();                                                            // CLear VDC screen with spaces in color ywllow
    vdc_enable_display();                                                 // Enable display again
    vdc_state.memextended = 0;                                            // Set state flag
}

void vdc_bgcolor(char color)
// Set VDC background color
{
    vdc_reg_write(VDCR_COLOR, (vdc_reg_read(VDCR_COLOR) & 0xf0) + color);
}

void vdc_fgcolor(char color)
// Set VDC foreground color
{
    vdc_reg_write(VDCR_COLOR, (vdc_reg_read(VDCR_COLOR) & 0x0f) + (color * 16));
}

void vdc_textcolor(char color)
// Set default text attributes
{
    // Set color while retaining bits 4-7
    vdc_state.text_attr = (vdc_state.text_attr & 0xf0) + color;
}

void vdc_altchar(char set)
// Clear (set=0) or set (set=1) alternate charset mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_ALTCHAR) : (vdc_state.text_attr & ~VDC_A_ALTCHAR);
}

void vdc_blink(char set)
// Clear (set=0) or set (set=1) blink mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_BLINK) : (vdc_state.text_attr & ~VDC_A_BLINK);
}

void vdc_underline(char set)
// Clear (set=0) or set (set=1) underline mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_UNDERLINE) : (vdc_state.text_attr & ~VDC_A_UNDERLINE);
}

void vdc_reverse(char set)
// Clear (set=0) or set (set=1) reverse mode
{
    vdc_state.text_attr = (set) ? (vdc_state.text_attr | VDC_A_REVERSE) : (vdc_state.text_attr & ~VDC_A_REVERSE);
}

void vdc_printc(char x, char y, char val, char attr)
// Function to plot a char at a given coordinate
{
    unsigned address = vdc_coords(x, y);
    vdc_mem_write_at(address + vdc_state.base_text, val);
    vdc_mem_write_at(address + vdc_state.base_attr, attr);
}

void vdc_prints_attr(char x, char y, const char *string, char attr)
// Function to plot a string at a given coordinate with given attributes
{
    unsigned address = vdc_coords(x, y);
    char len = strlen(string);

    // Check for legth. Return if 0 and use printc in case of only one char
    if(!len)
    {
        return;
    }
    if(len==1)
    {
        vdc_printc(x,y,string[0],attr);
        return;
    }

    // Print text
    vdc_mem_addr(address + vdc_state.base_text);
    for (char i = 0; i < len; i++)
    {
        vdc_write(pet2screen(string[i]));
    }

    // Color
    vdc_block_fill(address + vdc_state.base_attr, attr, len - 1);
}

void vdc_prints(char x, char y, const char *string)
// Function to plot a string at a given coordinate with active attributes
{
    vdc_prints_attr(x, y, string, vdc_state.text_attr);
}

void vdc_hchar(char x, char y, char val, char attr, char length)
// Function to plot horizontal line using block copy
// vdc_block_fill writes one byte plus a zero-based block of length - 1 bytes,
// and a VDC block count of 0 means 256: length 1 would fill 257 positions and
// length 0 would underflow. Handle both separately.
{
    if (!length)
    {
        return;
    }
    if (length == 1)
    {
        vdc_printc(x, y, val, attr);
        return;
    }

    unsigned address = vdc_coords(x, y);
    vdc_block_fill(address + vdc_state.base_text, val, length - 1);  // Text
    vdc_block_fill(address + vdc_state.base_attr, attr, length - 1); // Attributes
}

void vdc_vchar(char x, char y, char val, char attr, char length)
// Function to plot vertical line from top to bottom
{
    for (char i = y; i < y + length; i++)
    {
        vdc_printc(x, i, val, attr);
    }
}

void vdc_clear(char x, char y, char val, char length, char lines)
// Function to clear VDC area with given value and attribute
{
    for (char i = y; i < y + lines; i++)
    {
        vdc_hchar(x, i, val, vdc_state.text_attr, length);
    }
}

void vdc_cls()
// Function to clear VDC screen with given value and attribute
{
    vdc_clear(0, 0, C_SPACE, vdc_state.width, vdc_state.height);
}

static __native inline void vdc_wait_vblank()
// Function to wait until VDC VBLANK
{
    // Loop while bit 5 is on
    do
    {
    } while (!(vdc.addr & 0x20));
}

static __native inline void vdc_wait_no_vblank()
// Function to wait until after VDC VBLANK
{
    // Loop while bit 5 is on
    do
    {
    } while (vdc.addr & 0x20);
}

static __native inline void vdc_pass_vblank()
// Function to include a delay to wait for first scanlines passing
{
    vdc_wait_vblank();
    vdc_wait_no_vblank();
}