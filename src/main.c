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
#include <conio.h>
#include <petscii.h>
#include <c64/reu.h>
#include <c64/vic.h>
#include "defines.h"
#include "dualwin.h"
#include "banking.h"
#include "dmapi.h"
#include "reu128.h"
#include "testmode.h"
#include "overlay1.h"
#include "overlay2.h"

// Resident program region: everything below the overlay load slot
#pragma region(dmboot, RESIDENT_START, OVERLAYLOAD, , , { code, data, bss, heap, stack })

// Menu keys (raw PETSCII from KERNAL GETIN)
#define KEY_OVERLAY1        0x31    // '1'
#define KEY_OVERLAY2        0x32    // '2'
#define KEY_REUTEST_SAFE    0x52    // 'R'
#define KEY_REUTEST_UNSAFE  0x55    // 'U'
#define KEY_EXIT            0x58    // 'X'
#define KEY_POPUP           0x50    // 'P'
#define KEY_INPUT           0x49    // 'I'

// Screen control characters (KERNAL CHROUT)
#define CHR_LOWERCASE       0x0e    // Switch to the lower/upper case charset

// REU round-trip test parameters
#define REUTEST_BLOCK_SIZE  1024
#define REUTEST_ITERATIONS  32
#define REUTEST_REU_BASE    0x10000UL   // Directory heap area, unused in Phase 0
#define REUTEST_SEED        0x5a

// Screen layout of the Phase 0 test screen
#define TITLE_ROW           0
#define STATUS_ROW          2
#define STATUS_HEIGHT       6
#define CONSOLE_ROW         9
#define POPUP_WIDTH         30
#define POPUP_HEIGHT        7
#define INPUT_BUFFER_SIZE   31      // 30 characters plus terminator
#define INPUT_FIELD_WIDTH   20

// Logical colours of the test screen (C64/VIC colour numbers)
#define COLOR_TITLE         VCOL_YELLOW
#define COLOR_TEXT          VCOL_LT_BLUE
#define COLOR_KEY           VCOL_WHITE
#define COLOR_OK            VCOL_LT_GREEN
#define COLOR_ERROR         VCOL_LT_RED

// Global state
struct SystemInfo sysinfo;
struct DMApiInfo dminfo;
char overlay_active = OVERLAY_NONE;

// Overlay stores (index = overlay number - 1). Phase 0 tests one store in
// bank 1 and one in bank 0 RAM under the KERNAL ROM.
static const struct OverlayStore overlay_store[OVERLAY_COUNT] = {
    { BNK_1_FULL, OVERLAY_STORE_BANK1_1, "dmbovl1" },
    { BNK_0_FULL, OVERLAY_STORE_BANK0_1, "dmbovl2" },
};

// Test buffer for the REU round trip (bank 0, resident BSS)
static char reutest_buffer[REUTEST_BLOCK_SIZE];

// Windows of the test screen
static struct DWin screen;
static struct DWin status;
struct DWin console;

// Text edited by the input test
static char input_text[INPUT_BUFFER_SIZE] = "Edit me";

// ---------------------------------------------------------------------------
// Title:       Poll keyboard
// Description: Reads one key from the KERNAL keyboard buffer without
//              waiting and without character conversion.
// Syntax:      char key_poll(void);
// Input:       None
// Output:      Raw PETSCII key code, KEY_NONE when no key is waiting
// ---------------------------------------------------------------------------
char key_poll(void)
{
    return __asm {
        jsr $ffe4
        sta accu
    };
}

// ---------------------------------------------------------------------------
// Title:       Wait for key
// Description: Waits for a key while flagging the program as idle in the
//              test mailbox (the only moment a test harness may access
//              memory), then records the key.
// Syntax:      char key_wait(void);
// Input:       None
// Output:      Raw PETSCII key code
// ---------------------------------------------------------------------------
char key_wait(void)
{
    char key;

    tm_set_idle(1);
    do
    {
        tm_heartbeat();
        key = key_poll();
    } while (key == KEY_NONE);
    tm_set_idle(0);

    tm_set_key(key);
    return key;
}

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

        printf("Loading overlay %u (%s)\n", index + 1, store->name);
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
    if (number < 1 || number > OVERLAY_COUNT || number == overlay_active)
    {
        return;
    }

    const struct OverlayStore *store = &overlay_store[number - 1];
    bnk_memcpy(BNK_0_FULL, (volatile char *)OVERLAYLOAD,
               store->mmucr, (volatile char *)store->address, OVERLAYSIZE);
    overlay_active = number;
}

// ---------------------------------------------------------------------------
// Title:       Fill REU test pattern
// Description: Fills the test buffer with a pattern that differs per
//              iteration.
// Syntax:      void reutest_fill(char iteration);
// Input:       iteration - iteration number, used as pattern seed
// Output:      None
// ---------------------------------------------------------------------------
void reutest_fill(char iteration)
{
    for (unsigned i = 0; i < REUTEST_BLOCK_SIZE; i++)
    {
        reutest_buffer[i] = (char)i ^ iteration ^ REUTEST_SEED;
    }
}

// ---------------------------------------------------------------------------
// Title:       Check REU test pattern
// Description: Verifies the test buffer against the pattern of an
//              iteration.
// Syntax:      bool reutest_check(char iteration);
// Input:       iteration - iteration number used when filling
// Output:      true when every byte matches
// ---------------------------------------------------------------------------
bool reutest_check(char iteration)
{
    for (unsigned i = 0; i < REUTEST_BLOCK_SIZE; i++)
    {
        if (reutest_buffer[i] != (char)((char)i ^ iteration ^ REUTEST_SEED))
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Title:       REU round-trip test
// Description: Stores a pattern block to the REU, clears the buffer, loads
//              it back and verifies it, for a number of iterations. The test
//              runs with the CPU at 2 MHz. The safe variant uses the 1 MHz
//              wrappers; the unsafe variant calls the Oscar64 routines
//              directly at 2 MHz. Afterwards the speed is restored (1 MHz in
//              a TESTMODE build), so a harness may read the mailbox again
//              once the test is done (it must wait, not poll, meanwhile).
// Syntax:      void reutest(char key, bool safe);
// Input:       key  - menu key that started the test (reported in mailbox)
//              safe - true: 1 MHz DMA wrappers, false: DMA at current speed
// Output:      None (result printed and stored in the test mailbox)
// ---------------------------------------------------------------------------
void reutest(char key, bool safe)
{
    unsigned passes = 0;
    unsigned failures = 0;
    bool previous_fast = sysinfo.fast != 0;

    cpu_set_fast(true);
    for (char iteration = 0; iteration < REUTEST_ITERATIONS; iteration++)
    {
        unsigned long raddr = REUTEST_REU_BASE + (unsigned long)iteration * REUTEST_BLOCK_SIZE;

        reutest_fill(iteration);
        if (safe)
        {
            reu128_store(raddr, reutest_buffer, REUTEST_BLOCK_SIZE);
        }
        else
        {
            reu_store(raddr, reutest_buffer, REUTEST_BLOCK_SIZE);
        }

        memset(reutest_buffer, 0, REUTEST_BLOCK_SIZE);

        if (safe)
        {
            reu128_load(raddr, reutest_buffer, REUTEST_BLOCK_SIZE);
        }
        else
        {
            reu_load(raddr, reutest_buffer, REUTEST_BLOCK_SIZE);
        }

        if (reutest_check(iteration))
        {
            passes++;
        }
        else
        {
            failures++;
        }
    }

    cpu_set_fast(previous_fast);

    dwin_printf(&console, failures ? COLOR_ERROR : COLOR_OK, "REU test (%s): %u passed, %u failed\n",
                safe ? "1 MHz DMA" : "DMA at 2 MHz", passes, failures);
    tm_set_test(key, failures ? TM_RESULT_FAIL : TM_RESULT_PASS, passes, failures);
}

// ---------------------------------------------------------------------------
// Title:       Set up the test screen
// Description: Initialises DualWin and draws the fixed parts of the Phase 0
//              test screen: title, status window and console window.
// Syntax:      void screen_setup(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void screen_setup(void)
{
    dwin_setup(BNK_1_FULL, (char *)WINDOW_STORE_BASE, WINDOW_STORE_SIZE);
    dwin_screen_colors(VCOL_BLACK, VCOL_BLACK);

    dwin_init(&screen, 0, 0, 0, 0);
    dwin_clear(&screen);
    dwin_putat_string_reverse(&screen, 0, TITLE_ROW, " DMBoot 128 - Phase 0 skeleton ", COLOR_TITLE);
    dwin_putat_string(&screen, 0, TITLE_ROW + 1, VERSION, COLOR_TEXT);

    dwin_init(&status, 0, STATUS_ROW, 0, STATUS_HEIGHT);
    dwin_init(&console, 0, CONSOLE_ROW, 0, 0);
}

// ---------------------------------------------------------------------------
// Title:       Print status
// Description: Redraws the status window with the detected system state
//              and the test menu keys.
// Syntax:      void print_status(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void print_status(void)
{
    dwin_clear(&status);
    dwin_printf(&status, COLOR_TEXT, "Boot device %u, %u columns, %s\n", sysinfo.bootdevice,
                dwin_is80() ? 80 : 40, sysinfo.fast ? "2 MHz" : "1 MHz");
    dwin_printf(&status, COLOR_TEXT, "REU %u KB, overlay disk loads %u, active overlay %u\n",
                sysinfo.reupages * 64, sysinfo.diskloads, overlay_active);
    if (dminfo.present)
    {
        dwin_printf(&status, COLOR_TEXT, "DM API v%u.%u, hyperspeed ID %u\n",
                    dminfo.version_major, dminfo.version_minor, dminfo.hyperspeed_id);
    }
    else
    {
        dwin_printf(&status, COLOR_ERROR, "DM API not found\n");
    }
    dwin_put_string(&status, "1/2: Overlay  R/U: REU test (1 MHz / 2 MHz DMA)\n", COLOR_KEY);
    dwin_put_string(&status, "P: Popup  I: Input  X: Exit", COLOR_KEY);
}

// ---------------------------------------------------------------------------
// Title:       Popup test
// Description: Opens a popup over the screen, waits for a key and closes
//              it again, which must restore the screen underneath.
// Syntax:      void popup_test(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void popup_test(void)
{
    struct DWin popup;
    char x = (dwin_state.width - POPUP_WIDTH) / 2;

    if (!dwin_popup_open(&popup, x, STATUS_ROW + 1, POPUP_WIDTH, POPUP_HEIGHT, COLOR_TITLE, COLOR_TEXT))
    {
        dwin_put_string(&console, "Popup could not be opened\n", COLOR_ERROR);
        return;
    }

    dwin_putat_string(&popup, 1, 1, "DualWin popup test", COLOR_TITLE);
    dwin_putat_string(&popup, 1, 3, "Press a key to close", COLOR_TEXT);
    key_wait();
    dwin_popup_close();
    dwin_put_string(&console, "Popup closed\n", COLOR_OK);
}

// ---------------------------------------------------------------------------
// Title:       Input test
// Description: Lets the user edit a text in a popup with dwin_input and
//              shows the result in the console.
// Syntax:      void input_test(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void input_test(void)
{
    struct DWin popup;
    char x = (dwin_state.width - POPUP_WIDTH) / 2;

    if (!dwin_popup_open(&popup, x, STATUS_ROW + 1, POPUP_WIDTH, POPUP_HEIGHT, COLOR_TITLE, COLOR_TEXT))
    {
        dwin_put_string(&console, "Popup could not be opened\n", COLOR_ERROR);
        return;
    }

    dwin_putat_string(&popup, 1, 1, "Edit the text:", COLOR_TEXT);
    tm_set_idle(1);
    int result = dwin_input(&popup, 1, 3, input_text, sizeof(input_text), INPUT_FIELD_WIDTH, COLOR_KEY);
    tm_set_idle(0);
    dwin_popup_close();

    if (result == DWIN_INPUT_CANCEL)
    {
        dwin_put_string(&console, "Input cancelled\n", COLOR_ERROR);
    }
    else
    {
        dwin_put_string(&console, "Input: ", COLOR_OK);
        dwin_put_string(&console, input_text, COLOR_KEY);
        dwin_put_char(&console, '\n', COLOR_OK);
    }
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
    tm_init();
    tm_set_screen(TM_SCREEN_STARTUP);

    putrch(CHR_LOWERCASE);
    detect_mode();
    printf("DMBoot 128 %s starting\n", VERSION);

    if (!bnk_init())
    {
        tm_message("LMC load failed");
        return false;
    }

    if (!overlays_preload())
    {
        tm_message("Overlay load failed");
        return false;
    }

    sysinfo.reupages = reu128_count_pages();
    if (sysinfo.reupages < REU_MIN_PAGES)
    {
        printf("An REU of at least 128 KB is required\n");
        tm_message("No REU");
        return false;
    }

    dm_query(&dminfo);
    tm_sync();
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Main
// Description: Program entry: start-up, then the Phase 0 test menu loop.
// Syntax:      int main(void);
// Input:       None
// Output:      0 on normal exit, 1 on a fatal start-up error
// ---------------------------------------------------------------------------
int main(void)
{
    char key;

    if (!dmb_startup())
    {
        bnk_exit();
        return 1;
    }

    screen_setup();
    tm_set_screen(TM_SCREEN_MAINMENU);
    tm_message("Ready");
    print_status();

    do
    {
        key = key_wait();
        switch (key)
        {
        case KEY_OVERLAY1:
            loadoverlay(1);
            tm_set_overlay_signature(overlay1_selftest());
            break;

        case KEY_OVERLAY2:
            loadoverlay(2);
            tm_set_overlay_signature(overlay2_selftest());
            break;

        case KEY_REUTEST_SAFE:
            reutest(key, true);
            break;

        case KEY_REUTEST_UNSAFE:
            reutest(key, false);
            break;

        case KEY_POPUP:
            popup_test();
            break;

        case KEY_INPUT:
            input_test();
            break;

        default:
            break;
        }
        tm_sync();
        if (key != KEY_EXIT)
        {
            print_status();
        }
    } while (key != KEY_EXIT);

    tm_set_screen(TM_SCREEN_EXIT);
    bnk_exit();
    return 0;
}
