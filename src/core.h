/*
DMBoot 128 v5 - Core helpers

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef CORE_H
#define CORE_H

#include "defines.h"

void errorexit(const char *message);
void dmb_exit(void);
void delay(char seconds);
void spinning(void);
void headertext(const char *subtitle, char showtime);
void progress(const char *text);
void asc2pet(char *dst, const char *src, unsigned dstsize);
char dosCommand(char lfn, char device, char secaddr, const char *command);
char cmd(char device, const char *command);
void drive_root_reset(void);
char menuslotkey(char slotnumber);
char menuslotlabel(char slotnumber);
char keytomenuslot(char key);
bool isslotkey(char key);
char key_poll(void);
char key_wait(void);

#define DOS_STATUS_MAX      41      // DOS status message buffer
extern char DOSstatus[DOS_STATUS_MAX];

#pragma compile("core.c")

#endif // CORE_H
