#------------------------------------------------------------------------------
# Copyright (C) Robin Callender
#
# Makefile for all of the osrfx2 driver project.
#
# Pre-Build requirements:
#    1) install the compiler and binutils.
#    2) install and compile the linux kernel (to get usb headers).
#    3) copy osrfx2 sources into your home directory (~/osrfx2 for example).
#
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Tools used
#------------------------------------------------------------------------------
MAKE   = make
SUDO   = sudo

#------------------------------------------------------------------------------
# The dependency object files
#------------------------------------------------------------------------------
                
BINS   = driver/osrfx2.ko            \
         osrbulk/osrbulk             \
         osrfx2/osrfx2               \
         switch_events/switch_events

MAKES  = Makefile                    \
         driver/Makefile             \
         osrbulk/Makefile            \
         osrfx2/osrfx2               \
         switch_events/Makefile

#------------------------------------------------------------------------------
# Make each subcomponent of the project.
#------------------------------------------------------------------------------
all:    $(MAKES) $(BINS)

driver/osrfx2.ko: 
	$(MAKE) -C driver         -f Makefile

osrbulk/osrbulk: 
	$(MAKE) -C osrbulk        -f Makefile

osrfx2/osrfx2:
	$(MAKE) -C osrfx2         -f Makefile

switch_events/switch_events: 
	$(MAKE) -C switch_events  -f Makefile

#------------------------------------------------------------------------------
# Delete project build remenents
#------------------------------------------------------------------------------
clean: 
	$(MAKE) -C driver         -f Makefile clean
	$(MAKE) -C osrbulk        -f Makefile clean
	$(MAKE) -C osrfx2         -f Makefile clean
	$(MAKE) -C switch_events  -f Makefile clean

#------------------------------------------------------------------------------
# Install components
#------------------------------------------------------------------------------
install:
	$(SUDO) $(MAKE) -C driver -f Makefile install

#------------------------------------------------------------------------------
# Uninstall components
#------------------------------------------------------------------------------
uninstall:
	$(SUDO) $(MAKE) -C driver -f Makefile uninstall

