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

#ifndef VDC_CORE_H
#define VDC_CORE_H

// VDC color values
#define VDC_BLACK 0
#define VDC_DGREY 1
#define VDC_DBLUE 2
#define VDC_LBLUE 3
#define VDC_DGREEN 4
#define VDC_LGREEN 5
#define VDC_DCYAN 6
#define VDC_LCYAN 7
#define VDC_DRED 8
#define VDC_LRED 9
#define VDC_DPURPLE 10
#define VDC_LPURPLE 11
#define VDC_DYELLOW 12
#define VDC_LYELLOW 13
#define VDC_LGREY 14
#define VDC_WHITE 15

// VDC attribute values
#define VDC_A_BLINK 16
#define VDC_A_UNDERLINE 32
#define VDC_A_REVERSE 64
#define VDC_A_ALTCHAR 128

// Character codes
#define C_SPACE 0x20
#define C_ARROW 0x5B

// Function prototypes
char screen_width();
void screen_setmode(char mode);
void fastmode(char set);
char pet2screen(char p);
void vdc_set_disp_address(unsigned text, unsigned attr);
void vdc_set_charset_address(unsigned address);
void vdc_set_multab();
char vdc_set_mode(char mode);
void vdc_init(char mode, char extmem);
void vdc_exit();
unsigned vdc_coords(char x, char y);
void vdc_restore_charsets();
void vdc_detect_mem_size();
void vdc_disable_display();
void vdc_enable_display();
void vdc_block_fill(unsigned address, char value, char length);
void vdc_block_copy_page(unsigned dest, unsigned src, char length);
void vdc_block_copy(unsigned dest, unsigned src, unsigned length);
void vdc_scroll_copy(unsigned dest, unsigned src, char lines, char length);
void vdc_wipe_mem();
void vdc_set_extended_memsize();
void vdc_set_default_memsize();
void vdc_bgcolor(char color);
void vdc_fgcolor(char color);
void vdc_textcolor(char color);
void vdc_altchar(char set);
void vdc_blink(char set);
void vdc_underline(char set);
void vdc_reverse(char set);
void vdc_printc(char x, char y, char val, char attr);
void vdc_prints_attr(char x, char y, const char *string, char attr);
void vdc_prints(char x, char y, const char *string);
void vdc_hchar(char x, char y, char val, char attr, char length);
void vdc_vchar(char x, char y, char val, char attr, char length);
void vdc_clear(char x, char y, char val, char length, char lines);
void vdc_cls();
void vdc_wait_vblank();
void vdc_wait_no_vblank();
void vdc_pass_vblank();

// Global variables
enum VDCMode
{
    VDC_TEXT_80x25_PAL,
    VDC_TEXT_80x50_PAL,
    VDC_TEXT_80x70_PAL,
    VDC_TEXT_80x25_NTSC,
    VDC_TEXT_80x50_NTSC,
    VDC_TEXT_80x60_NTSC
};
struct VDCModeSet
{
    unsigned width;
    unsigned height;
    char extmem;
    unsigned base_text;
    unsigned base_attr;
    unsigned swap_text;
    unsigned swap_attr;
    unsigned char_std;
    unsigned char_alt;
    unsigned extended;
    char regset[17];
};
extern struct VDCModeSet vdc_modes[6];
struct VDCStatus
{
    char memsize;
    char memextended;
    char mode;
    unsigned width;
    unsigned height;
    char text_attr;
    unsigned base_text;
    unsigned base_attr;
    unsigned swap_text;
    unsigned swap_attr;
    unsigned char_std;
    unsigned char_alt;
    unsigned extended;
    unsigned dispaddr_offset;
    char disp_skip;
};
extern struct VDCStatus vdc_state;
extern char linebuffer[81];
extern unsigned multab[72];

#pragma compile("vdc_core.c")

#endif