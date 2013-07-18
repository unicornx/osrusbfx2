/**
 * osrfx2 - A Driver for the OSR USB FX2 Learning Kit device
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * Step4, this step:
 * 1) How to process read/write on bulk IN/OUT endpoints.
 * 2) How to support poll
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <asm/uaccess.h>
#include <linux/poll.h>

#include "public.h"

#define OSRFX2_DEVICE_MINOR_BASE   192

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER            (PAGE_SIZE - 512)
/* MAX_TRANSFER is chosen so that the VM is not stressed by
   allocations > PAGE_SIZE and the number of packets in a page
   is an integer 512 is the largest possible packet on EHCI */
#define WRITES_IN_FLIGHT        4
/* arbitrarily chosen */

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
	struct usb_device      	*udev;			/* the usb device for this device */
        struct usb_interface   	*interface;		/* the interface for this usb device
                                                         * Note, for osrfx2 device, it has only
                                                         * one interface.
                                                         */
	bool                   	connected;		/* flag if the device is connected */                                                    
	struct kref            	kref;			/* Refrence counter for a device */
	struct mutex           	io_mutex;		/* synchronize I/O with disconnect */
	struct usb_anchor      	submitted;		/* in case we need to retract our submissions */
	spinlock_t             	dev_lock;		/* lock for all device level attributes */
	/* all device attributes which will be be accessed in both process 
	 * context and interrupted context should be guarded by dev_lock
	*/
	int                    	errors;			/* error status for the last request tanked */    
	wait_queue_head_t      	pollq_wait;		/* wait queue for select/poll */
#if FILE_NOSHARE_READWRITE
	atomic_t 		bulk_read_available;	/* share read */
	atomic_t 		bulk_write_available;	/* share write */	 
#endif /* FILE_NOSHARE_READWRITE */

	/* 2. USB Bulk IN endpoint */
	__u8                   	bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
	size_t                 	bulk_in_size;		/* max packet size of the bulk in endpoint */
	unsigned char          	*bulk_in_buffer;	/* the buffer to receive data */
	struct urb             	*bulk_in_urb;		/* the urb to read data with */
	bool                   	ongoing_read;		/* flag a read is going on */
	wait_queue_head_t      	bulk_in_wait;		/* to wait for an ongoing read */
	size_t                 	bulk_in_filled;		/* number of bytes in the buffer */
	size_t                 	bulk_in_copied;		/* already copied to user space */

	/* 3. USB Bulk OUT endpoint 
	 * Note for Bulk OUT/write, different against read operaiton, we need not 
	 * a specific bulk_out_buffer allocated and stored in device context.
	 * When sending data, we will allocate dma-consistent buffer and free it in
	 * write callback routine the time after write IO is completed.
	 */
	__u8                   	bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
	/**
	 * NOTE: write_inflight & write_limit_sem are defined for poll for write.
	 * OSRFX2 device spec states that the firmware will loop the transfer
	 * from OUT endpoint to IN endpoint as a FIFO. The firmware will
	 * configure the FX2 OUT/IN endpoints as dual-buffer, that means 
	 * the FX2 will buffer up-to four(WRITES_IN_FLIGHT) write packet (two on EP6 and two on EP8).
	 * The buffers can be drained by issuing reads to EP8.
	 * The fifth outstanding write packet attempt will cause the write
	 * to block, waiting for the arrival of a read request to
	 * effectively free a buffer into which the write data can be held.
	 * From driver perspective, if there are already WRITES_IN_FLIGHT sent without
	 * being drained, the (WRITES_IN_FLIGHT+1)th submission will be blocked 
	 * and its callback will NOT be triggered by usb core.
	 * So we define write_inflight to record the number of inflight write URBs.
	 * If write_inflight < WRITES_IN_FLIGHT, write is allowed, otherwise not.
	 * And aslo use a semaphore up most to WRITES_IN_FLIGHT to block the 
	 * outstanding number of write operations on the device.
	 */
	int			write_inflight;		/* number of the write urbs' in flight */ 
	struct semaphore       	write_limit_sem;	/* limiting the number of writes in progress */
};

/**
 * This routine will attempt to find the required endpoints and 
 * retain relevant information in the osrfx2 context structure. 
 */
static int osrfx2_find_endpoints(struct osrfx2 *fx2dev)
{
	struct usb_interface *interface;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;

	interface = fx2dev->interface;

	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;

	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!fx2dev->bulk_in_endpointAddr &&
		    usb_endpoint_dir_in(endpoint) &&
		    usb_endpoint_xfer_bulk(endpoint)) {
			/* we found a bulk in endpoint */
			fx2dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			/* since 3.2, we need to use usb_endpoint_maxp() */
			fx2dev->bulk_in_size = endpoint->wMaxPacketSize; 
		}

		if (!fx2dev->bulk_out_endpointAddr &&
		    usb_endpoint_dir_out(endpoint) &&
		    usb_endpoint_xfer_bulk(endpoint)) {
			/* we found a bulk out endpoint */
			fx2dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}

	if (fx2dev->bulk_in_endpointAddr == 0 ||
	    fx2dev->bulk_out_endpointAddr == 0 ) {
		dev_err(&interface->dev, 
			"%s - failed to find required endpoints\n",
			__func__);
		return -ENODEV;
	}

	return 0;
}

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
	init_usb_anchor(&fx2dev->submitted);
	spin_lock_init(&fx2dev->dev_lock);
	/* store our device and interface pointers for later usage */
	fx2dev->udev = usb_get_dev(interface_to_usbdev(interface));
	fx2dev->interface = interface;
	fx2dev->connected = true;
	/* bulk fifo/poll */
	init_waitqueue_head(&fx2dev->pollq_wait);
#if FILE_NOSHARE_READWRITE
	fx2dev->bulk_read_available  = (atomic_t)ATOMIC_INIT(1);
	fx2dev->bulk_write_available = (atomic_t)ATOMIC_INIT(1);
#endif /* FILE_NOSHARE_READWRITE */

	/* probe and find the endpoints we require */
	retval = osrfx2_find_endpoints(fx2dev);
	if (0 != retval)
		goto exit;

	/* setup other attributes related to the endpoints we care */
	/* Bulk IN */
	fx2dev->bulk_in_buffer = kmalloc(fx2dev->bulk_in_size, GFP_KERNEL);
	if (!fx2dev->bulk_in_buffer) {
	        dev_err(&(fx2dev->interface->dev),
			"Could not allocate bulk_in_buffer\n");
		retval = -ENOMEM;
		goto exit;
	}
	fx2dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!fx2dev->bulk_in_urb) {
		dev_err(&(fx2dev->interface->dev),
			"Could not allocate bulk_in_urb\n");
		retval = -ENOMEM;
		goto exit;
	}
	init_waitqueue_head(&fx2dev->bulk_in_wait);

	/* Bulk OUT */
	sema_init(&fx2dev->write_limit_sem, WRITES_IN_FLIGHT);

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
	/* free Bulk IN memory */
	if (fx2dev->bulk_in_buffer) {
		kfree(fx2dev->bulk_in_buffer);
	}
	if (fx2dev->bulk_in_urb) {
		usb_free_urb(fx2dev->bulk_in_urb);
	}
	/* free Bulk OUT memory ... */

	/* free device context memory */
	kfree(fx2dev);

	EXIT(&(fx2dev->interface->dev), 0, "");
}

static void osrfx2_draw_down(struct osrfx2 *fx2dev)
{
	int time;

	time = usb_wait_anchor_empty_timeout(&fx2dev->submitted, 1000);
	if(!time)
		usb_kill_anchored_urbs(&fx2dev->submitted);

	usb_kill_urb(fx2dev->bulk_in_urb);
}

/**
 * We choose use sysfs attribte to read/write io data. More details please 
 * refer to http://www.ibm.com/developerworks/cn/linux/l-cn-sysfs/#resources
 * & http://blog.csdn.net/king_208/article/details/5599934 for show/store
 * 
 * This routine will retrieve the bargraph LED state, format it and return a
 * representative string.                                                   
 *                                                                         
 * Note the two different function defintions depending on kernel version.
 */
static ssize_t osrfx2_attr_bargraph_show(
	struct device           *dev, 
	struct device_attribute *attr, 
	char                    *buf)
{
	struct usb_interface  *intf = to_usb_interface(dev);
	struct osrfx2         *fx2dev = usb_get_intfdata(intf);
	struct bargraph_state *packet = NULL;
	int retval = 0;

	ENTRY(dev, "");
		
	packet = kzalloc(sizeof(*packet), GFP_KERNEL);
	if (!packet) {
		retval = -ENOMEM;
		goto exit;
	}

	retval = usb_control_msg(fx2dev->udev,
 				 usb_rcvctrlpipe(fx2dev->udev, 0), 
 				 OSRFX2_READ_BARGRAPH_DISPLAY,
 				 USB_DIR_IN | USB_TYPE_VENDOR,
 				 0,
 				 0,
 				 packet,
 				 sizeof(*packet),
 				 USB_CTRL_GET_TIMEOUT);

	if (retval < 0) {
		goto error;
	}

	retval = sprintf(buf, 
			 "%s%s%s%s%s%s%s%s",    /* bottom LED --> top LED */
			 (packet->Bar1) ? "*" : ".",
			 (packet->Bar2) ? "*" : ".",
			 (packet->Bar3) ? "*" : ".",
			 (packet->Bar4) ? "*" : ".",
			 (packet->Bar5) ? "*" : ".",
			 (packet->Bar6) ? "*" : ".",
			 (packet->Bar7) ? "*" : ".",
			 (packet->Bar8) ? "*" : "." );

error:
	if (packet) kfree (packet);
exit:
	EXIT(dev, retval, "");
	return retval;
}

/*
 * This routine will set the bargraph LEDs.
 *
 * Note the two different function defintions depending on kernel version.
 */
static ssize_t osrfx2_attr_bargraph_store(
		struct device           *dev,
		struct device_attribute *attr,
		const char              *buf,
		size_t                  count)
{
	struct usb_interface  *intf   = to_usb_interface(dev);
	struct osrfx2         *fx2dev = usb_get_intfdata(intf);
	struct bargraph_state *packet = NULL;

	int                   retval;
	char                  *end;

	ENTRY(dev, "");

	packet = kzalloc(sizeof(*packet), GFP_KERNEL);
	if (!packet) {
		retval = -ENOMEM;
		goto exit;
	}

	packet->BarsOctet = (unsigned char)(simple_strtoul(buf, &end, 10) & 0xFF);
	if (buf == end) {
		packet->BarsOctet = 0;
	}

	retval = usb_control_msg(fx2dev->udev,
				 usb_sndctrlpipe(fx2dev->udev, 0),
				 OSRFX2_SET_BARGRAPH_DISPLAY,
				 USB_DIR_OUT | USB_TYPE_VENDOR,
				 0,
				 0,
				 packet,
				 sizeof(*packet),
				 USB_CTRL_GET_TIMEOUT);

exit:
	if (packet) kfree(packet);

	EXIT(dev, retval, "");

	return retval;
}

/*
 * This macro DEVICE_ATTR creates an attribute under the sysfs directory
 * ---  /sys/bus/usb/devices/<root_hub>-<hub>:1.0/bargraph
 *
 * The DEVICE_ATTR() will create "dev_attr_bargraph".
 * "dev_attr_bargraph" is referenced in both probe (create the attribute) and 
 * disconnect (remove the attribute) routines.
 */
static DEVICE_ATTR(bargraph, \
		   S_IRUGO | S_IWUGO, \
		   osrfx2_attr_bargraph_show, \
		   osrfx2_attr_bargraph_store);

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

static int osrfx2_fp_flush(struct file *file, fl_owner_t id)
{
	struct osrfx2   *fx2dev;
	int             rv;

	fx2dev = file->private_data;
	if (NULL == fx2dev) {
		ENTRY(NULL, "get device failed!");
		EXIT(NULL, -ENODEV, "");
		return -ENODEV;
	}
    
	ENTRY(&(fx2dev->interface->dev), "");

	mutex_lock(&fx2dev->io_mutex);

	/* wait for io to stop */
	osrfx2_draw_down(fx2dev);

	/* read out errors, leave subsequent opens a clean slate */
	spin_lock_irq(&fx2dev->dev_lock);
	/* EPIPE means "halt" or "stall", for all error status, converted to
	 * EIO, except EPIPE.
	 */
	rv = fx2dev->errors ? (fx2dev->errors == -EPIPE ? -EPIPE : -EIO) : 0;
	fx2dev->errors = 0;
	fx2dev->write_inflight = 0;
	spin_unlock_irq(&fx2dev->dev_lock);

	mutex_unlock(&fx2dev->io_mutex);

	EXIT(&(fx2dev->interface->dev), rv, "");

	return rv;
}

static void osrfx2_fp_read_bulk_callback(struct urb *urb)
{
	struct osrfx2 *fx2dev = urb->context;

	if (!fx2dev) {
		ENTRY(NULL, "can't find device.");
		EXIT(NULL, -1, "");
		return;
	}

	ENTRY(&(fx2dev->interface->dev),
		"urb status=%d, actual length = %d",
		urb->status, urb->actual_length);

	spin_lock(&fx2dev->dev_lock);

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		      urb->status == -ECONNRESET ||
		      urb->status == -ESHUTDOWN)) {
			dev_err(&(fx2dev->interface->dev),
				"%s - nonzero read bulk status received: %d\n",
				__func__, urb->status );
		}

		fx2dev->errors = urb->status;
	} else {
		fx2dev->bulk_in_filled = urb->actual_length;
	}
	fx2dev->ongoing_read = 0;

	spin_unlock(&fx2dev->dev_lock);

	wake_up_interruptible(&fx2dev->bulk_in_wait);

	/* wake-up blocked poll */
	wake_up(&(fx2dev->pollq_wait));
	dbg_info(&(fx2dev->interface->dev),
		"%s - wake up poll when data is received.\n", __func__);

	EXIT(&(fx2dev->interface->dev), fx2dev->errors, "");
}

static int osrfx2_fp_read_submit_urb(struct osrfx2 *fx2dev, size_t count)
{
	int rv;

	/* prepare a read URB */
	usb_fill_bulk_urb(fx2dev->bulk_in_urb,
			  fx2dev->udev,
			  usb_rcvbulkpipe(fx2dev->udev, fx2dev->bulk_in_endpointAddr),
			  fx2dev->bulk_in_buffer,
			  min(fx2dev->bulk_in_size, count),
			  osrfx2_fp_read_bulk_callback,
			  fx2dev);
	/* tell everybody to leave the URB alone */
	spin_lock_irq(&fx2dev->dev_lock);
	fx2dev->ongoing_read = 1;
	spin_unlock_irq(&fx2dev->dev_lock);

	/* submit bulk in urb, which means no data to deliver */
	fx2dev->bulk_in_filled = 0;
	fx2dev->bulk_in_copied = 0;

	/* submit it */
	rv = usb_submit_urb(fx2dev->bulk_in_urb, GFP_KERNEL);
	if (rv < 0) {
		dev_err(&fx2dev->interface->dev,
			"%s - failed submitting read urb, error %d\n",
			__func__, rv);
		rv = (rv == -ENOMEM) ? rv : -EIO;
		spin_lock_irq(&fx2dev->dev_lock);
		fx2dev->ongoing_read = 0;
		spin_unlock_irq(&fx2dev->dev_lock);
	}

	return rv;
}

/**
 * In previous usb_skeleton.c sample, read is implemented by call synchronous
 * api - usb_bulk_msg - this really has some defects, for example:
 * - this disallows non-blocking IO by design
 * - the read would not exit if no data sent from the device before time-out and 
 *   the timeout comes out of thin air. Especially the read would block signals 
 *   for the duriation of the timeout.
 * To overcome upon shortcomings, the read mechanism was re-designed in new kenerl.
 * The new design targets to:
 * a) Supports non-blocking read IO
 * b) Supports signal interruptabled during blocking
*/
static ssize_t osrfx2_fp_read(
			struct file     *file,
			char            *buffer, 
			size_t          count, 
			loff_t          *ppos)
{
	struct osrfx2       	*fx2dev;
	int                 	rv = 0;
	bool                	ongoing_io;

	fx2dev = (struct osrfx2 *)file->private_data;
	if (!fx2dev) {
		ENTRY(NULL, "can't find device.");
		EXIT(NULL, -ENODEV, "");
		return -ENODEV;
	}
    
	ENTRY(&(fx2dev->interface->dev), "count=%d", count);

	/* if we cannot read at all, return EOF */
	if (!fx2dev->bulk_in_urb || !count) {
		goto exit; // rv = 0;
	}

	/* not allow concurrent readers */
	rv = mutex_lock_interruptible(&fx2dev->io_mutex);
	if (rv < 0)
		goto exit;

	if (!fx2dev->connected) { /* disconnect() was called */
		rv = -ENODEV;
		goto error;
	}

	/* if IO is under way, we must not touch things */
retry:
	spin_lock_irq(&fx2dev->dev_lock);
	ongoing_io = fx2dev->ongoing_read;
	spin_unlock_irq(&fx2dev->dev_lock);

	if (ongoing_io) {
		dbg_info(&(fx2dev->interface->dev), "read is on going ...\n");
			
		/* nonblocking IO shall not wait */
		if (file->f_flags & O_NONBLOCK) {
			dbg_info(&(fx2dev->interface->dev), "nonblock read, return -EAGAIN\n");
			rv = -EAGAIN;
			goto error;
		}
		/*
		 * IO may take forever
		 * hence wait in an interruptible state
		 */
		dbg_info(&(fx2dev->interface->dev), "enter sleep and waiting for read complete ...\n");
		rv = wait_event_interruptible(fx2dev->bulk_in_wait, (!fx2dev->ongoing_read));
		if (rv < 0) {
			dbg_info(&(fx2dev->interface->dev), "wake up due to signal.\n");
			goto error;
		}
		dbg_info(&(fx2dev->interface->dev), "wake up due to read complete.\n");
	}

	/* Step in here, the device should be idle and no ongoing read IO. */
	/* errors must be checked first and reported anyway */
	rv = fx2dev->errors;
	if (rv < 0) {
		/* any error is reported once */
		dbg_info(&(fx2dev->interface->dev), "any error (%d) is reported once\n", rv);
		fx2dev->errors = 0;
		/* to preserve notifications about reset (EPIPE means "halt" or "stall") */
		rv = (rv == -EPIPE) ? rv : -EIO;
		/* report it */
		goto error;
	}

	/*
	 * if the buffer is filled after ONE submision, it's driver's responsibility
	 * to copy ALL the data got by this submission before initiate next submission.
	 * 'bulk_in_filled' is defined to record the actual size of data driver 
	 * received from ONE submission, 'bulk_in_copied' is defined to record the 
	 * size of data driver has copied to userspace. Before driver completes
	 * copying ALL the data filled ( 'available' == 0 ), driver expects userspace
	 * application issue one or more read()'s to read back ALL the data filled in
	 * the buffer. Note driver can not assume the input count size of every read()
	 * from userspace caller can always hold the data we got last time from usb-core.
	 * Below is a special sample demo why from driver perspective, we have to 
	 * record how many we copied every time.

	Reader                        Driver                       USB-core   
	|                            |                               |
	+-open ( O_NONBLOCK )------->|                               |
	|                            |                               |
	+-read ( count == 512 )----->| filled = 0                    |
	|                            | copied = 0                    |
	|                            |-usb_submit_urb (512)--------->|
	|                            | return -EAGAIN                |
	|                            |                               |
	|                            |<---callback ( filled = 512 )--|
	| (Note this time count      |                               |
	|  is differnet against last |                               |
	|  time!)                    |                               |
	+-read ( count == 256 )----->| filled = 512                  |
	|                            | copied = 0                    |
	|                            | return 256, set copied = 256  |
	|                            |                               |
	+-read ( count == 256 )----->| filled = 512                  |
	|                            | copied = 256                  |
	|                            | return 256, set copied = 512  |
	|                            |                               |       
	+-read ( count == 256 )----->| filled = 512                  |
	|                            | copied = 512                  |
	|                            | Now we can issue next submission!
	|                            |-usb_submit_urb (256)--------->|
	......
	*/
	/* driver has data read/filled in the buffer */ 
	if (fx2dev->bulk_in_filled) {
		size_t available = fx2dev->bulk_in_filled - fx2dev->bulk_in_copied;
		size_t chunk = min(available, count);
		dbg_info(&(fx2dev->interface->dev),
			 "we had read data filled(%d), copied(%d), chunk(%d)\n",
			 fx2dev->bulk_in_filled, fx2dev->bulk_in_copied, chunk);

		if (!available) {
		/*
		 * if filled == copied, that means ALL data driver get by one submission
		 * has been used (copied).
		 * this means current read() has nothing to copy back to userspace
		 * and we can start a actual new IO
		 */
			dbg_info(&(fx2dev->interface->dev), "available is zero submit a new one\n");
			rv = osrfx2_fp_read_submit_urb(fx2dev, count);
			if (rv < 0) {
				goto error;
			} else {
			/* Due to read IO is executed in async, we'd better
			 * give self a chance to have a try if the usb core can
			 * read back the data before we exit this time of read(). 
			 * If retry success, we can complete in current read().
			 * Don't care this read() is blocking or nonblocking here, 
			 * the retry logic upon will handle this.
			 */
				dbg_info(&(fx2dev->interface->dev), "go to retry 1\n" );
				goto retry;
			}
		}
		/*
		 * data is available
		 * chunk tells us how much shall be copied
		 */
		if (copy_to_user(buffer,
				 fx2dev->bulk_in_buffer + fx2dev->bulk_in_copied,
				 chunk))
			rv = -EFAULT;
		else
			rv = chunk;

		fx2dev->bulk_in_copied += chunk;

		/*
		 * Till now we near completion of this read().
		 * Check more to see if we are asked for more than we have.
		 * If yes, we start an extra IO immedieately to help performance.
		 * Theoretically we need not do this but just return the actual read
		 * length to caller and leave the caller to issue another read(). But
		 * from driver perspective, we orght to try our best to expedite
		 * the read in behave of userspace app, this is plain as well.
		 * After submiting the urb, we don't wait and don't judge the return value
		 * of osrfx2_fp_read_submit_urb, bcos we are to close current read()
		 * and just return the actual data we have read to caller, that's the most
		 * important to us now.
		 */
		if (available < count) {
			dbg_info(&(fx2dev->interface->dev),
				 "we are asked for more than we have, submit a new one\n");
			osrfx2_fp_read_submit_urb(fx2dev, count - chunk);
		}
	} else {
		/* no data in the buffer */
		dbg_info(&(fx2dev->interface->dev), 
			 "no data in the buffer, submit a new one\n");
		rv = osrfx2_fp_read_submit_urb(fx2dev, count);
		if (rv < 0)
			goto error;
		else {
			/* also give self an inside retry chance */
			dbg_info(&(fx2dev->interface->dev), "go to retry 2\n" );
			goto retry;
		}
	}
error:
	mutex_unlock(&fx2dev->io_mutex);
exit:
	EXIT(&(fx2dev->interface->dev), rv, "");
	return rv;
}

static void osrfx2_fp_write_bulk_callback(struct urb *urb)
{
	struct osrfx2 *fx2dev = (struct osrfx2 *)urb->context;
	int out_urbs;
	
	if (!fx2dev) {
		ENTRY(NULL, "");
		EXIT(NULL, -1, "can't find device.");
		return;
	}

	ENTRY(&(fx2dev->interface->dev), "urb status = %d", urb->status);

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
	        if (!(urb->status == -ENOENT ||
		      urb->status == -ECONNRESET ||
		      urb->status == -ESHUTDOWN)) {
			dev_err(&(fx2dev->interface->dev),
				"%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);
		}

		spin_lock(&fx2dev->dev_lock);
		fx2dev->errors = urb->status;
		spin_unlock(&fx2dev->dev_lock);
	}
	spin_lock(&fx2dev->dev_lock);
	out_urbs = --fx2dev->write_inflight;
	spin_unlock(&fx2dev->dev_lock);
	dbg_info(&(fx2dev->interface->dev),
		"%s - inflight write urbs --, now %d\n", __func__, out_urbs );

	/* wake-up blocked poll */
        wake_up(&(fx2dev->pollq_wait));
	dbg_info(&(fx2dev->interface->dev),
		"%s - wake up poll when data is sent(ack)\n", __func__);
	
	/* free up our allocated buffer */
	usb_buffer_free(urb->dev,
			urb->transfer_buffer_length,
			urb->transfer_buffer, 
			urb->transfer_dma);
	up(&fx2dev->write_limit_sem);

	EXIT(&(fx2dev->interface->dev), fx2dev->errors, "");
}

static ssize_t osrfx2_fp_write( 
			struct file     *file, 
			const char      *user_buffer, 
			size_t          count, 
			loff_t          *ppos)
{
	struct urb          *urb            = NULL;
	char                *buf            = NULL;
	int                 retval          = 0;
	size_t              writesize       = min(count, (size_t)MAX_TRANSFER);    
	struct osrfx2       *fx2dev         = (struct osrfx2 *)file->private_data;

	if (!fx2dev) {
	        ENTRY(NULL, "");
	        EXIT(NULL, -ENODEV, "can't find device.");
	        return -ENODEV;
	}

	ENTRY(&(fx2dev->interface->dev), "count=%d\n", count);

	/* verify that we actually have some data to write */
	if (0 == count)
		goto exit;

	/**
	 * limit the number of URBs in flight to stop a user from using up all
	 * RAM
	 */
	if (!(file->f_flags & O_NONBLOCK)) {
	        if (down_interruptible(&fx2dev->write_limit_sem)) {
			retval = -ERESTARTSYS;
			goto exit;
		}
	} else {
		if (down_trylock(&fx2dev->write_limit_sem)) {
			retval = -EAGAIN;
			goto exit;
		}
	}

	spin_lock_irq(&fx2dev->dev_lock);
	retval = fx2dev->errors;
	if (retval < 0) {
		dev_err(&fx2dev->interface->dev,
			"%s - error (%d) is reported once\n",
			__func__, retval);

		/* any error is reported once */
		fx2dev->errors = 0;
		/* to preserve notifications about reset */
		retval = (retval == -EPIPE) ? retval : -EIO;
	}
	spin_unlock_irq(&fx2dev->dev_lock);
	if (retval < 0)
		goto error;

	/* Create a urb, and a buffer for it, and copy the data to the urb. */
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}

	buf = usb_buffer_alloc(fx2dev->udev,
				writesize,
				GFP_KERNEL,
				&urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}

	if (copy_from_user(buf, user_buffer, writesize)) {
		retval = -EFAULT;
		goto error;
	}

	/* this lock makes sure we don't submit URBs to gone devices */
	mutex_lock(&fx2dev->io_mutex);

	if (!fx2dev->connected) { /* disconnect() was called */
		mutex_unlock(&fx2dev->io_mutex);
		retval = -ENODEV;
		goto error;
	}

	/* initialize the urb properly */
	usb_fill_bulk_urb(urb,
			  fx2dev->udev,
			  usb_sndbulkpipe(fx2dev->udev, fx2dev->bulk_out_endpointAddr),
			  buf,
			  writesize,
			  osrfx2_fp_write_bulk_callback,
			  fx2dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	usb_anchor_urb(urb, &fx2dev->submitted);

	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);

	mutex_unlock(&fx2dev->io_mutex);

	if (retval) {
	        dev_err(&fx2dev->interface->dev,
			"%s - failed submitting write urb, error %d\n",
			__func__, retval);
		goto error_unanchor;
	} else {
		int out_urbs;
		spin_lock_irq(&fx2dev->dev_lock);
		out_urbs = ++fx2dev->write_inflight;
		spin_unlock_irq(&fx2dev->dev_lock);
		
		dbg_info(&(fx2dev->interface->dev),
			"%s - inflight write urbs ++, now %d\n", __func__, out_urbs );

		/* wake-up blocked poll */
	        wake_up(&(fx2dev->pollq_wait));
		dbg_info(&(fx2dev->interface->dev),
			"%s - wake up poll when data is sent(submit)\n", __func__);
		
	}

	/* release our reference to this urb, the USB core will eventually free it entirely */
	usb_free_urb ( urb );

	retval = writesize;
	goto exit;

error_unanchor:
	usb_unanchor_urb(urb);

error:
	if (urb) {
		usb_buffer_free(fx2dev->udev, writesize, buf, urb->transfer_dma);
		usb_free_urb(urb);
	}
	up(&fx2dev->write_limit_sem);

exit:
	EXIT(&(fx2dev->interface->dev), retval, "");
	return retval;
}

static unsigned int osrfx2_fp_poll(struct file *file, poll_table *wait)
{
	struct osrfx2	*fx2dev;
	unsigned int	mask = 0;

	fx2dev = (struct osrfx2 *)file->private_data;
	if (!fx2dev) {
		ENTRY(NULL, "can't find device.");
		EXIT(NULL, -ENODEV, "");
		return -ENODEV;
	}
    
	ENTRY(&(fx2dev->interface->dev), "");

	mutex_lock(&fx2dev->io_mutex);

	poll_wait(file, &fx2dev->pollq_wait, wait);

	if (!fx2dev->ongoing_read && fx2dev->bulk_in_filled &&
            (fx2dev->bulk_in_filled - fx2dev->bulk_in_copied)) {
            /* read is idle and have data filled needed to be copied back */
		mask |= POLLIN | POLLRDNORM;
	}
	if (fx2dev->write_inflight < WRITES_IN_FLIGHT) {
		mask |= POLLOUT | POLLWRNORM;
	}

	mutex_unlock(&fx2dev->io_mutex);

	EXIT(&(fx2dev->interface->dev), 0, "mask=0x%08x", mask);

	return mask;
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
	.owner   = THIS_MODULE,
	.open    = osrfx2_fp_open,
	.release = osrfx2_fp_release,
	.llseek  = no_llseek,
	.read    = osrfx2_fp_read,
	.write   = osrfx2_fp_write,
	.flush   = osrfx2_fp_flush,
	.poll    = osrfx2_fp_poll,
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

	/* create bargraph attribute under the sysfs directory
	 * ---  /sys/bus/usb/devices/<root_hub>-<hub>:1.0/bargraph
	 */
	device_create_file(&interface->dev, &dev_attr_bargraph);

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

	/* remove bargraph attribute from the sysfs */
	device_remove_file(&interface->dev, &dev_attr_bargraph);

	/* give back our minor */
	usb_deregister_dev(interface, &osrfx2_class);

	/* prevent more I/O from starting */
	mutex_lock(&fx2dev->io_mutex);
	fx2dev->connected = false;
	mutex_unlock(&fx2dev->io_mutex);

	usb_kill_anchored_urbs(&fx2dev->submitted);

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

