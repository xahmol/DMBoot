/*
DMBoot 128 v5 - Phase 0 dummy overlay 2 (stored in bank 0 RAM under the KERNAL ROM)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef OVERLAY2_H
#define OVERLAY2_H

#define OVERLAY2_SIGNATURE 0xa2

__noinline char overlay2_selftest(void);

#pragma compile("overlay2.c")

#endif // OVERLAY2_H
