/*
DMBoot 128 v5 - Clean return to BASIC 7

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Shared by DMBoot and the upgrade tool dmbupd45. Oscar64's zero page use
overlaps BASIC 7 work storage, so it is saved at the start of main() and
restored on exit; the screen editor's function key expansion is switched
off while DMBoot runs.

Code and resources from others used:
-   cc65 by Ullrich von Bassewitz et al. (https://github.com/cc65/cc65),
    libsrc/c128/cgetc.s: key store vector and editor entry point used to
    switch off the function key expansion. Adapted: C, saved once at
    start-up, restored in dmb_exit.
*/

#include "basicexit.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// Zero page used by Oscar64 (default layout, MachineTypes.cpp): registers
// $02-$26 and $43-$62, automatic zero page variables $F7-$FF. On the C128
// these overlap BASIC 7 work storage, which BASIC does not reinitialise on
// RUN, so they are saved at start-up and restored on exit (the cc65 runtime
// of v4 did the same).
#define ZP_LOW_START    0x02
#define ZP_LOW_SIZE     0x25    // $02-$26
#define ZP_MID_START    0x43
#define ZP_MID_SIZE     0x20    // $43-$62
#define ZP_HIGH_START   0xf7
#define ZP_HIGH_SIZE    0x09    // $F7-$FF

static char zp_save_low[ZP_LOW_SIZE];
static char zp_save_mid[ZP_MID_SIZE];
static char zp_save_high[ZP_HIGH_SIZE];

// ---------------------------------------------------------------------------
// Title:       Save BASIC zero page
// Description: Copies the zero page ranges Oscar64 uses to a buffer. Must be
//              the first call in main(): only the start-up code has run
//              before it (it sets ip $19-$1A and sp $23-$24, which BASIC
//              reinitialises itself, see dmb_exit).
// Syntax:      void dmb_zp_save(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void dmb_zp_save(void)
{
    __asm
    {
        ldx #0
lsave1:
        lda ZP_LOW_START, x
        sta zp_save_low, x
        inx
        cpx #ZP_LOW_SIZE
        bne lsave1
        ldx #0
lsave2:
        lda ZP_MID_START, x
        sta zp_save_mid, x
        inx
        cpx #ZP_MID_SIZE
        bne lsave2
        ldx #0
lsave3:
        lda ZP_HIGH_START, x
        sta zp_save_high, x
        inx
        cpx #ZP_HIGH_SIZE
        bne lsave3
    }
}

// Screen editor key store vector and the editor entry point after its
// function key expansion (C128 KERNAL, as used by cc65).
#define KEYSTORE_VECTOR     0x033c
#define KEYSTORE_NOFKEYS    0xc6b7

static unsigned keystore_saved;

// ---------------------------------------------------------------------------
// Title:       Raw function keys
// Description: Switches off the screen editor's function key expansion, so
//              F1-F8 and HELP return their own key codes instead of their
//              strings (F7 gave "LIST" + RETURN, which started slot L).
//              dmb_exit restores the original vector.
//              Based on cc65 libsrc/c128/cgetc.s (see the file header).
// Syntax:      void dmb_fkeys_raw(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void dmb_fkeys_raw(void)
{
    volatile unsigned *vector = (volatile unsigned *)KEYSTORE_VECTOR;

    keystore_saved = *vector;
    __asm { sei }
    *vector = KEYSTORE_NOFKEYS;
    __asm { cli }
}

// ---------------------------------------------------------------------------
// Title:       Exit to BASIC (C128)
// Description: Ends the program and returns to BASIC from anywhere. Restores
//              the zero page saved by dmb_zp_save (BASIC 7 work storage
//              that Oscar64 uses; without it BASIC programs that use
//              variables or string functions crashed after a slot start),
//              then resets the string temporaries as Oscar64 crt.c spexit
//              does for the C128 ($13, $16, $18, $1A: the locations the
//              start-up code overwrote), and the editor key store vector
//              changed by dmb_fkeys_raw. No C code may run after the
//              restore, so everything is in assembler.
// Syntax:      void dmb_exit(void);
// Input:       None
// Output:      Does not return (returns to BASIC)
// ---------------------------------------------------------------------------
void dmb_exit(void)
{
    __asm
    {
        ldx spentry
        txs
        ldx #0
lrest1:
        lda zp_save_low, x
        sta ZP_LOW_START, x
        inx
        cpx #ZP_LOW_SIZE
        bne lrest1
        ldx #0
lrest2:
        lda zp_save_mid, x
        sta ZP_MID_START, x
        inx
        cpx #ZP_MID_SIZE
        bne lrest2
        ldx #0
lrest3:
        lda zp_save_high, x
        sta ZP_HIGH_START, x
        inx
        cpx #ZP_HIGH_SIZE
        bne lrest3
        lda keystore_saved + 1
        beq lnokey
        sei
        lda keystore_saved
        sta KEYSTORE_VECTOR
        lda keystore_saved + 1
        sta KEYSTORE_VECTOR + 1
        cli
lnokey:
        lda #0
        sta $13
        sta $1a
        lda #$1b
        sta $18
        lda #$19
        sta $16
    }
}
