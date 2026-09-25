/*
DMBoot 128 v5 - C128 Device Manager ROM extended API

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Based on the extapi.txt description of the C128 Device Manager ROM by
Bart van Leeuwen (2022).
Adapted: Oscar64 inline assembly, placed in the low-memory code overlay
(earlier ca65 version: DMBoot v4, branch legacy-cc65).
*/

#include "dmapi.h"
#include "banking.h"

// Device Manager ROM extended API jump table
#define DM_API_HEADER       0x807b  // "NED" identification bytes
#define DM_API_VERSION      0x807e  // Version: low byte, high byte
#define DM_EXT_GET_DRIVETYPE 0x8083
#define DM_EXT_GET_HS_ID    0x8086
#define DM_EXT_SET_HS_ID    0x8089
#define DM_ID_BYTE1         0x4e    // 'N'
#define DM_ID_BYTE2         0x45    // 'E'
#define DM_ID_BYTE3         0x44    // 'D'
#define DM_HYPERSPEED_ID8   0x08

// ===========================================================================
// Low-memory code (LMC) part
// ===========================================================================
#pragma code(bcode1)
#pragma data(bdata1)
#pragma bss(bbss1)

// Results of the API calls. They live in the LMC area below $8000, because
// only RAM below $8000 is visible while the function ROM is switched in.
char dm_saved_mmu;
char dm_present;
char dm_version_low;
char dm_version_high;
char dm_hsid;
char dm_devtype;

// ---------------------------------------------------------------------------
// Title:       Get Device Manager API version
// Description: Switches the Device Manager (external function) ROM in,
//              checks the "NED" API identification bytes and reads the API
//              version. Sets dm_present to 1 when the API was found.
// Syntax:      void dm_api_getversion(void);
// Input:       None
// Output:      dm_present, dm_version_low, dm_version_high
// ---------------------------------------------------------------------------
void dm_api_getversion(void)
{
    __asm
    {
        lda #0
        sta dm_present
        sta dm_version_low
        sta dm_version_high

        lda $ff00
        sta dm_saved_mmu
        lda #BNK_DM_FUNCROM
        sta $ff00

        lda DM_API_HEADER
        cmp #DM_ID_BYTE1
        bne noapi
        lda DM_API_HEADER + 1
        cmp #DM_ID_BYTE2
        bne noapi
        lda DM_API_HEADER + 2
        cmp #DM_ID_BYTE3
        bne noapi

        lda #1
        sta dm_present
        lda DM_API_VERSION
        sta dm_version_low
        lda DM_API_VERSION + 1
        sta dm_version_high
    noapi:
        lda dm_saved_mmu
        sta $ff00
    }
}

// ---------------------------------------------------------------------------
// Title:       Get hyperspeed drive ID
// Description: Asks the Device Manager ROM for the device ID of its
//              hyperspeed (Ultimate SoftIEC) drive. Only call when the API is
//              present.
// Syntax:      void dm_api_get_hsid(void);
// Input:       None
// Output:      dm_hsid
// ---------------------------------------------------------------------------
void dm_api_get_hsid(void)
{
    __asm
    {
        lda $ff00
        sta dm_saved_mmu
        lda #BNK_DM_FUNCROM
        sta $ff00
        jsr DM_EXT_GET_HS_ID
        sta dm_hsid
        lda dm_saved_mmu
        sta $ff00
    }
}

// ---------------------------------------------------------------------------
// Title:       Set hyperspeed drive ID to 8
// Description: Tells the Device Manager ROM to move its hyperspeed drive to
//              device ID 8 (used by the Force 8 option). Only call when the
//              API is present.
// Syntax:      void dm_api_set_hsid8(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void dm_api_set_hsid8(void)
{
    __asm
    {
        lda $ff00
        sta dm_saved_mmu
        lda #BNK_DM_FUNCROM
        sta $ff00
        lda #DM_HYPERSPEED_ID8
        jsr DM_EXT_SET_HS_ID
        lda dm_saved_mmu
        sta $ff00
    }
}

// ---------------------------------------------------------------------------
// Title:       Get drive type
// Description: Asks the Device Manager ROM for the type of the drive at a
//              device ID (see the DM_TYPE_* codes). Only call when the API
//              is present.
// Syntax:      char dm_api_get_drivetype(char device);
// Input:       device - IEC device ID to test
// Output:      Device Manager drive type code
// ---------------------------------------------------------------------------
char dm_api_get_drivetype(char device)
{
    dm_devtype = device;
    __asm
    {
        lda $ff00
        sta dm_saved_mmu
        lda #BNK_DM_FUNCROM
        sta $ff00
        lda dm_devtype
        jsr DM_EXT_GET_DRIVETYPE
        sta dm_devtype
        lda dm_saved_mmu
        sta $ff00
    }
    return dm_devtype;
}

// ===========================================================================
// Resident part
// ===========================================================================
#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// ---------------------------------------------------------------------------
// Title:       Query Device Manager API
// Description: Detects the Device Manager extended API and, when present,
//              reads its version and the hyperspeed drive ID into a struct.
// Syntax:      void dm_query(struct DMApiInfo *info);
// Input:       info - struct to fill
// Output:      info->present, version_major, version_minor, hyperspeed_id
//              (all 0 when the API is absent)
// ---------------------------------------------------------------------------
void dm_query(struct DMApiInfo *info)
{
    info->present = 0;
    info->version_major = 0;
    info->version_minor = 0;
    info->hyperspeed_id = 0;

    dm_api_getversion();
    if (!dm_present)
    {
        return;
    }

    info->present = 1;
    info->version_major = dm_version_high;
    info->version_minor = dm_version_low;

    dm_api_get_hsid();
    info->hyperspeed_id = dm_hsid;
}
