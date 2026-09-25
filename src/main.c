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
#include "defines.h"
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

// Screen control characters (KERNAL CHROUT)
#define CHR_LOWERCASE       0x0e    // Switch to the lower/upper case charset

// REU round-trip test parameters
#define REUTEST_BLOCK_SIZE  1024
#define REUTEST_ITERATIONS  32
#define REUTEST_REU_BASE    0x10000UL   // Directory heap area, unused in Phase 0
#define REUTEST_SEED        0x5a

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

    printf("REU test (%s): %u passed, %u failed\n", safe ? "1 MHz DMA" : "DMA at 2 MHz", passes, failures);
    tm_set_test(key, failures ? TM_RESULT_FAIL : TM_RESULT_PASS, passes, failures);
}

// ---------------------------------------------------------------------------
// Title:       Print status
// Description: Prints the detected system state and the test menu.
// Syntax:      void print_status(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void print_status(void)
{
    printf("\nDMBoot 128 %s - Phase 0 skeleton\n", VERSION);
    printf("Boot device %u, %u columns, %s\n", sysinfo.bootdevice,
           sysinfo.mode80 ? 80 : 40, sysinfo.fast ? "2 MHz" : "1 MHz");
    printf("REU %u KB, overlay disk loads %u, active overlay %u\n",
           sysinfo.reupages * 64, sysinfo.diskloads, overlay_active);
    if (dminfo.present)
    {
        printf("DM API v%u.%u, hyperspeed ID %u\n",
               dminfo.version_major, dminfo.version_minor, dminfo.hyperspeed_id);
    }
    else
    {
        printf("DM API not found\n");
    }
    printf("1/2: Overlay  R: REU test  U: REU test with 2 MHz DMA  X: Exit\n");
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
