# DMBoot 128:
# Device Manager Boot Menu for the Commodore 128
# Written in 2020 by Xander Mol
# https://github.com/xahmol/DMBoot
# https://www.idreamtin8bits.com/

# See src/main.c for full credits

# Prerequisites for building:
# - CC65 compiled and included in path with sudo make avail
# - ZIP packages installed: sudo apt-get install zip
# - wput command installed: sudo apt-get install wput

SOURCESMAIN = src/main.c src/screen.c src/cat.c src/dir.c src/base.c src/ops.c src/db.c
SOURCESUPD = src/dmb-confupd-1-2.c
SOURCESTIME = src/u-time.c src/ultimate_lib.c
ZIP = DMBoot-v199-$(shell date "+%Y%m%d-%H%M").zip

# Hostname of Ultimate II+ target for deployment. Edit for proper IP and usb number
ULTHOST = ftp://192.168.1.31/usb1/11/

MAIN = autostart.128.prg
TIME = dmbtime.prg
UPDATE = dmb-confupd-1-2.prg

CC65_TARGET = c128
CC = cl65
CFLAGS  = -t $(CC65_TARGET) --create-dep $(<:.c=.d) -O -I include
LDFLAGS = -t $(CC65_TARGET) -m $(MAIN).map
LDFLAGSUPD = -t $(CC65_TARGET) -m $(UPDATE).map

########################################

.SUFFIXES:
.PHONY: all clean deploy
all: $(MAIN) $(TIME) $(UPDATE) $(ZIP)

ifneq ($(MAKECMDGOALS),clean)
-include $(SOURCESMAIN:.c=.d) $(SOURCESUPD:.c=.d)
endif

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
  
$(MAIN): $(SOURCESMAIN:.c=.o)
	$(CC) $(LDFLAGS) -o $@ $^ c128-ram.o

$(TIME): $(SOURCESTIME:.c=.o)
	$(CC) $(LDFLAGS) -o $@ $^

$(UPDATE): $(SOURCESUPD:.c=.o)
	$(CC) $(LDFLAGSUPD) -o $@ $^ c128-ram.o 

$(ZIP): $(MAIN) $(UPDATE)
	zip $@ $^

clean:
	$(RM) $(SOURCESMAIN:.c=.o) $(SOURCESMAIN:.c=.d) $(MAIN) $(MAIN).map
	$(RM) $(SOURCESTIME:.c=.o) $(SOURCESTIME:.c=.d) $(TIME) $(TIME).map
	$(RM) $(SOURCESUPD:.c=.o) $(SOURCESUPD:.c=.d) $(UPDATE) $(UPDATE).map
	
# To deploy software to UII+ enter make deploy. Obviously C128 needs to powered on with UII+ and USB drive connected.
deploy: $(MAIN) $(UPDATE)
	wput -u $(MAIN) $(ULTHOST)
	wput -u $(TIME) $(ULTHOST)
	wput -u $(UPDATE) $(ULTHOST)
