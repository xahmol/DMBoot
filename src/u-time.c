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
#include "ultimate_common_lib.h"
#include "ultimate_time_lib.h"
#include "ultimate_network_lib.h"
#include "u-time.h"
#include "defines.h"
#include "configcommon.h"
#include "ops.h"

#pragma code-name ("OVERLAY3");
#pragma rodata-name ("OVERLAY3");

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
    unsigned char attempt = 1;
    unsigned char clock;
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
    if(CheckStatus()) {
        uii_socketclose(socket);
        return;
    }

    printf("\nSending NTP request");
	fullcmd[2] = socket;
    uii_settarget(TARGET_NETWORK);
    uii_sendcommand(fullcmd, 51);//3 + sizeof( ntp_packet ));
	uii_readstatus();
	uii_accept();
    if(CheckStatus()) {
        uii_socketclose(socket);
        return;
    }

    // Do maximum of 4 attempts at receiving data
    do
    {
        // Add delay of a second to avoid time to wait on response being too short
        clock = cia_seconds;
        while (cia_seconds == clock) { ; }

        // Print attempt number
        printf("\nReading result attempt %d",attempt);

        // Try to read incoming data        
        uii_socketread(socket, 50);// 2 + sizeof( ntp_packet ));

        // If data received, end loop. Else do new attempt till counter = 5
        if(uii_success()) { 
            attempt = 5;
        } else {
            attempt++;
        }

    } while (attempt<5);
        
    if(CheckStatus()) {
        uii_socketclose(socket);
        return;
    }

    // Convert time received to UCI format
    t = uii_data[37] | (((unsigned long)uii_data[36])<<8)| (((unsigned long)uii_data[35])<<16)| (((unsigned long)uii_data[34])<<24);
    t -= NTP_TIMESTAMP_DELTA;
    
    // Close socket
    uii_socketclose(socket);

    // Print time received and parse to UII+ format
    printf("\nUnix epoch %lu", t);
    _tz.timezone = secondsfromutc;
    datetime = localtime(&t);
    if (strftime(res, sizeof(res), "%F %H:%M:%S", datetime) == 0){
        printf("\nError cannot parse date");
        return;
    }
    printf("\nNTP datetime: %s", res);

    // Set UII+ RTC clock
    settime[0]=datetime->tm_year;
    settime[1]=datetime->tm_mon + 1;
    settime[2]=datetime->tm_mday;
    settime[3]=datetime->tm_hour;
    settime[4]=datetime->tm_min;
    settime[5]=datetime->tm_sec;
    uii_set_time(settime);
    printf("\nStatus: %s", uii_status);
}

void time_main()
{
    if(timeonflag == 1)
    {
        get_ntp_time();
    }

    // Uncomment for debug
    //cgetc();
}