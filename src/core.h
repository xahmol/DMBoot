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
void headertext(const char *subtitle, char showtime);
void progress(const char *text);
char dosCommand(char lfn, char device, char secaddr, const char *command);
char cmd(char device, const char *command);
void drive_root_reset(void);
// IEC scan results (iec_scan)
#define IEC_OTHER           0x01    // A non-Ultimate device on the bus
#define IEC_HYPERSPEED      0x80    // Device Manager hyperspeed drive
#define IEC_ULT_EXISTS      0x01    // Ultimate device bits
#define IEC_ULT_POWERED     0x02
#define IEC_ULT_SWITCHABLE  0x04
#define UII_TYPE_SOFTIEC    0x0f    // uii_devinfo type: 0x00-0x02 are drives A/B
char iec_index_to_id(char index);
char iec_present(char id);
bool iec_scan(char *active);
bool iec_needs_switching(char state, char id);
char menuslotkey(char slotnumber);
char menuslotlabel(char slotnumber);
char keytomenuslot(char key);
bool isslotkey(char key);
char key_poll(void);
char key_wait(void);

#define DOS_STATUS_MAX      41      // DOS status message buffer
extern char DOSstatus[DOS_STATUS_MAX];

#include "petconv.h"
#include "basicexit.h"

#pragma compile("core.c")

#endif // CORE_H
