#------------------------------------------------------------------------------
# Copyright (C) unicornx
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
                
BINS   = exe/osrfx2                  \
         step1/osrfx2.ko             \
         step2/osrfx2.ko             \
         step3/osrfx2.ko             \
         step4/osrfx2.ko             \
         step5/osrfx2.ko

MAKES  = Makefile                    \
         exe/Makefile                \
         step1/Makefile              \
         step2/Makefile              \
         step3/Makefile              \
         step4/Makefile              \
         step5/Makefile

#------------------------------------------------------------------------------
# Make each subcomponent of the project.
#------------------------------------------------------------------------------
all:    $(MAKES) $(BINS)

exe/osrfx2: 
	$(MAKE) -C exe            -f Makefile

step1/osrfx2.ko: 
	$(MAKE) -C step1          -f Makefile

step2/osrfx2.ko: 
	$(MAKE) -C step2          -f Makefile

step3/osrfx2.ko: 
	$(MAKE) -C step3          -f Makefile

step4/osrfx2.ko: 
	$(MAKE) -C step4          -f Makefile

step5/osrfx2.ko: 
	$(MAKE) -C step5          -f Makefile

#------------------------------------------------------------------------------
# Delete project build remenents
#------------------------------------------------------------------------------
clean: 
	$(MAKE) -C driver         -f Makefile clean
	$(MAKE) -C exe            -f Makefile clean
	$(MAKE) -C step1          -f Makefile clean
	$(MAKE) -C step2          -f Makefile clean
	$(MAKE) -C step3          -f Makefile clean
	$(MAKE) -C step4          -f Makefile clean
	$(MAKE) -C step5          -f Makefile clean	

#------------------------------------------------------------------------------
# Install components
#------------------------------------------------------------------------------
#install:
#	$(SUDO) $(MAKE) -C driver -f Makefile install

#------------------------------------------------------------------------------
# Uninstall components
#------------------------------------------------------------------------------
#uninstall:
#	$(SUDO) $(MAKE) -C driver -f Makefile uninstall

