/*
DMBoot 128 v5 - C128 Device Manager ROM extended API

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Calls into the extended API jump table of the C128 Device Manager ROM by
Bart van Leeuwen (extapi.txt, jump table at $807B-$808F). The calls run
from the low-memory code overlay, because the external function ROM
configuration ($FF00 = $2A) only keeps RAM visible below $8000.
*/

#ifndef DMAPI_H
#define DMAPI_H

#include "defines.h"

// Device Manager drive type codes returned by dm_get_drivetype()
#define DM_TYPE_NONE        0x00
#define DM_TYPE_UNKNOWN     0x01
#define DM_TYPE_UII_A       0x02
#define DM_TYPE_UII_B       0x03
#define DM_TYPE_SD2IEC      0x04
#define DM_TYPE_MICROIEC    0x05
#define DM_TYPE_PRINTER     0x06
#define DM_TYPE_PLOTTER     0x07
#define DM_TYPE_UII_SOFTIEC 0x08
#define DM_TYPE_PI1541      0x09
#define DM_TYPE_1540        0x28
#define DM_TYPE_1541        0x29
#define DM_TYPE_1570        0x46
#define DM_TYPE_1571        0x47
#define DM_TYPE_1581        0x51
#define DM_TYPE_CMD_RL      0x80
#define DM_TYPE_CMD_HD      0xc0
#define DM_TYPE_CMD_FD      0xe0
#define DM_TYPE_CMD_RD      0xf0

// Maximum program name length for dm_run64 (CBM filename)
#define DM_PRGNAME_MAX      17

// Resident wrappers
void dm_query(struct DMApiInfo *info);
bool dm_prepare_run64(const char *name, char device);
unsigned dm_run64_address(void);
unsigned dm_version(void);

// Low-memory code functions
__noinline void dm_api_getversion(void);
__noinline void dm_api_get_hsid(void);
__noinline void dm_api_set_hsid8(void);
__noinline char dm_api_get_drivetype(char device);

#pragma compile("dmapi.c")

#endif // DMAPI_H
