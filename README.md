#osrusbfx2

Based on MS WDK osrusbfx2 samples + CY001 learning board, added more with my idea to demo learning writing host drivers for USB devices.

# 为何要写这个东东
诸位学习主机（host）侧USB驱动开发的同仁，想必在学习过程中一定也有鄙人相同的经历感受。那就是手头如果有一块USB的开发学习板，那效率一定会高很多。所以在下当时也是出于此目的在网上买了张佩大侠提供的配合《竹林蹊径：深入浅出windows驱动开发》的学习板CY001，以作学习的用途。

同时在学习USB驱动开发时，鄙人还有一个体会是：微软WDK中提供的例子也是最值得仔细研习的。USB驱动相关的例子中尤以osrusbfx2这个例子最为经典。osrusbfx2是配合著名的OSRFX2学习板精心编写的一套例子。非常适合Windows上学习USB驱动开发的朋友入门学习用。不光是入门用，平时也会常常参考。据说微软WDK开发小组的程序员也要经常用OSRFX2的板子来做测试呢（具体在哪里看到的忘记了），可见其重要性。

接下来，众所周知的原因，对于中国大陆的程序员来说OSR的那块板子是在是贵了点。所以我手上有了CY001之后就有了个想法：能不能将CY001改造一下，模拟成OSRFX2呢。它们两的物理基础实际上相差并不大啊。

再接下来就有了这个项目。所以如果你手头有CY001，又想学习WDK的OSRUSBFX2，那不妨试试阅读以下我写的这个玩意。其实在设备（Device）侧，如果你没有CY001，但只要有任何一块基于Cypress的FX2系列USB芯片的学习开发板（大陆做这种开发板的貌似不少，淘宝上可以搜搜），同时你对修改设备的固件也有兴趣，都可以按照相同的思路尝试以下。我的这个就当是抛砖引玉了吧。

# 目录结构
* osrfx2fw - CY001的固件Keil工程源代码。已经过修改模拟OSRFX2学习板
* windows  - Windows版本的OSRUSBFX2的源代码。已经过修改以适配CY001与实际的OSRFX2学习板的不同。
* linux - 鄙人正在研究Linux上的OSRFX2，如果有更新会放上来。（TBD）

更详细的说明可以参考各分目录下的说明README （TBD）

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

    
      


