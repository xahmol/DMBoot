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


// Configuration editor for DMBoot Utilities

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
#include "ultimate_time_lib.h"
#include "defines.h"
#include "version.h"
#include "configcommon.h"
#include "main.h"
#include "ops.h"
#include "screen.h"

#pragma code-name ("OVERLAY3");
#pragma rodata-name ("OVERLAY3");

unsigned char changesmade = 0;

void config_headertext(char* subtitle)
{
    // Draw header text
    // Input: subtitle is text to draw on second line

    revers(1);
    textcolor(DMB_COLOR_HEADER1);
    gotoxy(0,0);
    cspaces(SCREENW);
    gotoxy(0,0);  
    cprintf("DMBoot 128: Device Manager Boot Menu");
    textcolor(DMB_COLOR_HEADER2);
    gotoxy(0,1);
    cspaces(SCREENW);
    gotoxy(0,1);
    cprintf("%s\n\n\r", subtitle);
    if(SCREENW == 80)
    {
        uii_get_time();
        cputsxy(80-strlen((const char*)uii_data),1,(const char*)uii_data);
    }
    revers(0);
    textcolor(DC_COLOR_TEXT);
}

char config_mainmenu()
{
    // Draw main boot menu
    // Returns chosen menu option as char key value

    unsigned char key;
    char buffer1[80] = "";
    char buffer2[80] = "";

    clrscr();
    config_headertext("Configuration tool.");

    gotoxy(0,3);
    cputs("Present configuration settings:\n\n\r");
    cputs("NTP time update settings:\n\r");
    cprintf("- Update on boot toggle: %s\n\r",(timeonflag==0)?"Off":"On");
    cprintf("- Offset to UTC in seconds: %ld\n\r",secondsfromutc);

    mid(host,0,SCREENW,buffer2,SCREENW);
    cprintf("- NTP server hostname:\n\r%s\n\r",buffer2);

    cputs("GEOS RAM Boot settings:\n\r");

    strcpy(buffer1,reufilepath);
    strcat(buffer1,imagename);
    mid(buffer1,0,SCREENW,buffer2,SCREENW);
    cprintf("- REU filepath + name:\n\r%s\n\r",buffer2);
    cprintf("- REU file size: (%i) %s\n\r",reusize,reusizelist[reusize]);

    strcpy(buffer1,imageapath);
    strcat(buffer1,imageaname);
    mid(buffer1,0,SCREENW,buffer2,SCREENW);
    cprintf("- Drive A image path+name: %i\n\r%s\n\r",imageaid, buffer2);

    strcpy(buffer1,imagebpath);
    strcat(buffer1,imagebname);
    mid(buffer1,0,SCREENW,buffer2,SCREENW);
    cprintf("- Drive B image path+name: %i\n\r%s\n\r",imagebid, buffer2);

    cputs("Make your choice:\n\r");
    
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F1 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Edit NTP time configuration\n\r");

    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F3 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Edit GEOS RAM boot configuration\n\r");

    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F7 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Quit configuration tool\n\r");

    do
    {
        key = cgetc();
    } while (key != CH_F1 && key != CH_F3 && key != CH_F7);
    return key;    
}

void edittimeconfig()
{
    unsigned char key;
    char offsetinput[10] = "";
    char buffer2[80] = "";
    char* ptrend;

    clearArea(0,19,SCREENW,3);
    gotoxy(0,19);

    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F1 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Toggle update on boot on/off\n\r");

    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F3 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Edit time offset to UTC\n\r");

    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F5 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Edit NTP server host\n\r");

    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F7 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Back to main menu\n\r");

    do
    {
        do
        {
            key = cgetc();
        } while (key != CH_F1 && key != CH_F3 && key != CH_F5 && key != CH_F7);

        switch (key)
        {
        case CH_F1:
            timeonflag = (timeonflag==0)? 1:0;
            gotoxy(0,6);
            cprintf("- Update on boot toggle: %s\n\r",(timeonflag==0)?"Off":"On ");
            changesmade = 1;
            break;

        case CH_F3:
            cputsxy(0,23,"Input time offset to UTC:");
            textInput(0,24,offsetinput,10);
            secondsfromutc = strtol(offsetinput,&ptrend,10);
            clearArea(0,7,SCREENW,1);
            clearArea(0,23,SCREENW,2);
            gotoxy(0,7);
            cprintf("- Offset to UTC in seconds: %ld\n\n\r",secondsfromutc);
            changesmade = 1;
            break;

        case CH_F5:
            cputsxy(0,23,"Input NTP server hostname:");
            textInput(0,24,host,79);
            clearArea(0,9,SCREENW,1);
            clearArea(0,23,SCREENW,2);
            gotoxy(0,9);
            mid(host,0,SCREENW,buffer2,SCREENW);
            cprintf("%s",buffer2);
            changesmade = 1;
            break;

        default:
	    	break;
        }
    } while (key != CH_F7);
}

void editgeosconfig()
{
    unsigned char key, plusmin;
    char deviceidbuffer[3];
    char* ptrend;

    do
    {
        clrscr();
        config_headertext("Edit GEOS RAM Boot configuration.");

        gotoxy(0,3);

        cputs("Make your choice:\n\r");

        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cputs(" F1 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Edit REU file path and name\n\r");

        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cputs(" F3 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Edit REU size\n\r");

        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cputs(" F5 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Edit images to mount\n\r");

        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cputs(" F7 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Back to main menu\n\r");

        do
        {
            key = cgetc();
        } while (key != CH_F1 && key != CH_F3 && key != CH_F5 && key != CH_F7);

        switch (key)
        {
        case CH_F1:
            clrscr();
            config_headertext("Edit REU file path and name.");

            cputsxy(0,3,"Enter REU file path:");
            textInput(0,4,reufilepath,59);

            cputsxy(0,7,"Enter REU file name:");
            textInput(0,8,imagename,19);
            changesmade = 1;
            break;

        case CH_F3:
            clrscr();
            config_headertext("Edit REU size.");

            gotoxy(0,3);
            cprintf("REU file size: (%i) %s\n\n\r",reusize,reusizelist[reusize]);

            cputs("Make your choice:\n\r");

            revers(1);
            textcolor(DMB_COLOR_SELECT);
            cputs("  + ");
            revers(0);
            textcolor(DC_COLOR_TEXT);
            cputs(" Increase REU size\n\r");

            revers(1);
            textcolor(DMB_COLOR_SELECT);
            cputs("  - ");
            revers(0);
            textcolor(DC_COLOR_TEXT);
            cputs(" Decrease REU size\n\r");

            revers(1);
            textcolor(DMB_COLOR_SELECT);
            cputs(" F7 ");
            revers(0);
            textcolor(DC_COLOR_TEXT);
            cputs(" Back to previous menu\n\r");

            do
            {
              do
              {
                plusmin = cgetc();
              } while (plusmin != '+' && plusmin != '-' && plusmin != CH_F7);
  
              if(plusmin == '+')
              {
                  reusize++;
                  if(reusize > 7) { reusize = 0; }
                  changesmade = 1;
              }
              if(plusmin == '-')
              {
                  if(reusize == 0) { reusize = 7; }
                  else { reusize--; }
                  changesmade = 1;               
              }
              gotoxy(0,3);
              cprintf("REU file size: (%i) %s  ",reusize,reusizelist[reusize]);
            } while (plusmin != CH_F7);         
            break;

        case CH_F5:
            clrscr();
            config_headertext("Edit images to mount.");

            sprintf(deviceidbuffer,"%i",imageaid);
            cputsxy(0,3,"Enter image drive A device ID:");
            textInput(0,4,deviceidbuffer,2);
            imageaid = (unsigned char)strtol(deviceidbuffer,&ptrend,10);

            cputsxy(0,5,"Enter image drive A file path:");
            textInput(0,6,imageapath,59);

            cputsxy(0,8,"Enter image drive A file name:");
            textInput(0,9,imageaname,19);

            sprintf(deviceidbuffer,"%i",imagebid);
            cputsxy(0,10,"Enter image drive B device ID:");
            textInput(0,11,deviceidbuffer,2);
            imagebid = (unsigned char)strtol(deviceidbuffer,&ptrend,10);

            cputsxy(0,12,"Enter image drive B file path:");
            textInput(0,13,imagebpath,59);

            cputsxy(0,15,"Enter image drive B file name:");
            textInput(0,16,imagebname,19);

            changesmade = 1;

            break;

        default:
	    	  break;

        }
    } while (key != CH_F7);
}

void information()
{
    // Routine for version information and credits

    clrscr();
    config_headertext("Information and credits");

    cputs("\n\rDMBoot 128:\n\r");
    cputs("Device Manager Boot Menu for the C128\n\r");
    cprintf("%dKB VDC memory detected.\n\n\r",vdcmemory);
    cprintf("Version: v%i%i-", VERSION_MAJOR, VERSION_MINOR);
    cprintf("%c%c%c%c", BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3);
    cprintf("%c%c%c%c-", BUILD_MONTH_CH0, BUILD_MONTH_CH1, BUILD_DAY_CH0, BUILD_DAY_CH1);
    cprintf("%c%c%c%c\n\r", BUILD_HOUR_CH0, BUILD_HOUR_CH1, BUILD_MIN_CH0, BUILD_MIN_CH1);
    cputs("Written 2020-2023 by Xander Mol.\n\n\r");
    cputs("Based on DraBrowse:\n\r");
    cputs("DraBrowse is a simple file browser.\n\r");
    cputs("Original 2009 by Sascha Bader.\n\r");
    cputs("Used version adapted by Dirk Jagdmann.\n\n\r");
    cputs("Requires and made possible by:\n\n\r");
    cputs("The C128 Device Manager ROM,\n\r");
    cputs("Created by Bart van Leeuwen.\n\n\r");
    cputs("The Ultimate II+ cartridge,\n\r");
    cputs("Created by Gideon Zweijtzer.\n\n\r");

    cputs("Press a key to continue.");

    getkey(2);    
}

void config_main(void)
{
    int menuselect;
    
    clrscr();

    do
    {
        menuselect = config_mainmenu();

        switch (menuselect)
        {
        case CH_F1:
            edittimeconfig();
            break;
        
        case CH_F3:
            editgeosconfig();
            break;
        
        default:
            break;
        }
    } while (menuselect != CH_F7);

    if (changesmade == 1)
    {
        writeconfigfile(configfilename);
    }
}