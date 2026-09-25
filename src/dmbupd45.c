/*
DMBoot 128 - Upgrade tool v4 -> v5 (dmbupd45)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Converts the DMBoot v4 slot file (dmbootconf.prg) and utility settings
(DMBCFGFILE) in the DMBoot directory (11 on the USB stick) into the v5 files
dmbslots.cfg and dmbconf.cfg. The v4 files are left untouched as a backup.
The conversion itself is in v4convert.c (tested on the PC: tests/host).

Code and resources from others used:
-   Oscar64 by DrMortalWombat (https://github.com/drmortalwombat/oscar64)
-   Ultimate II+ command interface library, ultimateii-dos-lib by Scott
    Hutter and Francesco Sblendorio (https://github.com/xlar54/ultimateii-dos-lib)
*/

#include <stdio.h>
#include <string.h>
#include <petscii.h>
#include "defines.h"
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "dmpaths.h"
#include "petconv.h"
#include "cfgdefaults.h"
#include "basicexit.h"
#include "v4convert.h"

#define UCI_TIMEOUT_SECS    10
#define FILE_READ           0x01
#define FILE_CREATE         0x06
#define KEY_NONE            0x00

// v4 settings file name as raw ASCII (the slot file name is in dmpaths.c)
static const char v4cfgfile[] = { 0x44, 0x4d, 0x42, 0x43, 0x46, 0x47, 0x46, 0x49, 0x4c, 0x45, 0x00 };  // DMBCFGFILE

static char v4slots[V4_LOADADDR_BYTES + V4_SLOTS_BYTES];
static char v4cfg[V4CFG_SIZE];
static struct SlotStruct slot;
static struct ConfigStruct cfgout;
static char writebuf[SAVE_BUF_SIZE];
static char text[MAXPATHLEN];      // Status and path texts for printing

// ---------------------------------------------------------------------------
// Title:       Wait for a key
// Description: Waits for a key with KERNAL GETIN.
// Syntax:      static char key_wait(void);
// Input:       None
// Output:      PETSCII key code
// ---------------------------------------------------------------------------
static char key_wait(void)
{
    char key;

    do
    {
        key = __asm
        {
            jsr $ffe4
            sta accu
        };
    } while (key == KEY_NONE);
    return key;
}

// ---------------------------------------------------------------------------
// Title:       Ask yes or no
// Description: Prints a question and waits for Y or N.
// Syntax:      static bool ask_yesno(const char *question);
// Input:       question - text (PETSCII)
// Output:      true for yes
// ---------------------------------------------------------------------------
static bool ask_yesno(const char *question)
{
    printf("%s (Y/N)\n", question);
    while (true)
    {
        char key = key_wait();
        if (key == 'y' || key == 'Y')
        {
            return true;
        }
        if (key == 'n' || key == 'N')
        {
            return false;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Stop with an error
// Description: Prints an error and the Ultimate status, waits for a key and
//              returns to BASIC.
// Syntax:      static void fail(const char *message);
// Input:       message - text (PETSCII)
// Output:      Does not return
// ---------------------------------------------------------------------------
static void fail(const char *message)
{
    asc2pet(text, uii_status, sizeof(text));
    printf("\n%s\nStatus: %s\nPress a key.\n", message, text);
    key_wait();
    dmb_exit();
}

// ---------------------------------------------------------------------------
// Title:       Does a file exist
// Description: Tries to open a file in the DMBoot directory for reading.
// Syntax:      static bool file_exists(const char *name);
// Input:       name - ASCII file name
// Output:      true when it could be opened
// ---------------------------------------------------------------------------
static bool file_exists(const char *name)
{
    bool found;

    uii_change_dir(configpath);
    uii_open_file(FILE_READ, (char *)name);
    found = UII_SUCCESS;
    uii_close_file();
    return found;
}

// ---------------------------------------------------------------------------
// Title:       Read a whole file
// Description: Reads a file from the DMBoot directory into a buffer, never
//              beyond its size.
// Syntax:      static unsigned read_file(const char *name, char *buffer,
//                                        unsigned size);
// Input:       name   - ASCII file name
//              buffer - destination
//              size   - size of buffer
// Output:      Number of bytes read (0 when the file does not exist)
// ---------------------------------------------------------------------------
static unsigned read_file(const char *name, char *buffer, unsigned size)
{
    unsigned filled = 0;

    uii_change_dir(configpath);
    uii_open_file(FILE_READ, (char *)name);
    if (!UII_SUCCESS)
    {
        uii_close_file();
        return 0;
    }
    uii_read_file(size);
    while (uii_isdataavailable() || uii_ismoredataavailable())
    {
        unsigned bytes = uii_readdata();
        uii_accept();
        if (bytes > size - filled)
        {
            bytes = size - filled;
        }
        memcpy(buffer + filled, uii_data, bytes);
        filled += bytes;
    }
    uii_close_file();
    return filled;
}

// ---------------------------------------------------------------------------
// Title:       Write data to the open file
// Description: Writes a block in chunks of SAVE_BUF_SIZE bytes.
// Syntax:      static void write_block(const char *data, unsigned length);
// Input:       data   - bytes to write
//              length - number of bytes
// Output:      None (stops on an error)
// ---------------------------------------------------------------------------
static void write_block(const char *data, unsigned length)
{
    while (length)
    {
        unsigned chunk = (length < SAVE_BUF_SIZE) ? length : SAVE_BUF_SIZE;
        memcpy(writebuf, data, chunk);
        uii_write_file(writebuf, chunk);
        if (!UII_SUCCESS)
        {
            fail("Writing failed.");
        }
        data += chunk;
        length -= chunk;
    }
}

// ---------------------------------------------------------------------------
// Title:       Create a file for writing
// Description: Deletes an old file of that name (overwriting does not work)
//              and creates it in the DMBoot directory.
// Syntax:      static void create_file(const char *name);
// Input:       name - ASCII file name
// Output:      None (stops on an error)
// ---------------------------------------------------------------------------
static void create_file(const char *name)
{
    uii_change_dir(configpath);
    uii_delete_file((char *)name);
    uii_open_file(FILE_CREATE, (char *)name);
    if (!UII_SUCCESS)
    {
        fail("Creating a file failed.");
    }
}

// ---------------------------------------------------------------------------
// Title:       Upgrade tool
// Description: Finds the DMBoot directory, reads the v4 files, asks before
//              overwriting existing v5 files, writes dmbslots.cfg and
//              dmbconf.cfg.
// Syntax:      int main(void);
// Input:       None
// Output:      0 (returns to BASIC through dmb_exit)
// ---------------------------------------------------------------------------
int main(void)
{
    unsigned bytes;
    bool havecfg;
    char used = 0;

    dmb_zp_save();
    printf("%c", 14);
    printf("DMBoot 128 upgrade v4 to v5\n\n");

    if (!uii_wait_for_uci(UCI_TIMEOUT_SECS))
    {
        printf("No Ultimate Command Interface.\n");
        dmb_exit();
    }
    if (resolve_storage_path() == STORAGE_NONE)
    {
        fail("DMBoot directory (11) not found.");
    }
    asc2pet(text, configpath, sizeof(text));
    printf("Directory: %s\n", text);

    bytes = read_file(v4slotfilename, v4slots, sizeof(v4slots));
    if (bytes != sizeof(v4slots))
    {
        fail("No complete v4 slot file dmbootconf.");
    }
    havecfg = read_file(v4cfgfile, v4cfg, sizeof(v4cfg)) == sizeof(v4cfg);
    printf("v4 slots read. v4 settings %s.\n", havecfg ? "read" : "not found, using defaults");

    if (file_exists(slotfilename) || file_exists(configfilename))
    {
        if (!ask_yesno("\nv5 files exist. Overwrite them?"))
        {
            printf("Nothing changed.\n");
            dmb_exit();
        }
    }

    printf("\nConverting slots:\n");
    create_file(slotfilename);
    for (char x = 0; x < SLOTS; x++)
    {
        if (v4_convert_slot(v4slots + V4_LOADADDR_BYTES + (unsigned)x * V4_SLOT_STRIDE, &slot))
        {
            used++;
            printf("%2u %s\n", x, slot.menu);
        }
        write_block((const char *)&slot, sizeof(slot));
    }
    uii_close_file();
    printf("%u slots converted.\n\n", used);

    if (v4_convert_config(v4cfg, havecfg, configpath, &cfgout))
    {
        printf("GEOS image without path: using the\nDMBoot directory. Check it in F4, F8.\n");
    }
    create_file(configfilename);
    write_block((const char *)&cfgout, sizeof(cfgout));
    uii_close_file();
    printf("Settings converted.\n\n");

    printf("Done. The v4 files are kept as backup.\nPress a key.\n");
    key_wait();
    dmb_exit();
    return 0;
}
