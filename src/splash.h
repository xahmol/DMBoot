/*
DMBoot 128 v5 - Splash screen overlay

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef SPLASH_H
#define SPLASH_H

#define OVERLAY_SPLASH      6

__noinline void splash_show(void);

#pragma compile("splash.c")

#endif // SPLASH_H
