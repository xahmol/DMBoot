// DMBoot 128:
// Device Manager Boot Menu for the Commodore 128
//
// DMBoot Utilities
//
// Written in 2021 by Xander Mol
// https://github.com/xahmol/DMBoot
// https://www.idreamtin8bits.com/
//
// See credits in main.c for credits to DraBrowse on main program (some code also used here).
// Credits for the utilities code:
// - ultimate-lib by xlar54 / Scott Hutter: main library to access the UII++ command interface and Ultimate DOS
//   https://github.com/xlar54/ultimateii-dos-lib
// - ntp2ultimate by MaxPlap: code for obtaining time via NTP
//   https://github.com/MaxPlap/ntp2ultimate
// - GRB128 by bvl1999 / Bart van Leeuwen: code for GEOS RAM boot
//   https://github.com/bvl1999
//
// Requires and made possible by the C128 Device Manager ROM,
// Created by Bart van Leeuwen
// https://www.bartsplace.net/content/publications/devicemanager128.shtml
//
// Requires and made possible by the Ultimate II+ cartridge,
// Created by Gideon Zweijtzer
// https://ultimate64.com/
//
// The code can be used freely as long as you retain
// a notice describing original source and author.
//
// THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
// BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!


// Functions to set time using NTP server
// Source: https://github.com/MaxPlap/ntp2ultimate

#include <stdio.h>
#include <string.h>
#include <cbm.h>
#include <conio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <device.h>
#include <time.h>
#include "ultimate_lib.h"
#include "u-time.h"
#include "defines.h"
#include "configcommon.h"

unsigned char CheckStatus()
{
    // Function to check UII+ status

    if (uii_status[0] != '0' || uii_status[1] != '0') {
        printf("\nStatus: %s Data:%s", uii_status, uii_data);
        return 1;
    }
    return 0;
}

void get_ntp_time()
{
    // Function to get time from NTP server and set UII+ time with this

    struct tm *datetime;
    extern struct _timezone _tz;

    char settime[6];
    unsigned char fullcmd[] = { 0x00, NET_CMD_SOCKET_WRITE, 0x00, \
                               0x1b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char socket = 0;
    time_t t;
    char res[32];

    printf("\nUpdating UII+ time from NTP Server.");
    uii_get_time();
    printf("\nUltimate datetime: %s", uii_data);

    printf("\nConnecting to: %s", host);
	socket = uii_udpconnect(host, 123); //https://github.com/markusC64/1541ultimate2/blob/master/software/io/network/network_target.cc
    if(CheckStatus()) return;

    printf("\nSending data");
	fullcmd[2] = socket;
    uii_settarget(TARGET_NETWORK);
    uii_sendcommand(fullcmd, 51);//3 + sizeof( ntp_packet ));
	uii_readstatus();
	uii_accept();
    if(CheckStatus()) return;

    printf("\nReading result");
    uii_socketread(socket, 50);// 2 + sizeof( ntp_packet ));
    if(CheckStatus()) return;
    uii_socketclose(socket);

    t = uii_data[37] | (((unsigned long)uii_data[36])<<8)| (((unsigned long)uii_data[35])<<16)| (((unsigned long)uii_data[34])<<24);
    t -= NTP_TIMESTAMP_DELTA;
    printf("\nUnix epoch %lu", t);
    _tz.timezone = secondsfromutc;
    datetime = localtime(&t);
    if (strftime(res, sizeof(res), "%F %H:%M:%S", datetime) == 0){
        printf("\nError cannot parse date");
        return;
    }
    printf("\nNTP datetime: %s", res);
    settime[0]=datetime->tm_year;
    settime[1]=datetime->tm_mon + 1;
    settime[2]=datetime->tm_mday;
    settime[3]=datetime->tm_hour;
    settime[4]=datetime->tm_min;
    settime[5]=datetime->tm_sec;
    uii_set_time(settime);
    printf("\nStatus: %s", uii_status);
}

void main()
{
    char configfilename[10] = "dmbcfgfile";
    unsigned char bootdevice = getcurrentdevice();

    textcolor(DC_COLOR_TEXT);
    uii_change_dir("/usb*/11/");
    printf("\n\nDir changed\nStatus: %s", uii_status);	

    readconfigfile(configfilename);

    cmd(bootdevice,"cd:/usb*/11");

    if(timeonflag == 1)
    {
        get_ntp_time();
    }

    // Uncomment for debug
    //cgetc();

    clrscr();
    gotoxy(0,2);
    cprintf("run\"dmbootmain\",u%i", bootdevice);
    *((unsigned char *)KBCHARS)=13;
    *((unsigned char *)KBNUM)=1;
    gotoxy(0,0);
    exit(0);
}