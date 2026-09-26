/*
DMBoot 128 v5 - Splash screen overlay

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Shows the PETSCII splash screen (40 or 80 columns) until a key is pressed,
as the UBoot64-v2 splash (https://github.com/xahmol/UBoot64-v2). The
screens come from assets/splash.petmate (Petmate9), converted to packed
data in src/splashdata.c by tools/petmate2c.py.

Small overlay: stored in bank 1 at $E000, so code and data must stay
within OVERLAY_SMALL_SIZE (the region below enforces it).
*/

#include <stddef.h>
#include <c64/vic.h>
#include <c128/vdc.h>
#include "defines.h"
#include "dualwin.h"
#include "vdc_core.h"
#include "core.h"
#include "splash.h"

#pragma overlay(dmbovl6, 7)
#pragma section(codeovl6, 0)
#pragma section(dataovl6, 0)
#pragma section(bssovl6, 0)
#pragma region(ovl6, OVERLAYLOAD, OVERLAYLOAD + OVERLAY_SMALL_SIZE, , 7, { codeovl6, dataovl6, bssovl6 })

#pragma code(codeovl6)
#pragma data(dataovl6)
#pragma bss(bssovl6)

#include "splashdata.c"

#define SPLASH_VIC_SCREEN   0x0400  // VIC text screen
#define SPLASH_VIC_COLORRAM 0xd800  // VIC colour RAM
#define SPLASH_VIC_CELLS    1000    // 40x25
#define SPLASH_VDC_CELLS    2000    // 80x25
#define PACK_RUN            0x80    // PackBits: control bytes from here are runs
#define PACK_RUN_BASE       257     // Run length = PACK_RUN_BASE - control byte

// ---------------------------------------------------------------------------
// Title:       Unpack a splash stream
// Description: Unpacks a PackBits stream (see tools/petmate2c.py) into C128
//              memory, or into VDC memory from the current VDC address
//              (the VDC advances its address after each write). Never
//              writes more than `size` bytes.
// Syntax:      static void splash_unpack(const char *src, volatile char *dst,
//                                        unsigned size);
// Input:       src  - packed stream
//              dst  - destination in bank 0, or NULL for VDC memory
//              size - number of bytes to write
// Output:      None
// ---------------------------------------------------------------------------
static void splash_unpack(const char *src, volatile char *dst, unsigned size)
{
    while (size)
    {
        char control = *src++;
        bool run = control >= PACK_RUN;
        unsigned count = run ? PACK_RUN_BASE - control : control + 1;

        if (count > size)
        {
            count = size;
        }
        size -= count;
        while (count--)
        {
            char value = run ? *src : *src++;
            if (dst)
            {
                *dst++ = value;
            }
            else
            {
                vdc_mem_write(value);
            }
        }
        if (run)
        {
            src++;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Show the splash screen
// Description: Draws the splash screen on the active screen and waits for
//              a key. In 40 columns the VIC is switched to the character
//              set of the design (upper case/graphics) and back to lower
//              case afterwards; border and background come from the
//              design. Afterwards the configured screen colours are set
//              again; the caller redraws the screen.
// Syntax:      void splash_show(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void splash_show(void)
{
    if (dwin_is80())
    {
        vdc_bgcolor(splash_vdc_background);
        vdc_mem_addr(vdc_state.base_text);
        splash_unpack(splash_vdc_codes, NULL, SPLASH_VDC_CELLS);
        vdc_mem_addr(vdc_state.base_attr);
        splash_unpack(splash_vdc_attrs, NULL, SPLASH_VDC_CELLS);
    }
    else
    {
        dwin_vic_charset(splash_vic_lowercase);
        vic.color_border = splash_vic_border;
        vic.color_back = splash_vic_background;
        splash_unpack(splash_vic_codes, (volatile char *)SPLASH_VIC_SCREEN, SPLASH_VIC_CELLS);
        splash_unpack(splash_vic_colours, (volatile char *)SPLASH_VIC_COLORRAM, SPLASH_VIC_CELLS);
    }

    key_wait();

    if (!dwin_is80())
    {
        dwin_vic_charset(true);
    }
    dwin_screen_colors(cfg.colors.border, cfg.colors.background);
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
