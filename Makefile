# DMBoot 128:
# Device Manager Boot Menu for the Commodore 128
# Written in 2020 by Xander Mol
# https://github.com/xahmol/DMBoot
# https://www.idreamtin8bits.com/

# See src/main.c and src/dmbconfig.c for full credits

# Prerequisites for building:
# - CC65 compiled and included in path with sudo make avail
# - ZIP packages installed: sudo apt-get install zip
# - wput command installed: sudo apt-get install wput

SOURCESMAIN = src/main.c src/bootmenu.c src/screen.c src/dir.c src/base.c src/ops.c src/db.c src/ultimate_common_lib.c src/ultimate_dos_lib.c src/ultimate_time_lib.c src/ultimate_network_lib.c src/configcommon.c src/u-time.c src/dmbconfig.c src/geosramboot.c src/vdc.c
LIBMAIN = src/geosramroutine.s src/dmapiasm.s src/vdc_assembly.s
SOURCESUPD = src/dmb-confupd-3-4.c
README = readme.txt
ZIP = DMBoot-v391-$(shell date "+%Y%m%d-%H%M").zip

# Hostname of Ultimate II+ target for deployment. Edit for proper IP and usb number
ULTHOST = ftp://192.168.1.19/usb1/11/
ULTHOST2 = ftp://192.168.1.31/usb1/11/
ULTHOST3 = ftp://192.168.1.80/usb1/11/

MAIN = autostart.128.prg
UPDATE = dmb-confupd-3-4.prg
DEPLOYS = $(MAIN) $(UPDATE) dmb-fb.prg dmb-menu.prg dmb-util.prg dmb-lowc.prg dmb-geos.prg

CC65_TARGET = c128
CC = cl65
CFLAGS  = -t $(CC65_TARGET) --create-dep $(<:.c=.d) -O -I include
LDFLAGSMAIN = -C dmboot-cc65.cfg -t $(CC65_TARGET) -m $(MAIN).map
LDFLAGSUPD = -t $(CC65_TARGET) -m $(UPDATE).map

########################################

.SUFFIXES:
.PHONY: all clean deploy
all: $(MAIN) $(TIME) $(CFG) $(UPDATE) $(ZIP)

ifneq ($(MAKECMDGOALS),clean)
-include $(SOURCESMAIN:.c=.d) $(SOURCESUPD:.c=.d) $(SOURCESTIME:.c=.d) $(SOURCESGEOS:.c=.d) $(SOURCESCFG:.c=.d)
endif

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
  
$(MAIN): $(LIBMAIN) $(SOURCESMAIN:.c=.o)
	$(CC) $(LDFLAGSMAIN) -o $@ $^ c128-ram.o

$(TIME): $(SOURCESTIME:.c=.o)
	$(CC) $(LDFLAGSTIME) -o $@ $^

$(UPDATE): $(SOURCESUPD:.c=.o)
	$(CC) $(LDFLAGSUPD) -o $@ $^ c128-ram.o 

$(CFG): $(SOURCESCFG:.c=.o)
	$(CC) $(LDFLAGSCFG) -o $@ $^

$(ZIP): $(DEPLOYS) $(README)
	zip $@ $^

clean:
	$(RM) $(SOURCESMAIN:.c=.o) $(SOURCESMAIN:.c=.d) $(MAIN) $(MAIN).map
	$(RM) $(SOURCESTIME:.c=.o) $(SOURCESTIME:.c=.d) $(TIME) $(TIME).map
	$(RM) $(SOURCESGEOS:.c=.o) $(SOURCESGEOS:.c=.d) $(GEOS) $(GEOS).map
	$(RM) $(SOURCESCFG:.c=.o) $(SOURCESCFG:.c=.d) $(CFG) $(CFG).map
	$(RM) $(SOURCESUPD:.c=.o) $(SOURCESUPD:.c=.d) $(UPDATE) $(UPDATE).map
	
# To deploy software to UII+ enter make deploy. Obviously C128 needs to powered on with UII+ and USB drive connected.
deploy: $(MAIN) $(UPDATE)
	wput -u $(DEPLOYS) $(ULTHOST)
#	wput -u $(DEPLOYS) $(ULTHOST2)
#	wput -u $(DEPLOYS) $(ULTHOST3)