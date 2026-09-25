/*
DMBoot 128 v5 - Test mailbox for hardware testing via c64bridge

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <string.h>
#include "testmode.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

// Magic bytes as raw ASCII values, independent of any charmap
static const char tm_magic[TM_MAGIC_SIZE] = { 0x44, 0x4d, 0x42, 0x35 }; // "DMB5"

// ---------------------------------------------------------------------------
// Title:       Initialise test mailbox
// Description: Clears the mailbox page and writes the magic bytes and the
//              layout version, so a harness can recognise a TESTMODE build.
// Syntax:      void tm_init(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void tm_init(void)
{
    memset((char *)TM_MAILBOX_ADDRESS, 0, TM_MAILBOX_MAXSIZE);
    memcpy(tm_mailbox.magic, tm_magic, TM_MAGIC_SIZE);
    tm_mailbox.layout_version = TM_LAYOUT_VERSION;
}

// ---------------------------------------------------------------------------
// Title:       Synchronise mailbox with system state
// Description: Copies the detected system and Device Manager information
//              and the active overlay into the mailbox.
// Syntax:      void tm_sync(void);
// Input:       None (reads sysinfo, dminfo, overlay_active)
// Output:      None
// ---------------------------------------------------------------------------
void tm_sync(void)
{
    tm_mailbox.overlay_active = overlay_active;
    tm_mailbox.mode80 = sysinfo.mode80;
    tm_mailbox.fast = sysinfo.fast;
    tm_mailbox.bootdevice = sysinfo.bootdevice;
    tm_mailbox.reupages = sysinfo.reupages;
    tm_mailbox.diskloads = sysinfo.diskloads;
    tm_mailbox.dm_present = dminfo.present;
    tm_mailbox.dm_version_major = dminfo.version_major;
    tm_mailbox.dm_version_minor = dminfo.version_minor;
    tm_mailbox.dm_hsid = dminfo.hyperspeed_id;
}

// ---------------------------------------------------------------------------
// Title:       Set active screen
// Description: Records which screen is active.
// Syntax:      void tm_set_screen(char screen);
// Input:       screen - TM_SCREEN_* identifier
// Output:      None
// ---------------------------------------------------------------------------
void tm_set_screen(char screen)
{
    tm_mailbox.screen = screen;
}

// ---------------------------------------------------------------------------
// Title:       Set idle flag
// Description: Marks whether DMBoot is idle (waiting for a key). The harness
//              may only access memory while this flag is 1.
// Syntax:      void tm_set_idle(char idle);
// Input:       idle - 1 when idle, 0 when busy
// Output:      None
// ---------------------------------------------------------------------------
void tm_set_idle(char idle)
{
    tm_mailbox.idle = idle;
}

// ---------------------------------------------------------------------------
// Title:       Heartbeat
// Description: Increments the heartbeat counter; called from idle loops so
//              the harness can see the program is alive.
// Syntax:      void tm_heartbeat(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void tm_heartbeat(void)
{
    tm_mailbox.heartbeat++;
}

// ---------------------------------------------------------------------------
// Title:       Record last key
// Description: Stores the last key handled by a menu loop.
// Syntax:      void tm_set_key(char key);
// Input:       key - raw PETSCII key code
// Output:      None
// ---------------------------------------------------------------------------
void tm_set_key(char key)
{
    tm_mailbox.lastkey = key;
}

// ---------------------------------------------------------------------------
// Title:       Record test result
// Description: Stores the outcome of a built-in hardware test.
// Syntax:      void tm_set_test(char test_id, char result, unsigned passes,
//                               unsigned failures);
// Input:       test_id  - key that started the test
//              result   - TM_RESULT_PASS or TM_RESULT_FAIL
//              passes   - number of passed iterations
//              failures - number of failed iterations
// Output:      None
// ---------------------------------------------------------------------------
void tm_set_test(char test_id, char result, unsigned passes, unsigned failures)
{
    tm_mailbox.test_id = test_id;
    tm_mailbox.test_result = result;
    tm_mailbox.test_passes = passes;
    tm_mailbox.test_failures = failures;
}

// ---------------------------------------------------------------------------
// Title:       Record overlay signature
// Description: Stores the value returned by the last overlay function call,
//              proving which overlay code actually ran.
// Syntax:      void tm_set_overlay_signature(char signature);
// Input:       signature - value returned by the overlay
// Output:      None
// ---------------------------------------------------------------------------
void tm_set_overlay_signature(char signature)
{
    tm_mailbox.overlay_signature = signature;
}

// ---------------------------------------------------------------------------
// Title:       Record status message
// Description: Copies a status message into the mailbox, truncated to fit.
// Syntax:      void tm_message(const char *text);
// Input:       text - 0-terminated message
// Output:      None
// ---------------------------------------------------------------------------
void tm_message(const char *text)
{
    strncpy(tm_mailbox.message, text, TM_MESSAGE_SIZE - 1);
    tm_mailbox.message[TM_MESSAGE_SIZE - 1] = 0;
}
