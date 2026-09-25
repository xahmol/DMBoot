/*
DMBoot 128 v5 - Core helpers

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef CORE_H
#define CORE_H

#include "defines.h"

void errorexit(const char *message);
void delay(char seconds);
void spinning(void);
void headertext(const char *subtitle);
void progress(const char *text);
void asc2pet(char *dst, const char *src, unsigned dstsize);

#pragma compile("core.c")

#endif // CORE_H
