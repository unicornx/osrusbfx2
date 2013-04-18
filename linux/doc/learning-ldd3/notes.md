学习笔记 ldd3

#Chapter2:#
按照书上的例子hello的说法会在终端显示“Hello, world”。但是运行后什么都没有出现 （原因不解）。

对ubuntu下控制台无法显示printk的解释：

=================================================  
printk 是内核的调用接口，它在系统init之前，把消息写往控制台，但是一旦系统init之后，便改写到系统的日志中。因为init之后用户所能够感受到的进程(拥有控制台)都是用户空间的进程,而用户空间的进程是无法反映内核数据的,除非通过一定的方式(如proc文件,netlink...)向内核申请相关的信息,并在用户空间反映出来.这样做的好处不言而喻的，如果内核在任何的时候都可以写信息到控制台，那控制台一定会被这样的信息淹没，而无法工作。

系统初始化进程(pid=0),工作在内核空间,它在启动init进程(pid=1)之前,把所有的信息通过printk内核方法写往控制台.printk把欲打印的日志消息,首先首先保存到内核的日志缓冲区之中.而后申请控制台的信号量，如果申请到，则调用控制台写方法，写控制台.而此时系统中并没有打开控制台,故而初始化进程,可以申请到控制台的信号量.当系统初始化到达一定阶段后,便会启动init进程(pid=1),并在此之前打开控制台,控制台的信号量增加,此后，printk便无法申请到信号量,而无法写数据到控制台.转而通过和用户空间的进程协作把内核的日志消息写到系统道的日志文件之中.-------前台进程通过sys_log系统调用读出，并根据配置文件sys_conf写向相应的日志文件或/var/log/messages文件中。

读printk原码可知流程。

可以切换到linux提供的终端来看到tty下的输出， 这个就很简单了，直接按Alt+Ctrl+F1~F6
Alt+Ctrl+F7即可退出返回GNOME
=================================================

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
console_loglevel: the current level below which the messages are displayed in console  
default_message_loglevel: the default level for messages without a specified priority when you call printk  
minimum_console_loglevel: the minimum level allowed for console messages //? don't know what it is used, when I see it is "1", but if I call printk with 0, still output  
default_console_loglevel: the level used by the command 'enable printk's to console' in syslog(2).   
以上对应console_printk变量

to quick open console printk  
echo 8 > /proc/sys/kernel/printk

##通过监视调试##
strace命令的使用，它可以显示由用户空间程序发出的所有系统调用。不仅显示调用，还能显示调用参数以及用符号形式表示的返回值。







