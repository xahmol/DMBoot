/*
DMBoot 128 v5 - Main menu overlay

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Menu and auto-boot countdown ported from src/slotmenu.c of my UBoot64-v2
project (https://github.com/xahmol/UBoot64-v2); adapted: 36 slots in two
columns (80 columns) or two pages (40 columns), DMBoot F-key layout, the
countdown returns the slot key instead of starting it (overlays cannot
call each other).
*/

#include <string.h>
#include <petscii.h>
#include <c64/cia.h>
#include "defines.h"
#include "dualwin.h"
#include "testmode.h"
#include "core.h"
#include "fileio.h"
#include "slotmenu.h"

#pragma overlay(dmbovl1, 2)
#pragma section(codeovl1, 0)
#pragma section(dataovl1, 0)
#pragma section(bssovl1, 0)
#pragma region(ovl1, OVERLAYLOAD, OVERLAY_SLOT_END, , 2, { codeovl1, dataovl1, bssovl1 })

#pragma code(codeovl1)
#pragma data(dataovl1)
#pragma bss(bssovl1)

#define MENU_FIRST_ROW      3
#define MENU_ROWS           18      // Slots per column / page
#define MENU_COLUMN_WIDTH   40      // 80 columns: two columns of 40
#define MENU_NAME_OFFSET    4       // Name starts after " k "
#define LEGEND_ROW          21
#define PROMPT_ROW          24
#define NO_SLOT             0xff
#define KEY_CURSOR_LEFT     0x9d
#define KEY_CURSOR_RIGHT    0x1d
#define DEFAULT_MARK        " [D]"
#define NAME_TEXT_MAX       (MAXMENUNAME + sizeof(DEFAULT_MARK))
#define TIMEOUT_OPTIONS     5

// Auto-boot timeout in seconds per cfg.timeoutidx (0 = off)
static const char timeoutseconds[TIMEOUT_OPTIONS] = { 0, 1, 3, 5, 10 };

// ---------------------------------------------------------------------------
// Title:       Print a function key hint
// Description: Prints " Fx " in reverse followed by its description.
// Syntax:      void menu_fkey(char x, char y, const char *key,
//                             const char *text);
// Input:       x, y - screen position
//              key  - key label, e.g. " F1 "
//              text - description
// Output:      None
// ---------------------------------------------------------------------------
static void menu_fkey(char x, char y, const char *key, const char *text)
{
    char len = dwin_putat_string_reverse(&screenwin, x, y, key, cfg.colors.key);
    dwin_putat_string(&screenwin, x + len + 1, y, text, cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Draw one slot line
// Description: Draws the key and name of a non-empty slot at a screen
//              position ("[D]" marks the auto-boot default slot).
// Syntax:      void menu_draw_slot(char slot, char x, char y);
// Input:       slot - slot number
//              x, y - screen position
// Output:      None
// ---------------------------------------------------------------------------
static void menu_draw_slot(char slot, char x, char y)
{
    char label[4] = { ' ', 0, ' ', 0 };
    char name[NAME_TEXT_MAX];

    get_slot_from_reu(slot);
    if (!Slot.menu[0])
    {
        return;
    }

    label[1] = menuslotlabel(slot);
    dwin_putat_string_reverse(&screenwin, x, y, label, cfg.colors.key);

    strncpy(name, Slot.menu, MAXMENUNAME - 1);
    name[MAXMENUNAME - 1] = 0;
    if (Slot.isdefault == 1)
    {
        strncat(name, DEFAULT_MARK, sizeof(name) - 1 - strlen(name));
    }
    dwin_putat_string(&screenwin, x + MENU_NAME_OFFSET, y, name, cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Draw the main menu
// Description: Draws header, slots and the function key legend. 80 columns:
//              all 36 slots in two columns. 40 columns: one page of 18.
// Syntax:      void menu_draw(char page);
// Input:       page - 40 columns: 0 = slots 0-17, 1 = slots 18-35
// Output:      None
// ---------------------------------------------------------------------------
static void menu_draw(char page)
{
    dwin_clear(&screenwin);
    headertext("Welcome to your C128.", 1);

    if (dwin_is80())
    {
        for (char slot = 0; slot < SLOTS; slot++)
        {
            menu_draw_slot(slot, (slot / MENU_ROWS) * MENU_COLUMN_WIDTH, MENU_FIRST_ROW + slot % MENU_ROWS);
        }
        menu_fkey(0, LEGEND_ROW, " F1 ", "Filebrowser");
        menu_fkey(20, LEGEND_ROW, " F2 ", "Information");
        menu_fkey(40, LEGEND_ROW, " F3 ", "Edit/order/del");
        menu_fkey(60, LEGEND_ROW, " F4 ", "Configuration");
        menu_fkey(0, LEGEND_ROW + 1, " F5 ", "Go 64");
        menu_fkey(20, LEGEND_ROW + 1, " F6 ", "GEOS RAM boot");
        menu_fkey(40, LEGEND_ROW + 1, " F7 ", "Quit to BASIC");
    }
    else
    {
        for (char row = 0; row < MENU_ROWS; row++)
        {
            menu_draw_slot(page * MENU_ROWS + row, 0, MENU_FIRST_ROW + row);
        }
        menu_fkey(0, LEGEND_ROW, " F1 ", "Browse");
        menu_fkey(13, LEGEND_ROW, " F2 ", "Info");
        menu_fkey(24, LEGEND_ROW, " F3 ", "Edit");
        menu_fkey(0, LEGEND_ROW + 1, " F4 ", "Config");
        menu_fkey(13, LEGEND_ROW + 1, " F5 ", "Go 64");
        menu_fkey(24, LEGEND_ROW + 1, " F6 ", "GEOS");
        menu_fkey(0, LEGEND_ROW + 2, " F7 ", "Quit");
        menu_fkey(13, LEGEND_ROW + 2, " <> ", page ? "Page 2/2" : "Page 1/2");
    }
    dwin_putat_string(&screenwin, 0, PROMPT_ROW, "Make your choice.", cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Is this a valid menu choice
// Description: Accepts the function keys F1-F7 and keys of non-empty slots.
// Syntax:      bool menu_validkey(char key);
// Input:       key - raw PETSCII key
// Output:      true when the key selects something
// ---------------------------------------------------------------------------
static bool menu_validkey(char key)
{
    if (key >= KEY_F1 && key < KEY_F8)       // F1-F7
    {
        return true;
    }
    if (isslotkey(key))
    {
        get_slot_from_reu(keytomenuslot(key));
        return Slot.menu[0] != 0;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Title:       Find the default slot
// Description: Finds the slot marked as auto-boot default.
// Syntax:      char find_default_slot(void);
// Input:       None
// Output:      Slot number, or NO_SLOT
// ---------------------------------------------------------------------------
static char find_default_slot(void)
{
    for (char slot = 0; slot < SLOTS; slot++)
    {
        get_slot_from_reu(slot);
        if (Slot.isdefault == 1 && Slot.menu[0])
        {
            return slot;
        }
    }
    return NO_SLOT;
}

// ---------------------------------------------------------------------------
// Title:       BCD to binary
// Description: Converts a CIA time-of-day BCD value to binary.
// Syntax:      char bcdtoseconds(char bcd);
// Input:       bcd - BCD value (e.g. cia1.tods)
// Output:      Binary value
// ---------------------------------------------------------------------------
static char bcdtoseconds(char bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0f);
}

// ---------------------------------------------------------------------------
// Title:       Auto-boot countdown
// Description: When a default slot and a timeout are configured, shows a
//              countdown. Any key cancels it (the key is not used as a menu
//              choice).
// Syntax:      char autobootcountdown(void);
// Input:       None
// Output:      The default slot number when the countdown ran out, else
//              NO_SLOT
// ---------------------------------------------------------------------------
static char autobootcountdown(void)
{
    char seconds;
    char slot;
    char lastshown = NO_SLOT;
    char elapsed;

    if (!cfg.timeoutidx || cfg.timeoutidx >= TIMEOUT_OPTIONS)
    {
        return NO_SLOT;
    }
    slot = find_default_slot();
    if (slot == NO_SLOT)
    {
        return NO_SLOT;
    }
    seconds = timeoutseconds[cfg.timeoutidx];
    get_slot_from_reu(slot);

    dwin_clear(&screenwin);
    headertext("Welcome to your C128.", 1);
    dwin_putat_string(&screenwin, 0, MENU_FIRST_ROW, "Default boot slot:", cfg.colors.text);
    dwin_putat_string(&screenwin, 0, MENU_FIRST_ROW + 1, Slot.menu, cfg.colors.key);
    dwin_putat_string(&screenwin, 0, MENU_FIRST_ROW + 3, "Auto-boot in    sec.", cfg.colors.text);
    dwin_putat_string(&screenwin, 0, MENU_FIRST_ROW + 4, "Press any key to open the menu.", cfg.colors.text);

    cia1.todt = 0;
    cia1.tods = 0;
    do
    {
        elapsed = bcdtoseconds(cia1.tods);
        char remaining = (elapsed >= seconds) ? 0 : seconds - elapsed;
        if (remaining != lastshown)
        {
            char text[3] = { ' ', ' ', 0 };
            text[0] = remaining >= 10 ? '1' : ' ';
            text[1] = '0' + remaining % 10;
            dwin_putat_string(&screenwin, 13, MENU_FIRST_ROW + 3, text, cfg.colors.text);
            lastshown = remaining;
        }
        if (key_poll() != KEY_NONE)
        {
            return NO_SLOT;
        }
    } while (elapsed < seconds);

    return slot;
}

// ---------------------------------------------------------------------------
// Title:       Main menu
// Description: Runs the auto-boot countdown, then shows the menu until a
//              valid choice is made. In 40 columns cursor left/right switch
//              between the two slot pages.
// Syntax:      char mainmenu(void);
// Input:       None
// Output:      Key of the choice: a function key, or a slot key (also for
//              an auto-boot)
// ---------------------------------------------------------------------------
char mainmenu(void)
{
    char page = 0;
    char key;
    char slot = autobootcountdown();

    if (slot != NO_SLOT)
    {
        return menuslotkey(slot);
    }

    tm_set_screen(TM_SCREEN_MAINMENU);
    menu_draw(page);
    while (true)
    {
        key = key_wait();
        if (!dwin_is80() && (key == KEY_CURSOR_LEFT || key == KEY_CURSOR_RIGHT))
        {
            page ^= 1;
            menu_draw(page);
        }
        else if (menu_validkey(key))
        {
            return key;
        }
    }
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
