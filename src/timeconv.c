/*
DMBoot 128 v5 - UNIX time to Ultimate RTC time

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Used by the NTP time update (overlay 4) and tested on the PC (tests/host).

Code and resources from others used:
-   EPOCH-to-time-date-converter by sidsingh78
    https://github.com/sidsingh78/EPOCH-to-time-date-converter/blob/master/epoch_conv.c
    Epoch to date conversion. Adapted: whole years and months are
    subtracted in loops instead of estimating the year first.
*/

#include "timeconv.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

#define SECONDS_PER_MINUTE  60
#define MINUTES_PER_HOUR    60
#define HOURS_PER_DAY       24
#define EPOCH_YEAR          1970
#define DAYS_PER_YEAR       365
#define UII_YEAR_BASE       1900

// ---------------------------------------------------------------------------
// Title:       Leap year
// Description: Tells whether a year is a leap year.
// Syntax:      static bool leapyear(unsigned year);
// Input:       year - year (e.g. 2026)
// Output:      true for a leap year
// ---------------------------------------------------------------------------
static bool leapyear(unsigned year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

// ---------------------------------------------------------------------------
// Title:       UNIX time to Ultimate time
// Description: Converts a UNIX epoch (plus the configured UTC offset) to
//              the Ultimate RTC format: year - 1900, month, day, hour,
//              minute, second.
// Syntax:      void epoch_to_uiitime(unsigned long epoch, long offset,
//                                char *uiitime);
// Input:       epoch   - seconds since 1970-01-01 00:00 UTC
//              offset  - seconds to add (time zone)
//              uiitime - UII_TIME_BYTES bytes
// Output:      uiitime
// ---------------------------------------------------------------------------
void epoch_to_uiitime(unsigned long epoch, long offset, char *uiitime)
{
    static const char monthdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    unsigned days;
    unsigned year = EPOCH_YEAR;
    char month = 0;

    epoch += offset;
    uiitime[5] = epoch % SECONDS_PER_MINUTE;
    epoch /= SECONDS_PER_MINUTE;
    uiitime[4] = epoch % MINUTES_PER_HOUR;
    epoch /= MINUTES_PER_HOUR;
    uiitime[3] = epoch % HOURS_PER_DAY;
    days = epoch / HOURS_PER_DAY;

    // Whole years, then whole months
    while (days >= (leapyear(year) ? DAYS_PER_YEAR + 1 : DAYS_PER_YEAR))
    {
        days -= leapyear(year) ? DAYS_PER_YEAR + 1 : DAYS_PER_YEAR;
        year++;
    }
    while (month < 11)
    {
        char length = monthdays[month] + ((month == 1 && leapyear(year)) ? 1 : 0);
        if (days < length)
        {
            break;
        }
        days -= length;
        month++;
    }

    uiitime[0] = year - UII_YEAR_BASE;
    uiitime[1] = month + 1;
    uiitime[2] = days + 1;
}

