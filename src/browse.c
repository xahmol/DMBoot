/*
DMBoot 128 v5 - File browser overlay

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

IEC-only file browser. Directory reading, the REU-backed linked list and
the slot picker are ported from src/filebrowse.c and src/slotmenu.c
(pickmenuslot) of my UBoot64-v2 project
(https://github.com/xahmol/UBoot64-v2); the key set, dirtrace, Force 8,
FAST, run in C64 mode and BOOT follow DMBoot v4 (src/db.c on branch
legacy-cc65). Adapted: no UCI browse mode and no partition list (see
docs/REBUILD_PLAN.md §9, §11), 80-column two-column listing, one index-
based navigation routine, 16-bit block counts, bounded dirtrace string.

Code and resources from others used:
-   DraBrowse (db*) by Sascha Bader, adapted by Dirk Jagdmann (doj)
    https://github.com/doj/dracopy
    Directory entry parsing (via UBoot64-v2 and DMBoot v4).
*/

#include <stdio.h>
#include <string.h>
#include <petscii.h>
#include <c64/kernalio.h>
#include "defines.h"
#include "dualwin.h"
#include "dmapi.h"
#include "reu128.h"
#include "testmode.h"
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "core.h"
#include "fileio.h"
#include "slotlist.h"
#include "dirparse.h"
#include "browse.h"

#pragma overlay(dmbovl3, 4)
#pragma section(codeovl3, 0)
#pragma section(dataovl3, 0)
#pragma section(bssovl3, 0)
#pragma region(ovl3, OVERLAYLOAD, OVERLAY_SLOT_END, , 4, { codeovl3, dataovl3, bssovl3 })

#pragma code(codeovl3)
#pragma data(dataovl3)
#pragma bss(bssovl3)

// Screen layout
#define DIR_LFN             2       // Logical file number of the directory
#define DIR_PROGRESS_ROW    2
#define DIR_HEADER_ROW      3
#define DIR_TRACE_ROW       4
#define DIR_ROW0            5
#define DIR_ROWS            19
#define DIR_FOOTER_ROW      24
#define DIR_COLW_40         25      // "bbbb nnnnnnnnnnnnnnnn ttt"
#define DIR_COLW_80         26
#define PANEL_X_40          25
#define PANEL_X_80          53
#define PANEL_ROW0          3
#define DIR_HEADER_MAX      (16 + 1 + DISK_ID_LEN + 1)
#define BLOCKS_SHOWN_MAX    9999
#define DEVICE_NONE         0

// What a slot is made from (browse_pick)
#define PICK_PROGRAM        1       // Run a file from the traced directory
#define PICK_BOOT           2       // BOOT the traced directory
#define PICK_MOUNTRUN       3       // Mount the traced image on drive A, run a file from it
#define PICK_MOUNT_A        4       // Add a disk image mount on drive A
#define PICK_MOUNT_B        5       // Add a disk image mount on drive B
#define PICK_REU            6       // Add an REU image

// Keys
#define KEY_UPARROW         0x5e

// The directory being shown
struct Directory
{
    unsigned long first;            // REU address of the first entry, 0 = empty
    unsigned long firstprint;       // First entry on the shown page
    unsigned long present;          // Selected entry
    unsigned long address;          // Where the next entry is stored
    unsigned long limit;            // End of the usable REU area
    unsigned count;                 // Number of entries
    unsigned index;                 // Index of the selected entry
    unsigned pagefirst;             // Index of the first entry on the page
    unsigned free;                  // Blocks free
    char header[DIR_HEADER_MAX];    // Disk name and ID
};

// Browser state
struct BrowseState
{
    char device;                    // IEC device being browsed
    bool softiec;                   // Device is the DM hyperspeed / Ultimate SoftIEC drive
    const char *devtype;            // Drive type name of the device (read once per device)
    bool trace;                     // Dirtrace on: selections go to a slot
    bool sorted;
    bool force8;
    bool fast;
    bool comma1;
    bool demo;
    bool inimage;                   // Traced into a disk image on the SoftIEC drive
    char imagedepth;                // Length of tracepath before entering the image
    char tracepath[MAXPATHLEN];     // Traced directories (PETSCII), each followed by '/'
    char imagepath[MAXPATHLEN];     // Ultimate path (ASCII) of the traced image's directory
    char imagefile[MAXFILENAME];    // Traced image file name (ASCII)
};

static struct DirElement entry;     // Selected (or last loaded) entry
static struct DirElement sortbuf;   // Buffer for sorted inserts
static struct Directory dir;
static struct BrowseState bs;
static char active[IEC_ID_COUNT];
static char line[DIR_LINE_MAX];
static char diskid[DISK_ID_LEN + 1];
static char pathbuf[MAXPATHLEN];

static const char *const reg_types[] = { "seq", "prg", "usr", "rel", "vrp" };
static const char *const oth_types[] = { "del", "cbm", "dir", "lnk", "???", "hdr" };
static const char cmd_up_bytes[] = { 0x5f, 0 };  // CBM DOS "go up" (left arrow)

// ===========================================================================
// Directory storage (REU). Only these two functions know where entries
// live; the rest uses entry addresses.
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Load a directory entry
// Description: Loads the entry at an address into a buffer (metadata, then
//              the name with its stored length, bounded by the buffer).
// Syntax:      static void dir_load(unsigned long address,
//                                   struct DirElement *element);
// Input:       address - REU address of the entry
//              element - buffer
// Output:      element
// ---------------------------------------------------------------------------
static void dir_load(unsigned long address, struct DirElement *element)
{
    reu128_load(address, (volatile char *)&element->meta, sizeof(element->meta));
    if (element->meta.length > sizeof(element->name))
    {
        element->meta.length = sizeof(element->name);
    }
    reu128_load(address + sizeof(element->meta), (volatile char *)element->name, element->meta.length);
    element->name[sizeof(element->name) - 1] = 0;
}

// ---------------------------------------------------------------------------
// Title:       Store directory entry metadata
// Description: Writes the metadata of an entry (the name is written once,
//              when the entry is added).
// Syntax:      static void dir_store_meta(unsigned long address,
//                                         const struct DirMeta *meta);
// Input:       address - REU address of the entry
//              meta    - metadata
// Output:      None
// ---------------------------------------------------------------------------
static void dir_store_meta(unsigned long address, const struct DirMeta *meta)
{
    reu128_store(address, (const volatile char *)meta, sizeof(*meta));
}

// ---------------------------------------------------------------------------
// Title:       Step through the list
// Description: Follows next (or prev) links from an entry, loading only the
//              metadata, and stops at the end of the list.
// Syntax:      static unsigned long dir_walk(unsigned long address,
//                                            unsigned steps, bool forward,
//                                            unsigned *done);
// Input:       address - start entry
//              steps   - number of steps
//              forward - true: next links, false: prev links
//              done    - receives the number of steps taken
// Output:      Address of the entry reached
// ---------------------------------------------------------------------------
static unsigned long dir_walk(unsigned long address, unsigned steps, bool forward, unsigned *done)
{
    struct DirMeta meta;

    *done = 0;
    while (*done < steps)
    {
        reu128_load(address, (volatile char *)&meta, sizeof(meta));
        unsigned long target = forward ? meta.next : meta.prev;
        if (!target)
        {
            break;
        }
        address = target;
        (*done)++;
    }
    return address;
}

// ===========================================================================
// Reading a directory
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Open a directory
// Description: Opens the "$" file of a device and skips the load address.
// Syntax:      static bool dir_open(char device);
// Input:       device - IEC device ID
// Output:      true when open (input switched to it)
// ---------------------------------------------------------------------------
static bool dir_open(char device)
{
    krnio_setnam("$");
    if (!krnio_open(DIR_LFN, device, 0) || krnio_status())
    {
        krnio_close(DIR_LFN);
        return false;
    }
    if (!krnio_chkin(DIR_LFN) || krnio_status())
    {
        krnio_clrchn();
        krnio_close(DIR_LFN);
        return false;
    }
    krnio_chrin();
    krnio_chrin();
    if (krnio_status())
    {
        krnio_clrchn();
        krnio_close(DIR_LFN);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Close the directory
// Description: Restores the default I/O channels and closes the directory.
// Syntax:      static void dir_close(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
static void dir_close(void)
{
    krnio_clrchn();
    krnio_close(DIR_LFN);
}

// ---------------------------------------------------------------------------
// Title:       Read one directory entry
// Description: Reads the next BASIC line of the directory: block count
//              (16 bits; UBoot64 stored it in a char) and text, parsed by
//              dir_parse_line (name, type, disk header, blocks free).
// Syntax:      static char dir_readentry(struct DirElement *element);
// Input:       element - buffer
// Output:      DIRPARSE_OK, 1 = end of directory, DIRPARSE_SKIP
// ---------------------------------------------------------------------------
static char dir_readentry(struct DirElement *element)
{
    char b;
    char i = 0;

    // Link bytes: zero means end of directory
    if (!krnio_chrin() || krnio_status() & KRNIO_EOF)
    {
        return 1;
    }
    krnio_chrin();

    element->meta.size = krnio_chrin();
    element->meta.size |= (unsigned)krnio_chrin() << 8;

    memset(line, 0, sizeof(line));
    while (true)
    {
        b = krnio_chrin();
        if (!b || krnio_status())
        {
            break;
        }
        if (i < sizeof(line) - 1)
        {
            line[i++] = b;
        }
    }

    return dir_parse_line(line, i, element, diskid);
}

// ---------------------------------------------------------------------------
// Title:       Link a new entry into the list
// Description: Links the entry in `entry` (to be stored at `address`) at
//              the end of the list, or in name order when sorting.
// Syntax:      static void dir_link(unsigned long address,
//                                   unsigned long *last);
// Input:       address - REU address the new entry will get
//              last    - address of the last entry (updated)
// Output:      entry.meta.next/prev set; neighbours updated in the REU
// ---------------------------------------------------------------------------
static void dir_link(unsigned long address, unsigned long *last)
{
    entry.meta.next = 0;
    entry.meta.prev = 0;
    if (!dir.first)
    {
        dir.first = address;
        *last = address;
        return;
    }

    if (bs.sorted)
    {
        unsigned long element = dir.first;
        while (element)
        {
            dir_load(element, &sortbuf);
            if (strcmp(sortbuf.name, entry.name) > 0)
            {
                // Insert before element
                entry.meta.prev = sortbuf.meta.prev;
                entry.meta.next = element;
                sortbuf.meta.prev = address;
                dir_store_meta(element, &sortbuf.meta);
                if (entry.meta.prev)
                {
                    dir_load(entry.meta.prev, &sortbuf);
                    sortbuf.meta.next = address;
                    dir_store_meta(entry.meta.prev, &sortbuf.meta);
                }
                else
                {
                    dir.first = address;
                }
                return;
            }
            element = sortbuf.meta.next;
        }
    }

    // Append at the end
    entry.meta.prev = *last;
    reu128_store(*last, (const volatile char *)&address, sizeof(address));
    *last = address;
}

// ---------------------------------------------------------------------------
// Title:       Read the directory into the REU
// Description: Reads the directory of the browsed device into a linked list
//              in the REU (from DIR_REU_START up to the top of the REU) and
//              selects the first entry.
// Syntax:      static bool dir_read(void);
// Input:       bs.device, bs.sorted
// Output:      true when the directory could be opened
// ---------------------------------------------------------------------------
static bool dir_read(void)
{
    unsigned long last = 0;

    memset(&dir, 0, sizeof(dir));
    dir.address = DIR_REU_START;
    dir.limit = (unsigned long)sysinfo.reupages * REU_PAGE_BYTES;
    diskid[0] = 0;

    dwin_putat_string(&screenwin, 0, DIR_PROGRESS_ROW, "Reading directory", cfg.colors.text);
    if (!dir_open(bs.device))
    {
        dwin_fill_rect(&screenwin, 0, DIR_PROGRESS_ROW, screenwin.wx, 1, ' ', cfg.colors.text);
        return false;
    }

    while (true)
    {
        char ret = dir_readentry(&entry);
        if (ret == 1)
        {
            break;
        }
        if (ret)
        {
            continue;
        }

        if (entry.meta.type == CBM_T_HEADER)
        {
            strncpy(dir.header, entry.name, 16);
            dir.header[16] = 0;
            strcat(dir.header, ",");
            strncat(dir.header, diskid, sizeof(dir.header) - 1 - strlen(dir.header));
            continue;
        }
        if (entry.meta.type == CBM_T_FREE)
        {
            dir.free = entry.meta.size;
            break;
        }
        if (dir.address + sizeof(entry.meta) + entry.meta.length > dir.limit)
        {
            break;
        }

        dir_link(dir.address, &last);
        dir_store_meta(dir.address, &entry.meta);
        reu128_store(dir.address + sizeof(entry.meta), (const volatile char *)entry.name, entry.meta.length);
        dir.address += sizeof(entry.meta) + entry.meta.length;
        dir.count++;
    }
    dir_close();
    dwin_fill_rect(&screenwin, 0, DIR_PROGRESS_ROW, screenwin.wx, 1, ' ', cfg.colors.text);

    dir.present = dir.first;
    dir.firstprint = dir.first;
    if (dir.first)
    {
        dir_load(dir.present, &entry);
    }
    return true;
}

// ===========================================================================
// Showing the directory
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Entries per page
// Description: 19 in 40 columns, 38 (two columns) in 80 columns.
// Syntax:      static char dir_pagesize(void);
// Input:       None
// Output:      Entries per page
// ---------------------------------------------------------------------------
static char dir_pagesize(void)
{
    return dwin_is80() ? 2 * DIR_ROWS : DIR_ROWS;
}

// ---------------------------------------------------------------------------
// Title:       File type text
// Description: Three-letter text of a CBM file type.
// Syntax:      static const char *filetype_text(char type);
// Input:       type - CBM_T_*
// Output:      Text (PETSCII)
// ---------------------------------------------------------------------------
static const char *filetype_text(char type)
{
    if (type & CBM_T_REG)
    {
        type &= ~CBM_T_REG;
        return (type < sizeof(reg_types) / sizeof(reg_types[0])) ? reg_types[type] : "???";
    }
    return (type < sizeof(oth_types) / sizeof(oth_types[0])) ? oth_types[type] : "???";
}

// ---------------------------------------------------------------------------
// Title:       Print one entry
// Description: Prints `entry` at a position on the page (column and row
//              from the position), reversed when it is the selected one.
// Syntax:      static void dir_print_entry(char pos, bool selected);
// Input:       pos      - position on the page (0 .. page size - 1)
//              selected - highlight
// Output:      None
// ---------------------------------------------------------------------------
static void dir_print_entry(char pos, bool selected)
{
    char colw = dwin_is80() ? DIR_COLW_80 : DIR_COLW_40;
    char x = (pos / DIR_ROWS) * colw;
    char y = DIR_ROW0 + pos % DIR_ROWS;
    char name[17];
    unsigned size = (entry.meta.size > BLOCKS_SHOWN_MAX) ? BLOCKS_SHOWN_MAX : entry.meta.size;

    strncpy(name, entry.name, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    // At most 4 + 1 + 16 + 1 + 3 = 25 characters: fits line
    sprintf(line, "%4u %-16s %s", size, name, filetype_text(entry.meta.type));
    if (selected)
    {
        dwin_putat_string_reverse(&screenwin, x, y, line, cfg.colors.diritem_select);
    }
    else
    {
        dwin_putat_string(&screenwin, x, y, line, cfg.colors.diritem_normal);
    }
}

// ---------------------------------------------------------------------------
// Title:       Build the dirtrace path
// Description: The DOS command that changes to the traced directory:
//              "cd:/" + trace on the SoftIEC drive, "cd//" + trace on other
//              drives (as DMBoot v4 pathconcat).
// Syntax:      static const char *browse_pathconcat(void);
// Input:       bs.tracepath, bs.softiec
// Output:      Pointer to pathbuf (PETSCII)
// ---------------------------------------------------------------------------
static const char *browse_pathconcat(void)
{
    trace_command(pathbuf, sizeof(pathbuf), bs.tracepath, bs.softiec);
    return pathbuf;
}

// ---------------------------------------------------------------------------
// Title:       Device type text
// Description: Name of the drive type of the browsed device.
// Syntax:      static const char *browse_devtype(void);
// Input:       bs.device
// Output:      Text (PETSCII)
// ---------------------------------------------------------------------------
static const char *browse_devtype(void)
{
    if (dminfo.present && bs.device == dminfo.hyperspeed_id)
    {
        return "Hyperspeed";
    }
    return dminfo.present ? dm_drivetype_name(bs.device) : "IEC";
}

// ---------------------------------------------------------------------------
// Title:       Draw the directory page
// Description: Header (device, disk name, dirtrace), the entries of the
//              shown page and the footer (drive type, blocks free).
// Syntax:      static void dir_draw(void);
// Input:       dir, bs
// Output:      None
// ---------------------------------------------------------------------------
static void dir_draw(void)
{
    char listw = dwin_is80() ? PANEL_X_80 : PANEL_X_40;
    unsigned long element = dir.firstprint;
    unsigned long selected = dir.present;

    dwin_fill_rect(&screenwin, 0, DIR_HEADER_ROW, listw, DIR_FOOTER_ROW - DIR_HEADER_ROW + 1, ' ', cfg.colors.text);

    // dir.header is at most 23 characters: fits line; clip to the list
    sprintf(line, "[%u] %s", bs.device, dir.header);
    line[listw - 1] = 0;
    dwin_putat_string(&screenwin, 0, DIR_HEADER_ROW, line, cfg.colors.text);
    if (bs.trace)
    {
        const char *path = browse_pathconcat();
        char len = strlen(path);
        dwin_putat_string(&screenwin, 0, DIR_TRACE_ROW, (len > listw - 1) ? path + len - (listw - 1) : path,
                          cfg.colors.text);
    }
    else
    {
        dwin_putat_string(&screenwin, 0, DIR_TRACE_ROW, "No dirtrace active.", cfg.colors.text);
    }
    // Drive type names are at most 10 characters: fits line; clip to the list
    sprintf(line, "(%s) %u blocks free", bs.devtype, dir.free);
    line[listw - 1] = 0;
    dwin_putat_string(&screenwin, 0, DIR_FOOTER_ROW, line, cfg.colors.text);

    if (!element)
    {
        dwin_putat_string(&screenwin, 0, DIR_ROW0, "Empty directory.", cfg.colors.error);
        return;
    }
    for (char pos = 0; pos < dir_pagesize() && element; pos++)
    {
        dir_load(element, &entry);
        dir_print_entry(pos, element == selected);
        element = entry.meta.next;
    }
    dir_load(selected, &entry);
}

// Option toggles in the key panel, below the key list
#define TOGGLE_TRACE        0
#define TOGGLE_SORT         1
#define TOGGLE_FORCE8       2
#define TOGGLE_FAST         3
#define TOGGLE_COMMA1       4
#define TOGGLE_DEMO         5
#define TOGGLES             6
#define TOGGLE_ROW0         (PANEL_ROW0 + 13)

// ---------------------------------------------------------------------------
// Title:       Draw one option toggle
// Description: Redraws the line of one toggle in the key panel with its
//              on/off state (only that line changes when it is switched).
// Syntax:      static void browse_toggle(char toggle);
// Input:       toggle - TOGGLE_*
// Output:      None
// ---------------------------------------------------------------------------
static void browse_toggle(char toggle)
{
    static const char *const labels[TOGGLES] = {
        "  D Trace  ", "  S Sort   ", "  8 Force8 ", "  F FAST   ", "  1 ,1     ", "  O Demo   "
    };
    bool states[TOGGLES];
    char x = dwin_is80() ? PANEL_X_80 : PANEL_X_40;
    char len;

    states[TOGGLE_TRACE] = bs.trace;
    states[TOGGLE_SORT] = bs.sorted;
    states[TOGGLE_FORCE8] = bs.force8;
    states[TOGGLE_FAST] = bs.fast;
    states[TOGGLE_COMMA1] = bs.comma1;
    states[TOGGLE_DEMO] = bs.demo;
    len = dwin_putat_string(&screenwin, x, TOGGLE_ROW0 + toggle, labels[toggle], cfg.colors.text);
    dwin_putat_string(&screenwin, x + len, TOGGLE_ROW0 + toggle, states[toggle] ? "on " : "off", cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Draw the key panel
// Description: Full draw of the key reference and the option toggles.
// Syntax:      static void browse_panel(void);
// Input:       bs
// Output:      None
// ---------------------------------------------------------------------------
static void browse_panel(void)
{
    char x = dwin_is80() ? PANEL_X_80 : PANEL_X_40;
    char y = PANEL_ROW0;
    static const char *const keys[] = {
        " F1 Refresh", " +- Device", "RET Run/select", "DEL Dir up", "  \x5e Root",
        "T/E Top/end", "P/U Page", " F5 Boot dir", "  6 Run in 64", "A/B Add mount",
        "  M Run mount", " F7 Quit"
    };

    dwin_fill_rect(&screenwin, x, PANEL_ROW0, screenwin.wx - x, DIR_FOOTER_ROW - PANEL_ROW0, ' ', cfg.colors.text);
    for (char i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        dwin_putat_string(&screenwin, x, y++, keys[i], cfg.colors.text);
    }
    for (char t = 0; t < TOGGLES; t++)
    {
        browse_toggle(t);
    }
}

// ---------------------------------------------------------------------------
// Title:       Select an entry by index
// Description: Moves the selection to an entry index (clamped to the list).
//              Within the shown page only the two changed lines are
//              redrawn; otherwise the page containing it is drawn.
// Syntax:      static void dir_goto(int target);
// Input:       target - wanted entry index (may be out of range)
// Output:      None
// ---------------------------------------------------------------------------
static void dir_goto(int target)
{
    unsigned done;
    unsigned newindex;
    char pagesize = dir_pagesize();

    if (!dir.first)
    {
        return;
    }
    if (target < 0)
    {
        target = 0;
    }
    newindex = ((unsigned)target >= dir.count) ? dir.count - 1 : (unsigned)target;
    if (newindex == dir.index)
    {
        return;
    }

    unsigned long address = (newindex > dir.index)
        ? dir_walk(dir.present, newindex - dir.index, true, &done)
        : dir_walk(dir.present, dir.index - newindex, false, &done);
    unsigned newpagefirst = newindex - newindex % pagesize;

    if (newpagefirst == dir.pagefirst)
    {
        dir_load(dir.present, &entry);
        dir_print_entry(dir.index - dir.pagefirst, false);
        dir.present = address;
        dir.index = newindex;
        dir_load(dir.present, &entry);
        dir_print_entry(dir.index - dir.pagefirst, true);
    }
    else
    {
        dir.present = address;
        dir.index = newindex;
        dir.pagefirst = newpagefirst;
        dir.firstprint = dir_walk(address, newindex - newpagefirst, false, &done);
        dir_draw();
    }
}

// ===========================================================================
// Directory changes
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Ultimate path of the trace
// Description: The traced directory as an Ultimate file system path:
//              "/" + trace (PETSCII to ASCII).
// Syntax:      static void browse_ultpath(char *dst, unsigned size);
// Input:       dst  - destination
//              size - size of dst
// Output:      dst
// ---------------------------------------------------------------------------
static void browse_ultpath(char *dst, unsigned size)
{
    trace_ultpath(dst, size, bs.tracepath);
}

// ---------------------------------------------------------------------------
// Title:       Go up in the dirtrace
// Description: Drops the last traced directory and leaves the traced image
//              when going above it.
// Syntax:      static void browse_trace_up(void);
// Input:       None
// Output:      bs.tracepath, bs.inimage
// ---------------------------------------------------------------------------
static void browse_trace_up(void)
{
    if (trace_up(bs.tracepath) <= bs.imagedepth)
    {
        bs.inimage = false;
    }
}

// ---------------------------------------------------------------------------
// Title:       Change directory
// Description: Sends the DOS command for root (NULL), up ("..") or a named
//              directory, keeps the dirtrace in step and reads the new
//              directory. "Up" on the SoftIEC drive retries with the
//              firmware 3.15 form "cd_" (as UBoot64-v2).
// Syntax:      static bool browse_cd(const char *name);
// Input:       name - NULL = root, ".." = up, else a directory name
// Output:      true on success (directory read and drawn)
// ---------------------------------------------------------------------------
static bool browse_cd(const char *name)
{
    char status;
    bool up = name && strcmp(name, "..") == 0;

    // Names come from the listing: at most MAXFILENAME - 1 characters, so
    // "cd/" + name + "/" fits line
    if (!name)
    {
        strcpy(line, "cd//");
    }
    else if (up)
    {
        strcpy(line, "cd:");
        strcat(line, bs.softiec ? ".." : cmd_up_bytes);
    }
    else if (bs.softiec || dir_imagekind(name) == IMAGE_DISK)
    {
        strcpy(line, "cd:");
        strncat(line, name, MAXFILENAME - 1);
    }
    else
    {
        strcpy(line, "cd/");
        strncat(line, name, MAXFILENAME - 1);
        strcat(line, "/");
    }

    // Keep trace and drive in step: refuse a directory the trace cannot hold
    if (bs.trace && name && !up && !trace_fits(bs.tracepath, sizeof(bs.tracepath), name))
    {
        return false;
    }

    status = cmd(bs.device, line);
    if (status && up && bs.softiec)
    {
        status = cmd(bs.device, "cd_");
    }
    if (status)
    {
        return false;
    }

    if (bs.trace)
    {
        if (!name)
        {
            bs.tracepath[0] = 0;
            bs.inimage = false;
        }
        else if (up)
        {
            browse_trace_up();
        }
        else
        {
            if (bs.softiec && dir_imagekind(name) == IMAGE_DISK && !bs.inimage)
            {
                bs.inimage = true;
                bs.imagedepth = strlen(bs.tracepath);
                browse_ultpath(bs.imagepath, sizeof(bs.imagepath));
                pet2asc(bs.imagefile, name, sizeof(bs.imagefile));
            }
            trace_add(bs.tracepath, sizeof(bs.tracepath), name);
        }
    }
    dir_read();
    dir_draw();
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Switch to a device
// Description: Makes a device the browsed one and reads its directory.
//              The dirtrace is switched off.
// Syntax:      static void browse_device(char device);
// Input:       device - IEC device ID
// Output:      None
// ---------------------------------------------------------------------------
static void browse_device(char device)
{
    bs.device = device;
    bs.softiec = dminfo.present && (device == dminfo.hyperspeed_id ||
                                    dm_api_get_drivetype(device) == DM_TYPE_UII_SOFTIEC);
    bs.devtype = browse_devtype();
    bs.trace = false;
    bs.inimage = false;
    bs.tracepath[0] = 0;
    dir_read();
    dir_draw();
    browse_toggle(TOGGLE_TRACE);
}

// ---------------------------------------------------------------------------
// Title:       Next active device
// Description: Steps to the next (or previous) active IEC device 8-29.
// Syntax:      static void browse_nextdevice(bool forward);
// Input:       forward - true for the next ID, false for the previous
// Output:      None
// ---------------------------------------------------------------------------
static void browse_nextdevice(bool forward)
{
    char device = bs.device;

    for (char tries = 0; tries < IEC_ID_COUNT; tries++)
    {
        if (forward)
        {
            device = (device >= IEC_ID_FIRST + IEC_ID_COUNT - 2) ? IEC_ID_FIRST : device + 1;
        }
        else
        {
            device = (device <= IEC_ID_FIRST) ? IEC_ID_FIRST + IEC_ID_COUNT - 2 : device - 1;
        }
        if (active[device - IEC_ID_FIRST])
        {
            browse_device(device);
            return;
        }
    }
}

// ===========================================================================
// Storing a selection in a slot
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Ask yes or no
// Description: Prints a question in the prompt row and waits for Y or N.
// Syntax:      static bool pick_yesno(const char *question);
// Input:       question - text before " (Y/N)"
// Output:      true for yes
// ---------------------------------------------------------------------------
static bool pick_yesno(const char *question)
{
    char len = dwin_putat_string(&screenwin, 0, SLOTLIST_PROMPT_ROW, question, cfg.colors.text);
    dwin_putat_string(&screenwin, len, SLOTLIST_PROMPT_ROW, " (Y/N)", cfg.colors.text);
    while (true)
    {
        char key = key_wait();
        if (key == 'y' || key == 'Y')
        {
            return true;
        }
        if (key == 'n' || key == 'N' || key == KEY_STOP)
        {
            return false;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Choose the REU size
// Description: Lets the user step through the REU sizes with + and -.
// Syntax:      static void pick_reusize(void);
// Input:       Slot.reusize (start value)
// Output:      Slot.reusize
// ---------------------------------------------------------------------------
static void pick_reusize(void)
{
    if (Slot.reusize >= REU_SIZES)
    {
        Slot.reusize = REU_SIZES - 1;
    }
    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "REU size: + and -, RETURN to accept.", cfg.colors.text);
    while (true)
    {
        dwin_fill_rect(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, SLOTLIST_COLUMN, 1, ' ', cfg.colors.text);
        dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, reusizenames[Slot.reusize], cfg.colors.text_input);
        char key = key_wait();
        if (key == '+')
        {
            Slot.reusize = (Slot.reusize + 1) % REU_SIZES;
        }
        else if (key == '-')
        {
            Slot.reusize = (Slot.reusize + REU_SIZES - 1) % REU_SIZES;
        }
        else if (key == KEY_RETURN)
        {
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Store the selection in a slot
// Description: Shows the slots, asks which one to use and its name, and
//              fills it from the selection, then saves the slots.
//              A drive A mount added to a slot that starts a program asks
//              first, because it replaces the disk that program needs;
//              drive B and REU images are added to what the slot does.
//              As UBoot64-v2 pickmenuslot.
// Syntax:      static bool browse_pick(char kind, const char *name,
//                                      char runboot);
// Input:       kind    - PICK_*
//              name    - selected file or image name (PETSCII)
//              runboot - EXEC_* flags for program, boot and mount starts
// Output:      true when a slot was stored
// ---------------------------------------------------------------------------
static bool browse_pick(char kind, const char *name, char runboot)
{
    char page = 0;
    char slot;
    char text[MAXMENUNAME];

    dwin_clear(&screenwin);
    headertext("Choose a slot", 1);
    slotlist_draw(page);
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "Store in which slot? (F7 = cancel)", cfg.colors.text);
    slot = slotlist_pick(&page, true);
    if (slot == NO_SLOT)
    {
        return false;
    }

    get_slot_from_reu(slot);
    slotlist_draw_slot(slot, page, SLOTLIST_SELECTED);
    slotlist_clear_bottom();
    if (Slot.menu[0])
    {
        if (!pick_yesno("Slot is not empty. Change it?"))
        {
            return false;
        }
    }
    else
    {
        memset(&Slot, 0, sizeof(Slot));
        Slot.cfgvs = CFGVERSION;
        strncpy(Slot.menu, name, sizeof(Slot.menu) - 1);
        Slot.menu[sizeof(Slot.menu) - 1] = 0;
    }

    if (kind == PICK_MOUNT_A && Slot.file[0])
    {
        slotlist_clear_bottom();
        dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "The slot starts a program from a disk.",
                          cfg.colors.text);
        if (!pick_yesno("Replace it with this mount?"))
        {
            return false;
        }
        Slot.file[0] = 0;
        Slot.path[0] = 0;
        Slot.runboot = 0;
        Slot.device = 0;
    }

    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "Name of the slot (STOP = cancel):", cfg.colors.text);
    strncpy(text, Slot.menu, sizeof(text) - 1);
    text[sizeof(text) - 1] = 0;
    if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, text, sizeof(text), MAXMENUNAME - 1,
                   cfg.colors.text_input) <= 0)
    {
        return false;
    }
    strncpy(Slot.menu, text, sizeof(Slot.menu) - 1);
    Slot.menu[sizeof(Slot.menu) - 1] = 0;

    switch (kind)
    {
    case PICK_PROGRAM:
    case PICK_BOOT:
        Slot.device = bs.device;
        strncpy(Slot.path, browse_pathconcat(), sizeof(Slot.path) - 1);
        Slot.path[sizeof(Slot.path) - 1] = 0;
        strncpy(Slot.file, (kind == PICK_BOOT) ? "" : name, sizeof(Slot.file) - 1);
        Slot.file[sizeof(Slot.file) - 1] = 0;
        Slot.runboot = runboot;
        break;

    case PICK_MOUNTRUN:
    case PICK_MOUNT_A:
        uii_parse_deviceinfo();
        Slot.image_a_id = uii_devinfo[0].id;
        strncpy(Slot.image_a_path, bs.imagepath, sizeof(Slot.image_a_path) - 1);
        Slot.image_a_path[sizeof(Slot.image_a_path) - 1] = 0;
        if (kind == PICK_MOUNTRUN)
        {
            strncpy(Slot.image_a_file, bs.imagefile, sizeof(Slot.image_a_file) - 1);
            strncpy(Slot.file, name, sizeof(Slot.file) - 1);
            Slot.file[sizeof(Slot.file) - 1] = 0;
            Slot.path[0] = 0;
            Slot.device = Slot.image_a_id;
            Slot.runboot = runboot | EXEC_MOUNT;
        }
        else
        {
            browse_ultpath(Slot.image_a_path, sizeof(Slot.image_a_path));
            pet2asc(Slot.image_a_file, name, sizeof(Slot.image_a_file));
        }
        Slot.image_a_file[sizeof(Slot.image_a_file) - 1] = 0;
        Slot.command |= COMMAND_IMGA;
        break;

    case PICK_MOUNT_B:
        uii_parse_deviceinfo();
        Slot.image_b_id = uii_devinfo[1].id;
        browse_ultpath(Slot.image_b_path, sizeof(Slot.image_b_path));
        pet2asc(Slot.image_b_file, name, sizeof(Slot.image_b_file));
        Slot.command |= COMMAND_IMGB;
        break;

    case PICK_REU:
        pick_reusize();
        browse_ultpath(Slot.reu_path, sizeof(Slot.reu_path));
        pet2asc(Slot.reu_image, name, sizeof(Slot.reu_image));
        Slot.command |= COMMAND_REU;
        break;
    }

    save_slot_to_reu(slot);
    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "Saving the slots, please wait.", cfg.colors.text);
    write_slotsfile();
    return true;
}

// ===========================================================================
// Main loop
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Run flags
// Description: EXEC_* flags from the browser options.
// Syntax:      static char browse_runflags(void);
// Input:       bs
// Output:      EXEC_* flags
// ---------------------------------------------------------------------------
static char browse_runflags(void)
{
    return (bs.force8 ? EXEC_FRC8 : 0) | (bs.fast ? EXEC_FAST : 0) |
           (bs.comma1 ? EXEC_COMMA1 : 0) | (bs.demo ? EXEC_DEMO : 0);
}

// ---------------------------------------------------------------------------
// Title:       Message
// Description: Shows a message in the progress row until the next key.
// Syntax:      static void browse_message(const char *text);
// Input:       text - message (PETSCII)
// Output:      None
// ---------------------------------------------------------------------------
static void browse_message(const char *text)
{
    dwin_putat_string(&screenwin, 0, DIR_PROGRESS_ROW, text, cfg.colors.error);
    key_wait();
    dwin_fill_rect(&screenwin, 0, DIR_PROGRESS_ROW, screenwin.wx, 1, ' ', cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Redraw the whole browser screen
// Description: Header, key panel and the directory page.
// Syntax:      static void browse_screen(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
static void browse_screen(void)
{
    dwin_clear(&screenwin);
    headertext("Filebrowser", 1);
    browse_panel();
    dir_draw();
}

// ---------------------------------------------------------------------------
// Title:       Start or select a program
// Description: With dirtrace off the program is started directly (via
//              browsereq and the slot start overlay); with dirtrace on it
//              goes to a slot.
// Syntax:      static bool browse_start(char kind, const char *name,
//                                       char runboot);
// Input:       kind    - PICK_PROGRAM or PICK_BOOT
//              name    - program name (PETSCII), ignored for BOOT
//              runboot - EXEC_* flags
// Output:      true when the browser should close
// ---------------------------------------------------------------------------
static bool browse_start(char kind, const char *name, char runboot)
{
    if (bs.trace)
    {
        bool stored = browse_pick(kind, name, runboot);
        if (!stored)
        {
            browse_screen();
        }
        return stored;
    }
    browsereq.action = BROWSE_RUN;
    browsereq.device = bs.device;
    browsereq.runboot = runboot;
    strncpy(browsereq.file, (kind == PICK_BOOT) ? "" : name, sizeof(browsereq.file) - 1);
    browsereq.file[sizeof(browsereq.file) - 1] = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Title:       File browser
// Description: Browses the IEC devices, starting on the Device Manager
//              hyperspeed drive (or the first active device). RETURN starts
//              a program or enters a directory; with dirtrace (D) on, the
//              choice is stored in a slot. A/B/M and REU images need the
//              dirtrace on the SoftIEC (hyperspeed) drive, because slots
//              store them as Ultimate file system paths.
// Syntax:      void browse(void);
// Input:       None
// Output:      browsereq: BROWSE_RUN with a program to start, or
//              BROWSE_QUIT
// ---------------------------------------------------------------------------
void browse(void)
{
    memset(&bs, 0, sizeof(bs));
    browsereq.action = BROWSE_QUIT;

    // Active devices; start on the hyperspeed drive as DMBoot v4 did
    uii_parse_deviceinfo();
    iec_scan(active);
    bs.device = DEVICE_NONE;
    if (dminfo.present && dminfo.hyperspeed_id >= IEC_ID_FIRST)
    {
        bs.device = dminfo.hyperspeed_id;
    }
    for (char x = 0; x < IEC_ID_COUNT - 1 && !bs.device; x++)
    {
        if (active[x])
        {
            bs.device = iec_index_to_id(x);
        }
    }

    dwin_clear(&screenwin);
    headertext("Filebrowser", 1);
    if (!bs.device)
    {
        dwin_putat_string(&screenwin, 0, DIR_HEADER_ROW, "No active IEC drives. Press a key.", cfg.colors.error);
        key_wait();
        return;
    }
    browse_panel();
    browse_device(bs.device);

    while (true)
    {
        char key = key_wait();
        bool is80 = dwin_is80();
        char imagekind;

        if (dir.present)
        {
            dir_load(dir.present, &entry);
        }
        imagekind = dir.present ? dir_imagekind(entry.name) : IMAGE_NONE;

        switch (key)
        {
        case KEY_F1:
            dir_read();
            dir_draw();
            break;
        case 's':
            bs.sorted = !bs.sorted;
            dir_read();
            dir_draw();
            browse_toggle(TOGGLE_SORT);
            break;
        case '+':
            browse_nextdevice(true);
            break;
        case '-':
            browse_nextdevice(false);
            break;

        case KEY_CURSOR_DOWN:
            dir_goto(dir.index + 1);
            break;
        case KEY_CURSOR_UP:
            dir_goto((int)dir.index - 1);
            break;
        case 'p':
            dir_goto(dir.index + dir_pagesize());
            break;
        case 'u':
            dir_goto((int)dir.index - dir_pagesize());
            break;
        case 't':
        case KEY_HOME:
            dir_goto(0);
            break;
        case 'e':
            dir_goto(dir.count);
            break;

        case KEY_CURSOR_RIGHT:
            if (is80)
            {
                // Two columns: move to the right column
                if ((dir.index - dir.pagefirst) < DIR_ROWS)
                {
                    dir_goto(dir.index + DIR_ROWS);
                }
                break;
            }
            // 40 columns: enter, as RETURN
        case KEY_RETURN:
            if (!dir.present)
            {
                break;
            }
            if (entry.meta.type == CBM_T_PRG && imagekind == IMAGE_NONE)
            {
                if (browse_start(PICK_PROGRAM, entry.name, browse_runflags()))
                {
                    return;
                }
            }
            else if (imagekind == IMAGE_REU)
            {
                if (!bs.trace || !bs.softiec)
                {
                    browse_message("REU images: dirtrace on the hyperspeed drive.");
                }
                else if (browse_pick(PICK_REU, entry.name, 0))
                {
                    return;
                }
                else
                {
                    browse_screen();
                }
            }
            else if (!browse_cd(entry.name))
            {
                browse_message("Cannot enter. Press a key.");
            }
            break;

        case KEY_CURSOR_LEFT:
            if (is80)
            {
                if ((dir.index - dir.pagefirst) >= DIR_ROWS)
                {
                    dir_goto((int)dir.index - DIR_ROWS);
                }
                break;
            }
            // 40 columns: up, as DEL
        case KEY_DEL:
            browse_cd("..");
            break;
        case KEY_UPARROW:
            browse_cd(NULL);
            break;

        case 'd':
            bs.trace = !bs.trace;
            bs.tracepath[0] = 0;
            bs.inimage = false;
            if (bs.trace)
            {
                browse_cd(NULL);
            }
            else
            {
                dir_draw();
            }
            browse_toggle(TOGGLE_TRACE);
            break;
        case '8':
            bs.force8 = !bs.force8;
            browse_toggle(TOGGLE_FORCE8);
            break;
        case 'f':
            bs.fast = !bs.fast;
            browse_toggle(TOGGLE_FAST);
            break;
        case '1':
            bs.comma1 = !bs.comma1;
            browse_toggle(TOGGLE_COMMA1);
            break;
        case 'o':
            bs.demo = !bs.demo;
            browse_toggle(TOGGLE_DEMO);
            break;

        case KEY_F5:
        {
            // Default slot name: the disk name (header up to the comma)
            char diskname[17];
            char n = 0;
            while (n < sizeof(diskname) - 1 && dir.header[n] && dir.header[n] != ',')
            {
                diskname[n] = dir.header[n];
                n++;
            }
            diskname[n] = 0;
            if (browse_start(PICK_BOOT, diskname, browse_runflags() | EXEC_BOOT))
            {
                return;
            }
            break;
        }
        case '6':
            if (dir.present && entry.meta.type == CBM_T_PRG &&
                browse_start(PICK_PROGRAM, entry.name, EXEC_RUN64 | (bs.demo ? EXEC_DEMO : 0)))
            {
                return;
            }
            break;

        case 'a':
        case 'b':
            if (imagekind != IMAGE_DISK)
            {
                break;
            }
            if (!bs.trace || !bs.softiec)
            {
                browse_message("Mounts: dirtrace on the hyperspeed drive.");
            }
            else if (browse_pick((key == 'a') ? PICK_MOUNT_A : PICK_MOUNT_B, entry.name, 0))
            {
                return;
            }
            else
            {
                browse_screen();
            }
            break;
        case 'm':
            if (!bs.inimage || !dir.present || entry.meta.type != CBM_T_PRG)
            {
                break;
            }
            if (browse_pick(PICK_MOUNTRUN, entry.name, browse_runflags() & ~EXEC_FRC8))
            {
                return;
            }
            browse_screen();
            break;

        case 'q':
        case KEY_F7:
            return;
        default:
            break;
        }
    }
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
