/*
DMBoot 128 v5 - Configuration overlay: NTP time, settings, information

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef CONFIG_H
#define CONFIG_H

#define OVERLAY_CONFIG      4

__noinline void ntp_update(void);
__noinline void config_edit(void);
__noinline void information(void);

#pragma compile("config.c")

#endif // CONFIG_H
