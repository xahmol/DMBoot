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
#include "ultimate_lib.h"
#include "defines.h"
#include "configcommon.h"

unsigned int SCREENW;
char spaces[81]    = "                                                                                ";
char spacedest[81];
BYTE bootdevice;
unsigned char changesmade = 0;

void clearArea(const BYTE xpos, const BYTE ypos, const BYTE xsize, const BYTE ysize)
{
  BYTE y = ypos;
  for (; y < (ypos+ysize); ++y)
    {
      cclearxy(xpos,y,xsize);
    }
}

int textInput(const BYTE xpos, const BYTE ypos, char *str, const BYTE size)
{
  register BYTE idx = strlen(str);
  register BYTE c;

  cursor(1);
  cputsxy(xpos, ypos, str);

  while(1)
    {
      c = cgetc();
      switch (c)
        {
      case CH_LARROW:
      case CH_ESC:
        cursor(0);
        return -1;

      case CH_ENTER:
        idx = strlen(str);
        str[idx] = 0;
        cursor(0);
        return idx;

      case CH_DEL:
        if (idx)
          {
            --idx;
            cputcxy(xpos + idx, ypos, ' ');
            for(c = idx; 1; ++c)
              {
                const BYTE b = str[c+1];
                str[c] = b;
                cputcxy(xpos + c, ypos, b ? b : ' ');
                if (b == 0)
                  break;
              }
            gotoxy(xpos + idx, ypos);
          }
        break;

        case CH_INS:
          c = strlen(str);
          if (c < size &&
              c > 0 &&
              idx < c)
            {
              ++c;
              while(c >= idx)
                {
                  str[c+1] = str[c];
                  if (c == 0)
                    break;
                  --c;
                }
              str[idx] = ' ';
              cputsxy(xpos, ypos, str);
              gotoxy(xpos + idx, ypos);
            }
          break;

      case CH_CURS_LEFT:
        if (idx)
          {
            --idx;
            gotoxy(xpos + idx, ypos);
          }
        break;

      case CH_CURS_RIGHT:
        if (idx < strlen(str) &&
            idx < size)
          {
            ++idx;
            gotoxy(xpos + idx, ypos);
          }
        break;

      default:
        if (isprint(c) &&
            idx < size)
          {
            const BYTE flag = (str[idx] == 0);
            str[idx] = c;
            cputc(c);
            ++idx;
            if (flag)
              str[idx+1] = 0;
            break;
          }
        break;
      }
    }
  return 0;
}

void headertext(char* subtitle)
{
    // Draw header text
    // Input: subtitle is text to draw on second line

    mid(spaces,1,SCREENW,spacedest, sizeof(spacedest)); // select spaces based on screenwidth
    revers(1);
    textcolor(DMB_COLOR_HEADER1);
    gotoxy(0,0);
    cprintf("%s\n",spacedest);
    gotoxy(0,0);  
    cprintf("DMBoot 128: Device Manager Boot Menu");
    textcolor(DMB_COLOR_HEADER2);
    gotoxy(0,1);
    cprintf("%s\n",spacedest);
    gotoxy(0,1);
    cprintf("%s\n\n\r", subtitle);
    revers(0);
    textcolor(DC_COLOR_TEXT);
}

char mainmenu()
{
    // Draw main boot menu
    // Returns chosen menu option as char key value

    unsigned char key;
    char buffer1[80] = "";
    char buffer2[80] = "";

    clrscr();
    headertext("Configuration tool.");

    gotoxy(0,3);
    cputs("Present configuration settings:\n\n\r");
    cputs("NTP time update settings:\n\r");
    cprintf("- Update on boot toggle: %s\n\r",(timeonflag==0)?"Off":"On");
    cprintf("- Offset to UTC in seconds: %ld\n\r",secondsfromutc);

    mid(host,0,SCREENW,buffer2,SCREENW);
    cprintf("- NTP server hostname:\n\r%s\n\r",buffer2);

    cputs("GEOS RAM Boot settings:\n\r");

    strcpy(buffer1,reufilepath);
    strcat(buffer1,reufilename);
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

    clearArea(0,19,40,3);
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
            clearArea(0,7,40,1);
            clearArea(0,23,40,2);
            gotoxy(0,7);
            cprintf("- Offset to UTC in seconds: %ld\n\n\r",secondsfromutc);
            changesmade = 1;
            break;

        case CH_F5:
            cputsxy(0,23,"Input NTP server hostname:");
            textInput(0,24,host,80);
            clearArea(0,9,40,1);
            clearArea(0,23,40,2);
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
        headertext("Edit GEOS RAM Boot configuration.");

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
            headertext("Edit REU file path and name.");

            cputsxy(0,3,"Enter REU file path:");
            textInput(0,4,reufilepath,60);

            cputsxy(0,7,"Enter REU file name:");
            textInput(0,8,reufilename,20);
            changesmade = 1;
            break;

        case CH_F3:
            clrscr();
            headertext("Edit REU size.");

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
            headertext("Edit images to mount.");

            sprintf(deviceidbuffer,"%i",imageaid);
            cputsxy(0,3,"Enter image drive A device ID:");
            textInput(0,4,deviceidbuffer,2);
            imageaid = (unsigned char)strtol(deviceidbuffer,&ptrend,10);

            cputsxy(0,5,"Enter image drive A file path:");
            textInput(0,6,imageapath,60);

            cputsxy(0,8,"Enter image drive A file name:");
            textInput(0,9,imageaname,20);

            sprintf(deviceidbuffer,"%i",imagebid);
            cputsxy(0,10,"Enter image drive B device ID:");
            textInput(0,11,deviceidbuffer,2);
            imagebid = (unsigned char)strtol(deviceidbuffer,&ptrend,10);

            cputsxy(0,12,"Enter image drive B file path:");
            textInput(0,13,imagebpath,60);

            cputsxy(0,15,"Enter image drive B file name:");
            textInput(0,16,imagebname,20);

            changesmade = 1;

            break;

        default:
	    	  break;

        }
    } while (key != CH_F7);
}

void main(void)
{
    char cfgfilename[10] = "dmbcfgfile";
    int menuselect;

    bootdevice = getcurrentdevice();
    
    //Check column width of present screen
    if ( PEEK(0xee) == 79) //Memory position $ee is present screen width
    {
        SCREENW = 80;  //Set flag for 80 column
        set_c128_speed(SPEED_FAST);
    }
    else
    {
        SCREENW = 40;  //Set flag for 40 column
    }

    textcolor(DC_COLOR_TEXT);

    cputs("Starting: Reading config file.");  

	  uii_change_dir("/usb*/11/");
    readconfigfile(cfgfilename);

    bordercolor(DC_COLOR_BORDER);
    bgcolor(DC_COLOR_BG);
    clrscr();

    do
    {
        menuselect = mainmenu();

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
        writeconfigfile(cfgfilename);
    }
    
    cmd(bootdevice,"cd:/usb*/11");

    clrscr();
    gotoxy(0,2);
    cprintf("run\"dmbootmain\",u%i", bootdevice);
    *((unsigned char *)KBCHARS)=13;
    *((unsigned char *)KBNUM)=1;
    gotoxy(0,0);
    exit(0);
}