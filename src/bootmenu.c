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
#include <stdio.h>
#include <string.h>
#include <peekpoke.h>
#include <cbm.h>
#include <conio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <device.h>
#include <accelerator.h>
#include <em.h>
#include "defines.h"
#include "ops.h"
#include "screen.h"
#include "base.h"
#include "main.h"
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "ultimate_time_lib.h"

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

#pragma code-name ("OVERLAY2");
#pragma rodata-name ("OVERLAY2");

// Predefines
void pickmenuslot();
void headertext(char* subtitle);
char mainmenu();
void runbootfrommenu(int select);
void commandfrommenu(char * command, int confirm);
void bootfromfloppy();
void information();
char* completeVersion();
void editmenuoptions();
void presentmenuslots();
int deletemenuslot();
int renamemenuslot();
int reordermenuslot();
int edituserdefinedcommand();
void printnewmenuslot(int pos, int color, char* name);
void getslotfromem(int slotnumber);
void putslottoem(int slotnumber);
char menuslotkey(int slotnumber);
int keytomenuslot(char keypress);

// Functions

void pickmenuslot()
{
    // Routine to pick a slot to store the chosen dir trace path
    
    int menuslot;
    char key,devid,plusmin;
    BYTE yesno;
    BYTE selected = 0;
    char deviceidbuffer[3];
    char* ptrend;
    
    clrscr();
    headertext("Choose menuslot for chosen start option.");
    presentmenuslots();
    gotoxy(0,21);
    cputs("Choose slot by pressing key: ");
    do
    {
        key = cgetc();
    } while ((key<48 || key>57) && (key<65 || key>90));  
    menuslot = keytomenuslot(key);
    selected = 1;
    getslotfromem(menuslot);
    cprintf("%c", key);
    if ( strlen(Slot.menu) != 0 )
    {
        gotoxy(0,22);
        cprintf("Slot not empty. Edit? Y/N ");
        yesno = getkey(128);
        cprintf("%c", yesno);
        if ( yesno == 78 )
        {
            selected = 0;
        }
    } else {
        strcpy(Slot.menu, pathfile);
    }
    if ( selected == 1)
    {
        clearArea(0,22,SCREENW,3);
        gotoxy(0,23);
        cputs("Choose name for slot:");
        textInput(0,24,Slot.menu,20);

        clearArea(0,23,SCREENW,2);
        gotoxy(0,23);
        if(reuflag || addmountflag) {
            if(reuflag) {

                cputs("Select REU size (+/-/ENTER):");

                do
                {
                  gotoxy(0,24);
                  cprintf("REU file size: (%i) %s",Slot.reusize,reusizelist[Slot.reusize]);
                  do
                  {
                    plusmin = cgetc();
                  } while (plusmin != '+' && plusmin != '-' && plusmin != CH_ENTER);
                  if(plusmin == '+')
                  {
                      Slot.reusize++;
                      if(Slot.reusize > 7) { Slot.reusize = 0; }
                  }
                  if(plusmin == '-')
                  {
                      if(Slot.reusize == 0) { Slot.reusize = 7; }
                      else { Slot.reusize--; }       
                  }
                } while (plusmin != CH_ENTER);
                strcpy(Slot.reu_image,imagename);
                Slot.command = Slot.command | COMMAND_REU;
            } else {
                sprintf(deviceidbuffer,"%d",addmountflag==1?Slot.image_a_id:Slot.image_b_id);
                cputs("Enter drive ID:       ");
                textInput(0,24,deviceidbuffer,2);
                devid = (unsigned char)strtol(deviceidbuffer,&ptrend,10);
                if(addmountflag==1) {
                    strcpy(Slot.image_a_path, pathconcat());
                    strcpy(Slot.image_a_file,imageaname);
                    Slot.image_a_id = devid;
                    Slot.command = Slot.command | COMMAND_IMGA;
                } else {
                    strcpy(Slot.image_b_path, pathconcat());
                    strcpy(Slot.image_b_file,imagebname);
                    Slot.image_b_id = devid;
                    Slot.command = Slot.command | COMMAND_IMGB;
                }
            }
        } else {
            Slot.device = pathdevice;
            strcpy(Slot.file, pathfile);
            if(runmountflag) {
                strcpy(Slot.path, "");
            } else {
                strcpy(Slot.path, pathconcat());
            }
            Slot.runboot = pathrunboot;

            if ( devicetype[pathdevice] != U64 && forceeight)
            {
                Slot.runboot = pathrunboot - EXEC_FRC8;
            }
        }

        putslottoem(menuslot);

        gotoxy(0,24);
        cputs("Saving. Please wait.          ");
        std_write("dmbootconf");
    }
}

char mainmenu()
{
    // Draw main boot menu
    // Returns chosen menu option as char key value

    int x;
    int select;
    char key;

    clrscr();
    headertext("Welcome to your Commodore 128.");

    for ( x=0 ; x<36 ; ++x )
    {
        if (SCREENW==40 && x>13)
        {
            break;
        }
        if (x>17)
        {
            gotoxy(40,x-15);
        }
        else
        {
            gotoxy(0,x+3);
        }
        
        getslotfromem(x);
        if ( strlen(Slot.menu) != 0 )
        {
            revers(1);
            textcolor(DMB_COLOR_SELECT);
            cprintf(" %2c ",menuslotkey(x));
            revers(0);
            textcolor(DC_COLOR_TEXT);
            cprintf(" %s",Slot.menu);
        }
    }

    if(SCREENW==80)
    {
        gotoxy(0,21);
    }
    else
    {
        gotoxy(0,18);
    }    
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F1 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Filebrowser");

    
    if(SCREENW==80)
    {
        gotoxy(40,21);
    }
    else
    {
        gotoxy(0,19);
    }
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F2 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Information");

    if(SCREENW==80)
    {
        gotoxy(0,22);
    }
    else
    {
        gotoxy(0,20);
    }
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F3 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Quit to C128 Basic");
    
    if(SCREENW==80)
    {
        gotoxy(40,22);
    }
    else
    {
        gotoxy(0,21);
    }
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F4 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" NTP time / GEOS config");

    if(SCREENW==80)
    {
        gotoxy(0,23);
    }
    else
    {
        gotoxy(0,22);
    }
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F5 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" C64 mode ");
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F6 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" GEOS RAM boot");

    if(SCREENW==80)
    {
        gotoxy(40,23);
    }
    else
    {
        gotoxy(0,23);
    }
    revers(1);
    textcolor(DMB_COLOR_SELECT);
    cputs(" F7 ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Edit/Reorder/Delete menu");

    cputsxy(0,24,"Make your choice.");

    select = 0;

    do
    {
        key = cgetc();
        if (key == CH_F1 || key == CH_F2 || key == CH_F3 || key == CH_F4 || key == CH_F5 || key == CH_F6 || key == CH_F7)
        {
            select = 1;
        }
        else
        {
            if((key>47 && key<58) || (key>64 && key<91))    // If keys 0 - 9 or a - z
            {
                getslotfromem(keytomenuslot(key));
                if(strlen(Slot.menu) != 0)   // Check if menslot is empty
                {
                    select = 1;
                }
            }
        }
    } while (select == 0);
    return key;    
}

void mountimage(unsigned char device, char* path, char* image) {
    uii_change_dir(path);
    uii_mount_disk(device,image);
}

void runbootfrommenu(int select)
{
    // Function to execute selected boot option choice slot 0-9
    // Input: select: chosen menuslot 0-9

    getslotfromem(select);

    clrscr();
    gotoxy(0,0);
    if(Slot.command & COMMAND_IMGA) {
        cprintf("%s on ID %d.\n\r",Slot.image_a_file,Slot.image_a_id);
        mountimage(Slot.image_a_id,Slot.image_a_path,Slot.image_a_file);
    }
    if(Slot.command & COMMAND_IMGB) {
        cprintf("%s on ID %d.\n\r",Slot.image_b_file,Slot.image_b_id);
        mountimage(Slot.image_b_id,Slot.image_b_path,Slot.image_b_file);
    }
    if(Slot.command & COMMAND_REU) {
        cprintf("REU file %s",Slot.reu_image);
        uii_change_dir(Slot.image_a_path);
        uii_open_file(1, Slot.reu_image);
        uii_load_reu(Slot.reusize);
        uii_close_file();
    }

    // Enter correct directory path on correct device number
    if(Slot.runboot & EXEC_MOUNT) {
        // Run from mounted disk
        execute(Slot.file,Slot.image_a_id,Slot.runboot,Slot.cmd);
    } else {
        // Run from hyperspeed filesystem
        cmd(Slot.device,Slot.path);
        execute(Slot.file,Slot.device,Slot.runboot,Slot.cmd);
    }
}

void commandfrommenu(char * command, int confirm)
{
    // Function to type specified command and execute by placing chars
    // in keyboard buffer.
    // Input:
    // command: command to be executed
    // confirm: is confirmation of command needed. 0 is no, 1 is yes.

    // prepare the screen with the basic command to load the next program
    exitScreen();
    gotoxy(0,2);

    cprintf("%s",command);

    // put CR in keyboard buffer
    *((unsigned char *)KBCHARS)=13;
    if (confirm == 1)  // if confirm is 1 also put 'y'+CR in buffer
    {
        *((unsigned char *)KBCHARS+1)=89;  // place 'y'
        *((unsigned char *)KBCHARS+2)=13;  // place CR
        *((unsigned char *)KBNUM)=3;
    }
    else
    {
        *((unsigned char *)KBNUM)=1;
    }

    // exit DraCopy, which will execute the BASIC LOAD above
    gotoxy(0,0);
    exit(0);
}

void editmenuoptions()
{
    // Routine for edit / re-order / delete menu slots

    int changesmade = 0;
    int select = 0;
    char key;

    do
    {
        clrscr();
        headertext("Edit/Re-order/Delete menu slots");

        presentmenuslots();

        if(SCREENW==80)
        {
            gotoxy(0,21);
        }
        else
        {
            gotoxy(0,18);
        }
        cputs("Choose:");

        if(SCREENW==80)
        {
            gotoxy(0,22);
        }
        else
        {
            gotoxy(0,19);
        }
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" F1 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Edit menuslot name");

        if(SCREENW==80)
        {
            gotoxy(40,22);
        }
        else
        {
            gotoxy(0,20);
        }
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" F2 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Edit userdefined command");

        if(SCREENW==80)
        {
            gotoxy(0,23);
        }
        else
        {
            gotoxy(0,21);
        }
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" F3 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Re-order menuslot");

        if(SCREENW==80)
        {
            gotoxy(40,23);
        }
        else
        {
            gotoxy(0,22);
        }
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" F5 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Delete menuslot");

        if(SCREENW==80)
        {
            gotoxy(0,24);
        }
        else
        {
            gotoxy(0,23);
        }
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" F7 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" Quit to main menu");

        select = 0;

        do
        {
            key = cgetc();
            if (key == CH_F1 || key == CH_F2 || key == CH_F3 || key == CH_F5 || key == CH_F7)
            {
                select = 1;
            }
        } while (select == 0);

        switch (key)
        {
        case CH_F5:
            changesmade = deletemenuslot();
            break;

        case CH_F1:
            changesmade = renamemenuslot();
            break;

        case CH_F2:
            changesmade = edituserdefinedcommand();
            break;

        case CH_F3:
            changesmade = reordermenuslot();
            break;
        
        
        default:
            break;
        }

    } while (key != CH_F7);
    
    if (changesmade == 1)
    {
        gotoxy(0,24);
        cputs("Saving. Please wait.          ");
        std_write("dmbootconf");
    } 
}

void presentmenuslots()
{
    // Routine to show the present menu slots
    
    int x;

    for ( x=0 ; x<36 ; ++x )
    {
        if (SCREENW==40 && x>14)
        {
            break;
        }
        if (x>17)
        {
            gotoxy(40,x-15);
        }
        else
        {
            gotoxy(0,x+3);
        }

        getslotfromem(x);

        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" %2c ",menuslotkey(x));
        revers(0);
        textcolor(DC_COLOR_TEXT);
        if ( strlen(Slot.menu) == 0 )
        {
            cputs(" <EMPTY>");
        }
        else
        {
            cprintf(" %s",Slot.menu);
        }
    }
}

int deletemenuslot()
{
    // Routine to delete a chosen menu slot
    // Returns 1 if something has been deleted, else 0

    int menuslot;
    int changesmade = 0;
    char key;
    char* page;
    unsigned char pagenr;
    BYTE yesno;
    BYTE selected = 0;

    clrscr();
    headertext("Delete menu slots");

    presentmenuslots();

    gotoxy(0,23);
    cputs("Choose menu slot to be deleted. ");

    do
    {
        key = cgetc();
        if ((key>47 && key<58) || (key>64 && key<91))    // If keys 0 - 9 or a - z
        {
            menuslot = keytomenuslot(key);
            selected = 1;
        }
    } while (selected == 0);

    getslotfromem(menuslot);

    cprintf("%c\n\r", key);
    if ( strlen(Slot.menu) != 0 )
    {
        cprintf("Are you sure? Y/N ");
        yesno = getkey(128);
        cprintf("%c", yesno);
        if ( yesno == 78 )
        {
            selected = 0;
        }
    }
    else
    {
        cprintf("Slot is already empty. Press key.");
        getkey(2);
        selected = 0;
    }
    if (selected == 1)
    {
        pagenr = menuslot * 2;
        page = em_use(pagenr);
        memset(page,0,256);
        em_commit();
        pagenr++;
        page = em_use(pagenr);
        memset(page,0,256);
        em_commit();
        changesmade = 1;
    }
    
    return changesmade;
}

int renamemenuslot()
{
    // Routine to rename a chosen menu slot
    // Returns 1 if something has been renamed, else 0

    int menuslot;
    int changesmade = 0;
    char key;
    BYTE yesno;
    BYTE selected = 0;

    clrscr();
    headertext("Rename menu slots");

    presentmenuslots();

    gotoxy(0,21);
    cputs("Choose menu slot to be renamed. ");

    do
    {
        key = cgetc();
        if ((key>47 && key<58) || (key>64 && key<91))    // If keys 0 - 9 or a - z
        {
            menuslot = keytomenuslot(key);
            selected = 1;
        }
    } while (selected == 0);

    getslotfromem(menuslot);
    
    cprintf("%c\n\r", key);
    if ( strlen(Slot.menu) != 0 )
    {
        cprintf("Are you sure? Y/N ");
        yesno = getkey(128);
        cprintf("%c\n\r", yesno);
        if ( yesno == 78 )
        {
            selected = 0;
        }
    }
    else
    {
        cprintf("Slot is empty. Press key.");
        getkey(2);
        selected = 0;
    }
    if (selected == 1)
    {
        gotoxy(0,23);
        cputs("Choose name for slot:");
        textInput(0,24,Slot.menu,20);
        putslottoem(menuslot);
        changesmade = 1;
    }
    
    return changesmade;
}

int reordermenuslot()
{
    // Routine to reorder a chosen menu slot
    // Returns 1 if something has been renamed, else 0

    int menuslot;
    int newslot;
    int changesmade = 0;
    char key;
    BYTE select = 0;
    char menubuffer[21];
    int oldslotbuffer;
    int x;
    int xpos;
    int ypos;
    int maxpos;

    do
    {
        clrscr();
        headertext("Re-order menu slots");

        presentmenuslots();

        cputsxy(0,22,"Choose menu slot to be re-ordered.");
        cputsxy(0,23,"Or choose ");
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cputs(" F7 ");
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cputs(" to return to main menu.");

        do
        {
            key = cgetc();    // obtain alphanumeric key
            if (key == CH_F7)
            {
                select = 1;
            }
            else
            {
                if ((key>47 && key<58) || (key>64 && key<91))   // If keys 0-9,a-z
                {
                    getslotfromem(keytomenuslot(key));
                    if(strlen(Slot.menu) != 0)   // Check if menslot is empty
                    {
                        select = 1;
                    }
                }
            }
        } while (select == 0);

        if (key != CH_F7)
        {
            clearArea(0,22,SCREENW,2);
            menuslot = keytomenuslot(key);
            getslotfromem(menuslot);
            if (menuslot>17)
            {
                xpos = 40;
                ypos = menuslot-15;
            }
            else
            {
                xpos = 0;
                ypos = menuslot+3;
            }            
            gotoxy(xpos,ypos);
            revers(1);
            textcolor(DC_COLOR_HIGHLIGHT);
            cprintf(" %2c ",menuslotkey(menuslot));
            revers(0);
            cprintf(" %s",Slot.menu);
            gotoxy(0,22);
            textcolor(DC_COLOR_TEXT);
            cputs("Move slot up or down by ");
            textcolor(DMB_COLOR_SELECT);
            cputs("cursor keys.\n\r");
            cputs("ENTER");
            textcolor(DC_COLOR_TEXT);
            cputs(" to confirm position, ");
            textcolor(DMB_COLOR_SELECT);
            cputs("F7");
            textcolor(DC_COLOR_TEXT);
            cputs(" to cancel.");

            for (x=0;x<36;x++)
            {
                getslotfromem(x);
                putslottoem(x+40);
                strcpy(newmenuname[x],Slot.menu);
                newmenuoldslot[x] = x;
            }

            newslot = menuslot;

            if (SCREENW==40)
            {
                maxpos = 14;
            }
            else
            {
                maxpos = 35;
            }
            
            do
            {

                do
                {
                    key = cgetc();
                } while (key != CH_F7 && key !=13 && key !=17 && key !=145);

                switch (key)
                {
                case 17:
                    if (newslot == maxpos)
                    {
                        strcpy(menubuffer, newmenuname[0]);
                        strcpy(newmenuname[0], newmenuname[maxpos]);
                        strcpy(newmenuname[maxpos], menubuffer);
                        oldslotbuffer = newmenuoldslot[0];
                        newmenuoldslot[0] = newmenuoldslot[maxpos];
                        newmenuoldslot[maxpos] = oldslotbuffer;
                        printnewmenuslot(maxpos,0,newmenuname[maxpos]);
                        printnewmenuslot(0,1,newmenuname[0]);
                        newslot = 0;
                    }
                    else
                    {
                        strcpy(menubuffer, newmenuname[newslot+1]);
                        strcpy(newmenuname[newslot+1], newmenuname[newslot]);
                        strcpy(newmenuname[newslot], menubuffer);
                        oldslotbuffer = newmenuoldslot[newslot+1];
                        newmenuoldslot[newslot+1] = newmenuoldslot[newslot];
                        newmenuoldslot[newslot] = oldslotbuffer;
                        printnewmenuslot(newslot,0,newmenuname[newslot++]);
                        printnewmenuslot(newslot,1,newmenuname[newslot]);
                    }                 
                    break;

                case 145:
                    if (newslot == 0)
                    {
                        strcpy(menubuffer, newmenuname[maxpos]);
                        strcpy(newmenuname[maxpos], newmenuname[0]);
                        strcpy(newmenuname[0], menubuffer);
                        oldslotbuffer = newmenuoldslot[maxpos];
                        newmenuoldslot[maxpos] = newmenuoldslot[0];
                        newmenuoldslot[0] = oldslotbuffer;
                        printnewmenuslot(0,0,newmenuname[0]);
                        printnewmenuslot(maxpos,1,newmenuname[maxpos]);
                        newslot = maxpos;
                    }
                    else
                    {
                        strcpy(menubuffer, newmenuname[newslot-1]);
                        strcpy(newmenuname[newslot-1], newmenuname[newslot]);
                        strcpy(newmenuname[newslot], menubuffer);
                        oldslotbuffer = newmenuoldslot[newslot-1];
                        newmenuoldslot[newslot-1] = newmenuoldslot[newslot];
                        newmenuoldslot[newslot] = oldslotbuffer;
                        printnewmenuslot(newslot,0,newmenuname[newslot--]);
                        printnewmenuslot(newslot,1,newmenuname[newslot]);
                    }
                    break;
                
                default:
                    break;
                }                
            } while (key != CH_F7 && key != 13);

            if (key == 13)
            {
                changesmade = 1;
                for (x=0;x<36;x++)
                {
                    getslotfromem(newmenuoldslot[x]);
                    strcpy(Slot.menu,newmenuname[x]);
                    putslottoem(x+40);
                }
                for (x=0;x<36;x++)
                {
                    getslotfromem(x+40);
                    putslottoem(x);
                }
            }
        }
    } while (key != CH_F7);

    return changesmade;
}

void printnewmenuslot(int pos, int color, char* name)
{
    // Routine to print menuslot item
    // Input: color for slotnumber
    // 0 Selectable text color
    // 1 Selected text color

    int xpos;
    int ypos;

    if (pos>17)
    {
        xpos = 40;
        ypos = pos-15;
    }
    else
    {
        xpos = 0;
        ypos = pos+3;
    }

    clearArea(xpos,ypos,40,1);
    gotoxy(xpos,ypos);
    revers(1);
    if (color == 0)
    {
        textcolor(DMB_COLOR_SELECT);
    }
    else
    {
        textcolor(DC_COLOR_HIGHLIGHT);
    }
    cprintf(" %2c ",menuslotkey(pos));
    revers(0);
    if (color == 0)
    {
        textcolor(DC_COLOR_TEXT);
    }
    else
    {
        textcolor(DC_COLOR_HIGHLIGHT);
    }
    if ( strlen(name) == 0 )
    {
        cputs(" <EMPTY>");
    }
    else
    {
        cprintf(" %s",name);
    }
}

void getslotfromem(int slotnumber)
{
    // Routine to read a menu option from extended memory page
    // Input: Slotnumber 0-36 (or 40-76 for backup)

    char* page;
    unsigned char pagenr = slotnumber * 2;

    // Calculate page address from slotnumber 
    page = em_map(pagenr);

    // Copy data from first page
    strcpy(Slot.path, page);
    page += 100;
    strcpy(Slot.menu, page);
    page += 21;
    strcpy(Slot.file, page);
    page += 20;
    strcpy(Slot.cmd, page);
    page += 80;
    strcpy(Slot.reu_image, page);
    page += 20;
    Slot.reusize = *page;
    page++;
    Slot.runboot = *page;
    page++;
    Slot.device = *page;
    page++;
    Slot.command  = *page;
    page++;
    Slot.cfgvs  = *page;

    // Copy data from second page
    pagenr++;
    page = em_map(pagenr);
    strcpy(Slot.image_a_path, page);
    page += 100;
    strcpy(Slot.image_a_file, page);
    page += 20;
    Slot.image_a_id = *page;
    page++;
    strcpy(Slot.image_b_path, page);
    page += 100;
    strcpy(Slot.image_b_file, page);
    page += 20;
    Slot.image_b_id = *page;
}

void putslottoem(int slotnumber)
{
    // Routine to write a menu option to extended memory page
    // Input: Slotnumber 0-36 (or 40-76 for backup)
    char* page;
    unsigned char pagenr = slotnumber * 2;

    // Point at first page and erase page
    page = em_use(pagenr);
    memset(page,0,256);

    // Store data in first page
    strcpy(page, Slot.path);
    page += 100;
    strcpy(page, Slot.menu);
    page += 21;
    strcpy(page, Slot.file);
    page += 20;
    strcpy(page, Slot.cmd);
    page += 80;
    strcpy(page, Slot.reu_image);
    page += 20;
    *page = Slot.reusize;
    page++;
    *page = Slot.runboot;
    page++;
    *page = Slot.device;
    page++;
    *page = Slot.command;
    page++;
    *page = Slot.cfgvs;  
    em_commit();

    // Point at first page and erase page
    pagenr++;
    page = em_use(pagenr);
    memset(page,0,256);

    // Store data in second page
    page = em_use(pagenr);
    strcpy(page, Slot.image_a_path);
    page += 100;
    strcpy(page, Slot.image_a_file);
    page += 20;
    *page = Slot.image_a_id;
    page++;
    strcpy(page, Slot.image_b_path);
    page += 100;
    strcpy(page, Slot.image_b_file);
    page += 20;
    *page = Slot.image_b_id;
    em_commit();
}

char menuslotkey(int slotnumber)
{
    // Routine to convert numerical slotnumber to key in menu
    // Input: Slotnumber = menu slot number
    // Output: Corresponding 0-9, a-z key

    if(slotnumber<10)
    {
        return slotnumber+48; // Numbers 0-9
    }
    else
    {
        return slotnumber+87; // Letters a-z
    }
}

int keytomenuslot(char keypress)
{
    // Routine to convert keypress to numerical slotnumber
    // Input: keypress = ASCII value of key pressed 0-9 or a-z
    // Output: Corresponding menuslotnumber

    if(keypress>64)
    {
        return keypress - 55;
    }
    else
    {
        return keypress - 48;
    }
}

int edituserdefinedcommand()
{
    // Routine to edit user defined command in menuslot
    // Returns 1 if something has been renamed, else 0

    int menuslot;
    int changesmade = 0;
    unsigned char key;
    BYTE selected = 0;

    clrscr();
    headertext("Edit user defined command");

    presentmenuslots();

    gotoxy(0,21);
    cputs("Choose menu slot to edit. ");

    do
    {
        key = cgetc();
        if ((key>47 && key<58) || (key>64 && key<91))    // If keys 0 - 9 or a - z
        {
            menuslot = keytomenuslot(key);
            selected = 1;
        }
    } while (selected == 0);

    getslotfromem(menuslot);
    
    cprintf("%c\n\r", key);
    if ( strlen(Slot.menu) == 0 )
    {
        cprintf("Slot is empty. Press key.");
        getkey(2);
        selected = 0;
    }
    if (selected == 1)
    {
        clrscr();
        headertext("Edit user defined command");

        gotoxy(0,3);
        cputs("Chosen slot:\n\r");
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" %c ",menuslotkey(menuslot));
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cprintf(" %s",Slot.menu);

        cputsxy(0,6,"Enter command (empty=none):");
        textInput(0,7,Slot.cmd,79);
        if( strlen(Slot.cmd) == 0)
        {
            Slot.command = 0;
        }
        else
        {
            Slot.command = 1;
        }
        
        putslottoem(menuslot);
        changesmade = 1;
    }
    
    return changesmade;
}