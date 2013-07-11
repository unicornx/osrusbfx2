/*
 osrfx2 - A Driver for the OSR USB FX2 Learning Kit device

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation, version 2.

 Step4, this step:
 1) How to process read/write on bulk IN/OUT endpoints.
 2) How to support poll
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <asm/uaccess.h>                            

#include "public.h"

#define OSRFX2_DEVICE_MINOR_BASE   192

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER            (PAGE_SIZE - 512)
/* MAX_TRANSFER is chosen so that the VM is not stressed by
   allocations > PAGE_SIZE and the number of packets in a page
   is an integer 512 is the largest possible packet on EHCI */
#define WRITES_IN_FLIGHT        8
/* arbitrarily chosen */

#define OSRFX2_DEBUG 1
#define DEBUG // $$$TBD temp for test
#ifdef DEBUG
static int debug = OSRFX2_DEBUG;
#define dbg_info(dev, format, arg...) do \
    { if (debug) dev_info(dev , format , ## arg); } while (0)
#else
#define dbg_info(dev, format, arg...)
#endif //DEBUG

/* 
 Table of devices that work with this driver.
 The struct usb_device_id structure is used to define a list of the different
 types of USB devices that a driver supports. 
 In most cases, drivers will create this table by using USB_DEVICE(), or 
 similar macros designed for that purpose. After defining the table, we can
 export it to userspace using MODULE_DEVICE_TABLE(), and provide it to 
 the USB core through usb_driver structure - when registering the driver.
 */
static const struct usb_device_id osrfx2_table [] = {
	{ USB_DEVICE(OSRFX2_VENDOR_ID, OSRFX2_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

/*
 MODULE_DEVICE_TABLE() creates a local variable in our .ko file 
 called __mod_usb_device_table that points to the list of struct usb_device_id. 

 In the procedure of installing the drivers - read the Makefile, 
 - first, we will put our usb driver modules file(.ko) under to
   /lib/modules/KERNEL_VERSION/kernel/drivers/usb/misc, 
 - then run depmod. This program searches all modules for the symbol 
   __mod_usb_device_table. If that symbol is found, it pulls the data out of 
   the module and adds it to the file /lib/modules/KERNEL_VERSION/modules.usbmap.
   This file records the mapping info between the driver_info (mainly the 
   driver/module name, here is osrfx2) and the device info, such as vid/pid. 
   After depmod completes, all USB devices that are supported by modules in 
   the kernel are listed, along with their module names, in that file. 

 When a device is plugged into the system, the kernel will notify the udev 
 system a new USB device has been found, plus some device info, such as vid/pid,
 and a few other bits and pieces.
 Udev then will call modprobe, and modprobe simply searches that modules.usbmap
 file to map the device info such as vid/pid back to a "real" driver name 
 - osrfx2 and load the module.
 */
MODULE_DEVICE_TABLE (usb, osrfx2_table);

/*
 This is the private device context structure to hold all of our device 
 specific stuff.
 */
struct osrfx2 {

	/* common attributes */
    struct usb_device      *udev;                   /* the usb device for this device */
    struct usb_interface   *interface;              /* the interface for this usb device
                                                      Note, for osrfx2 device, it has only
                                                      one interface.
                                                    */
	bool                   connected;               /* flag if the device is connected */                                                    
    struct kref            kref;                    /* Refrence counter for a device */
	struct mutex           io_mutex;                /* synchronize I/O with disconnect */
    struct usb_anchor      submitted;               /* in case we need to retract our submissions */
	/* error status */
    int                    errors;                  /* the last request tanked */    
    spinlock_t             err_lock;                /* lock for errors */

	/* USB Bulk IN endpoint */
    __u8                   bulk_in_endpointAddr;    /* the address of the bulk in endpoint */
    size_t                 bulk_in_size;            /* max packet size of the receive buffer */
    unsigned char          *bulk_in_buffer;         /* the buffer to receive data */
    struct urb             *bulk_in_urb;            /* the urb to read data with */
    bool                   ongoing_read;            /* flag a read is going on */
    wait_queue_head_t      bulk_in_wait;            /* to wait for an ongoing read */
    size_t                 bulk_in_filled;          /* number of bytes in the buffer */
    size_t                 bulk_in_copied;          /* already copied to user space */

	/* USB Bulk OUT endpoint */
    __u8                   bulk_out_endpointAddr;   /* the address of the bulk out endpoint */
    size_t                 bulk_out_size;           /* the size of the send buffer */
    unsigned char          *bulk_out_buffer;        /* the buffer to send data */
    struct semaphore       limit_sem;               /* limiting the number of writes in progress */
   
};

/**
 * This routine will attempt to find the required endpoints and 
 * retain relevant information in the osrfx2 context structure. 
 */
static int osrfx2_find_endpoints ( struct osrfx2 * fx2dev )
{
    struct usb_interface *interface;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;

	interface = fx2dev->interface;
	
	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;

    for ( i = 0; i < iface_desc->desc.bNumEndpoints; i++ ) {

        endpoint = &iface_desc->endpoint[i].desc;

        if ( !fx2dev->bulk_in_endpointAddr &&
		     usb_endpoint_dir_in ( endpoint ) &&
		     usb_endpoint_xfer_bulk ( endpoint ) ) {
			/* we found a bulk in endpoint */
			fx2dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			fx2dev->bulk_in_size = endpoint->wMaxPacketSize;
		}

		if ( !fx2dev->bulk_out_endpointAddr &&
		     usb_endpoint_dir_out ( endpoint ) &&
		     usb_endpoint_xfer_bulk ( endpoint ) ) {
			/* we found a bulk out endpoint */
			fx2dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
			fx2dev->bulk_out_size = endpoint->wMaxPacketSize;
		}
    }

    if ( fx2dev->bulk_in_endpointAddr  == 0 ||
         fx2dev->bulk_out_endpointAddr == 0 ) {
        dev_err ( &interface->dev, "%s - failed to find required endpoints\n", 
        	__FUNCTION__ );
        return -ENODEV;
    }
    
    return 0;
}

/**
 * constructor of osrfx2 device
 */
static int osrfx2_new ( struct osrfx2 **fx2devOut, 
                        struct usb_interface *interface )
{
	int retval = 0;
	struct osrfx2 *fx2dev = NULL;
	
	/* allocate memory for our device context and initialize it */
	fx2dev = kzalloc ( sizeof ( struct osrfx2 ), GFP_KERNEL );
	if ( !fx2dev ) {
		dev_err ( &interface->dev, "Out of memory\n" );
		retval = -ENOMEM;
		goto exit;
	}

	/* common attributes */
	kref_init ( &(fx2dev->kref) );
    mutex_init ( &(fx2dev->io_mutex) );
    init_usb_anchor(&fx2dev->submitted);
    spin_lock_init(&fx2dev->err_lock);
	/* store our device and interface pointers for later usage */
	fx2dev->udev = usb_get_dev ( interface_to_usbdev ( interface ) );
	fx2dev->interface = interface;
	fx2dev->connected = true;

	/* probe and find the endpoints we require */
	retval = osrfx2_find_endpoints ( fx2dev );
    if ( 0 != retval ) 
        goto exit;

    /* setup other attributes related to the endpoints we care */
    /* Bulk IN */
    fx2dev->bulk_in_buffer = kmalloc ( fx2dev->bulk_in_size, GFP_KERNEL );
    if ( !fx2dev->bulk_in_buffer ) {
    	dev_err( &(fx2dev->interface->dev),
    		"Could not allocate bulk_in_buffer\n" );
        retval = -ENOMEM;
        goto exit;
    }
	fx2dev->bulk_in_urb = usb_alloc_urb ( 0, GFP_KERNEL );
	if ( !fx2dev->bulk_in_urb ) {
	    dev_err( &(fx2dev->interface->dev),
            "Could not allocate bulk_in_urb\n");
	    retval = -ENOMEM;
	    goto exit;
	}
    init_waitqueue_head(&fx2dev->bulk_in_wait);
    
	/* Bulk OUT */
    fx2dev->bulk_out_buffer = kmalloc(fx2dev->bulk_out_size, GFP_KERNEL);
    if ( !fx2dev->bulk_out_buffer ) {
    	dev_err( &(fx2dev->interface->dev),
    		"Could not allocate bulk_out_buffer\n" );
        retval = -ENOMEM;
        goto exit;
    }
    sema_init(&fx2dev->limit_sem, WRITES_IN_FLIGHT);
    
exit:
	if ( retval < 0 ) {
		*fx2devOut = NULL;
		/* leave other deconstruct in osrfx2_delete() */
	}
	else {
		*fx2devOut = fx2dev;
	}
    return retval;
}

/**
 * de-constructor of the osrfx2 device
 */
static void osrfx2_delete ( struct kref *kref )
{	
	/* use macro container_of to find the appropriate device structure */
	struct osrfx2 *fx2dev = container_of ( kref, struct osrfx2, kref );

    dev_info ( &(fx2dev->interface->dev), "--> osrfx2_delete\n" );
		
    /* release a use of the usb device structure */
	usb_put_dev ( fx2dev->udev );

	/* free device context memory */
	/* free Bulk IN memory */
    if ( fx2dev->bulk_in_buffer ) {
        kfree( fx2dev->bulk_in_buffer );
    }
    if ( fx2dev->bulk_in_urb ) {
    	usb_free_urb ( fx2dev->bulk_in_urb );
    }
    /* free Bulk OUT memory */
    if ( fx2dev->bulk_out_buffer ) {
        kfree( fx2dev->bulk_out_buffer );
    }

    /* free device context memory */
	kfree ( fx2dev );
}

static void osrfx2_draw_down ( struct osrfx2 *fx2dev )
{
    int time;

    time = usb_wait_anchor_empty_timeout ( &fx2dev->submitted, 1000 );
    if ( !time )
            usb_kill_anchored_urbs ( &fx2dev->submitted );
//	dbg_info ( &(fx2dev->interface->dev), "usb_kill_urb\n" );
    usb_kill_urb ( fx2dev->bulk_in_urb );
}

/**
 * We choose use sysfs attribte to read/write io data. More details please 
 * refer to http://www.ibm.com/developerworks/cn/linux/l-cn-sysfs/#resources
 * 
 * This routine will retrieve the bargraph LED state, format it and return a
 * representative string.                                                   
 *                                                                         
 * Note the two different function defintions depending on kernel version.  
 */
static ssize_t osrfx2_attr_bargraph_get (
	struct device * dev, 
    struct device_attribute * attr, 
    char * buf )
{
    struct usb_interface  * intf   = to_usb_interface(dev);
    struct osrfx2         * fx2dev = usb_get_intfdata(intf);
    struct bargraph_state * packet;
    int retval;

	dev_info ( dev, "--> osrfx2_attr_bargraph_get\n" );
		
    packet = kzalloc ( sizeof(*packet), GFP_KERNEL );
    if ( !packet ) {
        return -ENOMEM;
    }

    retval = usb_control_msg ( fx2dev->udev, 
                               usb_rcvctrlpipe ( fx2dev->udev, 0 ), 
                               OSRFX2_READ_BARGRAPH_DISPLAY, 
                               USB_DIR_IN | USB_TYPE_VENDOR,
                               0,
                               0,
                               packet, 
                               sizeof(*packet),
                               USB_CTRL_GET_TIMEOUT );

    if ( retval < 0 ) {
        dev_err ( dev, "%s - retval=%d\n", 
                __FUNCTION__, retval );
        kfree ( packet );
        return retval;
    }

    retval = sprintf( buf, "%s%s%s%s%s%s%s%s",    /* bottom LED --> top LED */
                      (packet->Bar1) ? "*" : ".",
                      (packet->Bar2) ? "*" : ".",
                      (packet->Bar3) ? "*" : ".",
                      (packet->Bar4) ? "*" : ".",
                      (packet->Bar5) ? "*" : ".",
                      (packet->Bar6) ? "*" : ".",
                      (packet->Bar7) ? "*" : ".",
                      (packet->Bar8) ? "*" : "." );

    kfree ( packet );

    return retval;
}

/*
 This routine will set the bargraph LEDs.

 Note the two different function defintions depending on kernel version.
 */
static ssize_t osrfx2_attr_bargraph_set (
	struct device * dev, 
    struct device_attribute * attr, 
    const char * buf,
    size_t count )
{
    struct usb_interface  * intf   = to_usb_interface(dev);
    struct osrfx2         * fx2dev = usb_get_intfdata(intf);
    struct bargraph_state * packet;

    int retval;
    char * end;

   	dev_info ( dev, "--> osrfx2_attr_bargraph_set\n" );

    packet = kzalloc ( sizeof(*packet), GFP_KERNEL );
    if ( !packet ) {
        return -ENOMEM;
    }

	packet->BarsOctet = (unsigned char)(simple_strtoul(buf, &end, 10) & 0xFF);
    if ( buf == end ) {
        packet->BarsOctet = 0;
    }

    retval = usb_control_msg ( fx2dev->udev, 
                               usb_sndctrlpipe(fx2dev->udev, 0), 
                               OSRFX2_SET_BARGRAPH_DISPLAY, 
                               USB_DIR_OUT | USB_TYPE_VENDOR,
                               0,
                               0,
                               packet, 
                               sizeof(*packet),
                               USB_CTRL_GET_TIMEOUT );

    if ( retval < 0 ) {
        dev_err ( dev, "%s - retval=%d\n", 
                __FUNCTION__, retval );
    }
    
    kfree ( packet );

    return count;
}

/*
 This macro DEVICE_ATTR creates an attribute under the sysfs directory
 ---  /sys/bus/usb/devices/<root_hub>-<hub>:1.0/bargraph

 The DEVICE_ATTR() will create "dev_attr_bargraph".
 "dev_attr_bargraph" is referenced in both probe (create the attribute) and 
 disconnect (remove the attribute) routines.
 */
static DEVICE_ATTR ( bargraph, \
                     S_IRUGO | S_IWUGO, \
                     osrfx2_attr_bargraph_get, \
                     osrfx2_attr_bargraph_set );

static struct usb_driver osrfx2_driver;

/*
 The open method is always the first operation performed on the device file.
 It is provided for a driver to do any initialization in preparation for 
 later operations. In most drivers, open should perform the following tasks:
 1) Check for device-specific errors (such as device-not-ready or similar 
    hardware problems).
 2) Initialize the device if it is being opened for the first time
 3) Update the f_op pointer, if necessary
 4) Allocate and fill any data structure to be put in filp->private_data.
    Driver can store device context content, such as device handler,
    interface handler etc. in the private_data. We can treat the private_data as
    the bridge between file structure and lower level device object. More than 
    one application processes have their own file descriptor opened for a device.
    After driver store device context data in the open method for this device, 
    other file operation methods can get access to these data easily later.
 */
static int osrfx2_fp_open ( struct inode *inode, struct file *file )
{
	struct osrfx2 *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);
	interface = usb_find_interface ( &osrfx2_driver, subminor );
	if ( !interface ) {
		pr_err ( "--> osrfx2_fp_open error, can't find device for minor %d\n", subminor );
		pr_err ( "<-- osrfx2_fp_open, return (%d)\n", -ENODEV );
		return -ENODEV;
	}
	dev_info ( &interface->dev, "--> osrfx2_fp_open for minor %d\n", subminor );

	dev = usb_get_intfdata ( interface );
	if ( !dev ) {
		retval = -ENODEV;
		goto exit;
	}

	/* Set this device as non-seekable bcos it does not make sense for osrfx2. 
	   Refer to http://lwn.net/Articles/97154/ "Safe seeks" and ldd3 section 6.5.
	*/ 
    retval = nonseekable_open(inode, file);
    if ( 0 != retval ) {
		goto exit;
    }
    
	/* increment our usage count for the device */
	kref_get( &dev->kref );

	/* save our object in the file's private structure */
	file->private_data = dev;

exit:
	dev_info ( &interface->dev, "<-- osrfx2_fp_open, return (%d)\n", retval );
	return retval;
}

/*
 The release operation is invoked when the file structure is being released.
 The role of the release method is the reverse of open. Normally it should 
 perform the following tasks:
 1) Deallocate anything that open allocated in filp->private_data.
 2) Shut down the device on last close.
 */
static int osrfx2_fp_release ( struct inode *inode, struct file *file )
{
	struct osrfx2 *fx2dev;

	fx2dev = (struct osrfx2 *)file->private_data;
	if ( NULL == fx2dev ) {
		pr_err ( "--> osrfx2_fp_release, but get device failed!\n" );
		return -ENODEV;
	}
	dev_info ( &(fx2dev->interface->dev), "--> osrfx2_fp_release for minor %d\n", iminor(inode) );
	
	/* decrement the count on our device */
	kref_put ( &fx2dev->kref, osrfx2_delete );

	dev_info ( &(fx2dev->interface->dev), "<-- osrfx2_fp_release, return 0\n" );
	return 0;
}

static int osrfx2_fp_flush ( struct file *file, fl_owner_t id )
{
    struct osrfx2 *fx2dev;
    int rv;

    fx2dev = file->private_data;
    if ( NULL == fx2dev ) {
    	pr_err ( "--> osrfx2_fp_flush, but get device failed!\n" );
		pr_err ( "<-- osrfx2_fp_flush, return (%d)\n", -ENODEV );
        return -ENODEV;
    }
	dev_info ( &(fx2dev->interface->dev), "--> osrfx2_fp_flush\n" );

    mutex_lock ( &fx2dev->io_mutex );

    /* wait for io to stop */
    osrfx2_draw_down ( fx2dev );

    /* read out errors, leave subsequent opens a clean slate */
    spin_lock_irq ( &fx2dev->err_lock );
    rv = fx2dev->errors ? ( fx2dev->errors == -EPIPE ? -EPIPE : -EIO ) : 0;
    fx2dev->errors = 0;
    spin_unlock_irq ( &fx2dev->err_lock );

    mutex_unlock ( &fx2dev->io_mutex );
    
    dev_info ( &(fx2dev->interface->dev), "<-- osrfx2_fp_flush, return (%d)\n", rv );
    return rv;
}

static void osrfx2_fp_read_bulk_callback ( struct urb *urb )
{
    struct osrfx2 *fx2dev = urb->context;

    if ( !fx2dev ) {
   		pr_err ( "--> osrfx2_fp_read_bulk_callback error, can't find device.\n" );
		return;
    }
	dev_info ( &(fx2dev->interface->dev), 
		"--> osrfx2_fp_read_bulk_callback (urb status=%d, actual length = %d)\n", 
		urb->status, urb->actual_length );

    spin_lock ( &fx2dev->err_lock );

    /* sync/async unlink faults aren't errors */
    if ( urb->status ) {
        if ( ! ( urb->status == -ENOENT ||
	             urb->status == -ECONNRESET ||
	             urb->status == -ESHUTDOWN ) ) {
            dev_err ( &(fx2dev->interface->dev),
	                  "%s - nonzero read bulk status received: %d\n",
	                    __func__, urb->status );
        }

        fx2dev->errors = urb->status;
    } 
    else {
        fx2dev->bulk_in_filled = urb->actual_length;
    }
    fx2dev->ongoing_read = 0;

    spin_unlock ( &fx2dev->err_lock );

    wake_up_interruptible ( &fx2dev->bulk_in_wait );
}

static int osrfx2_fp_read_submit_urb ( struct osrfx2 *fx2dev, size_t count )
{
    int rv;

    /* prepare a read URB */
    usb_fill_bulk_urb ( fx2dev->bulk_in_urb,
                        fx2dev->udev,
                        usb_rcvbulkpipe ( fx2dev->udev,
                            fx2dev->bulk_in_endpointAddr ),
                        fx2dev->bulk_in_buffer,
                        min(fx2dev->bulk_in_size, count),
                        osrfx2_fp_read_bulk_callback,
                        fx2dev );
    /* tell everybody to leave the URB alone */
    spin_lock_irq (&fx2dev->err_lock );
    fx2dev->ongoing_read = 1;
    spin_unlock_irq ( &fx2dev->err_lock );

    /* submit bulk in urb, which means no data to deliver */
    fx2dev->bulk_in_filled = 0;
    fx2dev->bulk_in_copied = 0;

    /* submit it */
    rv = usb_submit_urb ( fx2dev->bulk_in_urb, GFP_KERNEL );
    if ( rv < 0 ) {
        dev_err ( &fx2dev->interface->dev,
                  "%s - failed submitting read urb, error %d\n",
                  __func__, rv );
        rv = (rv == -ENOMEM) ? rv : -EIO;
        spin_lock_irq ( &fx2dev->err_lock );
        fx2dev->ongoing_read = 0;
        spin_unlock_irq ( &fx2dev->err_lock );
    }

    return rv;
}

static ssize_t osrfx2_fp_read ( 
	struct file * file, 
	char * buffer, 
    size_t count, 
    loff_t * ppos )
{
    struct osrfx2 * fx2dev;
    int rv = 0;
    bool ongoing_io;

	fx2dev = ( struct osrfx2* ) file->private_data;
    if ( !fx2dev ) {
   		pr_err ( "--> osrfx2_fp_read, can't find device.\n" );
   		pr_err ( "<-- osrfx2_fp_read, return (%d)\n", -ENODEV );
		return -ENODEV;
    }
	dev_info ( &(fx2dev->interface->dev), "--> osrfx2_fp_read, count=%d\n", count );

    /* if we cannot read at all, return EOF */
    if ( !fx2dev->bulk_in_urb || !count )
	    return 0;

    /* no concurrent readers */
    rv = mutex_lock_interruptible ( &fx2dev->io_mutex );
    if ( rv < 0 )
        goto quick_exit;

    if ( !fx2dev->connected ) { /* disconnect() was called */
        rv = -ENODEV;
        goto exit;
    }

    /* if IO is under way, we must not touch things */
retry:
    spin_lock_irq ( &fx2dev->err_lock );
    ongoing_io = fx2dev->ongoing_read;
    spin_unlock_irq ( &fx2dev->err_lock );

    if ( ongoing_io ) {
    	dbg_info ( &(fx2dev->interface->dev), "read is on going ...\n" );
    		
        /* nonblocking IO shall not wait */
        if ( file->f_flags & O_NONBLOCK ) {
        	dbg_info ( &(fx2dev->interface->dev), "nonblock read, return -EAGAIN\n" );
            rv = -EAGAIN;
            goto exit;
	    }
        /*
         * IO may take forever
         * hence wait in an interruptible state
         */
    	dbg_info ( &(fx2dev->interface->dev), "waiting for read on going ...\n" );
        rv = wait_event_interruptible ( fx2dev->bulk_in_wait, (!fx2dev->ongoing_read) );
       	dbg_info ( &(fx2dev->interface->dev), "wait complete\n" );
        if ( rv < 0 )
            goto exit;
    }

	/* errors must be reported */
	rv = fx2dev->errors;
	if ( rv < 0 ) {
        /* any error is reported once */
        dbg_info ( &(fx2dev->interface->dev), "any error (%d) is reported once\n", rv );
        fx2dev->errors = 0;
        /* to preserve notifications about reset */
        rv = (rv == -EPIPE) ? rv : -EIO;
        /* report it */
        goto exit;
	}

    /*
     * if the buffer is filled we may satisfy the read
     * else we need to start IO
     */
    if ( fx2dev->bulk_in_filled ) {
        /* we had read data */
        size_t available = fx2dev->bulk_in_filled - fx2dev->bulk_in_copied;
        size_t chunk = min ( available, count );
		dbg_info ( &(fx2dev->interface->dev), "we had read data filled(%d), copied(%d), chunk(%d)\n", 
			fx2dev->bulk_in_filled, fx2dev->bulk_in_copied, chunk );
		
        if ( !available ) {
            /*
             * all data has been used
             * actual IO needs to be done
             */
       		dbg_info ( &(fx2dev->interface->dev), "available is zero submit a new one\n" );
            rv = osrfx2_fp_read_submit_urb ( fx2dev, count );
            if ( rv < 0 ) {
            	goto exit;
            }
            else {
   	       		dbg_info ( &(fx2dev->interface->dev), "go to retry 1\n" );
                goto retry;
            }
    	}
	    /*
	     * data is available
	     * chunk tells us how much shall be copied
	     */

	    if ( copy_to_user ( buffer,
			                fx2dev->bulk_in_buffer + fx2dev->bulk_in_copied,
                            chunk ) )
            rv = -EFAULT;
	    else
            rv = chunk;

    	fx2dev->bulk_in_copied += chunk;

	    /*
	     * if we are asked for more than we have,
	     * we start IO but don't wait
	     */
	    if ( available < count ) {
       		dbg_info ( &(fx2dev->interface->dev), "we are asked for more than we have, submit a new one\n" );
	        osrfx2_fp_read_submit_urb ( fx2dev, count - chunk );
	    }
    }
    else {
        /* no data in the buffer */
   		dbg_info ( &(fx2dev->interface->dev), "no data in the buffer, submit a new one\n" );
        rv = osrfx2_fp_read_submit_urb ( fx2dev, count );
        if ( rv < 0 )
            goto exit;
        else if ( !(file->f_flags & O_NONBLOCK) ) {
       		dbg_info ( &(fx2dev->interface->dev), "it's a blocking read, goto retry 2\n" );
            goto retry;
        }
   		dbg_info ( &(fx2dev->interface->dev), "it's non-blocking read\n" );
        rv = -EAGAIN;
    }
exit:
    mutex_unlock ( &fx2dev->io_mutex );
quick_exit:
	dev_info ( &(fx2dev->interface->dev), "<-- osrfx2_fp_read, return (%d)\n", rv );
	return rv;
}

static void osrfx2_fp_write_bulk_callback ( struct urb *urb )
{
    struct osrfx2 * fx2dev = (struct osrfx2 *)urb->context;

    if ( !fx2dev ) {
   		pr_err ( "--> osrfx2_fp_write_bulk_callback error, can't find device.\n" );
		return;
    }
	dev_info ( &(fx2dev->interface->dev), 
		"--> osrfx2_fp_write_bulk_callback (urb status = %d)\n", urb->status );

	/* sync/async unlink faults aren't errors */
    if ( urb->status ) {
        if ( ! ( urb->status == -ENOENT ||
	             urb->status == -ECONNRESET ||
	             urb->status == -ESHUTDOWN ) ) {
            dev_err ( &(fx2dev->interface->dev),
	                  "%s - nonzero write bulk status received: %d\n",
	                    __func__, urb->status );
        }

		spin_lock ( &fx2dev->err_lock );
		fx2dev->errors = urb->status;
		spin_unlock ( &fx2dev->err_lock );
    }

   	/* free up our allocated buffer */
   	usb_buffer_free( urb->dev, 
   		             urb->transfer_buffer_length,
                     urb->transfer_buffer, 
                     urb->transfer_dma );
    up ( &fx2dev->limit_sem );
}

static ssize_t osrfx2_fp_write ( 
	struct file * file, 
	const char * user_buffer, 
	size_t count, 
	loff_t * ppos )
{
    struct osrfx2 * fx2dev;
    struct urb * urb = NULL;
    char * buf = NULL;
    int retval = 0;
    size_t writesize = min(count, (size_t)MAX_TRANSFER);    

    fx2dev = (struct osrfx2 *)file->private_data;
    if ( !fx2dev ) {
   		pr_err ( "--> osrfx2_fp_write error, can't find device.\n" );
   		pr_err ( "<-- osrfx2_fp_write, return (%d)\n", -ENODEV );
		return -ENODEV;
    }
	dev_info ( &(fx2dev->interface->dev), "--> osrfx2_fp_write, count=%d\n", count );

  	/* verify that we actually have some data to write */
    if ( 0 == count )
        goto exit;

    /*
     * limit the number of URBs in flight to stop a user from using up all
     * RAM
     */
    if ( !(file->f_flags & O_NONBLOCK) ) {
        if ( down_interruptible ( &fx2dev->limit_sem ) ) {
            retval = -ERESTARTSYS;
            goto exit;
        }
    }
    else {
        if ( down_trylock ( &fx2dev->limit_sem ) ) {
            retval = -EAGAIN;
            goto exit;
        }
    }
    
    spin_lock_irq ( &fx2dev->err_lock );
    retval = fx2dev->errors;
    if ( retval < 0 ) {
        /* any error is reported once */
        fx2dev->errors = 0;
        /* to preserve notifications about reset */
        retval = (retval == -EPIPE) ? retval : -EIO;
    }
    spin_unlock_irq ( &fx2dev->err_lock );
    if ( retval < 0 )
        goto error;

    /* Create a urb, and a buffer for it, and copy the data to the urb. */
    urb = usb_alloc_urb ( 0, GFP_KERNEL );
    if ( !urb ) {
        retval = -ENOMEM;
        goto error;
    }

    buf = usb_buffer_alloc ( fx2dev->udev, 
                             writesize, 
                             GFP_KERNEL, 
                             &urb->transfer_dma );
    if ( !buf ) {
        retval = -ENOMEM;
        goto error;
    }

    if ( copy_from_user ( buf, user_buffer, writesize ) ) {
        retval = -EFAULT;
        goto error;
    }

    /* this lock makes sure we don't submit URBs to gone devices */
    mutex_lock ( &fx2dev->io_mutex );

    if ( !fx2dev->connected ) { /* disconnect() was called */
        mutex_unlock ( &fx2dev->io_mutex );
        retval = -ENODEV;
        goto error;
    }

	/* initialize the urb properly */
    usb_fill_bulk_urb( urb, 
                       fx2dev->udev,
                       usb_sndbulkpipe ( fx2dev->udev, fx2dev->bulk_out_endpointAddr ),
                       buf, 
                       writesize, 
                       osrfx2_fp_write_bulk_callback, 
                       fx2dev );
    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	usb_anchor_urb ( urb, &fx2dev->submitted );
	
	/* send the data out the bulk port */
	retval = usb_submit_urb ( urb, GFP_KERNEL );

	mutex_unlock ( &fx2dev->io_mutex );
	
    if ( retval ) {
        dev_err ( &fx2dev->interface->dev, 
        	"%s - failed submitting write urb, error %d\n",
        	__func__, retval );
        goto error_unanchor;
    }

	/* release our reference to this urb, the USB core will eventually free it entirely */
	usb_free_urb ( urb );

    retval = writesize;
    goto exit;

error_unanchor:
    usb_unanchor_urb(urb);

error:
	if ( urb ) {
	    usb_buffer_free ( fx2dev->udev, writesize, buf, urb->transfer_dma );
    	usb_free_urb ( urb );
	}
    up ( &fx2dev->limit_sem );
    
exit:
	dev_info ( &(fx2dev->interface->dev), "<-- osrfx2_fp_write, return (%d)\n", retval );
    return retval;
}

#if 0
static unsigned int osrfx2_fp_poll ( struct file * file, poll_table * wait )
{
    struct osrfx2 * fx2dev = (struct osrfx2 *)file->private_data;
    unsigned int mask = 0;
    int retval = 0;

//    poll_wait(file, &fx2dev->FieldEventQueue, wait);


//    up( &fx2dev->sem );
    
    return mask;
}
#endif 

/*
 In UNIX/LINUX world, from applicaiton perspective, eveything, including device,
 is a file. To communicate a device, applicaiton will open the file which
 represnet the device and call other file operaiton functions. After we 
 register the device driver and get a minor number for one
 device connected, we need to connect driver's operations to those numbers. 
 The file_operations structure is how a usb/char driver sets up this connection. 
 The structure, is a collection of function pointers. Each open file 
 (represented internally by a file structure) is associated with its own set of 
 functions. Application can invoke them through kernel on a device.
 */
static struct file_operations osrfx2_fops = {
	.owner   = THIS_MODULE,
	.open    = osrfx2_fp_open,
	.release = osrfx2_fp_release,
	.llseek  = no_llseek,
	.read    = osrfx2_fp_read,
	.write   = osrfx2_fp_write,
	.flush   = osrfx2_fp_flush,
//	.poll    = osrfx2_fp_poll,
};

/* 
 usb class driver info in order to get a minor number from the usb core,
 and to have the device registered with sysfs and the driver core
 Note, since 2.6.17, devfs was removed, so we need not care the devfs 
 related stuff for such as .name and .mode.
 */
static struct usb_class_driver osrfx2_class = {

	.name = "osrfx2_%d",  /* The name that sysfs uses to describe the device.*/
	.fops = &osrfx2_fops, /* Pointer to the struct file_operations that this 
	                         driver has defined to use to register as the 
	                         character device.
	                       */
	.minor_base = 
	  OSRFX2_DEVICE_MINOR_BASE, /* Start of the assigned minor range for this 
	                               driver. This member is ignored when 
	                               CONFIG_USB_DYNAMIC_MINORS configuration
                                   option has been enabled for the kernel.
                                   Enable the CONFIG_USB_DYNAMIC_MINORS will 
                                   make all minor numbers for the device are 
                                   allocated on a first-come, first-served manner.
                                   It is recommended that systems that have 
                                   enabled this option use a program such as 
                                   udev to manage the device nodes in the system,
                                   as a static /dev tree will not work properly
                                 */
};

/* 
 The probe function is called when a device is installed that the USB core 
 thinks this driver should handle; the probe function should perform checks 
 on the information passed to it about the device and decide whether the 
 driver is really appropriate for that device.
 */
static int osrfx2_drv_probe ( 
	struct usb_interface *interface, 
	const struct usb_device_id *id )
{
	int retval = -ENOMEM;
	struct osrfx2 *fx2dev = NULL;
		
	dev_info ( &interface->dev, "--> osrfx2_drv_probe\n" );

    retval = osrfx2_new ( &fx2dev, interface );
    if ( 0 != retval ) 
        goto error;
//    assert ( fx2dev );

	/* USB driver needs to retrieve the local device context data structure 
	   that is associated with this struct usb_interface later in the 
	   lifecycle of the device, so we have to save our dev data pointer 
	   in this interface device. We can use usb_set_intfdata to save the data
	   and use usb_get_intfdata to retrieve it in other methods later 

       Note: compare this store/retrieve with that in open method, the save of
       device context data in open method is on file.private_data, that's for
       remember device info for file operation scope, but here is inside 
       device/driver scope. Don't confuse these two.
	*/
	usb_set_intfdata ( interface, fx2dev );

    /* create bargraph attribute under the sysfs directory
       ---  /sys/bus/usb/devices/<root_hub>-<hub>:1.0/bargraph
    */
    device_create_file(&interface->dev, &dev_attr_bargraph);

	/* we can register the device now, as it is ready 

	   On Linux, there are two reserved major ids for USB, (include/linux/usb.h)
	   #define USB_MAJOR                       180
       #define USB_DEVICE_MAJOR                189
       Only if the driver for a USB device is not registered with other 
       subsystem, for example input, vedio and etc, USB_MAJOR is allocated 
       for usb_interface, and USB_DEVICE_MAJOR is allocated for usb_device.
       We can check the device file in /dev to have deeper understanding on this.
       For example, after one osrfx2 is connected, we can see two important
       file nodes are created under /dev.
       (1) crw-rw-r-- 1 root root 189, 4 2013-05-31 11:28 /dev/bus/usb/001/005
	   (2) crw-rw---- 1 root root 180, 192 2013-05-31 11:47 /dev/osrf2x2_0
	   The first one is created for the usb device and it will help sysfs to 
	   create proper file nodes. The Second one is the actual device file created
	   by udev for userspace applicaiton to access.
	   Due to in Linux, every usb driver serves a usb interface. So you can see
	   the first input param type of usb_register_dev is usb_interface. That's
	   why the /dev/osrfx2_0 is allocated with a major id = 180 (USB_MAJOR)
	   plus a minor id started from 192 (OSRFX2_DEVICE_MINOR_BASE).
	*/
	retval = usb_register_dev ( interface, &osrfx2_class );
	if ( retval ) {
		/* something prevented us from registering this driver */
		dev_err ( &interface->dev,
		          "Not able to register and get a minor for this device.\n" );
		usb_set_intfdata ( interface, NULL );
		goto error;
	}

	/* let the user know what node this device is now attached to */
	dev_info ( &interface->dev,
	           "OSRFX2 device now attached to osrfx2_%d", interface->minor );
	return 0;

error:
	if ( fx2dev )
		/* this frees allocated memory */
		kref_put ( &fx2dev->kref, osrfx2_delete );
	return retval;
}

/*
 The disconnect function is called when the driver should no longer control 
 the device for some reason and can do clean-up.

 If you read the ldd3 book sample, the skel_disconnect() used 
 lock_kernel/unlock_kernel to prevent xxx_open() from racing xxx_disconnect().
 Since BKL was removed since 2.6.39 ( @http://kernelnewbies.org/BigKernelLock ),
 to catch up the usb_skeleton.c since 2.6.32, we now use a mutex instead.
 */
static void osrfx2_drv_disconnect ( struct usb_interface *interface )
{
	struct osrfx2 *fx2dev;
	int minor = interface->minor;

	dev_info ( &interface->dev, "--> osrfx2_drv_disconnect\n" );

	fx2dev = usb_get_intfdata ( interface );
	usb_set_intfdata ( interface, NULL );

	/* remove bargraph attribute from the sysfs */
    device_remove_file ( &interface->dev, &dev_attr_bargraph );
    
	/* give back our minor */
	usb_deregister_dev ( interface, &osrfx2_class );

    /* prevent more I/O from starting */
    mutex_lock ( &fx2dev->io_mutex );
    fx2dev->connected = false;
    mutex_unlock ( &fx2dev->io_mutex );

    usb_kill_anchored_urbs ( &fx2dev->submitted );

	/* decrement our usage count */
	kref_put ( &fx2dev->kref, osrfx2_delete );

	dev_info ( &interface->dev, "osrfx2_%d now disconnected\n", minor );
}

/*
 The main structure that all USB drivers must create is a struct usb_driver. 
 This structure must be filled out by the USB driver and consists of a number 
 of function callbacks and variables that describe the USB driver to the 
 USB core.
 To register the struct usb_driver with the USB core, a call to 
 usb_register_driver should be made with a pointer to the struct usb_driver. 
 This is traditionally done in the module initialization code for the USB 
 driver, here it is osrfx2_int.
 When the USB driver is to be unloaded (rmmod), the struct usb_driver needs to 
 be unregistered from the kernel. This is done with a call to 
 usb_deregister_driver in module exiting code, here it is osrfx2_exit. When 
 this call happens, any USB interfaces that were currently bound to this driver
 are disconnected, and the disconnect function (osr_disconnect) will be called 
 for them. Note the disconnect function will be triggerred in two cases, one is
 the case I just described upon, another happens when the device is removed from
 the system, while for the second case, normally the driver module has not been
 unloaded and exit function also is not called till we issue rmmod command.
*/
static struct usb_driver osrfx2_driver = {
    .name        = "osrfx2",
    .probe       = osrfx2_drv_probe,
    .disconnect  = osrfx2_drv_disconnect,
    .id_table    = osrfx2_table,
};

/*
 This driver's commission routine, called when the driver module is installed       
 */
static int __init osrfx2_init(void)
{
	int result;

	pr_info ( "--> osrfx2_init\n" );

	/* register this driver with the USB subsystem */
	result = usb_register(&osrfx2_driver);
	if (result)
		pr_err ( "usb_register failed. Error number %d\n", result );

	return result;
}


/*
 This driver's decommission routine, called when the driver module is removed
 */
static void __exit osrfx2_exit(void)
{
	pr_info ( "--> osrfx2_exit\n" );
	
	/* deregister this driver with the USB subsystem */
	usb_deregister ( &osrfx2_driver );
}

/*****************************************************************************/
/* Advertise this driver's init and exit routines                            */
/*****************************************************************************/
module_init( osrfx2_init );
module_exit( osrfx2_exit );

MODULE_AUTHOR("unicornx");
MODULE_DESCRIPTION("A driver for the OSR USB-FX2 Learning Kit device");
MODULE_LICENSE("GPL");

