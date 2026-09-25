/*
DMBoot 128 v5 - Phase 0 dummy overlay 2 (stored in bank 0 RAM under the KERNAL ROM)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <stdio.h>
#include <petscii.h>
#include "defines.h"
#include <c64/vic.h>
#include "dualwin.h"
#include "overlay2.h"

#pragma overlay(dmbovl2, 3)
#pragma section(codeovl2, 0)
#pragma section(dataovl2, 0)
#pragma section(bssovl2, 0)
#pragma region(ovl2, OVERLAYLOAD, OVERLAY_SLOT_END, , 3, { codeovl2, dataovl2, bssovl2 })

#pragma code(codeovl2)
#pragma data(dataovl2)
#pragma bss(bssovl2)

// ---------------------------------------------------------------------------
// Title:       Overlay 2 self test
// Description: Prints a message in the console window from inside overlay 2 and returns its
//              signature, proving the overlay was copied into the load slot
//              and its code runs.
// Syntax:      char overlay2_selftest(void);
// Input:       None
// Output:      OVERLAY2_SIGNATURE
// ---------------------------------------------------------------------------
char overlay2_selftest(void)
{
    dwin_printf(&console, VCOL_LT_GREEN, "Overlay 2 running (stored in bank 0 RAM under the KERNAL ROM)\n");
    return OVERLAY2_SIGNATURE;
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
