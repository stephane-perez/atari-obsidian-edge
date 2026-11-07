# Makefile for Obsidian Edge (GCC cross build for the Atari ST/TT/Falcon)
#
# Requires the m68k-atari-mint cross toolchain (GCC, binutils, mintbin,
# MiNTLib, GEMlib): see README.md and scripts/get-cross-mint.sh, and
# libcmini: scripts/get-libcmini.sh.
#
#   make              build build/EDGE.PRG, build/DMIRROR.PRG, build/INSTALL.PRG
#   make LIBC=mintlib the same, with MiNTLib instead of libcmini (bigger)
#   make dist         folder ready to copy to the Atari: build/dist/OBSIDIAN/
#   make zip          the same, as build/obsidian-edge-<version>.zip
#   make smoke-test   install and start EDGE in Hatari: EMUTOS=<path to etos512uk.img>
#   make clean
#
# The original Lattice C project files (*.PRJ) are kept for reference; the
# GCC build uses the Lattice compatibility layer in port/.

CROSS   ?= m68k-atari-mint-
CC      := $(CROSS)gcc
FLAGS   := $(CROSS)flags

BUILD   := build
GENINC  := $(BUILD)/include

# Lattice C options of the original build: -m0 (68000), -cu (unsigned char)
CPUFLAGS := -m68000
CFLAGS   ?= -O2 -fomit-frame-pointer
CFLAGS   += $(CPUFLAGS) -funsigned-char -Wall -Wno-pointer-sign -Wno-format \
            -Wno-char-subscripts -Wno-parentheses -Wno-unused-value \
            -Wno-unused-but-set-variable -Wno-missing-braces
CPPFLAGS := -I$(GENINC) -Iport/include -include lattice.h -DNDEBUG
ASFLAGS  := $(CPUFLAGS)

# --- C library -------------------------------------------------------------
# libcmini (default): small C library for TOS; MiNTLib is about 100 KB
# bigger in each program, mostly for features the programs do not use
# (floating-point printf, time zones, Unix users and groups).
LIBC     ?= cmini
LIBCMINI ?= .tools/libcmini

ifeq ($(LIBC),cmini)
# only the libcmini headers (plus GCC's own and port/), never MiNTLib's
GCCINC   := $(shell $(CC) -print-file-name=include)
CPPFLAGS := -nostdinc -I$(GENINC) -Iport/include -Iport/cmini -I$(LIBCMINI)/include \
            -isystem $(GCCINC) -include lattice.h -include cmini.h -DNDEBUG
STARTUP  := $(LIBCMINI)/lib/crt0.o
LDFLAGS  := -nostdlib -L$(LIBCMINI)/lib
# libiiomini: printf() & co without floating point.  libcmini is named
# again after libgcc, whose __main() (constructors) needs atexit()
LDLIBS   := -lgem -liiomini -lcmini -lgcc -lcmini
LIBC_OBJS := port/cmini/cmini.o
ifeq ($(wildcard $(LIBCMINI)/lib/libcmini.a)$(filter clean,$(MAKECMDGOALS)),)
$(error libcmini not found in $(LIBCMINI): run scripts/get-libcmini.sh, or build with LIBC=mintlib)
endif
else ifeq ($(LIBC),mintlib)
STARTUP  :=
LDFLAGS  :=
LDLIBS   := -lgem
LIBC_OBJS :=
else
$(error LIBC must be cmini or mintlib)
endif

# program header flags of the original binaries: fastload, load into and
# malloc from TT-RAM (Lattice .L [TTMALLOC TTLOAD])
PRGFLAGS := --mfastload --mfastram --mfastalloc

PORT_OBJS := port/lattice.o port/mediach.o $(LIBC_OBJS)

EDGE_SRCS := main.c copyd.c disk.c dmap.c fileio.c info.c medic1.c mirror.c \
             misc.c opt2.c part1.c protect.c rsc.c section.c undel.c valid.c \
             viewfile.c xrsrc.c lib/driver.c lib/encrypt.c
EDGE_OBJS := $(EDGE_SRCS:.c=.o) lib/scsidrv.o port/edgeasm.o port/compmem.o \
             $(PORT_OBJS)

DMIRROR_OBJS := dmirror/dmirror.o dmirror/section.o dmirror/xmode.o \
                port/compmem.o $(PORT_OBJS)

INSTALL_OBJS := install/install.o lib/encrypt.o $(PORT_OBJS)

PROGRAMS := $(BUILD)/EDGE.PRG $(BUILD)/DMIRROR.PRG $(BUILD)/INSTALL.PRG

# version of the fork (the programs themselves still say 2.57)
VERSION := $(shell cat VERSION)

.PHONY: all dist zip smoke-test clean
.SUFFIXES:

all: $(PROGRAMS)

# --- the patched GEMlib header -------------------------------------------
# The sources use OBJECT.ob_spec as a plain pointer (Lattice C), GEMlib
# declares it as a union (OBSPEC) of the same size.  We use a copy of
# GEMlib's <mt_gem.h> where ob_spec is a 'void *', found before the
# installed one.
HASH := \#
MT_GEM_H := $(shell echo '$(HASH)include <mt_gem.h>' | $(CC) -M -E - 2>/dev/null | tr ' \\' '\n\n' | grep 'mt_gem\.h$$')
ifeq ($(MT_GEM_H)$(filter clean,$(MAKECMDGOALS)),)
$(error cannot find GEMlib's mt_gem.h: is the m68k-atari-mint toolchain installed?)
endif

$(GENINC)/mt_gem.h: $(MT_GEM_H) $(GENINC)/gem.h
	@mkdir -p $(@D)
	sed 's/^\([[:space:]]*\)OBSPEC\([[:space:]]*\)ob_spec;/\1void *\2ob_spec;/' $< > $@.tmp
	@grep -q 'void \*[[:space:]]*ob_spec;' $@.tmp || { echo "error: cannot patch ob_spec in $<"; rm -f $@.tmp; exit 1; }
	mv $@.tmp $@

# with -nostdinc (libcmini build), GEMlib's <gem.h> must be found here too
$(GENINC)/gem.h: $(dir $(MT_GEM_H))gem.h
	@mkdir -p $(@D)
	cp $< $@

# --- compilation -----------------------------------------------------------
OBJDIR := $(BUILD)/obj-$(LIBC)

# relink the programs when LIBC changes (the objects are kept apart)
LIBC_STAMP := $(BUILD)/libc.stamp
$(LIBC_STAMP): FORCE
	@mkdir -p $(@D)
	@echo $(LIBC) | cmp -s - $@ || echo $(LIBC) > $@
.PHONY: FORCE
obj = $(addprefix $(OBJDIR)/,$(1))

$(OBJDIR)/%.o: %.c $(GENINC)/mt_gem.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

# the SCSIDRV routines take 16-bit parameters: see port/include/scsidefs.h
$(OBJDIR)/lib/scsidrv.o: CFLAGS += -mshort

# DMIRROR: Lattice's AUTO-folder startup code set _XMODE (port/xmode.c)
$(OBJDIR)/dmirror/xmode.o: port/xmode.c $(GENINC)/mt_gem.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# --- link --------------------------------------------------------------
define link
	$(CC) $(CFLAGS) $(LDFLAGS) -s -o $@ $(STARTUP) $(filter %.o,$^) -Wl,--stack,$(2) $(LDLIBS)
	$(FLAGS) -q $(PRGFLAGS) $@
endef

$(BUILD)/EDGE.PRG: $(call obj,$(EDGE_OBJS)) $(LIBC_STAMP)
	$(call link,EDGE,32k)

$(BUILD)/DMIRROR.PRG: $(call obj,$(DMIRROR_OBJS)) $(LIBC_STAMP)
	$(call link,DMIRROR,16k)

$(BUILD)/INSTALL.PRG: $(call obj,$(INSTALL_OBJS)) $(LIBC_STAMP)
	$(call link,INSTALL,16k)

-include $(shell find $(OBJDIR) -name '*.d' 2>/dev/null)

# --- distribution --------------------------------------------------------
# same layout as the original v2.57 release: everything in one folder,
# INSTALL.PRG copies the files where they belong
DIST := $(BUILD)/dist/OBSIDIAN
HELP_FILES := $(wildcard HELP/*.HLP)

dist: $(PROGRAMS)
	rm -rf $(BUILD)/dist
	mkdir -p $(DIST)
	cp $(PROGRAMS) EDGE.RSC install/INSTALL.RSC $(HELP_FILES) $(DIST)/
	cp DOC/LICENSE.TXT $(DIST)/LICENSE.TXT
	sed -e 's/@VERSION@/$(VERSION)/' -e 's/$$/\r/' port/README.TXT > $(DIST)/README.TXT

zip: dist
	rm -f $(BUILD)/obsidian-edge-$(VERSION).zip
	cd $(BUILD)/dist && zip -q -r -X ../obsidian-edge-$(VERSION).zip OBSIDIAN

# --- test in Hatari ------------------------------------------------------
EMUTOS ?=
smoke-test: dist
	python3 tools/smoke_test.py --dist $(DIST) --tos "$(EMUTOS)" --out $(BUILD)/smoke

clean:
	rm -rf $(BUILD)
