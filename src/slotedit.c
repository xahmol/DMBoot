/*
DMBoot 128 v5 - Slot editor overlay

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Edit / re-order / delete / default slot ported from editmenuoptions and
its helpers in src/slotmenu.c of my UBoot64-v2 project
(https://github.com/xahmol/UBoot64-v2); adapted: 36 slots in two columns
(80 columns) or two pages (40 columns), auto-boot timeout setting added
(F4), and changes of several actions are all saved (UBoot64 overwrote the
"changes made" flag per action, so an earlier change could stay unsaved).
*/

#include <stdio.h>
#include <string.h>
#include <petscii.h>
#include "defines.h"
#include "dualwin.h"
#include "testmode.h"
#include "core.h"
#include "fileio.h"
#include "reu128.h"
#include "slotlist.h"
#include "slotedit.h"

#pragma overlay(dmbovl2, 3)
#pragma section(codeovl2, 0)
#pragma section(dataovl2, 0)
#pragma section(bssovl2, 0)
#pragma region(ovl2, OVERLAYLOAD, OVERLAY_SLOT_END, , 3, { codeovl2, dataovl2, bssovl2 })

#pragma code(codeovl2)
#pragma data(dataovl2)
#pragma bss(bssovl2)

#define KEY_YES             'y'
#define KEY_YES_SHIFT       'Y'
#define KEY_NO              'n'
#define KEY_NO_SHIFT        'N'
#define TIMEOUT_TEXT_MAX    16      // "Timeout: 10 s" + terminator

// Result of an edit action
#define EDIT_NONE           0x00
#define EDIT_SLOTS          0x01    // Slots changed: save the slots file
#define EDIT_CONFIG         0x02    // Config changed: save the config file

// Slot being moved by the re-order function
static struct SlotStruct moving;

// ---------------------------------------------------------------------------
// Title:       Prompt line
// Description: Clears the legend and prompt rows and prints a prompt in the
//              first of them.
// Syntax:      static void edit_prompt(const char *text);
// Input:       text - prompt (PETSCII)
// Output:      None
// ---------------------------------------------------------------------------
static void edit_prompt(const char *text)
{
    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, text, cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Ask yes or no
// Description: Prints a question in the prompt row and waits for Y or N.
// Syntax:      static bool edit_yesno(const char *question);
// Input:       question - text before " (Y/N)"
// Output:      true for yes
// ---------------------------------------------------------------------------
static bool edit_yesno(const char *question)
{
    char len = dwin_putat_string(&screenwin, 0, SLOTLIST_PROMPT_ROW, question, cfg.colors.text);
    dwin_putat_string(&screenwin, len, SLOTLIST_PROMPT_ROW, " (Y/N)", cfg.colors.text);
    while (true)
    {
        char key = key_wait();
        if (key == KEY_YES || key == KEY_YES_SHIFT)
        {
            return true;
        }
        if (key == KEY_NO || key == KEY_NO_SHIFT || key == KEY_STOP)
        {
            return false;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Timeout text
// Description: Builds "Timeout: <n> s" or "Timeout: off" for the legend.
// Syntax:      static void edit_timeout_text(char *text, char size);
// Input:       text - buffer
//              size - buffer size
// Output:      text
// ---------------------------------------------------------------------------
static void edit_timeout_text(char *text, char size)
{
    char seconds = (cfg.timeoutidx < TIMEOUT_OPTIONS) ? timeoutseconds[cfg.timeoutidx] : 0;

    if (seconds)
    {
        sprintf(text, "Timeout: %u s", seconds);
    }
    else
    {
        strncpy(text, "Timeout: off", size - 1);
        text[size - 1] = 0;
    }
}

// ---------------------------------------------------------------------------
// Title:       Draw the editor screen
// Description: Header, slot list and function key legend.
// Syntax:      static void edit_draw(char page);
// Input:       page - shown page (40 columns)
// Output:      None
// ---------------------------------------------------------------------------
static void edit_draw(char page)
{
    char timeout[TIMEOUT_TEXT_MAX];

    edit_timeout_text(timeout, sizeof(timeout));
    dwin_clear(&screenwin);
    headertext("Edit/re-order/delete", 1);
    slotlist_draw(page);
    if (dwin_is80())
    {
        fkey_hint(0, SLOTLIST_LEGEND_ROW, " F1 ", "Rename");
        fkey_hint(20, SLOTLIST_LEGEND_ROW, " F2 ", "Command");
        fkey_hint(40, SLOTLIST_LEGEND_ROW, " F3 ", "Re-order");
        fkey_hint(60, SLOTLIST_LEGEND_ROW, " F4 ", timeout);
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 1, " F5 ", "Delete");
        fkey_hint(20, SLOTLIST_LEGEND_ROW + 1, " F6 ", "Default slot");
        fkey_hint(40, SLOTLIST_LEGEND_ROW + 1, " F7 ", "Back");
    }
    else
    {
        fkey_hint(0, SLOTLIST_LEGEND_ROW, " F1 ", "Name");
        fkey_hint(13, SLOTLIST_LEGEND_ROW, " F2 ", "Cmd");
        fkey_hint(24, SLOTLIST_LEGEND_ROW, " F3 ", "Order");
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 1, " F4 ", timeout);
        fkey_hint(24, SLOTLIST_LEGEND_ROW + 1, " F5 ", "Delete");
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 2, " F6 ", "Default");
        fkey_hint(13, SLOTLIST_LEGEND_ROW + 2, " F7 ", "Back");
        fkey_hint(24, SLOTLIST_LEGEND_ROW + 2, " <> ", page ? "Pg 2" : "Pg 1");
    }
}

// ---------------------------------------------------------------------------
// Title:       Rename a slot
// Description: Asks for a slot and a new name. STOP in the input keeps the
//              old name.
// Syntax:      static char edit_rename(char *page);
// Input:       page - shown page (40 columns)
// Output:      EDIT_SLOTS when renamed, else EDIT_NONE
// ---------------------------------------------------------------------------
static char edit_rename(char *page)
{
    char name[MAXMENUNAME];
    char slot;

    edit_prompt("Rename which slot? (F7 = cancel)");
    slot = slotlist_pick(page, false);
    if (slot == NO_SLOT)
    {
        return EDIT_NONE;
    }

    get_slot_from_reu(slot);
    strncpy(name, Slot.menu, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    edit_prompt("New name (STOP = cancel):");
    if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, name, sizeof(name),
                   MAXMENUNAME - 1, cfg.colors.text_input) <= 0)
    {
        return EDIT_NONE;
    }
    strncpy(Slot.menu, name, sizeof(Slot.menu) - 1);
    Slot.menu[sizeof(Slot.menu) - 1] = 0;
    save_slot_to_reu(slot);
    return EDIT_SLOTS;
}

// ---------------------------------------------------------------------------
// Title:       Edit the user command of a slot
// Description: Asks for a slot and its command. An empty slot becomes a
//              command-only slot: its name is asked first. An empty
//              command removes the command.
// Syntax:      static char edit_command(char *page);
// Input:       page - shown page (40 columns)
// Output:      EDIT_SLOTS when changed, else EDIT_NONE
// ---------------------------------------------------------------------------
static char edit_command(char *page)
{
    char text[MAXCOMMAND];
    char slot;
    char width = dwin_is80() ? MAXCOMMAND - 1 : SLOTLIST_COLUMN - 1;

    edit_prompt("Command for which slot? (F7 = cancel)");
    slot = slotlist_pick(page, true);
    if (slot == NO_SLOT)
    {
        return EDIT_NONE;
    }

    get_slot_from_reu(slot);
    if (!Slot.menu[0])
    {
        // New command-only slot
        memset(&Slot, 0, sizeof(Slot));
        text[0] = 0;
        edit_prompt("Name of the new slot (STOP = cancel):");
        if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, text, MAXMENUNAME,
                       MAXMENUNAME - 1, cfg.colors.text_input) <= 0)
        {
            return EDIT_NONE;
        }
        strncpy(Slot.menu, text, sizeof(Slot.menu) - 1);
        Slot.menu[sizeof(Slot.menu) - 1] = 0;
        Slot.cfgvs = CFGVERSION;
        Slot.device = sysinfo.bootdevice;
    }

    strncpy(text, Slot.cmd, sizeof(text) - 1);
    text[sizeof(text) - 1] = 0;
    edit_prompt("Command (empty = none, STOP = cancel):");
    if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, text, sizeof(text),
                   width, cfg.colors.text_input) == DWIN_INPUT_CANCEL)
    {
        return EDIT_NONE;
    }
    strncpy(Slot.cmd, text, sizeof(Slot.cmd) - 1);
    Slot.cmd[sizeof(Slot.cmd) - 1] = 0;
    if (Slot.cmd[0])
    {
        Slot.command |= COMMAND_CMD;
    }
    else
    {
        Slot.command &= ~COMMAND_CMD;
    }
    save_slot_to_reu(slot);
    return EDIT_SLOTS;
}

// ---------------------------------------------------------------------------
// Title:       Delete a slot
// Description: Asks for a slot and a confirmation, then clears it.
// Syntax:      static char edit_delete(char *page);
// Input:       page - shown page (40 columns)
// Output:      EDIT_SLOTS when deleted, else EDIT_NONE
// ---------------------------------------------------------------------------
static char edit_delete(char *page)
{
    char slot;

    edit_prompt("Delete which slot? (F7 = cancel)");
    slot = slotlist_pick(page, false);
    if (slot == NO_SLOT)
    {
        return EDIT_NONE;
    }

    slotlist_draw_slot(slot, *page, SLOTLIST_SELECTED);
    if (!edit_yesno("Delete the selected slot?"))
    {
        return EDIT_NONE;
    }
    memset(&Slot, 0, sizeof(Slot));
    save_slot_to_reu(slot);
    return EDIT_SLOTS;
}

// ---------------------------------------------------------------------------
// Title:       Set or clear the default slot
// Description: The default slot is started by the auto-boot countdown.
//              Only one slot can be default; picking the default slot
//              again clears it.
// Syntax:      static char edit_default(char *page);
// Input:       page - shown page (40 columns)
// Output:      EDIT_SLOTS when changed, else EDIT_NONE
// ---------------------------------------------------------------------------
static char edit_default(char *page)
{
    char slot;
    char wasdefault;

    edit_prompt("Default slot? Pick [D] again to clear.");
    slot = slotlist_pick(page, false);
    if (slot == NO_SLOT)
    {
        return EDIT_NONE;
    }

    get_slot_from_reu(slot);
    wasdefault = Slot.isdefault;
    for (char x = 0; x < SLOTS; x++)
    {
        if (x == slot)
        {
            continue;
        }
        get_slot_from_reu(x);
        if (Slot.isdefault)
        {
            Slot.isdefault = 0;
            save_slot_to_reu(x);
        }
    }
    get_slot_from_reu(slot);
    Slot.isdefault = wasdefault ? 0 : 1;
    save_slot_to_reu(slot);
    return EDIT_SLOTS;
}

// ---------------------------------------------------------------------------
// Title:       Next auto-boot timeout
// Description: Steps the auto-boot timeout to the next option (off, 1, 3,
//              5, 10 seconds, then off again).
// Syntax:      static char edit_timeout(void);
// Input:       None
// Output:      EDIT_CONFIG
// ---------------------------------------------------------------------------
static char edit_timeout(void)
{
    cfg.timeoutidx = (cfg.timeoutidx + 1 < TIMEOUT_OPTIONS) ? cfg.timeoutidx + 1 : 0;
    return EDIT_CONFIG;
}

// ---------------------------------------------------------------------------
// Title:       Move the slot being re-ordered one place
// Description: Swaps the moving slot with its neighbour in the REU and
//              redraws both lines (the whole page when the move crosses
//              a 40-column page). At the top or bottom it wraps around:
//              all other slots shift one place.
// Syntax:      static char edit_move(char pos, bool down, char *page);
// Input:       pos  - current position of the moving slot
//              down - true to move down, false to move up
//              page - shown page (40 columns)
// Output:      New position
// ---------------------------------------------------------------------------
static char edit_move(char pos, bool down, char *page)
{
    char last = SLOTS - 1;
    char newpos;
    bool wrapped = (down && pos == last) || (!down && pos == 0);

    if (wrapped && down)
    {
        for (char x = last; x > 0; x--)
        {
            get_slot_from_reu(x - 1);
            save_slot_to_reu(x);
        }
        newpos = 0;
    }
    else if (wrapped)
    {
        for (char x = 0; x < last; x++)
        {
            get_slot_from_reu(x + 1);
            save_slot_to_reu(x);
        }
        newpos = last;
    }
    else
    {
        newpos = down ? pos + 1 : pos - 1;
        get_slot_from_reu(newpos);
        save_slot_to_reu(pos);
    }
    memcpy(&Slot, &moving, sizeof(Slot));
    save_slot_to_reu(newpos);

    // A wrap moves every slot; a page change shows another page
    if (wrapped || slotlist_page(newpos) != *page)
    {
        *page = slotlist_page(newpos);
        slotlist_draw(*page);
    }
    else
    {
        slotlist_draw_slot(pos, *page, 0);
    }
    slotlist_draw_slot(newpos, *page, SLOTLIST_SELECTED);
    return newpos;
}

// ---------------------------------------------------------------------------
// Title:       Copy all slots within the REU
// Description: Copies the 36 slots between the slot area and the backup
//              area in the REU, one slot at a time through Slot.
// Syntax:      static void edit_copy_slots(unsigned long from,
//                                          unsigned long to);
// Input:       from - REU source address
//              to   - REU destination address
// Output:      None (Slot is overwritten)
// ---------------------------------------------------------------------------
static void edit_copy_slots(unsigned long from, unsigned long to)
{
    for (char x = 0; x < SLOTS; x++)
    {
        unsigned long offset = (unsigned long)x * SLOTSIZE;
        reu128_load(from + offset, (volatile char *)&Slot, SLOTSIZE);
        reu128_store(to + offset, (volatile char *)&Slot, SLOTSIZE);
    }
}

// ---------------------------------------------------------------------------
// Title:       Re-order a slot
// Description: Asks for a slot, then moves it with cursor up/down.
//              RETURN keeps the new order; F7 or STOP restores the order
//              from a backup in the REU.
// Syntax:      static char edit_reorder(char *page);
// Input:       page - shown page (40 columns)
// Output:      EDIT_SLOTS when the order changed, else EDIT_NONE
// ---------------------------------------------------------------------------
static char edit_reorder(char *page)
{
    char pos;
    char start;

    edit_prompt("Re-order which slot? (F7 = cancel)");
    start = slotlist_pick(page, false);
    if (start == NO_SLOT)
    {
        return EDIT_NONE;
    }

    // Back up the slots in the REU: a cancel restores them from there, so
    // earlier unsaved edits of this session are kept
    edit_copy_slots(SLOT_REU_START, SLOT_REU_BACKUP);
    get_slot_from_reu(start);
    memcpy(&moving, &Slot, sizeof(moving));
    slotlist_draw_slot(start, *page, SLOTLIST_SELECTED);
    edit_prompt("Cursor up/down: move. RETURN: keep.");
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, "F7: cancel.", cfg.colors.text);

    pos = start;
    while (true)
    {
        char key = key_wait();
        if (key == KEY_CURSOR_DOWN || key == KEY_CURSOR_UP)
        {
            pos = edit_move(pos, key == KEY_CURSOR_DOWN, page);
        }
        else if (key == KEY_RETURN)
        {
            return (pos != start) ? EDIT_SLOTS : EDIT_NONE;
        }
        else if (key == KEY_F7 || key == KEY_STOP)
        {
            if (pos != start)
            {
                edit_copy_slots(SLOT_REU_BACKUP, SLOT_REU_START);
            }
            return EDIT_NONE;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Slot editor
// Description: Menu for renaming, command, re-order, delete, default slot
//              and auto-boot timeout. On leaving, changed slots and config
//              are written to their files.
// Syntax:      void slotedit(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void slotedit(void)
{
    char changes = EDIT_NONE;
    char page = 0;

    while (true)
    {
        char key;

        edit_draw(page);
        key = key_wait();
        switch (key)
        {
        case KEY_F1:
            changes |= edit_rename(&page);
            break;
        case KEY_F2:
            changes |= edit_command(&page);
            break;
        case KEY_F3:
            changes |= edit_reorder(&page);
            break;
        case KEY_F4:
            changes |= edit_timeout();
            break;
        case KEY_F5:
            changes |= edit_delete(&page);
            break;
        case KEY_F6:
            changes |= edit_default(&page);
            break;
        case KEY_CURSOR_LEFT:
        case KEY_CURSOR_RIGHT:
            if (!dwin_is80())
            {
                page ^= 1;
            }
            break;
        case KEY_F7:
        case KEY_STOP:
            if (changes & EDIT_SLOTS)
            {
                edit_prompt("Saving the slots, please wait.");
                write_slotsfile();
            }
            if (changes & EDIT_CONFIG)
            {
                edit_prompt("Saving the configuration.");
                writeconfigfile();
            }
            return;
        default:
            break;
        }
    }
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
