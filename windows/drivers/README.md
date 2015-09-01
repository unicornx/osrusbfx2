#drivers

# Description

驱动安装包。目前只提供了x86的版本。

# Running the Sample

1) 卸载驱动包并清除系统中已经配置的设备节点
drvInst.exe /u osrusbfx2.inf

2) 安装驱动包，如果当前有osrfx2设备连接在电脑上则安装之并配置设备节点。由于这里提供的驱动未提供WHQL证书，所以在XP上需要带参数16。在Vista以及Win7上则需要切换到Test模式下才可安装。
drvInst.exe /i osrusbfx2.inf 16

更详细的介绍请看[device-and-driver-installation.md](../drvinst/device-and-driver-installation.md)

# 联系方式
**Email: unicorn_wang@outlook.com**  
**Blog:  http://unicornx.gitcafe.io/**

p.s. 该项目已经上传到GitHub，大家如果熟悉GitHub也可以上Github观赏并联机提出宝贵意见。地址是: https://github.com/unicornx/osrusbfx2

下载该项目的方法：

mkdir yourworkspacefolder  
cd yourworkspacefolder  
git clone https://github.com/unicornx/osrusbfx2.git

enjoy it! :)

    
      


