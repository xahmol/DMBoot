// DMBoot 128:
// Device Manager Boot Menu for the Commodore 128
// Written in 2020 by Xander Mol
// https://github.com/xahmol/DMBoot
// https://www.idreamtin8bits.com/
//
// Based on DraBrowse:
// DraBrowse (db*) is a simple file browser.
// Originally created 2009 by Sascha Bader.
// Used version adapted by Dirk Jagdmann (doj)
// https://github.com/doj/dracopy
//
// Uses code from:
// Ultimate 64/II+ Command Library
// Scott Hutter, Francesco Sblendorio
// https://github.com/xlar54/ultimateii-dos-lib
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

//Includes
#include "screen.h"
#include "version.h"
#include "base.h"
#include <cbm.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include "ops.h"
#include "screen.h"
#include "version.h"
#include "base.h"
#include "main.h"
#include "dmapi.h"
#include "vdc.h"

const char *value2hex = "0123456789abcdef";
const char *reg_types[] = { "SEQ","PRG","URS","REL","VRP" };
const char *oth_types[] = { "DEL","CBM","DIR","LNK","OTH","HDR"};
char bad_type[4];

BYTE device = 8;
char linebuffer[81];
char linebuffer2[81];
char DOSstatus[40];

/// string descriptions of enum drive_e
const char* drivetype[LAST_DRIVE_E] = {"", "Pi1541", "1540", "1541", "1551", "1570", "1571", "1581", "1001", "2031", "8040", "sd2iec", "cmd", "vice", "u64"};/// enum drive_e value for each device 0-19.
BYTE devicetype[MAXDEVID+1];

const char* getDeviceType(const BYTE device)
{
  BYTE idx;

  idx = dm_getdevicetype(device);
  
  if(idx != 0)
  {
    devicetype[device] = idx;
    return drivetype[idx];
  }

  if (device > sizeof(devicetype))
    {
      return "!d";
    }
  idx = cmd(device, "ui");
  if (idx != 73)
    {
      linebuffer2[0] = 'Q';
      linebuffer2[1] = value2hex[idx >> 4];
      linebuffer2[2] = value2hex[idx & 15];
      linebuffer2[3] = 0;
      return linebuffer2;
    }
  for(idx = 1; idx < LAST_DRIVE_E; ++idx)
    {
      if(strstr(DOSstatus, drivetype[idx]))
        {
          devicetype[device] = idx;
          return drivetype[idx];
        }
    }
  return "!n";
}

BYTE
dosCommand(const BYTE lfn, const BYTE drive, const BYTE sec_addr, const char *cmd)
{
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

int
cmd(const BYTE device, const char *cmd)
{
  return dosCommand(15, device, 15, cmd);
}

void
execute(char * prg, BYTE device, BYTE boot, char * command)
{
  // Routine to execute or boot chosen file or dir
  // Input:
  // prg:     Filename
  // device:  device number
  // boot:    Execute flag
  //          bit 0: Run from mount
  //          bit 1: Force 8
  //          bit 2: Run 64
  //          bit 3: Fast
  //          bit 4: Boot
  // command: User defined command to be executed before execution.
  //          Empty is no command.

  int ypos = 2;
  int numberenter =1;
  int x;
  
  getDeviceType(device);  // Recognise drive type of device
  
  exitScreen(); // prepare the screen with the basic command to load the next program

  gotoxy(0,ypos);

  if (strlen(command) != 0)
  {
    cprintf("%s", command);
    ypos = ypos + (strlen(command)/SCREENW) + 3;
    gotoxy(0,ypos);
    numberenter++;
  }

  if(boot & EXEC_RUN64) {
    // Run in C65 mode
    if(dm_apipresent==1 && dm_apiversion>1)
    {
      // Set filename
	    strcpy(dm_prgnam,prg);
      // Set filename length
      dm_prglen = strlen(prg);
      // Set file drive ID
      dm_devid = device;
      // Print call to start in 64 mode function
      cprintf("sys %i",&dm_run64);
    }
  } else {
    // Force 8 mode
    if(boot & EXEC_FRC8) {
      if(dm_apipresent==1 && dm_apiversion>0)
      {
        dm_sethsidviaapi();
      }
      else
      {
        cputs("poke 673,8");
        gotoxy(0,ypos+3);
        numberenter++;
      }
      device = 8;
    }

    // Fast mode
    if(boot & EXEC_FAST) {
      cprintf("fast:");
    }

    // Boot or just run
    if(boot & EXEC_BOOT) {
      cprintf("boot u%i", device);
    } else {
      cprintf("run\"%s\",u%i", prg, device);
    }
  }

  // put CRs in keyboard buffer
  for(x=0;x<numberenter;x++)
  {
    *((unsigned char *)KBCHARS+x)=13;
  }
  *((unsigned char *)KBNUM)=numberenter;
    
  // exit DraCopy, which will execute the BASIC LOAD above
  gotoxy(0,0);
  exit(0);
}

const char*
fileTypeToStr(BYTE ft)
{
  if (ft & _CBM_T_REG)
    {
      ft &= ~_CBM_T_REG;
      if (ft <= 4)
        return reg_types[ft];
    }
  else
    {
      if (ft <= 5)
        return oth_types[ft];
    }
  bad_type[0] = '?';
  bad_type[1] = value2hex[ft >> 4];
  bad_type[2] = value2hex[ft & 15];
  bad_type[3] = 0;
  return bad_type;
}

void drawDirFrame()
{
  unsigned char length;

  clearArea(0,3,DIRW,22);

  gotoxy(0,3);
  cprintf("[%02i] %s",device,cwd.name);

  gotoxy(0,24);
  cprintf("(%s) %u blocks free",drivetype[devicetype[device]],cwd.free);

  if(trace) {
    strcpy((char*)utilbuffer,pathconcat());
    length = strlen((char*)utilbuffer);
    if(length > DIRW) {
      strcpy(linebuffer,(char*)utilbuffer+length-DIRW);
    } else {
      strcpy(linebuffer,(char*)utilbuffer);
    }
  } else {
    strcpy(linebuffer,"No dirtrace active.");
  }
  gotoxy(0,4);
  cputs(linebuffer);
}

void showDir()
{
  drawDirFrame();
  printDir();
}

void clrDir()
{
  clearArea(0,5,DIRW,19);
}

void CheckMounttype(const char *dirname) {
  register BYTE l = strlen(dirname);

  mountflag = 0;
  
  if(dirname) {
      if (l > 4 && dirname[l-4] == '.')
      {
        if ((dirname[l-3] == 'd' || dirname[l-3] == 'D') &&
            (dirname[l-2] == '6') &&
            (dirname[l-1] == '4'))
          {
            mountflag = 1;
          }
        if ((dirname[l-3] == 'g' || dirname[l-3] == 'G') &&
          (dirname[l-2] == '6') &&
          (dirname[l-1] == '4'))
        {
          mountflag = 1;
        }
        else if ((dirname[l-3] == 'd' || dirname[l-3] == 'D') &&
                 (dirname[l-2] == '7' || dirname[l-2] == '8') &&
                 (dirname[l-1] == '1'))
          {
            mountflag = 1;
          }
        else if ((dirname[l-3] == 'g' || dirname[l-3] == 'G') &&
                 (dirname[l-2] == '7' || dirname[l-2] == '8') &&
                 (dirname[l-1] == '1'))
          {
            mountflag = 1;
          }
        else if ((dirname[l-3] == 'd' || dirname[l-3] == 'D') &&
                 (dirname[l-2] == 'n' || dirname[l-2] == 'N') &&
                 (dirname[l-1] == 'p' || dirname[l-1] == 'P'))
          {
            mountflag = 1;
          }
        else if ((dirname[l-3] == 'r' || dirname[l-3] == 'R') &&
                 (dirname[l-2] == 'e' || dirname[l-2] == 'E') &&
                 (dirname[l-1] == 'u' || dirname[l-1] == 'U'))
          {
            mountflag = 2;
          }
      }
  }
}

int changeDir(const BYTE device, const char *dirname, const BYTE sorted)
{
  int ret;
  register BYTE l = strlen(dirname);
  
  if (dirname)
    {
      CheckMounttype(dirname);
      if(mountflag==2 && trace == 1) {
        reuflag = 1;
        strcpy(imagename,dirname );
      }

      if (mountflag==1 ||
          (l == 1 && dirname[0]==CH_LARROW) ||
          devicetype[device] == VICE || devicetype[device] == U64)
        {
          sprintf(linebuffer, "cd:%s", dirname);
        }
      else
        {
          sprintf(linebuffer, "cd/%s/", dirname);
        }
      if (trace == 1 )
        {
          strcpy(path[depth], dirname);
        }
    }
  else
    {
      strcpy(linebuffer, "cd//");
    }
  ret = cmd(device, linebuffer);
  if (ret == 0)
    {
      refreshDir(sorted);
    }
  return ret;
}

void printElementPriv(const BYTE xpos, const BYTE ypos)
{
  textcolor(DC_COLOR_HIGHLIGHT);
  gotoxy(xpos,ypos);
  if ((current == cwd.selected))
    {
      textcolor(DMB_COLOR_SELECT);
      revers(1);
    }
  // if blocks are >= 10000 shorten the file type to 2 characters
  strcpy(linebuffer2, fileTypeToStr(PresentDir.dirent.type));
  if (PresentDir.dirent.size >= 10000 &&
      strlen(PresentDir.dirent.name) == 16)
    {
      linebuffer2[0] = linebuffer2[1];
      linebuffer2[1] = linebuffer2[2];
      linebuffer2[2] = 0;
    }
  cprintf((PresentDir.dirent.size < 10000) ? "%4u %-16s %s" : "%u %-15s %s",
          PresentDir.dirent.size,
          PresentDir.dirent.name,
          linebuffer2);
  if (PresentDir.flags!=0)
    {
      gotoxy(xpos,ypos);
      cputc('>');
    }
  textcolor(DC_COLOR_TEXT);
  revers(0);
}

/**
 * input/modify a string.
 * based on version 1.0e, then modified.
 * @param[in] xpos screen x where input starts.
 * @param[in] ypos screen y where input starts.
 * @param[in,out] str string that is edited, it can have content and must have at least @p size + 1 bytes. Maximum size if 255 bytes.
 * @param[in] size maximum length of @p str in bytes.
 * @return -1 if input was aborted.
 * @return >= 0 length of edited string @p str.
 */
int
textInput(const BYTE xpos, const BYTE ypos, char *str, const BYTE size)
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

#pragma code-name (push, "OVERLAY1");
#pragma rodata-name (push, "OVERLAY1");

void updateScreen()
{
  clrscr();
  headertext("Filebrowser");
  textcolor(DC_COLOR_TEXT);
  updateMenu();
  showDir();
}

void refreshDir(const BYTE sorted)
{
  readDir(device, sorted);
  cwd.selected=cwd.firstelement;
  showDir();
}

void printDir()
{
  int selidx = 0;
  int page = 0;
  int skip = 0;
  int pos = 0;
  int idx = 0;
  int xpos,ypos;
  int DIRH = (SCREENW==80)? 38:19;
  const char *typestr = NULL;

  if (!cwd.firstelement)
    {
      clrDir();
      return;
    }

  revers(0);
  current = cwd.firstelement;
  idx=0;
  while (current!=NULL)
    {
      if (current==cwd.selected)
        {
          break;
        }
      idx++;
      VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
      current=PresentDir.next;
    }

  page=idx/DIRH;
  skip=page*DIRH;

  current = cwd.firstelement;

  // skip pages
  if (page>0)
    {
      for (idx=0; (idx < skip) && (current != NULL); ++idx)
        {
          VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
          current=PresentDir.next;
          pos++;
        }
    }

  for(idx=0; (current != NULL) && (idx < DIRH); ++idx)
    {
      xpos = (idx>18)?26:0;
      ypos = (idx>18)?idx-14:idx+5;
      VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
      printElementPriv(xpos,ypos);
      current = PresentDir.next;
    }

  // clear empty lines
  for (;idx < DIRH; ++idx)
    {
      xpos = (idx>18)?26:1;
      ypos = (idx>18)?idx-14:idx+5;
      gotoxy(xpos,ypos);
      cputs("                         ");
    }
}

#pragma code-name(pop);
#pragma rodata-name(pop);