/*
DMBoot 128 v5 - UNIX time to Ultimate RTC time

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef TIMECONV_H
#define TIMECONV_H

#define UII_TIME_BYTES      6       // year - 1900, month, day, hour, minute, second

void epoch_to_uiitime(unsigned long epoch, long offset, char *uiitime);

#pragma compile("timeconv.c")

#endif // TIMECONV_H
