/*
DMBoot 128 v5 - C128 banking and overlay layer

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <stdio.h>
#include <string.h>
#include <petscii.h>
#include <c64/kernalio.h>
#include <c128/mmu.h>
#include <c128/vdc.h>
#include "banking.h"

// Low-memory code overlay: loaded once at $1300, stays resident in the
// 8 KB common RAM so it is visible whichever bank is mapped in.
#pragma overlay(dmblmc, 1)
#pragma section(bcode1, 0)
#pragma section(bdata1, 0)
#pragma section(bbss1, 0)
#pragma region(bank1, LMC_START, LMC_END, , 1, { bcode1, bdata1, bbss1 })

// ===========================================================================
// Resident part (main program region)
// ===========================================================================
#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// Maximum length of a composed load name: partition prefix + file name
#define LOADNAME_MAX (sizeof(DM_PARTITION_PREFIX) - 1 + OVERLAY_NAME_MAX)

// ---------------------------------------------------------------------------
// Title:       Get current device
// Description: Returns the device number of the last used I/O device, which
//              right after start-up is the device DMBoot was loaded from.
//              Falls back to 8 when the KERNAL has not set one yet.
// Syntax:      char getcurrentdevice(void);
// Input:       None
// Output:      Device number (8 when unknown)
// ---------------------------------------------------------------------------
char getcurrentdevice(void)
{
    char device = *(volatile char *)ZP_CURRENT_DEVICE;

    if (!device)
    {
        device = 8;
    }
    return device;
}

// ---------------------------------------------------------------------------
// Title:       Load overlay file
// Description: Loads an overlay (or LMC) file from the Device Manager boot
//              partition into bank 0 at the load address stored in the file,
//              using the KERNAL LOAD routine. Counts disk loads in sysinfo.
// Syntax:      bool load_overlay(const char *fname);
// Input:       fname - overlay file name without partition prefix,
//                      at most OVERLAY_NAME_MAX - 1 characters
// Output:      true on success, false when the name is too long or the load
//              failed (KERNAL status is printed)
// ---------------------------------------------------------------------------
bool load_overlay(const char *fname)
{
    char loadname[LOADNAME_MAX];

    // Reject names that would not fit, instead of silently truncating them
    if (strlen(fname) >= OVERLAY_NAME_MAX)
    {
        printf("Overlay name too long: %s\n", fname);
        return false;
    }

    strncpy(loadname, DM_PARTITION_PREFIX, sizeof(loadname) - 1);
    loadname[sizeof(loadname) - 1] = 0;
    strncat(loadname, fname, sizeof(loadname) - 1 - strlen(loadname));

    krnio_setbnk(0, 0);
    krnio_setnam(loadname);
    if (!krnio_load(1, sysinfo.bootdevice, 1))
    {
        printf("Loading %s failed, status %u\n", loadname, krnio_pstatus[1]);
        return false;
    }

    sysinfo.diskloads++;
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Initialise banking
// Description: Records the boot device, switches the MMU to 8 KB common RAM
//              at the bottom of memory and loads the low-memory code (LMC)
//              overlay to $1300.
// Syntax:      bool bnk_init(void);
// Input:       None
// Output:      true on success, false when the LMC file could not be loaded
// ---------------------------------------------------------------------------
bool bnk_init(void)
{
    sysinfo.bootdevice = getcurrentdevice();

    // 8 KB common RAM at $0000-$1FFF, VIC in bank 0
    xmmu.rcr = RCR_COMMON_8K_BOTTOM;

    return load_overlay("dmblmc");
}

// ---------------------------------------------------------------------------
// Title:       Exit banking
// Description: Restores the C128 default 1 KB common RAM configuration.
//              Must be called before any exit to BASIC, C64 mode or GEOS.
// Syntax:      void bnk_exit(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void bnk_exit(void)
{
    xmmu.rcr = RCR_COMMON_DEFAULT;
}

// ===========================================================================
// Low-memory code (LMC) part, $1300-$1AFF
// ===========================================================================
#pragma code(bcode1)
#pragma data(bdata1)
#pragma bss(bbss1)

// ---------------------------------------------------------------------------
// Title:       Banked read byte
// Description: Reads one byte from an address in the memory configuration
//              given by an MMU $FF00 value, then restores the configuration.
// Syntax:      char bnk_readb(char cr, volatile char *p);
// Input:       cr - MMU $FF00 configuration value (for example BNK_1_FULL)
//              p  - address to read
// Output:      The byte read
// ---------------------------------------------------------------------------
char bnk_readb(char cr, volatile char *p)
{
    char old = mmu.cr;
    mmu.cr = cr;
    char c = *p;
    mmu.cr = old;
    return c;
}

// ---------------------------------------------------------------------------
// Title:       Banked write byte
// Description: Writes one byte to an address in the memory configuration
//              given by an MMU $FF00 value, then restores the configuration.
// Syntax:      void bnk_writeb(char cr, volatile char *p, char b);
// Input:       cr - MMU $FF00 configuration value
//              p  - address to write
//              b  - byte to write
// Output:      None
// ---------------------------------------------------------------------------
void bnk_writeb(char cr, volatile char *p, char b)
{
    char old = mmu.cr;
    mmu.cr = cr;
    *p = b;
    mmu.cr = old;
}

// ---------------------------------------------------------------------------
// Title:       Banked memory copy
// Description: Copies a block of memory between two memory configurations,
//              byte by byte, switching the MMU for every read and write.
// Syntax:      void bnk_memcpy(char dcr, volatile char *dp, char scr,
//                              volatile char *sp, unsigned size);
// Input:       dcr  - MMU $FF00 value for the destination
//              dp   - destination address
//              scr  - MMU $FF00 value for the source
//              sp   - source address
//              size - number of bytes to copy
// Output:      None
// ---------------------------------------------------------------------------
void bnk_memcpy(char dcr, volatile char *dp, char scr, volatile char *sp, unsigned size)
{
    char old = mmu.cr;
    while (size > 0)
    {
        mmu.cr = scr;
        char c = *sp++;
        mmu.cr = dcr;
        *dp++ = c;
        size--;
    }
    mmu.cr = old;
}

// ---------------------------------------------------------------------------
// Title:       Banked memory fill
// Description: Fills a block of memory in the given memory configuration
//              with one value.
// Syntax:      void bnk_memset(char cr, volatile char *p, char val,
//                              unsigned size);
// Input:       cr   - MMU $FF00 configuration value
//              p    - start address
//              val  - fill value
//              size - number of bytes to fill
// Output:      None
// ---------------------------------------------------------------------------
void bnk_memset(char cr, volatile char *p, char val, unsigned size)
{
    char old = mmu.cr;
    mmu.cr = cr;
    while (size > 0)
    {
        *p++ = val;
        size--;
    }
    mmu.cr = old;
}

// Bytes per character in the ROM charset and in the VDC charset memory
#define CHARSET_ROM_BYTES   8
#define CHARSET_VDC_BYTES   16

// ---------------------------------------------------------------------------
// Title:       Banked copy to VDC memory
// Description: Copies a block from any bank to VDC RAM, switching the MMU
//              to the source configuration for every byte read.
// Syntax:      void bnk_cpytovdc(unsigned vdcdest, char scr,
//                                volatile char *sp, unsigned size);
// Input:       vdcdest - destination address in VDC RAM
//              scr     - MMU $FF00 value for the source
//              sp      - source address
//              size    - number of bytes to copy
// Output:      None
// ---------------------------------------------------------------------------
void bnk_cpytovdc(unsigned vdcdest, char scr, volatile char *sp, unsigned size)
{
    char old = mmu.cr;
    mmu.cr = BNK_DEFAULT;
    vdc_mem_addr(vdcdest);

    while (size > 0)
    {
        mmu.cr = scr;
        char c = *sp++;
        mmu.cr = BNK_DEFAULT;
        vdc_write(c);
        size--;
    }
    mmu.cr = old;
}

// ---------------------------------------------------------------------------
// Title:       Banked copy from VDC memory
// Description: Copies a block from VDC RAM to any bank, switching the MMU
//              to the destination configuration for every byte written.
// Syntax:      void bnk_cpyfromvdc(char dcr, volatile char *dp,
//                                  unsigned vdcsrc, unsigned size);
// Input:       dcr    - MMU $FF00 value for the destination
//              dp     - destination address
//              vdcsrc - source address in VDC RAM
//              size   - number of bytes to copy
// Output:      None
// ---------------------------------------------------------------------------
void bnk_cpyfromvdc(char dcr, volatile char *dp, unsigned vdcsrc, unsigned size)
{
    char old = mmu.cr;
    while (size > 0)
    {
        mmu.cr = BNK_DEFAULT;
        char c = vdc_mem_read_at(vdcsrc++);
        mmu.cr = dcr;
        *dp++ = c;
        mmu.cr = BNK_DEFAULT;
        size--;
    }
    mmu.cr = old;
}

// ---------------------------------------------------------------------------
// Title:       Banked charset copy to VDC
// Description: Copies character definitions of 8 bytes each (ROM or RAM
//              charset format) into VDC charset memory, which uses 16 bytes
//              per character; the 8 extra bytes are filled with zero.
// Syntax:      void bnk_redef_charset(unsigned vdcdest, char scr,
//                                     volatile char *sp, unsigned size);
// Input:       vdcdest - VDC charset address to start at
//              scr     - MMU $FF00 value for the source (e.g. BNK_CHARROM)
//              sp      - source address of the 8-byte definitions
//              size    - number of characters to copy
// Output:      None
// ---------------------------------------------------------------------------
void bnk_redef_charset(unsigned vdcdest, char scr, volatile char *sp, unsigned size)
{
    char old = mmu.cr;
    mmu.cr = BNK_DEFAULT;
    vdc_mem_addr(vdcdest);

    while (size > 0)
    {
        for (char i = 0; i < CHARSET_ROM_BYTES; i++)
        {
            mmu.cr = scr;
            char c = *sp++;
            mmu.cr = BNK_DEFAULT;
            vdc_write(c);
        }
        for (char i = CHARSET_ROM_BYTES; i < CHARSET_VDC_BYTES; i++)
        {
            vdc_write(0);
        }
        size--;
    }
    mmu.cr = old;
}

// Back to the resident sections for any file included after this one
#pragma code(code)
#pragma data(data)
#pragma bss(bss)
