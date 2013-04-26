学习笔记 ldd3

LDD3 on line doc @ http://lwn.net/Kernel/LDD3/

Linux Kernel Source on line @ http://lxr.free-electrons.com/ident

一个简单但生动的教程 "Linux Device Drivers Series" tag @ http://www.linuxforu.com/tag/linux-device-drivers-series/page/2/

一些例子教程:  
http://www.codeproject.com/Articles/112474/A-Simple-Driver-for-Linux-OS  
https://www.kernel.org/doc/htmldocs/writing_usb_driver.html

http://www.kroah.com/ - Greg Kroah-Hartman's homepage  
https://github.com/gregkh - Greg Kroah-Hartman's github

#Chapter2:#
对ubuntu下,按照书上的例子hello的说法会在终端显示“Hello, world”。但是运行后什么都没有出现的解释:

>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
printk 是内核的调用接口，它在系统init之前，把消息写往控制台，但是一旦系统init之后，便改写到系统的日志中。因为init之后用户所能够感受到的进程(拥有控制台)都是用户空间的进程,而用户空间的进程是无法反映内核数据的,除非通过一定的方式(如proc文件,netlink...)向内核申请相关的信息,并在用户空间反映出来.这样做的好处不言而喻的，如果内核在任何的时候都可以写信息到控制台，那控制台一定会被这样的信息淹没，而无法工作。

系统初始化进程(pid=0),工作在内核空间,它在启动init进程(pid=1)之前,把所有的信息通过printk内核方法写往控制台.printk把欲打印的日志消息,首先首先保存到内核的日志缓冲区之中.而后申请控制台的信号量，如果申请到，则调用控制台写方法，写控制台.而此时系统中并没有打开控制台,故而初始化进程,可以申请到控制台的信号量.当系统初始化到达一定阶段后,便会启动init进程(pid=1),并在此之前打开控制台,控制台的信号量增加,此后，printk便无法申请到信号量,而无法写数据到控制台.转而通过和用户空间的进程协作把内核的日志消息写到系统道的日志文件之中.-------前台进程通过sys_log系统调用读出，并根据配置文件sys_conf写向相应的日志文件或/var/log/messages文件中。

读printk原码可知流程。

可以切换到linux提供的终端来看到tty下的输出， 这个就很简单了，直接按Alt+Ctrl+F1~F6
Alt+Ctrl+F7即可退出返回GNOME
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

书中说明如果不出现在终端 则会写进 syslog 文件中，所以看一下系统日志：

`cat /var/log/syslog |grep world`
or directly run 'dmesg'

因为模块是被链接到内核的，所以它能够调用的函数仅仅是内核导出的函数，不存在任何可以链接的函数库。这个和用户程序是不一样的。

#Chapter3:#
第一步 设备编号的注册

几个重要的与文件相关的数据结构：
/include/linux/fs.h

struct file {  
....  
    const struct file_operations	*f_op;  
....  
    struct address_space	*f_mapping; // point to inode  
....  
};


struct file_operations {  
	struct module *owner;  
	loff_t (*llseek) (struct file *, loff_t, int);  
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);  
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);  
	ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);  
	ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);  
	int (*readdir) (struct file *, void *, filldir_t);  
	unsigned int (*poll) (struct file *, struct poll_table_struct *);  
	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);  
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);  
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);  
	int (*mmap) (struct file *, struct vm_area_struct *);  
	int (*open) (struct inode *, struct file *);  
	int (*flush) (struct file *, fl_owner_t id);  
	int (*release) (struct inode *, struct file *);  
	int (*fsync) (struct file *, struct dentry *, int datasync);  
	int (*aio_fsync) (struct kiocb *, int datasync);  
	int (*fasync) (int, struct file *, int);  
	int (*lock) (struct file *, int, struct file_lock *);  
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);  
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);  
	int (*check_flags)(int);  
	int (*flock) (struct file *, int, struct file_lock *);  
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);  
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);  
	int (*setlease)(struct file *, long, struct file_lock **);  
};  

struct inode {  
...  
    dev_t			i_rdev;  
//在2.6.32中看不到i_cdev成员了  
....  
}

file是文件描述符，对单个物理文件，对应一个inode结构，但可以对应多个file结构，即多个打开的文件描述符。


#Chapter4 调试技术#

##printk##

printk的日志级别：
/include/linux/kernel.h  
`#define	KERN_EMERG	"<0>"	/* system is unusable			*/`  
`#define	KERN_ALERT	"<1>"	/* action must be taken immediately	*/`  
`#define	KERN_CRIT	          "<2>"	/* critical conditions			*/`  
`#define	KERN_ERR	          "<3>"	/* error conditions			*/`  
`#define	KERN_WARNING	"<4>"	/* warning conditions			*/`  
`#define	KERN_NOTICE	"<5>"	/* normal but significant condition	*/`  
`#define	KERN_INFO	          "<6>"	/* informational			*/`  
`#define	KERN_DEBUG	"<7>"	/* debug-level messages			*/`  

/* Use the default kernel loglevel */
`#define KERN_DEFAULT	"<d>"`  

/kernel/printk.c

int console_printk[4] = {  
	DEFAULT_CONSOLE_LOGLEVEL,	/* console_loglevel */  
	DEFAULT_MESSAGE_LOGLEVEL,	/* default_message_loglevel */  
	MINIMUM_CONSOLE_LOGLEVEL,	/* minimum_console_loglevel */  
	DEFAULT_CONSOLE_LOGLEVEL,	/* default_console_loglevel */  
};

http://www.linuxquestions.org/questions/programming-9/where-is-printk-output-650288/

查看当前控制台日志级别：  
cat /proc/sys/kernel/printk  
4 4 1 7  
- console_loglevel: the current level below which the messages are displayed in console  
- default_message_loglevel: the default level for messages without a specified priority when you call printk  
- minimum_console_loglevel: the minimum level allowed for console messages //? don't know what it is used, when I see it is "1", but if I call printk with 0, still output  
- default_console_loglevel: the level used by the command 'enable printk's to console' in syslog(2).   
以上对应console_printk变量

to quick open console printk  
echo 8 > /proc/sys/kernel/printk

##通过监视调试##
strace命令的使用，它可以显示由用户空间程序发出的所有系统调用。不仅显示调用，还能显示调用参数以及用符号形式表示的返回值。

#Chapter8 need read more

#Chapter11 内核的数据类型#
使用-Wall -Wstrict-prototypes选项编译可以防止大多数的和数据类型有关的代码缺陷。

内核使用的数据类型主要有三大类：
- 类似int这样的标准C语言类型   - C语言标准类型
- 类似u32这样的有确定大小的类型 - 内核标准类型
- 像pid_t这样的内核对象类型    - 内核数据结构

对于注意对于C语言标准类型，在不同的体系架构上，其所占空间的大小并不相同。所以要慎用C语言标准类型。如果你明确某个数据需要使用指定字节长度，则需要改用类似u32这样的确定大小的内核标准类型,因为内核标准类型已经根据不同的体系架构做了适配。具体可以参考include/linux/types.h，而该文件又包含了<asm/types.h>，这么写是为了在编译时根据不同的arch选择不同arch目录下对应的type.h。

使用标准C语言类型有一个典型的例子，就是内核中通常的内存地址常常是 unsigned long, 利用了如下事实：至少在 Linux 目前支持的所有平台上,指针和长整型的大小总是相同的。

移植性问题:
- 时间间隔
- 页大小
- 字节序
- 数据对齐
- 指针和错误值


文件系统对设备的表达一直没有搞清楚。

32位总线
`pci0000:00/0000:00:1d.7`  
`          |    |  |    |  |  |_功能(3bit)`         
`          |    |  |__设备(5bit)`  
`          |    |_总线（8位）`  
`          |_PCI域(16bit)`


#Chapter13 USB drivers
/include/linux/usb/Ch9.h, in which define all usb2 basic types


USB设备的表达式格式：  
root_hub-hub_port:config.interface  
root_hub： 1 起序  
hub_port： 1 起序  
config： 1 起序  
interface： 0 起序  
sysfs例子：  
/sys/devices/pci0000:00/0000:00:09.0/usb2/2-1  
|-- 2-1:1.0  
| |-- bAlternateSetting  
| |-- bInterfaceClass  
| |-- bInterfaceNumber  
| |-- bInterfaceProtocol  
| |-- bInterfaceSubClass  
| |-- bNumEndpoints  
| |-- detach_state  
| |-- iInterface  
| `-- power  
| `-- state  
|-- bConfigurationValue  
|-- bDeviceClass  
|-- bDeviceProtocol  
|-- bDeviceSubClass  
|-- bMaxPower  
|-- bNumConfigurations  
|-- bNumInterfaces  
|-- bcdDevice  
|-- bmAttributes  
|-- detach_state  
|-- devnum  
|-- idProduct  
|-- idVendor  
|-- maxchild  
|-- power  
| `-- state  
|-- speed  
`-- version  


usbfs does not exit since 2.6.32

##USB Urbs
The typical lifecycle of a urb is as follows:
• Created by a USB device driver.  
• Assigned to a specific endpoint of a specific USB device.  
• Submitted to the USB core, by the USB device driver.  
• Submitted to the specific USB host controller driver for the specified device by the USB core.  
• Processed by the USB host controller driver that makes a USB transfer to the device.  
• When the urb is completed, the USB host controller driver notifies the USB device driver.  

/include/linux/usb.h  
struct urb {  
	/* private: usb core and host controller only fields in the urb */  
	struct kref kref;		/* reference count of the URB */  
	void *hcpriv;			/* private data for host controller */  
	atomic_t use_count;		/* concurrent submissions counter */  
	atomic_t reject;		/* submissions will fail */  
	int unlinked;			/* unlink error code */  
  
	/* public: documented fields in the urb that can be used by drivers */  
	struct list_head urb_list;	/* list head for use by the urb's  current owner */  
	struct list_head anchor_list;	/* the URB may be anchored */  
	struct usb_anchor *anchor;  
	struct usb_device *dev; 	/* (in) pointer to associated device */  
	struct usb_host_endpoint *ep;	/* (internal) pointer to endpoint */  
	unsigned int pipe;		/* (in) pipe information */  
	int status;			/* (return) non-ISO status */ 当URB结束或者正在被usbcore处理时返回的当前状态。主要用于Non-ISO（相对于Isochronous传输，Non-ISO指Bulk，Control或者Interrut传输）的返回状态值，对于Isochronous传输，如果该值不为0，则表示URB发生了unlink现象（所谓unlink是指当一个URB被提交给core之后而未完成之前被驱动主动撤销或者发生了设备被移除的事件）。该值驱动应该只在完成函数中访问该变量，对于Isochronous的URB返回状态值，参考iso_frame_desc成员  
	unsigned int transfer_flags;	/* (in) URB_SHORT_NOT_OK | ...*/  
	void *transfer_buffer;		/* (in) associated data buffer */  
	dma_addr_t transfer_dma;	/* (in) dma addr for transfer_buffer */  
	struct usb_sg_request *sg;	/* (in) scatter gather buffer list */  
	int num_sgs;			/* (in) number of entries in the sg list */  
	u32 transfer_buffer_length;	/* (in) data buffer length */  
	u32 actual_length;		/* (return) actual transfer length */ URB结束时实际发送或者接收的字节数  
	unsigned char *setup_packet;	/* (in) setup packet (control only) */  
	dma_addr_t setup_dma;		/* (in) dma addr for setup_packet */  
	int start_frame;		/* (modify) start frame (ISO) */  
	int number_of_packets;		/* (in) number of ISO packets */  
	int interval;			/* (modify) transfer interval (INT/ISO) */  
	int error_count;		/* (return) number of ISO errors */  
	void *context;			/* (in) context for completion */  
	usb_complete_t complete;	/* (in) completion routine */ 完成函数，当URB被usbcore执行完成时回调  
	struct usb_iso_packet_descriptor iso_frame_desc[0]; /* (in) ISO ONLY */ IsochronousURB状态返回值  
};  

##URB的创建和删除

创建必需调用API： struct urb *usb_alloc_urb(int iso_packets, int mem_flags); 不可以自己静态定义，否则会破坏系统内建的引用计数跟踪机制  
删除：void usb_free_urb(struct urb *urb);

创建完URB后可以调用一系列对应的初始化函数对URB进行初始化  
- Interrupt urbs： usb_fill_int_urb  
- Bulk urbs： usb_fill_bulk_urb  
- Control urbs：usb_fill_control_urb  
- Isochronous urbs： 没有现成的API，需要手工初始化  

URB的提交
创建和初始化完成后，驱动就可以将URB提交给USB core来发送给设备  
int usb_submit_urb(struct urb *urb, int mem_flags);  
一旦调用完成后，驱动就只能在完成函数中对URB的内存数据进行访问，提前访问URB中的数据是不对的。

##完成函数

##URB的撤销
int usb_kill_urb(struct urb *urb);该函数通常在disconnect回调函数中被调用，在设备从系统上断开时用于撤销未完成的URB。  
int usb_unlink_urb(struct urb *urb);###异步###通知Core停止一个未完成的URB   

查看usb的设备和驱动可以观察以下文件系统：
/sys/bus/usb: 
|_/sys/bus/usb/devices:都link指向/sys/devices下的设备文件
|_/sys/bus/usb/drivers：


#Chapter14 设备模型

有关热插拔

浅谈hotplug, udev, hal, d-bus: http://linux.chinaunix.net/techdoc/net/2009/06/29/1120750.shtml  
理解和使用Linux的硬件抽象层HAL http://blog.csdn.net/colorant/article/details/2611559  
Linux里udev的工作原理：http://www.ha97.com/1003.html  
Udev: Introduction to Device Management In Modern Linux System： http://www.linux.com/news/hardware/peripherals/180950-udev  
http://blog.csdn.net/fjb2080/article/details/4842814 - a serial article about udev  
有关内核加载模块: http://www.360doc.com/content/12/0628/16/1162697_220995749.shtml  


5.App  
↑  
4.D-Bus  
↑  
3.HAL  
↑  
2.udev  
↑  
1.Kernel  
 

##1. Kernel
Udev depends on the sysfs file system which was introduced in the 2.5 kernel. It is sysfs which makes devices visible in user space. When a device is added or removed, kernel events are produced which will notify Udev in user space.


##2.udev
u 是指 user space ， dev 是指 device 
udev文件系统是针对2.6 内核，提供一个基于用户空间的动态设备节点管理和命名的解决方案。

创建Udev的目的Goals

- Run in user space. 
- Create persistent device names, take the device naming out of kernel space and implement rule based device naming.
- Create a dynamic /dev with device nodes for devices present in the system and allocate major/minor numbers dynamically.
- Provide a user space API to access the device information in the system.

###2.1 Udevd如何获取内核的这些模块动态变化的信息

设备节点的创建，是通过sysfs接口分析dev文件取得设备节点号，这个很显而易见。那么udevd是通过什么机制来得知内核里模块的变化情况，如何得知设备的插入移除情况呢？当然是通过hotplug机制了，那hotplug又是怎么实现的？或者说内核是如何通知用户空间一个事件的发生的呢？

答案是通过netlink socket通讯，在内核和用户空间之间传递信息。

The Udev daemon listens to the netlink socket that the kernel uses for communicating with user space applications. The kernel will send a bunch of data through the netlink socket when a device is added to, or removed from a system. The Udev daemon catches all this data and will do the rest, i.e., device node creation, module loading etc.
Udevd通过标准的socket机制，创建socket连接来获取内核广播的uevent事件 并解析这些uevent事件。

内核调用kobject_uevent函数发送netlink message给用户空间，这部分工作通常不需要驱动去自己处理，在统一设备模型里面，在子系统这一层面，已经将这部分代码处理好了，包括在设备对应的特定的Kobject创建和移除的时候都会发送相应add和remove消息，当然前提是你在内核中配置了hotplug的支持。

Netlink socket作为一种内核与用户空间的通信方式，不仅仅用在hotplug机制中，同样还应用在其它很多真正和网络相关的内核子系统中。


###2.2 Udevd如何监控规则文件的变更

如果内核版本足够新的话，在规则文件发生变化的时候，udev也能够自动的重新应用这些规则，这得益于内核的inotify机制， inotify是一种文件系统的变化通知机制，如文件增加、删除等事件可以立刻让用户态得知。

在udevd中，对inotify和udev的netlink socket文件描述符都进行了select的等待操作。有事件发生以后再进一步处理。

###2.3 系统冷启动阶段Udevtrigger的工作机制？

运行udevd以后，使用udevtrigger的时候，会把内核中已经存在的设备的节点创建出来，那么他是怎么做到这一点的？ 分析udevtrigger的代码可以看出：

udevtrigger通过向/sysfs 文件系统下现有设备的uevent节点写”add”字符串，从而触发uevent事件，使得udevd能够接收到这些事件，并创建buildin的设备驱动的设备节点以及所有已经insmod的模块的设备节点。


##3. HAL
The HAL gets information from the Udev service, when a device is attached to the system and it creates an XML representation of that device. It then notifies the corresponding desktop application like Nautilus through the Dbus and Nautilus will open the mounted device‚Äôs files.
它是一个位于操作系统和驱动程序之上，运行在用户空间中的服务程序。
它的目的是对上层应用提供一个统一的简单的查询硬件设备的接口。它所谓的抽象，基本上也就仅限于这一功能，它通常并不提供对硬件的实际操作，对硬件的操作，还是由应用程序来完成。
细化来说，除了提供标准的硬件查询接口，它甚至并不考虑如何对硬件进行配置，这不是它要完成的工作，但它确实提供了存储硬件配置相关信息的空间。下面我们会说到，那被称为属性。
所以，简单的说，你可以把HAL理解为：一堆的硬件列表以及他们的相关属性的集合。

HAL是Hardware Abstraction Layer的首字母缩写。不过Windows下的HAL和Linux下的HAL两者所指并非相同之物：
Windows下的HAL：位于操作系统的最底层，直接操作物理硬件，隔离与硬件相关的信息，为上层的操作系统和设备驱动程序提供一个统一的接口，起到对硬件的抽象作用。有了HAL，编写驱动程序就容易多了，因为HAL的接口不但使用简单，而且具有更好的可移植性（没用过）。
Linux 下的HAL：至于对硬件的抽象，Linux内核早就有类似机制，只不过没有专门的名称罢了。而Linux的HAL指的并非这个，它不是位于操作系统的最底层，直接操作硬件，相反，它位于操作系统和驱动程序之上，是一个运行在用户空间中的服务程序。

udev创建dev下的文件结点，加载驱动程序，让设备处于可用状态。而HAL则告诉应用程序，现在有哪些设备可用，这些设备的类型、特性和能力，让应用程序知道如何使用它们。详细地讲它提供以下几项功能：
- 获取指定类型的设备列表。
- 获取/更改设备的属性值。
- 获取设备具有的能力描述。
- 设备插入/拔除时，通知相关应用程序。
- 设备属性或能力变化时，通知相关应用程序。
设备的属性管理是HAL最重要任务之一，有的设备属性来源于实际的硬件，有的来源于设备信息文件(/usr/share/hal/fdi/)，有的来源其它配置信息(如/usr/share/hwdata/)。设备属性的都有标准的定义，这些属性定义是HAL的SPEC的主要内容之一，可以参考http://people.freedesktop.org/~david/hal-spec/hal-spec.html

###HAL是如何和udev建立联系的：
- udev只是一个框架，它的行为完全受它的规则所控制，这些规则存放在目录/etc/udev/rules.d/中，其中90-hal.rules是用来让udev把设备插入/拔除的事件通过socket socket:/org/freedesktop/hal/udev_event转发给HAL的。
- HAL挂在socket:/org/freedesktop/hal/udev_event上等待事件，有事件发生时就调用函数hald_udev_data处理，它先从事件中取出主要参数，创建一个hotplug_event对象，把它放入事件队列中，然后调用hotplug_event_process_queue处理事件。
- 函数hotplug_event_begin负责具体事件的处理，它把全部事件分为四类，并分别处理hotplug_event_begin_sysfs处理普通设备事件，hotplug_event_begin_acpi处理ACPI事件，hotplug_event_begin_apm处理APM事件，hotplug_event_begin_pmu处理PMU事件。要注意的是，后三者的事件源并非源于udev，而是在device_reprobe时触发的(osspec_device_reprobe/hotplug_reprobe_tree/hotplug_reprobe_generate_add_events/acpi_generate_add_hotplug_event)。

###HAL和应用程序之间的交互：
- 函数hotplug_event_begin_sysfs中，如果是插入设备，则创建一个设备对象，设置设备的属性，调用相关callouts，然后放入设备列表中，并触发signal让dbus通知相关应用程序。如果是拔除设备，则调用相关callouts，然后从设备列表中删除，并触发signal让dbus通知相关应用程序。
- 应用程序可以主动调用HAL提供的DBUS接口函数，这些函数在libhal.h中有定义。应用程序也可以注册HAL的signal，当设备变化时，HAL通过DBUS上报事件给应用程序。
- callout是HAL一种扩展方式，它在设备插入/拔除时执行。可以在设备信息文件中(/usr/share/hal目录)指定。
- addon也是HAL一种扩展方式，它与callout的不同之处在于addon往往是事件的触发者，而不是事件的消费者。HAL的事件源主要源于udev，而udev源于kernel的hotplug，然而有的设备如电源设备、磁盘设备和特殊按键等，它们并不产生hotplug事件。HAL就得不到通知，怎么办呢，addon就是用于支持新事件源的扩展方式。比如addon-acpi从/proc/acpi/event或者/var/run/acpid.socket收到事件，然后转发成HAL事件。addon-storage检测光盘或磁盘的状态，并设置设备的属性。addon-keyboard检测一些特殊按键，并触发相应事件。

简单的说，HAL就是一个设备数据库，它管理当前系统中所有的设备，你可以以多种灵活的方式去查询这些设备，可以获取指定设备的特性，可以注册设备变化事件。


##4. D-Bus
Dbus is like a system bus which is used for inter-process communication.

##5. APP
终结者对于 gnome 来说就是 gnome-volume-manager (名字太长了，下面简称 gvm)，它从 dbus 上探听消息，当发现有设备挂载提示的时候就会尝试把设备挂载上来。缺省的，还会打开一个 nautilus 浏览窗口来浏览新挂载上的分区的内容。




比较老的但经典的介绍hotplug： http://www.linuxjournal.com/article/5604

Kernel网站上一篇比较老的介绍udev  http://www.kernel.org/pub/linux/utils/kernel/hotplug/udev/udev.html


一些有关udev操作配置的教程
http://www.linuxsky.org/doc/admin/200710/139.html - pratical but not deep in mechanism

SUSE udev introduction: good
http://www.mpipks-dresden.mpg.de/~mueller/docs/suse10.1/suselinux-manual_en/manual/cha.udev.html#sec.udev.devdir

a good doc on how to config udev rule file
http://www.linuxformat.co.uk/wiki/index.php/Connect_your_devices_with_udev
http://ubuntuforums.org/showthread.php?t=168221




偏向于从代码一级进行解释
http://www.doc88.com/p-618600403098.html
Linux hotplug 从代码角度
http://www.embexperts.com/forum.php?mod=viewthread&tid=551

a good sample for writing a driver with UDEV
http://pete.akeo.ie/2011/08/writing-linux-device-driver-for-kernels.html


  



