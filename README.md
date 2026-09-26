## DMBoot 128

Device Manager Boot Menu for the Commodore 128

Written in 2020-2026 by Xander Mol

<https://github.com/xahmol/DMBoot>

<https://www.idreamtin8bits.com/>

DMBoot is a boot menu and file browser for the Commodore 128 with an Ultimate II+ cartridge. The C128 Device Manager ROM starts it automatically. It offers 36 menu slots that start a program, boot a disk, or run a command, with optional disk image mounts and an REU image per slot. It also has a file browser to set these up, NTP time, and GEOS RAM boot. It works in 40 and 80 columns.

> **Draft for v5.0.0.** This README describes the rebuilt v5. Screenshots are still from v4 and will be replaced.

### Changelog

**Version 5.0.0** (in development, branch `Oscar64Rebuild`)

A complete rebuild in the Oscar64 C compiler, along the lines of the C64 sibling project [UBoot64](https://github.com/xahmol/UBoot64-v2).

* Works in both 40 and 80 columns. The screen that is active at start-up is used. In 40 columns the 36 slots are shown on two pages (cursor left/right).
* Long names and paths: menu names up to 30 characters, file names up to 50, paths up to 255.
* Directory listings are stored in the REU, so large directories (thousands of entries) fit.
* Mount paths and REU image paths are stored per slot, separately (v4 had to share one path between the drive A image and the REU file).
* If a disk image or REU file is not found on the USB port stored in the slot, the other USB ports are tried (for when the stick moved).
* Default slot with an auto-boot countdown (off, 1, 3, 5 or 10 seconds): a fully unattended start from power-on.
* Command-only slots, and slots that only mount images.
* Demo mode: before starting, Ultimate drives not on ID 8 are switched off, and other devices are listed for switching off by hand (or ignored, for loaders such as Krill's that silence them themselves).
* `,1` load option, for machine language programs.
* Ultimate drives A/B are switched on automatically when a slot needs them.
* Start-up feedback: silent, show messages, or show messages and wait for a key. Shows the Ultimate drives and all active IEC devices with their drive type (via the Device Manager API).
* Configurable colours, one scheme for 40 and 80 columns.
* NTP time sync with three servers, tried in turn; off by default, as firmware 3.14d and later sync the clock themselves.
* Upgrade tool `dmbupd45` converts the v4 slots and settings.
* Bug fixes compared with v4, among others: C64 mode, BASIC programs with variables after a slot start (zero page), locked files in the directory listing, BOOT slots without a file name.

**Older versions:** see [the changelog of v1.99 to v4](#older-versions) at the end.

### Requirements

* Ultimate II+ cartridge with the Ultimate Command Interface enabled (Ultimate menu, C64 and Cartridge Settings). Firmware 3.9 or newer; see the note on firmware 3.15 below.
* REU enabled in the Ultimate, at least 128 KB. DMBoot keeps its slots and directory listings in the REU and overwrites what is in it at start-up. Slots that need specific REU contents load their own REU image.
* C128 Device Manager ROM by Bart van Leeuwen as cartridge ROM on the Ultimate II+: <https://www.bartsplace.net/content/publications/devicemanager128.shtml>. Starting in C64 mode needs Device Manager API version 2 or newer.

**Firmware 3.15 and newer:** SoftIEC partitions are not supported yet. DMBoot uses the `/11/` convention of the Device Manager ROM and waits for a Device Manager update for 3.15.

### Installation

* Create a directory called `11` on your USB stick and unzip the DMBoot ZIP into it. The Device Manager ROM starts `autostart.128.prg` from there.
* Files:
  * `autostart.128.prg`: DMBoot itself.
  * `dmblmc.prg`, `dmbovl1.prg` to `dmbovl5.prg`: program parts loaded at start-up (not programs of their own).
  * `dmbupd45.prg`: upgrade tool for DMBoot v4 settings.
  * `README.pdf`: this manual.
* DMBoot writes its settings to `dmbconf.cfg` and its slots to `dmbslots.cfg` in the same directory.

### Upgrading from DMBoot v4

v4 stored its slots in `dmbootconf.prg` and its NTP/GEOS settings in `DMBCFGFILE`. v5 does not read them directly, but the upgrade tool converts them. The v4 files are not changed, so you can go back.

1. Copy the v5 files into the `11` directory (keep a copy of your v4 `autostart.128.prg` if you want to go back).
2. Start the upgrade tool from BASIC: `RUN"11:DMBUPD45",U11` (with the hyperspeed drive on ID 11).
3. It shows the converted slots and writes `dmbslots.cfg` and `dmbconf.cfg`. If v5 files already exist, it asks first.
4. Reset: the Device Manager ROM starts DMBoot v5 with your slots.

If you start v5 with v4 files but without v5 files, DMBoot asks whether to start with empty slots or to run the upgrade tool first.

Notes on the conversion:
* Slots keep their key, name, path, file, command, flags, mounts and REU image.
* The NTP on/off setting and UTC offset are kept; a v4 NTP server you chose yourself becomes server 1.
* v4 slots that BOOT from a disk image that uses a fast loader such as Krill's need the image mounted on drive A with demo mode (the hyperspeed drive cannot run drive code). Change these in the slot editor.
* A GEOS image without a path gets the DMBoot directory. Check it in the configuration (F4, F6).

### Main menu

| Key | Function |
|---|---|
| **0-9, A-Z** | Start the slot (Shift + letter works too) |
| **F1** | File browser |
| **F2** | Information |
| **F3** | Edit, re-order and delete slots |
| **F4** | Configuration |
| **F5** | Go to C64 mode |
| **F6** | GEOS RAM boot |
| **F7** | Quit to BASIC |
| **F8** | Switch to the other screen (40 or 80 columns). It stays active for the slot you start and for BASIC; the next boot follows the 40/80 key again. Your monitor must show the other screen (second input or cable) |
| **Cursor left/right** | 40 columns: other page of slots |

With a default slot and an auto-boot timeout, a countdown starts that slot. Any key opens the menu instead.

**Starting a slot** does, in this order: switch on and mount the drive A and B images, load the REU image (last: after that DMBoot cannot return to the menu, because the REU is overwritten), change to the slot's directory, then put the slot's command and the RUN/BOOT/LOAD statement on one BASIC line (joined with `:`) and exit to BASIC to run it. The command therefore must not end the line itself (for example with `RUN` or `GOTO`).

### F1: File browser

The browser shows the IEC devices, starting with the Device Manager's hyperspeed drive. With dirtrace off, RETURN starts a program directly. With dirtrace on (**D**), the choice goes into a slot: DMBoot asks for the slot and a name.

| Key | Function |
|---|---|
| **F1** | Read the directory again |
| **S** | Sort on/off (slow on very large directories) |
| **+ / -** | Next / previous active device |
| **RETURN** | Start the program, or enter the directory or disk image. On an REU image (dirtrace on): add it to a slot |
| **DEL** | Parent directory (40 columns: also cursor left) |
| **↑** | Root directory |
| **Cursor keys** | Move; in 80 columns left/right switch between the two columns |
| **T / HOME, E** | First / last entry |
| **P / U** | Page down / up |
| **D** | Dirtrace on/off |
| **F5** | BOOT the current directory or disk image |
| **6** | Start the program in C64 mode (Device Manager) |
| **A / B** | Add the selected disk image to a slot as mount on drive A / B |
| **M** | Add the selected program as "mount the image you are in on drive A and start the program from it" |
| **8** | Force 8 on/off: the hyperspeed drive becomes ID 8 before starting |
| **F** | FAST on/off: start in 2 MHz mode (80 columns) |
| **1** | `,1` load on/off: `LOAD"name",id,1` + `RUN` |
| **O** | Demo mode on/off |
| **F7 / Q** | Back to the main menu |

Disk image mounts, **M** and REU images need the dirtrace on the hyperspeed drive, because slots store them as paths on the Ultimate file system.

### F2: Information

Shows the version, the Ultimate, the REU size, the screen mode, the Device Manager API version and the credits.

### F3: Edit, re-order and delete

| Key | Function |
|---|---|
| **F1** | Rename a slot |
| **F2** | Command of a slot: a BASIC line run before the program. On an empty slot this creates a command-only slot |
| **F3** | Re-order: pick a slot, move it with cursor up/down (left/right: to the other page), RETURN keeps the new place, F7 cancels |
| **F4** | Auto-boot timeout: off, 1, 3, 5, 10 seconds |
| **F5** | Delete a slot (asks first) |
| **F6** | Set or clear the default slot ([D] behind its name) |
| **F7** | Back; changes are saved now |

### F4: Configuration

| Key | Function |
|---|---|
| **F1** | NTP time sync at start-up on/off (default off, see below) |
| **F2** | Start-up: silent, show messages, or show messages + wait for a key |
| **F3** | Offset to UTC in seconds (e.g. 3600 for CET, 7200 for CEST; no automatic daylight saving time) |
| **F4** | The three NTP servers, edited in turn (empty = not used, STOP = keep). Defaults: time.google.com, time.windows.com, pool.ntp.org |
| **F5** | Colours: cursor up/down chooses, left/right changes, DEL undoes, F7 back |
| **F6** | GEOS RAM boot: REU image and size, disk images for drives A and B |
| **F7** | Back; changes are saved now |

**NTP time sync and the Ultimate firmware:** from firmware 3.14d the Ultimate sets its clock itself (Ultimate menu, Network settings: SNTP Enable, time zone and three time servers). DMBoot's own time sync is therefore off by default; switch it on (F1) only for older firmware. DMBoot asks the servers in turn until one answers. The UTC offset (F3) is only used by DMBoot's own sync.

### F5: C64 mode

Types `GO 64` and confirms it.

### F6: GEOS RAM boot

Mounts the configured disk images, loads the GEOS REU image and starts GEOS from it. Configure it first in F4, F6. The REU image must hold a GEOS system saved with its RAM boot loader (for example made with GEOS's own RAM boot tools or MegaPatch).

Demonstration of booting GEOS via DMBoot v4 (click the picture for the video on YouTube):

[![](https://img.youtube.com/vi/u9hQ0eEtpeI/0.jpg)](https://www.youtube.com/watch?v=u9hQ0eEtpeI)

### F7: Quit to BASIC

Clears the BASIC program area (`SCNCLR:NEW`) and returns to BASIC in SLOW mode.

### Credits

Based on DraBrowse: DraBrowse (db*) is a simple file browser, originally created 2009 by Sascha Bader. Used version adapted by Dirk Jagdmann (doj). <https://github.com/doj/dracopy>

Uses code from:
* Ultimate 64/II+ Command Library by Scott Hutter and Francesco Sblendorio: <https://github.com/xlar54/ultimateii-dos-lib>
* ntp2ultimate by MaxPlap (NTP time): <https://github.com/MaxPlap/ntp2ultimate>
* EPOCH-to-time-date-converter by sidsingh78: <https://github.com/sidsingh78/EPOCH-to-time-date-converter>
* GRB128, GEOS 128 RAM boot, by Bart van Leeuwen (public domain)
* cc65 (C128 function key handling): <https://github.com/cc65/cc65>

Built with the Oscar64 C compiler by DrMortalWombat: <https://github.com/drmortalwombat/oscar64>

Requires and made possible by the C128 Device Manager ROM, created by Bart van Leeuwen: <https://www.bartsplace.net/content/publications/devicemanager128.shtml>

Requires and made possible by the Ultimate II+ cartridge, created by Gideon Zweijtzer: <https://ultimate64.com/>

The code can be used freely as long as you retain a notice describing original sources and authors.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!

### Older versions

The changelog of DMBoot v1.99 to v4 (`v391` builds). These versions were built with cc65; their source is on branch `legacy-cc65`.

**Version v391-20231011-1210:**

[Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v391-20231011-1210.zip)

* Fix of a serious bug causing changing directories to fail via UCI on mounting images or loading a REU file. This causes that mounting images and REU files only succeeded if they were placed in the present working directory (which is usally /11)
* Made F7 exit in the main menu for consistency throughout the program, so F3 became Edit/Reorder/Rename/Delete slots
* Memory optimalisations, a.o. moving image mounting and REU loading functions to a seperate overlay (dmb-exec.prg)
* Added much more comprehensive error handling and text feedback on mounting images and loading REU

**Version v391-20230819-1737:**

[Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v391-20230819-1737.zip)

* Second public alpha of DMBoot v4 with a completely new filebrowser.
* The file browser part that in previous version was just a slightly adapted DraBrowse has now for a large part been rewritten to be able to store the directory entries in free VDC memory. On C128s with 16 KB VDC memory this already solves memory issues I had, causing the maximum amount of direntries that could be loaded to be too low for my taste (less than 70 entries). Now it loads up to 175 entries, IMHO perfectly acceptable.
If your C128 has 64 KB VDC however, a whopping 48 KB memory is now available, which should be enough for 2.137 dir entries. Did not test that, as I do not have a dir so large, but did manage to succesfully have a testdir with 300 subdirs in memory completely. Would almost dare you to test the limits, I know there is always someone with so many items in a dir that he can still break this limit 😉
* As I had to rewrite the filebrowser part for this anyway, I have also changed some other things:
Removed the option in 80 column mode to show two drives at once. Maybe looked cool, but IMHO showing two dirs at once makes only sense for a copyer, not a browser. More importantly, need for two dirs in memory costs valuable memory space and makes allocation routines way more complicates (especially as I could no longer use simple alloc() and pointers as VDC memory is not directly accesible). Instead of this, in 80 column mode now two columns are used to show the directory of the present drives, doubling the number of items that can be shown. Think this makes way more sense for a browser;
* Previously, cursor right triggers a dir change and cursor left a dir up (if possible). With two columns this is counter intuitive, you just want to move left and right. So changed behaviour of cursor left and right to enable sideway navigation. Of course you can still select directories by the ENTER key to go in a dir, and the DEL key to go back.
* Added a page down (P key) and page up (U key) function to quicker navigate large dirs.
* Sort still works, but as VDC memory management is slower, and also now much larger dirs are supported, beware to use it in large dirs as kit then gets very slow.
* Rest should still work unaltered in browser mode (please let me know otherwise).
* File browser now starts with the device ID of the hyperspeed drive (if active), instead of just 8. For me this makes more sense as in DMBoot most logical use of the filebrowser is selecting things on the hyperspeed device ID. Of course the other devices can still be selected by + and - key and F1 for dir refresh.
* Bug fix: Device type detection did not work after passing past a device ID on which no device is active. Solved. Probably this bug has there been a long while, if not from the start. Is actually also present in DraBrowse (made a pull request there as well)

**Version v3.91 - 20230627-0852:**

[Link to build of version](https://github.com/xahmol/DMBoot/blob/main/DMBoot-v391-20230627-0852.zip)

* New version with small bugfix reparing that configuration is not saved after deleting a slot.

**Version v391-20230608-1541:**

[Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v391-20230608-1541.zip)

* First alpha version of v4 of DMBoot.
* Added the possibility to add mounting disk images for both the A and B drives of the UII+ and loading a REU file to every menu slot.
* Disk images and REU files are selectable via the file browser
* Bug fixes
* Memory optimalisations

Known issues and limitations:

In its present form, the program is very close to full memory. Considering a complete rewrite, but that takes more time than I have in the foreseeable future.
Therefore, had to make some sacrifices:

* No validation if the configuration is correct or coherent is done, so setting up valid configurations is the users responsibility
Only very limited error handling on executing incorrectly configured menu slots is done
* Mounts and REU file can be added or changed (by adding again and overwriting the previous one), but not deleted seperately. To do so, the whole entry needs to be deleted.
* Adding disk images to mount, a REU file and choosing the file to start erquire all seperate dirtrace actions in the filebrowser, so setting up a menuslot might take up to four entries to the filebrowser and navigating via dirtrace.
* I removed the functionality originiating from DraCopy to view a file in HEX or ASCII, or to perform disk commands from the file browser. As alternative, you can of course add the original DraBrowse/DraCopy as entries in de boot menu.

I personally think these limitations are acceptable as setting up the configiration of the menuslots is not done on a daily basis anyway, you set it up once to use it often.

**Version v299-20220812-0958:**

 [Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v299-20220812-0958.zip)

 * Added Load in C64 mode option with key '6' in the filebrowser. Works on directly executing programs from the filebrowser, as well as adding programs to load in 64 mode as slots in the bootmenu. Keeps supporting user defined commands and disk mounts. Requires Device Manager ROM API v2, so at least c128dm-200-alpha-20409. Thanks to Bart van Leeuwen providing API functionality to his Load 64 program function.
 * Minor other improvements / fixes

**Version v299-20210909-1708:**

 [Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v299-20210909-1708.zip)

* Complete redesign of internal memory structure creating more working memory space
* This enabled placing eveything in one menu again instead of having to start seperate programs for the NTP time and GEOS utilities.
* The ZIP still contains multiple files as the program now makes use of memory overlays that are dynamically loaded on demand. So still unzip all files in the zip to your /usb*/11/ directory.
* New menu options:
  * F4: Edit configuration settings for NTP time set and GEOS ram boot (used to be dmbconfig.prg)
  * F6: Boot GEOS from REU file using the settings provided with the F4 option
* New functionality:
  * Maximum device ID in filebrowser is now 30 instead of 11
  * Changing device ID in the filebrowser is now done with + and - keys instead of just F2, to avoid having to click F2 a lot if you want to decrease the ID from say 12 to 11.
  * You can now choose to have a disk image mounted on starting a menu slot. The program referred to in the slot will still be loaded from the Ultimate drive, but an image will be mounted on the chosen ID at the same time. This could be a working disk along to the main program, but could also be the program disk if the main executable demands to load parts from a disk.
  For each menu slot you can choose between either a disk to be mounted, or an user defined command. Both is not possible.
  Set and select this image from the main menu using: F7 > F2 > choose slot > F1 and enter image details (device ID, path and image file name).
  * No changes made to configuration files, so no upgrader needed if you are coming from a 299 version.
* Deleted functionality:
  * Removed the previous F4 Boot from floppy option. Not only did I need this space for the new NTP time and GEOS configuration option on F4, but also this functionality is delivered by the DM ROM itself already at boot.

**Version v299-20210726-1019:**

 [Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v299-20210726-1019.zip)

* Changed configuration file for main program from a sequential file to a binary blob, which loads much faster, so shortening boot time.
* Added dmb-confupd-2-3.prg: Utility to migrate the old sequential config file to the new format
* The new autostart.128.prg now sets time from NTP server on boot before starting DMBoot itself;
* Geosramboot.prg starts GEOS from a specified REU file
* Dmb-config.prg is a program to configure NTP time set and GEOS ram boot configuration

**Version v199-20210125-2234:**

 [Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v199-20210125-2234.zip)

* Menuslots now are stored in bank 1 memory, giving much more available memory, enabling all other changes below
* Increased possible number of memory slots from 10 to 36, accessable via 0-9 and a-z keys (suggested by Bart van Leeuwen ). Visible in two columns in 80 column mode, in 40 column mode only the first 15 options are shown (did not go for scrolling, at least not yet). Suggest to use therefore the right column for 80 column suported programs.
* Made the other options available via Function-keys (as the letter keys are now taken by the extra menu slots)
* Added option to add a user defined command to each menuslot (e.g. a partition change, a POKE or anything else you can imagine as long as it runs from the BASIC prompt with one enter, fits in 100 chars and does not give screen output other than the READY prompt (suggested by Bob Grimes)
