/**
 * osrfx2 - A Driver for the OSR USB FX2 Learning Kit device
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * Step1, this is the most basic step. The source file contains absolutely 
 * minimal amount of code to get the driver loaded in memory and respond to 
 * PNP and Power events. You can install, uninstall, disable, enable, suspend 
 * and resume the system and everything will work fine.
 * In details, the step shows:
 * 1) how to create a simplest driver module for osrfx2, with 
      module's init and exit funtion so we can install and remove it.
 * 2) how to export device info vid/pid to userspace so udev can bind&load the 
      driver when osrfx2 device is connected
 * 3) how to register the driver to usb core and two major callback functions 
      (probe & disconnect) are provoded with minimum stuff filled.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "public.h"

#define OSRFX2_DEBUG 1
#if OSRFX2_DEBUG
	#define dbg_info(dev, format, arg...) do {\
		dev_info(dev , format , ## arg);\
	} while (0)

	#define ENTRY(dev, format, arg...) do {\
		if (dev) \
			dbg_info(dev , "--> %s: "format"\n" , __func__ , ## arg);\
		else\
			pr_info("--> %s: "format"\n" , __func__ , ## arg);\
	} while (0)

	/* given retval < 0, then EXIT will be output as error */
	#define EXIT(dev, retval, format, arg...) do {\
		if ((dev) && (retval < 0))\
			dev_err(dev , "<-- %s: return (%d). "format"\n" , __func__ , retval , ## arg);\
		if ((dev) && (retval >= 0))\
			dev_info(dev , "<-- %s: return (%d). "format"\n" , __func__ , retval , ## arg);\
		if ((!dev) && (retval < 0))\
			pr_err("<-- %s: return (%d). "format"\n" , __func__ , retval , ## arg);\
		if ((!dev) && (retval >= 0))\
			pr_info("<-- %s: return (%d). "format"\n" , __func__ , retval , ## arg);\
	} while (0)

#else
	#define dbg_info(dev, format, arg...)
	#define ENTRY(dev, format, arg...)
	#define EXIT(dev, retval, format, arg...)
#endif /* OSRFX2_DEBUG */

/**
 * Table of devices that work with this driver.
 * The struct usb_device_id structure is used to define a list of the different
 * types of USB devices that a driver supports. 
 * In most cases, drivers will create this table by using USB_DEVICE(), or 
 * similar macros designed for that purpose. After defining the table, we can
 * export it to userspace using MODULE_DEVICE_TABLE(), and provide it to 
 * the USB core through usb_driver structure - when registering the driver.
 */
static const struct usb_device_id osrfx2_table [] = {
	{ USB_DEVICE(OSRFX2_VENDOR_ID, OSRFX2_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

/*
 * MODULE_DEVICE_TABLE() creates a local variable in our .ko file 
 * called __mod_usb_device_table that points to the list of struct usb_device_id. 
 * 
 * In the procedure of installing the drivers - read the Makefile, 
 * - first, we will put our usb driver modules file(.ko) under to
 * /lib/modules/KERNEL_VERSION/kernel/drivers/usb/misc, 
 * - then run depmod. This program searches all modules for the symbol 
 * __mod_usb_device_table. If that symbol is found, it pulls the data out of 
 * the module and adds it to the file /lib/modules/KERNEL_VERSION/modules.usbmap.
 * This file records the mapping info between the driver_info (mainly the 
 * driver/module name, here is osrfx2) and the device info, such as vid/pid. 
 * After depmod completes, all USB devices that are supported by modules in 
 * the kernel are listed, along with their module names, in that file. 
 *
 * When a device is plugged into the system, the kernel will notify the udev 
 * system a new USB device has been found, plus some device info, such as vid/pid,
 * and a few other bits and pieces.
 * Udev then will call modprobe, and modprobe simply searches that modules.usbmap
 * file to map the device info such as vid/pid back to a "real" driver name 
 * - osrfx2 and load the module.
 */
MODULE_DEVICE_TABLE (usb, osrfx2_table);

/** 
 * The probe function is called when a device is installed that the USB core 
 * thinks this driver should handle; the probe function should perform checks 
 * on the information passed to it about the device and decide whether the 
 * driver is really appropriate for that device.
 */
static int osrfx2_drv_probe( 
			struct usb_interface            *interface, 
			const struct usb_device_id      *id)
{
	int             retval  = 0;
	
	ENTRY(&interface->dev, "");

	EXIT(&interface->dev, retval, "");

	return retval;
}

/**
 * The disconnect function is called when the driver should no longer control 
 * the device for some reason and can do clean-up.
 */
static void osrfx2_drv_disconnect(struct usb_interface *interface)
{
	ENTRY(&interface->dev, "");

	EXIT(&interface->dev, 0, "osrfx2_%d now disconnected", interface->minor);
}

/**
 * The main structure that all USB drivers must create is a struct usb_driver. 
 * This structure must be filled out by the USB driver and consists of a number 
 * of function callbacks and variables that describe the USB driver to the 
 * USB core.
 * To register the struct usb_driver with the USB core, a call to 
 * usb_register_driver should be made with a pointer to the struct usb_driver. 
 * This is traditionally done in the module initialization code for the USB 
 * driver, here it is osrfx2_int.
 * When the USB driver is to be unloaded (rmmod), the struct usb_driver needs to 
 * be unregistered from the kernel. This is done with a call to 
 * usb_deregister_driver in module exiting code, here it is osrfx2_exit. When 
 * this call happens, any USB interfaces that were currently bound to this driver
 * are disconnected, and the disconnect function (osr_disconnect) will be called 
 * for them. Note the disconnect function will be triggerred in two cases, one is
 * the case I just described upon, another happens when the device is removed from
 * the system, while for the second case, normally the driver module has not been
 * unloaded and exit function also is not called till we issue rmmod command.
 */
static struct usb_driver osrfx2_driver = {
	.name        = "osrfx2",
	.probe       = osrfx2_drv_probe,
	.disconnect  = osrfx2_drv_disconnect,
	.id_table    = osrfx2_table,
};

/*
 * This driver's commission routine, called when the driver module is installed       
 */
static int __init osrfx2_init(void)
{
	int result;

	ENTRY(NULL, "");

	/* register this driver with the USB subsystem */
	result = usb_register(&osrfx2_driver);

	EXIT(NULL, result, "");

	return result;
}

/*
 * This driver's decommission routine, called when the driver module is removed
 */
static void __exit osrfx2_exit(void)
{
	ENTRY(NULL, "");

	/* deregister this driver with the USB subsystem */
	usb_deregister(&osrfx2_driver);

	EXIT(NULL, 0, "");
}

/*****************************************************************************/
/* Advertise this driver's init and exit routines                            */
/*****************************************************************************/
module_init( osrfx2_init );
module_exit( osrfx2_exit );

MODULE_AUTHOR("unicornx");
MODULE_DESCRIPTION("A driver for the OSR USB-FX2 Learning Kit device");
MODULE_LICENSE("GPL");

