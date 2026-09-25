/*
DMBoot 128 v5 - Config and slot file I/O (REU <-> Ultimate file system)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef FILEIO_H
#define FILEIO_H

#include "defines.h"

void CheckStatus(const char *message);
void get_slot_from_reu(char number);
void save_slot_to_reu(char number);
void write_slotsfile(void);
void read_slotsfile(void);
void config_defaults(void);
void writeconfigfile(void);
void readconfigfile(void);

#pragma compile("fileio.c")

#endif // FILEIO_H
