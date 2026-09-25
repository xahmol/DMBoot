/*
DMBoot 128 v5 - REU access for the C128

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Wraps Oscar64's <c64/reu.h> DMA transfers for use on the C128:
- every transfer runs at 1 MHz (VIC-IIe clock bit cleared and restored),
  because REU DMA at 2 MHz can crash the machine after completion;
- REU size detection routes the probe byte through a call barrier to avoid
  an Oscar64 -O2 dead-code elimination (documented in ARCHITECTURE.md
  §12.11 of my UBoot64-v2 project, https://github.com/xahmol/UBoot64-v2);
- all C128-side buffers must be in bank 0 (MMU RCR DMA bank bit stays 0).
*/

#ifndef REU128_H
#define REU128_H

#include "defines.h"

unsigned reu128_count_pages(void);
void reu128_store(unsigned long raddr, const volatile char *src, unsigned length);
void reu128_load(unsigned long raddr, volatile char *dst, unsigned length);

#pragma compile("reu128.c")

#endif // REU128_H
