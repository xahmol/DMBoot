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

char DOSstatus[40];

struct SlotStruct Slot;
BYTE bootdevice;

int main() {
    unsigned char slot;

    bootdevice = getcurrentdevice();    // Get device number program started from

    clrscr();
    cputs("DM Boot: Upgrade config from 2.99 to 3.9x \n\r");
    cputs("Loading extended memory driver...\n\r");
    em_install(&c128_ram); ; // Load extended memory driver 
    cputs("Read old config....\n\r");
    std_read("dmbootconf"); // Read config file old format
    cputs("Convert to new format.\n\r");

    for(slot=0;slot<36;slot++) {
        getslotfromem(slot);
        putslottoem(slot+40);
    }

    cputs("Write in new format...\n\r");
    std_write("dmbootconf"); // Write config new format
    cputs("Finished.\n\r");

    em_uninstall();
    return 0;
}

BYTE
dosCommand(const BYTE lfn, const BYTE drive, const BYTE sec_addr, const char *cmd)
{
  int res;
  if (cbm_open(lfn, drive, sec_addr, cmd) != 0)
    {
      return _oserror;
    }

  if (lfn != 15)
    {
      if (cbm_open(15, drive, 15, "") != 0)
        {
          cbm_close(lfn);
          return _oserror;
        }
    }

  DOSstatus[0] = 0;
  res = cbm_read(15, DOSstatus, sizeof(DOSstatus));

  if(lfn != 15)
    {
      cbm_close(15);
    }
  cbm_close(lfn);

  if (res < 1)
    {
      return _oserror;
    }

  return (DOSstatus[0] - 48) * 10 + DOSstatus[1] - 48;
}

int
cmd(const BYTE device, const char *cmd)
{
  return dosCommand(15, device, 15, cmd);
}

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
	cbm_k_save(0x0400 + (256*80), 0x0400 + (256*156));

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
        cputs("\n\rError in loading config file.\n\n\r");
        exit(1);
    }

    // Set load/save bank back to 0
    __asm__ (
		"\tlda\t#0\n"
		"\tldx\t#0\n"
		"\tjsr\t$ff68\n"
		);
}

void getslotfromem(int slotnumber)
{
    // Routine to read a menu option from extended memory page
    // Input: Slotnumber = pagenumber

    unsigned char old;

    char* page = em_map(slotnumber);
    strcpy(Slot.path, page);
    page = page + 100;
    strcpy(Slot.menu, page);
    page = page + 21;
    strcpy(Slot.file, page);
    page = page + 20;
    strcpy(Slot.cmd, page);
    page = page + 100;
    strcpy(Slot.reu_image, page);
    page += 20;
    Slot.reusize = 0;

    // Recalculate runboot flag
    old = *page;
    Slot.runboot = 0;
    if(old == 1 || old == 11) { Slot.runboot += EXEC_BOOT; }
    if(old == 2 || old == 12) { Slot.runboot += EXEC_FRC8; }
    if(old == 5) { Slot.runboot += EXEC_RUN64; }
    if(old>9) { Slot.runboot += EXEC_FAST; }

    page++;
    Slot.device = *page;
    page++;

    // Recalculate command flag
    old  = *page;
    Slot.command = 0;
    Slot.image_a_id = 0;
    strcpy(Slot.image_a_path, "");
    strcpy(Slot.image_a_file, "");
    if(old == 1) { Slot.command += COMMAND_CMD; }
    if(old > 1) {
        Slot.command += COMMAND_IMGA;
        Slot.image_a_id = old/2;
        strcpy(Slot.image_a_path, Slot.cmd);
        strcpy(Slot.image_a_file, Slot.reu_image);
        strcpy(Slot.cmd,"");
    }
    strcpy(Slot.reu_image,"");

    Slot.cfgvs  = CFGVERSION;
    strcpy(Slot.image_b_path, "");
    strcpy(Slot.image_b_file, "");
    Slot.image_b_id = 0;
}

void putslottoem(int slotnumber)
{
    // Routine to write a menu option to extended memory page
    // Input: Slotnumber 0-36 (or 40-76 for backup)
    char* page;

    // Calculate page address from slotnumber
    slotnumber = slotnumber *2;    
    page = em_use(slotnumber++);

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
    Slot.reusize = *page;
    page++;
    *page = Slot.runboot;
    page++;
    *page = Slot.device;
    page++;
    *page = Slot.command;
    page++;
    *page = Slot.cfgvs;  
    em_commit();

    // Store data in second page
    page = em_use(slotnumber);
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
