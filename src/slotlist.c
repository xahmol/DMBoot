/*
DMBoot 128 v5 - Slot list (resident)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Slot drawing ported from presentmenuslots / printnewmenuslot in
src/slotmenu.c of my UBoot64-v2 project
(https://github.com/xahmol/UBoot64-v2); adapted: 36 slots in two columns
(80 columns) or two pages (40 columns), shared by two overlays.
*/

#include <string.h>
#include <petscii.h>
#include "defines.h"
#include "dualwin.h"
#include "core.h"
#include "fileio.h"
#include "slotlist.h"

#define DEFAULT_MARK        " [D]"
#define EMPTY_TEXT          "<empty>"
#define NAME_TEXT_MAX       (MAXMENUNAME + sizeof(DEFAULT_MARK))

// REU size names per size index (Ultimate "Load REU" sizes)
const char *const reusizenames[REU_SIZES] = {
    "128 KB", "256 KB", "512 KB", "1 MB", "2 MB", "4 MB", "8 MB", "16 MB"
};

// Auto-boot timeout in seconds per cfg.timeoutidx (0 = off)
const char timeoutseconds[TIMEOUT_OPTIONS] = { 0, 1, 3, 5, 10 };

// ---------------------------------------------------------------------------
// Title:       Print a function key hint
// Description: Prints a key label in reverse followed by its description.
// Syntax:      void fkey_hint(char x, char y, const char *key,
//                             const char *text);
// Input:       x, y - screen position
//              key  - key label, e.g. " F1 "
//              text - description
// Output:      None
// ---------------------------------------------------------------------------
void fkey_hint(char x, char y, const char *key, const char *text)
{
    char len = dwin_putat_string_reverse(&screenwin, x, y, key, cfg.colors.key);
    dwin_putat_string(&screenwin, x + len + 1, y, text, cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Page of a slot
// Description: Returns the 40-column page that shows a slot (always 0 in
//              80 columns, where all slots fit).
// Syntax:      char slotlist_page(char slot);
// Input:       slot - slot number
// Output:      Page number
// ---------------------------------------------------------------------------
char slotlist_page(char slot)
{
    return dwin_is80() ? 0 : slot / SLOTLIST_ROWS;
}

// ---------------------------------------------------------------------------
// Title:       Draw one slot line
// Description: Draws the key and name of a slot at its place in the list,
//              when it is on the shown page. Empty slots are drawn only
//              when selected (as "<empty>"); otherwise their line is
//              cleared. "[D]" marks the auto-boot default slot.
//              Loads the slot into Slot.
// Syntax:      void slotlist_draw_slot(char slot, char page, char flags);
// Input:       slot  - slot number
//              page  - shown page (40 columns)
//              flags - SLOTLIST_SELECTED to highlight
// Output:      None
// ---------------------------------------------------------------------------
void slotlist_draw_slot(char slot, char page, char flags)
{
    char label[4];
    char name[NAME_TEXT_MAX];
    char x = 0;
    char y;
    bool selected = (flags & SLOTLIST_SELECTED) != 0;
    char colorkey = selected ? cfg.colors.text_input : cfg.colors.key;
    char colortext = selected ? cfg.colors.text_input : cfg.colors.text;

    if (dwin_is80())
    {
        x = (slot / SLOTLIST_ROWS) * SLOTLIST_COLUMN;
    }
    else if (slotlist_page(slot) != page)
    {
        return;
    }
    y = SLOTLIST_FIRST_ROW + slot % SLOTLIST_ROWS;

    dwin_fill_rect(&screenwin, x, y, SLOTLIST_COLUMN, 1, ' ', cfg.colors.text);
    get_slot_from_reu(slot);
    if (!Slot.menu[0] && !selected)
    {
        return;
    }

    label[0] = selected ? '-' : ' ';
    label[1] = menuslotlabel(slot);
    label[2] = ' ';
    label[3] = 0;
    dwin_putat_string_reverse(&screenwin, x, y, label, colorkey);

    if (Slot.menu[0])
    {
        strncpy(name, Slot.menu, MAXMENUNAME - 1);
        name[MAXMENUNAME - 1] = 0;
        if (Slot.isdefault == 1)
        {
            strncat(name, DEFAULT_MARK, sizeof(name) - 1 - strlen(name));
        }
    }
    else
    {
        strncpy(name, EMPTY_TEXT, sizeof(name) - 1);
        name[sizeof(name) - 1] = 0;
    }
    dwin_putat_string(&screenwin, x + SLOTLIST_NAME_X, y, name, colortext);
}

// ---------------------------------------------------------------------------
// Title:       Draw the slot list
// Description: Draws all slots (80 columns: two columns of 18) or one page
//              of 18 slots (40 columns).
// Syntax:      void slotlist_draw(char page);
// Input:       page - 40 columns: 0 = slots 0-17, 1 = slots 18-35
// Output:      None
// ---------------------------------------------------------------------------
void slotlist_draw(char page)
{
    for (char slot = 0; slot < SLOTS; slot++)
    {
        slotlist_draw_slot(slot, page, 0);
    }
}

// ---------------------------------------------------------------------------
// Title:       Clear the legend and prompt rows
// Description: Clears the rows below the slot list (legend and prompt).
// Syntax:      void slotlist_clear_bottom(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void slotlist_clear_bottom(void)
{
    dwin_fill_rect(&screenwin, 0, SLOTLIST_LEGEND_ROW, screenwin.wx,
                   SLOTLIST_PROMPT_ROW - SLOTLIST_LEGEND_ROW + 1, ' ', cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Pick a slot
// Description: Waits for a slot key. In 40 columns cursor left/right switch
//              the shown page. F7 or STOP cancels.
// Syntax:      char slotlist_pick(char *page, bool allowempty);
// Input:       page       - shown page (40 columns), updated on a switch
//              allowempty - true to accept empty slots too
// Output:      Slot number, or NO_SLOT when cancelled
// ---------------------------------------------------------------------------
char slotlist_pick(char *page, bool allowempty)
{
    while (true)
    {
        char key = key_wait();

        if (key == KEY_F7 || key == KEY_STOP)
        {
            return NO_SLOT;
        }
        if (!dwin_is80() && (key == KEY_CURSOR_LEFT || key == KEY_CURSOR_RIGHT))
        {
            *page ^= 1;
            slotlist_draw(*page);
        }
        else if (isslotkey(key))
        {
            char slot = keytomenuslot(key);
            get_slot_from_reu(slot);
            if (allowempty || Slot.menu[0])
            {
                return slot;
            }
        }
    }
}
