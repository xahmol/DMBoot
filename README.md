## DMBoot 128

Device Manager Boot Menu for the Commodore 128

Written in 2020/2021 by Xander Mol

<https://github.com/xahmol/DMBoot>

<https://www.idreamtin8bits.com/>

### Changelog release versions

**Version v299-20210812-1320:**

 [Link to build of version](https://github.com/xahmol/DMBoot/raw/main/DMBoot-v299-20210812-1320.zip)

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

**Upgrade instructions from version 1.99 to 2.99:**

* If you want to be able to revert to the old version (and also in all other cases as backing up is always a good idea), please backup your present config file as it will be altered/overwritten to the updated format unreadable by the old version.
* The ZIP contains the utility dmb-confupd-2-3.prg to update your present config file to the new format.
* Unzip the ZIP file and transfer first dmb-confupd-2-3.prg to the 11 dir of your UII+ USB drive.
* On your C128, move to the 11 dir and run dmb-confupd-2-3.prg to upgrade the config file.
  Of course you can also opt to start clean: in that case just delete the dmbootconf.seq file and a new empty one will be created on start of the new DMBoot version.
  Starting the new DMBoot version with the old config file however gives unexpected results as I did not program a check on the config file version.
* Only then transfer the new autostart.128.prg to the 11 dir of your USB stick.
* You can now reboot as normal. The upgraded config file should be read correctly now.

### Instructions

**Prerequisites:**

* UltimateII+ (U2+) cartridge installed, with firmware at version 3.9 or higher
  (link to firmware page, scroll down for U2 firmware: <https://ultimate64.com/Firmware> )
* 128 Device Manager ROM installed as Cartridge ROM on the U2+, version 1.99 or higher.
  (link: <https://www.bartsplace.net/content/publications/devicemanager128.shtml> )

**Installation:**

* Create a directory called '11' on your usb stick, and put the contents of the DMBoot ZIP there. It doesn't really matter if the usb stick is usb0 or usb1 or such, but what is important is the 'default path' setting in the Software IEC menu of the U2+ cartridge is pointing to the proper path for your usb stick. While it doesn't matter if the usb stick is usb0/1 etc, the names of the directory and file are important.

* Unzipping should place these files in the 11 directory:

  * autostart.128.prg: DMBoot main program that will be auto started on boot (and therefore has to have this name, otherwise the Device Manager ROM will not be able to recognise that this needs to be started)

  * dmb-fb.prg: Memory overlay for the file browser (cannot be executed as stand alone program)

  * dmb-geos.prg: Memory overlay for the GEOS RAM Boot code (cannot be executed as stand alone program)

  * dmb-lowc.prg: Memory overlay for shared functions loaded in the $1300 area (cannot be executed as stand alone program)

  * dmb-menu.prg: Memory overlay for the DMBoot main menu functions (cannot be executed as stand alone program)

  * dmb-util.prg: Memory overlay for the NTP time set and GEOS configuration settings and UI.

  * dmb-confupd-2-3.prg: Utility to upgrade the configuration file of the DM Boot main program from the 1.99 version to the 2.99 version. Only needed if coming from a previous version.

  * readme.txt:
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
* Select your desired drive target as highlighted (white border) by switching to the correct column (only applicable for 80 column mode) by the **<-** (left arrow) key and/or pressing **+** or **-** to increase resp. decrease the device number until the desired device number is selected.
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

**F1: Filebrowse menu**

The filebrowser is actually a slightly adapted DraBrowse program from <https://github.com/doj/dracopy>
![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20filebrowsermenu.png)

Menu options are mostly the same, with some changes/additions.

| Key            | Function                                                     |
| ---- | ------------------------------------------- |
| **F1 / 1**  | Read directory in current window |
| **+** | Increase devicenumber for the current window |
| **-** | Decrease devicenumber for the current window |
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

**F2: Information**

Shows information screen. Press key to return to main menu.
![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20information%20screen.png)
  
**F3: Quit to C128 Basic**

Exit the bootmenu to the C128 BASIC Ready prompt. Memory will be erased on exit, SLOW mode will be selected also in 80 column mode for compatibility purposes.

**F4: NTP time / GEOS config**

Enables editing the settings for:
* automatically obtaining the actual time from an NTP server and setting the internal clock of the Ultimate II+ to this time
* RAM booting GEOS from a specified REU file with specified images mounted on the UII+ drives.

After pressing F4, you arrive at this screen:

![](https://github.com/xahmol/DMBoot/blob/main/pictures/dmboot%20-%20utilsettings.png?raw=true)

The screens shows you the present settings and allows you to edit them.

* **F1** Edit the settings for obtaining the time from an NTP server. This gives you this screen:
![](https://github.com/xahmol/DMBoot/blob/main/pictures/dmboot%20-%20ntpsettings.png?raw=true)

  * **F1** Toggles to enable or disable updating UII+ time from an NTP server at boot. Default: Enabled.
  * **F3** Edits the time offset to UTC (Universal standard time). The offset needs to provided in seconds. Automated adjustment for daylight savings ('Summer' and 'Winter' time) is not provided, so you have to adjust your offset on the change from daylight saving time to not. Example: Central Europen Time requires an offset of 3600, Central European Summer Time of 7200. See https://www.timeanddate.com/time/zones/ for all offsets in hours (multiply by 3600 to get to seconds). Default: 0 (=UTC).
  * **F5** Edits the NTP time server to be used. It defaults on pool.ntp.org, but you can specify your own if you want.
  * **F7** Back to main menu

* **F3** Edit the GEOS RAM boot configuration options. This gives you this screen:
![](https://github.com/xahmol/DMBoot/blob/main/pictures/dmboot%20-%20geossettings.png?raw=true)

  * **F1** Enables editing of the path to and the filename of the REU file to use on your UII+ device. Gives this screen: ![](https://github.com/xahmol/DMBoot/blob/main/pictures/dmboot%20-%20geosreupath.png?raw=true)
  * **F3** Enables editing the REU file size to match the image you have chosen. Choose **+** to increase size, **-** to decrease. ![](https://github.com/xahmol/DMBoot/blob/main/pictures/dmboot%20-%20geosreusize.png?raw=true)
  * **F5** Edits the device IDs, paths and the filenames of the images you want to mount. ![](https://github.com/xahmol/DMBoot/blob/main/pictures/dmboot%20-%20geosimages.png?raw=true)
  * **F7** Back to main menu

* **F7** Quit configuration tool. Only at this time new settings will be saved.

**F5: C64 mode**

Go to C64 mode (no confirmation will be asked)

**F6: GEOS RAM boot**

Boot GEOS from RAM with the specified settings. Those settings need to be configured first (via the F4 option in the main menu) for this to work.

Demonstration of booting GEOS via DMBoot:
<!-- blank line -->
<figure class="video_container">
  <iframe src="https://www.youtube.com/embed/u9hQ0eEtpeI" frameborder="0" allowfullscreen="true"> </iframe>
</figure>
<!-- blank line -->

**F7: Edit / re-order / delete**

Enables to rename menuslots, re-order the slots or delete a slot. Selecting provides this menu:
![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20editreorderdelete.png)

* **F1** enables renaming a menuslot. Choose slot to be renamed by pressing **0-9** or **A-Z**. Enter new name. Enter to confirm.
![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20rename.png)

* **F3** enables re-ordering menu slots. Choose slot to be re-ordered by pressing **0-9** or **A-Z**. Selected menu slot is highlighted white. Move option by pressing **UP** or **DOWN**. Confirm by **ENTER**. Cancel with **F7**.
![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20reorder.png)

* **F5** enables deleting a menu slot. Choose slot to be re-ordered by pressing **0-9** or **A_Z**. Confirm by pressing **Y** for yes, or **N** for no.
![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20delete.png)
  
* **F7** takes you back to main menu. Changes made are saved only now.

### Screenshot from real device

![](https://github.com/xahmol/DMBoot/raw/main/pictures/dmboot%20-%20real%20screen.jpg)

### Credits

Based on DraBrowse:  
DraBrowse (db*) is a simple file browser, originally created 2009 by Sascha Bader.
Used version adapted by Dirk Jagdmann (doj)
<https://github.com/doj/dracopy>

Requires and made possible by the C128 Device Manager ROM, created by Bart van Leeuwen.
<https://www.bartsplace.net/content/publications/devicemanager128.shtml>

Requires and made possible by the Ultimate II+ cartridge, created by Gideon Zweijtzer.
<https://ultimate64.com/>

The code can be used freely as long as you retain a notice describing original sources and authors.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
