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
char menupath[10][100];
char menuname[10][21];
char menufile[10][20];
unsigned int menurunboot[10];
unsigned int menudevice[10];
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

    checkdmdevices();
    bootdevice = getcurrentdevice();    // Get device number program started from
    std_read("dmbootconf"); // Read config file
    
    initScreen(DC_COLOR_BORDER, DC_COLOR_BG, DC_COLOR_TEXT);

    do
    {
        menuselect = mainmenu();

        switch (menuselect)
        {
        case 'f':
            // Filebrowser
            mainLoopBrowse();
            if (trace == 1)
            {
                pickmenuslot();
            }
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            // Menuslots 0-9
            runbootfrommenu(menuselect - 48);
            break;
        
        case 'c':
            // Go to C64 mode
            commandfrommenu("go 64", 1);
            break;

        case 'b':
            // Boot from floppy
            bootfromfloppy();
            break;

        case 'i':
            // Information and credits
            information();
            break;
        
        default:
            break;
        }
    } while (menuselect != 'q');

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
        for (x=0 ; x<10 ; ++x)
        {
            fwrite(menuname[x], sizeof(menuname[x]),1, file);
            fwrite(menupath[x], sizeof(menupath[x]),1, file);
            fwrite(menufile[x], sizeof(menufile[x]),1, file);
            fputc(menudevice[x], file);
            fputc(menurunboot[x], file);
        }
        fclose(file);
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
        for (x=0 ; x<10 ; ++x)
        {
            fread(menuname[x], sizeof(menuname[x]),1, file);
            fread(menupath[x], sizeof(menupath[x]),1, file);
            fread(menufile[x], sizeof(menufile[x]),1, file);
            menudevice[x] = fgetc(file);
            menurunboot[x] = fgetc(file);
        }
        fclose(file);
    }
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
    } while ( !(mask&1 && keychar > 47 && keychar < 58) && !(mask&2 && keychar > 31 && keychar < 96) && !(mask&4 && keychar > 95 && keychar < 128) && !(mask&8 && (keychar == 29 || keychar == 157)) && !(mask&16 && (keychar == 17 || keychar == 145)) && !(mask&32 && (keychar == 20 || keychar == 148)) && !(mask&64 && keychar == 13) && !(mask&128 && (keychar == 78 || keychar == 89)) );
    return keychar;    
}

void pickmenuslot()
{
    // Routine to pick a slot to store the chosen dir trace path
    
    int x;
    int menuslot;
    BYTE yesno;
    BYTE selected = 0;
    
    clrscr();
    headertext("Choose menuslot for chosen start option.");
    cputs("Present menu slots:\n\n\r");
    for ( x=0 ; x<10 ; ++x )
    {
        revers(1);
        textcolor(COLOR_CYAN);
        cprintf(" %i ",x);
        revers(0);
        textcolor(DC_COLOR_TEXT);
        if ( strlen(menuname[x]) == 0 )
        {
            cputs(" <EMPTY>\n\r");
        }
        else
        {
            cprintf(" %s\n\r",menuname[x]);
        }
    }
    cputs("\nChoose slot by pressing number: ");
    menuslot = getkey(1) - 48;
    selected = 1 ;
    cprintf("%i\n\r", menuslot);
    if ( strlen(menuname[menuslot]) != 0 )
    {
        cprintf("Slot not empty. Are you sure? Y/N ");
        yesno = getkey(128);
        cprintf("%c\n\r", yesno);
        if ( yesno == 78 )
        {
            selected = 0;
        }
    }
    if ( selected == 1)
    {
        gotoxy(0,18);
        cputs("Choose name for slot:");
        strcpy(menuname[menuslot],pathfile);
        textInput(0,19,menuname[menuslot],20);

        menudevice[menuslot] = pathdevice;
        strcpy(menufile[menuslot],pathfile);
        strcpy(menupath[menuslot],pathconcat());
        menurunboot[menuslot] = pathrunboot;

        if ( devicetype[pathdevice] == U64)
        {
            gotoxy(0,20);
            cputs("Device ID = 8 required? Y/N ");
            yesno = getkey(128);
            cprintf("%c\n\r", yesno);
            if ( yesno == 89 )
            {
                menurunboot[menuslot] = pathrunboot + 2;
            }
        }

        std_write("dmbootconf");
    }
}

void headertext(char* subtitle)
{
    // Draw header text
    // Input: subtitle is text to draw on second line

    mid(spaces,1,SCREENW,spacedest, sizeof(spacedest)); // select spaces based on screenwidth
    revers(1);
    textcolor(COLOR_GREEN);
    gotoxy(0,0);
    cprintf("%s\n",spacedest);
    gotoxy(0,0);  
    cprintf("DMBoot 128: Device Manager Boot Menu");
    textcolor(COLOR_LIGHTGREEN);
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
    int select = 0;
    char key;

    clrscr();
    headertext("Welcome to your Commodore 128.");

    for ( x=0 ; x<10 ; ++x )
    {
        if ( strlen(menuname[x]) != 0 )
        {
            revers(1);
            textcolor(COLOR_CYAN);
            cprintf(" %i ",x);
            revers(0);
            textcolor(DC_COLOR_TEXT);
            cprintf(" %s\n\r",menuname[x]);
        }
    }

    revers(1);
    textcolor(COLOR_CYAN);
    cputs(" F ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Filebrowser\n\r");

    revers(1);
    textcolor(COLOR_CYAN);
    cputs(" Q ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Quit to C128 Basic\n\r");

    revers(1);
    textcolor(COLOR_CYAN);
    cputs(" C ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" C64 mode\n\r");

    revers(1);
    textcolor(COLOR_CYAN);
    cputs(" B ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Boot from floppy\n\r");

    revers(1);
    textcolor(COLOR_CYAN);
    cputs(" I ");
    revers(0);
    textcolor(DC_COLOR_TEXT);
    cputs(" Information\n\r");

    cputs("\nMake your choice.");

    do
    {
        key = getkey(2);    // obtain alphanumeric key
        if (key == 'f' || key == 'q' || key == 'c' || key == 'b' || key == 'i')
        {
            select = 1;
        }
        else
        {
            if(key>47 && key<58)    // If keys 0 - 9
            {
                if(strlen(menuname[key-48]) != 0)   // Check if menslot is empty
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

    // Enter correct directory path on correct device number
    cmd(menudevice[select],menupath[select]);
    // Execute or boot
    execute(menufile[select],menudevice[select],menurunboot[select]);
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
                textcolor(COLOR_CYAN);
                if (x < 10)
                {
                    cputs(" ");
                }
                cprintf(" %i ",x);
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
    clrscr();
    headertext("Information and credits");

    cputs("DMBoot 128:\n\r");
    cputs("Device Manager Boot Menu for the C128\n\r");
    cputs("Written in 2020 by Xander Mol.\n\n\r");
    cputs("Based on DraBrowse:\n\r");
    cputs("DraBrowse is a simple file browser.\n\r");
    cputs("Originally created 2009 by Sascha Bader.\n\r");
    cputs("Used version adapted by Dirk Jagdmann.\n\n\r");
    cputs("Requires and made possible by:\n\n\r");
    cputs("The C128 Device Manager ROM,\n\r");
    cputs("Created by Bart van Leeuwen.\n\n\r");
    cputs("The Ultimate II+ cartridge,\n\r");
    cputs("Created by Gideon Zweijtzer.\n\n\r");

    cputs("Press a key to coninue.");

    getkey(2);    
}