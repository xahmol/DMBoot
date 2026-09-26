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
#include "timeconv.h"
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

// Configuration screen
#define CFG_ROW0            3
#define CFG_VALUE_X         18
#define UTC_OFFSET_MAX      50400L  // 14 hours
#define UTC_INPUT_MAX       8       // "-50400" + terminator, with room
#define COLOR_ROW0          3
#define COLOR_SWATCH_X      24
#define COLOR_OPTIONS       11      // Fields of struct ColorPalette
#define COLOR_MAX           15
#define GEOS_ROW0           3
#define DEVICE_ID_INPUT     3       // "30" + terminator
#define DEVICE_ID_MAX       30
#define INPUT_SIZE_MAX      255     // dwin_input takes a char size

static char uiitime[UII_TIME_BYTES];
static char textbuf[MAXHOSTLENGTH];
static char pathbuf[MAXPATHLEN];

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
    ntp_report("No answer: ", uii_status);
    uii_socketclose(socket);
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Ask one NTP server for the time
// Description: Sends an NTP request over a UDP socket of the Ultimate and
//              waits up to NTP_READ_ATTEMPTS seconds for the answer.
//              As UBoot64-v2 get_ntp_time.
// Syntax:      static bool ntp_query(char *host, unsigned long *seconds);
// Input:       host    - server name (ASCII)
//              seconds - receives the NTP transmit time (seconds since 1900)
// Output:      true when an answer came
// ---------------------------------------------------------------------------
static bool ntp_query(char *host, unsigned long *seconds)
{
    char request[3 + NTP_PACKET_SIZE];
    char socket;

    ntp_report("Asking time server: ", host);
    socket = uii_udpconnect(host, NTP_PORT);
    if (ntp_failed(socket))
    {
        return false;
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
        return false;
    }

    for (char attempt = 0; attempt < NTP_READ_ATTEMPTS; attempt++)
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
        return false;
    }

    // Reply after 2 length bytes; transmit timestamp seconds, big endian
    *seconds = ((unsigned long)(unsigned char)uii_data[2 + NTP_SECONDS_OFFSET] << 24) |
               ((unsigned long)(unsigned char)uii_data[3 + NTP_SECONDS_OFFSET] << 16) |
               ((unsigned long)(unsigned char)uii_data[4 + NTP_SECONDS_OFFSET] << 8) |
               (unsigned long)(unsigned char)uii_data[5 + NTP_SECONDS_OFFSET];
    uii_socketclose(socket);
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Update the time from an NTP server
// Description: When enabled in the configuration, asks the NTP servers in
//              turn (empty ones are skipped) until one answers, and sets the
//              Ultimate's clock (with the configured UTC offset). Firmware
//              3.14d and later set the clock themselves; this is for older
//              firmware and is off by default.
// Syntax:      void ntp_update(void);
// Input:       cfg.timeon, cfg.host/host2/host3 (ASCII), cfg.secondsfromutc
// Output:      None
// ---------------------------------------------------------------------------
void ntp_update(void)
{
    char *hosts[NTP_SERVERS] = { cfg.host, cfg.host2, cfg.host3 };
    unsigned long seconds;

    if (!cfg.timeon)
    {
        return;
    }

    for (char x = 0; x < NTP_SERVERS; x++)
    {
        if (!hosts[x][0] || !ntp_query(hosts[x], &seconds))
        {
            continue;
        }
        epoch_to_uiitime(seconds - NTP_TIMESTAMP_DELTA, cfg.secondsfromutc, uiitime);
        uii_set_time(uiitime);
        if (!UII_SUCCESS)
        {
            ntp_report("Setting the clock failed: ", uii_status);
            ntp_report("Reply: ", uii_data);
            sprintf(textbuf, "Sent: %u %u %u %u %u %u", uiitime[0], uiitime[1], uiitime[2], uiitime[3],
                    uiitime[4], uiitime[5]);
            ntp_report(textbuf, NULL);
            return;
        }
        uii_get_time();
        ntp_report("Clock set to: ", uii_data);
        return;
    }
    ntp_report("No time server answered.", NULL);
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

// Rows of the settings on the configuration screen
#define CFG_ROW_TIMEON      (CFG_ROW0 + 0)
#define CFG_ROW_VERBOSE     (CFG_ROW0 + 1)
#define CFG_ROW_UTC         (CFG_ROW0 + 2)
#define CFG_ROW_TIMEOUT     (CFG_ROW0 + 3)
#define CFG_ROW_HOST        (CFG_ROW0 + 4)  // Three rows, one per server
#define CFG_ROW_COLOURS     (CFG_ROW0 + 7)
#define CFG_ROW_GEOS        (CFG_ROW0 + 8)
#define CFG_ROW_BACK        (CFG_ROW0 + 10)
#define CFG_SERVER_LABEL_X  5       // "Server 2/3" under "NTP servers"

// ---------------------------------------------------------------------------
// Title:       Show a setting value
// Description: Clears the value column of a row and prints a value.
// Syntax:      static void cfg_value(char row, const char *value);
// Input:       row   - screen row
//              value - text (PETSCII)
// Output:      None
// ---------------------------------------------------------------------------
static void cfg_value(char row, const char *value)
{
    dwin_fill_rect(&screenwin, CFG_VALUE_X, row, screenwin.wx - CFG_VALUE_X, 1, ' ', cfg.colors.text);
    dwin_putat_string(&screenwin, CFG_VALUE_X, row, value, cfg.colors.text_input);
}

// ---------------------------------------------------------------------------
// Title:       Show the changeable values
// Description: One function per setting, so a change redraws only its
//              value.
// Syntax:      static void cfg_show_timeon(void); (and _verbose, _utc,
//              _timeout; cfg_show_host(char server) for NTP server 0-2)
// Input:       cfg
// Output:      None
// ---------------------------------------------------------------------------
static void cfg_show_timeon(void)
{
    cfg_value(CFG_ROW_TIMEON, cfg.timeon ? "On" : "Off");
}

static void cfg_show_verbose(void)
{
    cfg_value(CFG_ROW_VERBOSE, verbosenames[(cfg.verbose < VERBOSE_OPTIONS) ? cfg.verbose : VERBOSE_ON]);
}

static void cfg_show_utc(void)
{
    sprintf(textbuf, "%ld s", cfg.secondsfromutc);
    cfg_value(CFG_ROW_UTC, textbuf);
}

static void cfg_show_timeout(void)
{
    if (cfg.timeoutidx && cfg.timeoutidx < TIMEOUT_OPTIONS)
    {
        sprintf(textbuf, "%u s", timeoutseconds[cfg.timeoutidx]);
        cfg_value(CFG_ROW_TIMEOUT, textbuf);
    }
    else
    {
        cfg_value(CFG_ROW_TIMEOUT, "Off");
    }
}

static void cfg_show_host(char server)
{
    char *hosts[NTP_SERVERS] = { cfg.host, cfg.host2, cfg.host3 };
    char hostwidth = screenwin.wx - CFG_VALUE_X;

    asc2pet(textbuf, hosts[server], sizeof(textbuf));
    if (strlen(textbuf) > hostwidth)
    {
        textbuf[hostwidth] = 0;
    }
    cfg_value(CFG_ROW_HOST + server, textbuf[0] ? textbuf : "-");
}

// ---------------------------------------------------------------------------
// Title:       Draw the configuration screen
// Description: Full draw on entry: header, all settings with their keys.
// Syntax:      static void cfg_draw(void);
// Input:       cfg
// Output:      None
// ---------------------------------------------------------------------------
static void cfg_draw(void)
{
    dwin_clear(&screenwin);
    headertext("Configuration", 1);

    cfg_line(CFG_ROW_TIMEON, " F1 ", "NTP time sync", NULL);
    cfg_line(CFG_ROW_VERBOSE, " F2 ", "Start-up", NULL);
    cfg_line(CFG_ROW_UTC, " F3 ", "UTC offset", NULL);
    cfg_line(CFG_ROW_TIMEOUT, " F4 ", "Auto-boot", NULL);
    cfg_line(CFG_ROW_HOST, " F5 ", "NTP servers", NULL);
    dwin_putat_string(&screenwin, CFG_SERVER_LABEL_X, CFG_ROW_HOST + 1, "Server 2", cfg.colors.text);
    dwin_putat_string(&screenwin, CFG_SERVER_LABEL_X, CFG_ROW_HOST + 2, "Server 3", cfg.colors.text);
    cfg_line(CFG_ROW_COLOURS, " F6 ", "Colours", NULL);
    cfg_line(CFG_ROW_GEOS, " F8 ", "GEOS RAM boot", NULL);
    cfg_line(CFG_ROW_BACK, " F7 ", "Back (saves changes)", NULL);
    cfg_show_timeon();
    cfg_show_verbose();
    cfg_show_utc();
    cfg_show_timeout();
    for (char x = 0; x < NTP_SERVERS; x++)
    {
        cfg_show_host(x);
    }
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
// Title:       Input the NTP servers
// Description: Edits the three NTP host names in turn (stored in ASCII,
//              edited in PETSCII). STOP keeps a server as it was; an empty
//              name switches that server off. Each changed server's row is
//              redrawn.
// Syntax:      static bool cfg_hosts(void);
// Input:       None
// Output:      true when a server changed
// ---------------------------------------------------------------------------
static bool cfg_hosts(void)
{
    char *hosts[NTP_SERVERS] = { cfg.host, cfg.host2, cfg.host3 };
    char width = (screenwin.wx < MAXHOSTLENGTH) ? screenwin.wx - 1 : MAXHOSTLENGTH - 1;
    bool changed = false;

    for (char x = 0; x < NTP_SERVERS; x++)
    {
        asc2pet(textbuf, hosts[x], sizeof(textbuf));
        slotlist_clear_bottom();
        sprintf(pathbuf, "Server %u, empty = off, STOP = keep:", x + 1);
        dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, pathbuf, cfg.colors.text);
        if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, textbuf, sizeof(textbuf), width,
                       cfg.colors.text_input) != DWIN_INPUT_CANCEL)
        {
            pet2asc(hosts[x], textbuf, MAXHOSTLENGTH);
            cfg_show_host(x);
            changed = true;
        }
    }
    return changed;
}

// Colour editor: index of each colour in struct ColorPalette
#define COL_BACKGROUND      0
#define COL_BORDER          1
#define COL_HEADER1         2
#define COL_HEADER2         3
#define COL_TEXT            4
#define COL_KEY             6
#define COL_DIRNORMAL       7
#define COL_DIRSELECT       8

// ---------------------------------------------------------------------------
// Title:       Draw one colour row
// Description: Name, number and a swatch of one colour.
// Syntax:      static void color_row(char option, bool selected);
// Input:       option   - colour index (struct ColorPalette order)
//              selected - highlight the row
// Output:      None
// ---------------------------------------------------------------------------
static void color_row(char option, bool selected)
{
    char *colors = (char *)&cfg.colors;
    char row = COLOR_ROW0 + option;

    sprintf(textbuf, "%-16s %2u", colornames[option], colors[option]);
    if (selected)
    {
        dwin_putat_string_reverse(&screenwin, 0, row, textbuf, cfg.colors.diritem_select);
    }
    else
    {
        dwin_putat_string(&screenwin, 0, row, textbuf, cfg.colors.diritem_normal);
    }
    dwin_fill_rect(&screenwin, COLOR_SWATCH_X, row, 4, 1, ' ', colors[option]);
    dwin_reverse_rect(&screenwin, COLOR_SWATCH_X, row, 4, 1);
}

// ---------------------------------------------------------------------------
// Title:       Draw the colour rows
// Description: All colour rows, one of them highlighted.
// Syntax:      static void color_rows(char option);
// Input:       option - highlighted colour
// Output:      None
// ---------------------------------------------------------------------------
static void color_rows(char option)
{
    for (char x = 0; x < COLOR_OPTIONS; x++)
    {
        color_row(x, x == option);
    }
}

// ---------------------------------------------------------------------------
// Title:       Draw the colour editor legend
// Description: Key hints below the colour rows.
// Syntax:      static void color_legend(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
static void color_legend(void)
{
    fkey_hint(0, SLOTLIST_LEGEND_ROW - 1, " UP/DOWN ", "Choose");
    fkey_hint(0, SLOTLIST_LEGEND_ROW, " LEFT/RIGHT ", "Change colour");
    fkey_hint(0, SLOTLIST_LEGEND_ROW + 1, " DEL ", "Undo all");
    fkey_hint(0, SLOTLIST_LEGEND_ROW + 2, " F7 ", "Back");
}

// ---------------------------------------------------------------------------
// Title:       Draw the colour editor
// Description: Full draw: screen colours, header, rows and legend.
// Syntax:      static void color_draw(char option);
// Input:       option - highlighted colour
// Output:      None
// ---------------------------------------------------------------------------
static void color_draw(char option)
{
    dwin_screen_colors(cfg.colors.border, cfg.colors.background);
    dwin_clear(&screenwin);
    headertext("Colours", 1);
    color_rows(option);
    color_legend();
}

// ---------------------------------------------------------------------------
// Title:       Show a colour change
// Description: Redraws what uses the changed colour: its row, and the
//              screen colours, the header, all rows or the legend when the
//              colour is theirs.
// Syntax:      static void color_changed(char option);
// Input:       option - changed colour (also the highlighted one)
// Output:      None
// ---------------------------------------------------------------------------
static void color_changed(char option)
{
    switch (option)
    {
    case COL_BACKGROUND:
    case COL_BORDER:
        dwin_screen_colors(cfg.colors.border, cfg.colors.background);
        color_row(option, true);
        break;
    case COL_HEADER1:
    case COL_HEADER2:
        headertext("Colours", 1);
        color_row(option, true);
        break;
    case COL_DIRNORMAL:
    case COL_DIRSELECT:
        color_rows(option);
        break;
    case COL_TEXT:
    case COL_KEY:
        color_legend();
        color_row(option, true);
        break;
    default:
        color_row(option, true);
        break;
    }
}

// ---------------------------------------------------------------------------
// Title:       Colour editor
// Description: Cursor up/down selects a colour, left/right changes it and
//              shows the result at once (only what uses the colour is
//              redrawn). DEL restores the colours from when the editor was
//              opened.
// Syntax:      static bool cfg_colors(void);
// Input:       None
// Output:      true when a colour changed
// ---------------------------------------------------------------------------
static bool cfg_colors(void)
{
    struct ColorPalette saved = cfg.colors;
    char *colors = (char *)&cfg.colors;
    char option = 0;

    color_draw(option);
    while (true)
    {
        switch (key_wait())
        {
        case KEY_CURSOR_DOWN:
            color_row(option, false);
            option = (option + 1) % COLOR_OPTIONS;
            color_row(option, true);
            break;
        case KEY_CURSOR_UP:
            color_row(option, false);
            option = (option + COLOR_OPTIONS - 1) % COLOR_OPTIONS;
            color_row(option, true);
            break;
        case KEY_CURSOR_LEFT:
            colors[option] = (colors[option] - 1) & COLOR_MAX;
            color_changed(option);
            break;
        case KEY_CURSOR_RIGHT:
            colors[option] = (colors[option] + 1) & COLOR_MAX;
            color_changed(option);
            break;
        case KEY_DEL:
            cfg.colors = saved;
            color_draw(option);
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
// Title:       Edit an Ultimate path or file name
// Description: Edits an ASCII field (Ultimate file system) in PETSCII on the
//              input rows. STOP keeps the old value.
// Syntax:      static bool cfg_ascii_field(const char *prompt, char *field,
//                                          unsigned size);
// Input:       prompt - text above the input
//              field  - ASCII string to edit
//              size   - size of field
// Output:      true when changed
// ---------------------------------------------------------------------------
static bool cfg_ascii_field(const char *prompt, char *field, unsigned size)
{
    char inputsize = (size > INPUT_SIZE_MAX) ? INPUT_SIZE_MAX : size;

    asc2pet(pathbuf, field, inputsize);
    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, prompt, cfg.colors.text);
    if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, pathbuf, inputsize, screenwin.wx - 1,
                   cfg.colors.text_input) == DWIN_INPUT_CANCEL)
    {
        return false;
    }
    pet2asc(field, pathbuf, size);
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Edit a drive image setting
// Description: Asks for the device ID (0 = none), path and file name of a
//              GEOS disk image.
// Syntax:      static bool cfg_geos_image(char *id, char *path, char *file);
// Input:       id, path, file - fields of cfg.geos
// Output:      true when changed
// ---------------------------------------------------------------------------
static bool cfg_geos_image(char *id, char *path, char *file)
{
    char input[DEVICE_ID_INPUT];
    const char *end;
    long value;

    sprintf(input, "%u", *id);
    slotlist_clear_bottom();
    dwin_putat_string(&screenwin, 0, SLOTLIST_LEGEND_ROW, "Device ID (0 = no image):", cfg.colors.text);
    if (dwin_input(&screenwin, 0, SLOTLIST_LEGEND_ROW + 1, input, sizeof(input), sizeof(input) - 1,
                   cfg.colors.text_input) <= 0)
    {
        return false;
    }
    value = strtol(input, &end, 10);
    if (*end || value < 0 || value > DEVICE_ID_MAX)
    {
        return false;
    }
    *id = value;
    if (*id)
    {
        cfg_ascii_field("Image path (e.g. /usb1/11/):", path, MAXPATHLEN);
        cfg_ascii_field("Image file name:", file, MAXFILENAME);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Title:       Show one GEOS setting
// Description: Prints a key with its label and, on the next row, the path
//              and file name (clipped to the screen width).
// Syntax:      static char cfg_geos_line(char row, const char *key,
//                                        const char *label, char id,
//                                        const char *path,
//                                        const char *file);
// Input:       row   - screen row
//              key   - key label
//              label - setting name
//              id    - device ID (0 = not shown)
//              path  - ASCII path
//              file  - ASCII file name
// Output:      Next free row
// ---------------------------------------------------------------------------
static char cfg_geos_line(char row, const char *key, const char *label, char id, const char *path,
                          const char *file)
{
    fkey_hint(0, row, key, label);
    if (!file[0])
    {
        dwin_putat_string(&screenwin, CFG_VALUE_X, row, "(none)", cfg.colors.text_input);
        return row + 2;
    }
    if (id)
    {
        sprintf(textbuf, "ID %u", id);
        dwin_putat_string(&screenwin, CFG_VALUE_X, row, textbuf, cfg.colors.text_input);
    }
    asc2pet(pathbuf, path, sizeof(pathbuf));
    asc2pet(textbuf, file, sizeof(textbuf));
    strncat(pathbuf, textbuf, sizeof(pathbuf) - 1 - strlen(pathbuf));
    if (strlen(pathbuf) > screenwin.wx - 2)
    {
        pathbuf[screenwin.wx - 2] = 0;
    }
    dwin_putat_string(&screenwin, 1, row + 1, pathbuf, cfg.colors.text_input);
    return row + 2;
}

// Rows of the GEOS settings (each image setting uses two rows)
#define GEOS_ROW_REU        GEOS_ROW0
#define GEOS_ROW_SIZE       (GEOS_ROW0 + 2)
#define GEOS_ROW_A          (GEOS_ROW0 + 4)
#define GEOS_ROW_B          (GEOS_ROW0 + 6)
#define GEOS_ROW_BACK       (GEOS_ROW0 + 9)

// ---------------------------------------------------------------------------
// Title:       Show one GEOS setting
// Description: One function per setting: clears its rows and redraws them,
//              so a change redraws only that setting.
// Syntax:      static void geos_show_reu(void); (and _size, _a, _b)
// Input:       cfg.geos
// Output:      None
// ---------------------------------------------------------------------------
static void geos_clear(char row, char rows)
{
    dwin_fill_rect(&screenwin, 0, row, screenwin.wx, rows, ' ', cfg.colors.text);
}

static void geos_show_reu(void)
{
    geos_clear(GEOS_ROW_REU, 2);
    cfg_geos_line(GEOS_ROW_REU, " F1 ", "REU image", 0, cfg.geos.reu_path, cfg.geos.reu_image);
}

static void geos_show_size(void)
{
    geos_clear(GEOS_ROW_SIZE, 1);
    cfg_line(GEOS_ROW_SIZE, " F2 ", "REU size", reusizenames[cfg.geos.reusize]);
}

static void geos_show_a(void)
{
    geos_clear(GEOS_ROW_A, 2);
    cfg_geos_line(GEOS_ROW_A, " F3 ", "Drive A image", cfg.geos.image_a_id, cfg.geos.image_a_path,
                  cfg.geos.image_a_file);
}

static void geos_show_b(void)
{
    geos_clear(GEOS_ROW_B, 2);
    cfg_geos_line(GEOS_ROW_B, " F5 ", "Drive B image", cfg.geos.image_b_id, cfg.geos.image_b_path,
                  cfg.geos.image_b_file);
}

// ---------------------------------------------------------------------------
// Title:       GEOS RAM boot settings
// Description: REU image and size, and the disk images for drives A and B
//              that the GEOS RAM boot (main menu F6) uses. Drawn once;
//              a change redraws only its setting.
// Syntax:      static bool cfg_geos(void);
// Input:       None
// Output:      true when something changed
// ---------------------------------------------------------------------------
static bool cfg_geos(void)
{
    struct GeosConfig *geos = &cfg.geos;
    bool changed = false;

    if (geos->reusize >= REU_SIZES)
    {
        geos->reusize = REU_SIZES - 1;
    }
    dwin_clear(&screenwin);
    headertext("GEOS RAM boot settings", 1);
    geos_show_reu();
    geos_show_size();
    geos_show_a();
    geos_show_b();
    cfg_line(GEOS_ROW_BACK, " F7 ", "Back", NULL);

    while (true)
    {
        switch (key_wait())
        {
        case KEY_F1:
            changed |= cfg_ascii_field("REU image path (e.g. /usb1/11/):", geos->reu_path, MAXPATHLEN);
            changed |= cfg_ascii_field("REU image file name:", geos->reu_image, MAXFILENAME);
            slotlist_clear_bottom();
            geos_show_reu();
            break;
        case KEY_F2:
            geos->reusize = (geos->reusize + 1) % REU_SIZES;
            geos_show_size();
            changed = true;
            break;
        case KEY_F3:
            changed |= cfg_geos_image(&geos->image_a_id, geos->image_a_path, geos->image_a_file);
            slotlist_clear_bottom();
            geos_show_a();
            break;
        case KEY_F5:
            changed |= cfg_geos_image(&geos->image_b_id, geos->image_b_path, geos->image_b_file);
            slotlist_clear_bottom();
            geos_show_b();
            break;
        case KEY_F7:
        case KEY_STOP:
            return changed;
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

    cfg_draw();
    while (true)
    {
        switch (key_wait())
        {
        case KEY_F1:
            cfg.timeon = !cfg.timeon;
            cfg_show_timeon();
            changed = true;
            break;
        case KEY_F2:
            cfg.verbose = (cfg.verbose + 1 < VERBOSE_OPTIONS) ? cfg.verbose + 1 : VERBOSE_SILENT;
            cfg_show_verbose();
            changed = true;
            break;
        case KEY_F3:
            changed |= cfg_utcoffset();
            slotlist_clear_bottom();
            cfg_show_utc();
            break;
        case KEY_F4:
            cfg.timeoutidx = (cfg.timeoutidx + 1 < TIMEOUT_OPTIONS) ? cfg.timeoutidx + 1 : 0;
            cfg_show_timeout();
            changed = true;
            break;
        case KEY_F5:
            changed |= cfg_hosts();
            slotlist_clear_bottom();
            break;
        case KEY_F6:
            // Another screen: full redraw on return
            changed |= cfg_colors();
            cfg_draw();
            break;
        case KEY_F8:
            changed |= cfg_geos();
            cfg_draw();
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
