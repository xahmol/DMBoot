/*
DMBoot 128 v5 - IEC directory parsing and dirtrace paths

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Hardware independent parts of the file browser, tested on the PC
(tests/host): directory line parsing, image name recognition and the
dirtrace path. Letters are written as PETSCII codes through PET_LC(), so
the result does not depend on the compiler's charmap.

Code and resources from others used:
-   DraBrowse (db*) by Sascha Bader, adapted by Dirk Jagdmann (doj)
    https://github.com/doj/dracopy
    Directory entry parsing, via src/filebrowse.c of my UBoot64-v2 project
    (https://github.com/xahmol/UBoot64-v2). Adapted: parsing separated from
    reading, 16-bit block counts, bounds on every index.
*/

#include <string.h>
#include "petconv.h"
#include "dirparse.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// PETSCII code of an unshifted letter (letters are contiguous in both the
// PC's and the petscii.h charmap, so this is charmap independent)
#define PET_LC(c)           ((char)((c) - 'a' + 0x41))
#define PET_SHIFT_MASK      0x7f
#define PET_QUOTE           0x22
#define PET_SPACE           0x20
#define PET_SHIFTSPACE      0xa0
#define PET_DOT             0x2e
#define PET_SLASH           0x2f
#define PET_COLON           0x3a
#define PET_LOCKED          0x3c    // "<" after the type of a locked file
#define PET_RVS_ON          0x12    // Starts the disk header line
#define PET_DIGIT(d)        ((char)(0x30 + (d)))

// ---------------------------------------------------------------------------
// Title:       Line ends with a file type
// Description: Tests whether the stripped directory line ends with three
//              given characters.
// Syntax:      static bool ends_with(const char *line, char len, char a,
//                                    char b, char c);
// Input:       line, len - line and its stripped length
//              a, b, c   - PETSCII characters
// Output:      true on a match
// ---------------------------------------------------------------------------
static bool ends_with(const char *line, char len, char a, char b, char c)
{
    return len >= 3 && line[len - 3] == a && line[len - 2] == b && line[len - 1] == c;
}

// ---------------------------------------------------------------------------
// Title:       Parse a directory line
// Description: Parses the text of one directory BASIC line (after the link
//              and block count) into name and type. The disk header (a
//              reverse-on character before the quotes) fills diskid and
//              gets its padding removed; the "blocks free" line gets type
//              CBM_T_FREE; unknown types CBM_T_OTHER (DraBrowse/UBoot64
//              took every unknown type, and locked "PRG<" files, for the
//              header). Trailing blanks and shifted spaces are ignored; the
//              name is taken between the first pair of quotes.
// Syntax:      char dir_parse_line(const char *line, char len,
//                                  struct DirElement *element,
//                                  char *diskid);
// Input:       line    - line text (PETSCII), len characters
//              len     - number of characters (< DIR_LINE_MAX)
//              element - entry to fill (meta.size is set by the caller)
//              diskid  - DISK_ID_LEN + 1 bytes, filled for the header
// Output:      DIRPARSE_OK, or DIRPARSE_SKIP for a line that is no entry
// ---------------------------------------------------------------------------
char dir_parse_line(const char *line, char len, struct DirElement *element, char *diskid)
{
    char i;
    char n;

    element->name[0] = 0;
    element->meta.length = 1;
    if (len && (line[0] & PET_SHIFT_MASK) == PET_LC('b'))
    {
        element->meta.type = CBM_T_FREE;
        return DIRPARSE_OK;
    }
    if (len < 5)
    {
        return DIRPARSE_SKIP;
    }

    while (len > 0 && (line[len - 1] == 0 || line[len - 1] == PET_SPACE || line[len - 1] == PET_SHIFTSPACE))
    {
        len--;
    }
    // Locked files end in "<" (e.g. "PRG<"): not part of the type
    if (len > 0 && line[len - 1] == PET_LOCKED)
    {
        len--;
    }

    // Name between the quotes; a reverse-on character before them marks the
    // disk header line (1541, SD2IEC and SoftIEC all send it)
    bool header = false;
    for (i = 0; i < len && line[i] != PET_QUOTE; i++)
    {
        if (line[i] == PET_RVS_ON)
        {
            header = true;
        }
    }
    n = 0;
    for (i++; i < len && line[i] != PET_QUOTE && n < sizeof(element->name) - 1; i++)
    {
        element->name[n++] = line[i];
    }
    element->name[n] = 0;
    element->meta.length = n + 1;

    if (header)
    {
        // Disk header: name (padded with spaces inside the quotes), then
        // the disk ID
        element->meta.type = CBM_T_HEADER;
        while (n > 0 && (element->name[n - 1] == PET_SPACE || element->name[n - 1] == PET_SHIFTSPACE))
        {
            element->name[--n] = 0;
        }
        element->meta.length = n + 1;
        if (i < len && line[i] == PET_QUOTE)
        {
            i++;
        }
        if (i < len && line[i] == PET_SPACE)
        {
            i++;
        }
        for (n = 0; n < DISK_ID_LEN; n++)
        {
            diskid[n] = (i < len) ? line[i++] : PET_SPACE;
        }
        diskid[DISK_ID_LEN] = 0;
    }
    else if (ends_with(line, len, PET_LC('p'), PET_LC('r'), PET_LC('g'))) element->meta.type = CBM_T_PRG;
    else if (ends_with(line, len, PET_LC('s'), PET_LC('e'), PET_LC('q'))) element->meta.type = CBM_T_SEQ;
    else if (ends_with(line, len, PET_LC('u'), PET_LC('s'), PET_LC('r'))) element->meta.type = CBM_T_USR;
    else if (ends_with(line, len, PET_LC('d'), PET_LC('e'), PET_LC('l'))) element->meta.type = CBM_T_DEL;
    else if (ends_with(line, len, PET_LC('r'), PET_LC('e'), PET_LC('l'))) element->meta.type = CBM_T_REL;
    else if (ends_with(line, len, PET_LC('c'), PET_LC('b'), PET_LC('m'))) element->meta.type = CBM_T_CBM;
    else if (ends_with(line, len, PET_LC('d'), PET_LC('i'), PET_LC('r'))) element->meta.type = CBM_T_DIR;
    else if (ends_with(line, len, PET_LC('v'), PET_LC('r'), PET_LC('p'))) element->meta.type = CBM_T_VRP;
    else if (ends_with(line, len, PET_LC('l'), PET_LC('n'), PET_LC('k'))) element->meta.type = CBM_T_LNK;
    else                                                                   element->meta.type = CBM_T_OTHER;
    return DIRPARSE_OK;
}

// ---------------------------------------------------------------------------
// Title:       Image kind of a file name
// Description: Recognises disk images (.d64 .g64 .d71 .g71 .d81 .g81 .dnp)
//              and REU images (.reu) by their extension, in either case.
// Syntax:      char dir_imagekind(const char *name);
// Input:       name - file name (PETSCII)
// Output:      IMAGE_NONE, IMAGE_DISK or IMAGE_REU
// ---------------------------------------------------------------------------
char dir_imagekind(const char *name)
{
    unsigned l = strlen(name);
    char a, b, c;

    if (l < 5 || name[l - 4] != PET_DOT)
    {
        return IMAGE_NONE;
    }
    a = name[l - 3] & PET_SHIFT_MASK;
    b = name[l - 2] & PET_SHIFT_MASK;
    c = name[l - 1] & PET_SHIFT_MASK;
    if ((a == PET_LC('d') || a == PET_LC('g')) && b == PET_DIGIT(6) && c == PET_DIGIT(4))
    {
        return IMAGE_DISK;
    }
    if ((a == PET_LC('d') || a == PET_LC('g')) && (b == PET_DIGIT(7) || b == PET_DIGIT(8)) && c == PET_DIGIT(1))
    {
        return IMAGE_DISK;
    }
    if (a == PET_LC('d') && b == PET_LC('n') && c == PET_LC('p'))
    {
        return IMAGE_DISK;
    }
    if (a == PET_LC('r') && b == PET_LC('e') && c == PET_LC('u'))
    {
        return IMAGE_REU;
    }
    return IMAGE_NONE;
}

// ---------------------------------------------------------------------------
// Title:       Does a name fit in the dirtrace
// Description: Tells whether "name/" can be added to the trace. Check this
//              before changing the drive's directory, so trace and drive
//              stay in step.
// Syntax:      bool trace_fits(const char *trace, unsigned size,
//                              const char *name);
// Input:       trace - dirtrace (PETSCII, each directory followed by '/')
//              size  - size of the trace buffer
//              name  - directory name
// Output:      true when it fits
// ---------------------------------------------------------------------------
bool trace_fits(const char *trace, unsigned size, const char *name)
{
    return strlen(trace) + strlen(name) + 1 < size;
}

// ---------------------------------------------------------------------------
// Title:       Add a directory to the dirtrace
// Description: Appends "name/" when it fits (see trace_fits).
// Syntax:      void trace_add(char *trace, unsigned size, const char *name);
// Input:       trace - dirtrace
//              size  - size of the trace buffer
//              name  - directory name
// Output:      trace
// ---------------------------------------------------------------------------
void trace_add(char *trace, unsigned size, const char *name)
{
    unsigned len = strlen(trace);

    if (!trace_fits(trace, size, name))
    {
        return;
    }
    strcpy(trace + len, name);
    len += strlen(name);
    trace[len++] = PET_SLASH;
    trace[len] = 0;
}

// ---------------------------------------------------------------------------
// Title:       Remove the last traced directory
// Description: Drops the last "name/" from the dirtrace.
// Syntax:      unsigned trace_up(char *trace);
// Input:       trace - dirtrace
// Output:      New length of the trace
// ---------------------------------------------------------------------------
unsigned trace_up(char *trace)
{
    unsigned len = strlen(trace);

    if (len)
    {
        len--;                              // Skip the trailing '/'
        while (len > 0 && trace[len - 1] != PET_SLASH)
        {
            len--;
        }
        trace[len] = 0;
    }
    return len;
}

// ---------------------------------------------------------------------------
// Title:       DOS command for the dirtrace
// Description: The command that changes to the traced directory: "cd:/" +
//              trace on the SoftIEC (hyperspeed) drive, "cd//" + trace on
//              other drives (as DMBoot v4 pathconcat).
// Syntax:      void trace_command(char *dst, unsigned size,
//                                 const char *trace, bool softiec);
// Input:       dst     - destination
//              size    - size of dst
//              trace   - dirtrace
//              softiec - device is the SoftIEC drive
// Output:      dst (PETSCII)
// ---------------------------------------------------------------------------
void trace_command(char *dst, unsigned size, const char *trace, bool softiec)
{
    char prefix[5];

    prefix[0] = PET_LC('c');
    prefix[1] = PET_LC('d');
    prefix[2] = softiec ? PET_COLON : PET_SLASH;
    prefix[3] = PET_SLASH;
    prefix[4] = 0;
    strncpy(dst, prefix, size - 1);
    dst[size - 1] = 0;
    strncat(dst, trace, size - 1 - strlen(dst));
}

// ---------------------------------------------------------------------------
// Title:       Ultimate path of the dirtrace
// Description: The traced directory as an Ultimate file system path:
//              "/" + trace, PETSCII converted to ASCII.
// Syntax:      void trace_ultpath(char *dst, unsigned size,
//                                 const char *trace);
// Input:       dst   - destination
//              size  - size of dst (at least 2)
//              trace - dirtrace
// Output:      dst (ASCII)
// ---------------------------------------------------------------------------
void trace_ultpath(char *dst, unsigned size, const char *trace)
{
    dst[0] = PET_SLASH;
    pet2asc(dst + 1, trace, size - 1);
}
