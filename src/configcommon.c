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
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "defines.h"
#include "configcommon.h"
#include "ops.h"
#include "main.h"

#pragma code-name ("OVERLAY3");
#pragma rodata-name ("OVERLAY3");

void writeconfigfile(char* filename)
{
	// Function to write config file
	// Inout: filename of config file

  unsigned char x;

  // Clear buffer memory
  memset(utilbuffer,0,328);

	// Place all variables in buffer memory
  for(x=0;x<60;x++)
  {
    utilbuffer[x]=reufilepath[x];
  }

  for(x=0;x<20;x++)
  {
    utilbuffer[x+60]=imagename[x];
  }
  for(x=0;x<60;x++)
  {
    utilbuffer[x+80]=imageapath[x];
  }
  for(x=0;x<20;x++)
  {
    utilbuffer[x+140]=imageaname[x];
  }
  for(x=0;x<60;x++)
  {
    utilbuffer[x+160]=imagebpath[x];
  }
  for(x=0;x<20;x++)
  {
    utilbuffer[x+220]=imagebname[x];
  }
  utilbuffer[240] = reusize;
  utilbuffer[241] = timeonflag;

  utilbuffer[242] = (secondsfromutc & 0xFF000000) >> 24;
  utilbuffer[243] = (secondsfromutc & 0xFF0000) >> 16;
  utilbuffer[244] = (secondsfromutc & 0xFF00) >> 8;
  utilbuffer[245] = secondsfromutc & 0xFF;

  utilbuffer[246] = imageaid;
  utilbuffer[247] = imagebid;
  for(x=0;x<80;x++)
  {
    utilbuffer[248+x] = host[x];
  }
  
  // Delete old config file as I can not (yet) get overwrite to work
	uii_delete_file(filename);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_open_file(0x06,filename);
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);

	uii_write_file(utilbuffer,328);
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
  if(strcmp((const char*)uii_status,"00,ok") != 0)
  {
    printf("\nNo config file found, writing defaults.");
    writeconfigfile(filename);
    return;
  }

	uii_read_file(328);
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
    imagename[x]=uii_data[x+60];
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

  for(x=0;x<80;x++)
  {
    host[x] = uii_data[248+x];
  }

  // If no hostname is read due to old config file format, set default
  if(strlen(host)==0) { strcpy(host,"pool.ntp.org"); }

	uii_close_file();
  // Uncomment for debbug
  //printf("\nStatus: %s", uii_status);
	
  // Debug messages. Uncomment for debug mode
  //printf("\nREU file path+name: %s%s", reufilepath, imagename);
  //printf("\nImage a ID: %i",imageaid);
  //printf("\nImage b ID: %i",imagebid);
  //printf("\nImage a path+name: %s%s", imageapath, imageaname);
  //printf("\nImage b path+name: %s%s", imagebpath, imagebname);
  //printf("\nREU size: %i", reusize);
  //printf("\nTime on flag: %i", timeonflag);
  //printf("\nConverted UTC offset: %ld",secondsfromutc);
  //printf("\nNTP Hostname: %s",host);
  //cgetc();
}