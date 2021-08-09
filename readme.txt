DMBoot 128
----------

Device Manager Boot Menu for the Commodore 128

Written in 2020 by Xander Mol

Prerequisites:
--------------
- UltimateII+ (U2+) cartridge installed, with firmware at version 3.9 or higher (link to firmware page, scroll down for U2 firmware: https://ultimate64.com/Firmware )
- 128 Device Manager ROM installed as Cartridge ROM on the U2+, version 1.99 or higher. (link: https://www.bartsplace.net/content/publications/devicemanager128.shtml )

Installation:
-------------
- Create a directory called '11' on your usb stick, and put the contents of the DMBoot ZIP there. It doesn't really matter if the usb stick is usb0 or usb1 or such, but what is important is the 'default path' setting in the Software IEC menu of the U2+ cartridge is pointing to the proper path for your usb stick. While it doesn't matter if the usb stick is usb0/1 etc, the names of the directory and file are important.

- Unzipping should place these files in the 11 directory:

autostart.128.prg:
The executable that will automatically start DM Boot running with the DM manager Autoboot option enabled / chosen from the DM menu. This will update time via the chosen NTP server if enabled and then start the DMBoot main program.

dmbconfig.prg:
Configuration program to set the options for the NTP time server update and the GEOS Ram boot options and file paths/names.

dmb-confupd-2-3.prg:
Utility to upgrade the configuration file of the DM Boot main program from the 1.99 version to the 2.99 version. Only needed if coming from a previous version.

dmbootmain.prg:
DMBoot main program.

geosramboot.prg:
GEOS RAM boot executable. This will boot GEOS from the REU file specified using the dmbconfig.prg program.

readme.txt:
This readme file.

First run:
----------
On first run, configuration options with default values will be created, start menu slots will be empty. It is suggested to add the dmbconfig and geosramboot executables to the start menu for easy access.

Instructions:
-------------
Please see full instructions(and full source code) at:
https://github.com/xahmol/DMBoot


Credits:
--------

DMBoot 128:
Device Manager Boot Menu for the Commodore 128
Written in 2020 by Xander Mol
https://github.com/xahmol/DMBoot
https://www.idreamtin8bits.com/

Based on DraBrowse:
DraBrowse (db*) is a simple file browser.
Originally created 2009 by Sascha Bader.
Used version adapted by Dirk Jagdmann (doj)
https://github.com/doj/dracopy

Requires and made possible by the C128 Device Manager ROM,
Created by Bart van Leeuwen
https://www.bartsplace.net/content/publications/devicemanager128.shtml

Requires and made possible by the Ultimate II+ cartridge,
Created by Gideon Zweijtzer
https://ultimate64.com/

The code can be used freely as long as you retain
a notice describing original source and author.
THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!