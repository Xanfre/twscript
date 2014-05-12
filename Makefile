## @file
# Makefile for TWScript, modified from the Makefile included with Telliamed's
# Publis Scripts package.
#
# @author Chris Page <chris@starforge.co.uk>
# @author Tom N Harris <telliamed@whoopdedo.org>
#

# Original header comment follows:
#
###############################################################################
##  Makefile-gcc
##
##  This file is part of Public Scripts
##  Copyright (C) 2005-2011 Tom N Harris <telliamed@whoopdedo.org>
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program. If not, see <http://www.gnu.org/licenses/>.
##
###############################################################################

# Update these with the name of your script file, and the output .osm
MYSCRIPT  = TWScript
MYOSM     = twscript.osm
SCRIPTVER = 2.0.7

# Change this to `1` for Thief 1, 3 for SS2.
GAME      = 2

# Directories needed throughout the makefile
SRCDIR    = .
DOCDIR    = ./docs
BINDIR    = ./obj
PUBDIR    = ./pubscript
BASEDIR   = ./base
SCRPTDIR  = ./twscript
DISTDIR   = ./TWScript-$(SCRIPTVER)
LGDIR     = ../lg
SCRLIBDIR = ../ScriptLib

# Commands used
CC        = gcc
CXX       = g++
AR        = ar
LD        = g++
DLLTOOL   = dlltool
RC        = windres
PACKER    = 7z
MAKEDOCS  = $(DOCDIR)/makedocs.pl

DEFINES   = -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -DWIN32_LEAN_AND_MEAN
GAMEDEF   = -D_DARKGAME=$(GAME)

ifdef DEBUG
DEFINES  := $(DEFINES) -DDEBUG
CXXDEBUG  = -g -O0
LDDEBUG   = -g
LGLIB     = -llg-d
SCRIPTLIB = -lScript$(GAME)-d
else
DEFINES  := $(DEFINES) -DNDEBUG
CXXDEBUG  = -O2
LDDEBUG   =
LGLIB     = -llg
SCRIPTLIB = -lScript$(GAME)
endif

# Command arguments/flags
ARFLAGS   = rc
LDFLAGS   = -mwindows -mdll -Wl,--enable-auto-image-base
LIBDIRS   = -L. -L$(LGDIR) -L$(SCRLIBDIR)
LIBS      = $(LGLIB) -luuid
INCLUDES  = -I. -I$(SRCDIR) -I$(LGDIR) -I$(SCRLIBDIR) -I$(PUBDIR) -I$(BASEDIR) -I$(SCRPTDIR)
CXXFLAGS  = -W -Wall -Wno-unused-parameter -masm=intel -std=gnu++0x
DLLFLAGS  = --add-underscore
PACKARGS  = a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on

# Core scripts objects
PUB_OBJS  = $(PUBDIR)/ScriptModule.o $(PUBDIR)/Script.o $(PUBDIR)/Allocator.o $(PUBDIR)/exports.o
BASE_OBJS = $(BASEDIR)/TWBaseScript.o $(BASEDIR)/TWBaseTrap.o $(BASEDIR)/SavedCounter.o
MISC_OBJS = $(BINDIR)/ScriptDef.o $(PUBDIR)/utils.o

# Custom script objects
SCR_OBJS  = $(SCRPTDIR)/TWTrapAIBreath.o $(SCRPTDIR)/TWTrapPhysStateCtrl.o $(SCRPTDIR)/TWTrapSetSpeed.o $(SCRPTDIR)/TWCloudDrift.o
RES_OBJS  = $(BINDIR)/$(MYSCRIPT)_res.o

# Docs
DOC_FILES = $(DISTDIR)/docs/TWTrapAIBreath.html $(DISTDIR)/docs/TWTrapSetSpeed.html $(DISTDIR)/docs/TWTrapPhysStateCtrl.html \
	        $(DISTDIR)/docs/DesignNote.html $(DISTDIR)/docs/Changes.html $(DISTDIR)/docs/CheckingVersion.html $(DISTDIR)/docs/TWBaseTrap.html

# Archive file
PACKFILE = $(MYSCRIPT)-$(SCRIPTVER).7z

# Build rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CXXDEBUG) $(DEFINES) $(GAMEDEF) $(INCLUDES) -o $@ -c $<

%_res.o: %.rc
	$(RC) $(DEFINES) $(GAMEDEF) -o $@ -i $<

%.osm: %.o $(PUB_OBJS)
	$(LD) $(LDFLAGS) $(LDDEBUG) $(LIBDIRS) -o $@ script.def $< $(PUB_OBJS) $(SCRIPTLIB) $(LIBS)

$(BINDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXDEBUG) $(DEFINES) $(GAMEDEF) $(INCLUDES) -o $@ -c $<

$(BINDIR)/%_res.o: $(SRCDIR)/%.rc
	$(RC) $(DEFINES) $(GAMEDEF) -o $@ -i $<

$(PUBDIR)/%.o: $(PUBDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXDEBUG) $(DEFINES) $(GAMEDEF) $(INCLUDES) -o $@ -c $<

$(PUBDIR)/%_res.o: $(PUBDIR)/%.rc
	$(RC) $(DEFINES) $(GAMEDEF) -o $@ -i $<

$(BASEDIR)/%.o: $(BASEDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXDEBUG) $(DEFINES) $(GAMEDEF) $(INCLUDES) -o $@ -c $<

$(SCRPTDIR)/%.o: $(SCRPTDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXDEBUG) $(DEFINES) $(GAMEDEF) $(INCLUDES) -o $@ -c $<

$(DISTDIR)/docs/%.html: $(DOCDIR)/%.md
	$(MAKEDOCS) $< $@

# Targets
all: $(BINDIR) $(MYOSM)

clean: cleandist
	$(RM) $(BINDIR)/* $(BASEDIR)/*.o $(PUBDIR)/*.o $(SCRPTDIR)/*.o $(MYOSM)

cleandist:
	$(RM) $(PACKFILE)
	rm -rf $(DISTDIR)

dist: all $(DISTDIR) $(DOC_FILES)
	$(MAKEDOCS) README.md $(DISTDIR)/docs/README.html
	cp $(DOCDIR)/markdown.css $(DISTDIR)/docs/markdown.css
	cp $(DOCDIR)/resourcer.png $(DISTDIR)/docs/resourcer.png
	cp LICENSE $(DISTDIR)/
	cp $(MYOSM) $(DISTDIR)/
	$(PACKER) $(PACKARGS) $(PACKFILE) $(DISTDIR)
	rm -rf $(DISTDIR)

# Source dependencies
$(PUBDIR)/exports.o: $(PUBDIR)/ScriptModule.o
	$(DLLTOOL) $(DLLFLAGS) --dllname script.osm --output-exp $@ $^

$(PUBDIR)/ScriptModule.o: $(PUBDIR)/ScriptModule.cpp $(PUBDIR)/ScriptModule.h $(PUBDIR)/Allocator.h
$(PUBDIR)/Script.o: $(PUBDIR)/Script.cpp $(PUBDIR)/Script.h
$(PUBDIR)/Allocator.o: $(PUBDIR)/Allocator.cpp $(PUBDIR)/Allocator.h

$(BASEDIR)/TWBaseScript.o: $(BASEDIR)/TWBaseScript.cpp $(BASEDIR)/TWBaseScript.h $(PUBDIR)/Script.h $(PUBDIR)/ScriptModule.h
$(BASEDIR)/TWBaseTrap.o: $(BASEDIR)/TWBaseTrap.cpp $(BASEDIR)/TWBaseTrap.h $(BASEDIR)/TWBaseScript.h $(BASEDIR)/SavedCounter.h $(PUBDIR)/Script.h
$(BASEDIR)/SavedCounter.o: $(BASEDIR)/SavedCounter.cpp $(BASEDIR)/SavedCounter.h

$(SCRPTDIR)/TWTrapAIBreath.o: $(SCRPTDIR)/TWTrapAIBreath.cpp $(SCRPTDIR)/TWTrapAIBreath.h $(BASEDIR)/TWBaseTrap.h $(BASEDIR)/TWBaseScript.h $(PUBDIR)/Script.h
$(SCRPTDIR)/TWTrapPhysStateCtrl.o: $(SCRPTDIR)/TWTrapPhysStateCtrl.cpp $(SCRPTDIR)/TWTrapPhysStateCtrl.h $(BASEDIR)/TWBaseTrap.h $(BASEDIR)/TWBaseScript.h $(PUBDIR)/Script.h
$(SCRPTDIR)/TWTrapSetSpeed.o: $(SCRPTDIR)/TWTrapSetSpeed.cpp $(SCRPTDIR)/TWTrapSetSpeed.h $(BASEDIR)/TWBaseTrap.h $(BASEDIR)/TWBaseScript.h $(PUBDIR)/Script.h
$(SCRPTDIR)/TWCloudDrift.o: $(SCRPTDIR)/TWCloudDrift.cpp $(SCRPTDIR)/TWCloudDrift.h $(BASEDIR)/TWBaseScript.h $(PUBDIR)/Script.h

$(BINDIR)/ScriptDef.o: ScriptDef.cpp $(SCRPTDIR)/TWTrapSetSpeed.h $(SCRPTDIR)/TWTrapPhysStateCtrl.h $(BASEDIR)/TWBaseTrap.h $(BASEDIR)/TWBaseScript.h $(PUBDIR)/ScriptModule.h $(PUBDIR)/genscripts.h
$(BINDIR)/$(MYSCRIPT)_res.o: $(MYSCRIPT).rc $(PUBDIR)/version.rc

$(BINDIR):
	mkdir -p $@

$(DISTDIR):
	mkdir -p $(DISTDIR)/docs

$(MYOSM): $(SCR_OBJS) $(BASE_OBJS) $(PUB_OBJS) $(MISC_OBJS) $(RES_OBJS)
	$(LD) $(LDFLAGS) -Wl,--image-base=0x11200000 $(LDDEBUG) $(LIBDIRS) -o $@ $(PUBDIR)/script.def $^ $(SCRIPTLIB) $(LIBS)
