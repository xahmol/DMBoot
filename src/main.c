// DMBoot 128:
// Device Manager Boot Menu for the Commodore 128
// Written in 2020-2023 by Xander Mol
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
#include "bootmenu.h"
#include "utils.h"
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "ultimate_time_lib.h"
#include "dmapi.h"
#include "vdc.h"
#include "exec.h"

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

//Functions
//void checkdmdevices();
//const char* deviceidtext (int id);
void dm_getapiversion();
unsigned char dm_getdevicetype(unsigned char id);
void dm_sethsidviaapi();
void std_write(char * file_name);
void std_read(char * file_name);
void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen);
void cspaces(unsigned char number);
char* pathconcat();
char getkey(BYTE mask);
unsigned char loadoverlay(char* name);

// Global variables
BYTE SCREENW;
BYTE DIRW;
BYTE MENUX;
char path[8][20];
char pathfile[20];
BYTE pathdevice;
BYTE pathrunboot;
BYTE depth = 0;
BYTE trace = 0;
BYTE forceeight = 0;
BYTE fastflag = 0;
BYTE commandflag = 0;
BYTE reuflag = 0;
BYTE addmountflag = 0;
BYTE runmountflag = 0;
BYTE mountflag = 0;

struct SlotStruct Slot;
char newmenuname[36][21];
unsigned int newmenuoldslot[36];
BYTE bootdevice;
long secondsfromutc = 0; 
unsigned char timeonflag = 1;
char host[80] = "pool.ntp.org";
char imagename[20] = "default.reu";
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
unsigned int dm_apiversion = 0;
unsigned char configversion = CFGVERSION;
unsigned char vdcmemory;
unsigned int vdc_alloc_address;

//Main program
int main() {
    int menuselect;

    cputs("Starting DMBoot.\n\r");
 
    //Check column width of present screen
    if ( PEEK(0xee) == 79) //Memory position $ee is present screen width
    {
        SCREENW = 80;  //Set flag for 80 column
        DIRW = 51;
        MENUX = 65;
        set_c128_speed(SPEED_FAST);
    }
    else
    {
        SCREENW = 40;  //Set flag for 40 column
        DIRW = 25;
        MENUX = 25;
    }

    if(!uii_detect()) {
        cputs("No Ultimate Command Interface enabled.\n\r");
        return 1;
    }

    //checkdmdevices();
    bootdevice = getcurrentdevice();    // Get device number program started from

    // Load util config
    loadoverlay("11:dmb-lowc");         // Load code in low memory
    loadoverlay("11:dmb-util");         // Load overlay for utils

    dm_getapiversion();
    if(dm_apipresent==1)
    {
        cprintf("\n\rDM API version: %i\n\r",dm_apiversion);
    }

    uii_change_dir("/usb*/11/");
	cprintf("\n\rDir changed\n\rStatus: %s\n\n\r", uii_status);
	readconfigfile(configfilename);

    // Set time from NTP server
    time_main();

    // Initialize drivers and memory for boot menu
    loadoverlay("11:dmb-menu");         // Load overlay of main DMBoot menu routines
    em_install(&c128_ram);              // Load extended memory driver

    // Load slot config
    cputs("\n\rReading slot data.");
    std_read("11:dmbootconf"); // Read config file

    //Uncomment to debug based on init feedback
    //cgetc();

    // Detect VDC memory size
    vdcmemory = VDC_DetectVDCMemSize();
    if(vdcmemory==64) {VDC_SetExtendedVDCMemSize(); }

    // Init screen and menu
    initScreen(DC_COLOR_BORDER, DC_COLOR_BG, DC_COLOR_TEXT);
    cmd(bootdevice,"cd:\xff");          // Go to root of partition
    do
    {
        menuselect = mainmenu();

        if((menuselect>47 && menuselect<58) || (menuselect>64 && menuselect<91))
        // Menuslots 0-9, a-z
        {
            loadoverlay("11:dmb-exec");
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
        
        case CH_F2:
            // Information and credits
            loadoverlay("11:dmb-util");      // Load util routines
            information();
            loadoverlay("11:dmb-menu");      // Load overlay of main DMBoot menu routines
            break;
        
        case CH_F3:
            // Edit / re-order and delete menuslots
            editmenuoptions();
            break;

        case CH_F4:
            loadoverlay("11:dmb-util");      // Load util routines
            config_main();
            loadoverlay("11:dmb-menu");      // Load overlay of main DMBoot menu routines
            break;
        
        case CH_F5:
            // Go to C64 mode
            loadoverlay("11:dmb-exec");
            commandfrommenu("go 64", 1);
            break;

        case CH_F6:
            // GEOS RAM boot
            clrscr();
            textcolor(DC_COLOR_TEXT);
            loadoverlay("11:dmb-geos");      // Load GEOS assembly code
            geosboot_main();
            break;

        default:
            break;
        }
    } while (menuselect != CH_F7);

    exitScreen();
    loadoverlay("11:dmb-exec");
    commandfrommenu("scnclr:new",0);    // Erase memory and clear screen on exit
    return 0;
}

//User defined functions
void dm_getapiversion()
{
    dm_getapiversion_core();
    dm_apiversion = dm_apiverhigb * 256 + dm_apiverlowb;
}

unsigned char dm_getdevicetype(unsigned char id)
{
    if(dm_apipresent==1)
    {
        dm_gethsidviaapi();
        if(dm_devid==id) {
            dm_devtype = 0x08;
        } else {
            dm_devtype = id;
            dm_getdevicetype_core();
        }
    }
    else
    {
        dm_devtype=0;
    }
    
    switch (dm_devtype)
    {
    case 0x02:
    case 0x03:
    case 0x08:
        return 14;                      // Return code for Ultimate II+ drives
        break;

    case 0x09:
        return 1;                       // Return code for Pi1541
        break;

    case 0x04:
    case 0x05:
        return 11;                      // Return code for SDIEC/mIEC
        break;

    case 0x28:
        return 2;                       // 1540
        break;

    case 0x29:
        return 3;                       // 1541
        break;

    case 0x46:
        return 5;                       // 1570
        break;

    case 0x47:
        return 6;                       // 1571
        break;
    
    case 0x51:
        return 7;                       // 1581
        break;

    case 0x80:
    case 0xc0:
    case 0xe0:
    case 0xf0:
        return 12;                      // CMD drives
        break;

    default:
        return 0;                       // All other / unrecognised / error
        break;
    }
}

void std_write(char * file_name)
{
    char cmdbuf[20] = "s:";
    // Function to write config file
    // Input: file_name is the name of the config file

    // Set directory of boot partition to root
    cmd(bootdevice,"cp11");             // Set working partition to autoboot partition
    cmd(bootdevice,"cd:\xff");          // Go to root of partition
   
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
	cbm_k_save(0x0400, 0x0400 + (256*72));

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
            strcpy(Slot.reu_image,"");
            Slot.device = 0;
            Slot.runboot = 0;
            Slot.command = 0;
            Slot.cfgvs = CFGVERSION;
            strcpy(Slot.image_a_path,"");
            strcpy(Slot.image_a_file,"");
            Slot.image_a_id = 0;
            strcpy(Slot.image_b_path,"");
            strcpy(Slot.image_b_file,"");
            Slot.image_b_id = 0;
            putslottoem(x);
        }
    } else {
        getslotfromem(0);
        if(Slot.cfgvs < CFGVERSION) {
            printf("\n\rOld configuration file format.");
            printf("\n\rRun upgrade tool first.");
            exit(1);
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

void cspaces(unsigned char number)
{
    /* Function to print specified number of spaces, cursor set by conio.h functions */

    unsigned char x;

    for(x=0;x<number;x++) { cputc(32); }
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

void headertext(char* subtitle)
{
    // Draw header text
    // Input: subtitle is text to draw on second line

    revers(1);
    textcolor(DMB_COLOR_HEADER1);
    gotoxy(0,0);
    cspaces(SCREENW);
    gotoxy(0,0);  
    cprintf("DMBoot 128: Device Manager Boot Menu");
    textcolor(DMB_COLOR_HEADER2);
    gotoxy(0,1);
    cspaces(SCREENW);
    gotoxy(0,1);
    cprintf("%s\n\n\r", subtitle);
    if(SCREENW == 80)
    {
        uii_get_time();
        cputsxy(80-strlen((const char*)uii_data),1,(const char*)uii_data);
    }
    revers(0);
    textcolor(DC_COLOR_TEXT);
}
