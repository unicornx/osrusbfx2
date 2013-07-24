/**
 * osrfx2 - A Driver for the OSR USB FX2 Learning Kit device
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * Step2, this step:
 * 1) Creates a context for the osrfx2 device object.
 * 2) Registers a usb interface and provide associated basic file operaitons
 *    so that application can open the device.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "public.h"

#define OSRFX2_DEVICE_MINOR_BASE   192

/* share mode when open */
#define FILE_NOSHARE_READWRITE	1 /* 1: no share, 0: share */

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

/*
 * This is the private device context structure to hold all of our device 
 * specific stuff.
 */
struct osrfx2 {
	/* 1. device level attributes */
	struct usb_device	*udev;			/* the usb device for this device */
        struct usb_interface	*interface;		/* the interface for this usb device
                                                         * Note, for osrfx2 device, it has only
                                                         * one interface.
                                                         */
	struct kref		kref;			/* Refrence counter for a device */
	struct mutex		io_mutex;		/* synchronize I/O with disconnect */
#if FILE_NOSHARE_READWRITE
	atomic_t 		bulk_read_available;	/* share read */
	atomic_t 		bulk_write_available;	/* share write */	 
#endif /* FILE_NOSHARE_READWRITE */
	
};

/**
 * constructor of osrfx2 device
 */
static int osrfx2_new(struct osrfx2 **fx2devOut,
		      struct usb_interface *interface)
{
	int retval = 0;
	struct osrfx2 *fx2dev = NULL;

	/* allocate memory for our device context and initialize it */
	fx2dev = kzalloc(sizeof(struct osrfx2), GFP_KERNEL);
	if (!fx2dev) {
		dev_err(&interface->dev, "Out of memory\n");
		retval = -ENOMEM;
		goto exit;
	}

	/* common attributes */
	kref_init(&(fx2dev->kref));
	mutex_init(&(fx2dev->io_mutex));
	/* store our device and interface pointers for later usage */
	fx2dev->udev = usb_get_dev(interface_to_usbdev(interface));
	fx2dev->interface = interface;
#if FILE_NOSHARE_READWRITE
	fx2dev->bulk_read_available  = (atomic_t)ATOMIC_INIT(1);
	fx2dev->bulk_write_available = (atomic_t)ATOMIC_INIT(1);
#endif /* FILE_NOSHARE_READWRITE */

exit:
	if (retval < 0) {
	        *fx2devOut = NULL;
	        /* leave other deconstruct in osrfx2_delete() */
	} else {
		*fx2devOut = fx2dev;
	}
	return retval;
}

/**
 * de-constructor of the osrfx2 device
 */
static void osrfx2_delete(struct kref *kref)
{	
	/* use macro container_of to find the appropriate device structure */
	struct osrfx2 *fx2dev = container_of(kref, struct osrfx2, kref);

	ENTRY(&(fx2dev->interface->dev), "");

	/* release a use of the usb device structure */
	usb_put_dev(fx2dev->udev);

	/* free device context memory */
	kfree(fx2dev);

	EXIT(&(fx2dev->interface->dev), 0, "");
}

static struct usb_driver osrfx2_driver;

/*
 * The open method is always the first operation performed on the device file.
 * It is provided for a driver to do any initialization in preparation for 
 * later operations. In most drivers, open should perform the following tasks:
 * 1) Check for device-specific errors (such as device-not-ready or similar 
 *    hardware problems).
 * 2) Initialize the device if it is being opened for the first time
 * 3) Update the f_op pointer, if necessary
 * 4) Allocate and fill any data structure to be put in filp->private_data.
 *    Driver can store device context content, such as device handler,
 *    interface handler etc. in the private_data. We can treat the private_data as
 *    the bridge between file structure and lower level device object. More than 
 *    one application processes have their own file descriptor opened for a device.
 *    After driver store device context data in the open method for this device, 
 *    other file operation methods can get access to these data easily later.
 */
static int osrfx2_fp_open(struct inode *inode, struct file *file)
{
	struct osrfx2           *fx2dev;
	struct usb_interface    *interface;
	int                     subminor;
	int                     retval = 0;
#if FILE_NOSHARE_READWRITE
	int 			oflags;
#endif /* FILE_NOSHARE_READWRITE */

	subminor = iminor(inode);
	interface = usb_find_interface(&osrfx2_driver, subminor);
	if (!interface) {
		ENTRY(NULL, "can't find device for minor %d\n", subminor);
		EXIT(NULL, -ENODEV, "");
		return -ENODEV;
	}
	
	ENTRY(&interface->dev, "minor = %d\n", subminor);

	fx2dev = usb_get_intfdata(interface);
	if (!fx2dev) {
	        retval = -ENODEV;
	        goto exit;
	}

#if FILE_NOSHARE_READWRITE
	/*
	 *  Serialize access to each of the bulk pipes.
	 */
	oflags = (file->f_flags & O_ACCMODE);
	if ((oflags == O_WRONLY) || (oflags == O_RDWR)) {
		if (atomic_dec_and_test( &fx2dev->bulk_write_available ) == 0) {
			atomic_inc( &fx2dev->bulk_write_available );
			retval = -EBUSY;
			goto exit;
		}
	}

	if ((oflags == O_RDONLY) || (oflags == O_RDWR)) {
		if (atomic_dec_and_test( &fx2dev->bulk_read_available ) == 0) {
			atomic_inc( &fx2dev->bulk_read_available );
			if (oflags == O_RDWR) 
				atomic_inc( &fx2dev->bulk_write_available );
			retval = -EBUSY;
			goto exit;
		}
	}
#endif /* FILE_NOSHARE_READWRITE */

	/**
	 * Set this device as non-seekable bcos it does not make sense for osrfx2. 
	 * Refer to http://lwn.net/Articles/97154/ "Safe seeks" and ldd3 section 6.5.
	 */ 
	retval = nonseekable_open(inode, file);
	if (0 != retval) {
		goto exit;
	}

	/* increment our usage count for the device */
	kref_get(&fx2dev->kref);

	/* save our object in the file's private structure */
	file->private_data = fx2dev;

exit:
	EXIT(&interface->dev, retval, "");
	return retval;
}

/**
 * The release operation is invoked when the file structure is being released.
 * The role of the release method is the reverse of open. Normally it should 
 * perform the following tasks:
 * 1) Deallocate anything that open allocated in filp->private_data.
 * 2) Shut down the device on last close.
 */
static int osrfx2_fp_release(struct inode *inode, struct file *file)
{
	struct osrfx2 *fx2dev;
#if FILE_NOSHARE_READWRITE
	int oflags;
#endif /* FILE_NOSHARE_READWRITE */

	fx2dev = (struct osrfx2 *)file->private_data;
	if (NULL == fx2dev) {
		ENTRY(NULL, "get device failed!");
		EXIT(NULL, -ENODEV, "");
		return -ENODEV;
	}

	ENTRY(&(fx2dev->interface->dev), "minor = %d", iminor(inode));

#if FILE_NOSHARE_READWRITE
	/* 
	 *  Release any bulk_[write|read]_available serialization.
	 */
	oflags = (file->f_flags & O_ACCMODE);
	if ((oflags == O_WRONLY) || (oflags == O_RDWR))
		atomic_inc(&fx2dev->bulk_write_available);

	if ((oflags == O_RDONLY) || (oflags == O_RDWR)) 
		atomic_inc(&fx2dev->bulk_read_available);
#endif /* FILE_NOSHARE_READWRITE */

	/* decrement the count on our device */
	kref_put(&fx2dev->kref, osrfx2_delete);

	EXIT(&(fx2dev->interface->dev), 0, "");
	return 0;
}

/**
 * In UNIX/LINUX world, from applicaiton perspective, eveything, including device,
 * is a file. To communicate a device, applicaiton will open the file which
 * represnet the device and call other file operaiton functions. After we 
 * register the device driver and get a minor number for one
 * device connected, we need to connect driver's operations to those numbers. 
 * The file_operations structure is how a usb/char driver sets up this connection. 
 * The structure, is a collection of function pointers. Each open file 
 * (represented internally by a file structure) is associated with its own set of 
 * functions. Application can invoke them through kernel on a device.
 */
static struct file_operations osrfx2_fops = {
	.owner		= THIS_MODULE,
	.open		= osrfx2_fp_open,
	.release	= osrfx2_fp_release,
	.llseek		= no_llseek,
};

/** 
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with sysfs and the driver core
 * Note, since 2.6.17, devfs was removed, so we need not care the devfs 
 * related stuff for such as .name and .mode.
 */
static struct usb_class_driver osrfx2_class = {
	.name = "osrfx2_%d",  /* The name that sysfs uses to describe the device.*/
	.fops = &osrfx2_fops, /* Pointer to the struct file_operations that this 
			       * driver has defined to use to register as the 
			       * character device.
			       */
	.minor_base = 
	  OSRFX2_DEVICE_MINOR_BASE,	
	  /* Start of the assigned minor range for this driver. 
	   * This member is ignored when CONFIG_USB_DYNAMIC_MINORS configuration
	   * option has been enabled for the kernel.
	   * Enable the CONFIG_USB_DYNAMIC_MINORS will make all minor numbers 
	   * for the device are allocated on a first-come, first-served manner.
	   * It is recommended that systems that have enabled this option use 
	   * a program such as udev to manage the device nodes in the system,
	   * as a static /dev tree will not work properly
	   */
};

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
	int             retval  = -ENOMEM;
	struct osrfx2   *fx2dev = NULL;
		
	ENTRY(&interface->dev, "");

	retval = osrfx2_new(&fx2dev, interface);
	if (0 != retval) 
		goto error;
//	assert ( fx2dev );

	/**
	 * USB driver needs to retrieve the local device context data structure 
	 * that is associated with this struct usb_interface later in the 
	 * lifecycle of the device, so we have to save our dev data pointer 
	 * in this interface device. We can use usb_set_intfdata to save the data
	 * and use usb_get_intfdata to retrieve it in other methods later 

	 * Note: compare this store/retrieve with that in open method, the save of
	 * device context data in open method is on file.private_data, that's for
	 * remember device info for file operation scope, but here is inside 
	 * device/driver scope. Don't confuse these two.
	 */
	usb_set_intfdata(interface, fx2dev);

	/* we can register the device now, as it is ready 
	 *
	 * On Linux, there are two reserved major ids for USB, (include/linux/usb.h)
	 * #define USB_MAJOR                       180
	 * #define USB_DEVICE_MAJOR                189
	 * Only if the driver for a USB device is not registered with other 
	 * subsystem, for example input, vedio and etc, USB_MAJOR is allocated 
	 * for usb_interface, and USB_DEVICE_MAJOR is allocated for usb_device.
	 * We can check the device file in /dev to have deeper understanding on this.
	 * For example, after one osrfx2 is connected, we can see two important
	 * file nodes are created under /dev.
	 * (1) crw-rw-r-- 1 root root 189, 4 2013-05-31 11:28 /dev/bus/usb/001/005
	 * (2) crw-rw---- 1 root root 180, 192 2013-05-31 11:47 /dev/osrf2x2_0
	 * The first one is created for the usb device and it will help sysfs to 
	 * create proper file nodes. The Second one is the actual device file created
	 * by udev for userspace applicaiton to access.
	 * Due to in Linux, every usb driver serves a usb interface. So you can see
	 * the first input param type of usb_register_dev is usb_interface. That's
	 * why the /dev/osrfx2_0 is allocated with a major id = 180 (USB_MAJOR)
	 * plus a minor id started from 192 (OSRFX2_DEVICE_MINOR_BASE).
	 */
	retval = usb_register_dev(interface, &osrfx2_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev,
			"Not able to register and get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */
	EXIT(&interface->dev, 0,
		"OSRFX2 device now attached to osrfx2_%d", interface->minor);
	return 0;

error:
	if (fx2dev)
		/* this frees allocated memory */
		kref_put ( &fx2dev->kref, osrfx2_delete );

	EXIT(&interface->dev, retval, "");

	return retval;
}

/**
 * The disconnect function is called when the driver should no longer control 
 * the device for some reason and can do clean-up.
 *
 * If you read the ldd3 book sample, the skel_disconnect() used 
 * lock_kernel/unlock_kernel to prevent xxx_open() from racing xxx_disconnect().
 * Since BKL was removed since 2.6.39 ( @http://kernelnewbies.org/BigKernelLock ),
 * to catch up the usb_skeleton.c since 2.6.32, we now use a mutex instead.
 */
static void osrfx2_drv_disconnect(struct usb_interface *interface)
{
	struct osrfx2   *fx2dev;

	ENTRY(&interface->dev, "");

	fx2dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &osrfx2_class);

	/* decrement our usage count */
	kref_put(&fx2dev->kref, osrfx2_delete);

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

