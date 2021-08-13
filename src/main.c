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
#include "bootmenu.h"
#include "utils.h"
#include "ultimate_lib.h"

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

//Functions
//void checkdmdevices();
//const char* deviceidtext (int id);
void std_write(char * file_name);
void std_read(char * file_name);
void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen);
char* pathconcat();
char getkey(BYTE mask);
unsigned char loadoverlay(char* name);

// Global variables
BYTE DIR1H;
BYTE DIR2H;
unsigned int SCREENW;
unsigned int MENUX;
unsigned int MENUXT;
unsigned int MENUW;
unsigned int DIR2X;
unsigned int DIR2Y;
//unsigned int validdriveid;
//unsigned int idnr[30];
char path[8][20];
char pathfile[20];
BYTE pathdevice;
BYTE pathrunboot;
BYTE depth = 0;
BYTE trace = 0;
BYTE forceeight = 0;
BYTE fastflag = 0;
struct SlotStruct Slot;
char newmenuname[36][21];
unsigned int newmenuoldslot[36];
char spacefill[81] = "                                                                                 ";
char spacedest[81];
BYTE bootdevice;
long secondsfromutc = 0; 
unsigned char timeonflag = 1;
char host[80] = "pool.ntp.org";
char reufilename[20] = "default.reu";
char reufilepath[60] = "/usb*/11/";
char imageaname[20] = "";
char imageapath[60] = "";
unsigned char imageaid = 0;
char imagebname[20] = "";
char imagebpath[60] = "";
unsigned char imagebid = 0;
unsigned char reusize = 2;
char* reusizelist[8] = { "128 KB","256 KB","512 KB","1 MB","2 MB","4 MB","8 MB","16 MB"};
unsigned char utilbuffer[328];
char configfilename[11] = "dmbcfgfile";

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

    //checkdmdevices();
    bootdevice = getcurrentdevice();    // Get device number program started from

    // Load util config
    loadoverlay("11:dmb-lowc");         // Load code in low memory
    loadoverlay("11:dmb-util");         // Load overlay for utils
    uii_change_dir("/usb*/11/");
	printf("\nDir changed\nStatus: %s", uii_status);
	readconfigfile(configfilename);

    // Set time from NTP server
    time_main();

    loadoverlay("11:dmb-menu");        // Load overlay of main DMBoot menu routines

    em_install(&c128_ram); // Load extended memory driver

    std_read("11:dmbootconf"); // Read config file
    
    initScreen(DC_COLOR_BORDER, DC_COLOR_BG, DC_COLOR_TEXT);

    cmd(bootdevice,"cd:\xff");          // Go to root of partition

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
            loadoverlay("11:dmb-fb");        // Load overlay of file browser routines
            mainLoopBrowse();
            loadoverlay("11:dmb-menu");      // Load overlay of main DMBoot menu routines
            if (trace == 1)
            {
                pickmenuslot();
            }
            break;
        
        case CH_F5:
            // Go to C64 mode
            commandfrommenu("go 64", 1);
            break;

        case CH_F6:
            // GEOS RAM boot
            clrscr();
            loadoverlay("11:dmb-geos");      // Load GEOS assembly code
            geosboot_main();
            break;

        case CH_F4:
            loadoverlay("11:dmb-util");      // Load util routines
            config_main();
            loadoverlay("11:dmb-menu");      // Load overlay of main DMBoot menu routines
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

//void checkdmdevices() {
//    //Read memory for devices recognised by Device Manager Rom
//
//    unsigned int checksum = 0x42; // Set base value for checksum
//    unsigned int x;
//
//    for (x=0; x<30; ++x) // Check for device number 0 to 30
//    {
//        idnr[x] = PEEK(0x0c00 + x);
//        checksum = checksum ^ idnr[x]; // Perform bitwise exlusive OR with checksum for each memory position
//    }
//
//    if (checksum == PEEK(0x0c00+31) ) // Compare calculated checsum with memory position where valid checksum would be
//    {
//        validdriveid = 1;
//    }
//    else
//    {
//        validdriveid = 0;
//    }   
//}
//
//const char* deviceidtext (int id)
//{
//    // Function to return device ID string based on ID value
//
//    switch( id )
//    {
//        case 0:
//            return "none";
//        case 1:
//            return "unkown";
//        case 2:
//            return "U2 A";
//        case 3:
//            return "U2 B";
//        case 4:
//            return "SD2IEC";
//        case 5:
//            return "uIEC";
//        case 6:
//            return "Printer";
//        case 7:
//            return "Plotter";
//        case 8:
//            return "SoftIEC";
//        case 40:
//            return "1540";
//        case 41:
//            return "1541";
//        case 51:
//            return "1551";
//        case 70:
//            return "1570";
//        case 71:
//            return "1571";
//        case 81:
//            return "1581";
//        default:
//            return "other";
//    }
//}

void std_write(char * file_name)
{
    char cmdbuf[20] = "s:";
    // Function to write config file
    // Input: file_name is the name of the config file

    // Set directory of boot partition to root
    cmd(bootdevice,"cp11");             // Set working partition to autoboot partition
    cmd(bootdevice,"cd:\xff");          // Go to root of partition

    // For reference: old sequential config file
    //_filetype = 's';
    //if(file = fopen(file_name, "w"))
    //{
    //    for (x=0 ; x<36 ; ++x)
    //    {
    //        getslotfromem(x);
    //        fwrite(Slot.menu, sizeof(Slot.menu),1, file);
    //        fwrite(Slot.path, sizeof(Slot.path),1, file);
    //        fwrite(Slot.file, sizeof(Slot.file),1, file);
    //        fwrite(Slot.cmd, sizeof(Slot.cmd),1, file);
    //        fputc(Slot.device, file);
    //        fputc(Slot.runboot, file);
    //        fputc(Slot.command, file);
    //    }
    //    fclose(file);
    //}
    
    // Remove old file
    strcat(cmdbuf,file_name);
    cmd(bootdevice,cmdbuf);

    // Set device ID
	cbm_k_setlfs(0, bootdevice, 0);

    // Set filename
	cbm_k_setnam(file_name);

    // Set bank to 1
    __asm__ (
		"\tlda\t#1\n"
		"\tldx\t#0\n"
		"\tjsr\t$ff68\n"
		);

    // Save BANK 1 slots
	cbm_k_save(0x0400, 0x0400 + (256*36));

    // Set load/save bank back to 0
    __asm__ (
		"\tlda\t#0\n"
		"\tldx\t#0\n"
		"\tjsr\t$ff68\n"
		);

    cmd(bootdevice,"cp0");              // Go back to main partition
}

void std_read(char * file_name)
{
    // Function to read config file
    // Input: file_name is the name of the config file

    //FILE *file;
    unsigned char x;
    unsigned int error;

    // Set directory of boot partition to root
    cmd(bootdevice,"cp11");             // Set working partition to autoboot partition
    cmd(bootdevice,"cd:\xff");          // Go to root of partition
    cmd(bootdevice,"cp0");              // Go back to main partition

    // For reference: old sequential config file
    //_filetype = 's';
    //if(file = fopen(file_name, "r"))
    //{
    //    for (x=0 ; x<36 ; ++x)
    //    {
    //        fread(Slot.menu, sizeof(Slot.menu),1, file);
    //        fread(Slot.path, sizeof(Slot.path),1, file);
    //        fread(Slot.file, sizeof(Slot.file),1, file);
    //        fread(Slot.cmd, sizeof(Slot.cmd),1, file);
    //        Slot.device = fgetc(file);
    //        Slot.runboot = fgetc(file);
    //        Slot.command = fgetc(file);
    //        putslottoem(x);
    //    }
    //    fclose(file);
    //}

    // Set device ID
	cbm_k_setlfs(0, bootdevice, 0);

    // Set filename
	cbm_k_setnam(file_name);

    // Set bank to 1
    __asm__ (
		"\tlda\t#1\n"
		"\tldx\t#0\n"
		"\tjsr\t$ff68\n"
		);

    // Load BANK 1 slots
    error = cbm_k_load(0, 0x0400);

    if(error<=0x0400)
    {
        for(x=0; x<36; ++x)
        {
            strcpy(Slot.menu,"");
            strcpy(Slot.path,"");
            strcpy(Slot.file,"");
            strcpy(Slot.cmd,"");
            strcpy(Slot.image,"");
            Slot.device = 0;
            Slot.runboot = 0;
            Slot.command = 0;
            putslottoem(x);
        }
    }

    // Set load/save bank back to 0
    __asm__ (
		"\tlda\t#0\n"
		"\tldx\t#0\n"
		"\tjsr\t$ff68\n"
		);
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

unsigned char loadoverlay(char *name)
{
    // Set directory of boot partition to root
    cmd(bootdevice,"cp11");             // Set working partition to autoboot partition
    cmd(bootdevice,"cd:\xff");          // Go to root of partition
    cmd(bootdevice,"cp0");              // Go back to main partition

    // Load overlay file, exit if not found
    if (cbm_load (name, bootdevice, NULL) == 0) {
        printf("\nLoading overlay file failed\n");
        exit(1);
    }
}
