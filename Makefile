SOURCES = src/main.c src/screen.c src/cat.c src/dir.c src/base.c src/ops.c src/db.c
TIMESTAMP= v099-$(shell date "+%Y%m%d-%H%M").prg

PROGRAM = autostart.128.prg

CC65_TARGET = c128
CC = cl65
CFLAGS  = -t $(CC65_TARGET) --create-dep $(<:.c=.d) -O -I include
LDFLAGS = -t $(CC65_TARGET) -m $(PROGRAM).map

########################################

.SUFFIXES:
.PHONY: all clean
all: $(PROGRAM)

ifneq ($(MAKECMDGOALS),clean)
-include $(SOURCES:.c=.d)
endif

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
  
$(PROGRAM): $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) -o $@ $^
	cp $@ $@_$(TIMESTAMP)

clean:
	$(RM) $(SOURCES:.c=.o) $(SOURCES:.c=.d) $(PROGRAM) $(PROGRAM).map