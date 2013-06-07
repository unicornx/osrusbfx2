#------------------------------------------------------------------------------
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
# Environmentals
#------------------------------------------------------------------------------
PWD         := $(shell pwd)
VERSION      = $(shell uname -r)
KERNEL_DIR  ?= /lib/modules/$(VERSION)/build

# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSCULL_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS) -I$(INCLUDE_DIR)

# [for Install]
INSTALL_DIR  = /lib/modules/$(VERSION)/kernel/drivers/usb/misc
UDEV_DIR     = /etc/udev

#------------------------------------------------------------------------------
# Build all components
#------------------------------------------------------------------------------
ifneq ($(KERNELRELEASE),)

# call from kernel build system
obj-m      := osrfx2.o

else

default:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) INCLUDE_DIR=$(PWD)/../include modules

endif

#------------------------------------------------------------------------------
# Delete build remenents
#------------------------------------------------------------------------------
clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.symvers Module.markers modules.order
	rm -f -r .tmp_versions

#------------------------------------------------------------------------------
# Install components
#------------------------------------------------------------------------------
install:
	-chmod 644 osrfx2.rules
	-cp -f osrfx2.rules $(UDEV_DIR)
	-ln -s $(UDEV_DIR)/osrfx2.rules $(UDEV_DIR)/rules.d/60-osrfx2.rules
	-install -d $(INSTALL_DIR)
	-install -c osrfx2.ko $(INSTALL_DIR)
	-/sbin/depmod -r

#------------------------------------------------------------------------------
# Refresh components : Used by developers to refresh just the driver files.
#------------------------------------------------------------------------------
refresh:
	-cp -f osrfx2.rules $(UDEV_DIR)
	-cp -f osrfx2.ko $(INSTALL_DIR)/osrfx2.ko

#------------------------------------------------------------------------------
# Uninstall components
#------------------------------------------------------------------------------
uninstall:
	-rm -f $(INSTALL_DIR)/osrfx2.ko
	-rm -f $(UDEV_DIR)/rules.d/60-osrfx2.rules
	-rm -f $(UDEV_DIR)/osrfx2.rules
	-/sbin/depmod -r

