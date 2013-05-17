#------------------------------------------------------------------------------
# Copyright (C) Robin Callender
#
# Makefile for the osrfx2 driver program.
#
# To build the driver:  
#   make
#
# To clean-up the built components:  
#   make clean
#
# To install the driver:
#   sudo make install
#
# To refresh the driver binary:
#   sudo make refresh
#
# To uninstall the driver:
#   sudo make uninstall
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Build object name
#------------------------------------------------------------------------------
obj-m      := osrfx2.o

#------------------------------------------------------------------------------
# Environmentals
#------------------------------------------------------------------------------
PWD         := $(shell pwd)
VERSION      = $(shell uname -r)

KERNEL_DIR  ?= /lib/modules/$(VERSION)/build
INCLUDE_DIR  = $(KERNELDIR)/include
INSTALL_DIR  = /lib/modules/$(VERSION)/kernel/drivers/usb/misc
UDEV_DIR     = /etc/udev
COLLISION    = osrfx2_collision

#------------------------------------------------------------------------------
# Build all components
#------------------------------------------------------------------------------
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD)

#------------------------------------------------------------------------------
# Delete build remenents
#------------------------------------------------------------------------------
clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f -r .tmp_versions

#------------------------------------------------------------------------------
# Install components
#
# NOTE: There is a potential collision with usbtest and osrfx2 on their use 
#       of Product_ID and Vendor_ID. The hack for this is to rename usbtest.
#       Search /lib/modules/`uname -r`/modules.usbmap for colliding IDs.
#------------------------------------------------------------------------------
install:
	-chmod 644 osrfx2.rules
	-cp -f osrfx2.rules $(UDEV_DIR)
	-ln -s $(UDEV_DIR)/osrfx2.rules $(UDEV_DIR)/rules.d/60-osrfx2.rules
	-install -d $(INSTALL_DIR)
	-install -c osrfx2.ko $(INSTALL_DIR)
	-mv $(INSTALL_DIR)/usbtest.ko $(INSTALL_DIR)/usbtest.ko.$(COLLISION)
	-/sbin/depmod -r

#------------------------------------------------------------------------------
# Refresh components : Used by developers to refresh just the driver files.
#------------------------------------------------------------------------------
refresh:
	-cp -f osrfx2.rules $(UDEV_DIR)
	-cp -f osrfx2.ko $(INSTALL_DIR)/osrfx2.ko

#------------------------------------------------------------------------------
# Uninstall components
#
# NOTE: See NOTE for install above. Upon uninstall the usbtest module will
#       be rename back to its original name.
#------------------------------------------------------------------------------
uninstall:
	-rm -f $(INSTALL_DIR)/osrfx2.ko
	-mv $(INSTALL_DIR)/usbtest.ko.$(COLLISION) $(INSTALL_DIR)/usbtest.ko
	-rm -f $(UDEV_DIR)/rules.d/60-osrfx2.rules
	-rm -f $(UDEV_DIR)/osrfx2.rules
	-/sbin/depmod -r

