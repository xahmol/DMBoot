/*
DMBoot 128 v5 - Test mailbox for hardware testing via c64bridge

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

In a TESTMODE build (make test-build) DMBoot keeps a fixed-layout status
block ("mailbox") at $0B00 that a test harness reads over the Ultimate REST
API (c64bridge memory read). Keys are injected through the KERNAL keyboard
buffer ($034A, count $D0). See docs/REBUILD_PLAN.md §7.1.

SAFETY: REST memory access stops the CPU with DMA, which crashes the C128
when it runs at 2 MHz (confirmed on hardware 2026-09-25). A TESTMODE build
therefore stays at 1 MHz, except inside tests that explicitly switch to
2 MHz; after injecting such a test key the harness must WAIT (not poll) for
the test's maximum duration before reading again. Otherwise the harness may
only read or write while mailbox.idle == 1 (DMBoot waiting for a key).

In release builds all tm_* calls compile to nothing.
*/

#ifndef TESTMODE_H
#define TESTMODE_H

#include "defines.h"

#define TM_MAILBOX_ADDRESS  0x0b00
#define TM_MAILBOX_MAXSIZE  256
#define TM_MAGIC_SIZE       4
#define TM_LAYOUT_VERSION   1
#define TM_MESSAGE_SIZE     40

// Screen identifiers reported in mailbox.screen
#define TM_SCREEN_STARTUP   0x01
#define TM_SCREEN_MAINMENU  0x02
#define TM_SCREEN_EXIT      0xff

// Test result codes
#define TM_RESULT_NONE      0x00
#define TM_RESULT_PASS      0x01
#define TM_RESULT_FAIL      0x02

// Mailbox layout (little endian words). Keep TM_LAYOUT_VERSION in sync
// with tests/ when fields change.
struct TestMailbox
{
    char magic[TM_MAGIC_SIZE];  // "DMB5" (ASCII)
    char layout_version;        // TM_LAYOUT_VERSION
    char idle;                  // 1 = waiting for a key: REST access allowed
    char heartbeat;             // Incremented while idle
    char screen;                // TM_SCREEN_* of the active screen
    char overlay_active;        // Currently loaded overlay (0 = none)
    char mode80;                // 1 = 80 column mode
    char fast;                  // 1 = 2 MHz
    char bootdevice;            // Device DMBoot was loaded from
    unsigned reupages;          // Detected REU size in 64 KB pages
    unsigned diskloads;         // Overlay files loaded from disk so far
    char dm_present;            // Device Manager API present
    char dm_version_major;
    char dm_version_minor;
    char dm_hsid;               // Hyperspeed drive ID
    char lastkey;               // Last key handled by the main loop
    char test_id;               // Key of the last executed test
    char test_result;           // TM_RESULT_*
    unsigned test_passes;       // Passed iterations of the last test
    unsigned test_failures;     // Failed iterations of the last test
    char overlay_signature;     // Value returned by the last overlay call
    char message[TM_MESSAGE_SIZE]; // Last status message (PETSCII, 0-terminated)
};

// Compile-time check that the mailbox fits in its page
typedef char tm_mailbox_size_check[(sizeof(struct TestMailbox) <= TM_MAILBOX_MAXSIZE) ? 1 : -1];

#ifdef TESTMODE

#define tm_mailbox (*(struct TestMailbox *)TM_MAILBOX_ADDRESS)

void tm_init(void);
void tm_sync(void);
void tm_set_screen(char screen);
void tm_set_idle(char idle);
void tm_heartbeat(void);
void tm_set_key(char key);
void tm_set_test(char test_id, char result, unsigned passes, unsigned failures);
void tm_set_overlay_signature(char signature);
void tm_message(const char *text);

#pragma compile("testmode.c")

#else

// Release build: no mailbox. The macros still evaluate their arguments, so
// a call passed as an argument (for example an overlay function whose
// result is recorded) is never compiled away together with the macro.
#define tm_init()                                       ((void)0)
#define tm_sync()                                       ((void)0)
#define tm_set_screen(screen)                           ((void)(screen))
#define tm_set_idle(idle)                               ((void)(idle))
#define tm_heartbeat()                                  ((void)0)
#define tm_set_key(key)                                 ((void)(key))
#define tm_set_test(test_id, result, passes, failures)  ((void)(test_id), (void)(result), (void)(passes), (void)(failures))
#define tm_set_overlay_signature(signature)             ((void)(signature))
#define tm_message(text)                                ((void)(text))

#endif // TESTMODE

#endif // TESTMODE_H
