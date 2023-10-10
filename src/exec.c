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

#pragma code-name ("OVERLAY6");
#pragma rodata-name ("OVERLAY6");

void ErrorCheckMmounting() {
// Error handling for disk and REU mounting

    if(!uii_success()) {
        cprintf("Error on mounting.\n\r");
        cprintf("%s\n\r",uii_status);
        exit(0);
    }
}

void mountimage(unsigned char device, char* path, char* image) {
    cprintf("Change dir to:\n\r%s\n\r",path);
    uii_change_dir(path);
    ErrorCheckMmounting();
    cprintf("Mount image on ID %d:\n\r%s\n\r",device,image);
    uii_mount_disk(device,image);
    ErrorCheckMmounting();
}

void runbootfrommenu(int select)
{
    // Function to execute selected boot option choice slot 0-9
    // Input: select: chosen menuslot 0-9

    getslotfromem(select);

    clrscr();
    gotoxy(0,0);
    if(Slot.command & COMMAND_IMGA) {
        mountimage(Slot.image_a_id,Slot.image_a_path+3,Slot.image_a_file);
    }
    if(Slot.command & COMMAND_IMGB) {
        mountimage(Slot.image_b_id,Slot.image_b_path+3,Slot.image_b_file);
    }
    if(Slot.command & COMMAND_REU) {
        cprintf("Change dir to:\n\r%s\n\r",Slot.image_a_path+3);
        uii_change_dir(Slot.image_a_path+3);
        ErrorCheckMmounting();
        cprintf("Open REU file %s\n\r",Slot.reu_image);
        uii_open_file(1, Slot.reu_image);
        ErrorCheckMmounting();
        cprintf("Loading.\n\r",Slot.reu_image);
        uii_load_reu(Slot.reusize);
        ErrorCheckMmounting();
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
