/*
DMBoot 128 v5 - Configuration overlay: NTP time, settings, information

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

NTP time, configuration and colour editor ported from src/time.c of my
UBoot64-v2 project (https://github.com/xahmol/UBoot64-v2); adapted: the
converted time is kept in a static buffer (UBoot64 returned a pointer to
a local array), three start-up feedback options, colour undo from a copy
taken on entry (UBoot64 re-read the config file, which also dropped other
unsaved changes), changes of all editors are combined before saving.

Code and resources from others used:
-   ntp2ultimate by MaxPlap
    https://github.com/MaxPlap/ntp2ultimate
    Time via NTP (NTP request over the Ultimate's UDP socket).
-   EPOCH-to-time-date-converter by sidsingh78
    https://github.com/sidsingh78/EPOCH-to-time-date-converter/blob/master/epoch_conv.c
    Epoch to date conversion.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <petscii.h>
#include "defines.h"
#include "dualwin.h"
#include "dmapi.h"
#include "testmode.h"
#include "ultimate_common_lib.h"
#include "ultimate_time_lib.h"
#include "ultimate_network_lib.h"
#include "core.h"
#include "fileio.h"
#include "slotlist.h"
#include "config.h"

#pragma overlay(dmbovl4, 5)
#pragma section(codeovl4, 0)
#pragma section(dataovl4, 0)
#pragma section(bssovl4, 0)
#pragma region(ovl4, OVERLAYLOAD, OVERLAY_SLOT_END, , 5, { codeovl4, dataovl4, bssovl4 })

#pragma code(codeovl4)
#pragma data(dataovl4)
#pragma bss(bssovl4)

// NTP
#define NTP_PORT            123
#define NTP_TIMESTAMP_DELTA 2208988800UL    // Seconds from 1900 to 1970
#define NTP_PACKET_SIZE     48
#define NTP_REQUEST_FIRST   0x1b    // LI 0, version 3, client mode
#define NTP_READ_ATTEMPTS   4
#define NTP_SECONDS_OFFSET  32      // Transmit timestamp (seconds) in the reply
#define SECONDS_PER_MINUTE  60
#define MINUTES_PER_HOUR    60
#define HOURS_PER_DAY       24
#define EPOCH_YEAR          1970
#define DAYS_PER_YEAR       365
#define UII_YEAR_BASE       1900
#define UII_TIME_BYTES      6

// Configuration screen
#define CFG_ROW0            3
#define CFG_VALUE_X         18
#define UTC_OFFSET_MAX      50400L  // 14 hours
#define UTC_INPUT_MAX       8       // "-50400" + terminator, with room
#define COLOR_ROW0          3
#define COLOR_SWATCH_X      24
#define COLOR_OPTIONS       11      // Fields of struct ColorPalette
#define COLOR_MAX           15

static char uiitime[UII_TIME_BYTES];
static char textbuf[MAXHOSTLENGTH];

static const char *const colornames[COLOR_OPTIONS] = {
    "Background", "Border", "Header line 1", "Header line 2", "Normal text", "Text input",
    "Key text", "Dir item normal", "Dir item select", "Error text", "OK text"
};
static const char *const verbosenames[VERBOSE_OPTIONS] = {
    VERBOSE_NAME_SILENT, VERBOSE_NAME_ON, VERBOSE_NAME_WAIT
};

// ===========================================================================
// NTP time
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Leap year
// Description: Tells whether a year is a leap year.
// Syntax:      static bool leapyear(unsigned year);
// Input:       year - year (e.g. 2026)
// Output:      true for a leap year
// ---------------------------------------------------------------------------
static bool leapyear(unsigned year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

// ---------------------------------------------------------------------------
// Title:       UNIX time to Ultimate time
// Description: Converts a UNIX epoch (plus the configured UTC offset) to
//              the Ultimate RTC format: year - 1900, month, day, hour,
//              minute, second.
// Syntax:      static void epoch_to_uiitime(unsigned long epoch);
// Input:       epoch - seconds since 1970-01-01 00:00 UTC
// Output:      uiitime
// ---------------------------------------------------------------------------
static void epoch_to_uiitime(unsigned long epoch)
{
    static const char monthdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    unsigned days;
    unsigned year = EPOCH_YEAR;
    char month = 0;

    epoch += cfg.secondsfromutc;
    uiitime[5] = epoch % SECONDS_PER_MINUTE;
    epoch /= SECONDS_PER_MINUTE;
    uiitime[4] = epoch % MINUTES_PER_HOUR;
    epoch /= MINUTES_PER_HOUR;
    uiitime[3] = epoch % HOURS_PER_DAY;
    days = epoch / HOURS_PER_DAY;

    // Whole years, then whole months
    while (days >= (leapyear(year) ? DAYS_PER_YEAR + 1 : DAYS_PER_YEAR))
    {
        days -= leapyear(year) ? DAYS_PER_YEAR + 1 : DAYS_PER_YEAR;
        year++;
    }
    while (month < 11)
    {
        char length = monthdays[month] + ((month == 1 && leapyear(year)) ? 1 : 0);
        if (days < length)
        {
            break;
        }
        days -= length;
        month++;
    }

    uiitime[0] = year - UII_YEAR_BASE;
    uiitime[1] = month + 1;
    uiitime[2] = days + 1;
}

// ---------------------------------------------------------------------------
// Title:       Report an NTP step
// Description: Prints a line in verbose mode, or turns the spinner.
// Syntax:      static void ntp_report(const char *label, const char *ascii);
// Input:       label - text (PETSCII)
//              ascii - value from the Ultimate (ASCII), may be NULL
// Output:      None
// ---------------------------------------------------------------------------
static void ntp_report(const char *label, const char *ascii)
{
    if (!cfg.verbose)
    {
        spinning();
        return;
    }
    dwin_put_string(&console, label, cfg.colors.text);
    if (ascii)
    {
        asc2pet(textbuf, ascii, sizeof(textbuf));
        dwin_put_string(&console, textbuf, cfg.colors.text);
    }
    dwin_put_char(&console, '\n', cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       NTP step failed
// Description: Reports a failed NTP step (verbose mode) and closes the
//              socket.
// Syntax:      static bool ntp_failed(char socket);
// Input:       socket - open socket
// Output:      true when the last UCI command failed
// ---------------------------------------------------------------------------
static bool ntp_failed(char socket)
{
    if (UII_SUCCESS)
    {
        return false;
    }
    ntp_report("NTP time update failed: ", uii_status);
    uii_socketclose(socket);
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Update the time from an NTP server
// Description: When enabled in the configuration, asks the NTP server for
//              the time over a UDP socket of the Ultimate and sets the
//              Ultimate's clock (with the configured UTC offset).
//              As UBoot64-v2 get_ntp_time.
// Syntax:      void ntp_update(void);
// Input:       cfg.timeon, cfg.host (ASCII), cfg.secondsfromutc
// Output:      None
// ---------------------------------------------------------------------------
void ntp_update(void)
{
    char request[3 + NTP_PACKET_SIZE];
    char socket;
    char attempt;
    unsigned long seconds;

    if (!cfg.timeon)
    {
        return;
    }

    ntp_report("Updating the time from: ", cfg.host);
    socket = uii_udpconnect(cfg.host, NTP_PORT);
    if (ntp_failed(socket))
    {
        return;
    }

    memset(request, 0, sizeof(request));
    request[1] = NET_CMD_SOCKET_WRITE;
    request[2] = socket;
    request[3] = NTP_REQUEST_FIRST;
    uii_settarget(TARGET_NETWORK);
    uii_sendcommand(request, sizeof(request));
    uii_readstatus();
    uii_accept();
    if (ntp_failed(socket))
    {
        return;
    }

    for (attempt = 0; attempt < NTP_READ_ATTEMPTS; attempt++)
    {
        delay(1);
        uii_socketread(socket, NTP_PACKET_SIZE + 2);
        if (UII_SUCCESS)
        {
            break;
        }
    }
    if (ntp_failed(socket))
    {
        return;
    }

    // Reply after 2 length bytes; transmit timestamp seconds, big endian
    seconds = ((unsigned long)(unsigned char)uii_data[2 + NTP_SECONDS_OFFSET] << 24) |
              ((unsigned long)(unsigned char)uii_data[3 + NTP_SECONDS_OFFSET] << 16) |
              ((unsigned long)(unsigned char)uii_data[4 + NTP_SECONDS_OFFSET] << 8) |
              (unsigned long)(unsigned char)uii_data[5 + NTP_SECONDS_OFFSET];
    uii_socketclose(socket);

    epoch_to_uiitime(seconds - NTP_TIMESTAMP_DELTA);
    uii_set_time(uiitime);
    if (!UII_SUCCESS)
    {
        ntp_report("Setting the clock failed: ", uii_status);
        return;
    }
    uii_get_time();
    ntp_report("Clock set to: ", uii_data);
}

// ===========================================================================
// Configuration screen
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Print a setting line
// Description: Prints a function key, a label and a value on one row.
// Syntax:      static void cfg_line(char row, const char *key,
//                                   const char *label, const char *value);
// Input:       row   - screen row
//              key   - key label, e.g. " F1 "
//              label - setting name
//              value - current value (PETSCII), may be NULL
// Output:      None
// ---------------------------------------------------------------------------
static void cfg_line(char row, const char *key, const char *label, const char *value)
{
    fkey_hint(0, row, key, label);
    if (value)
    {
        dwin_putat_string(&screenwin, CFG_VALUE_X, row, value, cfg.colors.text_input);
    }
}

// ---------------------------------------------------------------------------
// Title:       Draw the configuration screen
// Description: Shows all settings with their keys.
// Syntax:      static void cfg_draw(void);
// Input:       cfg
// Output:      None
// ---------------------------------------------------------------------------
static void cfg_draw(void)
{
    char value[16];
    char row = CFG_ROW0;
    char hostwidth = screenwin.wx - CFG_VALUE_X;

    dwin_clear(&screenwin);
    headertext("Configuration", 1);

    cfg_line(row++, " F1 ", "NTP time sync", cfg.timeon ? "On" : "Off");
    cfg_line(row++, " F2 ", "Start-up", verbosenames[(cfg.verbose < VERBOSE_OPTIONS) ? cfg.verbose : VERBOSE_ON]);
    sprintf(value, "%ld s", cfg.secondsfromutc);
    cfg_line(row++, " F3 ", "UTC offset", value);
    if (cfg.timeoutidx && cfg.timeoutidx < TIMEOUT_OPTIONS)
    {
        sprintf(value, "%u s", timeoutseconds[cfg.timeoutidx]);
        cfg_line(row++, " F4 ", "Auto-boot", value);
    }
    else
    {
        cfg_line(row++, " F4 ", "Auto-boot", "Off");
    }
    asc2pet(textbuf, cfg.host, sizeof(textbuf));
    if (strlen(textbuf) > hostwidth)
    {
        textbuf[hostwidth] = 0;
    }
    cfg_line(row++, " F5 ", "NTP server", textbuf);
    cfg_line(row++, " F6 ", "Colours", NULL);
    row++;
    cfg_line(row, " F7 ", "Back (saves changes)", NULL);
}

// ---------------------------------------------------------------------------
// Title:       Input the UTC offset
// Description: Asks for the offset to UTC in seconds (-50400 .. 50400).
//              Invalid input keeps the old value.
// Syntax:      static bool cfg_utcoffset(void);
// Input:       None
// Output:      true when changed
// ---------------------------------------------------------------------------
static bool cfg_utcoffset(void)
{
    char input[UTC_INPUT_MAX];
    const char *end;
    long offset;

    sprintf(input, "%ld", cfg.secondsfromutc);
    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "UTC offset in seconds (7200 = +2 h):", cfg.colors.text);
    if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, input, sizeof(input), sizeof(input) - 1,
                   cfg.colors.text_input) <= 0)
    {
        return false;
    }
    offset = strtol(input, &end, 10);
    if (*end || offset > UTC_OFFSET_MAX || offset < -UTC_OFFSET_MAX)
    {
        return false;
    }
    cfg.secondsfromutc = offset;
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Input the NTP server
// Description: Edits the NTP host name (stored in ASCII, edited in PETSCII).
// Syntax:      static bool cfg_host(void);
// Input:       None
// Output:      true when changed
// ---------------------------------------------------------------------------
static bool cfg_host(void)
{
    char width = (screenwin.wx < MAXHOSTLENGTH) ? screenwin.wx - 1 : MAXHOSTLENGTH - 1;

    asc2pet(textbuf, cfg.host, sizeof(textbuf));
    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "NTP server host name:", cfg.colors.text);
    if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, textbuf, sizeof(textbuf), width,
                   cfg.colors.text_input) <= 0)
    {
        return false;
    }
    pet2asc(cfg.host, textbuf, sizeof(cfg.host));
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Colour editor
// Description: Cursor up/down selects a colour, left/right changes it and
//              shows the result at once. DEL restores the colours from
//              when the editor was opened.
// Syntax:      static bool cfg_colors(void);
// Input:       None
// Output:      true when a colour changed
// ---------------------------------------------------------------------------
static bool cfg_colors(void)
{
    struct ColorPalette saved = cfg.colors;
    char *colors = (char *)&cfg.colors;
    char option = 0;

    while (true)
    {
        dwin_screen_colors(cfg.colors.border, cfg.colors.background);
        dwin_clear(&screenwin);
        headertext("Colours", 1);
        for (char x = 0; x < COLOR_OPTIONS; x++)
        {
            char row = COLOR_ROW0 + x;
            sprintf(textbuf, "%-16s %2u", colornames[x], colors[x]);
            if (x == option)
            {
                dwin_putat_string_reverse(&screenwin, 0, row, textbuf, cfg.colors.diritem_select);
            }
            else
            {
                dwin_putat_string(&screenwin, 0, row, textbuf, cfg.colors.diritem_normal);
            }
            dwin_fill_rect(&screenwin, COLOR_SWATCH_X, row, 4, 1, ' ', colors[x]);
            dwin_reverse_rect(&screenwin, COLOR_SWATCH_X, row, 4, 1);
        }
        fkey_hint(0, SLOTLIST_LEGEND_ROW - 1, " UP/DOWN ", "Choose");
        fkey_hint(0, SLOTLIST_LEGEND_ROW, " LEFT/RIGHT ", "Change colour");
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 1, " DEL ", "Undo all");
        fkey_hint(0, SLOTLIST_LEGEND_ROW + 2, " F7 ", "Back");

        switch (key_wait())
        {
        case KEY_CURSOR_DOWN:
            option = (option + 1) % COLOR_OPTIONS;
            break;
        case KEY_CURSOR_UP:
            option = (option + COLOR_OPTIONS - 1) % COLOR_OPTIONS;
            break;
        case KEY_CURSOR_LEFT:
            colors[option] = (colors[option] - 1) & COLOR_MAX;
            break;
        case KEY_CURSOR_RIGHT:
            colors[option] = (colors[option] + 1) & COLOR_MAX;
            break;
        case KEY_DEL:
            cfg.colors = saved;
            break;
        case KEY_F7:
        case KEY_STOP:
            return memcmp(&saved, &cfg.colors, sizeof(saved)) != 0;
        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Configuration
// Description: Menu for the settings; F7 returns and writes the config
//              file when something changed.
// Syntax:      void config_edit(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void config_edit(void)
{
    bool changed = false;

    while (true)
    {
        cfg_draw();
        switch (key_wait())
        {
        case KEY_F1:
            cfg.timeon = !cfg.timeon;
            changed = true;
            break;
        case KEY_F2:
            cfg.verbose = (cfg.verbose + 1 < VERBOSE_OPTIONS) ? cfg.verbose + 1 : VERBOSE_SILENT;
            changed = true;
            break;
        case KEY_F3:
            changed |= cfg_utcoffset();
            break;
        case KEY_F4:
            cfg.timeoutidx = (cfg.timeoutidx + 1 < TIMEOUT_OPTIONS) ? cfg.timeoutidx + 1 : 0;
            changed = true;
            break;
        case KEY_F5:
            changed |= cfg_host();
            break;
        case KEY_F6:
            changed |= cfg_colors();
            break;
        case KEY_F7:
        case KEY_STOP:
            if (changed)
            {
                slotlist_clear_bottom();
                dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "Saving the configuration.", cfg.colors.text);
                writeconfigfile();
            }
            return;
        default:
            break;
        }
    }
}

// ===========================================================================
// Information
// ===========================================================================

// ---------------------------------------------------------------------------
// Title:       Splash
// Description: Simple text logo, centred for 40 or 80 columns. Kept behind
//              one function so PETSCII art can replace it later.
// Syntax:      static char splash(void);
// Input:       None
// Output:      First free row below the logo
// ---------------------------------------------------------------------------
static char splash(void)
{
    static const char *const logo[] = {
        "+--------------------------------+",
        "|         D M B o o t  128       |",
        "|   Device Manager Boot Menu     |",
        "+--------------------------------+"
    };
    char rows = sizeof(logo) / sizeof(logo[0]);
    char x = (screenwin.wx - strlen(logo[0])) / 2;

    for (char r = 0; r < rows; r++)
    {
        dwin_putat_string(&screenwin, x, 2 + r, logo[r], (r == 1) ? cfg.colors.header1 : cfg.colors.text);
    }
    return 2 + rows + 1;
}

// ---------------------------------------------------------------------------
// Title:       Information
// Description: Splash, version, system information and credits; any key
//              returns.
// Syntax:      void information(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void information(void)
{
    char row;

    dwin_clear(&screenwin);
    row = splash();

    dwin_cursor_move(&screenwin, 0, row);
    dwin_printf(&screenwin, cfg.colors.text, "Version %s\n", VERSION);
    dwin_put_string(&screenwin, "Written by Xander Mol, 2020-2026.\n\n", cfg.colors.text);

    uii_identify();
    asc2pet(textbuf, uii_data, sizeof(textbuf));
    textbuf[screenwin.wx - 1] = 0;
    dwin_printf(&screenwin, cfg.colors.text, "%s\n", textbuf);
    dwin_printf(&screenwin, cfg.colors.text, "REU %u KB, %s columns\n", sysinfo.reupages * 64,
                dwin_is80() ? "80" : "40");
    if (dminfo.present)
    {
        dwin_printf(&screenwin, cfg.colors.text, "Device Manager API v%u.%u\n\n",
                    dminfo.version_major, dminfo.version_minor);
    }

    dwin_put_string(&screenwin, "Thanks to:\n", cfg.colors.header2);
    dwin_put_string(&screenwin, "Bart van Leeuwen: Device Manager ROM\n", cfg.colors.text);
    dwin_put_string(&screenwin, "Gideon Zweijtzer: Ultimate II+\n", cfg.colors.text);
    dwin_put_string(&screenwin, "DrMortalWombat: Oscar64\n", cfg.colors.text);
    dwin_put_string(&screenwin, "Scott Hutter, Francesco Sblendorio:\n  ultimateii-dos-lib\n", cfg.colors.text);
    dwin_put_string(&screenwin, "Sascha Bader, Dirk Jagdmann: DraBrowse\n", cfg.colors.text);
    dwin_put_string(&screenwin, "MaxPlap: ntp2ultimate\n", cfg.colors.text);

    dwin_putat_string(&screenwin, 0, SLOTLIST_PROMPT_ROW, "Press a key.", cfg.colors.text);
    key_wait();
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
