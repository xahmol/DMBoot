/*
DMBoot 128 v5 - Device Manager path conventions

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

The only module that knows where the Device Manager ROM keeps DMBoot:
the "11" directory on the USB stick (Software IEC partition 11). When the
Device Manager ROM defines its firmware 3.15 layout, only this module
changes (docs/REBUILD_PLAN.md §9).
*/

#ifndef DMPATHS_H
#define DMPATHS_H

#include "defines.h"

// resolve_storage_path() results
#define STORAGE_NONE        0   // No storage device found
#define STORAGE_PRESENT     1   // Device found, no config file yet
#define STORAGE_CONFIG      2   // Existing config file found

#define STORAGE_CANDIDATES  4

extern char configpath[STORAGE_PATH_MAX];
extern char configfilename[];
extern char slotfilename[];

char resolve_storage_path(void);

#pragma compile("dmpaths.c")

#endif // DMPATHS_H
