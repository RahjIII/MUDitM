# $Id: makefile,v 1.10 2024/02/27 04:39:21 malakai Exp $

# Copyright © 2021 Jeff Jahr <malakai@jeffrika.com>
#
# This file is part of MUDitM - MUD in the Middle
#
# MUDitM is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# MUDitM is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with MUDitM.  If not, see <https://www.gnu.org/licenses/>.

# $(MUDITM) is the name of the server binary. 
MUDITM = muditm
INSTALLDIR=/usr/local

# List the .c files here.  Order doesn't matter.  Dont worry about header file
# dependencies, this makefile will figure them out automatically.
MUDITM_CFILES = muditm.c debug.c proxy.c iobuf.c handlers.c mccp.c iostats.c

# The list of HFILES, (required for making the ctags database) is generated
# automatically from the MUDITM_CFILES list.  However, it is possible that not
# everything in MUDITM_CFILES has a corresponding .h file.  MISSING_HFILES
# lists the .h's that aren't expected to exist.  ADDITIONAL_HFILES lists the
# .h's for which no .c exists.
ADDITIONAL_HFILES = 
MISSING_HFILES = 

# CDEBUG, use -g for gdb symbols.  For gprof, add -pg and -no-pie.
CDEBUG = -g

# #### Flags and linklibs definitions ####

#CFLAGS = -Wunused -Wimplicit-function-declaration -Wno-unused-but-set-variable -Wno-format-overflow -Wno-format-truncation `pkg-config --cflags glib-2.0`
CFLAGS = -Wall -Wno-unused-but-set-variable `pkg-config --cflags glib-2.0`
LDFLAGS = 
LINKLIBS = -lresolv -lssl -lcrypto `pkg-config --libs glib-2.0` -lpcre2-8 -lz

# #### ############################################# ###
# ####         Makefile magic begins here.           ###
# #### Very little needs to change beyond this line! ###
# #### ############################################# ###

CC=gcc
BUILD = ./build

CFILES = $(MUDITM_CFILES)

# HFILES generated automatically from CFILES, with additions and exclusions
HFILES := $(ADDITIONAL_HFILES)
HFILES += $(addsuffix .h, $(basename $(CFILES)))
HFILES := $(filter-out $(MISSING_HFILES), $(HFILES))

MUDITM_OFILES = $(MUDITM_CFILES:%.c=$(BUILD)/%.o)
OFILES = $(MUDITM_OFILES) $(MUDITMRESOLVD_OFILES)

MUDITM_DFILES = $(MUDITM_CFILES:%.c=$(BUILD)/%.d)
DFILES = $(MUDITM_DFILES) $(MUDITMRESOLVD_DFILES)

RUN = .

# #### Recipies Start Here ####

$(info ---------- START OF MUD in the Middle COMPILATION -----------)
all : $(BUILD) run tags

# Copying the binaries...
#.PHONY: $(MUDITM)
$(MUDITM) : $(BUILD) $(BUILD)/$(MUDITM)

.PHONY: run
run : $(MUDITM) $(RUN)/$(MUDITM)

# The mv command is to prevent 'text file busy' error, and the 2>/ is some bash
# bullshit to keep the command from failing the make if the source file doesn't
# exist.
$(RUN)/$(MUDITM) : $(BUILD)/$(MUDITM)
	-mv -f $(RUN)/$(MUDITM) $(RUN)/$(MUDITM).prev 2>/dev/null || true
	cp $(BUILD)/$(MUDITM) $(RUN)/$(MUDITM)

# Create build directory...
$(BUILD) : 
	mkdir -p $(BUILD)

# Create run directory...
$(RUN) : 
	mkdir -p $(RUN)

# Linking the MUDITM binary...
$(BUILD)/$(MUDITM) : $(MUDITM_OFILES)
	$(CC) $(CDEBUG) $(LDFLAGS) $^ -o $(@) $(LINKLIBS)

# check the .h dependency rules in the .d files made by gcc
-include $(MUDITM_DFILES)

# Build the .o's from the .c files, building .d's as you go.
$(BUILD)/%.o : %.c
	$(CC) $(CDEBUG) $(CFLAGS) -MMD -c $< -o $(@)

# Updating the tags file...
tags : $(HFILES) $(CFILES)
	ctags $(HFILES) $(CFILES)

# Cleaning up...
# .PHONY just means 'not really a filename to check for'
.PHONY: clean
clean : 
	-rm $(BUILD)/$(MUDITM) $(MUDITM) $(OFILES) $(DFILES) tags
	-rm -rI $(BUILD)

.PHONY: wall-summary
wall-summary: 
	$(info ** Enable these errors in the CFLAGS section to track them down. )
	$(info ** Run this after a make clean for full effect! )
	make "CFLAGS=-Wall" 2>&1 | egrep -o "\[-W.*\]" | sort | uniq -c | sort -n

cert.pem : 
	$(info --- Creating self-signed cert and key ----) 
	openssl req -nodes -new -x509 -keyout key.pem -out cert.pem

.PHONY: cert
cert : cert.pem
	$(info --- made cert.pem and key.pem ----) 

.PHONY: dist
dist: $(BUILD) $(BUILD)/$(MUDITM) FILES
	$(eval DISTDIR := $(shell $(BUILD)/$(MUDITM) -v))
	$(eval DISTFILES := $(shell xargs -a FILES))
	$(info --- Creating Distribution File in $(BUILD)/$(DISTDIR).tgz ----) 
	-mkdir $(BUILD)/$(DISTDIR)
	-cp $(DISTFILES) $(BUILD)/$(DISTDIR)
	(cd $(BUILD); tar -cvzf $(DISTDIR).tgz $(DISTDIR))

.PHONY: install
install: $(BUILD)/$(MUDITM) muditm.conf
	$(info ---- Install dir is $(INSTALLDIR) ----)
	cp $(BUILD)/$(MUDITM) $(INSTALLDIR)/bin
	cp muditm.conf $(INSTALLDIR)/etc

.PHONY: systemd
systemd: 
	cp muditm.service /etc/systemd/system
	systemctl enable muditm.service
