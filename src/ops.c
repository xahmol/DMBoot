/* -*- c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil -*-
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */
/** @file
 * \date 10.01.2009
 * \author bader
 *
 * DraCopy (dc*) is a simple copy program.
 * DraBrowse (db*) is a simple file browser.
 *
 * Since both programs make use of kernal routines they shall
 * be able to work with most file oriented IEC devices.
 *
 * Created 2009 by Sascha Bader
 *
 * The code can be used freely as long as you retain
 * a notice describing original source and author.
 *
 * THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
 * BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
 *
 * https://github.com/doj/dracopy
 */

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

const char *value2hex = "0123456789abcdef";

Directory* dirs[] = {NULL,NULL};
BYTE devices[] = {8,9};
char linebuffer[81];
char linebuffer2[81];

char DOSstatus[40];

/// string descriptions of enum drive_e
const char* drivetype[LAST_DRIVE_E] = {"", "Pi1541", "1540", "1541", "1551", "1570", "1571", "1581", "1001", "2031", "8040", "sd2iec", "cmd", "vice", "u64"};/// enum drive_e value for each device 0-19.
BYTE devicetype[MAXDEVID+1];

void
initDirWindowHeight(void)
{
  if (SCREENW == 80)
  {
    DIR1H = 23;
    DIR2H = 23;
  }
  else
  {
    DIR1H = 11;
    DIR2H = 10;
  }
}

const char*
getDeviceType(const BYTE device)
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

void
clrDir(BYTE context)
{
  clearArea(DIRX+1, DIRY+1, DIRW, DIRH);
}

const char *reg_types[] = { "SEQ","PRG","URS","REL","VRP" };
const char *oth_types[] = { "DEL","CBM","DIR","LNK","OTH","HDR"};
char bad_type[4];
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

void
drawDirFrame(BYTE context, const BYTE mycontext)
{
  const Directory *dir = GETCWD;
  const char *dt = drivetype[devicetype[devices[context]]];
  sprintf(linebuffer, "%i:%s", (int)devices[context], dir ? dir->name : "");
  if (dir)
    {
      sprintf(linebuffer2, "%s>%u bl free<", dt, dir->free);
      dt = linebuffer2;
    }
  textcolor((mycontext==context) ? DC_COLOR_HIGHLIGHT : DC_COLOR_TEXT);
  drawFrame(linebuffer, DIRX, DIRY, DIRW+2, DIRH+2, dt);
  textcolor(DC_COLOR_TEXT);
}

void
showDir(BYTE context, const BYTE mycontext)
{
  drawDirFrame(context, mycontext);
  printDir(context, DIRX+1, DIRY);
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

int
changeDir(const BYTE context, const BYTE device, const char *dirname, const BYTE sorted)
{
  int ret;
  register BYTE l = strlen(dirname);
  
  if (dirname)
    {
      CheckMounttype(dirname);
      if(mountflag==2 && trace == 1) {
        reuflag = 1;
        strcpy(imagename,dirs[context]->selected->dirent.name );
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
      refreshDir(context, sorted, context);
    }
  return ret;
}

static void
printElementPriv(const BYTE context, const Directory *dir, const DirElement *current, const BYTE xpos, const BYTE ypos)
{
  Directory * cwd = GETCWD;
  gotoxy(xpos,ypos);
  if ((current == dir->selected) && (cwd == dir))
    {
      revers(1);
    }

  // if blocks are >= 10000 shorten the file type to 2 characters
  strcpy(linebuffer2, fileTypeToStr(current->dirent.type));
  if (current->dirent.size >= 10000 &&
      strlen(current->dirent.name) == 16)
    {
      linebuffer2[0] = linebuffer2[1];
      linebuffer2[1] = linebuffer2[2];
      linebuffer2[2] = 0;
    }
  cprintf((current->dirent.size < 10000) ? "%4u %-16s %s" : "%u %-15s %s",
          current->dirent.size,
          current->dirent.name,
          linebuffer2);

  if (current->flags!=0)
    {
      gotoxy(xpos,ypos);
      textcolor(DC_COLOR_HIGHLIGHT);
      cputc('>');
    }

  textcolor(DC_COLOR_TEXT);
  revers(0);
}

void
printElement(const BYTE context, const Directory *dir, const BYTE xpos, const BYTE ypos)
{
  const DirElement *current;

  int page = 0;
  int idx = 0;
  int pos = 0;
  int yoff=0;

  if (dir==NULL || dir->firstelement == NULL)
    {
      return;
    }

  revers(0);
  current = dir->firstelement;

  pos = dir->pos;

  idx=pos;
  while (current!=NULL && (idx--) >0)
    {
      current=current->next;
    }

  page=pos/DIRH;
  yoff=pos-(page*DIRH);

  printElementPriv(context, dir, current, xpos, ypos+yoff+1);
}

void
changeDeviceID(BYTE device)
{
  int i;
  newscreen("change device ID");
  cprintf("\n\rchange device ID %i to (0-255): ", device);
  sprintf(linebuffer, "%i", device);
  i = textInput(31, 2, linebuffer, 3);
  if (i <= 0)
    return;
  i = atoi(linebuffer);

  if (devicetype[device] == SD2IEC)
    {
      sprintf(linebuffer, "U0>%c", i);
    }
  else
    {
      // TODO: doesn't work

      // Commodore drives:
      // OPEN 15,8,15:PRINT#15,"M-W";CHR$(119);CHR$(0);CHR$(2);CHR$(device number+32);CHR$(device number+64):CLOSE 15
      char *s = linebuffer;
      *s++ = 'm';
      *s++ = '-';
      *s++ = 'w';
      *s++ = 119; // addr lo
      *s++ = 0;   // addr hi
      *s++ = 2;   // number of bytes
      *s++ = 32+i;// device num + 0x20 for LISTEN
      *s++ = 64+i;// device num + 0x40 for TALK
      *s = 0;
    }

  cmd(device, linebuffer);
}

void
debugs(const char *s)
{
  gotoxy(MENUXT,BOTTOM);
  cclear(SCREENW-MENUXT);
  gotoxy(MENUXT,BOTTOM);
  cputs(s);
}

void
debugu(const unsigned u)
{
  gotoxy(MENUXT,BOTTOM);
  cclear(SCREENW-MENUXT);
  gotoxy(MENUXT,BOTTOM);
  cprintf("%04x", u);
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

void
updateScreen(const BYTE context, BYTE num_dirs)
{
  clrscr();
  updateMenu();
  showDir(context, context);
  if (num_dirs > 1)
    {
      const BYTE other_context = context^1;
      showDir(other_context, context);
    }
}

void
refreshDir(const BYTE context, const BYTE sorted, const BYTE mycontext)
{
  Directory * cwd = dirs[context];
  textcolor(DC_COLOR_HIGHLIGHT);
  cwd = readDir(cwd, devices[context], context, sorted);
  dirs[context]=cwd;
  cwd->selected=cwd->firstelement;
  showDir(context, mycontext);
}

void
printDir(const BYTE context, const BYTE xpos, const BYTE ypos)
{
  const Directory *dir = GETCWD;
  DirElement * current;
  int selidx = 0;
  int page = 0;
  int skip = 0;
  int pos = 0;
  int idx = 0;
  const char *typestr = NULL;

  if (dir==NULL)
    {
      clrDir(context);
      return;
    }

  revers(0);
  current = dir->firstelement;
  idx=0;
  while (current!=NULL)
    {
      if (current==dir->selected)
        {
          break;
        }
      idx++;
      current=current->next;
    }

  page=idx/DIRH;
  skip=page*DIRH;

  current = dir->firstelement;

  // skip pages
  if (page>0)
    {
      for (idx=0; (idx < skip) && (current != NULL); ++idx)
        {
          current=current->next;
          pos++;
        }
    }

  for(idx=0; (current != NULL) && (idx < DIRH); ++idx)
    {
      printElementPriv(context, dir, current, xpos, ypos+idx+1);
      current = current->next;
    }

  // clear empty lines
  for (;idx < DIRH; ++idx)
    {
      gotoxy(xpos,ypos+idx+1);
      cputs("                         ");
    }
}

#pragma code-name(pop);
#pragma rodata-name(pop);