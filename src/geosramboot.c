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


// GEOS RAM Boot utility

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <stdlib.h>
#include "ultimate_lib.h"
#include "defines.h"
#include "geosramboot.h"
#include "configcommon.h"

void main()
{
	char configfilename[10] = "dmbcfgfile";

    textcolor(DC_COLOR_TEXT);

	uii_change_dir("/usb*/11/");
	printf("\nDir changed\nStatus: %s", uii_status);	

	readconfigfile(configfilename);

	// Load REU file
	uii_change_dir(reufilepath);
	printf("\nREU Dir changed\nStatus: %s", uii_status);

	// Exit if REU dir is not found
	if(strcmp((const char*)uii_status,"00,ok") != 0)
	{
		printf("\nREU path not found.");
		return;
	}	

    printf("\nOpen REU file");
	uii_open_file(1, reufilename);
	printf("\nStatus: %s", uii_status);

	// Exit if REU file is not found
	if(strcmp((const char*)uii_status,"00,ok") != 0)
	{
		printf("\nREU file not found.");
		return;
	}

	printf("\nLoad REU file");
	uii_load_reu(reusize);
	printf("\nData: %s\nStatus: %s", uii_data, uii_status);

	printf("\nClose REU file");
	uii_close_file();
	printf("\nStatus: %s", uii_status);

	// Load drive A image
	if(imageaid>0)
	{
		uii_change_dir(imageapath);
		printf("\nImage A Dir changed\nStatus: %s", uii_status);

		// Exit if REU dir is not found
		if(strcmp((const char*)uii_status,"00,ok") != 0)
		{
			printf("\nImage A path not found.");
			return;
		}

		uii_mount_disk(imageaid,imageaname);
		printf("\nImage A loaded\nStatus: %s", uii_status);

		// Exit if image A is not found
		if(strcmp((const char*)uii_status,"00,ok") != 0)
		{
			printf("\nImage A file not found.");
			return;
		}
	}
	
	// Load drive B image
	if(imagebid>0)
	{
		uii_change_dir(imagebpath);
		printf("\nImage B Dir changed\nStatus: %s", uii_status);

		// Exit if REU dir is not found
		if(strcmp((const char*)uii_status,"00,ok") != 0)
		{
			printf("\nImage B path not found.");
			return;
		}

		uii_mount_disk(imagebid,imagebname);
		printf("\nImage B loaded\nStatus: %s", uii_status);

		// Exit if image A is not found
		if(strcmp((const char*)uii_status,"00,ok") != 0)
		{
			printf("\nImage B file not found.");
			return;
		}
	}

	printf("\nBooting GEOS.");
    startgeos();
}