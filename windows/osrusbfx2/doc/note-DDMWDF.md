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


### Memory
用户模式下的进程和内核模式下的驱动都工作在虚拟内存地址上下文中。但内核模式下的内存使用方式有以下几点和用户模式下是不同的。  
1. 内核模式下的驱动模块共享同一个虚拟地址空间，实际上操作系统的内核模块也使用这个虚拟地址空间。所以不恰当的驱动代码操作有可能破坏操作系统内核的运行，导致系统崩溃-蓝屏。用户模式下每个进程有自己的私有的虚拟内存空间，所有本进程的线程是共享这个虚拟地址空间的。  
2. 用户模式的进程代码不可以访问内核的地址空间  
3. 内核模式的过程可以访问用户模式下的地址空间，但操作必须十分小心。  

内核堆（内存页池）
内核驱动可以象用户模式下的代码一样为持久存在的数据申请堆空间。但内核模式的代码申请的堆和用户模式下不同。  
为了区分对待内核模式下页错误可能引起的问题，内核驱动有两种申请堆内存的机制：  
1. 可交换内存页池  
2. 不可交换内存页池。在这个池中申请的内存页将永久驻留内存不会被交换到物理硬盘上，主要用于储存那些可以被DISPATCH中断级别及以上过程访问的数据。  
3. 

内核栈：
内核过程可以使用的栈空间比用户模式下进程可用的栈空间小的多，所以内核驱动代码必须要非常小心节省地使用栈。  
1. 在内核驱动代码中尽量避免递归。
2. The disk I/O path is very sensitive to stack usage, because it's a deeply layered path and often requires the handling of one or two page faults.（不是很理解这个）  
3. 驱动代码中传递大数据结构对象时一般不推荐使用栈。我们推荐的方式是申请不可交换的内存页，然后传递指向这些内存的地址指针。由于内核代码共享同一个内核地址空间，所以这么做也是很方便的。  

MDLs： 还没搞清楚


# Chapter3: WDF Fundamentals

WDF为各种各样的设备类型提供了一个统一的驱动模型，这个模型的最最重要的特性包括：  
# ***支持内核态的驱动也支持用户态的驱动***

# ***提供了一个设计良好的对象模型***
（详细参考Chapter5）。
WDF将Windows的内核数据结构和与驱动操作相关的数据结构封装为对象，such as device, driver, I/O request, queue, and so on。有些对象是由FrameWork创建并传递给WDF驱动使用，其他的对象可以由驱动代码根据自己的需要创建，使用并删除。我们驱动要做的一件很重要的事情就是要学会和这些WDF对象打交道并把他们组织起来为实现我们设备的特定功能服务。  

## 1. 对象的编程接口

	* 每个对象有属性(Properties),对属相的访问要通过方法(Methods)。
	* 每个对象提供了操作方法(Methods)，WDF驱动通过调用这些函数操作对象的属性或者指示对象动作。  
	* 每个对象支持事件 (Events)。实现上是回调函数。FrameWork提供了对这些事件的缺省处理，我们的驱动不需要去关心所有的回调函数，但是如果我们的驱动希望对某个事件添加自己的特定于设备的处理时（这总是需要的），则可以注册这个事件的回调函数。这样当系统状态改变或者一个I/O请求发生时FrameWork就会调用驱动注册的回调函数，并执行我们驱动自己的代码，可以类比于C++中的重载一样。  

## 2. 在对象模型中提供了一套设计精巧的对象依附关系
这套关系可以帮助我们的驱动代码简化对对象生命周期的管理，具体来说，如果一个对象B是依附于等级A，那么当A被删除时，FrameWork会自动删除对象B。就像封建社会的主人和仆从的关系，主子完蛋的时候，仆从也要跟着一起玩完。  

	* Driver对象的级别最高，是WDF对象这棵大树的根，也是FrameWork为我们的驱动创建的第一个对象  
	* 有些对象必须依附于Device对象，或者是Device对象的仆从对象的仆从。比如Queue对象，都依附于Device而存在。  
	* 有些对象可以有多个主子，比如Memory对象。

对象模型并可以简化I/O请求的同步操作。（还没有完全理解）


# ***定义了一个I/O模型***
FrameWork做为OS和驱动之间的一个中间层，当Windows向WDF驱动发送I/O请求时，FrameWork首先收到该请求，并决定是由FrameWork自己处理还是调用驱动注册的回调函数进行处理。如果需要交给驱动处理，则FrameWork就要进入利用一套机制代表驱动进行I/O请求的分发，排队，结束和取消操作。请注意这里FrameWork并不是简单地将I/O请求全权交给驱动进行处理的，而是通过内建了一套完善的PnP和Power事件处理以及状态管理机制来跟踪所有I/O请求，并在合适的时间点上呼叫驱动注册的回调处理函数。这些时间点包括系统电源状态的变化（比如进入休眠，从休眠退出）；设备和系统之间的连接发生变化（比如设备连上主机，或者设备从主机上断开）等等。这么做可以极大地减轻原来驱动处理的负担，并代替驱动处理好了复杂的同步处理。
不需要跟踪管理复杂的I/O状态、事件,也不需要太多顾虑事件的同步，基于WDF的驱动要做的事情就只剩下创建合适的队列和注册定义在队列上的事件处理回调函数并加入自己的处理代码就可以了。  
WDF创建队列时需要对队列进行仔细的设置，这样FrameWork才会更好地满足我们驱动的需要来为我们服务。对于驱动创建的每一个对象，我们要配置好：  
1. 该队列可以处理的I/O请求的类型。  
2. FrameWork应该怎样将队列上的I/O请求分发给WDF驱动。Parallel，Sequential还是Manual。  
3. 当PnP和Power事件发生时FrameWork应该如何处理队列，包括启动，停止还是恢复队列。




# ***提供了内建的对PnP和Power的管理***
对状态的管理和迁移提供了缺省操作。  








