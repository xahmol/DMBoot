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
char reufilepath[60] = "/usb*/11/";
char imageaname[20] = "";
char imageapath[60] = "";
unsigned char imageaid = 0;
char imagebname[20] = "";
char imagebpath[60] = "";
unsigned char imagebid = 0;
unsigned char reusize = 2;
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

	unsigned char buffer[248];
  unsigned char x;

  // Clear buffer memory
  memset(buffer,0,248);

	// Place all variables in buffer memory
  for(x=0;x<60;x++)
  {
    buffer[x]=reufilepath[x];
  }

  for(x=0;x<20;x++)
  {
    buffer[x+60]=reufilename[x];
  }
  for(x=0;x<60;x++)
  {
    buffer[x+80]=imageapath[x];
  }
  for(x=0;x<20;x++)
  {
    buffer[x+140]=imageaname[x];
  }
  for(x=0;x<60;x++)
  {
    buffer[x+160]=imagebpath[x];
  }
  for(x=0;x<20;x++)
  {
    buffer[x+220]=imagebname[x];
  }
  buffer[240] = reusize;
  buffer[241] = timeonflag;

  buffer[242] = (secondsfromutc & 0xFF000000) >> 24;
  buffer[243] = (secondsfromutc & 0xFF0000) >> 16;
  buffer[244] = (secondsfromutc & 0xFF00) >> 8;
  buffer[245] = secondsfromutc & 0xFF;

  buffer[246] = imageaid;
  buffer[247] = imagebid;
  
  // Delete old config file as I can not (yet) get overwrite to work
	uii_delete_file(filename);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_open_file(0x06,filename);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_write_file(buffer,248);
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

  unsigned char x;

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

	uii_read_file(248);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_readdata();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_accept();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

  // Read variables from read data
  for(x=0;x<60;x++)
  {
    reufilepath[x]=uii_data[x];
  }
  for(x=0;x<20;x++)
  {
    reufilename[x]=uii_data[x+60];
  }
  for(x=0;x<60;x++)
  {
    imageapath[x]=uii_data[x+80];
  }
  for(x=0;x<20;x++)
  {
    imageaname[x]=uii_data[x+140];
  }
  for(x=0;x<60;x++)
  {
    imagebpath[x]=uii_data[x+160];
  }
  for(x=0;x<20;x++)
  {
    imagebname[x]=uii_data[x+220];
  }
  
  reusize = uii_data[240];

  timeonflag = uii_data[241];
  
  secondsfromutc = uii_data[245] | (((unsigned long)uii_data[244])<<8)| (((unsigned long)uii_data[243])<<16)| (((unsigned long)uii_data[242])<<24);

  imageaid = uii_data[246];
  imagebid = uii_data[247];

	uii_close_file();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);
	
  // Debug messages. Uncomment for debug mode
  //printf("\nREU file path+name: %s%s", reufilepath, reufilename);
  //printf("\nImage a ID: %i",imageaid);
  //printf("\nImage b ID: %i",imagebid);
  //printf("\nImage a path+name: %s%s", imageapath, imageaname);
  //printf("\nImage b path+name: %s%s", imagebpath, imagebname);
  //printf("\nREU size: %i", reusize);
  //printf("\nTime on flag: %i", timeonflag);
  //printf("\nConverted UTC offset: %ld",secondsfromutc);
  //cgetc();
}