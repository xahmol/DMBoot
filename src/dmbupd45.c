/*
DMBoot 128 - Upgrade tool v4 -> v5 (dmbupd45)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Converts the DMBoot v4 slot file (dmbootconf.prg) and utility settings
(DMBCFGFILE) in the DMBoot directory (11 on the USB stick) into the v5 files
dmbslots.cfg and dmbconf.cfg. The v4 files are left untouched as a backup.
Rules: docs/REBUILD_PLAN.md §10; reference implementation and test data:
tests/tools/convert_v4_slots.py, tests/data/.

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

#define UCI_TIMEOUT_SECS    10
#define FILE_READ           0x01
#define FILE_CREATE         0x06
#define KEY_NONE            0x00

// v4 slot file: load address, then 36 slots of 512 bytes (two 256-byte
// pages: path..cfgvs, then the image fields; see v4 getslotfromem)
#define V4_LOADADDR_BYTES   2
#define V4_SLOT_STRIDE      512
#define V4_PAGE             256
#define V4_SLOTS_BYTES      (SLOTS * V4_SLOT_STRIDE)
#define V4_PATH             0
#define V4_PATH_LEN         100
#define V4_MENU             100
#define V4_MENU_LEN         21
#define V4_FILE             121
#define V4_FILE_LEN         20
#define V4_CMD              141
#define V4_CMD_LEN          80
#define V4_REUIMAGE         221
#define V4_REUIMAGE_LEN     20
#define V4_REUSIZE          241
#define V4_RUNBOOT          242
#define V4_DEVICE           243
#define V4_COMMAND          244
#define V4_IMGA_PATH        (V4_PAGE + 0)
#define V4_IMGA_FILE        (V4_PAGE + 100)
#define V4_IMGA_ID          (V4_PAGE + 120)
#define V4_IMGB_PATH        (V4_PAGE + 121)
#define V4_IMGB_FILE        (V4_PAGE + 221)
#define V4_IMGB_ID          (V4_PAGE + 241)
#define V4_IMGPATH_LEN      100
#define V4_IMGFILE_LEN      20

// v4 utility settings (DMBCFGFILE, v4 configcommon.c)
#define V4CFG_SIZE          328
#define V4CFG_REUPATH       0
#define V4CFG_REUPATH_LEN   60
#define V4CFG_REUIMAGE      60
#define V4CFG_NAME_LEN      20
#define V4CFG_IMGA_PATH     80
#define V4CFG_IMGA_FILE     140
#define V4CFG_IMGB_PATH     160
#define V4CFG_IMGB_FILE     220
#define V4CFG_REUSIZE       240
#define V4CFG_TIMEON        241
#define V4CFG_UTCOFFSET     242     // 4 bytes, big endian
#define V4CFG_IMGA_ID       246
#define V4CFG_IMGB_ID       247
#define V4CFG_HOST          248
#define V4CFG_HOST_LEN      80

// v4 settings file name as raw ASCII (the slot file name is in dmpaths.c)
static const char v4cfgfile[] = { 0x44, 0x4d, 0x42, 0x43, 0x46, 0x47, 0x46, 0x49, 0x4c, 0x45, 0x00 };  // DMBCFGFILE

static char v4slots[V4_LOADADDR_BYTES + V4_SLOTS_BYTES];
static char v4cfg[V4CFG_SIZE];
static struct SlotStruct slot;
static struct ConfigStruct cfgout;
static char writebuf[SAVE_BUF_SIZE];
static char text[MAXPATHLEN];

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
// Title:       Copy a v4 string field
// Description: Copies a 0-terminated field of fixed length (also when the
//              v4 field has no terminator) into a v5 field.
// Syntax:      static void copy_field(char *dst, unsigned dstsize,
//                                     const char *src, unsigned srclen);
// Input:       dst, dstsize - destination and its size
//              src, srclen  - v4 field and its length
// Output:      dst
// ---------------------------------------------------------------------------
static void copy_field(char *dst, unsigned dstsize, const char *src, unsigned srclen)
{
    unsigned i = 0;

    while (i < srclen && i < dstsize - 1 && src[i])
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

// ---------------------------------------------------------------------------
// Title:       Convert a v4 path to an Ultimate path
// Description: Removes a leading "cd:" (only when present: some v4 paths
//              lack it, which v4's "+3" then broke) and converts PETSCII to
//              ASCII.
// Syntax:      static void ult_path(char *dst, unsigned dstsize,
//                                   const char *src, unsigned srclen);
// Input:       dst, dstsize - destination and its size
//              src, srclen  - v4 field and its length
// Output:      dst (ASCII)
// ---------------------------------------------------------------------------
static void ult_path(char *dst, unsigned dstsize, const char *src, unsigned srclen)
{
    char field[V4_PATH_LEN + 1];
    const char *start = field;

    copy_field(field, sizeof(field), src, srclen);
    if ((field[0] & 0x7f) == 'c' && (field[1] & 0x7f) == 'd' && field[2] == ':')
    {
        start += 3;
    }
    pet2asc(dst, start, dstsize);
}

// ---------------------------------------------------------------------------
// Title:       Convert one v4 slot
// Description: Converts a 512-byte v4 slot into the v5 slot structure.
// Syntax:      static bool convert_slot(const char *v4);
// Input:       v4 - start of the v4 slot
// Output:      true when the slot is in use; result in slot
// ---------------------------------------------------------------------------
static bool convert_slot(const char *v4)
{
    char command = v4[V4_COMMAND];

    memset(&slot, 0, sizeof(slot));
    slot.cfgvs = CFGVERSION;
    if (!v4[V4_MENU])
    {
        return false;
    }

    copy_field(slot.menu, sizeof(slot.menu), v4 + V4_MENU, V4_MENU_LEN);
    copy_field(slot.path, sizeof(slot.path), v4 + V4_PATH, V4_PATH_LEN);
    copy_field(slot.file, sizeof(slot.file), v4 + V4_FILE, V4_FILE_LEN);
    copy_field(slot.cmd, sizeof(slot.cmd), v4 + V4_CMD, V4_CMD_LEN);
    slot.reusize = v4[V4_REUSIZE];
    slot.runboot = v4[V4_RUNBOOT];
    slot.device = v4[V4_DEVICE];

    // Mount flags without an image file name were seen in real v4 files
    if ((command & COMMAND_IMGA) && !v4[V4_IMGA_FILE])
    {
        command &= ~COMMAND_IMGA;
    }
    if ((command & COMMAND_IMGB) && !v4[V4_IMGB_FILE])
    {
        command &= ~COMMAND_IMGB;
    }
    slot.command = command;

    if (command & COMMAND_IMGA)
    {
        slot.image_a_id = v4[V4_IMGA_ID];
        ult_path(slot.image_a_path, sizeof(slot.image_a_path), v4 + V4_IMGA_PATH, V4_IMGPATH_LEN);
        copy_field(text, sizeof(text), v4 + V4_IMGA_FILE, V4_IMGFILE_LEN);
        pet2asc(slot.image_a_file, text, sizeof(slot.image_a_file));
    }
    if (command & COMMAND_IMGB)
    {
        slot.image_b_id = v4[V4_IMGB_ID];
        ult_path(slot.image_b_path, sizeof(slot.image_b_path), v4 + V4_IMGB_PATH, V4_IMGPATH_LEN);
        copy_field(text, sizeof(text), v4 + V4_IMGB_FILE, V4_IMGFILE_LEN);
        pet2asc(slot.image_b_file, text, sizeof(slot.image_b_file));
    }
    if (command & COMMAND_REU)
    {
        // v4 used image_a_path+3 as the REU directory; when that is empty
        // the program path is the best guess
        copy_field(text, sizeof(text), v4 + V4_REUIMAGE, V4_REUIMAGE_LEN);
        pet2asc(slot.reu_image, text, sizeof(slot.reu_image));
        if (v4[V4_IMGA_PATH])
        {
            ult_path(slot.reu_path, sizeof(slot.reu_path), v4 + V4_IMGA_PATH, V4_IMGPATH_LEN);
        }
        else
        {
            ult_path(slot.reu_path, sizeof(slot.reu_path), v4 + V4_PATH, V4_PATH_LEN);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Convert a GEOS image setting
// Description: Converts a v4 GEOS image path and name. An empty path means
//              the DMBoot directory (v4 then used the current directory).
// Syntax:      static void convert_geos_image(char *path, char *file,
//                                             unsigned pathoffset,
//                                             unsigned fileoffset);
// Input:       path, file             - v5 fields (MAXPATHLEN, MAXFILENAME)
//              pathoffset, fileoffset - positions in the v4 settings
// Output:      path, file (ASCII)
// ---------------------------------------------------------------------------
static void convert_geos_image(char *path, char *file, unsigned pathoffset, unsigned fileoffset)
{
    copy_field(text, sizeof(text), v4cfg + fileoffset, V4CFG_NAME_LEN);
    pet2asc(file, text, MAXFILENAME);
    if (v4cfg[pathoffset])
    {
        ult_path(path, MAXPATHLEN, v4cfg + pathoffset, V4CFG_REUPATH_LEN);
    }
    else if (file[0])
    {
        strncpy(path, configpath, MAXPATHLEN - 1);
        path[MAXPATHLEN - 1] = 0;
        printf("GEOS image without path: using the\nDMBoot directory. Check it in F4, F8.\n");
    }
}

// ---------------------------------------------------------------------------
// Title:       Build the v5 configuration
// Description: Defaults, plus the NTP and GEOS settings of v4 when the v4
//              settings file was read.
// Syntax:      static void convert_config(bool havev4);
// Input:       havev4 - v4cfg holds DMBCFGFILE
// Output:      cfgout
// ---------------------------------------------------------------------------
static void convert_config(bool havev4)
{
    config_set_defaults(&cfgout);
    if (!havev4)
    {
        return;
    }

    cfgout.timeon = v4cfg[V4CFG_TIMEON] ? 1 : 0;
    cfgout.secondsfromutc = ((long)(unsigned char)v4cfg[V4CFG_UTCOFFSET] << 24) |
                            ((long)(unsigned char)v4cfg[V4CFG_UTCOFFSET + 1] << 16) |
                            ((long)(unsigned char)v4cfg[V4CFG_UTCOFFSET + 2] << 8) |
                            (long)(unsigned char)v4cfg[V4CFG_UTCOFFSET + 3];
    if (v4cfg[V4CFG_HOST])
    {
        copy_field(text, sizeof(text), v4cfg + V4CFG_HOST, V4CFG_HOST_LEN);
        pet2asc(cfgout.host, text, sizeof(cfgout.host));
    }

    cfgout.geos.reusize = v4cfg[V4CFG_REUSIZE];
    convert_geos_image(cfgout.geos.reu_path, cfgout.geos.reu_image, V4CFG_REUPATH, V4CFG_REUIMAGE);
    cfgout.geos.image_a_id = v4cfg[V4CFG_IMGA_ID];
    convert_geos_image(cfgout.geos.image_a_path, cfgout.geos.image_a_file, V4CFG_IMGA_PATH, V4CFG_IMGA_FILE);
    cfgout.geos.image_b_id = v4cfg[V4CFG_IMGB_ID];
    convert_geos_image(cfgout.geos.image_b_path, cfgout.geos.image_b_file, V4CFG_IMGB_PATH, V4CFG_IMGB_FILE);
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
        if (convert_slot(v4slots + V4_LOADADDR_BYTES + (unsigned)x * V4_SLOT_STRIDE))
        {
            used++;
            printf("%2u %s\n", x, slot.menu);
        }
        write_block((const char *)&slot, sizeof(slot));
    }
    uii_close_file();
    printf("%u slots converted.\n\n", used);

    convert_config(havecfg);
    create_file(configfilename);
    write_block((const char *)&cfgout, sizeof(cfgout));
    uii_close_file();
    printf("Settings converted.\n\n");

    printf("Done. The v4 files are kept as backup.\nPress a key.\n");
    key_wait();
    dmb_exit();
    return 0;
}
