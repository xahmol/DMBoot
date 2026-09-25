/*
DMBoot 128 v5 - C128 banking and overlay layer

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Provides MMU-banked memory access (bank 0 / bank 1 / RAM under ROM), the
one-time loading of overlay and low-memory-code files and the MMU set-up
and restore around the program run. The bnk_* access routines live in the
low-memory code overlay (LMC, $1300-$1AFF) inside the 8 KB common RAM, so
they keep running while any bank is switched in.
*/

#ifndef BANKING_H
#define BANKING_H

#include "defines.h"

// Scroll directions, expected by the VDC library suite (vdc_win.c)
#define SCROLL_LEFT     0x01
#define SCROLL_RIGHT    0x02
#define SCROLL_DOWN     0x04
#define SCROLL_UP       0x08

// Resident functions (main program region)
char getcurrentdevice(void);
bool load_overlay(const char *fname);
bool bnk_init(void);
void bnk_exit(void);

// Low-memory code (LMC) functions, must never be inlined into callers
__noinline char bnk_readb(char cr, volatile char *p);
__noinline void bnk_writeb(char cr, volatile char *p, char b);
__noinline void bnk_memcpy(char dcr, volatile char *dp, char scr, volatile char *sp, unsigned size);
__noinline void bnk_memset(char cr, volatile char *p, char val, unsigned size);
__noinline void bnk_cpytovdc(unsigned vdcdest, char scr, volatile char *sp, unsigned size);
__noinline void bnk_cpyfromvdc(char dcr, volatile char *dp, unsigned vdcsrc, unsigned size);
__noinline void bnk_redef_charset(unsigned vdcdest, char scr, volatile char *sp, unsigned size);

#pragma compile("banking.c")

#endif // BANKING_H
