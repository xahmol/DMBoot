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

long secondsfromutc = 7200; 
unsigned char timeonflag = 1;
char DOSstatus[40];

unsigned char CheckStatus()
{
    // Function to check UII+ status

    if (uii_status[0] != '0' || uii_status[1] != '0') {
        printf("\nStatus: %s Data:%s", uii_status, uii_data);
        return 1;
    }
    return 0;
}

BYTE dosCommand(const BYTE lfn, const BYTE drive, const BYTE sec_addr, const char *cmd)
{
    // Function to send a DOS command

    int res;
    if (cbm_open(lfn, drive, sec_addr, cmd) != 0)
      {
        return _oserror;
      }

    if (lfn != 15)
      {
        if (cbm_open(15, drive, 15, "") != 0)
          {
            cbm_close(lfn);
            return _oserror;
          }
      }

    DOSstatus[0] = 0;
    res = cbm_read(15, DOSstatus, sizeof(DOSstatus));

    if(lfn != 15)
      {
        cbm_close(15);
      }
    cbm_close(lfn);

    if (res < 1)
      {
        return _oserror;
      }

    return (DOSstatus[0] - 48) * 10 + DOSstatus[1] - 48;
}

int cmd(const BYTE device, const char *cmd)
{
    // Function to send command for disk operations

    return dosCommand(15, device, 15, cmd);
}

void std_read(char * file_name)
{
    // Function to read time config file
    // Input: file_name is the name of the config file

    FILE *file;

    _filetype = 's';
    if(file = fopen(file_name, "r"))
    {
        timeonflag = fgetc(file);
        fscanf(file,"%ld",secondsfromutc);
        fclose(file);
    }
}

void get_ntp_time()
{
    // Function to get time from NTP server and set UII+ time with this

    char host[] = "pool.ntp.org";
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
    unsigned char bootdevice = getcurrentdevice();
    
    textcolor(DC_COLOR_TEXT);

    cmd(bootdevice,"cd:/usb*/11"); 

    //std_read("dmbtimeconf");

    if(timeonflag == 1)
    {
        get_ntp_time();
    }

    clrscr();
    gotoxy(0,2);
    cprintf("run\"dmbootmain\",u%i", bootdevice);
    *((unsigned char *)KBCHARS)=13;
    *((unsigned char *)KBNUM)=1;
    gotoxy(0,0);
    exit(0);
}