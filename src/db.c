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
#include "cat.h"
#include "dir.h"
#include "base.h"
#include "defines.h"
#include "ops.h"
#include "version.h"

static BYTE sorted = 0;

void
updateMenu(void)
{
  BYTE menuy=MENUY;

  revers(0);
  textcolor(DC_COLOR_TEXT);
  drawFrame(" DMBoot ",MENUX,MENUY,MENUW,MENUH,NULL);

  ++menuy;
  cputsxy(MENUXT+1,++menuy,"F1 DIR");
  cputsxy(MENUXT+1,++menuy,"F2 DEVICE");
  cputsxy(MENUXT+1,++menuy,"F3 HEX");
  cputsxy(MENUXT+1,++menuy,"F4 ASC");
  cputsxy(MENUXT+1,++menuy,"F5 BOOT");
  cputsxy(MENUXT+1,++menuy,"F7 RUN");
  cputsxy(MENUXT+1,++menuy,"CR RUN/CD");
  cputsxy(MENUXT+1,++menuy,"BS DIR UP");
  cputsxy(MENUXT+1,++menuy," \x5e PAR DIR");
  cputsxy(MENUXT+1,++menuy," T TOP");
  cputsxy(MENUXT+1,++menuy," B BOTTOM");
  cputsxy(MENUXT+1,++menuy," S SORT");
  cputsxy(MENUXT+1,++menuy," D DIRTRAC");
  cputsxy(MENUXT+1,++menuy," 8 FORCE 8");
  cputsxy(MENUXT+1,++menuy," F FAST");
  cputsxy(MENUXT+1,++menuy," @ DOScmd");
  cputsxy(MENUXT+1,++menuy," Q QUIT");
  if (SCREENW == 80 )
  {
    cputsxy(MENUXT+1,++menuy," \xff SW WIN");
  }
  ++menuy;
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
  BYTE num_windows;

  trace = 0;
  depth = 0;

  DIR1H = DIR2H = SCREENH-2;
  dirs[0] = dirs[1] = NULL;

  if (SCREENW == 80)
  {
    num_windows = 2;
  }
  else
  {
    num_windows = 1;
  }
  
  updateScreen(context, num_windows);

  {
    BYTE i;
    for(i = 0; i < 16; ++i)
      {
        cbm_close(i);
        cbm_closedir(i);
      }

    textcolor(DC_COLOR_HIGHLIGHT);
    i = 7;
    while(++i < 12)
      {
        devices[context] = i;
        dirs[context] = readDir(NULL, devices[context], context, sorted);
        if (dirs[context])
          {
            getDeviceType(devices[context]);
            showDir(context, context);
            goto found_upper_drive;
          }
      }

    found_upper_drive:;
    if (SCREENW == 80)
    {
      textcolor(DC_COLOR_TEXT);
      while(++i < 12)
      {
        devices[1] = i;
        dirs[1] = readDir(NULL, devices[1], 1, sorted);
        if (dirs[1])
        {
            getDeviceType(devices[1]);
            showDir(1, context);
            goto found_lower_drive;
        }
      }
      found_lower_drive:;
    }
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
          dirs[context]=readDir(dirs[context], devices[context], context, sorted);
          showDir(context, context);
          break;

        case '2':
        case CH_F2:
          if (++devices[context] > 11)
            devices[context]=8;
          freeDir(&dirs[context]);
          if (! devicetype[devices[context]])
            {
              getDeviceType(devices[context]);
            }
          showDir(context, context);
          break;

        case '3':
        case CH_F3:
          cathex(devices[context],dirs[context]->selected->dirent.name);
          updateScreen(context, num_windows);
          break;

        case '4':
        case CH_F4:
          catasc(devices[context],dirs[context]->selected->dirent.name);
          updateScreen(context, num_windows);
          break;

        // --- boot directory
        case '5':
        case CH_F5:
          cwd=GETCWD;
          if (trace == 0)
          {
            execute(dirs[context]->selected->dirent.name,devices[context], 1 + 2*forceeight + 10*fastflag, "");
          }
          else
          {
            strcpy(pathfile, "" );
            pathrunboot = 1 + 2*forceeight + 10*fastflag;
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

        case 'b':
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

        case '@':
          doDOScommand(context, sorted, 0);
          updateScreen(context, num_windows);
          break;

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

          // --- start / enter directory
        case '7':
        case CH_F7:
        case CH_ENTER:
          cwd=GETCWD;
          if (cwd->selected && cwd->selected->dirent.type==CBM_T_PRG)
            {
              if (trace == 0)
              {
                execute(dirs[context]->selected->dirent.name,devices[context], 0 + 2*forceeight + 10*fastflag, "");
              }
              else
              {
                strcpy(pathfile, dirs[context]->selected->dirent.name );
                pathrunboot = 0 + 2*forceeight + 10*fastflag;
                goto done;
              }             
            }
          // else fallthrough to CURS_RIGHT

        case CH_CURS_RIGHT:
          cwd=GETCWD;
          if (cwd->selected)
            {
              if (trace == 1)
              {
                strcpy(path[depth++],cwd->selected->dirent.name);
              }
              changeDir(context, devices[context], cwd->selected->dirent.name, sorted);
            }
          break;

          // --- leave directory
        case CH_DEL:
        case CH_CURS_LEFT:
          if (trace == 1)
          {
            --depth;
          }
          changeDir(context, devices[context], "\xff", sorted);
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
        }
    }

 done:;
  // nobody cares about freeing memory upon program exit.
#if 0
  freeDir(&dirs[0]);
#endif
}
