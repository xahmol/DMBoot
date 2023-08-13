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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <cbm.h>
#include "dir.h"
#include "defines.h"
#include "ops.h"
#include "vdc.h"

#define CBM_T_FREE 100

#define DISK_ID_LEN 5
#define disk_id_buf linebuffer2

#pragma code-name ("OVERLAY1");
#pragma rodata-name ("OVERLAY1");

static const char progressBar[4] = { 0xA5, 0xA1, 0xA7, ' ' };
static const char progressRev[4] = { 0,    0,    1,    1 };

struct DirElement PresentDir;
struct Directory cwd;
unsigned int previous;

// VDC alloc functions

unsigned char VDC_AllocDirEntry() {
// Check if new entry fits and return 1 if space

  unsigned int highaddress = (vdcmemory==64)?VDC64END:VDC16END;

  if(highaddress - vdc_alloc_address < sizeof(PresentDir)) {
    return 0;
  } else {
    vdc_alloc_address += sizeof(PresentDir);
    return 1;
  }
}

/**
 * read directory of device @p device.
 * @param device CBM device number
 * @param sorted if true, return directory entries sorted.
 * @param context window context.
 * @return 1 on success
 */
unsigned char readDir(const BYTE device, const BYTE sorted)
{
  BYTE cnt = 0xff;
  const BYTE y = 2;
  BYTE x = 0;

  vdc_alloc_address = (vdcmemory==64)?VDC64START:VDC16START;
  memset(&cwd,0,sizeof(cwd));
  memset(disk_id_buf, 0, DISK_ID_LEN);

  if (cbm_opendir(device, device) != 0)
    {
      cbm_closedir(device);
      return NULL;
    }

  while(1)
    {
      BYTE ret;
      if (!VDC_AllocDirEntry())
        goto stop;

      memset(&PresentDir,0,sizeof(PresentDir));
      ret = myCbmReadDir(device, &(PresentDir.dirent));
      if (ret != 0)
        {
          goto stop;
        }

      // print progress bar
      if ((cnt>>2) >= DIRW)
        {
          x = 4;
          revers(0);
          cnt = 0;
          gotoxy(x, y);
          cclear(DIRW);
          gotoxy(0, y);
          cprintf("[%02i]", device);
        }
      else
        {
          gotoxy(x + (cnt>>2), y);
          revers(progressRev[cnt & 3]);
          cputc(progressBar[cnt & 3]);
          ++cnt;
        }

      if (!cwd.name[0])
        {
          // initialize directory
          if (PresentDir.dirent.type == _CBM_T_HEADER)
            {
              BYTE i;
              for(i = 0; PresentDir.dirent.name[i]; ++i)
                {
                  cwd.name[i] = PresentDir.dirent.name[i];
                }
              cwd.name[i++] = ',';
              memcpy(&cwd.name[i], disk_id_buf, DISK_ID_LEN);
            }
          else
            {
              strcpy(cwd.name, "Unknown type");
            }
          vdc_alloc_address -= sizeof(PresentDir);
        }
      else if (PresentDir.dirent.type==CBM_T_FREE)
        {
          // blocks free entry
          cwd.free=PresentDir.dirent.size;
          goto stop;
        }
      else if (cwd.firstelement==NULL)
        {
          // first element
          cwd.firstelement = vdc_alloc_address;
          VDC_CopyMemToVDC(vdc_alloc_address,(unsigned int)&PresentDir,sizeof(PresentDir));
          previous = vdc_alloc_address;
        }
      else
        {
          // all other elements
          //if (sorted)
            //{
            //  // iterate the sorted list
            //  DirElement *e;
            //  for(e = dir->firstelement; e->next; e = e->next)
            //    {
            //      // if the new name is greater than the current list item,
            //      // it needs to be inserted in the previous position.
            //      if (strncmp(e->dirent.name, de->dirent.name, 16) > 0)
            //        {
            //          // if the previous position is NULL, insert at the front of the list
            //          if (! e->prev)
            //            {
            //              de->next = e;
            //              e->prev = de;
            //              dir->firstelement = de;
            //            }
            //          else
            //            {
            //              // insert somewhere in the middle
            //              DirElement *p = e->prev;
            //              assert(p->next == e);
            //              p->next = de;
            //              de->next = e;
//
            //              de->prev = p;
            //              e->prev = de;
            //            }
            //          goto inserted;
            //        }
            //    }
            //  assert(e->next == NULL);
            //  e->next = de;
            //  de->prev = e;
            //inserted:;
            //}
          //else
            //{
              PresentDir.prev = previous;
              VDC_CopyMemToVDC(vdc_alloc_address,(unsigned int)&PresentDir,sizeof(PresentDir));
              VDC_CopyVDCToMem(previous,(unsigned int)&PresentDir,sizeof(PresentDir));
              PresentDir.next = vdc_alloc_address;
              VDC_CopyMemToVDC(previous,(unsigned int)&PresentDir,sizeof(PresentDir));
              previous = vdc_alloc_address;
            //}
        }
    }

 stop:
  cbm_closedir(device);
  revers(0);

  if (cwd.name[0])
    {
      cwd.selected = cwd.firstelement;
    }
  return 1;
}

/**
 * @param l_dirent pointer to cbm_dirent object, must be memset to 0.
 * @return 0 upon success, @p l_dirent was set.
 * @return >0 upon error.
 */
unsigned char
myCbmReadDir(const BYTE device, struct cbm_dirent* l_dirent)
{
  BYTE b, len;
  BYTE i = 0;

  // check that device is ready
  if (cbm_k_chkin (device) != 0)
    {
      cbm_k_clrch();
      return 1;
    }
  if (cbm_k_readst() != 0)
    {
      return 7;
    }

  // skip next basic line: 0x01, 0x01
  cbm_k_basin();
  cbm_k_basin();

  // read file size
  l_dirent->size = cbm_k_basin();
  l_dirent->size |= (cbm_k_basin()) << 8;

  // read line into linebuffer
  memset(linebuffer, 0, sizeof(linebuffer));
  //cclearxy(0,BOTTOM,SCREENW);//debug
  while(1)
    {
      // read byte
      b = cbm_k_basin();
      // EOL?
      if (b == 0)
        {
          break;
        }
      // append to linebuffer
      if (i < sizeof(linebuffer))
        {
          linebuffer[i++] = b;
          //cputcxy(i,BOTTOM,b);//debug
        }
      // return if reading had error
      if (cbm_k_readst() != 0)
        {
          cbm_k_clrch();
          return 2;
        }
    }
  cbm_k_clrch();
  //cputcxy(i,BOTTOM,'?');//debug

  // handle "B" BLOCKS FREE
  if (linebuffer[0] == 'b')
    {
      l_dirent->type = CBM_T_FREE;
      return 0;
    }

  // check that we have a minimum amount of characters to work with
  if (i < 5)
    {
      return 3;
    }

  // strip whitespace from right part of line
  for(len = i; len > 0; --len)
    {
      b = linebuffer[len];
      if (b == 0 ||
          b == ' ' ||
          b == 0xA0)
        {
          linebuffer[len] = 0;
          continue;
        }
      ++len;
      break;
    }

  //cputcxy(len,BOTTOM,'!');//debug
  //cgetc();//debug

  // parse file name

  // skip until first "
  for(i = 0; i < sizeof(linebuffer) && linebuffer[i] != '"'; ++i)
    {
      // do nothing
    }

  // copy filename, until " or max size
  b = 0;
  for(++i; i < sizeof(linebuffer) && linebuffer[i] != '"' && b < 16; ++i)
    {
      l_dirent->name[b++] = linebuffer[i];
    }

  // check file type
#define X(a,b,c) linebuffer[len-3]==a && linebuffer[len-2]==b && linebuffer[len-1]==c

  if (X('p','r','g'))
    {
      l_dirent->type = CBM_T_PRG;
    }
  else if (X('s','e','q'))
    {
      l_dirent->type = CBM_T_SEQ;
    }
  else if (X('u','s','r'))
    {
      l_dirent->type = CBM_T_USR;
    }
  else if (X('d','e','l'))
    {
      l_dirent->type = CBM_T_DEL;
    }
  else if (X('r','e','l'))
    {
      l_dirent->type = CBM_T_REL;
    }
  else if (X('c','b','m'))
    {
      l_dirent->type = CBM_T_CBM;
    }
  else if (X('d','i','r'))
    {
      l_dirent->type = CBM_T_DIR;
    }
  else if (X('v','r','p'))
    {
      l_dirent->type = CBM_T_VRP;
    }
  else if (X('l','n','k'))
    {
      l_dirent->type = CBM_T_LNK;
    }
  else
    {
      // parse header
      l_dirent->type = _CBM_T_HEADER;

      // skip one character which should be "
      if (linebuffer[i] == '"')
        {
          ++i;
        }
      // skip one character which should be space
      if (linebuffer[i] == ' ')
        {
          ++i;
        }

      // copy disk ID
      for(b = 0; i < sizeof(linebuffer) && b < DISK_ID_LEN; ++i, ++b)
        {
          disk_id_buf[b] = linebuffer[i];
        }

      // strip disk name
      for(b = 15; b > 0; --b)
        {
          if (l_dirent->name[b] == 0 ||
              l_dirent->name[b] == ' ' ||
              l_dirent->name[b] == 0xA0)
            {
              l_dirent->name[b] = 0;
              continue;
            }
          break;
        }

      return 0;
    }

  // parse read-only
  l_dirent->access = (linebuffer[i-4] == 0x3C) ? CBM_A_RO : CBM_A_RW;

  return 0;
}