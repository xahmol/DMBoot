/*
DMBoot 128 v5
Device Manager Boot Menu for the Commodore 128

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
https://www.idreamtin8bits.com/

Code and resources from others used:
-   Oscar64 cross compiler
    https://github.com/drmortalwombat/oscar64
-   Ultimate 64/II+ Command Library, Scott Hutter, Francesco Sblendorio
    https://github.com/xlar54/ultimateii-dos-lib
-   C128 Device Manager ROM and its extended API, Bart van Leeuwen
    https://www.bartsplace.net/content/publications/devicemanager128.shtml
-   Ultimate II+ cartridge, Gideon Zweijtzer
    https://ultimate64.com/

The code can be used freely as long as you retain a notice describing
original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!

Phase 0 skeleton: proves the memory model on real hardware (autostart via
the Device Manager ROM, low-memory code, overlays stored in bank 1 and in
bank 0 under ROM, REU DMA at 2 MHz, Device Manager API, test mailbox).
*/

#include <stdio.h>
#include <string.h>
#include <petscii.h>
#include <c64/vic.h>
#include <c64/cia.h>
#include "defines.h"
#include "dualwin.h"
#include "banking.h"
#include "dmapi.h"
#include "reu128.h"
#include "testmode.h"
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "dmpaths.h"
#include "core.h"
#include "fileio.h"
#include "slotmenu.h"
#include "slotedit.h"
#include "browse.h"
#include "config.h"
#include "exec.h"

// Resident program region: everything below the overlay load slot
#pragma region(dmboot, RESIDENT_START, OVERLAYLOAD, , , { code, data, bss, heap, stack })

// Screen layout (rows 0-1: header)
#define STARTUP_ROW         3       // Start-up messages
#define POPUP_WIDTH         36
#define POPUP_HEIGHT        5
#define POPUP_ROW           10
#define STORAGE_RETRY_SECS  5       // USB may still be enumerating at cold boot
#define UCI_TIMEOUT_SECS    10
#define TEXT_LINE_MAX       81

// Global state
struct SystemInfo sysinfo;
struct SlotStruct Slot;
struct ConfigStruct cfg;
struct DMApiInfo dminfo;
struct BrowseRequest browsereq;
char overlay_active = OVERLAY_NONE;

// Overlay stores (index = overlay number - 1), see docs/REBUILD_PLAN.md §4.
// An empty name marks an overlay of a later phase.
static const struct OverlayStore overlay_store[OVERLAY_COUNT] = {
    { BNK_1_FULL, OVERLAY_STORE_BANK1_1, "dmbovl1", "main menu" },
    { BNK_1_FULL, OVERLAY_STORE_BANK1_2, "dmbovl2", "slot editing" },
    { BNK_1_FULL, OVERLAY_STORE_BANK1_3, "dmbovl3", "file browser" },
    { BNK_1_FULL, OVERLAY_STORE_BANK1_4, "dmbovl4", "configuration" },
    { BNK_0_FULL, OVERLAY_STORE_BANK0_1, "dmbovl5", "slot start" },
};

// Windows
struct DWin screenwin;
struct DWin console;

// ---------------------------------------------------------------------------
// Title:       Set CPU speed
// Description: Switches the C128 between 1 MHz and 2 MHz and records the
//              state in sysinfo.
// Syntax:      void cpu_set_fast(bool fast);
// Input:       fast - true for 2 MHz, false for 1 MHz
// Output:      sysinfo.fast
// ---------------------------------------------------------------------------
void cpu_set_fast(bool fast)
{
    volatile char *clock = (volatile char *)VIC_CLOCK_REG;

    if (fast)
    {
        *clock |= VIC_CLOCK_FAST;
    }
    else
    {
        *clock &= ~VIC_CLOCK_FAST;
    }
    sysinfo.fast = fast ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Title:       Detect screen mode and speed
// Description: Detects 40 or 80 column mode. In 80 column mode the release
//              build runs at 2 MHz (the VIC screen is not used there). A
//              TESTMODE build stays at 1 MHz, because the Ultimate's REST
//              memory access uses DMA, which crashes the C128 at 2 MHz.
// Syntax:      void detect_mode(void);
// Input:       None
// Output:      sysinfo.mode80, sysinfo.fast
// ---------------------------------------------------------------------------
void detect_mode(void)
{
    sysinfo.mode80 = (*(volatile char *)ZP_MODE_80COL & MODE_80COL_FLAG) ? 1 : 0;
#ifdef TESTMODE
    cpu_set_fast(false);
#else
    cpu_set_fast(sysinfo.mode80 != 0);
#endif
}

// ---------------------------------------------------------------------------
// Title:       Preload overlays
// Description: Loads every overlay file once from disk into the overlay
//              load slot and copies it to its store in bank 1 or in bank 0
//              under ROM, so later overlay switches need no disk access.
// Syntax:      bool overlays_preload(void);
// Input:       None
// Output:      true when all overlays were loaded, false otherwise
// ---------------------------------------------------------------------------
bool overlays_preload(void)
{
    for (char index = 0; index < OVERLAY_COUNT; index++)
    {
        const struct OverlayStore *store = &overlay_store[index];

        // Overlays of later phases are not there yet
        if (!store->name[0])
        {
            continue;
        }

        if (cfg.verbose)
        {
            dwin_printf(&console, cfg.colors.text, "Loading overlay %u: %s\n", index + 1, store->purpose);
        }
        else
        {
            spinning();
        }
        if (!load_overlay(store->name))
        {
            return false;
        }
        bnk_memcpy(store->mmucr, (volatile char *)store->address,
                   BNK_0_FULL, (volatile char *)OVERLAYLOAD, OVERLAYSIZE);
    }

    // The load slot now holds the last loaded file, not a selected overlay
    overlay_active = OVERLAY_NONE;
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Activate overlay
// Description: Copies an overlay from its store into the load slot, unless
//              it is already the active one.
// Syntax:      void loadoverlay(char number);
// Input:       number - overlay number, 1..OVERLAY_COUNT
// Output:      None (invalid numbers are ignored)
// ---------------------------------------------------------------------------
void loadoverlay(char number)
{
    if (number < 1 || number > OVERLAY_COUNT || number == overlay_active ||
        !overlay_store[number - 1].name[0])
    {
        return;
    }

    const struct OverlayStore *store = &overlay_store[number - 1];
    bnk_memcpy(BNK_0_FULL, (volatile char *)OVERLAYLOAD,
               store->mmucr, (volatile char *)store->address, OVERLAYSIZE);
    overlay_active = number;
    tm_sync();
}

// ---------------------------------------------------------------------------
// Title:       Set up the screen
// Description: Initialises DualWin with the configured colours and draws
//              the header; the console window takes the rest of the screen
//              from a given row.
// Syntax:      void screen_setup(const char *subtitle, char consolerow);
// Input:       subtitle   - header subtitle
//              consolerow - first row of the console window
// Output:      None
// ---------------------------------------------------------------------------
void screen_setup(const char *subtitle, char consolerow)
{
    dwin_screen_colors(cfg.colors.border, cfg.colors.background);
    dwin_init(&screenwin, 0, 0, 0, 0);
    dwin_clear(&screenwin);
    headertext(subtitle, 0);
    dwin_init(&console, 0, consolerow, 0, 0);
}

// ---------------------------------------------------------------------------
// Title:       Switch to the other screen
// Description: Makes the other screen (40 or 80 columns) active for the
//              rest of the session and for BASIC after exit (dwin_exit keeps
//              it). Sets the CPU speed for it (release: 2 MHz in 80
//              columns, 1 MHz in 40), re-initialises the screen and console
//              windows and sets the colours. The caller redraws.
// Syntax:      void screen_swap(void);
// Input:       None
// Output:      None (sysinfo.mode80, dwin_state)
// ---------------------------------------------------------------------------
void screen_swap(void)
{
    dwin_swap_screen();
    sysinfo.mode80 = dwin_is80() ? 1 : 0;
#ifndef TESTMODE
    cpu_set_fast(sysinfo.mode80 != 0);
#endif
    dwin_screen_colors(cfg.colors.border, cfg.colors.background);
    dwin_init(&screenwin, 0, 0, 0, 0);
    dwin_init(&console, 0, STARTUP_ROW, 0, 0);
}

// ---------------------------------------------------------------------------
// Title:       Print an Ultimate string
// Description: Prints a label and an ASCII string from the Ultimate
//              (converted to PETSCII, bounded) as one console line.
// Syntax:      void print_ascii_line(const char *label, const char *ascii);
// Input:       label - PETSCII label
//              ascii - ASCII text from the Ultimate
// Output:      None
// ---------------------------------------------------------------------------
void print_ascii_line(const char *label, const char *ascii)
{
    char text[TEXT_LINE_MAX];

    asc2pet(text, ascii, sizeof(text));
    dwin_put_string(&console, label, cfg.colors.text);
    dwin_put_string(&console, text, cfg.colors.text);
    dwin_put_char(&console, '\n', cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Show the drives
// Description: Prints the Ultimate devices (ID, power, type), whether IDs
//              need manual power switching, and every active IEC ID with
//              its drive type from the Device Manager ROM (verbose mode).
//              Ultimate part as UBoot64-v2 main.c; the Device Manager
//              drive types as DMBoot v4 (getDeviceType).
// Syntax:      void print_devices(void);
// Input:       None (uii_devinfo, filled by uii_parse_deviceinfo; dminfo)
// Output:      None
// ---------------------------------------------------------------------------
void print_devices(void)
{
    static const char *const names[UII_DEVINFO_COUNT] = { "Drive A", "Drive B", "SoftIEC", "Printer" };
    char active[IEC_ID_COUNT];

    dwin_put_string(&console, "Ultimate devices:\n", cfg.colors.text);
    for (char x = 0; x < UII_DEVINFO_COUNT; x++)
    {
        if (uii_devinfo[x].exist)
        {
            dwin_printf(&console, cfg.colors.text, "%s: ID %2u, power %s %s\n", names[x],
                        uii_devinfo[x].id, uii_devinfo[x].power ? "on " : "off",
                        uii_device_type(uii_devinfo[x].type));
        }
    }

    dwin_printf(&console, cfg.colors.text, "IDs needing manual power switching: %s\n",
                iec_scan(active) ? "yes" : "no");
    dwin_put_string(&console, "Active IEC IDs:", cfg.colors.text);
    for (char x = 0; x < IEC_ID_COUNT; x++)
    {
        if (active[x])
        {
            char id = iec_index_to_id(x);
            dwin_printf(&console, cfg.colors.text, " %u (", id);
            if (active[x] == IEC_HYPERSPEED)
            {
                dwin_put_string(&console, "Hyperspeed)", cfg.colors.text);
                continue;
            }
            // Ultimate devices: label from uii_devinfo, because the Device
            // Manager reports only their drive type
            for (char u = 0; u < UII_DEVINFO_COUNT; u++)
            {
                if (uii_devinfo[u].exist && uii_devinfo[u].power && uii_devinfo[u].id == id)
                {
                    dwin_printf(&console, cfg.colors.text, "%s ", names[u]);
                }
            }
            dwin_printf(&console, cfg.colors.text, "%s)",
                        (dminfo.present && id != dminfo.hyperspeed_id) ? dm_drivetype_name(id) : "");
        }
    }
    dwin_put_char(&console, '\n', cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Start-up
// Description: Detects the machine, initialises banking and the LMC,
//              preloads overlays, detects the REU and the Device Manager API.
// Syntax:      bool dmb_startup(void);
// Input:       None
// Output:      true when DMBoot can run, false on a fatal error
// ---------------------------------------------------------------------------
bool dmb_startup(void)
{
    char storage;

    tm_init();
    tm_set_screen(TM_SCREEN_STARTUP);
    detect_mode();
    config_defaults();

    // The low-memory code is needed by DualWin, so load it first
    if (!bnk_init())
    {
        tm_message("LMC load failed");
        return false;
    }

    dwin_setup(BNK_1_FULL, (char *)WINDOW_STORE_BASE, WINDOW_STORE_SIZE);
    screen_setup("Starting...", STARTUP_ROW);

    if (!uii_wait_for_uci(UCI_TIMEOUT_SECS))
    {
        errorexit("No Ultimate Command Interface enabled.\n"
                  "Enable it in the Ultimate menu,\nor update to firmware 3.15 or later.");
    }

    // Find the DMBoot directory; retry while USB storage is enumerating
    cia1.tods = 0;
    cia1.todt = 0;
    do
    {
        storage = resolve_storage_path();
    } while (storage == STORAGE_NONE && cia1.tods < STORAGE_RETRY_SECS);
    if (storage == STORAGE_NONE)
    {
        errorexit("DMBoot directory (11) not found on USB storage.");
    }

    // Read the config before any verbose output, so its verbose and colour
    // settings apply from here on
    readconfigfile();
    screen_setup("Starting...", STARTUP_ROW);

    progress("Ultimate Command Interface detected.");
    if (!cfg.verbose)
    {
        dwin_put_string(&console, STARTUP_SILENT_TEXT, cfg.colors.text);
    }
    else
    {
        print_ascii_line("Storage: ", configpath);
        uii_identify();
        print_ascii_line("Ultimate: ", uii_data);
    }

    drive_root_reset();

    if (!overlays_preload())
    {
        tm_message("Overlay load failed");
        errorexit("Loading the overlay files failed.");
    }

    sysinfo.reupages = reu128_count_pages();
    if (sysinfo.reupages < REU_MIN_PAGES)
    {
        tm_message("No REU");
        errorexit("An REU of at least 128 KB is required.\nEnable the REU in the Ultimate menu.");
    }
    if (cfg.verbose)
    {
        dwin_printf(&console, cfg.colors.text, "REU: %u KB\n", sysinfo.reupages * 64);
    }

    read_slotsfile();

    if (!uii_parse_deviceinfo())
    {
        errorexit("Reading the Ultimate drive info failed.");
    }

    dm_query(&dminfo);
    if (cfg.verbose)
    {
        if (dminfo.present)
        {
            dwin_printf(&console, cfg.colors.text, "DM API v%u.%u, hyperspeed ID %u\n",
                        dminfo.version_major, dminfo.version_minor, dminfo.hyperspeed_id);
        }
        else
        {
            dwin_put_string(&console, "Device Manager API not found\n", cfg.colors.error);
        }
        print_devices();
    }


    // Time from an NTP server (overlay 4), after all detection output
    if (cfg.timeon)
    {
        loadoverlay(OVERLAY_CONFIG);
        ntp_update();
    }

    // Keep the start-up messages on screen until a key is pressed
    if (cfg.verbose == VERBOSE_WAIT)
    {
        dwin_put_string(&console, "\nPress a key to continue.", cfg.colors.text);
        key_wait();
    }

    tm_sync();
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Main
// Description: Program entry: start-up, then the main menu loop.
// Syntax:      int main(void);
// Input:       None
// Output:      1 on a fatal start-up error (the menu options exit to BASIC
//              themselves)
// ---------------------------------------------------------------------------
int main(void)
{
    dmb_zp_save();
    dmb_fkeys_raw();

    if (!dmb_startup())
    {
        bnk_exit();
        dmb_exit();
    }

    while (true)
    {
        loadoverlay(OVERLAY_MENU);
        char key = mainmenu();

        if (isslotkey(key))
        {
            loadoverlay(OVERLAY_EXEC);
            runbootfrommenu(keytomenuslot(key));
        }

        switch (key)
        {
        case KEY_F1:
            loadoverlay(OVERLAY_BROWSE);
            browse();
            if (browsereq.action == BROWSE_RUN)
            {
                loadoverlay(OVERLAY_EXEC);
                exec_browse();
            }
            break;
        case KEY_F2:
            loadoverlay(OVERLAY_CONFIG);
            information();
            break;
        case KEY_F3:
            loadoverlay(OVERLAY_EDIT);
            slotedit();
            break;
        case KEY_F4:
            loadoverlay(OVERLAY_CONFIG);
            config_edit();
            break;
        case KEY_F5:
            loadoverlay(OVERLAY_EXEC);
            exec_go64();
            break;
        case KEY_F6:
            loadoverlay(OVERLAY_EXEC);
            exec_geos();
            break;
        case KEY_F7:
            loadoverlay(OVERLAY_EXEC);
            exec_exit_to_basic();
            break;
        default:
            break;
        }
    }
}
