SOURCES = src/main.c src/screen.c src/cat.c src/dir.c src/base.c src/ops.c src/db.c
SOURCESUPD = src/dmb-confupd-1-2.c src/screen.c src/cat.c src/dir.c src/base.c src/ops.c src/db.c
ZIP = DMBoot-v199-$(shell date "+%Y%m%d-%H%M").zip

PROGRAM = autostart.128.prg
DRIVER = c128-ram.emd
UPDATE = dmb-confupd-1-2.prg

CC65_TARGET = c128
CC = cl65
CFLAGS  = -t $(CC65_TARGET) --create-dep $(<:.c=.d) -O -I include
LDFLAGS = -t $(CC65_TARGET) -m $(PROGRAM).map
LDFLAGSUPD = -t $(CC65_TARGET) -m $(UPDATE).map

########################################

.SUFFIXES:
.PHONY: all clean
all: $(PROGRAM) $(UPDATE) $(ZIP)

ifneq ($(MAKECMDGOALS),clean)
-include $(SOURCES:.c=.d) $(SOURCESUPD:.c=.d)
endif

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
  
$(PROGRAM): $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) -o $@ $^

$(UPDATE): $(SOURCESUPD:.c=.o)
	$(CC) $(LDFLAGSUPD) -o $@ $^

$(ZIP): $(PROGRAM) $(DRIVER) $(UPDATE)
	zip $@ $^

clean:
	$(RM) $(SOURCES:.c=.o) $(SOURCES:.c=.d) $(SOURCESUPD:.c=.o) $(SOURCESUPD:.c=.d) $(PROGRAM) $(PROGRAM).map $(UPDATE) $(UPDATE).map