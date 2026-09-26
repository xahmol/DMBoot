/*
DMBoot 128 v5 - Core helpers

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <stdlib.h>
#include <string.h>
#include <petscii.h>
#include <c64/cia.h>
#include <c64/kernalio.h>
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "ultimate_time_lib.h"
#include "banking.h"
#include "dmapi.h"
#include "dualwin.h"
#include "testmode.h"
#include "core.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

#define HEADER_TITLE        "DMBoot 128: Device Manager Boot Menu"
#define HEADER_ROW_TITLE    0
#define HEADER_ROW_SUB      1
#define SPINNER_ROW         3
#define SPINNER_COLUMN      sizeof(STARTUP_SILENT_TEXT)  // Text length + 1 space
#define SPINNER_FRAMES      4
#define CHR_SPACE           0x20
#define TIME_TEXT_MAX       24      // "yyyy/mm/dd hh:mm:ss" plus margin
#define TIME_ONLY_OFFSET    11      // Start of "hh:mm:ss" in that text
#define DWIN_VDC_WIDTH_COLS 80
#define DOS_COMMAND_CHANNEL 15
#define SLOT_KEYS_DIGITS    10
#define PETSCII_ZERO        0x30
#define PETSCII_LETTER_A    0x41    // Unshifted letter keys (a-z)

#define PETSCII_SHIFT       0x80    // Added to a letter for its shifted (uppercase) code

// Spinner animation (PETSCII graphics)
static const char spinner[SPINNER_FRAMES] = { 0xbe, 0xbc, 0xac, 0xbb };
static char spinnerframe;


// ---------------------------------------------------------------------------
// Title:       Exit on error
// Description: Shows an error message, waits for a key, restores the MMU
//              configuration and returns to BASIC.
// Syntax:      void errorexit(const char *message);
// Input:       message - error text (PETSCII); may be empty
// Output:      Does not return
// ---------------------------------------------------------------------------
void errorexit(const char *message)
{
    dwin_put_char(&console, '\n', cfg.colors.error);
    dwin_put_string(&console, message, cfg.colors.error);
    dwin_put_string(&console, "\nPress a key to exit to BASIC.\n", cfg.colors.text);
    dwin_getch();
    dwin_exit();
    bnk_exit();
    dmb_exit();
}

// ---------------------------------------------------------------------------
// Title:       Wait
// Description: Waits a number of seconds using the CIA 1 time-of-day clock.
// Syntax:      void delay(char seconds);
// Input:       seconds - seconds to wait (max. 59)
// Output:      None
// ---------------------------------------------------------------------------
void delay(char seconds)
{
    cia1.tods = 0;
    cia1.todt = 0;
    while (cia1.tods < seconds)
    {
        ;
    }
}

// ---------------------------------------------------------------------------
// Title:       Spinner
// Description: Shows the next frame of the start-up spinner (silent mode),
//              after the STARTUP_SILENT_TEXT line.
// Syntax:      void spinning(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void spinning(void)
{
    // Only in silent mode: with messages on, the spinner's place is taken by text
    if (cfg.verbose)
    {
        return;
    }
    dwin_putat_char(&screenwin, SPINNER_COLUMN, SPINNER_ROW, spinner[spinnerframe], cfg.colors.text);
    spinnerframe = (spinnerframe + 1) % SPINNER_FRAMES;
}

// ---------------------------------------------------------------------------
// Title:       Draw the header
// Description: Draws the two header lines: the program title and a subtitle.
//              At the right of the second line the version, or the time of
//              the Ultimate's real-time clock.
// Syntax:      void headertext(const char *subtitle, char showtime);
// Input:       subtitle - text for the second header line
//              showtime - 1: show the time, 0: show the version
// Output:      None
// ---------------------------------------------------------------------------
void headertext(const char *subtitle, char showtime)
{
    char width = dwin_state.width;
    char righttext[TIME_TEXT_MAX];
    const char *right = VERSION;

    if (showtime)
    {
        uii_get_time();
        if (UII_SUCCESS)
        {
            asc2pet(righttext, uii_data, sizeof(righttext));
            // In 40 columns show only the time part ("hh:mm:ss")
            if (width < DWIN_VDC_WIDTH_COLS && strlen(righttext) > TIME_ONLY_OFFSET)
            {
                right = righttext + TIME_ONLY_OFFSET;
            }
            else
            {
                right = righttext;
            }
        }
    }
    char versionlen = strlen(right);

    // Fill with spaces, write the texts, then reverse both full lines
    // (PETSCII $A0 is not a reverse space after conversion to screen codes)
    dwin_fill_rect(&screenwin, 0, HEADER_ROW_TITLE, width, 1, CHR_SPACE, cfg.colors.header1);
    dwin_fill_rect(&screenwin, 0, HEADER_ROW_SUB, width, 1, CHR_SPACE, cfg.colors.header2);
    dwin_putat_string(&screenwin, 0, HEADER_ROW_TITLE, HEADER_TITLE, cfg.colors.header1);
    dwin_putat_string(&screenwin, 0, HEADER_ROW_SUB, subtitle, cfg.colors.header2);
    if (versionlen < width)
    {
        dwin_putat_string(&screenwin, width - versionlen, HEADER_ROW_SUB, right, cfg.colors.header2);
    }
    dwin_reverse_rect(&screenwin, 0, HEADER_ROW_TITLE, width, 2);
}

// ---------------------------------------------------------------------------
// Title:       Start-up progress
// Description: Reports a start-up step: a line in the console in verbose
//              mode, or the next spinner frame in silent mode.
// Syntax:      void progress(const char *text);
// Input:       text - message (PETSCII)
// Output:      None
// ---------------------------------------------------------------------------
void progress(const char *text)
{
    if (cfg.verbose)
    {
        dwin_put_string(&console, text, cfg.colors.text);
        dwin_put_char(&console, '\n', cfg.colors.text);
    }
    else
    {
        spinning();
    }
}

char DOSstatus[DOS_STATUS_MAX];

// ---------------------------------------------------------------------------
// Title:       Send a DOS command
// Description: Opens a channel with a command (or file name), reads the
//              drive status from the command channel and closes again.
// Syntax:      char dosCommand(char lfn, char device, char secaddr,
//                              const char *command);
// Input:       lfn     - logical file number
//              device  - IEC device number
//              secaddr - secondary address
//              command - DOS command / file name (PETSCII)
// Output:      DOS status number (0 = OK), or a KERNAL error; DOSstatus
//              holds the status text
// ---------------------------------------------------------------------------
char dosCommand(char lfn, char device, char secaddr, const char *command)
{
    int res;

    krnio_setnam(command);
    if (!krnio_open(lfn, device, secaddr))
    {
        krnio_close(lfn);
        return krnio_status();
    }

    if (lfn != DOS_COMMAND_CHANNEL)
    {
        krnio_setnam("");
        if (!krnio_open(DOS_COMMAND_CHANNEL, device, DOS_COMMAND_CHANNEL))
        {
            krnio_close(lfn);
            krnio_close(DOS_COMMAND_CHANNEL);
            return krnio_pstatus[DOS_COMMAND_CHANNEL];
        }
    }

    DOSstatus[0] = 0;
    res = krnio_read(DOS_COMMAND_CHANNEL, DOSstatus, sizeof(DOSstatus) - 1);
    DOSstatus[(res > 0) ? res : 0] = 0;

    if (lfn != DOS_COMMAND_CHANNEL)
    {
        krnio_close(DOS_COMMAND_CHANNEL);
    }
    krnio_close(lfn);

    if (res < 2)
    {
        return krnio_status();
    }
    return (DOSstatus[0] - PETSCII_ZERO) * 10 + DOSstatus[1] - PETSCII_ZERO;
}

// ---------------------------------------------------------------------------
// Title:       Send a command to the command channel
// Description: Sends a DOS command (e.g. "cd//path") to a device.
// Syntax:      char cmd(char device, const char *command);
// Input:       device  - IEC device number
//              command - DOS command (PETSCII)
// Output:      DOS status number (0 = OK)
// ---------------------------------------------------------------------------
char cmd(char device, const char *command)
{
    return dosCommand(DOS_COMMAND_CHANNEL, device, DOS_COMMAND_CHANNEL, command);
}

// v4 drive commands as raw PETSCII bytes, so no charmap can alter them.
// $FF after "cd:" is what DMBoot v4 sent to go to the partition root.
static const char cmd_cp11[]   = {0x43, 0x50, 0x31, 0x31, 0x00};       // "cp11"
static const char cmd_cp0[]    = {0x43, 0x50, 0x30, 0x00};             // "cp0"
static const char cmd_cdroot[] = {0x43, 0x44, 0x3a, 0xff, 0x00};       // "cd:" + $FF

// ---------------------------------------------------------------------------
// Title:       Reset the boot drive directories
// Description: Same sequence as DMBoot v4: sets the root directory of the
//              DMBoot partition (11) and of the main partition (0) of the
//              boot drive, leaving partition 0 as the working partition.
//              Slot paths of v4 assume this starting state.
// Syntax:      void drive_root_reset(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void drive_root_reset(void)
{
    cmd(sysinfo.bootdevice, cmd_cp11);
    cmd(sysinfo.bootdevice, cmd_cdroot);
    cmd(sysinfo.bootdevice, cmd_cp0);
    cmd(sysinfo.bootdevice, cmd_cdroot);
}

// ---------------------------------------------------------------------------
// Title:       IEC scan index to device ID
// Description: Converts an index of the IEC scan array to a device ID.
// Syntax:      char iec_index_to_id(char index);
// Input:       index - 0..IEC_ID_COUNT-1
// Output:      Device ID (8-29, or 4 for the last index)
// ---------------------------------------------------------------------------
char iec_index_to_id(char index)
{
    return (index == IEC_ID_COUNT - 1) ? IEC_ID_PRINTER : IEC_ID_FIRST + index;
}

// ---------------------------------------------------------------------------
// Title:       Is a device present
// Description: Tests whether a device answers on an IEC device ID
//              (LISTEN + secondary address, then the KERNAL status).
// Syntax:      char iec_present(char id);
// Input:       id - IEC device ID
// Output:      1 = device present, 0 = no device
// ---------------------------------------------------------------------------
char iec_present(char id)
{
    __asm
    {
        lda id
        ldy #0
        sty $90
        jsr $ffb1           // LISTEN
        lda #$ff
        jsr $ff93           // SECOND
        lda $90
        bpl iec_pres_active
        jsr $ffae           // UNLSN
        lda #0
        sta accu
        rts
iec_pres_active:
        jsr $ffae           // UNLSN
        lda #1
        sta accu
        rts
    }
}

// ---------------------------------------------------------------------------
// Title:       Ultimate device on an ID
// Description: Looks up an ID in the Ultimate device info.
// Syntax:      static char iec_ultimate_on_id(char id);
// Input:       id - IEC device ID
// Output:      0 = no Ultimate device, else bits: 0 exists, 1 powered,
//              2 power switchable through the UCI (drive A/B)
// ---------------------------------------------------------------------------
static char iec_ultimate_on_id(char id)
{
    for (char x = 0; x < UII_DEVINFO_COUNT; x++)
    {
        if (uii_devinfo[x].exist && uii_devinfo[x].id == id)
        {
            return IEC_ULT_EXISTS |
                   (uii_devinfo[x].power ? IEC_ULT_POWERED : 0) |
                   ((uii_devinfo[x].type < UII_TYPE_SOFTIEC) ? IEC_ULT_SWITCHABLE : 0);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Title:       Does a device answer
// Description: Presence test for one ID. With the Device Manager API the
//              ROM is asked (drive type not "none"), else the bus is
//              probed with iec_present.
// Syntax:      static bool iec_device_answers(char id);
// Input:       id - IEC device ID
// Output:      true if a device is present
// ---------------------------------------------------------------------------
static bool iec_device_answers(char id)
{
    if (dminfo.present)
    {
        return dm_api_get_drivetype(id) != DM_TYPE_NONE;
    }
    return iec_present(id);
}

// ---------------------------------------------------------------------------
// Title:       Does a device need manual switching
// Description: Tells whether a device found by iec_scan must be switched
//              off by hand for demo mode: any other device except on ID 8,
//              or a powered Ultimate device the UCI cannot switch.
// Syntax:      bool iec_needs_switching(char state, char id);
// Input:       state - active[] entry from iec_scan
//              id    - its device ID
// Output:      true if it needs manual switching
// ---------------------------------------------------------------------------
bool iec_needs_switching(char state, char id)
{
    if (state == IEC_OTHER)
    {
        return id != IEC_ID_FIRST;
    }
    return (state & IEC_ULT_POWERED) && !(state & IEC_ULT_SWITCHABLE) && state != IEC_HYPERSPEED;
}

// ---------------------------------------------------------------------------
// Title:       Scan the IEC bus
// Description: Fills an array with the active devices on IDs 8-29 and 4,
//              and tells whether any device needs manual power switching
//              (for demo mode): a powered Ultimate device the UCI cannot
//              switch (SoftIEC, printer), or any other device except on
//              ID 8. The Device Manager hyperspeed drive is listed but
//              needs no switching.
//              Uses uii_devinfo and dminfo: call uii_parse_deviceinfo and
//              dm_query first.
//              As UBoot64-v2 CheckActiveIECdevices; fixed there: the
//              result was overwritten per ID instead of accumulated.
// Syntax:      bool iec_scan(char *active);
// Input:       active - array of IEC_ID_COUNT bytes
// Output:      true if manual power switching is needed; active[] per
//              index: 0 = none, IEC_OTHER, IEC_HYPERSPEED, else the
//              Ultimate bits (IEC_ULT_*)
// ---------------------------------------------------------------------------
bool iec_scan(char *active)
{
    bool manual = false;

    for (char x = 0; x < IEC_ID_COUNT; x++)
    {
        char id = iec_index_to_id(x);
        char ult = iec_ultimate_on_id(id);

        if (ult & IEC_ULT_POWERED)
        {
            // Ultimate device (drive A/B, SoftIEC, printer)
            active[x] = ult;
        }
        else if (dminfo.present && id == dminfo.hyperspeed_id)
        {
            // Device Manager hyperspeed drive: served by the ROM through
            // the UCI, not a device on the bus
            active[x] = IEC_HYPERSPEED;
        }
        else
        {
            // Not an Ultimate device, or one reported as off: ask the bus
            active[x] = iec_device_answers(id) ? IEC_OTHER : 0;
        }
        if (iec_needs_switching(active[x], id))
        {
            manual = true;
        }
    }
    return manual;
}

// ---------------------------------------------------------------------------
// Title:       Slot number to key
// Description: Returns the key (and label) of a slot: 0-9, then a-z.
// Syntax:      char menuslotkey(char slotnumber);
// Input:       slotnumber - 0..SLOTS-1
// Output:      PETSCII key code
// ---------------------------------------------------------------------------
char menuslotkey(char slotnumber)
{
    if (slotnumber < SLOT_KEYS_DIGITS)
    {
        return PETSCII_ZERO + slotnumber;
    }
    return PETSCII_LETTER_A + slotnumber - SLOT_KEYS_DIGITS;
}

// ---------------------------------------------------------------------------
// Title:       Slot label
// Description: Returns the character shown in front of a slot in the menu:
//              0-9, then A-Z in uppercase.
// Syntax:      char menuslotlabel(char slotnumber);
// Input:       slotnumber - 0..SLOTS-1
// Output:      PETSCII character
// ---------------------------------------------------------------------------
char menuslotlabel(char slotnumber)
{
    char key = menuslotkey(slotnumber);

    return (slotnumber < SLOT_KEYS_DIGITS) ? key : key + PETSCII_SHIFT;
}

// ---------------------------------------------------------------------------
// Title:       Unshift a slot letter key
// Description: Maps shifted letters (A-Z) to their unshifted key codes, so
//              a slot is selected with or without shift.
// Syntax:      static char slotkey_unshift(char key);
// Input:       key - raw PETSCII key code
// Output:      Unshifted key code; other keys unchanged
// ---------------------------------------------------------------------------
static char slotkey_unshift(char key)
{
    if (key >= PETSCII_LETTER_A + PETSCII_SHIFT &&
        key < PETSCII_LETTER_A + PETSCII_SHIFT + SLOTS - SLOT_KEYS_DIGITS)
    {
        return key - PETSCII_SHIFT;
    }
    return key;
}

// ---------------------------------------------------------------------------
// Title:       Is this a slot key
// Description: Tells whether a key selects a slot (0-9, a-z, shifted A-Z).
// Syntax:      bool isslotkey(char key);
// Input:       key - raw PETSCII key code
// Output:      true for slot keys
// ---------------------------------------------------------------------------
bool isslotkey(char key)
{
    key = slotkey_unshift(key);
    return (key >= PETSCII_ZERO && key < PETSCII_ZERO + SLOT_KEYS_DIGITS) ||
           (key >= PETSCII_LETTER_A && key < PETSCII_LETTER_A + SLOTS - SLOT_KEYS_DIGITS);
}

// ---------------------------------------------------------------------------
// Title:       Key to slot number
// Description: Converts a slot key to its slot number.
// Syntax:      char keytomenuslot(char key);
// Input:       key - slot key (check with isslotkey first)
// Output:      Slot number 0..SLOTS-1
// ---------------------------------------------------------------------------
char keytomenuslot(char key)
{
    key = slotkey_unshift(key);
    if (key >= PETSCII_LETTER_A)
    {
        return key - PETSCII_LETTER_A + SLOT_KEYS_DIGITS;
    }
    return key - PETSCII_ZERO;
}

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
