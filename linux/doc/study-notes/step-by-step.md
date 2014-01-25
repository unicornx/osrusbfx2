## Knowledge points
device model

file-operations

sysfs
HAL http://www.freedesktop.org/wiki/Software/hal/

udev
netlink
D-BUS:http://dbus.freedesktop.org/

## important functions about usb
usb_init() @ drivers/usb/core/usb.c

## important structure types about usb:
### device model
@ include/linux/kobject.h
kobject
|_struct sysfs_dirent     *sd;
|_struct kset             *kset;

kset
|_struct kobject kobj;

@ include/linux/device.h
struct bus_type 
|_struct subsys_private *p; //The private data of the driver core, only the driver core can touch this.

struct device_driver
|_struct bus_type         *bus; //The bus which the device of this driver belongs to.
|_struct driver_private *p; //Driver core's private data, no one other than the driver core can touch this.

struct device
|_struct device_private   *p; //Holds the private data of the driver core portions of the device.
|_struct kobject kobj;  // <-------kobject. A top-level, abstract class from which other classes are derived.
|_struct bus_type *bus; //Type of bus device is on.
|_struct device_driver *driver; //Which driver has allocated this

@ drivers/base/base.h
struct subsys_private
|_struct kset subsys; // <-------kobject/kset
|_struct kset *devices_kset;
|_struct kset *drivers_kset;
|_struct klist klist_devices;
|_struct klist klist_drivers;

struct driver_private
|_struct kobject kobj; // <-------kobject
|_struct klist klist_devices;

struct device_private


### usb
@ include/linux/usb.h

struct usbdrv_wrap {
        struct device_driver driver; // The driver-model core driver structure.
        int for_devices; // flag this driver is for interface or device
};

struct usb_driver 
|_struct usbdrv_wrap drvwrap; //Driver-model core structure wrapper.

struct usb_device_driver
|_struct usbdrv_wrap drvwrap; //Driver-model core structure wrapper.

struct usb_class_driver 
This structure is used for the usb_register_dev() and usb_unregister_dev() functions, to consolidate a number of the parameters used for them.

struct usb_interface
|_struct usb_host_interface *altsetting;
|_struct device dev; //driver model's view of this device
|_struct device *usb_dev; //if an interface is bound to the USB major, this will point to the sysfs representation for that device.

/* host-side wrapper for one interface setting's parsed descriptors */
struct usb_host_interface
|_struct usb_interface_descriptor desc; //descriptor for this interface
|_struct usb_host_endpoint *endpoint; //array of endpoints associated with this interface setting.

struct usb_host_endpoint
|_struct usb_endpoint_descriptor          desc; //descriptor for this endpoint
|_struct list_head                urb_list; //urbs queued to this endpoint; maintained by usbcore
|_struct ep_device                *ep_dev; //ep_device for sysfs info

struct usb_device
|_struct device dev; //generic device interface
|_devpath: device ID string for use in messages (e.g., /port/...)
	e.p.: 	/sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6/1-6.7/1-6.7.7
	'1-6', '1-6.7', '1-6.7.7' are all device ID for device - ext-hub1, ext-hub2, mouse
	ehc		-> /sys/devices/pci0000:00/0000:00:1d.7
	|_roothub		-> /sys/devices/pci0000:00/0000:00:1d.7/usb1
	  |_ext-hub1	-> /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6
	    |_ext-hub2	-> /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6/1-6.7
	      |_mouse	-> /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6/1-6.7/1-6.7.7

struct usb_bus // every bus - a tree of usb devices - is defined as a usb bus
	|_struct device *controller;      /* host/master side hardware */

@ include/linux/usb/hcd.h
struct usb_hcd
|_struct usb_bus          self;           /* hcd is-a bus, hcd shoudl be a device, it's contained device object is included by the usb_bus/self.controller, which is a device object */
We'd better treat tge usb_hcd as a framework which stores some common data for other specific drivers, like ehci/ohci hcd drivers.

## important vars
@ drivers/usb/core/driver.c
struct bus_type usb_bus_type; 
	which is registered in usb_int()
	retval = bus_register(&usb_bus_type);

@ drivers/usb/core/message.c
struct device_type usb_if_device_type 

@ drivers/usb/core/usb.c
struct device_type usb_device_type



## hub routines:
@ drivers/usb/core/hub.c
int usb_hub_init(void)->kthread_run(hub_thread, NULL, "khubd");
	|_static int hub_thread(void *__unused)
		|_ static void hub_events(void)
			|_static void hub_port_connect_change(
				|_struct usb_device *usb_alloc_dev() @drivers/usb/core/usb.c


## good reference:
About device detection and driver binding:
usb接口驱动加载流程分析: http://blog.chinaunix.net/uid-20727076-id-3273535.html

usb热插拔实现机制 : http://blog.chinaunix.net/uid-20727076-id-3279435.html

Linux USB子系统--繁荣的背后: http://hongwazi.blog.163.com/blog/static/206776214201291810103717/

linux USB框架分析: http://liu1227787871.blog.163.com/blog/static/205363197201281823719281/