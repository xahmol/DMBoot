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

SOURCESMAIN = src/main.c src/screen.c src/cat.c src/dir.c src/base.c src/ops.c src/db.c
SOURCESUPD = src/dmb-confupd-2-3.c
SOURCESTIME = src/ultimate_lib.c src/configcommon.c src/u-time.c
SOURCESGEOS = src/ultimate_lib.c src/configcommon.c src/geosramboot.c
SOURCESCFG = src/ultimate_lib.c src/configcommon.c src/dmbconfig.c
LIBGEOS = src/geosramroutine.s
ZIP = DMBoot-v299-$(shell date "+%Y%m%d-%H%M").zip

# Hostname of Ultimate II+ target for deployment. Edit for proper IP and usb number
ULTHOST = ftp://192.168.1.31/usb1/11/

MAIN = dmbootmain.prg
TIME = autostart.128.prg
GEOS = geosramboot.prg
CFG = dmbconfig.prg
UPDATE = dmb-confupd-2-3.prg

CC65_TARGET = c128
CC = cl65
CFLAGS  = -t $(CC65_TARGET) --create-dep $(<:.c=.d) -O -I include
LDFLAGSMAIN = -t $(CC65_TARGET) -m $(MAIN).map
LDFLAGSTIME = -t $(CC65_TARGET) -m $(TIME).map
LDFLAGSGEOS = -t $(CC65_TARGET) -m $(GEOS).map
LDFLAGSCFG = -t $(CC65_TARGET) -m $(CFG).map
LDFLAGSUPD = -t $(CC65_TARGET) -m $(UPDATE).map

########################################

.SUFFIXES:
.PHONY: all clean deploy
all: $(MAIN) $(TIME) $(GEOS) $(CFG) $(UPDATE) $(ZIP)

ifneq ($(MAKECMDGOALS),clean)
-include $(SOURCESMAIN:.c=.d) $(SOURCESUPD:.c=.d) $(SOURCESTIME:.c=.d) $(SOURCESGEOS:.c=.d) $(SOURCESCFG:.c=.d)
endif

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
  
$(MAIN): $(SOURCESMAIN:.c=.o)
	$(CC) $(LDFLAGSMAIN) -o $@ $^ c128-ram.o

$(TIME): $(SOURCESTIME:.c=.o)
	$(CC) $(LDFLAGSTIME) -o $@ $^

$(UPDATE): $(SOURCESUPD:.c=.o)
	$(CC) $(LDFLAGSUPD) -o $@ $^ c128-ram.o 

$(GEOS): $(LIBGEOS) $(SOURCESGEOS:.c=.o)
	$(CC) $(LDFLAGSGEOS) -o $@ $^

$(CFG): $(SOURCESCFG:.c=.o)
	$(CC) $(LDFLAGSCFG) -o $@ $^

$(ZIP): $(MAIN) $(TIME) $(GEOS) $(CFG) $(UPDATE)
	zip $@ $^

clean:
	$(RM) $(SOURCESMAIN:.c=.o) $(SOURCESMAIN:.c=.d) $(MAIN) $(MAIN).map
	$(RM) $(SOURCESTIME:.c=.o) $(SOURCESTIME:.c=.d) $(TIME) $(TIME).map
	$(RM) $(SOURCESGEOS:.c=.o) $(SOURCESGEOS:.c=.d) $(GEOS) $(GEOS).map
	$(RM) $(SOURCESCFG:.c=.o) $(SOURCESCFG:.c=.d) $(CFG) $(CFG).map
	$(RM) $(SOURCESUPD:.c=.o) $(SOURCESUPD:.c=.d) $(UPDATE) $(UPDATE).map
	
# To deploy software to UII+ enter make deploy. Obviously C128 needs to powered on with UII+ and USB drive connected.
deploy: $(MAIN) $(UPDATE)
	wput -u $(MAIN) $(ULTHOST)
	wput -u $(TIME) $(ULTHOST)
	wput -u $(GEOS) $(ULTHOST)
	wput -u $(CFG) $(ULTHOST)
	wput -u $(UPDATE) $(ULTHOST)
