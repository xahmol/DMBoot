/*
DMBoot 128 v5 - Exec overlay: run a slot, go 64, exit

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot

Slot start (mounts with USB port rerouting, REU image) ported from
src/slotmenu.c of my UBoot64-v2 project (https://github.com/xahmol/UBoot64-v2);
program start (commands on screen + RETURNs in the keyboard buffer, Force 8,
C64 mode, FAST, BOOT) as in DMBoot v4 (branch legacy-cc65, src/ops.c).
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <petscii.h>
#include <c64/vic.h>
#include "defines.h"
#include "banking.h"
#include "dmapi.h"
#include "geosboot.h"
#include "dualwin.h"
#include "ultimate_common_lib.h"
#include "ultimate_dos_lib.h"
#include "dmpaths.h"
#include "core.h"
#include "fileio.h"
#include "exec.h"

#pragma overlay(dmbovl5, 6)
#pragma section(codeovl5, 0)
#pragma section(dataovl5, 0)
#pragma section(bssovl5, 0)
#pragma region(ovl5, OVERLAYLOAD, OVERLAY_SLOT_END, , 6, { codeovl5, dataovl5, bssovl5 })

#pragma code(codeovl5)
#pragma data(dataovl5)
#pragma bss(bssovl5)

#define EXEC_LINES_MAX      4       // Command lines put on screen
#define EXEC_LINE_MAX       100     // Longest command line (cmd 80 + extras)
#define EXEC_FIRST_ROW      2       // Row of the first command line
#define EXEC_LINE_SPACING   3       // Rows reserved per line (output + READY.)
#define CHR_RETURN          0x0d
#define CHR_HOME            0x13
#define CHR_DOWN            0x11    // Cursor down
#define CHR_YES             0x59    // 'y' key, confirms "go 64"
#define DOS_STATUS_NOTFOUND "82,"   // Ultimate: file not found (keep hunting)
#define TEXT_MAX            81
#define DEVICE_FORCED       8
#define DELAY_SHOW_STATUS   2       // Seconds to show the path status
#define DELAY_DRIVE_READY   2       // Seconds for an Ultimate drive to start after power on
#define EXEC_DRIVE_A        0       // uii_devinfo index of drive A
#define EXEC_DRIVE_B        1       // uii_devinfo index of drive B
#define DM_API_RUN64_MIN    2       // run64 needs API version > 1 (as DMBoot v4)
#define DM_API_HSID_MIN     1       // set hyperspeed ID needs API version > 0

// Command lines to execute after the exit to BASIC
static char execlines[EXEC_LINES_MAX][EXEC_LINE_MAX];
static char execcount;

// ---------------------------------------------------------------------------
// Title:       Add a command line
// Description: Adds a line to the list executed after the exit; ignored
//              when the list is full. The text is truncated to fit.
// Syntax:      void exec_add_line(const char *text);
// Input:       text - PETSCII command line
// Output:      None
// ---------------------------------------------------------------------------
static void exec_add_line(const char *text)
{
    if (execcount >= EXEC_LINES_MAX)
    {
        return;
    }
    strncpy(execlines[execcount], text, EXEC_LINE_MAX - 1);
    execlines[execcount][EXEC_LINE_MAX - 1] = 0;
    execcount++;
}

// ---------------------------------------------------------------------------
// Title:       KERNAL character out
// Description: Prints one character through the KERNAL screen editor.
// Syntax:      void exec_chrout(char ch);
// Input:       ch - PETSCII character
// Output:      None
// ---------------------------------------------------------------------------
static void exec_chrout(char ch)
{
    __asm
    {
        lda ch
        jsr $ffd2
    }
}

// ---------------------------------------------------------------------------
// Title:       Exit to BASIC and run the command lines
// Description: Hands the screen back to the KERNAL (clears it), prints the
//              command lines from row 2 down (as DMBoot v4 does;
//              positioned with HOME and cursor-down control characters)
//              (3 rows apart, room for their output and READY.), puts one
//              RETURN per line (plus extra keys) in the keyboard buffer,
//              restores 1 MHz and the MMU set-up and exits. BASIC prints
//              READY. and then executes the lines one by one.
// Syntax:      void exec_to_basic(const char *extrakeys);
// Input:       extrakeys - keys to add after the RETURNs (may be "")
// Output:      Does not return
// ---------------------------------------------------------------------------
static void exec_to_basic(const char *extrakeys)
{
    volatile char *keybuffer = (volatile char *)KEYBUF_ADDRESS;
    char row = EXEC_FIRST_ROW;
    char keys = 0;

    *(volatile char *)VIC_CLOCK_REG &= ~VIC_CLOCK_FAST;

    // Give the KERNAL its own clean screen set-up back (also clears it)
    dwin_exit();

    // Position with KERNAL control characters (as the screen editor
    // expects): HOME, then down to row 2; lines 3 rows apart (v4 layout)
    exec_chrout(CHR_HOME);
    for (char r = 0; r < EXEC_FIRST_ROW; r++)
    {
        exec_chrout(CHR_DOWN);
    }
    for (char line = 0; line < execcount; line++)
    {
        for (char i = 0; execlines[line][i]; i++)
        {
            exec_chrout(execlines[line][i]);
        }
        // Next line start: column 0, EXEC_LINE_SPACING rows further down
        char rows = strlen(execlines[line]) / dwin_state.width + EXEC_LINE_SPACING;
        exec_chrout(CHR_HOME);
        row += rows;
        for (char r = 0; r < row; r++)
        {
            exec_chrout(CHR_DOWN);
        }
    }
    exec_chrout(CHR_HOME);

    while (keys < execcount && keys < KEYBUF_SIZE)
    {
        keybuffer[keys++] = CHR_RETURN;
    }
    while (*extrakeys && keys < KEYBUF_SIZE)
    {
        keybuffer[keys++] = *extrakeys++;
    }
    *(volatile char *)ZP_KEYBUF_COUNT = keys;

    bnk_exit();
    dmb_exit();
}

// ---------------------------------------------------------------------------
// Title:       Print an ASCII value
// Description: Prints a label and a value in ASCII (from the Ultimate) as
//              one console line.
// Syntax:      void exec_print(const char *label, const char *ascii);
// Input:       label - PETSCII label
//              ascii - ASCII text
// Output:      None
// ---------------------------------------------------------------------------
static void exec_print(const char *label, const char *ascii)
{
    char text[TEXT_MAX];

    asc2pet(text, ascii, sizeof(text));
    dwin_put_string(&console, label, cfg.colors.text);
    dwin_put_string(&console, text, cfg.colors.text);
    dwin_put_char(&console, '\n', cfg.colors.text);
}

// ---------------------------------------------------------------------------
// Title:       Check a mount result
// Description: Stops with the Ultimate status when a mount or REU load
//              failed.
// Syntax:      void ErrorCheckMounting(void);
// Input:       None (uii_status)
// Output:      Returns only on success
// ---------------------------------------------------------------------------
static void ErrorCheckMounting(void)
{
    if (!UII_SUCCESS)
    {
        exec_print("Error: ", uii_status);
        errorexit("Mounting failed.");
    }
}

// ---------------------------------------------------------------------------
// Title:       Is this a USB port path
// Description: Tells whether a path starts with "/usbX/" (a numbered port
//              or the /usb*/ wildcard), which can be rerouted to another
//              USB port.
// Syntax:      bool exec_is_usb_path(const char *path);
// Input:       path - ASCII path
// Output:      true for "/usbX/..." paths
// ---------------------------------------------------------------------------
static bool exec_is_usb_path(const char *path)
{
    return memcmp(path, storagepaths[STORAGE_USB_FIRST], STORAGE_PORT_PREFIX - 2) == 0 &&
           path[STORAGE_PORT_PREFIX - 1] == storagepaths[STORAGE_USB_FIRST][STORAGE_PORT_PREFIX - 1];
}

// ---------------------------------------------------------------------------
// Title:       Wait for the USB stick
// Description: Asks the user to insert the USB stick; F7 gives up.
// Syntax:      void exec_ask_stick(void);
// Input:       None
// Output:      Returns for a retry; F7 exits to BASIC
// ---------------------------------------------------------------------------
static void exec_ask_stick(void)
{
    dwin_put_string(&console, "\nInsert USB stick. Key=retry, F7=BASIC\n", cfg.colors.error);
    if (key_wait() == KEY_F7)
    {
        errorexit("USB stick not found.");
    }
}

// ---------------------------------------------------------------------------
// Title:       Power on an Ultimate drive
// Description: Switches Ultimate drive A or B on when it is off and waits
//              until it is ready, as UBoot64-v2 ToggleDrivePower does.
//              Mounting right after switching a drive on fails with
//              "90,drive not present".
// Syntax:      static void exec_drive_power_on(char drive);
// Input:       drive - EXEC_DRIVE_A or EXEC_DRIVE_B
// Output:      None (errors exit through ErrorCheckMounting)
// ---------------------------------------------------------------------------
static void exec_drive_power_on(char drive)
{
    if (!uii_parse_deviceinfo())
    {
        ErrorCheckMounting();
    }
    if (uii_devinfo[drive].power)
    {
        return;
    }

    if (drive == EXEC_DRIVE_A)
    {
        uii_enable_drive_a();
    }
    else
    {
        uii_enable_drive_b();
    }
    dwin_printf(&console, cfg.colors.text, "Drive %c powered on.\n", 'A' + drive);
    delay(DELAY_DRIVE_READY);
}

// ---------------------------------------------------------------------------
// Title:       Mount an image, rerouting USB ports
// Description: Mounts a disk image on a device. When the image is not found
//              ("82,") and the path is on a USB port, the same path is tried
//              on the other USB ports (the stick may have moved). Other
//              errors stop immediately. The path in the Slot copy may be
//              changed for this start only; the saved slot is untouched.
// Syntax:      void mountimage(char device, char *path, char *image);
// Input:       device - device ID to mount on
//              path   - ASCII directory of the image (may be changed)
//              image  - ASCII image file name
// Output:      Returns when mounted
// ---------------------------------------------------------------------------
static void mountimage(char device, char *path, char *image)
{
    while (true)
    {
        uii_change_dir(path);
        if (UII_SUCCESS)
        {
            uii_mount_disk(device, image);
            if (UII_SUCCESS)
            {
                return;
            }
            if (strncmp(uii_status, DOS_STATUS_NOTFOUND, sizeof(DOS_STATUS_NOTFOUND) - 1) != 0)
            {
                ErrorCheckMounting();
            }
        }

        if (exec_is_usb_path(path))
        {
            for (char x = STORAGE_USB_FIRST; x < STORAGE_CANDIDATES; x++)
            {
                if (memcmp(path, storagepaths[x], STORAGE_PORT_PREFIX) == 0)
                {
                    continue;
                }
                memcpy(path, storagepaths[x], STORAGE_PORT_PREFIX);
                uii_change_dir(path);
                if (!UII_SUCCESS)
                {
                    continue;
                }
                uii_mount_disk(device, image);
                if (UII_SUCCESS)
                {
                    exec_print("Rerouted to ", path);
                    return;
                }
            }
        }
        exec_ask_stick();
    }
}

// ---------------------------------------------------------------------------
// Title:       Load an REU image, rerouting USB ports
// Description: Loads an REU image into the REU, retrying the path on the
//              other USB ports when it cannot be opened. Must be the last
//              step before the start: it overwrites the slots in the REU.
// Syntax:      void load_reu_with_reroute(char *path, char *image,
//                                         char reusize);
// Input:       path    - ASCII directory (may be changed)
//              image   - ASCII REU file name
//              reusize - REU size index 0-7
// Output:      Returns when loaded
// ---------------------------------------------------------------------------
static void load_reu_with_reroute(char *path, char *image, char reusize)
{
    bool found = false;

    while (!found)
    {
        uii_change_dir(path);
        if (UII_SUCCESS)
        {
            uii_open_file(0x01, image);
            found = UII_SUCCESS;
        }
        if (!found && exec_is_usb_path(path))
        {
            for (char x = STORAGE_USB_FIRST; x < STORAGE_CANDIDATES && !found; x++)
            {
                if (memcmp(path, storagepaths[x], STORAGE_PORT_PREFIX) == 0)
                {
                    continue;
                }
                memcpy(path, storagepaths[x], STORAGE_PORT_PREFIX);
                uii_change_dir(path);
                if (UII_SUCCESS)
                {
                    uii_open_file(0x01, image);
                    found = UII_SUCCESS;
                }
            }
        }
        if (!found)
        {
            exec_ask_stick();
        }
    }
    uii_load_reu(reusize);
    uii_close_file();
}

// ---------------------------------------------------------------------------
// Title:       Demo mode
// Description: Powers down the Ultimate drives that are not on ID 8, then
//              asks to switch off every other device that needs manual
//              switching (see iec_scan) until none is left, or until the
//              user chooses to ignore them (some loaders, such as Krill's,
//              silence other drives themselves).
//              As UBoot64-v2 DoDemoMode; added: the ignore option.
// Syntax:      static void DoDemoMode(void);
// Input:       None (uii_devinfo, dminfo)
// Output:      None
// ---------------------------------------------------------------------------
static void DoDemoMode(void)
{
    char active[IEC_ID_COUNT];

    if (uii_devinfo[0].exist && uii_devinfo[0].power && uii_devinfo[0].id != DEVICE_FORCED)
    {
        uii_disable_drive_a();
        dwin_put_string(&console, "Drive A powered off.\n", cfg.colors.text);
    }
    if (uii_devinfo[1].exist && uii_devinfo[1].power && uii_devinfo[1].id != DEVICE_FORCED)
    {
        uii_disable_drive_b();
        dwin_put_string(&console, "Drive B powered off.\n", cfg.colors.text);
    }

    while (true)
    {
        if (!uii_parse_deviceinfo())
        {
            ErrorCheckMounting();
        }
        if (!iec_scan(active))
        {
            dwin_put_string(&console, "Only ID 8 is active.\n", cfg.colors.ok);
            return;
        }

        dwin_put_string(&console, "Switch off ID", cfg.colors.text);
        for (char x = 0; x < IEC_ID_COUNT; x++)
        {
            if (iec_needs_switching(active[x], iec_index_to_id(x)))
            {
                dwin_printf(&console, cfg.colors.text, " %u", iec_index_to_id(x));
            }
        }
        dwin_put_string(&console, " and press a key,\nor I to ignore (loader switches them off).\n",
                        cfg.colors.text);
        char key = dwin_getch();
        if (key == 'i' || key == 'I')
        {
            dwin_put_string(&console, "Ignored.\n", cfg.colors.text);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Title:       Start a program
// Description: Builds the BASIC command lines for the start options of a
//              slot and exits to BASIC to execute them: user command, then
//              RUN (or BOOT, LOAD ,1 + RUN, or C64 mode via the Device
//              Manager ROM). An empty program name only runs the command.
// Syntax:      void execute(const char *prg, char device, char boot,
//                           const char *command);
// Input:       prg     - program file name (PETSCII), may be empty
//              device  - device to run from
//              boot    - EXEC_* flags
//              command - user command (PETSCII), may be empty
// Output:      Does not return
// ---------------------------------------------------------------------------
static void execute(const char *prg, char device, char boot, const char *command)
{
    char line[EXEC_LINE_MAX];
    const char *fast = (boot & EXEC_FAST) ? "fast:" : "";

    execcount = 0;
    if (boot & EXEC_DEMO)
    {
        DoDemoMode();
    }
    if (command[0])
    {
        exec_add_line(command);
    }

    // BOOT needs no file name (as v4); RUN, LOAD and C64 mode do
    if (boot & EXEC_RUN64)
    {
        if (prg[0] && dminfo.present && dm_version() >= DM_API_RUN64_MIN && dm_prepare_run64(prg, device))
        {
            sprintf(line, "sys %u", dm_run64_address());
            exec_add_line(line);
        }
        else
        {
            errorexit("C64 mode needs a file name and Device Manager API v2.");
        }
    }
    else
    {
        if (boot & EXEC_FRC8)
        {
            if (dminfo.present && dm_version() >= DM_API_HSID_MIN)
            {
                dm_api_set_hsid8();
            }
            else
            {
                exec_add_line("poke 673,8");
            }
            device = DEVICE_FORCED;
        }

        if (boot & EXEC_BOOT)
        {
            sprintf(line, "%sboot u%u", fast, device);
            exec_add_line(line);
        }
        else if (prg[0] && (boot & EXEC_COMMA1))
        {
            sprintf(line, "load\"%s\",%u,1", prg, device);
            exec_add_line(line);
            sprintf(line, "%srun", fast);
            exec_add_line(line);
        }
        else if (prg[0])
        {
            sprintf(line, "%srun\"%s\",u%u", fast, prg, device);
            exec_add_line(line);
        }
    }
    exec_to_basic("");
}

// ---------------------------------------------------------------------------
// Title:       Run a slot
// Description: Starts a menu slot: mounts its images (rerouting USB ports),
//              loads its REU image last, then starts the program.
// Syntax:      void runbootfrommenu(char select);
// Input:       select - slot number
// Output:      Does not return
// ---------------------------------------------------------------------------
void runbootfrommenu(char select)
{
    get_slot_from_reu(select);

    dwin_clear(&screenwin);
    headertext("Starting slot", 0);
    dwin_init(&console, 0, 3, 0, 0);
    dwin_put_string(&console, Slot.menu, cfg.colors.text);
    dwin_put_char(&console, '\n', cfg.colors.text);

    if (Slot.command & COMMAND_IMGA)
    {
        exec_drive_power_on(EXEC_DRIVE_A);
        exec_print("Mount A: ", Slot.image_a_file);
        mountimage(Slot.image_a_id, Slot.image_a_path, Slot.image_a_file);
        delay(1);
    }
    if (Slot.command & COMMAND_IMGB)
    {
        exec_drive_power_on(EXEC_DRIVE_B);
        exec_print("Mount B: ", Slot.image_b_file);
        mountimage(Slot.image_b_id, Slot.image_b_path, Slot.image_b_file);
        delay(1);
    }
    // The REU image goes last: it overwrites the slots in the REU
    if (Slot.command & COMMAND_REU)
    {
        exec_print("REU: ", Slot.reu_image);
        load_reu_with_reroute(Slot.reu_path, Slot.reu_image, Slot.reusize);
        ErrorCheckMounting();
    }

    // Firmware 3.15 hook (plan §9): select Slot.partition here when set.

    if (Slot.runboot & EXEC_MOUNT)
    {
        execute(Slot.file, Slot.image_a_id, Slot.runboot, Slot.cmd);
    }
    if (Slot.path[0])
    {
        // Change to the program's directory (as v4: also for BOOT slots,
        // which have no file name); show the drive's reply and stop on an
        // error instead of letting RUN or BOOT fail later.
        // "cd:/..." is relative on the SoftIEC drive, so first return to
        // the root, as v4 did before every overlay load (the file browser
        // may have left the drive in a subdirectory)
        drive_root_reset();
        char status = cmd(Slot.device, Slot.path);
        if (cfg.verbose || status)
        {
            dwin_put_string(&console, "Path: ", cfg.colors.text);
            dwin_put_string(&console, Slot.path, cfg.colors.text);
            dwin_put_string(&console, " -> ", cfg.colors.text);
            dwin_put_string(&console, DOSstatus, status ? cfg.colors.error : cfg.colors.ok);
            dwin_put_char(&console, '\n', cfg.colors.text);
        }
        if (status)
        {
            errorexit("Changing to the program directory failed.");
        }
        delay(DELAY_SHOW_STATUS);
    }
    execute(Slot.file, Slot.device, Slot.runboot, Slot.cmd);
}

// ---------------------------------------------------------------------------
// Title:       Start a program chosen in the file browser
// Description: Starts browsereq.file from the current directory of
//              browsereq.device with the browser's run flags.
// Syntax:      void exec_browse(void);
// Input:       browsereq (set by the file browser)
// Output:      Does not return
// ---------------------------------------------------------------------------
void exec_browse(void)
{
    dwin_clear(&screenwin);
    headertext("Starting program", 0);
    dwin_init(&console, 0, 3, 0, 0);
    execute(browsereq.file, browsereq.device, browsereq.runboot, "");
}

// ---------------------------------------------------------------------------
// Title:       GEOS RAM boot
// Description: Mounts the configured GEOS disk images on drives A and B,
//              loads the GEOS REU image last (nothing may return to the
//              menu after that: the slots in the REU are overwritten) and
//              starts GEOS from it through the low-memory code.
//              As DMBoot v4 geosboot_main, with drive power-on and USB
//              port rerouting of the slot start.
// Syntax:      void exec_geos(void);
// Input:       cfg.geos
// Output:      Does not return
// ---------------------------------------------------------------------------
void exec_geos(void)
{
    struct GeosConfig *geos = &cfg.geos;

    dwin_clear(&screenwin);
    headertext("GEOS RAM boot", 0);
    dwin_init(&console, 0, 3, 0, 0);

    if (!geos->reu_image[0])
    {
        errorexit("No GEOS REU image configured (F4, F8).");
    }
    if (geos->image_a_id && geos->image_a_file[0])
    {
        exec_drive_power_on(EXEC_DRIVE_A);
        exec_print("Mount A: ", geos->image_a_file);
        mountimage(geos->image_a_id, geos->image_a_path, geos->image_a_file);
    }
    if (geos->image_b_id && geos->image_b_file[0])
    {
        exec_drive_power_on(EXEC_DRIVE_B);
        exec_print("Mount B: ", geos->image_b_file);
        mountimage(geos->image_b_id, geos->image_b_path, geos->image_b_file);
    }
    exec_print("REU: ", geos->reu_image);
    load_reu_with_reroute(geos->reu_path, geos->reu_image, geos->reusize);
    ErrorCheckMounting();

    dwin_put_string(&console, "Starting GEOS.\n", cfg.colors.text);
    if (geos_boot() == GEOS_ERROR_NOREU)
    {
        errorexit("No REU found.");
    }
    errorexit("The REU image holds no GEOS boot loader.");
}

// ---------------------------------------------------------------------------
// Title:       Go to C64 mode
// Description: Exits to BASIC with "go 64" and confirms the question.
// Syntax:      void exec_go64(void);
// Input:       None
// Output:      Does not return
// ---------------------------------------------------------------------------
void exec_go64(void)
{
    static const char confirm[3] = { CHR_YES, CHR_RETURN, 0 };

    execcount = 0;
    exec_add_line("go 64");
    exec_to_basic(confirm);
}

// ---------------------------------------------------------------------------
// Title:       Exit to BASIC
// Description: Exits to BASIC and clears the screen and memory.
// Syntax:      void exec_exit_to_basic(void);
// Input:       None
// Output:      Does not return
// ---------------------------------------------------------------------------
void exec_exit_to_basic(void)
{
    execcount = 0;
    exec_add_line("scnclr:new");
    exec_to_basic("");
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
