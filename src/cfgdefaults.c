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

// Default NTP server as raw ASCII bytes ("pool.ntp.org"), charmap independent
static const char ntphost[] = { 0x70, 0x6f, 0x6f, 0x6c, 0x2e, 0x6e, 0x74, 0x70, 0x2e, 0x6f, 0x72, 0x67, 0x00 };

// ---------------------------------------------------------------------------
// Title:       Default configuration
// Description: Fills a configuration with the defaults: NTP on with
//              pool.ntp.org, messages at start-up, the default colours,
//              everything else zero.
// Syntax:      void config_set_defaults(struct ConfigStruct *config);
// Input:       config - configuration to fill
// Output:      config
// ---------------------------------------------------------------------------
void config_set_defaults(struct ConfigStruct *config)
{
    memset(config, 0, sizeof(*config));
    config->version = CFGVERSION;
    config->timeon = 1;
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
    strncpy(config->host, ntphost, sizeof(config->host) - 1);
    config->host[sizeof(config->host) - 1] = 0;
}
