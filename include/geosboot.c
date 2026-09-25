/*
DMBoot 128 v5 - GEOS 128 RAM boot (low-memory code)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Code and resources from others used:
-   GRB128 "GEOS 128 restart from REU image" by Bart van Leeuwen (2021,
    public domain, https://github.com/bvl1999); earlier ca65 port in
    DMBoot v4 (src/geosramroutine.s, branch legacy-cc65).
    Adapted: Oscar64 inline assembly in the low-memory code (it needs the
    bottom 16 KB of common RAM); the "no REU" error path now restores the
    memory configuration before returning (it left the ROMs switched in);
    the "no loader" path restores DMBoot's common RAM setting.
*/

#include "defines.h"
#include "geosboot.h"

// ===========================================================================
// Low-memory code (LMC) part
// ===========================================================================
#pragma code(bcode1)
#pragma data(bdata1)
#pragma bss(bbss1)

#define GEOS_RESET_HANDLER  0x03e4  // GEOS reset handler location
#define GEOS_SIG_OFFSET     0xc006  // "GEOS BOOT" signature of the rboot loader
#define GEOS_LOADER         0xc000
#define MMU_BANK15          0x00    // $FF00: all ROMs, I/O
#define MMU_BANK1_IO        0x7e    // $FF00: RAM bank 1, I/O
#define RCR_GEOS_SHARED     0x47    // $D506: 16 KB common top and bottom, DMA/VIC to bank 1

// "GEOS BOOT" as stored by the rboot loader
static const char geos_sig[9] = { 0x47, 0x45, 0x4f, 0x53, 0x20, 0x42, 0x4f, 0x4f, 0x54 };

// REU registers $DF01-$DF09: fetch the rboot loader to bank 1 $C000
static const char geos_reudata[9] = { 0x91, 0x00, 0xc0, 0x40, 0xbc, 0x00, 0x80, 0x00, 0x00 };

// Reset handler copied to $03E4: lda #$7e / sta $ff00 / jmp $c000
static const char geos_resetcode[8] = { 0xa9, 0x7e, 0x8d, 0x00, 0xff, 0x4c, 0x00, 0xc0 };

static char geos_saved_mmu;
static volatile char geos_error;

// ---------------------------------------------------------------------------
// Title:       Start GEOS 128 from an REU image
// Description: With the GEOS REU image already loaded into the REU: checks
//              the REU, switches to 16 KB common RAM with DMA to bank 1,
//              puts GEOS's reset handler in place, copies the rboot loader
//              to bank 1 $C000 by DMA (at 1 MHz), checks its signature and
//              jumps to it. Must run from the low-memory code.
// Syntax:      char geos_boot(void);
// Input:       None (REU contents)
// Output:      Does not return on success; GEOS_ERROR_NOREU or
//              GEOS_ERROR_NOBOOT otherwise
// ---------------------------------------------------------------------------
char geos_boot(void)
{
    geos_error = 0;
    __asm
    {
        sei
        lda $ff00
        sta geos_saved_mmu
        lda #MMU_BANK15
        sta $ff00

        // REU check: the bank register forces its top 5 bits to 1
        lda $df06
        pha
        lda #$07
        sta $df06
        lda $df06
        cmp #$ff
        bne reuerror
        lda #$00
        sta $df06
        lda $df06
        cmp #$f8
        bne reuerror
        pla

        lda $d506
        ora #RCR_GEOS_SHARED
        sta $d506
        lda #MMU_BANK1_IO
        sta $ff00

        ldx #7
    copyreset:
        lda geos_resetcode, x
        sta GEOS_RESET_HANDLER, x
        dex
        bpl copyreset

        // 1 MHz for the DMA
        lda $d030
        and #$fe
        sta $d030

        ldy #8
    setreu:
        lda geos_reudata, y
        sta $df01, y
        dey
        bpl setreu

        ldx #8
    checksig:
        lda GEOS_SIG_OFFSET, x
        cmp geos_sig, x
        bne sigerror
        dex
        bpl checksig
        jmp GEOS_LOADER

    reuerror:
        pla
        sta $df06
        lda geos_saved_mmu
        sta $ff00
        lda #GEOS_ERROR_NOREU
        sta geos_error
        cli
        jmp done

    sigerror:
        ldx geos_saved_mmu
        stx $ff00
        lda #RCR_COMMON_8K_BOTTOM
        sta $d506
        lda #GEOS_ERROR_NOBOOT
        sta geos_error
        cli
    done:
    }
    return geos_error;
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
