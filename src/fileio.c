/*
DMBoot 128 v5 - Config and slot file I/O (REU <-> Ultimate file system)

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Ported from src/fileio.c of my UBoot64-v2 project
(https://github.com/xahmol/UBoot64-v2), adapted: chunked config I/O (the
DMBoot config is larger than the 512 byte UCI data queue), REU writes
capped at the slot area, success checks with UII_SUCCESS, C128 REU access.
*/

#include <string.h>
#include <petscii.h>
#include <c64/vic.h>
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "reu128.h"
#include "dualwin.h"
#include "dmpaths.h"
#include "core.h"
#include "fileio.h"
#include "cfgdefaults.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

#define FILE_READ           0x01    // uii_open_file: read
#define FILE_CREATE         0x06    // uii_open_file: write, create new
#define SLOTS_BYTES         ((unsigned long)SLOTSIZE * SLOTS)
#define STATUS_TEXT_MAX     41

// Transfer buffer for chunked writes (bank 0)
static char save_buffer[SAVE_BUF_SIZE];

// ---------------------------------------------------------------------------
// Title:       Check UCI status
// Description: Stops with an error when the last UCI command failed.
// Syntax:      void CheckStatus(const char *message);
// Input:       message - what was being done (PETSCII)
// Output:      Returns only on success
// ---------------------------------------------------------------------------
void CheckStatus(const char *message)
{
    char statustext[STATUS_TEXT_MAX];

    if (UII_SUCCESS)
    {
        return;
    }
    asc2pet(statustext, uii_status, sizeof(statustext));
    uii_abort();
    dwin_printf(&console, cfg.colors.error, "\nI/O error in %s.\nStatus: ", message);
    dwin_put_string(&console, statustext, cfg.colors.error);
    errorexit("");
}

// ---------------------------------------------------------------------------
// Title:       Get a slot from the REU
// Description: Loads slot number into the Slot working copy.
// Syntax:      void get_slot_from_reu(char number);
// Input:       number - slot number 0..SLOTS-1 (others are ignored)
// Output:      Slot
// ---------------------------------------------------------------------------
void get_slot_from_reu(char number)
{
    if (number >= SLOTS)
    {
        return;
    }
    reu128_load(SLOT_REU_START + (unsigned long)number * SLOTSIZE, (volatile char *)&Slot, SLOTSIZE);
}

// ---------------------------------------------------------------------------
// Title:       Save a slot to the REU
// Description: Stores the Slot working copy as slot number.
// Syntax:      void save_slot_to_reu(char number);
// Input:       number - slot number 0..SLOTS-1 (others are ignored)
// Output:      None
// ---------------------------------------------------------------------------
void save_slot_to_reu(char number)
{
    if (number >= SLOTS)
    {
        return;
    }
    reu128_store(SLOT_REU_START + (unsigned long)number * SLOTSIZE, (volatile char *)&Slot, SLOTSIZE);
}

// ---------------------------------------------------------------------------
// Title:       Write the slots file
// Description: Writes all slots from the REU to the slots file, in chunks
//              of SAVE_BUF_SIZE bytes.
// Syntax:      void write_slotsfile(void);
// Input:       None
// Output:      None (stops with an error on failure)
// ---------------------------------------------------------------------------
void write_slotsfile(void)
{
    unsigned long address = SLOT_REU_START;
    unsigned long end = SLOT_REU_START + SLOTS_BYTES;

    uii_change_dir(configpath);
    uii_delete_file(slotfilename);          // Overwrite does not work: delete first
    uii_open_file(FILE_CREATE, slotfilename);
    CheckStatus("creating slots file");

    while (address < end)
    {
        unsigned length = (end - address < SAVE_BUF_SIZE) ? (unsigned)(end - address) : SAVE_BUF_SIZE;

        reu128_load(address, (volatile char *)save_buffer, length);
        uii_write_file(save_buffer, length);
        CheckStatus("writing slots");
        address += length;
        spinning();
    }
    uii_close_file();
}

// ---------------------------------------------------------------------------
// Title:       Create empty slots
// Description: Fills all slots in the REU with empty slots.
// Syntax:      void create_empty_slots(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
static void create_empty_slots(void)
{
    memset(&Slot, 0, sizeof(Slot));
    Slot.cfgvs = CFGVERSION;
    for (char x = 0; x < SLOTS; x++)
    {
        save_slot_to_reu(x);
    }
}

// ---------------------------------------------------------------------------
// Title:       Read the slots file
// Description: Reads the slots file into the REU. Creates a file with
//              empty slots when there is none. Data beyond the slot area
//              is ignored, a short file leaves the remaining slots empty.
//              Stops when the file has an older format version.
// Syntax:      void read_slotsfile(void);
// Input:       None
// Output:      None
// ---------------------------------------------------------------------------
void read_slotsfile(void)
{
    unsigned long address = SLOT_REU_START;
    unsigned long end = SLOT_REU_START + SLOTS_BYTES;

    uii_change_dir(configpath);
    uii_open_file(FILE_READ, slotfilename);
    if (!UII_SUCCESS)
    {
        progress("No slots file found, creating empty slots.");
        create_empty_slots();
        write_slotsfile();
        return;
    }

    // Start from empty slots, so a short file leaves the rest empty
    create_empty_slots();

    uii_read_file((unsigned)SLOTS_BYTES);
    while (uii_isdataavailable() || uii_ismoredataavailable())
    {
        unsigned bytesread = uii_readdata();
        uii_accept();
        CheckStatus("reading slots");

        // Never write beyond the slot area in the REU
        if (address < end)
        {
            if (bytesread > end - address)
            {
                bytesread = (unsigned)(end - address);
            }
            reu128_store(address, (volatile char *)uii_data, bytesread);
            address += bytesread;
        }
        spinning();
    }
    uii_close_file();

    get_slot_from_reu(0);
    if (Slot.cfgvs < CFGVERSION)
    {
        errorexit("Old slot file format. Run dmbupd45 first.");
    }
}

// ---------------------------------------------------------------------------
// Title:       Configuration defaults
// Description: Sets cfg to the default configuration.
// Syntax:      void config_defaults(void);
// Input:       None
// Output:      cfg
// ---------------------------------------------------------------------------
void config_defaults(void)
{
    config_set_defaults(&cfg);
}

// ---------------------------------------------------------------------------
// Title:       Write the config file
// Description: Writes cfg to the config file in chunks of SAVE_BUF_SIZE.
// Syntax:      void writeconfigfile(void);
// Input:       None
// Output:      None (stops with an error on failure)
// ---------------------------------------------------------------------------
void writeconfigfile(void)
{
    const char *data = (const char *)&cfg;
    unsigned remaining = sizeof(cfg);

    uii_change_dir(configpath);
    uii_delete_file(configfilename);
    uii_open_file(FILE_CREATE, configfilename);
    CheckStatus("creating config file");

    while (remaining)
    {
        unsigned length = remaining < SAVE_BUF_SIZE ? remaining : SAVE_BUF_SIZE;
        memcpy(save_buffer, data, length);
        uii_write_file(save_buffer, length);
        CheckStatus("writing config");
        data += length;
        remaining -= length;
    }
    uii_close_file();
}

// ---------------------------------------------------------------------------
// Title:       Read the config file
// Description: Reads the config file into cfg in chunks. Writes a default
//              config when there is none. A shorter (older) file leaves the
//              newer fields zero. Stops when the file has an older format
//              version.
// Syntax:      void readconfigfile(void);
// Input:       None
// Output:      cfg
// ---------------------------------------------------------------------------
void readconfigfile(void)
{
    char *data = (char *)&cfg;
    unsigned filled = 0;

    uii_change_dir(configpath);
    uii_open_file(FILE_READ, configfilename);
    if (!UII_SUCCESS)
    {
        config_defaults();
        progress("No config file found, writing defaults.");
        writeconfigfile();
        return;
    }

    memset(&cfg, 0, sizeof(cfg));
    uii_read_file(sizeof(cfg));
    while (uii_isdataavailable() || uii_ismoredataavailable())
    {
        unsigned bytesread = uii_readdata();
        uii_accept();
        CheckStatus("reading config");

        // Never write beyond cfg
        if (bytesread > sizeof(cfg) - filled)
        {
            bytesread = sizeof(cfg) - filled;
        }
        memcpy(data + filled, uii_data, bytesread);
        filled += bytesread;
    }
    uii_close_file();

    if (cfg.version < CFGVERSION)
    {
        config_defaults();
        errorexit("Old config file format. Run dmbupd45 first.");
    }
    // Validate settings that select from a list
    if (cfg.verbose >= VERBOSE_OPTIONS)
    {
        cfg.verbose = VERBOSE_ON;
    }
}
