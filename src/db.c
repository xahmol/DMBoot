/* -*- c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil -*-
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */
/** @file
 * \date 10.01.2009
 * \author bader
 *
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <screen.h>
#include <conio.h>
#include "dir.h"
#include "base.h"
#include "defines.h"
#include "ops.h"
#include "version.h"
#include "vdc.h"
#include "dmapi.h"

#pragma code-name ("OVERLAY1");
#pragma rodata-name ("OVERLAY1");

static BYTE sorted = 0;

void
updateMenu(void)
{
  BYTE menuy=2;

  clearArea(MENUX,3,15,22);
  revers(0);
  textcolor(DC_COLOR_TEXT);

  cputsxy(MENUX+1,++menuy," F1 Dir refr.");
  cputsxy(MENUX+1,++menuy,"+/- Device");
  cputsxy(MENUX+1,++menuy," F5 Boot");
  cputsxy(MENUX+1,++menuy,"ENT Run/Select");
  cputsxy(MENUX+1,++menuy,"DEL Dir up");
  cputsxy(MENUX+1,++menuy,"  \x5e Root dir");
  cputsxy(MENUX+1,++menuy,"  T Top");
  cputsxy(MENUX+1,++menuy,"  E End");
  cputsxy(MENUX+1,++menuy,"  S Sort");
  cputsxy(MENUX+1,++menuy,"P/U Page up/do");
  cputsxy(MENUX+1,++menuy,"Cur Navigate");
  cputsxy(MENUX+1,++menuy,"  D Dirtrace");
  if(trace) {
    cputsxy(MENUX+1,++menuy," AB Add mount");
    cputsxy(MENUX+1,++menuy,"  M Run mount");
  } else { menuy += 2; }
  cputsxy(MENUX+1,++menuy,"  6 Run in C64");
  cputsxy(MENUX+1,++menuy,"  8 Force ID 8");
  cputsxy(MENUX+1,++menuy,"  F Fast mode");
  cputsxy(MENUX+1,++menuy,"  Q Quit");

  menuy++;
  if (trace == 1)
  {
    cputsxy(MENUX,++menuy," Trace   ON ");
  }
  else
  {
    cputsxy(MENUX,++menuy," Trace   OFF");
  }
  if (forceeight == 1)
  {
    cputsxy(MENUX,++menuy," Force 8 ON ");
  }
  else
  {
    cputsxy(MENUX,++menuy," Force 8 OFF");
  }
  if (fastflag == 1)
  {
    cputsxy(MENUX,++menuy," Fast    ON ");
  }
  else
  {
    cputsxy(MENUX,++menuy," Fast    OFF");
  }
  
}

void mainLoopBrowse(void)
{
  unsigned int pos = 0;
  BYTE lastpage = 0;
  BYTE nextpage = 0;
  int DIRH = (SCREENW==80)? 38:19;
  int xpos,ypos,yoff;
  unsigned char count;
  
  trace = 0;
  depth = 0;
  reuflag = 0;
  addmountflag = 0;
  runmountflag = 0;
  mountflag = 0;

  updateScreen();

  {
    BYTE i;
    for(i = 0; i < 16; ++i)
      {
        cbm_close(i);
        cbm_closedir(i);
      }

    textcolor(DC_COLOR_TEXT);
    dm_devid=0;
    dm_gethsidviaapi();
    i = (dm_devid)?dm_devid-1:7;

    while(++i < MAXDEVID+1)
      {
        device = i;
        if(readDir(device, sorted))
          {
            getDeviceType(device);
            showDir();
            goto found_first_drive;
          }
      }

    found_first_drive:;
  }

  while(1)
    {
      current = cwd.selected;
      if(current) { VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir)); }
      pos=cwd.pos;
      lastpage=pos/DIRH;
      yoff=pos-(lastpage*DIRH);
      xpos = (yoff>18)?26:0;
      ypos = (yoff>18)?yoff-14:yoff+5;

      switch (cgetc())
        {
        case 's':
          sorted = ! sorted;
          // fallthrough
        case '1':
        case CH_F1:
          readDir(device, sorted);
          showDir();
          break;

        case '2':
        case CH_F2:
        case '+':
          if (++device > MAXDEVID)
            device=8;
          if (! devicetype[device])
            {
              getDeviceType(device);
            }
          memset(&cwd,0,sizeof(cwd));
          showDir();
          break;
        
        case '-':
          if (--device < 8) { device=MAXDEVID; }
          if (! devicetype[device])
            {
              getDeviceType(device);
            }
          memset(&cwd,0,sizeof(cwd));
          showDir();
          break;

        // --- boot directory
        case '5':
        case CH_F5:
          if (trace == 0)
          {
            execute(PresentDir.dirent.name,device, EXEC_BOOT + EXEC_FRC8*forceeight + EXEC_FAST*fastflag, "");
          }
          else
          {
            strcpy(pathfile, "" );
            pathrunboot = EXEC_BOOT + EXEC_FRC8*forceeight + EXEC_FAST*fastflag;
            goto done;
          }   
          break;

        case 't':
        case CH_HOME:
          cwd.selected=cwd.firstelement;
          cwd.pos=0;
          printDir();
          break;

        case 'd':
          if (trace == 0)
          {
            trace = 1;
            pathdevice = device;
            changeDir(device, NULL, sorted);
          }
          else
          {
            trace = 0;
            depth = 0;
            showDir();
          }
          updateMenu();
          break;

        case '8':
          if (forceeight == 0)
          {
            forceeight = 1;
          }
          else
          {
            forceeight = 0;
          }
          updateMenu();
          break;
        
        case 'f':
          if (fastflag == 0)
          {
            fastflag = 1;
          }
          else
          {
            fastflag = 0;
          }
          updateMenu();
          break;

        case 'e':
          current = cwd.firstelement;
          VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
          pos=0;
          while (1)
            {
              if (PresentDir.next!=NULL)
                {
                  current=PresentDir.next;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  pos++;
                }
              else
                {
                  break;
                }
            }
          cwd.selected=current;
          cwd.pos=pos;
          printDir();
          break;

        case 'q':
          trace = 0;
          goto done;

        case CH_CURS_DOWN:
          if (cwd.selected!=NULL && PresentDir.next!=NULL)
            {
              current=PresentDir.next;
              VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
              cwd.selected=current;
              nextpage=(pos+1)/DIRH;
              cwd.pos++;
              if (lastpage!=nextpage)
                {
                  cwd.firstprinted = current;
                  printDir();
                }
              else
                {
                  current=PresentDir.prev;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  printElementPriv(xpos, ypos);
                  xpos = (++yoff>18)?26:0;
                  ypos = (yoff>18)?yoff-14:yoff+5;
                  current=PresentDir.next;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  printElementPriv(xpos, ypos);
                }
            }
          break;

        case CH_CURS_UP:
          if (cwd.selected!=NULL && PresentDir.prev!=NULL)
            {
              current=PresentDir.prev;
              VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
              cwd.selected=current;
              nextpage=(pos-1)/DIRH;
              cwd.pos--;
              if (lastpage!=nextpage)
                {
                  for(count=0;count<DIRH;count++) {
                    if(PresentDir.prev != NULL) {
                      current=PresentDir.prev;
                      VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                    }
                  }
                  cwd.firstprinted = current;
                  printDir();
                }
              else
                {
                  current=PresentDir.next;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  printElementPriv(xpos, ypos);
                  xpos = (--yoff>18)?26:0;
                  ypos = (yoff>18)?yoff-14:yoff+5;
                  current=PresentDir.prev;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  printElementPriv(xpos, ypos);
                }
            }
          break;

        // --- Run in 64 mode
        case '6':
          if(dm_apipresent==1 && dm_apiversion>1)
          {
            if (cwd.selected && PresentDir.dirent.type==CBM_T_PRG)
              {
                if (trace == 0)
                {
                  execute(PresentDir.dirent.name,device, EXEC_RUN64, "");
                }
                else
                {
                  strcpy(pathfile, PresentDir.dirent.name );
                  pathrunboot = EXEC_RUN64;
                  goto done;
                }             
              }
          }
          break;

        case CH_CURS_RIGHT:
          // Check if two columns and not already last item? If yes, Cursor right moves to right
          if(SCREENW==80 && PresentDir.next!=NULL) {
            // Check if not already in right column
            if(xpos==0) {
              cwd.selected = 0;
              printElementPriv(xpos, ypos);
              for(count=0;count<19;count++)
              {
                if(PresentDir.next) {
                  current=PresentDir.next;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  cwd.pos++;
                  cwd.selected=current;
                }
              }
              pos=cwd.pos;
              yoff=pos-(lastpage*DIRH);
              xpos = (yoff>18)?26:0;
              ypos = (yoff>18)?yoff-14:yoff+5;
              printElementPriv(xpos, ypos);
            }
            break;

          } // Else fallthrough

          // --- start / enter directory
        case '7':
        case CH_F7:
        case CH_ENTER:
          // Executable PRG?
          if (cwd.selected && PresentDir.dirent.type==CBM_T_PRG)
            {
              if (trace == 0)
              {
                execute(PresentDir.dirent.name,device, EXEC_FRC8*forceeight + EXEC_FAST*fastflag, "");
              }
              else
              {
                strcpy(pathfile, PresentDir.dirent.name );
                pathrunboot = EXEC_FRC8*forceeight + EXEC_FAST*fastflag;
                goto done;
              }             
            }
          // else change dir
          if (cwd.selected)
            {
              if (trace == 1) {
                strcpy(path[depth++],PresentDir.dirent.name);
              }
              changeDir(device, PresentDir.dirent.name, sorted);
            }
            if(reuflag) { goto done; }
          break;

        case CH_CURS_LEFT:
        // Check if two columns and not already first item? If yes, Cursor right moves to left
          if(SCREENW==80 && PresentDir.prev!=NULL) {
          // Check if not already in left column
            if(xpos==26) {
              cwd.selected = 0;
              printElementPriv(xpos, ypos);
              for(count=0;count<19;count++)
              {
                if(PresentDir.prev) {
                  current=PresentDir.prev;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  cwd.pos--;
                  cwd.selected=current;
                }
              }
              pos=cwd.pos;
              yoff=pos-(lastpage*DIRH);
              xpos = (yoff>18)?26:0;
              ypos = (yoff>18)?yoff-14:yoff+5;
              printElementPriv(xpos, ypos);
            }
            break;

          } // Else fallthrough

          // --- leave directory
        case CH_DEL:
          if (trace == 1)
          {
            --depth;
          }
          changeDir(device, devicetype[device] == U64?"..":"\xff", sorted);
          break;

        // Page down
        case 'p':
        // Check if not already last item? If no, page down
          if(PresentDir.next!=NULL) {
              cwd.selected = 0;
              printElementPriv(xpos, ypos);
              for(count=0;count<DIRH;count++)
              {
                if(PresentDir.next) {
                  current=PresentDir.next;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  cwd.pos++;
                  cwd.selected=current;
                  cwd.firstprinted=current;
                }
              }
              pos=cwd.pos;
              yoff=pos-(lastpage*DIRH);
              xpos = (yoff>18)?26:0;
              ypos = (yoff>18)?yoff-14:yoff+5;
              printDir();
          }
          break;

        // Page up
        case 'u':
        // Check if not already first item? If no, page up
          if(PresentDir.prev!=NULL) {
              cwd.selected = 0;
              printElementPriv(xpos, ypos);
              for(count=0;count<DIRH;count++)
              {
                if(PresentDir.prev) {
                  current=PresentDir.prev;
                  VDC_CopyVDCToMem(current,(unsigned int)&PresentDir,sizeof(PresentDir));
                  cwd.pos--;
                  cwd.selected=current;
                  cwd.firstprinted=current;
                }
              }
              pos=cwd.pos;
              yoff=pos-(lastpage*DIRH);
              xpos = (yoff>18)?26:0;
              ypos = (yoff>18)?yoff-14:yoff+5;
              printDir();
          }
          break;

        case CH_UARROW:
          if (trace == 1)
          {
            depth = 0;
          }
          changeDir(device, NULL, sorted);
          break;

        case 'a':
          CheckMounttype(PresentDir.dirent.name);
          if(mountflag==1) {
            addmountflag = 1;
            strcpy(imageaname,PresentDir.dirent.name);
            goto done;
          }
        
        case 'b':
          CheckMounttype(PresentDir.dirent.name);
          if(mountflag==1) {
            addmountflag = 2;
            strcpy(imagebname,PresentDir.dirent.name);
            goto done;
          }

        case 'm':
          if(mountflag==1 && imageaid) {
            runmountflag = 1;
            strcpy(pathfile, PresentDir.dirent.name );
            pathrunboot = EXEC_MOUNT + EXEC_FAST*fastflag;
            goto done;
          }

        }
    }

 done:;
}