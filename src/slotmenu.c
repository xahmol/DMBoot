/*
DMBoot 128 v5 - Main menu overlay

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Menu and auto-boot countdown ported from src/slotmenu.c of my UBoot64-v2
project (https://github.com/xahmol/UBoot64-v2); adapted: DMBoot F-key
layout, the countdown returns the slot key instead of starting it
(overlays cannot call each other). The slot list itself is in slotlist.c.
*/

#include <string.h>
#include <petscii.h>
#include <c64/cia.h>
#include "defines.h"
#include "dualwin.h"
#include "testmode.h"
#include "core.h"
#include "fileio.h"
#include "slotlist.h"
#include "slotmenu.h"

#pragma overlay(dmbovl1, 2)
#pragma section(codeovl1, 0)
#pragma section(dataovl1, 0)
#pragma section(bssovl1, 0)
#pragma region(ovl1, OVERLAYLOAD, OVERLAY_SLOT_END, , 2, { codeovl1, dataovl1, bssovl1 })

#pragma code(codeovl1)
#pragma data(dataovl1)
#pragma bss(bssovl1)

#define TIMEOUT_TEXT_X      13      // Column of the countdown seconds

#define PAGE_X_40           13
#define SWAP_X_40           24      // F8 "80 col" after the page item
#define SWAP_X_80           60      // F8 "Go 40 columns" after F7
#define PAGE_ROW_40         (SLOTLIST_LEGEND_ROW + 2)

// ---------------------------------------------------------------------------
// Title:       Draw the page legend item
// Description: Redraws only the page item (40 columns).
// Syntax:      static void menu_legend_page(char page);
// Input:       page - shown page
// Output:      None
// ---------------------------------------------------------------------------
static void menu_legend_page(char page)
{
    fkey_hint(PAGE_X_40, PAGE_ROW_40, " <> ", page ? "Page 2/2" : "Page 1/2");
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

    slotlist_draw(page);
    if (dwin_is80())
    {
        fkey_hint(0, SLOTLIST_LEGEND_ROW, " F1 ", "Filebrowser");
        fkey_hint(20, SLOTLIST_LEGEND_ROW, " F2 ", "Information");
        fkey_hint(40, SLOTLIST_LEGEND_ROW, " F3 ", "Edit/order/del");
        fkey_hint(60, SLOTLIST_LEGEND_ROW, " F4 ", "Configuration");
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 1, " F5 ", "Go 64");
        fkey_hint(20, SLOTLIST_LEGEND_ROW + 1, " F6 ", "GEOS RAM boot");
        fkey_hint(40, SLOTLIST_LEGEND_ROW + 1, " F7 ", "Quit to BASIC");
        fkey_hint(SWAP_X_80, SLOTLIST_LEGEND_ROW + 1, " F8 ", "Go 40 columns");
    }
    else
    {
        fkey_hint(0, SLOTLIST_LEGEND_ROW, " F1 ", "Browse");
        fkey_hint(13, SLOTLIST_LEGEND_ROW, " F2 ", "Info");
        fkey_hint(24, SLOTLIST_LEGEND_ROW, " F3 ", "Edit");
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 1, " F4 ", "Config");
        fkey_hint(13, SLOTLIST_LEGEND_ROW + 1, " F5 ", "Go 64");
        fkey_hint(24, SLOTLIST_LEGEND_ROW + 1, " F6 ", "GEOS");
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 2, " F7 ", "Quit");
        menu_legend_page(page);
        fkey_hint(SWAP_X_40, PAGE_ROW_40, " F8 ", "80 col");
    }
    dwin_putat_string(&screenwin, 0, SLOTLIST_PROMPT_ROW, "Make your choice.", cfg.colors.text);
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
    dwin_putat_string(&screenwin, 0, SLOTLIST_FIRST_ROW, "Default boot slot:", cfg.colors.text);
    dwin_putat_string(&screenwin, 0, SLOTLIST_FIRST_ROW + 1, Slot.menu, cfg.colors.key);
    dwin_putat_string(&screenwin, 0, SLOTLIST_FIRST_ROW + 3, "Auto-boot in    sec.", cfg.colors.text);
    dwin_putat_string(&screenwin, 0, SLOTLIST_FIRST_ROW + 4, "Press any key to open the menu.", cfg.colors.text);

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
            dwin_putat_string(&screenwin, TIMEOUT_TEXT_X, SLOTLIST_FIRST_ROW + 3, text, cfg.colors.text);
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
//              between the two slot pages. F8 switches to the other screen
//              (40/80 columns) and redraws the menu there.
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
            slotlist_draw(page);
            menu_legend_page(page);
        }
        else if (key == KEY_F8)
        {
            screen_swap();
            page = 0;
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
