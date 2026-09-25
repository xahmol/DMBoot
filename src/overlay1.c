/*
DMBoot 128 v5 - Phase 0 dummy overlay 1 (stored in bank 1 RAM)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <stdio.h>
#include <petscii.h>
#include "defines.h"
#include "overlay1.h"

#pragma overlay(dmbovl1, 2)
#pragma section(codeovl1, 0)
#pragma section(dataovl1, 0)
#pragma section(bssovl1, 0)
#pragma region(ovl1, OVERLAYLOAD, OVERLAY_SLOT_END, , 2, { codeovl1, dataovl1, bssovl1 })

#pragma code(codeovl1)
#pragma data(dataovl1)
#pragma bss(bssovl1)

// ---------------------------------------------------------------------------
// Title:       Overlay 1 self test
// Description: Prints a message from inside overlay 1 and returns its
//              signature, proving the overlay was copied into the load slot
//              and its code runs.
// Syntax:      char overlay1_selftest(void);
// Input:       None
// Output:      OVERLAY1_SIGNATURE
// ---------------------------------------------------------------------------
char overlay1_selftest(void)
{
    printf("Overlay 1 running (stored in bank 1 RAM)\n");
    return OVERLAY1_SIGNATURE;
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
