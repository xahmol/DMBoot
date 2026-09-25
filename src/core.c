/*
DMBoot 128 v5 - Core helpers

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <stdlib.h>
#include <string.h>
#include <petscii.h>
#include <c64/cia.h>
#include "banking.h"
#include "dualwin.h"
#include "core.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

#define HEADER_TITLE        "DMBoot 128: Device Manager Boot Menu"
#define HEADER_ROW_TITLE    0
#define HEADER_ROW_SUB      1
#define SPINNER_ROW         3
#define SPINNER_FRAMES      4
#define CHR_REVSPACE        0xa0

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
    bnk_exit();
    exit(1);
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
// Description: Draws the two header lines: the program title and a subtitle
//              with the version at the right.
// Syntax:      void headertext(const char *subtitle);
// Input:       subtitle - text for the second header line
// Output:      None
// ---------------------------------------------------------------------------
void headertext(const char *subtitle)
{
    char width = dwin_state.width;
    char versionlen = strlen(VERSION);

    dwin_fill_rect(&screenwin, 0, HEADER_ROW_TITLE, width, 1, CHR_REVSPACE, cfg.colors.header1);
    dwin_fill_rect(&screenwin, 0, HEADER_ROW_SUB, width, 1, CHR_REVSPACE, cfg.colors.header2);
    dwin_putat_string_reverse(&screenwin, 0, HEADER_ROW_TITLE, HEADER_TITLE, cfg.colors.header1);
    dwin_putat_string_reverse(&screenwin, 0, HEADER_ROW_SUB, subtitle, cfg.colors.header2);
    if (versionlen < width)
    {
        dwin_putat_string_reverse(&screenwin, width - versionlen, HEADER_ROW_SUB, VERSION, cfg.colors.header2);
    }
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
