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
bus_type @ include/linux/device.h
|_struct subsys_private *p; //The private data of the driver core, only the driver core can touch this.

device_driver @ include/linux/device.h
|_struct bus_type         *bus; //The bus which the device of this driver belongs to.
|_struct driver_private *p; //Driver core's private data, no one other than the driver core can touch this.

device @ include/linux/device.h
|_struct device_private   *p; //Holds the private data of the driver core portions of the device.
|_struct bus_type *bus; //Type of bus device is on.
|_struct device_driver *driver; //Which driver has allocated this

subsys_private @ drivers/base/base.h
|_struct kset *devices_kset;
|_struct kset *drivers_kset;
|_struct klist klist_devices;
|_struct klist klist_drivers;

driver_private @ drivers/base/base.h
|_struct klist klist_devices;

device_private @ drivers/base/base.h

kobject
kset
klist

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
|_devpath: device ID string for use in messages (e.g., /port/...)
  e.p.: 	/sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6/1-6.7/1-6.7.7
'1-6', '1-6.7', '1-6.7.7' are all device ID for device - ext-hub1, ext-hub2, mouse
ehc		-> /sys/devices/pci0000:00/0000:00:1d.7
|_roothub		-> /sys/devices/pci0000:00/0000:00:1d.7/usb1
  |_ext-hub1	-> /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6
    |_ext-hub2	-> /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6/1-6.7
      |_mouse	-> /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-6/1-6.7/1-6.7.7

struct usb_bus // every bus - a tree of usb devices - is defined as a usb bus



## important vars
struct bus_type usb_bus_type; @ drivers/usb/core/driver.c
which is registered in usb_int()
retval = bus_register(&usb_bus_type);

struct device_type usb_if_device_type @ drivers/usb/core/message.c

