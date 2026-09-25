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
#include "ultimate_time_lib.h"
#include "banking.h"
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

// ASCII (as sent by the Ultimate) to PETSCII (lower/upper case charset)
static const unsigned char asc2pet_table[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x14, 0x20, 0x0d, 0x11, 0x93, 0x0a, 0x0e, 0x0f,
    0x10, 0x0b, 0x12, 0x13, 0x08, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0xc0, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x0c, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

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

// Zero page used by Oscar64 (default layout, MachineTypes.cpp): registers
// $02-$26 and $43-$62, automatic zero page variables $F7-$FF. On the C128
// these overlap BASIC 7 work storage, which BASIC does not reinitialise on
// RUN, so they are saved at start-up and restored on exit (the cc65 runtime
// of v4 did the same).
#define ZP_LOW_START    0x02
#define ZP_LOW_SIZE     0x25    // $02-$26
#define ZP_MID_START    0x43
#define ZP_MID_SIZE     0x20    // $43-$62
#define ZP_HIGH_START   0xf7
#define ZP_HIGH_SIZE    0x09    // $F7-$FF

static char zp_save_low[ZP_LOW_SIZE];
static char zp_save_mid[ZP_MID_SIZE];
static char zp_save_high[ZP_HIGH_SIZE];

// ---------------------------------------------------------------------------
// Title:       Save BASIC zero page
// Description: Copies the zero page ranges Oscar64 uses to a buffer. Must be
//              the first call in main(): only the start-up code has run
//              before it (it sets ip $19-$1A and sp $23-$24, which BASIC
//              reinitialises itself, see dmb_exit).
// Syntax:      void dmb_zp_save(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void dmb_zp_save(void)
{
    __asm
    {
        ldx #0
lsave1:
        lda ZP_LOW_START, x
        sta zp_save_low, x
        inx
        cpx #ZP_LOW_SIZE
        bne lsave1
        ldx #0
lsave2:
        lda ZP_MID_START, x
        sta zp_save_mid, x
        inx
        cpx #ZP_MID_SIZE
        bne lsave2
        ldx #0
lsave3:
        lda ZP_HIGH_START, x
        sta zp_save_high, x
        inx
        cpx #ZP_HIGH_SIZE
        bne lsave3
    }
}

// Screen editor key store vector and the editor entry point after its
// function key expansion (C128 KERNAL, as used by cc65).
#define KEYSTORE_VECTOR     0x033c
#define KEYSTORE_NOFKEYS    0xc6b7

static unsigned keystore_saved;

// ---------------------------------------------------------------------------
// Title:       Raw function keys
// Description: Switches off the screen editor's function key expansion, so
//              F1-F8 and HELP return their own key codes instead of their
//              strings (F7 gave "LIST" + RETURN, which started slot L).
//              dmb_exit restores the original vector.
//              Based on cc65 libsrc/c128/cgetc.s (Ullrich von Bassewitz,
//              https://github.com/cc65/cc65): same vector and entry point.
//              Adapted: C, saved once at start-up, restored in dmb_exit.
// Syntax:      void dmb_fkeys_raw(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void dmb_fkeys_raw(void)
{
    volatile unsigned *vector = (volatile unsigned *)KEYSTORE_VECTOR;

    keystore_saved = *vector;
    __asm { sei }
    *vector = KEYSTORE_NOFKEYS;
    __asm { cli }
}

// ---------------------------------------------------------------------------
// Title:       Exit to BASIC (C128)
// Description: Ends the program and returns to BASIC from anywhere. Restores
//              the zero page saved by dmb_zp_save (BASIC 7 work storage
//              that Oscar64 uses; without it BASIC programs that use
//              variables or string functions crashed after a slot start),
//              then resets the string temporaries as Oscar64 crt.c spexit
//              does for the C128 ($13, $16, $18, $1A: the locations the
//              start-up code overwrote), and the editor key store vector
//              changed by dmb_fkeys_raw. No C code may run after the
//              restore, so everything is in assembler.
// Syntax:      void dmb_exit(void);
// Input:       None
// Output:      Does not return (returns to BASIC)
// ---------------------------------------------------------------------------
void dmb_exit(void)
{
    __asm
    {
        ldx spentry
        txs
        ldx #0
lrest1:
        lda zp_save_low, x
        sta ZP_LOW_START, x
        inx
        cpx #ZP_LOW_SIZE
        bne lrest1
        ldx #0
lrest2:
        lda zp_save_mid, x
        sta ZP_MID_START, x
        inx
        cpx #ZP_MID_SIZE
        bne lrest2
        ldx #0
lrest3:
        lda zp_save_high, x
        sta ZP_HIGH_START, x
        inx
        cpx #ZP_HIGH_SIZE
        bne lrest3
        lda keystore_saved + 1
        beq lnokey
        sei
        lda keystore_saved
        sta KEYSTORE_VECTOR
        lda keystore_saved + 1
        sta KEYSTORE_VECTOR + 1
        cli
lnokey:
        lda #0
        sta $13
        sta $1a
        lda #$1b
        sta $18
        lda #$19
        sta $16
    }
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
// Description: Shows the next frame of the start-up spinner (silent mode).
// Syntax:      void spinning(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void spinning(void)
{
    dwin_putat_char(&screenwin, dwin_state.width / 2, SPINNER_ROW, spinner[spinnerframe], cfg.colors.text);
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

// ---------------------------------------------------------------------------
// Title:       ASCII to PETSCII
// Description: Converts an ASCII string (as delivered by the Ultimate) to
//              PETSCII for display, into a separate buffer of known size.
// Syntax:      void asc2pet(char *dst, const char *src, unsigned dstsize);
// Input:       dst     - destination buffer
//              src     - 0-terminated ASCII string
//              dstsize - size of dst in bytes (>= 1)
// Output:      dst holds the converted, 0-terminated (possibly shortened)
//              string
// ---------------------------------------------------------------------------
void asc2pet(char *dst, const char *src, unsigned dstsize)
{
    unsigned i = 0;

    if (!dstsize)
    {
        return;
    }
    while (i < dstsize - 1 && src[i])
    {
        dst[i] = asc2pet_table[(unsigned char)src[i]];
        i++;
    }
    dst[i] = 0;
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
