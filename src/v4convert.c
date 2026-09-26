/*
DMBoot 128 - DMBoot v4 to v5 data conversion

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Converts v4 slots (dmbootconf) and utility settings (DMBCFGFILE) into the
v5 structures. Used by the upgrade tool dmbupd45 and by the host tests in
tests/host (hardware independent: no charmap-dependent literals).
Rules: docs/REBUILD_PLAN.md §10; reference: tests/tools/convert_v4_slots.py.
*/

#include <string.h>
#include "petconv.h"
#include "cfgdefaults.h"
#include "v4convert.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// v4 slot layout (page 1: path..cfgvs, page 2 from offset 256: images)
#define V4_PAGE             256
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

// v4 utility settings layout (v4 configcommon.c)
#define V4CFG_REUPATH       0
#define V4CFG_PATH_LEN      60
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

// PETSCII "cd:" (unshifted letters; the shift bit is masked off)
#define PET_C               0x43
#define PET_D               0x44
#define PET_COLON           0x3a
#define PET_SHIFT_MASK      0x7f

static char text[MAXPATHLEN];

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
    if ((field[0] & PET_SHIFT_MASK) == PET_C && (field[1] & PET_SHIFT_MASK) == PET_D && field[2] == PET_COLON)
    {
        start += 3;
    }
    pet2asc(dst, start, dstsize);
}

// ---------------------------------------------------------------------------
// Title:       Convert a v4 file name
// Description: Copies a v4 name field and converts it to ASCII.
// Syntax:      static void ult_name(char *dst, unsigned dstsize,
//                                   const char *src, unsigned srclen);
// Input:       dst, dstsize - destination and its size
//              src, srclen  - v4 field and its length
// Output:      dst (ASCII)
// ---------------------------------------------------------------------------
static void ult_name(char *dst, unsigned dstsize, const char *src, unsigned srclen)
{
    copy_field(text, sizeof(text), src, srclen);
    pet2asc(dst, text, dstsize);
}

// ---------------------------------------------------------------------------
// Title:       Convert one v4 slot
// Description: Converts a 512-byte v4 slot into a v5 slot. Menu name, path,
//              file and command stay PETSCII; mount and REU paths become
//              Ultimate (ASCII) paths without "cd:". Mount flags without an
//              image name are cleared (seen in real v4 files). v4 used
//              image_a_path as the REU directory; when that is empty, the
//              program path is used.
// Syntax:      bool v4_convert_slot(const char *v4, struct SlotStruct *slot);
// Input:       v4   - start of the v4 slot
//              slot - v5 slot to fill
// Output:      true when the slot is in use (always filled; empty when not)
// ---------------------------------------------------------------------------
bool v4_convert_slot(const char *v4, struct SlotStruct *slot)
{
    char command = v4[V4_COMMAND];

    memset(slot, 0, sizeof(*slot));
    slot->cfgvs = CFGVERSION;
    if (!v4[V4_MENU])
    {
        return false;
    }

    copy_field(slot->menu, sizeof(slot->menu), v4 + V4_MENU, V4_MENU_LEN);
    copy_field(slot->path, sizeof(slot->path), v4 + V4_PATH, V4_PATH_LEN);
    copy_field(slot->file, sizeof(slot->file), v4 + V4_FILE, V4_FILE_LEN);
    copy_field(slot->cmd, sizeof(slot->cmd), v4 + V4_CMD, V4_CMD_LEN);
    slot->reusize = v4[V4_REUSIZE];
    slot->runboot = v4[V4_RUNBOOT];
    slot->device = v4[V4_DEVICE];

    if ((command & COMMAND_IMGA) && !v4[V4_IMGA_FILE])
    {
        command &= ~COMMAND_IMGA;
    }
    if ((command & COMMAND_IMGB) && !v4[V4_IMGB_FILE])
    {
        command &= ~COMMAND_IMGB;
    }
    slot->command = command;

    if (command & COMMAND_IMGA)
    {
        slot->image_a_id = v4[V4_IMGA_ID];
        ult_path(slot->image_a_path, sizeof(slot->image_a_path), v4 + V4_IMGA_PATH, V4_IMGPATH_LEN);
        ult_name(slot->image_a_file, sizeof(slot->image_a_file), v4 + V4_IMGA_FILE, V4_IMGFILE_LEN);
    }
    if (command & COMMAND_IMGB)
    {
        slot->image_b_id = v4[V4_IMGB_ID];
        ult_path(slot->image_b_path, sizeof(slot->image_b_path), v4 + V4_IMGB_PATH, V4_IMGPATH_LEN);
        ult_name(slot->image_b_file, sizeof(slot->image_b_file), v4 + V4_IMGB_FILE, V4_IMGFILE_LEN);
    }
    if (command & COMMAND_REU)
    {
        ult_name(slot->reu_image, sizeof(slot->reu_image), v4 + V4_REUIMAGE, V4_REUIMAGE_LEN);
        if (v4[V4_IMGA_PATH])
        {
            ult_path(slot->reu_path, sizeof(slot->reu_path), v4 + V4_IMGA_PATH, V4_IMGPATH_LEN);
        }
        else
        {
            ult_path(slot->reu_path, sizeof(slot->reu_path), v4 + V4_PATH, V4_PATH_LEN);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Convert a GEOS image setting
// Description: Converts a v4 GEOS image path and name. An empty path with a
//              name means the DMBoot directory (v4 then used the current
//              directory).
// Syntax:      static bool geos_image(const char *v4cfg, char *path,
//                                     char *file, unsigned pathoffset,
//                                     unsigned fileoffset,
//                                     const char *dmbootdir);
// Input:       v4cfg                  - v4 settings
//              path, file             - v5 fields (MAXPATHLEN, MAXFILENAME)
//              pathoffset, fileoffset - positions in the v4 settings
//              dmbootdir              - DMBoot directory (ASCII)
// Output:      true when the DMBoot directory was filled in
// ---------------------------------------------------------------------------
static bool geos_image(const char *v4cfg, char *path, char *file, unsigned pathoffset, unsigned fileoffset,
                       const char *dmbootdir)
{
    ult_name(file, MAXFILENAME, v4cfg + fileoffset, V4CFG_NAME_LEN);
    if (v4cfg[pathoffset])
    {
        ult_path(path, MAXPATHLEN, v4cfg + pathoffset, V4CFG_PATH_LEN);
        return false;
    }
    if (!file[0])
    {
        path[0] = 0;
        return false;
    }
    strncpy(path, dmbootdir, MAXPATHLEN - 1);
    path[MAXPATHLEN - 1] = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Convert the v4 settings
// Description: Fills a v5 configuration with the defaults, plus the NTP and
//              GEOS RAM boot settings of v4 when its settings file was read.
//              A v4 NTP server that is not one of the v5 defaults replaces
//              the first server.
// Syntax:      bool v4_convert_config(const char *v4cfg, bool havev4,
//                                     const char *dmbootdir,
//                                     struct ConfigStruct *config);
// Input:       v4cfg     - DMBCFGFILE contents (V4CFG_SIZE bytes)
//              havev4    - false: only the defaults
//              dmbootdir - DMBoot directory (ASCII), for GEOS images
//                          without a path
//              config    - configuration to fill
// Output:      true when a GEOS image path was filled in with dmbootdir
// ---------------------------------------------------------------------------
bool v4_convert_config(const char *v4cfg, bool havev4, const char *dmbootdir, struct ConfigStruct *config)
{
    bool defaulted = false;

    config_set_defaults(config);
    if (!havev4)
    {
        return false;
    }

    config->timeon = v4cfg[V4CFG_TIMEON] ? 1 : 0;
    config->secondsfromutc = ((unsigned long)(unsigned char)v4cfg[V4CFG_UTCOFFSET] << 24) |
                             ((unsigned long)(unsigned char)v4cfg[V4CFG_UTCOFFSET + 1] << 16) |
                             ((unsigned long)(unsigned char)v4cfg[V4CFG_UTCOFFSET + 2] << 8) |
                             (unsigned long)(unsigned char)v4cfg[V4CFG_UTCOFFSET + 3];
    // Keep a server the user chose; v4's default (pool.ntp.org) is already
    // one of the three v5 servers
    if (v4cfg[V4CFG_HOST])
    {
        ult_name(text, sizeof(text), v4cfg + V4CFG_HOST, V4CFG_HOST_LEN);
        if (strcmp(text, config->host) && strcmp(text, config->host2) && strcmp(text, config->host3))
        {
            strncpy(config->host, text, sizeof(config->host) - 1);
            config->host[sizeof(config->host) - 1] = 0;
        }
    }

    config->geos.reusize = v4cfg[V4CFG_REUSIZE];
    defaulted |= geos_image(v4cfg, config->geos.reu_path, config->geos.reu_image, V4CFG_REUPATH, V4CFG_REUIMAGE,
                            dmbootdir);
    config->geos.image_a_id = v4cfg[V4CFG_IMGA_ID];
    defaulted |= geos_image(v4cfg, config->geos.image_a_path, config->geos.image_a_file, V4CFG_IMGA_PATH,
                            V4CFG_IMGA_FILE, dmbootdir);
    config->geos.image_b_id = v4cfg[V4CFG_IMGB_ID];
    defaulted |= geos_image(v4cfg, config->geos.image_b_path, config->geos.image_b_file, V4CFG_IMGB_PATH,
                            V4CFG_IMGB_FILE, dmbootdir);
    return defaulted;
}
