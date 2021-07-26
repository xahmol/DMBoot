## DMBoot 128

Device Manager Boot Menu for the Commodore 128

Written in 2020 by Xander Mol

https://github.com/xahmol/DMBoot

https://www.idreamtin8bits.com/

### Changelog

**Version v199-20210125-2234 :**

 [Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v199-20210125-2234.zip)

* Menuslots now are stored in bank 1 memory, giving much more available memory, enabling all other changes below
* Increased possible number of memory slots from 10 to 36, accessable via 0-9 and a-z keys (suggested by Bart van Leeuwen ). Visible in two columns in 80 column mode, in 40 column mode only the first 15 options are shown (did not go for scrolling, at least not yet). Suggest to use therefore the right column for 80 column suported programs.
* Made the other options available via Function-keys (as the letter keys are now taken by the extra menu slots)
* Added option to add a user defined command to each menuslot (e.g. a partition change, a POKE or anything else you can imagine as long as it runs from the BASIC prompt with one enter, fits in 100 chars and does not give screen output other than the READY prompt (suggested by Bob Grimes) 

**Upgrade instructions from version 0.99 to 1.99:**

- If you want to be able to revert to the old version (and also in all other cases as backing up is always a good idea), please backup your present config file as it will be altered/overwritten to the updated format unreadable by the old version.
- In the ZIP are two files: next to the autostart.128.prg also an utility dmb-confupd-1-2.prg to update your present config file to the new format.
- Unzip the ZIP file and transfer first dmb-confupd-1-2.prg to the 11 dir of your UII+ USB drive.
- On your C128, move to the 11 dir and run dmb-confupd-1-2.prg to upgrade the config file.
  Of course you can also opt to start clean: in that case just delete the dmbootconf.seq file and a new empty one will be created on start of the new DMBoot version.
  Starting the new DMBoot version with the old config file however gives unexpected results as I did not program a check on the config file version.
- Only then transfer the new autostart.128.prg to the 11 dir of your USB stick.
- You can now reboot as normal. The upgraded config file should be read correctly now.

### Instructions

**NB:** Instruction screenshots are taken from VICE emulator for practical reasons, so not showing real devices.

**Prerequisites:**

* UltimateII+ (U2+) cartridge installed, with firmware at version 3.9 or higher
  (link to firmware page, scroll down for U2 firmware: https://ultimate64.com/Firmware )
* 128 Device Manager ROM installed as Cartridge ROM on the U2+, version 1.99 or higher.
  (link: https://www.bartsplace.net/content/publications/devicemanager128.shtml )

**Installation:**
* Create a directory called '11' on your usb stick, and put the contents of the DMBoot ZIP there. It doesn't really matter if the usb stick is usb0 or usb1 or such, but what is important is the 'default path' setting in the Software IEC menu of the U2+ cartridge is pointing to the proper path for your usb stick. While it doesn't matter if the usb stick is usb0/1 etc, the names of the directory and file are important.

* Unzipping should place these files in the 11 directory:

 * autostart.128.prg:
The executable that will automatically start DM Boot running with the DM manager Autoboot option enabled / chosen from the DM menu. This will update time via the chosen NTP server if enabled and then start the DMBoot main program.

 * dmbconfig.prg:
Configuration program to set the options for the NTP time server update and the GEOS Ram boot options and file paths/names.

 * dmb-confupd-2-3.prg:
Utility to upgrade the configuration file of the DM Boot main program from the 1.99 version to the 2.99 version. Only needed if coming from a previous version.

 * dmbootmain.prg:
DMBoot main program.

 *  geosramboot.prg:
GEOS RAM boot executable. This will boot GEOS from the REU file specified using the dmbconfig.prg program.

..* readme.txt:
This readme file.

**First run:**

* At first run no configuration file is present yet for the menu, so only the default menu options are visible:
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20firstrunmenu.png)
* For instructions of the menu options: see below.

**Add start options via the Filebrowser**

* Start options can be added to menuslots 0-9 and A-Z via the Filebrowser. This can be either running an executable program, or booting a specific disk image.
* For this, first choose **F1** for filebrowser.  You will get a screen like this:
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20filebrowsermenu.png)
* Full instructions for the filebrowser are below. Here only the quick instructions to add an option to the startmenu.
* Select your desired drive target as highlighted (white border) by switching to the correct column (only applicable for 80 column mode) by the **<-** (left arrow) key and/or pressing **F2** until the desired device number is selected.
* Refresh directory by **F1** if needed (empty column)
* Start a directory trace by pressing **D**
  
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20highlightdirtrace.png)
  
  This starts a trace of your movements through the directory tree, starting from the root directory of your device. You should see the directory refreshing to this root directory.
  You should also see the TRACE toggle switched to ON in the lower right corner of the screen.
  So from this ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20filebrowsertoggles.png) to this ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20toggledirtraceon.png).
* Also note the other two toggles Frc 8 and FAST: these are toggled by pressing the **8** and **F** keys.
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20highlightforce8andfast.png)
* Force 8: This forces the device ID to be loaded from to 8 regardless of the device ID the target is located at. Only works for the U2+ Software IEC drive (identified as U64 in the lowest line of the directory column). This enables loading software that is not supporting other parts of the program from other device IDs as 8
* FAST: this starts the program in FAST mode. Only use if the target is started in 80 column mode and the target supports it.
* Browse to your desired target via the **Cursor keys**: **UP/DOWN** to move within the directory, **ENTER or RIGHT** on a directory or disk image to enter the selected directory or image, **DEL / LEFT** to change to parent directory.
* To choose to have a program executed from the menuslot, select **ENTER** or **F7** on the desired executable
* To choose to boot the image you are now in from the menuslot, select **F5**.
* You should than get this screen to select the menuslot position:
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20choosemenuslot.png)
* Choose **0-9** or **A-Z** key to choose the desired slot.
* Enter the desired name for the menuslot and press **ENTER**
* You now return to the main menu where you should see the menu option appearing.
* Repeat until you have selected all desired menuslot options

**F: Filebrowse menu**

* The filebrowser is actually a slightly adapted DraBrowse program from https://github.com/doj/dracopy
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20filebrowsermenu.png)
* Menu options are mostly the same, with some changes/additions.

| Key            | Function                                                     |
| ---- | ------------------------------------------- |
| **F1 / 1**  | Read directory in current window |
| **F2 / 2** | Select the next device for the current window |
| **F3 / 3** | View current file as hex dump |
| **F4 / 4** | View current file as ASCII text |
| **F5 / 5** | Boot from present directory / image (*added compared to DraBrowse*) |
| **F7 / 7** | Run the selected program |
| **← , ESC, 0** | Switch window (only in 80 column mode) |
| **RETURN** | Enter directory or run the selected program |
| **RIGHT** | Enter directory |
| **DEL / LEFT** | Go to parent directory |
| ↑ | Go to root directory |
| **S** | Show directory entries sorted |
| **HOME / T** | Move cursor to top row of first page in current window |
| **B** | Go to bottom row in current window |
| **@** | Send a DOS command to the device in current window |
| **D** | Toggle Dirtrace: traces the directory movements from root directory to select menuslot option |
| **8** | Toggle Force 8, forcing device ID 8 on program execution or boot. Works for menuslot option as well as directly from filebrowser. |
| **F** | Toggle FAST, enabling this makes file execution or boot start in FAST mode. Works for menuslot option as well as directly from filebrowser. |
| **Q** | Quit to main menu (*altered function compared to DraBrowse*) |

**F3: Quit to C128 Basic**

* Exit the bootmenu to the C128 BASIC Ready prompt. Memory will be erased on exit, SLOW mode will be selected also in 80 column mode for compatibility purposes.

**F5: C64 mode**

* Go to C64 mode (no confirmation will be asked)

**F4: Boot from floppy**

* Boot from a floppy selected by the user
* Selection of this option lists all devices detected (if device detection was successful, otherwise nothing is shown)
* User is prompted to enter requested device ID (number of 8 to 30)
* Boot is initiated from this device ID

**F7: Edit / re-order / delete**

* Enables to rename menuslots, re-order the slots or delete a slot. Selecting provides this menu:
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20editreorderdelete.png)

* **F1** enables renaming a menuslot

![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20rename.png)

Choose slot to be renamed by pressing **0-9** or **A-Z**. Enter new name. Enter to confirm.

* **F3** enables re-ordering menu slots

![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20reorder.png)

Choose slot to be re-ordered by pressing **0-9** or **A-Z**. Selected menu slot is highlighted white. Move option by pressing **UP** or **DOWN**. Confirm by **ENTER**. Cancel with **F7**.

* **F5** enables deleting a menu slot
  
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20delete.png)
Choose slot to be re-ordered by pressing **0-9** or **A_Z**. Confirm by pressing **Y** for yes, or **N** for no.
  
* **F7** takes you back to main menu. Changes made are saved only now.

**F2: Information**

* Shows information screen. Press key to return to main menu.
  ![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20information%20screen.png)

### Screenshot from real device

![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20real%20screen.jpg)

### Credits
Based on DraBrowse:  
DraBrowse (db*) is a simple file browser, originally created 2009 by Sascha Bader.
Used version adapted by Dirk Jagdmann (doj)
https://github.com/doj/dracopy

Requires and made possible by the C128 Device Manager ROM, created by Bart van Leeuwen.
https://www.bartsplace.net/content/publications/devicemanager128.shtml

Requires and made possible by the Ultimate II+ cartridge, created by Gideon Zweijtzer.
https://ultimate64.com/

The code can be used freely as long as you retain a notice describing original sources and authors.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
