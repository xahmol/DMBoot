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


// Common functions for DMBoot Utilities

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <stdlib.h>
#include <cbm.h>
#include <errno.h>
#include <ctype.h>
#include <device.h>
#include <accelerator.h>
#include <peekpoke.h>
#include "ultimate_lib.h"
#include "defines.h"
#include "configcommon.h"

long secondsfromutc = 0; 
unsigned char timeonflag = 1;
char reufilename[20] = "default.reu";
unsigned char reusize = '2';
char* reusizelist[8] = { "128 KB","256 KB","512 KB","1 MB","2 MB","4 MB","8 MB","16 MB"};

void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen)
{       
    // Function to provide MID$ equivalent

    size_t len = min( dstlen - 1, length);
 
    strncpy(dst, src + start, len);
    // zero terminate because strncpy() didn't ? 
    if(len < length)
        dst[dstlen-1] = 0;
}

BYTE dosCommand(const BYTE lfn, const BYTE drive, const BYTE sec_addr, const char *cmd)
{
    // Function to send a DOS command

    int res;
    char DOSstatus[40];

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

void writeconfigfile(char* filename)
{
	// Function to write config file
	// Inout: filename of config file

	char buffer[32] = "";

	// Create text to write as config file, colon seperated
  sprintf(buffer,"%s:%c:%i:%ld", reufilename, reusize, timeonflag, secondsfromutc);

  // Delete old config file as I can not (yet) get overwrite to work
	uii_delete_file(filename);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_open_file(0x06,filename);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_write_file(buffer,sizeof(buffer));
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_close_file();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);
}

void readconfigfile(char* filename)
{
	// Function to read config file
	// Inout: filename of config file

	char buffer[32] = "";
  char utcoffset[10] = "";
  unsigned char indexbuffer = 0;
  unsigned char indexcopy = 0;
  unsigned char x;
  char* ptrend;

	uii_open_file(0x01,filename);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

  // Write a config file with default values if no file is found
  if(strcmp(uii_status,"00,ok") != 0)
  {
    printf("\nNo config file found, writing defaults.");
    writeconfigfile(filename);
    return;
  }

	uii_read_file(sizeof(buffer));
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_readdata();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_accept();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	strcpy(buffer, uii_data);

	uii_close_file();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

  // Obtain REU filename by getting chars until first colon
  do
  {
    reufilename[indexcopy++]=buffer[indexbuffer++];
  } while (buffer[indexbuffer] != ':');

  // Obtain REU size as first char after colon
  reusize = buffer[++indexbuffer];

  // Skip colon after REU size
  indexbuffer++;

  // Obtain next char as flag on/off for UTP time set. Substract '0' to get from ASCII to number
  timeonflag = buffer[++indexbuffer] - '0';

  // Skip colon
  indexbuffer+=2;

  // Obtain remainder als text input for UTC time offset
  for(x=0;x<strlen(buffer)-indexbuffer;x++)
  {
    utcoffset[x] = buffer[indexbuffer+x];
  }

  // Convert text of UTC time offset to long.
  secondsfromutc = strtol(utcoffset,&ptrend,10);
	
  // Debug messages. Uncomment for debug mode
  //printf("\nREU filename: %s", reufilename);
  //printf("\nREU size: %c", reusize);
  //printf("\nTime on flag: %i", timeonflag);
	//printf("\nSeconds from UTC: %s", utcoffset);
  //printf("\nConverted UTC offset: %ld",secondsfromutc);
}