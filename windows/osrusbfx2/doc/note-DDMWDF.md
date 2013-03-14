Developing Drivers with the Microsoft Windows Driver Foundation  
Penny Orwick  
Guy Smith  

学习笔记


# Chapter2：Windows Driver Fundamentals  

## What Is a Driver?

Figure 2-1, 描述了驱动是如何与Windows其他模块整合在一起协同工作的。
几个基本概念：
Device Objects 
Device Stack
Device Tree

##The Windows I/O Model  
这里有一段比较精彩的描述异步IO的
对于Applications，其调用Windows API的时候有同步和异步调用的区别。但是对于Drivers这一层，实际上是不用区分的,在内部统一为异步的方式。驱动要做的事情就是当它从I/O Manager收到一个处理请求时，如果可以立即处理完毕就立即返回，否则就做完必要的处理后先返回并通知I/O Manager当前请求没有结束。此时线程被内核子系统释放，可以去处理其他新的请求。当设备处理结束后驱动会被激活（中断？），然后I/O Manager, Applicaiton 会被依次中断唤醒。关键是中间有一个I/O Manager层，（kernel子系统），它会判断Applicaitons调用的API是同步还是异步，如果是同步则I/O Manager会一直等待Drivers返回，如果是异步则I/O Manager会立即返回。
此时有一个线程的处理机占用时序图会更好地描述。

I/O Requests
I/O Request的分类



http://msdn.microsoft.com/en-us/library/windows/hardware/ff565734(v=vs.85).aspx

The I/O manager has two subcomponents: the Plug and Play manager and power manager. They manage the I/O functionality for the technologies of Plug and Play and power management. 

## Basic Kernel-Mode Programming

### Interrupts and IRQLs
中断的IRQL
处理器的IRQL
中断服务程序的IRQL
当某个中断服务程序被调度到某个处理器上执行时，则该处理器的IRQL被设置为该中断的IRQL，如果已经相同则保持不变。

PASSIVE_LEVEL是唯一和中断没有关系的Level，所有的中断都不可以设置为这一级别。所有的用户态程序和低优先级的驱动函数都运行在这一级别。
DISPATCH_LEVEL：
DIRQL：或者叫Device IRQL，即硬件设备触发的中断。级别最高。

### Concurrency and Synchronization
并发与同步

内核模式的驱动通常自己不会创建线程，系统一般也不会创建专门的线程来运行驱动的函数。驱动程序的函数代码一般会运行在两种线程上下文里：
一种是当一个应用程序发起一个I/O请求时，此时该请求必然发起自一个应用的线程里。
还有一种，也是大部分的情况是线程的函数会运行在系统随意借用的一个线程上下文，所谓借用的方式主要发生在中断上下文中，当中断发生并呼叫到某个驱动程序的处理函数时，此时处理器上正在运行那个线程是不可预知的，OS直接借用当前处理器上运行的线程的上下文并执行我们驱动的代码。

书中给出的例子也是比较典型的。







