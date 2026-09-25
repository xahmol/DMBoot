/*
DMBoot 128 - DMBoot v4 to v5 data conversion

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef V4CONVERT_H
#define V4CONVERT_H

#include "defines.h"

// v4 slot file: load address, then 36 slots of 512 bytes (two 256-byte
// pages: path..cfgvs, then the image fields; see v4 getslotfromem)
#define V4_LOADADDR_BYTES   2
#define V4_SLOT_STRIDE      512
#define V4_SLOTS_BYTES      (SLOTS * V4_SLOT_STRIDE)

// v4 utility settings file (DMBCFGFILE)
#define V4CFG_SIZE          328

bool v4_convert_slot(const char *v4, struct SlotStruct *slot);
bool v4_convert_config(const char *v4cfg, bool havev4, const char *dmbootdir, struct ConfigStruct *config);

#pragma compile("v4convert.c")

#endif // V4CONVERT_H
