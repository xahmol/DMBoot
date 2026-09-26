/*
DMBoot 128 v5 - Default configuration

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Shared by DMBoot and the upgrade tool dmbupd45.
*/

#include <string.h>
#include <c64/vic.h>
#include "cfgdefaults.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

#define DEFAULT_UTC_OFFSET  7200    // Central European Summer Time

// Default NTP servers as raw ASCII bytes, charmap independent. The same
// servers as the Ultimate firmware's own time sync; pool.ntp.org last, as
// it did not always answer in tests.
static const char ntphost1[] = { 0x74, 0x69, 0x6d, 0x65, 0x2e, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e,
                                 0x63, 0x6f, 0x6d, 0x00 };                                  // time.google.com
static const char ntphost2[] = { 0x74, 0x69, 0x6d, 0x65, 0x2e, 0x77, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x73,
                                 0x2e, 0x63, 0x6f, 0x6d, 0x00 };                            // time.windows.com
static const char ntphost3[] = { 0x70, 0x6f, 0x6f, 0x6c, 0x2e, 0x6e, 0x74, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x00 };  // pool.ntp.org

// ---------------------------------------------------------------------------
// Title:       Copy a host name
// Description: Bounded copy of a default host name.
// Syntax:      static void set_host(char *dst, const char *src);
// Input:       dst - MAXHOSTLENGTH bytes
//              src - host name
// Output:      dst
// ---------------------------------------------------------------------------
static void set_host(char *dst, const char *src)
{
    strncpy(dst, src, MAXHOSTLENGTH - 1);
    dst[MAXHOSTLENGTH - 1] = 0;
}

// ---------------------------------------------------------------------------
// Title:       Default configuration
// Description: Fills a configuration with the defaults: NTP sync off
//              (firmware 3.14d and later sync the clock themselves) with
//              three servers, messages at start-up, the default colours,
//              everything else zero.
// Syntax:      void config_set_defaults(struct ConfigStruct *config);
// Input:       config - configuration to fill
// Output:      config
// ---------------------------------------------------------------------------
void config_set_defaults(struct ConfigStruct *config)
{
    memset(config, 0, sizeof(*config));
    config->version = CFGVERSION;
    config->timeon = 0;         // Firmware 3.14d and later set the time themselves
    config->secondsfromutc = DEFAULT_UTC_OFFSET;
    config->verbose = VERBOSE_ON;
    config->colors.background = VCOL_BLACK;
    config->colors.border = VCOL_BLACK;
    config->colors.header1 = VCOL_GREEN;
    config->colors.header2 = VCOL_LT_GREEN;
    config->colors.text = VCOL_YELLOW;
    config->colors.text_input = VCOL_WHITE;
    config->colors.key = VCOL_CYAN;
    config->colors.diritem_normal = VCOL_WHITE;
    config->colors.diritem_select = VCOL_CYAN;
    config->colors.error = VCOL_LT_RED;
    config->colors.ok = VCOL_LT_GREEN;
    set_host(config->host, ntphost1);
    set_host(config->host2, ntphost2);
    set_host(config->host3, ntphost3);
}
