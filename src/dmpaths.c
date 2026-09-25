/*
DMBoot 128 v5 - Device Manager path conventions

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <string.h>
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "dmpaths.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// Strings sent to the Ultimate are plain ASCII: switch off the petscii.h
// charmap (UBoot64-v2 ARCHITECTURE.md §12.10) for these literals only.
#pragma charmap(97, 97, 26)
#pragma charmap(65, 65, 26)

// Where the Device Manager ROM keeps DMBoot, in priority order: the USB
// wildcard first (v4 behaviour), then the numbered ports for several sticks.
char storagepaths[STORAGE_CANDIDATES][STORAGE_PATH_MAX] = {
    "/usb*/11/",
    "/usb0/11/",
    "/usb1/11/",
    "/usb2/11/"
};

char configfilename[] = "dmbconf.cfg";
char slotfilename[] = "dmbslots.cfg";

// Back to the petscii.h charmap for everything that follows
#pragma charmap(97, 65, 26)
#pragma charmap(65, 97, 26)

char configpath[STORAGE_PATH_MAX];

// ---------------------------------------------------------------------------
// Title:       Resolve the storage path
// Description: Tries the storage candidates in priority order. The first
//              candidate that already holds the config file wins; if none
//              has it, the first candidate that exists is used, so a fresh
//              install creates its files there. Sets configpath.
// Syntax:      char resolve_storage_path(void);
// Input:       None
// Output:      STORAGE_CONFIG, STORAGE_PRESENT or STORAGE_NONE
// ---------------------------------------------------------------------------
char resolve_storage_path(void)
{
    char firstpresent = STORAGE_CANDIDATES;

    for (char x = 0; x < STORAGE_CANDIDATES; x++)
    {
        uii_change_dir(storagepaths[x]);
        if (!UII_SUCCESS)
        {
            continue;
        }
        if (firstpresent == STORAGE_CANDIDATES)
        {
            firstpresent = x;
        }

        uii_open_file(0x01, configfilename);
        if (UII_SUCCESS)
        {
            uii_close_file();
            strncpy(configpath, storagepaths[x], sizeof(configpath) - 1);
            configpath[sizeof(configpath) - 1] = 0;
            return STORAGE_CONFIG;
        }
    }

    if (firstpresent < STORAGE_CANDIDATES)
    {
        strncpy(configpath, storagepaths[firstpresent], sizeof(configpath) - 1);
        configpath[sizeof(configpath) - 1] = 0;
        return STORAGE_PRESENT;
    }
    configpath[0] = 0;
    return STORAGE_NONE;
}
