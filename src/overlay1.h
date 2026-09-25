/*
DMBoot 128 v5 - Phase 0 dummy overlay 1 (stored in bank 1 RAM)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef OVERLAY1_H
#define OVERLAY1_H

#define OVERLAY1_SIGNATURE 0xa1

__noinline char overlay1_selftest(void);

#pragma compile("overlay1.c")

#endif // OVERLAY1_H
