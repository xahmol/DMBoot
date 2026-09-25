# DMBoot 128 v5
# Device Manager Boot Menu for the Commodore 128
# Written in 2020-2026 by Xander Mol
# https://github.com/xahmol/DMBoot
# https://www.idreamtin8bits.com/
#
# Oscar64 rebuild. See docs/REBUILD_PLAN.md for the architecture.
#
# Prerequisites for building:
# - Oscar64 (https://github.com/drmortalwombat/oscar64), path set in CC below
# - zip, wput (deploy), curl (check-deploy), pandoc + texlive-xetex (docs)

# Target
SYS = c128e

# Just the usual way to find out if we're
# using cmd.exe to execute make rules.
ifneq ($(shell echo),)
  CMD_EXE = 1
endif

ifdef CMD_EXE
  NULLDEV = nul:
  DEL = -del /f
  RMDIR = rmdir /s /q
  MKDIR = mkdir
else
  NULLDEV = /dev/null
  DEL = $(RM)
  RMDIR = $(RM) -r
  MKDIR = mkdir -p
endif

# Toolchain -- override the path if oscar64 lives elsewhere:
#   make CC=/path/to/oscar64/bin/oscar64
# (plain '=', not '?=': CC is a Make built-in that is never "unset")
CC = /home/xahmol/oscar64/bin/oscar64

# Application names
# MAIN is built as dmboot.prg and shipped as autostart.128.prg, the name the
# Device Manager ROM autostarts. Overlay files get the names from their
# #pragma overlay() directives.
MAIN = dmboot
AUTOSTART = autostart.128.prg
LMC = dmblmc
OVERLAYS = dmbovl1 dmbovl2 dmbovl3 dmbovl5

# Build versioning
VERSION_MAJOR = 5
VERSION_MINOR = 0
VERSION_PATCH = 0
VERSION_TIMESTAMP = $(shell date "+%Y%m%d-%H%M")
VERSION = v$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)-$(VERSION_TIMESTAMP)

# Compile flags
#   -i=include   : add include/ to the header search path
#   -tm=c128e    : C128, program in the first 48 KB ($1C01-$BFFF)
#   -O2          : optimise
#   -dNOFLOAT    : no float support in printf/stdlib (saves space)
#   -dHEAPCHECK  : heap integrity checking
#   -dVERSION    : version string for the program
#   -g           : .asm/.map/.lbl/.int debug output in build/
CFLAGS  = -i=include \
          -i=src \
          -tm=$(SYS) \
          -O2 \
          -dNOFLOAT \
          -dHEAPCHECK \
          -dVERSION="\"$(VERSION)\"" \
          -g

# TESTMODE build: adds the test mailbox at $0B00 for c64bridge testing
CFLAGSTEST = $(CFLAGS) -dTESTMODE

# Sources
# Oscar64 follows #pragma compile chains internally, but make doesn't know
# about them, so every transitively compiled .c/.h is listed here.
MAIN_SRCS = src/main.c \
            src/slotmenu.c src/slotmenu.h \
            src/slotlist.c src/slotlist.h \
            src/slotedit.c src/slotedit.h \
            src/browse.c src/browse.h \
            src/exec.c src/exec.h \
            src/dmpaths.c src/dmpaths.h \
            src/core.c src/core.h \
            src/fileio.c src/fileio.h \
            include/ultimate_common_lib.c include/ultimate_common_lib.h \
            include/ultimate_dos_lib.c include/ultimate_dos_lib.h \
            include/ultimate_time_lib.c include/ultimate_time_lib.h \
            include/defines.h \
            include/banking.c include/banking.h \
            include/dmapi.c include/dmapi.h \
            include/reu128.c include/reu128.h \
            include/testmode.c include/testmode.h \
            include/dualwin.c include/dualwin.h \
            include/vdc_core.c include/vdc_core.h \
            include/vdc_win.c include/vdc_win.h include/vdcwin_types.h \
            include/peekpoke.h

# Files to deploy / ship
BUILD_PRGS = build/$(AUTOSTART) build/$(LMC).prg $(addprefix build/,$(addsuffix .prg,$(OVERLAYS)))

# Ultimate II+ deployment target. Store only the IP in .env (gitignored);
# the path is the Device Manager ROM boot directory.
-include .env
ULTIP1  ?= <set_ULTIP1_in_.env>
ULTUSB  ?= Usb1
ULTPATH  = /$(ULTUSB)/11/
ULTFTP1  = ftp://$(ULTIP1)$(ULTPATH)

# Release ZIP
ZIP = build/dmboot_$(VERSION).zip
README = README.pdf

########################################

.SUFFIXES:
.PHONY: all build test-build clean deploy check-deploy docs zip

all: build $(README) zip

# Release build
build: $(MAIN_SRCS)
	@$(MKDIR) build 2>$(NULLDEV) ; true
	$(CC) $(CFLAGS) -n -o=build/$(MAIN).prg src/main.c
	cp build/$(MAIN).prg build/$(AUTOSTART)

# Test build (same output names, so 'make deploy' ships it; run 'make build'
# to go back to the release binaries)
test-build: $(MAIN_SRCS)
	@$(MKDIR) build 2>$(NULLDEV) ; true
	$(CC) $(CFLAGSTEST) -n -o=build/$(MAIN).prg src/main.c
	cp build/$(MAIN).prg build/$(AUTOSTART)

# Regenerate README.pdf from README.md (requires pandoc + texlive-xetex).
# Warns and skips if pandoc is unavailable; README.pdf is committed.
docs: $(README)

$(README): README.md pandoc-defaults.yaml pandoc-header.tex pandoc-wrap-tables.lua
	@if which pandoc >/dev/null 2>&1; then \
		pandoc --defaults=pandoc-defaults.yaml README.md -o $(README); \
	else \
		echo "WARNING: pandoc not found -- $(README) not updated (install: sudo apt install pandoc texlive-xetex)"; \
	fi

zip: build $(README)
	zip -j $(ZIP) $(BUILD_PRGS) $(README)

clean:
	$(DEL) build/*.* 2>$(NULLDEV)

# Safety check before deploy: make sure the Ultimate is reachable
check-deploy:
	@curl -s --connect-timeout 3 $(ULTFTP1) >/dev/null 2>&1 || \
		(echo "ERROR: Cannot reach Ultimate at $(ULTIP1) -- check ULTIP1 in .env" && false)

# Deploy the last build (release or test) to the Device Manager boot
# directory. NOTE: overwrites autostart.128.prg there.
deploy: check-deploy
	wput -u --basename=build/ $(BUILD_PRGS) $(ULTFTP1)
