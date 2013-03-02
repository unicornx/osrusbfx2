#osrfx2fw
参考了CY001的固件Keil工程源代码。已经过修改来模拟OSRFX2学习板。

# 参考文献

[Using the OSR USB FX-2 Learning Kit - OSR Online] : OSRUSBFX2学习板的DataSheet

# 为何要写这个东东
诸位学习主机（host）侧USB驱动开发的同仁，想必在学习过程中一定也有鄙人相同的经历感受。那就是手头如果有一块USB的开发学习板，那效率一定会高很多。所以在下当时也是出于此目的在网上买了张佩大侠提供的配合《竹林蹊径：深入浅出windows驱动开发》的学习板CY001，以作学习的用途。

同时在学习USB驱动开发时，鄙人还有一个体会是：微软WDK中提供的例子也是最值得仔细研习的。USB驱动相关的例子中尤以osrusbfx2这个例子最为经典。osrusbfx2是配合著名的OSRFX2学习板精心编写的一套例子。非常适合Windows上学习USB驱动开发的朋友入门学习用。不光是入门用，平时也会常常参考。据说微软WDK开发小组的程序员也要经常用OSRFX2的板子来做测试呢（具体在哪里看到的忘记了），可见其重要性。

接下来，众所周知的原因，对于中国大陆的程序员来说OSR的那块板子实在是贵了点。所以我手上有了CY001之后就有了个想法：能不能将CY001改造一下，模拟成OSRFX2呢。它们两的物理基础实际上相差并不大啊。

再接下来就有了这个项目。所以如果你手头有CY001，又想学习WDK的OSRUSBFX2，那不妨试试阅读以下我写的这个玩意。其实在设备（Device）侧，如果你没有CY001，但只要有任何一块基于Cypress的FX2系列USB芯片的学习开发板（大陆做这种开发板的貌似不少，淘宝上可以搜搜），同时你对修改设备的固件也有兴趣，都可以按照相同的思路尝试以下。我的这个就当是抛砖引玉了吧。

# 基于CY001模拟OSRUSBFX2
对照[Using the OSR USB FX-2 Learning Kit - OSR Online], 结合CY001做的主要改动总结：

- **1.Configurations**： 与OSRFX2保持一致

- **2.Interfaces**： 与OSRFX2保持一致

- **3.Endpoints**： 对EP6(Bulk-OUT)和EP8(Bulk-IN)功能定义保持一致。
	对EP1(Interrupt-IN)的功能定义有修改。OSRFX2利用EP1上报switch状态的功能。CY001没有switch但有四个方向按钮。为了演示通过Interrupt端点通知主机的功能，在CY001上我们使用四个方向按钮模拟四个方向的移动并利用EP1来上报四个方向的位移。

- **4.Board LED Displays**: 

 - ***4.1 7-Segment LED display***: CY001上也有，与OSRFX2保持一致
 - ***4.2 LED Bar Graph Display***： CY001上只有4个，而OSRFX2上有8个

- **5.Vendor Commands**:

 - 0xD4 – READ 7 SEGMENT DISPLAY： 目前还没有实现
 - 0xD6 – READ SWITCHES： CY001上没有开关，不予支持
 - 0xD7 – READ BARGRAPH DISPLAY： 支持，但CY001上只有4个
 - 0xD8 – SET BARGRAPH DISPLAY: 支持，但CY001上只有4个
 - 0xD9 – IS HIGH SPEED: 目前还没有实现
 - 0xDB – SET 7 SEGMENT DISPLAY: 目前还没有实现


# 联系方式
unicorn_wang@outlook.com

p.s. 该项目已经上传到GitHub，大家如果熟悉GitHub也可以上Github观赏并联机提出宝贵意见。地址是: https://github.com/unicornx/osrusbfx2

该项目另一个发布的平台在“驱动开发网” - "基于WDK的OSRUSBFX2写了点例子代码，有兴趣的可以看看"
http://bbs3.driverdevelop.com/read.php?tid-125258.html

# 下载该项目的方法：

mkdir yourworkspacefolder  
cd yourworkspacefolder  
git clone https://github.com/unicornx/osrusbfx2.git

enjoy it! :)

[Using the OSR USB FX-2 Learning Kit - OSR Online]: http://www.osronline.com/hardware/osrfx2_32.pdf
      


