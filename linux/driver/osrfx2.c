/*****************************************************************************/
/* osrfx2.c    A Driver for the OSR USB FX2 Learning Kit device              */
/*                                                                           */
/* Copyright (C) 2006 by Robin Callender                                     */
/*                                                                           */
/* This program is free software. You can redistribute it and/or             */
/* modify it under the terms of the GNU General Public License as            */
/* published by the Free Software Foundation, version 2.                     */
/*                                                                           */
/*****************************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <linux/usb.h>

/*****************************************************************************/
/* Define the vendor id and product id.                                      */
/*****************************************************************************/
#define VENDOR_ID   0x0547       
#define PRODUCT_ID  0x1002

#define DEVICE_MINOR_BASE   192
             
#undef TRUE
#define TRUE  (1)
#undef FALSE
#define FALSE (0)

/*****************************************************************************/
/* Define the vendor commands supported by OSR USB FX2 device.               */
/*****************************************************************************/
#define OSRFX2_READ_7SEGMENT_DISPLAY      0xD4
#define OSRFX2_READ_SWITCHES              0xD6
#define OSRFX2_READ_BARGRAPH_DISPLAY      0xD7
#define OSRFX2_SET_BARGRAPH_DISPLAY       0xD8
#define OSRFX2_IS_HIGH_SPEED              0xD9
#define OSRFX2_REENUMERATE                0xDA
#define OSRFX2_SET_7SEGMENT_DISPLAY       0xDB
              
/*****************************************************************************/
/* BARGRAPH_STATE is a bit field structure with each bit corresponding       */
/* to one on the bars on the bargraph LED of the OSR USB FX2 Learner Kit     */
/* development board.                                                        */
/*****************************************************************************/
struct bargraph_state {
    union {
        struct {
            /*
             * Individual bars (LEDs) starting from the top of the display.
             *
             * NOTE: The display has 10 LEDs, but the top two LEDs are not
             *       connected (don't light) and are not included here. 
             */
            unsigned char Bar4 : 1;
            unsigned char Bar5 : 1;
            unsigned char Bar6 : 1;
            unsigned char Bar7 : 1;
            unsigned char Bar8 : 1;
            unsigned char Bar1 : 1;
            unsigned char Bar2 : 1;
            unsigned char Bar3 : 1;
        };
        /*
         *  The state of all eight bars as a single octet.
         */
        unsigned char BarsOctet;
    };
} __attribute__ ((packed));

/*****************************************************************************/
/* SEGMENT_STATE is a bit field structure with each bit corresponding        */
/* to one on the segments on the 7-segment display of the OSR USB FX2        */
/* Learner Kit development board.                                            */
/*****************************************************************************/
struct segment_state {
    union {
        struct {
            /*
             * Individual segments.
             */
            unsigned char Segment_top         : 1;   /* 0x01 */
            unsigned char Segment_upper_right : 1;   /* 0x02 */
            unsigned char Segment_lower_right : 1;   /* 0x04 */
            unsigned char Segment_dot         : 1;   /* 0x08 */
            unsigned char Segment_lower_left  : 1;   /* 0x10 */
            unsigned char Segment_middle      : 1;   /* 0x20 */
            unsigned char Segment_upper_left  : 1;   /* 0x40 */
            unsigned char Segment_bottom      : 1;   /* 0x80 */
        };
        /*
         *  The state of all the segments as a single octet.
         */
        unsigned char SegmentsOctet;
    };
} __attribute__ ((packed));

static const unsigned char digit_to_segments [10] = { 
    0xD7,  /* 0 */
    0x06,  /* 1 */
    0xB3,  /* 2 */
    0xA7,  /* 3 */
    0x66,  /* 4 */
    0xE5,  /* 5 */
    0xF5,  /* 6 */
    0x07,  /* 7 */
    0xF7,  /* 8 */
    0x67   /* 9 */
};

static const unsigned char nondisplayable = 0x08;   /* the "dot" segment */
static const unsigned char high_speed     = 0x76;   /* high-speed "H" */
static const unsigned char power_active   = 0x77;   /* PM active "A" */
static const unsigned char power_suspend  = 0xE5;   /* PM suspend "S" */

/*****************************************************************************/
/* SWITCHES_STATE is a bit field structure with each bit corresponding       */
/* to one of the DIP switches on the OSR USB FX2 Learner Kit development     */
/* board.                                                                    */
/*****************************************************************************/
struct switches_state {
    union {
        struct {
            /*
             * Individual switches starting from the left
             */
            unsigned char Switch8 : 1;
            unsigned char Switch7 : 1;
            unsigned char Switch6 : 1;
            unsigned char Switch5 : 1;
            unsigned char Switch4 : 1;
            unsigned char Switch3 : 1;
            unsigned char Switch2 : 1;
            unsigned char Switch1 : 1;
        };
        /*
         *  The state of all the switches as a single octet.
         */
        unsigned char SwitchesOctet;
    };
} __attribute__ ((packed));

/*****************************************************************************/
/* Table of devices that work with this driver                               */
/*****************************************************************************/
static struct usb_device_id id_table [] = {
    { USB_DEVICE( VENDOR_ID, PRODUCT_ID ) },
    { },
};

MODULE_DEVICE_TABLE(usb, id_table);

/*****************************************************************************/
/* This is the private device context structure.                             */
/*****************************************************************************/
struct osrfx2 {

    struct usb_device    * udev;
    struct usb_interface * interface;
    
    /*
     * This queue is used by the poll and irq methods
     */
    wait_queue_head_t FieldEventQueue;
    
    /*
     *  Transfer Buffers
     */
    unsigned char * int_in_buffer;
    unsigned char * bulk_in_buffer;
    unsigned char * bulk_out_buffer;
    
    /*
     *  Buffer sizes
     */
    size_t int_in_size;
    size_t bulk_in_size;
    size_t bulk_out_size;
    
    /*
     *  USB Endpoints
     */
    __u8  int_in_endpointAddr;
    __u8  bulk_in_endpointAddr;
    __u8  bulk_out_endpointAddr;
    
    /*
     *  Endpoint intervals
     */
    __u8  int_in_endpointInterval;
    __u8  bulk_in_endpointInterval;
    __u8  bulk_out_endpointInterval;
    
    /*
     *  URBs
     */
    struct urb * bulk_in_urb;
    struct urb * int_in_urb;
    struct urb * bulk_out_urb;
    
    /*
     *  Refrence counter
     */
    struct kref      kref;
    struct semaphore sem;

    /*
     *  Data from interrupt is retained here.
     */
    struct switches_state  switches;

    unsigned char notify;

    /*
     *  Track usage of the bulk pipes: serialize each pipe's use.
     */
    atomic_t bulk_write_available;
    atomic_t bulk_read_available;

    /*
     *  Data tracking for Read/Write. 
     *  Writes will add to the pending_data count and 
     *  reads will deplete the pending_data count.
     *
     *  Note: The OSRFX2 device specs states that the firmware will buffer
     *        up-to four write packet (two on EP6 and two on EP8).
     *        The buffers can be drained by issuing reads to EP8.
     *        The fifth outstanding write packet attempt will cause the write
     *        to block, waiting for the arrival of a read request to
     *        effectively free a buffer into which the write data can be 
     *        held.
     */
    size_t  pending_data;

    /*
     *  Power Managment related fields
     */
    int   suspended;        /* boolean */

};
 
/*****************************************************************************/
/* Forward declaration for our usb_driver definition later.                  */
/*****************************************************************************/
static struct usb_driver osrfx2_driver;

/*****************************************************************************/
/* This is interrupt packet stucture                                         */
/*****************************************************************************/
struct interrupt_packet {

    struct switches_state  switches;

} __attribute__ ((packed));

/*****************************************************************************/
/* This routine will retrieve the switches state, format it and return a     */
/* representative string.                                                    */
/*                                                                           */
/* Note the two different function defintions depending on kernel version.   */
/*****************************************************************************/
static ssize_t show_switches(struct device * dev, 
                             struct device_attribute * attr, 
                             char * buf)
{
    struct usb_interface   * intf   = to_usb_interface(dev);
    struct osrfx2          * fx2dev = usb_get_intfdata(intf);
    struct switches_state  * packet;
    int retval;

    packet = kmalloc(sizeof(*packet), GFP_KERNEL);
    if (!packet) {
        return -ENOMEM;
    }
    packet->SwitchesOctet = 0;

    retval = usb_control_msg(fx2dev->udev, 
                             usb_rcvctrlpipe(fx2dev->udev, 0), 
                             OSRFX2_READ_SWITCHES, 
                             USB_DIR_IN | USB_TYPE_VENDOR,
                             0,
                             0,
                             packet, 
                             sizeof(*packet),
                             USB_CTRL_GET_TIMEOUT);

    if (retval < 0) {
        dev_err(&fx2dev->udev->dev, "%s - retval=%d\n", __FUNCTION__, retval);
        kfree(packet);
        return retval;
    }

    retval = sprintf(buf, "%s%s%s%s%s%s%s%s",    /* left sw --> right sw */
                     (packet->Switch1) ? "*" : ".",
                     (packet->Switch2) ? "*" : ".",
                     (packet->Switch3) ? "*" : ".",
                     (packet->Switch4) ? "*" : ".",
                     (packet->Switch5) ? "*" : ".",
                     (packet->Switch6) ? "*" : ".",
                     (packet->Switch7) ? "*" : ".",
                     (packet->Switch8) ? "*" : "." );

    kfree(packet);

    return retval;
}

/*****************************************************************************/
/* This macro creates an attribute under the sysfs directory                 */
/*   ---  /sys/bus/usb/devices/<root_hub>-<hub>:1.0/switches                 */
/*                                                                           */
/* The DEVICE_ATTR() will create "dev_attr_switches" .                       */
/* "dev_attr_switches" is referenced in both probe and disconnect routines.  */
/*                                                                           */
/* Note that there is no "set" function for this attribute; therefore the    */
/* S_IWUGO (write) flag is not included and the "set" routine point is set   */
/* to NULL.                                                                  */
/*****************************************************************************/
static DEVICE_ATTR( switches, S_IRUGO, show_switches, NULL );


/*****************************************************************************/
/* This routine will retrieve the bargraph LED state, format it and return a */
/* representative string.                                                    */
/*                                                                           */
/* Note the two different function defintions depending on kernel version.   */
/*****************************************************************************/
static ssize_t show_bargraph(struct device * dev, 
                             struct device_attribute * attr, 
                             char * buf)
{
    struct usb_interface  * intf   = to_usb_interface(dev);
    struct osrfx2         * fx2dev = usb_get_intfdata(intf);
    struct bargraph_state * packet;
    int retval;

    packet = kmalloc(sizeof(*packet), GFP_KERNEL);
    if (!packet) {
        return -ENOMEM;
    }
    packet->BarsOctet = 0;

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
        dev_err(&fx2dev->udev->dev, "%s - retval=%d\n", 
                __FUNCTION__, retval);
        kfree(packet);
        return retval;
    }

    retval = sprintf(buf, "%s%s%s%s%s%s%s%s",    /* bottom LED --> top LED */
                     (packet->Bar1) ? "*" : ".",
                     (packet->Bar2) ? "*" : ".",
                     (packet->Bar3) ? "*" : ".",
                     (packet->Bar4) ? "*" : ".",
                     (packet->Bar5) ? "*" : ".",
                     (packet->Bar6) ? "*" : ".",
                     (packet->Bar7) ? "*" : ".",
                     (packet->Bar8) ? "*" : "." );

    kfree(packet);

    return retval;
}

/*****************************************************************************/
/* This routine will set the bargraph LEDs.                                  */
/*                                                                           */
/* Note the two different function defintions depending on kernel version.   */
/*****************************************************************************/
static ssize_t set_bargraph(struct device * dev, 
                            struct device_attribute * attr, 
                            const char * buf,
                            size_t count)
{
    struct usb_interface  * intf   = to_usb_interface(dev);
    struct osrfx2         * fx2dev = usb_get_intfdata(intf);
    struct bargraph_state * packet;

    unsigned int value;
    int retval;
    char * end;

    packet = kmalloc(sizeof(*packet), GFP_KERNEL);
    if (!packet) {
        return -ENOMEM;
    }
    packet->BarsOctet = 0;

    value = (simple_strtoul(buf, &end, 10) & 0xFF);
    if (buf == end) {
        value = 0;
    }

    packet->Bar1 = (value & 0x01) ? 1 : 0;
    packet->Bar2 = (value & 0x02) ? 1 : 0;
    packet->Bar3 = (value & 0x04) ? 1 : 0;
    packet->Bar4 = (value & 0x08) ? 1 : 0;
    packet->Bar5 = (value & 0x10) ? 1 : 0;
    packet->Bar6 = (value & 0x20) ? 1 : 0;
    packet->Bar7 = (value & 0x40) ? 1 : 0;
    packet->Bar8 = (value & 0x80) ? 1 : 0;

    retval = usb_control_msg(fx2dev->udev, 
                             usb_sndctrlpipe(fx2dev->udev, 0), 
                             OSRFX2_SET_BARGRAPH_DISPLAY, 
                             USB_DIR_OUT | USB_TYPE_VENDOR,
                             0,
                             0,
                             packet, 
                             sizeof(*packet),
                             USB_CTRL_GET_TIMEOUT);

    if (retval < 0) {
        dev_err(&fx2dev->udev->dev, "%s - retval=%d\n", 
                __FUNCTION__, retval);
    }
    
    kfree(packet);

    return count;
}

/*****************************************************************************/
/* This macro creates an attribute under the sysfs directory                 */
/*   ---  /sys/bus/usb/devices/<root_hub>-<hub>:1.0/bargraph                 */
/*                                                                           */
/* The DEVICE_ATTR() will create "dev_attr_bargraph".                        */
/* "dev_attr_bargraph" is referenced in both probe and disconnect routines.  */
/*****************************************************************************/
static DEVICE_ATTR( bargraph, S_IRUGO | S_IWUGO, show_bargraph, set_bargraph );


/*****************************************************************************/
/* This routine will show the 7-segment display value.                       */
/*                                                                           */
/* The 7-segment display raw value is read and then looked-up in the         */
/* mapping table, digit_to_segments.                                         */
/* Any raw display value which is not mapped will be displayed as a ".".     */
/* There are other special cases handles as well, e.g "H" for high-speed.    */
/*                                                                           */
/* Note the two different function defintions depending on kernel version.   */
/*****************************************************************************/
static ssize_t show_7segment(struct device * dev, 
                             struct device_attribute * attr, 
                             char * buf)
{
    struct usb_interface  * intf   = to_usb_interface(dev);
    struct osrfx2         * fx2dev = usb_get_intfdata(intf);
    struct segment_state  * packet;
    int retval;
    int i;


    if (fx2dev->suspended == TRUE) {
        return sprintf(buf, "S ");   /* device is suspended */
    }

    packet = kmalloc(sizeof(*packet), GFP_KERNEL);
    if (!packet) {
        return -ENOMEM;
    }
    packet->SegmentsOctet = nondisplayable;

    retval = usb_control_msg(fx2dev->udev, 
                             usb_rcvctrlpipe(fx2dev->udev, 0), 
                             OSRFX2_READ_7SEGMENT_DISPLAY, 
                             USB_DIR_IN | USB_TYPE_VENDOR,
                             0,
                             0,
                             packet, 
                             sizeof(*packet),
                             USB_CTRL_GET_TIMEOUT);
    if (retval < 0) {
        dev_err(&fx2dev->udev->dev, "%s - retval=%d\n", 
                __FUNCTION__, retval);
        kfree(packet);
        return retval;
    }

    for (i=0; i < sizeof(digit_to_segments); i++) {
        if (packet->SegmentsOctet == digit_to_segments[i]) {
            break;
        }
    }

    if (i < sizeof(digit_to_segments)) {
        retval = sprintf(buf, "%d ", i );
    }
    else {
        /* Check for special cases */
        retval = sprintf(buf, "%s ", 
                         (packet->SegmentsOctet == nondisplayable) ? "." : 
                         (packet->SegmentsOctet == high_speed)     ? "H" : 
                         (packet->SegmentsOctet == power_active)   ? "A" : 
                         "?" );
    }

    kfree(packet);

    return retval;
}

/*****************************************************************************/
/* This routine will set the 7-segment display.                              */
/*                                                                           */
/* The input string (buf) is expected to be a numeric character between 0-9. */
/* Any ohter string values will be displayed on the 7-segment display by     */
/* turning on the "dot" segment, thus indicating a "nondisplayable" value.   */
/*                                                                           */
/* Note the two different function defintions depending on kernel version.   */
/*****************************************************************************/
static ssize_t set_7segment(struct device * dev, 
                            struct device_attribute * attr, 
                            const char * buf,
                            size_t count)
{
    struct usb_interface  * intf   = to_usb_interface(dev);
    struct osrfx2         * fx2dev = usb_get_intfdata(intf);
    struct segment_state  * packet;

    unsigned int value;
    int retval;
    char * end;

    packet = kmalloc(sizeof(*packet), GFP_KERNEL);
    if (!packet) {
        return -ENOMEM;
    }
    packet->SegmentsOctet = 0;

    value = (simple_strtoul(buf, &end, 10) & 0xFF);
    if (buf == end) {
        value = nondisplayable;
    }

    packet->SegmentsOctet = (value < 10) ? 
        digit_to_segments[value] : nondisplayable;

    retval = usb_control_msg(fx2dev->udev, 
                             usb_sndctrlpipe(fx2dev->udev, 0), 
                             OSRFX2_SET_7SEGMENT_DISPLAY, 
                             USB_DIR_OUT | USB_TYPE_VENDOR,
                             0,
                             0,
                             packet, 
                             sizeof(*packet),
                             USB_CTRL_GET_TIMEOUT);
    if (retval < 0) {
        dev_err(&fx2dev->udev->dev, "%s - retval=%d\n", 
                __FUNCTION__, retval);
    }
    
    kfree(packet);

    return count;
}

/*****************************************************************************/
/* This macro creates an attribute under the sysfs directory                 */
/*   ---  /sys/bus/usb/devices/<root_hub>-<hub>:1.0/7segment                 */
/*                                                                           */
/* The DEVICE_ATTR() will create "dev_attr_7segment".                        */
/* "dev_attr_7segment" is referenced in both probe and disconnect routines.  */
/*****************************************************************************/
static DEVICE_ATTR( 7segment, S_IRUGO | S_IWUGO, show_7segment, set_7segment );

/*****************************************************************************/
/* Whenever one of the DIP switches is toggled, an interrupt packet will     */
/* be sent by the device. This routine will catch that packet.               */
/*****************************************************************************/
static void interrupt_handler(struct urb * urb)
{
    struct osrfx2           * fx2dev = urb->context;
    struct interrupt_packet * packet = urb->transfer_buffer;
    int retval;

    if (urb->status == 0) {

        /* 
         *  Retain the updated switches state 
         */
        fx2dev->switches.SwitchesOctet = packet->switches.SwitchesOctet;
        fx2dev->notify = TRUE;
        
        /*
         *  Wake-up any requests enqueued.
         */
        wake_up(&(fx2dev->FieldEventQueue));

        /* 
         *  Restart interrupt urb 
         */
        retval = usb_submit_urb(urb, GFP_ATOMIC);
        if (retval != 0) {
            dev_err(&urb->dev->dev, "%s - error %d submitting interrupt urb\n",
                    __FUNCTION__, retval);
        }

        /* 
         *  Successful completion 
         */
        return;   
    } 

    /* Reached here due to an error: log some useful information */
    switch (urb->status) {
        case -ECONNRESET:
        case -ENOENT:
        case -ESHUTDOWN:
            return;
        default: 
            dev_err(&urb->dev->dev, "%s - non-zero urb status received: %d\n", 
                    __FUNCTION__, urb->status);
            return;
    }
}

/*****************************************************************************/
/* This routine will ready operations for receiving interrupts from the      */
/* OSR USB-FX2 device.                                                       */
/*****************************************************************************/
static int init_interrupts(struct osrfx2 * fx2dev)
{
    int pipe;
    int retval;

    pipe = usb_rcvintpipe(fx2dev->udev, fx2dev->int_in_endpointAddr);
    
    fx2dev->int_in_size = sizeof(struct interrupt_packet);  
    
    fx2dev->int_in_buffer = kmalloc(fx2dev->int_in_size, GFP_KERNEL);
    if (!fx2dev->int_in_buffer) {
        return -ENOMEM;
    }

    fx2dev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!fx2dev->int_in_urb) {
        return -ENOMEM;
    }

    usb_fill_int_urb( fx2dev->int_in_urb, 
                      fx2dev->udev,
                      pipe,
                      fx2dev->int_in_buffer, 
                      fx2dev->int_in_size,
                      interrupt_handler, 
                      fx2dev,
                      fx2dev->int_in_endpointInterval );

    retval = usb_submit_urb( fx2dev->int_in_urb, GFP_KERNEL );
    if (retval != 0) {
        dev_err(&fx2dev->udev->dev, "usb_submit_urb error %d \n", retval);
        return retval;
    }

    return 0; 
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
static int init_bulks(struct osrfx2 * fx2dev)
{
    fx2dev->bulk_in_buffer = kmalloc(fx2dev->bulk_in_size, GFP_KERNEL);
    if (!fx2dev->bulk_in_buffer) {
        return -ENOMEM;
    }
    fx2dev->bulk_out_buffer = kmalloc(fx2dev->bulk_out_size, GFP_KERNEL);
    if (!fx2dev->bulk_out_buffer) {
        return -ENOMEM;
    }

    init_MUTEX( &fx2dev->sem );
    init_waitqueue_head( &fx2dev->FieldEventQueue );

    return 0; 
}

/*****************************************************************************/
/* This routine will attempt to locate the required endpoints and            */
/* retain relevant information in the osrfx2 structure instance.             */
/*****************************************************************************/
static int find_endpoints(struct osrfx2 * fx2dev)
{
    struct usb_interface * interface = fx2dev->interface;
    struct usb_endpoint_descriptor * endpoint;
    unsigned char dir;
    unsigned char attr;
    int i;

    for (i=0; i < interface->cur_altsetting->desc.bNumEndpoints; i++) {

        endpoint = &interface->cur_altsetting->endpoint[i].desc;
        dir  = endpoint->bEndpointAddress & USB_DIR_IN;
        attr = endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;

        switch ((dir << 8) + attr) {
            case ((USB_DIR_IN << 8) + USB_ENDPOINT_XFER_INT) :
                fx2dev->int_in_endpointAddr = endpoint->bEndpointAddress;
                fx2dev->int_in_endpointInterval = endpoint->bInterval;
                fx2dev->int_in_size = endpoint->wMaxPacketSize;
                break;
            case ((USB_DIR_IN << 8) + USB_ENDPOINT_XFER_BULK) :
                fx2dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
                fx2dev->bulk_in_endpointInterval = endpoint->bInterval;
                fx2dev->bulk_in_size = endpoint->wMaxPacketSize;
                break;
            case ((USB_DIR_OUT << 8) + USB_ENDPOINT_XFER_BULK) :
                fx2dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
                fx2dev->bulk_out_endpointInterval = endpoint->bInterval;
                fx2dev->bulk_out_size = endpoint->wMaxPacketSize;
                break;
            default:
                break;
        }
    }
    if (fx2dev->int_in_endpointAddr   == 0 || 
        fx2dev->bulk_in_endpointAddr  == 0 ||
        fx2dev->bulk_out_endpointAddr == 0) {
        dev_err(&interface->dev, "%s - failed to find required endpoints\n", 
                __FUNCTION__);
        return -ENODEV;
    }
    return 0;
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
static void osrfx2_delete(struct kref * kref)
{
    struct osrfx2 * fx2dev = container_of(kref, struct osrfx2, kref);

    usb_put_dev( fx2dev->udev );
    
    if (fx2dev->int_in_urb) {
        usb_free_urb(fx2dev->int_in_urb);
    }
    if (fx2dev->int_in_buffer) {
        kfree(fx2dev->int_in_buffer);
    }
    if (fx2dev->bulk_in_buffer) {
        kfree( fx2dev->bulk_in_buffer );
    }
    if (fx2dev->bulk_out_buffer) {
        kfree( fx2dev->bulk_out_buffer );
    }

    kfree( fx2dev );
}

/*****************************************************************************/
/* osrfx2_open                                                               */
/*                                                                           */
/* Note:                                                                     */
/*   The serialization method used below has a side-effect which I don't     */
/*   particularly care for. In effect switch_events and bulk I/O are         */
/*   mutually exclusive, e.g. an open for switch_events will exclude         */
/*   opens for bulk I/O (osrbulk) and vis-a-verse.                           */
/*                                                                           */
/* Note:                                                                     */
/*   The usb_clear_halt() is being used to effect a pipe reset.              */
/*   This will clear any residual data at the endpoint and ready it for      */
/*   the new endpoint user's data.                                           */
/*****************************************************************************/
static int osrfx2_open(struct inode * inode, struct file * file)
{
    struct usb_interface * interface;
    struct osrfx2 * fx2dev;
    int retval;
    int flags;
    
    interface = usb_find_interface(&osrfx2_driver, iminor(inode));
    if (interface == NULL) 
        return -ENODEV;

    fx2dev = usb_get_intfdata(interface);
    if (fx2dev == NULL) 
        return -ENODEV;

    /*
     *   Serialize access to each of the bulk pipes.
     */ 
    flags = (file->f_flags & O_ACCMODE);

    if ((flags == O_WRONLY) || (flags == O_RDWR)) {
        if (atomic_dec_and_test( &fx2dev->bulk_write_available ) == 0) {
            atomic_inc( &fx2dev->bulk_write_available );
            return -EBUSY;
        }

        /*
         *   The write interface is serialized, so reset bulk-out pipe (ep-6).
         */
        retval = usb_clear_halt(fx2dev->udev, fx2dev->bulk_out_endpointAddr);
        if ((retval != 0) && (retval != -EPIPE)) {
            dev_err(&interface->dev, "%s - error(%d) usb_clear_halt(%02X)\n", 
                    __FUNCTION__, retval, fx2dev->bulk_out_endpointAddr);
        }
    }

    if ((flags == O_RDONLY) || (flags == O_RDWR)) {
        if (atomic_dec_and_test( &fx2dev->bulk_read_available ) == 0) {
            atomic_inc( &fx2dev->bulk_read_available );
            if (flags == O_RDWR) 
                atomic_inc( &fx2dev->bulk_write_available );
            return -EBUSY;
        }

        /*
         *   The read interface is serialized, so reset bulk-in pipe (ep-8).
         */
        retval = usb_clear_halt(fx2dev->udev, fx2dev->bulk_in_endpointAddr);
        if ((retval != 0) && (retval != -EPIPE)) {
            dev_err(&interface->dev, "%s - error(%d) usb_clear_halt(%02X)\n", 
                    __FUNCTION__, retval, fx2dev->bulk_in_endpointAddr);
        }
    }

    /*
     *   Set this device as non-seekable.
     */ 
    retval = nonseekable_open(inode, file);
    if (retval != 0) {
        return retval;
    }

    /*
     *   Increment our usage count for the device.
     */
    kref_get(&fx2dev->kref);

    /*
     *   Save pointer to device instance in the file's private structure.
     */
    file->private_data = fx2dev;

    return 0;
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
static int osrfx2_release(struct inode * inode, struct file * file)
{
    struct osrfx2 * fx2dev;
    int flags;

    fx2dev = (struct osrfx2 *)file->private_data;
    if (fx2dev == NULL)
        return -ENODEV;

    /* 
     *  Release any bulk_[write|read]_available serialization.
     */
    flags = (file->f_flags & O_ACCMODE);

    if ((flags == O_WRONLY) || (flags == O_RDWR))
        atomic_inc( &fx2dev->bulk_write_available );

    if ((flags == O_RDONLY) || (flags == O_RDWR)) 
        atomic_inc( &fx2dev->bulk_read_available );

    /* 
     *  Decrement the ref-count on the device instance.
     */
    kref_put(&fx2dev->kref, osrfx2_delete);
    
    return 0;
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
static ssize_t osrfx2_read(struct file * file, char * buffer, 
                           size_t count, loff_t * ppos)
{
    struct osrfx2 * fx2dev;
    int retval = 0;
    int bytes_read;
    int pipe;

    fx2dev = (struct osrfx2 *)file->private_data;

    pipe = usb_rcvbulkpipe(fx2dev->udev, fx2dev->bulk_in_endpointAddr),

    /* 
     *  Do a blocking bulk read to get data from the device 
     */
    retval = usb_bulk_msg( fx2dev->udev, 
                           pipe,
                           fx2dev->bulk_in_buffer,
                           min(fx2dev->bulk_in_size, count),
                           &bytes_read, 
                           10000 );

    /* 
     *  If the read was successful, copy the data to userspace 
     */
    if (!retval) {
        if (copy_to_user(buffer, fx2dev->bulk_in_buffer, bytes_read)) {
            retval = -EFAULT;
        }
        else {
            retval = bytes_read;
        }
        
        /*
         *  Increment the pending_data counter by the byte count received.
         */
        fx2dev->pending_data -= retval;
    }

    return retval;
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
static void write_bulk_backend(struct urb * urb)
{
    struct osrfx2 * fx2dev = (struct osrfx2 *)urb->context;

    /* 
     *  Filter sync and async unlink events as non-errors.
     */
    if (urb->status && 
        !(urb->status == -ENOENT || 
          urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN)) {
        dev_err(&fx2dev->interface->dev, 
                "%s - non-zero status received: %d\n",
                __FUNCTION__, urb->status);
    }

    /* 
     *  Free the spent buffer.
     */
    usb_buffer_free( urb->dev, 
                     urb->transfer_buffer_length, 
                     urb->transfer_buffer, 
                     urb->transfer_dma );
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
static ssize_t osrfx2_write(struct file * file, const char * user_buffer, 
                            size_t count, loff_t * ppos)
{
    struct osrfx2 * fx2dev;
    struct urb * urb = NULL;
    char * buf = NULL;
    int pipe;
    int retval = 0;

    fx2dev = (struct osrfx2 *)file->private_data;

    if (count == 0)
        return count;

    /* 
     *  Create a urb, and a buffer for it, and copy the data to the urb.
     */
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!urb) {
        retval = -ENOMEM;
        goto error;
    }

    buf = usb_buffer_alloc( fx2dev->udev, 
                            count, 
                            GFP_KERNEL, 
                            &urb->transfer_dma );
    if (!buf) {
        retval = -ENOMEM;
        goto error;
    }

    if (copy_from_user(buf, user_buffer, count)) {
        retval = -EFAULT;
        goto error;
    }

    /* 
     *  Initialize the urb properly.
     */
    pipe = usb_sndbulkpipe( fx2dev->udev, fx2dev->bulk_out_endpointAddr );

    usb_fill_bulk_urb( urb, 
                       fx2dev->udev,
                       pipe,
                       buf, 
                       count, 
                       write_bulk_backend, 
                       fx2dev );

    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    /* 
     *  Send the data out the bulk port
     */
    retval = usb_submit_urb(urb, GFP_KERNEL);
    if (retval) {
        dev_err(&fx2dev->interface->dev, "%s - usb_submit_urb failed: %d\n",
                __FUNCTION__, retval);
        goto error;
    }

    /*
     *  Increment the pending_data counter by the byte count sent.
     */
    fx2dev->pending_data += count;

    /* 
     *  Release the reference to this urb, the USB core
     *  will eventually free it entirely.
     */
    usb_free_urb(urb);

    return count;

error:
    usb_buffer_free(fx2dev->udev, count, buf, urb->transfer_dma);
    usb_free_urb(urb);
    return retval;
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
static unsigned int osrfx2_poll(struct file * file, poll_table * wait)
{
    struct osrfx2 * fx2dev = (struct osrfx2 *)file->private_data;
    unsigned int mask = 0;
    int retval = 0;

    retval = down_interruptible( &fx2dev->sem );
    
    poll_wait(file, &fx2dev->FieldEventQueue, wait);

    if ( fx2dev->notify == TRUE ) {
        fx2dev->notify = FALSE;
        mask |= POLLPRI;
    }

    if ( fx2dev->pending_data > 0) {
        mask |= POLLIN | POLLRDNORM;
    }

    up( &fx2dev->sem );
    
    return mask;
}

/*****************************************************************************/
/* This fills-in the driver-supported file_operations fields.                */
/*****************************************************************************/
static struct file_operations osrfx2_file_ops = {
    .owner   = THIS_MODULE,
    .open    = osrfx2_open,
    .release = osrfx2_release,
    .read    = osrfx2_read,
    .write   = osrfx2_write,
    .poll    = osrfx2_poll,
};
 
/*****************************************************************************/
/* Usb class driver info in order to get a minor number from the usb core,   */
/* and to have the device registered with devfs and the driver core.         */
/*****************************************************************************/
static struct usb_class_driver osrfx2_class = {
    .name       = "device/osrfx2_%d",
    .fops       = &osrfx2_file_ops,
    .minor_base = DEVICE_MINOR_BASE,
};

/*****************************************************************************/
/* Event: un-bound device instance is querying for suitable owner driver.    */
/*****************************************************************************/
static int osrfx2_probe(struct usb_interface * interface, 
                        const struct usb_device_id * id)
{
    struct usb_device * udev = interface_to_usbdev(interface);
    struct osrfx2 * fx2dev = NULL;
    int retval;

    fx2dev = kmalloc(sizeof(struct osrfx2), GFP_KERNEL);
    if (fx2dev == NULL) {
        retval = -ENOMEM;
        goto error;
    }
    memset(fx2dev, 0, sizeof(*fx2dev));
    kref_init( &fx2dev->kref );

    fx2dev->udev = usb_get_dev(udev);
    fx2dev->interface = interface;
    fx2dev->suspended = FALSE;

    fx2dev->bulk_write_available = (atomic_t) ATOMIC_INIT(1);
    fx2dev->bulk_read_available  = (atomic_t) ATOMIC_INIT(1);

    usb_set_intfdata(interface, fx2dev);

    device_create_file(&interface->dev, &dev_attr_switches);
    device_create_file(&interface->dev, &dev_attr_bargraph);
    device_create_file(&interface->dev, &dev_attr_7segment);

    retval = find_endpoints( fx2dev );
    if (retval != 0) 
        goto error;
    
    retval = init_interrupts( fx2dev );
    if (retval != 0)
        goto error;

    retval = init_bulks( fx2dev );
    if (retval != 0)
        goto error;

    retval = usb_register_dev(interface, &osrfx2_class);
    if (retval != 0) {
        usb_set_intfdata(interface, NULL);
    }

    dev_info(&interface->dev, "OSR USB-FX2 device now attached.\n");

    return 0;

error:
    dev_err(&interface->dev, "OSR USB-FX2 device probe failed: %d.\n", retval);
    if (fx2dev) {
        kref_put( &fx2dev->kref, osrfx2_delete );
    }
    return retval;
}

/*****************************************************************************/
/* Event: device instance is being disconnected (deleted)                    */
/*****************************************************************************/
static void osrfx2_disconnect(struct usb_interface * interface)
{
    struct osrfx2 * fx2dev;

    lock_kernel();

    fx2dev = usb_get_intfdata(interface);

    usb_kill_urb(fx2dev->int_in_urb);
    
    usb_set_intfdata(interface, NULL);

    device_remove_file(&interface->dev, &dev_attr_switches);
    device_remove_file(&interface->dev, &dev_attr_bargraph);
    device_remove_file(&interface->dev, &dev_attr_7segment);

    usb_deregister_dev(interface, &osrfx2_class);

    unlock_kernel();

    kref_put( &fx2dev->kref, osrfx2_delete );

    dev_info(&interface->dev, "OSR USB-FX2 now disconnected.\n");
}

/*****************************************************************************/
/* Event: device is being suspended.                                         */
/*****************************************************************************/
static int osrfx2_suspend(struct usb_interface * intf, pm_message_t message)
{
    struct osrfx2 * fx2dev = usb_get_intfdata(intf);

    dev_info(&intf->dev, "%s - entry\n", __FUNCTION__);

    if (down_interruptible(&fx2dev->sem)) {
        return -ERESTARTSYS;
    }

    fx2dev->suspended = TRUE;

    /* 
     *  Stop the interrupt pipe read urb.
     */
    usb_kill_urb(fx2dev->int_in_urb);

    up(&fx2dev->sem);

    return 0;
}

/*****************************************************************************/
/* Event: device is being resumed                                            */
/*****************************************************************************/
static int osrfx2_resume(struct usb_interface * intf)
{
    int retval;
    struct osrfx2 * fx2dev = usb_get_intfdata(intf);

    dev_info(&intf->dev, "%s - entry\n", __FUNCTION__);

    if (down_interruptible(&fx2dev->sem)) {
        return -ERESTARTSYS;
    }
    
    fx2dev->suspended = FALSE;

    /* 
     *  Re-start the interrupt pipe read urb.
     */
    retval = usb_submit_urb( fx2dev->int_in_urb, GFP_KERNEL );
    
    if (retval) {
        dev_err(&intf->dev, "%s - usb_submit_urb failed %d\n",
                __FUNCTION__, retval);

        switch (retval) {
        case -EHOSTUNREACH:
            dev_err(&intf->dev, "%s - EHOSTUNREACH probably cause: "
                    "parent hub/port still suspended.\n", 
                    __FUNCTION__);
            break;

        default:
            break;

        }
    }
    
    up(&fx2dev->sem);

    return 0;
}

/*****************************************************************************/
/* This driver's usb_driver structure: ref-ed by osrfx2_init and osrfx2_exit */
/*****************************************************************************/
static struct usb_driver osrfx2_driver = {
    .name        = "osrfx2",
    .probe       = osrfx2_probe,
    .disconnect  = osrfx2_disconnect,
    .suspend     = osrfx2_suspend,
    .resume      = osrfx2_resume,
    .id_table    = id_table,
};

/*****************************************************************************/
/* This driver's commission routine: just register with USB subsystem.       */
/*****************************************************************************/
static int __init osrfx2_init(void)
{
    int retval;

    retval = usb_register(&osrfx2_driver);

    return retval;
}

/*****************************************************************************/
/* This driver's decommission routine: just deregister with USB subsystem.   */
/*****************************************************************************/
static void __exit osrfx2_exit(void)
{
    usb_deregister( &osrfx2_driver );
}

/*****************************************************************************/
/* Advertise this driver's init and exit routines                            */
/*****************************************************************************/
module_init( osrfx2_init );
module_exit( osrfx2_exit );

MODULE_AUTHOR("Robin Callender");
MODULE_DESCRIPTION("A driver for the OSR USB-FX2 Learning Kit device");
MODULE_LICENSE("GPL");

