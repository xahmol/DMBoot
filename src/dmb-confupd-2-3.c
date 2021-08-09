#include <stdio.h>
#include <string.h>
#include <cbm.h>
#include <conio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <device.h>
#include <em.h>
#include "defines.h"

void std_write(char * file_name);
void std_read(char * file_name);
void getslotfromem(int slotnumber);
void putslottoem(int slotnumber);

struct SlotStruct Slot;
BYTE bootdevice;

int main() {
    bootdevice = getcurrentdevice();    // Get device number program started from

    clrscr();
    cputs("DM Boot: Upgrade config from 1.99 to 2.99 \n\r");
    cputs("Loading extended memory driver...\n\r");
    em_install(&c128_ram); ; // Load extended memory driver 
    cputs("Read old config....\n\r");
    std_read("dmbootconf"); // Read config file old format
    cputs("Write in new format...\n\r");
    std_write("dmbootconf"); // Write config new format
    cputs("Finished.\n\r");

    em_uninstall();
    return 0;
}

void std_write(char * file_name)
{
    // Function to write config file
    // Input: file_name is the name of the config file

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
    remove(file_name);

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
}

void std_read(char * file_name)
{
    // Function to read config file
    // Input: file_name is the name of the config file

    FILE *file;
    int x;

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
