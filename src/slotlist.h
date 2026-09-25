/*
DMBoot 128 v5 - Slot list (resident)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Drawing and picking of the 36 menu slots, shared by the main menu
(overlay 1) and the slot editor (overlay 2): overlays cannot call each
other, so this lives in the resident program.
*/

#ifndef SLOTLIST_H
#define SLOTLIST_H

#include "defines.h"

// Screen layout of the slot screens
#define SLOTLIST_FIRST_ROW  3
#define SLOTLIST_ROWS       18      // Slots per column (80) / page (40)
#define SLOTLIST_COLUMN     40      // 80 columns: two columns of 40
#define SLOTLIST_NAME_X     4       // Name starts after " k "
#define SLOTLIST_LEGEND_ROW 21      // Function key legend, rows 21-23
#define SLOTLIST_PROMPT_ROW 24
#define SLOTLIST_PAGES      2       // 40 columns
#define NO_SLOT             0xff

// Key codes
#define KEY_STOP            0x03
#define KEY_DEL             0x14
#define KEY_HOME            0x13
#define KEY_CURSOR_DOWN     0x11
#define KEY_CURSOR_UP       0x91
#define KEY_CURSOR_LEFT     0x9d
#define KEY_CURSOR_RIGHT    0x1d

// slotlist_draw_slot flags
#define SLOTLIST_SELECTED   0x01    // Highlight (and draw "<empty>" for an empty slot)

// REU sizes (Slot.reusize, cfg.geos.reusize)
#define REU_SIZES           8
extern const char *const reusizenames[REU_SIZES];

// Auto-boot timeout (cfg.timeoutidx)
#define TIMEOUT_OPTIONS     5
extern const char timeoutseconds[TIMEOUT_OPTIONS];

void fkey_hint(char x, char y, const char *key, const char *text);
char slotlist_page(char slot);
void slotlist_draw_slot(char slot, char page, char flags);
void slotlist_draw(char page);
char slotlist_pick(char *page, bool allowempty);
void slotlist_clear_bottom(void);

#pragma compile("slotlist.c")

#endif // SLOTLIST_H
