/*
DMBoot 128 v5 - Clean return to BASIC 7

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef BASICEXIT_H
#define BASICEXIT_H

void dmb_zp_save(void);
void dmb_fkeys_raw(void);
void dmb_exit(void);

#pragma compile("basicexit.c")

#endif // BASICEXIT_H
