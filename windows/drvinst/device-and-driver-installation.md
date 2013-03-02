# Windows设备和驱动的安装
MSDN上有关设备和驱动安装介绍的总入口 [Device and Driver Installation (Windows Drivers)]

# 1. Windows上和驱动相关的一些重点概念：
MSDN上有关知识点和一些基本概念的介绍分散在各个章节里，这里只是把我认为比较重要的，需要经常回顾的知识点罗列在这里，以供经常回顾和参考。

- [devnode]
- [hardware ID]
- [Driver Package]
- [INF file]
- [catalog file]
- [Device Installation Applicaiton]


# 2. Windows上设备以及驱动安装的相关模块

（经典的关系图） [Device Installation Components (Windows Drivers)]

驱动和设备安装中涉及到的模块由两大部分组成。一部分是由IHV和OEM提供，另外一部分由Windows操作系统提供

## __由IHV和OEM（可以简单理解为设备端驱动开发人员）提供的模块__
[Vendor-Provided Device Installation Components (Windows Drivers)]

泛泛来讲，为了支持某种设备，所有由驱动和设备开发人员提供的东西加在一起我们可以统一叫做一个[Driver Package]。所以MSDN图中的[Driver Package]表达的更多的是下面所描述的“驱动文件”和“与安装相关的文件”

细分起来[Driver Package]由以下几个部分组成。更消息的描述可以参考[Components of a Driver Package]

___* 驱动文件（Driver Files）___

就是我们平时看到的.sys文件,一个或者多个。Windows会在device被安装时copy一份sys文件到"%SystemRoot%\system32\drivers"

___* 与安装相关的文件（Installation Files）___

- [INF file]: 有且只有一个。就是我们平时看到的.inf文件，该文件是必须的,用于描述设备安装设置的详细信息。e.g. osrusbfx2\windows\drivers\x86\osrusbfx2.inf
- [catalog file]: 有且只有一个。就是我们平时看到的.cat文件，它提供了对整个driver package的数字签名

___* 其他文件___

[Driver Package]中除了驱动文件和安装相关文件外的部分都归到这里，主要包括:

- [Device Installation Applicaiton]: 
- [Class Installers], 可选，这里我们暂不介绍因为osrusbfx2没有用到
- [Class Co-Installers], 可选，这里我们暂不介绍因为osrusbfx2没有用到
- [Device Co-Installers], 可选，这里我们暂不介绍因为osrusbfx2没有用到
- ......

## __由Windows操作系统提供的模块__ 
[System-Provided Device Installation Components (Windows Drivers)]

- [PnP Manager]

	1) 系统booting过程中设备的检测和枚举

	2) 系统running过程中设备的添加和删除

- [SetuoAPI]: 
- [Configuration Manager API]
- [Driver Store] 从Vista开始，操作系统只会允许安装本地维护的一个系统目录中的[Driver Package]，这个系统目录就定义为[Driver Store]。

	一般情况下，设备驱动的开发人员都会提供一个驱动的安装包，这个安装包的工作就是将[Driver Package]拷贝到本地的一个目录下，在[Driver Package]中的文件可以被系统用来安装设备之前，这些文件首先应该已经被___Staging___进了[Driver Store]. (所谓的___Staging___，就是将[Driver Package]中的所有的文件，包括[INF file]以及[INF file]引用到的所有文件拷贝到[Driver Store]的过程，似乎可以将[Staging]翻译为“入场”或者“上台”。)

	一个[Driver Package]只有通过系统的验证和检查合格后才会被___Staging___入[Driver Store]，这些检查包括:
   
	1) 通过数字签名检查[Driver Package]是否是完整可信的
   
	2) 当前用户是否有权安装驱动

	3) [INF file]语法正确，[INF file]引用的文件都存在于[Driver Package]之中

	在[Driver Package]被___Staging___入[Driver Store]后，操作系统就可以使用[Driver Package]来自动安装新设备而不用用户的帮助了。

	[Driver Store]的位置:
	
	* XP: C:\WINDOWS\system32\DRVSTORE

	* Vista & Win7: C:\Windows\System32\DriverStore\FileRepository

- [Device Manager]
- [Hardware Wizards]

# 2. 驱动的安装
严格地讲，我们平时说的驱动的安装的概念应该分两大部分来讨论。
一个是[Driver Package]的**预安装**，也就是MSDN上所谓的***Preinstall***，预安装会将我们的inf,sys等等相关文件从我们自己的安装目录拷贝到Windows系统自己维护的驱动仓库中[Driver Store]。还有一个是设备的**配置**。MSDN上的官方说法是***configuration of devices***, 该动作发生的前提条件是设备连上主机PC。主要是由[Pnp Manager]根据[INF file]为设备创建设备节点[devnode]。这是两个不同的概念和过程，而且设备能够被配置必然发生在[Driver Package]被预安装之后-即系统上已经存在**匹配（match）**的驱动安装文件，不管这些文件是Windows安装时预置的(inbox)，还是我们自己提供的后来预安装上去的。

# 3. 驱动的卸载
可以重点看一下 [Uninstalling Devices and Driver Packages (Windows Drivers)]
类似的，卸载也包括两个方面。一个是[Driver Package]的**卸载**，也就是将Windows驱动仓库中的我们的inf,sys等文件移除掉。但注意这不会动到我们自己的安装目录。还有一个就是将系统中注册的[devnode]**卸载**掉，这里的卸载不考虑设备此时是否连接在PC上。

# 4. 驱动安装和卸载程序

如果我们希望定制安装和卸载过程则需要写一个专门的[Device Installation Applicaiton]。具体怎么写可以参考[Writing a Device Installation Application (Windows Drivers)]

一个真正商用化的[Device Installation Applicaiton]必须要兼容Windows的AutoRun机制，这主要是为了支持[hardware-first installation]过程，在该过程中如果Windows发现无法在本地找到一个匹配的驱动就会进入Found New Hardware Wizard流程并且在此过程中Windows还是无法自动帮助用户找到可安装的驱动，最后势必会提示要求用户提供安装盘，则AutoRun会调用安装盘里预置的[Device Installation Applicaiton]

同时还要考虑两种典型的用例：

**[software-first installation]**: 

所谓软件优先，指的是,在PC上还未插入过我们驱动需要支持的设备之前，先将[Driver Package]预安装(__PreInstall__)到电脑上。这样当下次设备插入时，Windows就会自动根据设备的[hardware ID]找到我们预先安装的驱动并为我们的设备安装驱动。

**[hardware-first installation]**:

所谓硬件优先，指的是：在我们最新的[Driver Package]还未被**预安装**到电脑上之前，我们的设备就被插入了。
此时有两种情况：

- 1）Windows发现本地有驱动匹配，那么就立即安装它。这样的驱动主要指的是Windows安装盘自带的inbox驱动或者是通过以前的[software-first installation]过程Preinstall的驱动。总之一旦本地有可以安装的驱动，Windows就不会尝试提示用户提供安装媒介或者发起Windows Update过程去获取更新的驱动了。
- 2）如果在本地没有发现匹配的驱动，Windows就会激活Found New Hardware Wizard流程。

# 5. Sample - drvinst

drvinst示例参考了WDK的例子DIFxCmd和devcon，主要演示了：

- 如何使用[DIFxAPI]-DriverPackageInstall**预安装**[Driver Package]，注意缺省情况下如果调用DriverPackageInstall的时候有设备连接在PC上则DriverPackageInstall也会执行设备的**配置**。这非常方便，因为DriverPackageInstall内部封装了更复杂的逻辑，这些逻辑明显是对一些[SetupAPI]的调用。
- 如何使用[DIFxAPI]-DriverPackageUninstall对[Driver Package]进行**卸载**
- 如何使用[SetupAPI]对设备节点进行**卸载**。

这里重点讲一下DriverPackageInstall在缺省参数情况下的行为（Flags参数为0）。其他参数的用法大家可以自己尝试。

- 1) 目前PC上没有连接我们的设备，以前也没有连接过我们的设备。
缺省情况下DriverPackageInstall会将我们的[Driver Package]预安装到[DIFx driver store]

- 2) 目前PC上没有连接我们的设备, 但以前连接过我们的设备[nonpresent device]，上次连接安装的驱动比我们要安装的驱动更匹配(match)
缺省情况下DriverPackageInstall会将我们的[Driver Package]预安装到[DIFx driver store]，因为上次连接安装的驱动比我们要安装的驱动更match，所以该函数不会设置CONFIGFLAG_REINSTALL标记，这样如果同样的设备连接到它上次连接过的同样的端口上，Windows并不会给它更新我们的驱动。如果我们希望对以后的设备都使用我们新安装的驱动，可以使用DRIVER_PACKAGE_FORCE标志。

- 3) 目前PC上没有连接我们的设备, 但以前连接过我们的设备，我们要安装的驱动比上次连接安装的驱动更匹配(match)
缺省情况下DriverPackageInstall会将我们的[Driver Package]预安装到[DIFx driver store]，同时对以前连接的设备节点设置CONFIGFLAG_REINSTALL标记，这样如果同样的设备连接到它上次连接过的同样的端口上，Windows会给它更新我们的驱动。

- 4) 目前PC上连接有我们的设备, 当前安装的驱动比我们要安装的驱动更匹配(match)
缺省情况下DriverPackageInstall会将我们的[Driver Package]预安装到[DIFx driver store],因为当前安装的驱动比我们要安装的驱动更match，所以该函数不会给连接的设备更新我们的驱动。

- 5) 目前PC上连接有我们的设备, 我们要安装的驱动比当前安装的驱动更匹配(match)
缺省情况下DriverPackageInstall会将我们的[Driver Package]预安装到[DIFx driver store],因为我们要安装的驱动比当前安装的驱动更匹配match，所以该函数会给连接的设备更新我们的驱动。

[Driver Package]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff544840(v=vs.85).aspx
[INF file]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff547402(v=vs.85).aspx
[catalog file]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff537872(v=vs.85).aspx
[class installer]: http://not-defined
[co-installer]: http://not-defined
[device driver]: http://not-defined
[Device Installation Applicaiton]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff556277(v=vs.85).aspx#wdkgloss.device_installation_application
[PnP Manager]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff728837(v=vs.85).aspx
[SetuoAPI]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff550855(v=vs.85).aspx
[Driver Store]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff544868(v=vs.85).aspx
[DIFx driver store]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff543652(v=vs.85).aspx
[Device Manager]: http://not-defined
[Hardware Wizards]: http://not-defined
[nonpresent device]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff556313(v=vs.85).aspx#wdkgloss.nonpresent_device
[software-first installation]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff552293(v=vs.85).aspx
[hardware-first installation]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff546139(v=vs.85).aspx
[Components of a Driver Package]:(http://msdn.microsoft.com/en-us/library/windows/hardware/ff539954(v=vs.85).aspx) 
[Vendor-Provided Device Installation Components (Windows Drivers)]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff728856(v=vs.85).aspx
[System-Provided Device Installation Components (Windows Drivers)]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff728855(v=vs.85).aspx
[Device and Driver Installation (Windows Drivers)]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff549731(v=vs.85).aspx
[Device Installation Components (Windows Drivers)]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff541277(v=vs.85).aspx
[hardware ID]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff546152(v=vs.85).aspx
[Uninstalling Devices and Driver Packages (Windows Drivers)]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff553525(v=vs.85).aspx
[devnode]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff556277(v=vs.85).aspx#wdkgloss.devnode
[Writing a Device Installation Application (Windows Drivers)]: http://msdn.microsoft.com/en-us/library/windows/hardware/ff554015(v=vs.85).aspx

