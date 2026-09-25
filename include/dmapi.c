/*
DMBoot 128 v5 - C128 Device Manager ROM extended API

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Based on the extapi.txt description of the C128 Device Manager ROM by
Bart van Leeuwen (2022).
Adapted: Oscar64 inline assembly, placed in the low-memory code overlay
(earlier ca65 version: DMBoot v4, branch legacy-cc65).
*/

#include <string.h>
#include "dmapi.h"
#include <petscii.h>
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
#define DM_EXT_RUN64        0x808f
#define DM_HYPERSPEED_ID8   0x08
#define KERNAL_SETBNK       0xff68
#define KERNAL_SETLFS       0xffba
#define KERNAL_SETNAM       0xffbd

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
volatile char dm_devtype;

// Program to start in C64 mode (read by dm_run64 after DMBoot has exited,
// so it must stay in the LMC area)
char dm_prgnam[DM_PRGNAME_MAX];
char dm_prglen;
char dm_devid;

// ---------------------------------------------------------------------------
// Title:       Run a program in C64 mode (SYS entry)
// Description: Entry point called from BASIC with SYS after DMBoot has
//              exited: sets file bank, device and name, switches the Device
//              Manager ROM in and jumps to its "run in 64 mode" routine,
//              which does not return.
// Syntax:      SYS <address of dm_run64> (see dm_run64_address)
// Input:       dm_prgnam, dm_prglen, dm_devid (set by dm_prepare_run64)
// Output:      Does not return
// ---------------------------------------------------------------------------
__asm dm_run64
{
        lda #0
        ldx #0
        jsr KERNAL_SETBNK
        lda #0
        ldx dm_devid
        ldy #0
        jsr KERNAL_SETLFS
        lda dm_prglen
        ldx #<dm_prgnam
        ldy #>dm_prgnam
        jsr KERNAL_SETNAM
        lda #BNK_DM_FUNCROM
        sta $ff00
        jmp DM_EXT_RUN64
}

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
    // dm_devtype is volatile: Oscar64 does not see the store in the __asm
    // block and otherwise returns the device ID passed in
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

// ---------------------------------------------------------------------------
// Title:       Device Manager drive type name
// Description: Returns a short name for the drive at a device ID, asked
//              from the Device Manager ROM. Not for the hyperspeed drive
//              (DMBoot v4 did not ask it either; see iec_scan).
// Syntax:      const char *dm_drivetype_name(char device);
// Input:       device - IEC device ID (API must be present)
// Output:      Drive type name (PETSCII), "?" for unknown codes
// ---------------------------------------------------------------------------
const char *dm_drivetype_name(char device)
{
    char type = dm_api_get_drivetype(device);
    switch (type)
    {
    case DM_TYPE_NONE:        return "none";
    case DM_TYPE_UII_A:       return "Ult A";
    case DM_TYPE_UII_B:       return "Ult B";
    case DM_TYPE_SD2IEC:      return "SD2IEC";
    case DM_TYPE_MICROIEC:    return "uIEC";
    case DM_TYPE_PRINTER:     return "printer";
    case DM_TYPE_PLOTTER:     return "plotter";
    case DM_TYPE_UII_SOFTIEC: return "SoftIEC";
    case DM_TYPE_PI1541:      return "Pi1541";
    case DM_TYPE_1540:        return "1540";
    case DM_TYPE_1541:        return "1541";
    case DM_TYPE_1570:        return "1570";
    case DM_TYPE_1571:        return "1571";
    case DM_TYPE_1581:        return "1581";
    case DM_TYPE_CMD_RL:      return "CMD RL";
    case DM_TYPE_CMD_HD:      return "CMD HD";
    case DM_TYPE_CMD_FD:      return "CMD FD";
    case DM_TYPE_CMD_RD:      return "CMD RD";
    default:                  return "?";
    }
}

// ---------------------------------------------------------------------------
// Title:       Prepare a C64 mode start
// Description: Stores the program name and device for dm_run64.
// Syntax:      bool dm_prepare_run64(const char *name, char device);
// Input:       name   - program file name (PETSCII)
//              device - device number
// Output:      false when the name is too long (nothing stored)
// ---------------------------------------------------------------------------
bool dm_prepare_run64(const char *name, char device)
{
    unsigned len = strlen(name);

    if (len >= DM_PRGNAME_MAX)
    {
        return false;
    }
    memcpy(dm_prgnam, name, len);
    dm_prgnam[len] = 0;
    dm_prglen = (char)len;
    dm_devid = device;
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Address of the C64 mode entry
// Description: Returns the address to SYS to after exiting.
// Syntax:      unsigned dm_run64_address(void);
// Input:       None
// Output:      Address of dm_run64
// ---------------------------------------------------------------------------
unsigned dm_run64_address(void)
{
    return (unsigned)dm_run64;
}

// ---------------------------------------------------------------------------
// Title:       Device Manager API version
// Description: Returns the API version as one number (high byte * 256 +
//              low byte), as DMBoot v4 compared it.
// Syntax:      unsigned dm_version(void);
// Input:       None (dminfo)
// Output:      Version number, 0 when the API is absent
// ---------------------------------------------------------------------------
unsigned dm_version(void)
{
    if (!dminfo.present)
    {
        return 0;
    }
    return ((unsigned)dminfo.version_major << 8) | dminfo.version_minor;
}
