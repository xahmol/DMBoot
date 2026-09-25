/*
DMBoot 128 v5 - GEOS 128 RAM boot (low-memory code)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef GEOSBOOT_H
#define GEOSBOOT_H

// geos_boot results (it does not return on success)
#define GEOS_ERROR_NOREU    1       // No REU found
#define GEOS_ERROR_NOBOOT   2       // No GEOS rboot loader in the REU image

__noinline char geos_boot(void);

#pragma compile("geosboot.c")

#endif // GEOSBOOT_H
