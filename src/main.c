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
#include "version.h"
#include "base.h"
#include "cat.h"

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

//Functions
void checkdmdevices();
const char* deviceidtext (int id);
void std_write(unsigned char * file_name);
void std_read(unsigned char * file_name);
void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen);
char* pathconcat();
char getkey(BYTE mask);
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

//Variables
BYTE DIR1H;
BYTE DIR2H;
unsigned int SCREENW;
unsigned int MENUX;
unsigned int MENUXT;
unsigned int MENUW;
unsigned int DIR2X;
unsigned int DIR2Y;
unsigned int validdriveid;
unsigned int idnr[30];
char path[8][20];
char pathfile[20];
BYTE pathdevice;
BYTE pathrunboot;
BYTE depth = 0;
BYTE trace = 0;
BYTE forceeight = 0;
BYTE fastflag = 0;
struct SlotStruct {
    char path[100];
    char menu[21];
    char file[20];
    char cmd[100];
    BYTE runboot;
    BYTE device;
    BYTE command;
};
struct SlotStruct Slot;
char newmenuname[36][21];
unsigned int newmenuoldslot[36];
char spaces[81]    = "                                                                                ";
char spacedest[81];
BYTE bootdevice;

//Main program
int main() {
    int menuselect;    
    
    //Check column width of present screen
    if ( PEEK(0xee) == 79) //Memory position $ee is present screen width
    {
        SCREENW = 80;  //Set flag for 80 column
        MENUX = 58; // x position of menu
        MENUXT = MENUX + 2; // x position of menu items
        MENUW = 15; // width of menu frame
        DIR2X = DIRW+4;
        DIR2Y = 0;
        set_c128_speed(SPEED_FAST);
    }
    else
    {
        SCREENW = 40;  //Set flag for 40 column
        MENUX = 27; // x position of menu
        MENUXT = MENUX + 1; // x position of menu items
        MENUW = 13; // width of menu frame
        DIR2X = 0;
        DIR2Y = (DIR1Y+2+DIR1H);
    }

    cputs("Starting: Reading config file.");

    checkdmdevices();
    bootdevice = getcurrentdevice();    // Get device number program started from

    em_install(&c128_ram); // Load extended memory driver

    std_read("dmbootconf"); // Read config file
    
    initScreen(DC_COLOR_BORDER, DC_COLOR_BG, DC_COLOR_TEXT);

    do
    {
        menuselect = mainmenu();

        if((menuselect>47 && menuselect<58) || (menuselect>64 && menuselect<91))
        // Menuslots 0-9, a-z
        {
            runbootfrommenu(keytomenuslot(menuselect));
        }

        switch (menuselect)
        {
        case CH_F1:
            // Filebrowser
            mainLoopBrowse();
            if (trace == 1)
            {
                pickmenuslot();
            }
            break;
        
        case CH_F5:
            // Go to C64 mode
            commandfrommenu("go 64", 1);
            break;

        case CH_F4:
            // Boot from floppy
            bootfromfloppy();
            break;

        case CH_F2:
            // Information and credits
            information();
            break;
        
        case CH_F7:
            // Edit / re-order and delete menuslots
            editmenuoptions();
            break;
        
        default:
            break;
        }
    } while (menuselect != CH_F3);

    exitScreen();
    commandfrommenu("scnclr:new",0);    // Erase memory and clear screen on exit
    return 0;
}

//User defined functions

void checkdmdevices() {
    //Read memory for devices recognised by Device Manager Rom

    unsigned int checksum = 0x42; // Set base value for checksum
    unsigned int x;

    for (x=0; x<30; ++x) // Check for device number 0 to 30
    {
        idnr[x] = PEEK(0x0c00 + x);
        checksum = checksum ^ idnr[x]; // Perform bitwise exlusive OR with checksum for each memory position
    }

    if (checksum == PEEK(0x0c00+31) ) // Compare calculated checsum with memory position where valid checksum would be
    {
        validdriveid = 1;
    }
    else
    {
        validdriveid = 0;
    }   
}

const char* deviceidtext (int id)
{
    // Function to return device ID string based on ID value

    switch( id )
    {
        case 0:
            return "none";
        case 1:
            return "unkown";
        case 2:
            return "U2 A";
        case 3:
            return "U2 B";
        case 4:
            return "SD2IEC";
        case 5:
            return "uIEC";
        case 6:
            return "Printer";
        case 7:
            return "Plotter";
        case 8:
            return "SoftIEC";
        case 40:
            return "1540";
        case 41:
            return "1541";
        case 51:
            return "1551";
        case 70:
            return "1570";
        case 71:
            return "1571";
        case 81:
            return "1581";
        default:
            return "other";
    }
}

void std_write(unsigned char * file_name)
{
    // Function to write config file
    // Input: file_name is the name of the config file

    FILE *file;
    int x;

    cmd(bootdevice,"cd:/usb*/11");  // Set working dir to 11 dir at SoftIEC

    _filetype = 's';
    if(file = fopen(file_name, "w"))
    {
        for (x=0 ; x<36 ; ++x)
        {
            getslotfromem(x);
            fwrite(Slot.menu, sizeof(Slot.menu),1, file);
            fwrite(Slot.path, sizeof(Slot.path),1, file);
            fwrite(Slot.file, sizeof(Slot.file),1, file);
            fwrite(Slot.cmd, sizeof(Slot.cmd),1, file);
            fputc(Slot.device, file);
            fputc(Slot.runboot, file);
            fputc(Slot.command, file);
        }
        fclose(file);
    }
    
    cmd(bootdevice, "cd:\xff");
}

void std_read(unsigned char * file_name)
{
    // Function to read config file
    // Input: file_name is the name of the config file

    FILE *file;
    int x;

    cmd(bootdevice,"cd:/usb*/11");  // Set working dir to 11 dir at SoftIEC

    _filetype = 's';
    if(file = fopen(file_name, "r"))
    {
        for (x=0 ; x<36 ; ++x)
        {
            fread(Slot.menu, sizeof(Slot.menu),1, file);
            fread(Slot.path, sizeof(Slot.path),1, file);
            fread(Slot.file, sizeof(Slot.file),1, file);
            fread(Slot.cmd, sizeof(Slot.cmd),1, file);
            Slot.device = fgetc(file);
            Slot.runboot = fgetc(file);
            Slot.command = fgetc(file);
            putslottoem(x);
        }
        fclose(file);
    }
    else
    {
        for(x=0; x<36; ++x)
        {
            strcpy(Slot.menu,"");
            strcpy(Slot.path,"");
            strcpy(Slot.file,"");
            strcpy(Slot.cmd,"");
            Slot.device = 0;
            Slot.runboot = 0;
            Slot.command = 0;
            putslottoem(x);
        }
    }
    
    cmd(bootdevice, "cd:\xff");
}

void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen)
{       
    // Function to provide MID$ equivalent

    size_t len = min( dstlen - 1, length);
 
    strncpy(dst, src + start, len);
    // zero terminate because strncpy() didn't ? 
    if(len < length)
        dst[dstlen-1] = 0;
}

char* pathconcat()
{
    // Function to concatinate the path strings

    char concat[100] ="";
    int x;

    if ( devicetype[pathdevice] == VICE || devicetype[pathdevice] == U64)
    {
        strcat( concat, "cd:/");
    }
    else
    {
        strcat( concat, "cd//");
    }
    for (x=0 ; x < depth ; ++x)
    {
        strcat( concat, path[x] );
        strcat( concat, "/");
    }
    return concat;
}

char getkey(BYTE mask)
{
    // Function to wait for key within input validation mask
    // Mask values for input validation (adds up for combinations):
    // 00000001 =   1 = Numeric
    // 00000010 =   2 = Alpha lowercase
    // 00000100 =   4 = Alpha uppercase
    // 00001000 =   8 = Up and down
    // 00010000 =  16 = Left and right
    // 00100000 =  32 = Delete and insert
    // 01000000 =  64 = Return
    // 10000000 = 128 = Y and N

    BYTE keychar;

    do
    {
        keychar = cgetc();
    } while ( !(mask&1 && keychar > 47 && keychar < 58) && !(mask&2 && keychar > 31 && keychar < 96) && !(mask&4 && keychar > 95 && keychar < 128) && !(mask&16 && (keychar == 29 || keychar == 157)) && !(mask&8 && (keychar == 17 || keychar == 145)) && !(mask&32 && (keychar == 20 || keychar == 148)) && !(mask&64 && keychar == 13) && !(mask&128 && (keychar == 78 || keychar == 89)) );
    return keychar;    
}

void pickmenuslot()
{
    // Routine to pick a slot to store the chosen dir trace path
    
    int menuslot;
    char key;
    BYTE yesno;
    BYTE selected = 0;
    
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
        cprintf("Slot not empty. Are you sure? Y/N ");
        yesno = getkey(128);
        cprintf("%c", yesno);
        if ( yesno == 78 )
        {
            selected = 0;
        }
    }
    if ( selected == 1)
    {
        gotoxy(0,23);
        cputs("Choose name for slot:");
        strcpy(Slot.menu, pathfile);
        textInput(0,24,Slot.menu,20);

        Slot.device = pathdevice;
        strcpy(Slot.file, pathfile);
        strcpy(Slot.path, pathconcat());
        strcpy(Slot.cmd, "");
        Slot.runboot = pathrunboot;
        Slot.command = 0;

        if ( devicetype[pathdevice] != U64 && (pathrunboot == 2 || pathrunboot == 3 || pathrunboot == 12 || pathrunboot == 13))
        {
            Slot.runboot = pathrunboot - 2;
        }

        putslottoem(menuslot);

        gotoxy(0,24);
        cputs("Saving. Please wait.          ");
        std_write("dmbootconf");
    }
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
    cputs(" Boot from floppy");

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
    cputs(" C64 mode");

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
        if (key == CH_F1 || key == CH_F2 || key == CH_F3 || key == CH_F4 || key == CH_F5 || key == CH_F7)
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

void runbootfrommenu(int select)
{
    // Function to execute selected boot option choice slot 0-9
    // Input: select: chosen menuslot 0-9

    getslotfromem(select);

    // Enter correct directory path on correct device number
    cmd(Slot.device,Slot.path);
    // Execute or boot
    execute(Slot.file,Slot.device,Slot.runboot,Slot.cmd);
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

void bootfromfloppy()
{
    // Routine to boot from selected floppy drive

    char devselect[3];
    char command[20];
    int devvalue;
    int validselect = 0;
    int x;

    clrscr();
    headertext("Boot from floppy");

    if (validdriveid == 1)
    {
        cputs("Drives and drivetypes detected:\n\r");
        for (x=8; x<30; ++x)
        {
            if (idnr[x] != 0)
            {
                revers(1);
                textcolor(DMB_COLOR_SELECT);
                cprintf(" %2i ",x);
                revers(0);
                textcolor(DC_COLOR_TEXT);
                cprintf(" %s\n\r",deviceidtext(idnr[x]));
            }
        }
    }

    do
    {
        gotoxy(0,22);
        cputs("Input device number to boot from:");
        devvalue = textInput(0,23,devselect,2);
        if (devvalue <= 0 )
        {
            return;
        }
        devvalue = atoi(devselect);
        if (devvalue > 7 && devvalue < 31)
        {
            validselect = 1;
            if(validdriveid == 1)
            {
                if(idnr[devvalue] == 0)
                {
                    validselect = 0;
                }
            }
        }
        if (validselect == 0)
        {
            gotoxy(0,24);
            cputs("Invalid drive ID!");
        }
    } while (validselect == 0);

    sprintf(command,"boot u%i",devvalue);
    commandfrommenu(command,0);
}

void information()
{
    // Routine for version information and credits

    clrscr();
    headertext("Information and credits");

    cputs("DMBoot 128:\n\r");
    cputs("Device Manager Boot Menu for the C128\n\n\r");
    cprintf("Version: v%i%i-", VERSION_MAJOR, VERSION_MINOR);
    cprintf("%c%c%c%c", BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3);
    cprintf("%c%c%c%c-", BUILD_MONTH_CH0, BUILD_MONTH_CH1, BUILD_DAY_CH0, BUILD_DAY_CH1);
    cprintf("%c%c%c%c\n\r", BUILD_HOUR_CH0, BUILD_HOUR_CH1, BUILD_MIN_CH0, BUILD_MIN_CH1);
    cputs("Written in 2020 by Xander Mol.\n\n\r");
    cputs("Based on DraBrowse:\n\r");
    cputs("DraBrowse is a simple file browser.\n\r");
    cputs("Original 2009 by Sascha Bader.\n\r");
    cputs("Used version adapted by Dirk Jagdmann.\n\n\r");
    cputs("Requires and made possible by:\n\n\r");
    cputs("The C128 Device Manager ROM,\n\r");
    cputs("Created by Bart van Leeuwen.\n\n\r");
    cputs("The Ultimate II+ cartridge,\n\r");
    cputs("Created by Gideon Zweijtzer.\n\n\r");

    cputs("Press a key to coninue.");

    getkey(2);    
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
        strcpy(Slot.menu,"");
        strcpy(Slot.file,"");
        strcpy(Slot.path,"");
        strcpy(Slot.cmd,"");
        Slot.runboot = 0;
        Slot.device = 0;
        Slot.command = 0;
        changesmade = 1;
        putslottoem(menuslot);
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
            clearArea(0,22,40,2);
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
    // Input: Slotnumber = pagenumber

    char* page = em_map(slotnumber);
    strcpy(Slot.path, page);
    page = page + 100;
    strcpy(Slot.menu, page);
    page = page + 21;
    strcpy(Slot.file, page);
    page = page + 20;
    strcpy(Slot.cmd, page);
    page = page + 100;
    Slot.runboot = *page;
    page++;
    Slot.device = *page;
    page++;
    Slot.command  = *page;  
}

void putslottoem(int slotnumber)
{
    // Routine to write a menu option to extended memory page
    // Input: Slotnumber = pagenumber

    char* page = em_use(slotnumber);
    strcpy(page, Slot.path);
    page = page + 100;
    strcpy(page, Slot.menu);
    page = page + 21;
    strcpy(page, Slot.file);
    page = page + 20;
    strcpy(page, Slot.cmd);
    page = page + 100;
    *page = Slot.runboot;
    page++;
    *page = Slot.device;
    page++;
    *page = Slot.command;  
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
    char key;
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

        cputs("Chosen slot:\n\r");
        revers(1);
        textcolor(DMB_COLOR_SELECT);
        cprintf(" %c ",menuslotkey(menuslot));
        revers(0);
        textcolor(DC_COLOR_TEXT);
        cprintf(" %s",Slot.menu);
        
        gotoxy(0,10);
        cputs("Enter command string (empty for none):");
        textInput(0,11,Slot.cmd,100);
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