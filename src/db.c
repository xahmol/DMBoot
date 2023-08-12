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

#pragma code-name ("OVERLAY1");
#pragma rodata-name ("OVERLAY1");

static BYTE sorted = 0;

void
updateMenu(void)
{
  BYTE menuy=MENUY;

  clearArea(MENUXT+1,MENUY+1,MENUW-2,MENUH-2);
  revers(0);
  textcolor(DC_COLOR_TEXT);
  drawFrame(" DMBoot ",MENUX,MENUY,MENUW,MENUH,NULL);

  menuy++;
  cputsxy(MENUXT+1,++menuy,"F1 DIR");
  cputsxy(MENUXT+1,++menuy,"+- DEVICE");
  cputsxy(MENUXT+1,++menuy,"F5 BOOT");
  cputsxy(MENUXT+1,++menuy,"CR RUN/CD");
  cputsxy(MENUXT+1,++menuy,"BS DIR UP");
  cputsxy(MENUXT+1,++menuy," \x5e PAR DIR");
  cputsxy(MENUXT+1,++menuy," T TOP");
  cputsxy(MENUXT+1,++menuy," E END");
  cputsxy(MENUXT+1,++menuy," S SORT");
  menuy++;
  cputsxy(MENUXT+1,++menuy," D DIRTRAC");
  if(trace) {
    cputsxy(MENUXT+1,++menuy,"AB ADD MNT");
    cputsxy(MENUXT+1,++menuy," M RUN MNT");
  } else { menuy += 2; }
  cputsxy(MENUXT+1,++menuy," 6 RUN 64");
  cputsxy(MENUXT+1,++menuy," 8 FORCE 8");
  cputsxy(MENUXT+1,++menuy," F FAST");
  cputsxy(MENUXT+1,++menuy," Q QUIT");

  menuy++;
  if (trace == 1)
  {
    cputsxy(MENUXT,++menuy," TRACE ON ");
  }
  else
  {
    cputsxy(MENUXT,++menuy," TRACE OFF");
  }
  if (forceeight == 1)
  {
    cputsxy(MENUXT,++menuy," Frc 8 ON ");
  }
  else
  {
    cputsxy(MENUXT,++menuy," Frc 8 OFF");
  }
  if (fastflag == 1)
  {
    cputsxy(MENUXT,++menuy," FAST  ON ");
  }
  else
  {
    cputsxy(MENUXT,++menuy," FAST  OFF");
  }
  
}

void
mainLoopBrowse(void)
{
  Directory * cwd = NULL;
  DirElement * current = NULL;
  unsigned int pos = 0;
  BYTE lastpage = 0;
  BYTE nextpage = 0;
  BYTE context = 0;
  
  trace = 0;
  depth = 0;
  reuflag = 0;
  addmountflag = 0;
  runmountflag = 0;
  mountflag = 0;

  DIR1H = DIR2H = SCREENH-2;
  dirs = NULL;
  
  updateScreen(context);

  {
    BYTE i;
    for(i = 0; i < 16; ++i)
      {
        cbm_close(i);
        cbm_closedir(i);
      }

    textcolor(DC_COLOR_HIGHLIGHT);
    i = 7;
    while(++i < MAXDEVID+1)
      {
        devices[context] = i;
        dirs = readDir(NULL, devices[context], sorted);
        if (dirs)
          {
            getDeviceType(devices[context]);
            showDir(context, context);
            goto found_upper_drive;
          }
      }

    found_upper_drive:;
  }

  while(1)
    {
      switch (cgetc())
        {
        case 's':
          sorted = ! sorted;
          // fallthrough
        case '1':
        case CH_F1:
          textcolor(DC_COLOR_HIGHLIGHT);
          dirs=readDir(dirs, devices[context], sorted);
          showDir(context, context);
          break;

        case '2':
        case CH_F2:
        case '+':
          if (++devices[context] > MAXDEVID)
            devices[context]=8;
          freeDir(&dirs);
          if (! devicetype[devices[context]])
            {
              getDeviceType(devices[context]);
            }
          showDir(context, context);
          break;
        
        case '-':
          if (--devices[context] < 8) { devices[context]=MAXDEVID; }
          freeDir(&dirs);
          if (! devicetype[devices[context]])
            {
              getDeviceType(devices[context]);
            }
          showDir(context, context);
          break;

        // --- boot directory
        case '5':
        case CH_F5:
          cwd=GETCWD;
          if (trace == 0)
          {
            execute(dirs->selected->dirent.name,devices[context], EXEC_BOOT + EXEC_FRC8*forceeight + EXEC_FAST*fastflag, "");
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
          cwd=GETCWD;
          cwd->selected=cwd->firstelement;
          cwd->pos=0;
          printDir(context, DIRX+1, DIRY);
          break;

        case 'd':
          if (trace == 0)
          {
            trace = 1;
            pathdevice = devices[context];
            changeDir(context, devices[context], NULL, sorted);
          }
          else
          {
            trace = 0;
            depth = 0;
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
          cwd=GETCWD;
          current = cwd->firstelement;
          pos=0;
          while (1)
            {
              if (current->next!=NULL)
                {
                  current=current->next;
                  pos++;
                }
              else
                {
                  break;
                }
            }
          cwd->selected=current;
          cwd->pos=pos;
          printDir(context, DIRX+1, DIRY);
          break;

        case 'q':
          trace = 0;
          goto done;

        case CH_CURS_DOWN:
          cwd=GETCWD;
          if (cwd->selected!=NULL && cwd->selected->next!=NULL)
            {
              cwd->selected=cwd->selected->next;
              pos=cwd->pos;
              lastpage=pos/DIRH;
              nextpage=(pos+1)/DIRH;
              if (lastpage!=nextpage)
                {
                  cwd->pos++;
                  printDir(context, DIRX+1, DIRY);
                }
              else
                {
                  printElement(context, cwd, DIRX+1, DIRY);
                  cwd->pos++;
                  printElement(context, cwd, DIRX+1, DIRY);
                }

            }
          break;

        case CH_CURS_UP:
          cwd=GETCWD;
          if (cwd->selected!=NULL && cwd->selected->prev!=NULL)
            {
              cwd->selected=cwd->selected->prev;
              pos=cwd->pos;
              lastpage=pos/DIRH;
              nextpage=(pos-1)/DIRH;
              if (lastpage!=nextpage)
                {
                  cwd->pos--;
                  printDir(context, DIRX+1, DIRY);
                }
              else
                {
                  printElement(context, cwd, DIRX+1, DIRY);
                  cwd->pos--;
                  printElement(context, cwd, DIRX+1, DIRY);
                }
            }
          break;

        // --- Run in 64 mode
        case '6':
          if(dm_apipresent==1 && dm_apiversion>1)
          {
            cwd=GETCWD;
            if (cwd->selected && cwd->selected->dirent.type==CBM_T_PRG)
              {
                if (trace == 0)
                {
                  execute(dirs->selected->dirent.name,devices[context], EXEC_RUN64, "");
                }
                else
                {
                  strcpy(pathfile, dirs->selected->dirent.name );
                  pathrunboot = EXEC_RUN64;
                  goto done;
                }             
              }
          }
          break;

          // --- start / enter directory
        case '7':
        case CH_F7:
        case CH_ENTER:
          cwd=GETCWD;
          if (cwd->selected && cwd->selected->dirent.type==CBM_T_PRG)
            {
              if (trace == 0)
              {
                execute(dirs->selected->dirent.name,devices[context], EXEC_FRC8*forceeight + EXEC_FAST*fastflag, "");
              }
              else
              {
                strcpy(pathfile, dirs->selected->dirent.name );
                pathrunboot = EXEC_FRC8*forceeight + EXEC_FAST*fastflag;
                goto done;
              }             
            }
          // else fallthrough to CURS_RIGHT

        case CH_CURS_RIGHT:
          cwd=GETCWD;
          if (cwd->selected)
            {
              if (trace == 1) {
                strcpy(path[depth++],cwd->selected->dirent.name);
              }
              changeDir(context, devices[context], cwd->selected->dirent.name, sorted);
            }
            if(reuflag) { goto done; }
          break;

          // --- leave directory
        case CH_DEL:
        case CH_CURS_LEFT:
          if (trace == 1)
          {
            --depth;
          }
          changeDir(context, devices[context], devicetype[devices[context]] == U64?"..":"\xff", sorted);
          break;

        case CH_UARROW:
          if (trace == 1)
          {
            depth = 0;
          }
          changeDir(context, devices[context], NULL, sorted);
          break;

          // ----- switch context -----
        case '0':
        case CH_ESC:
        case CH_LARROW:  // arrow left
          {
            if (SCREENW == 80 )
            {
              const BYTE prev_context = context;
              context = context ^ 1;
              drawDirFrame(context, context);
              drawDirFrame(prev_context, context);
              trace = 0;
              depth = 0;
              updateMenu();
            }
          }
          break;

        case 'a':
          CheckMounttype(cwd->selected->dirent.name);
          if(mountflag==1) {
            addmountflag = 1;
            strcpy(imageaname,dirs->selected->dirent.name);
            goto done;
          }
        
        case 'b':
          CheckMounttype(cwd->selected->dirent.name);
          if(mountflag==1) {
            addmountflag = 2;
            strcpy(imagebname,dirs->selected->dirent.name);
            goto done;
          }

        case 'm':
          if(mountflag==1 && imageaid) {
            runmountflag = 1;
            strcpy(pathfile, dirs->selected->dirent.name );
            pathrunboot = EXEC_MOUNT + EXEC_FAST*fastflag;
            goto done;
          }

        }
    }

 done:;
 freeDir(&dirs);
}
