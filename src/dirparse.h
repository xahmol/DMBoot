/*
DMBoot 128 v5 - IEC directory parsing and dirtrace paths

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef DIRPARSE_H
#define DIRPARSE_H

#include "defines.h"

// CBM file types (as DraBrowse / cc65 cbm.h)
#define CBM_T_REG           0x10    // Bit set for regular files
#define CBM_T_SEQ           0x10
#define CBM_T_PRG           0x11
#define CBM_T_USR           0x12
#define CBM_T_REL           0x13
#define CBM_T_VRP           0x14
#define CBM_T_DEL           0x00
#define CBM_T_CBM           0x01    // 1581 sub-partition
#define CBM_T_DIR           0x02    // CMD / SoftIEC sub-directory
#define CBM_T_LNK           0x03
#define CBM_T_OTHER         0x04
#define CBM_T_HEADER        0x05    // Disk header
#define CBM_T_FREE          0x64    // "blocks free" line

#define DIR_LINE_MAX        64      // One IEC directory line
#define DISK_ID_LEN         5

// dir_parse_line results
#define DIRPARSE_OK         0
#define DIRPARSE_SKIP       2       // Too short to be an entry

// dir_imagekind results
#define IMAGE_NONE          0
#define IMAGE_DISK          1
#define IMAGE_REU           2

// Directory entry metadata as stored in the REU, followed by the name
struct DirMeta
{
    unsigned long next;             // REU address of the next entry, 0 = none (first field)
    unsigned long prev;             // REU address of the previous entry, 0 = none
    unsigned size;                  // Size in blocks
    char type;                      // CBM_T_*
    char length;                    // Stored name length including the terminator
};

struct DirElement
{
    struct DirMeta meta;
    char name[MAXFILENAME];
};

char dir_parse_line(const char *line, char len, struct DirElement *element, char *diskid);
char dir_imagekind(const char *name);
bool trace_fits(const char *trace, unsigned size, const char *name);
void trace_add(char *trace, unsigned size, const char *name);
unsigned trace_up(char *trace);
void trace_command(char *dst, unsigned size, const char *trace, bool softiec);
void trace_ultpath(char *dst, unsigned size, const char *trace);

#pragma compile("dirparse.c")

#endif // DIRPARSE_H
