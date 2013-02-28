#drvInst

# Description

Based on MS WDK difxcmd (\WinDDK\7600.16385.1\src\setup\DIFxAPI\DIFxCmd) & devcon (\WinDDK\7600.16385.1\src\setup\devcon) samples, demo how to write a device & driver installation application using DIFxAPI and SetupAPI.

# Building the Sample

Navigate to the drvinst directory and run the following command:

build -cZ

# Running the Sample

Once the sample is built, run the following commands. Before running the command, copy the drvinst.exe into the driver package folder - "osrusbfx2\windows\drivers"

drvInst.exe to print out how to use the command line tool

drvInst.exe /i osrusbfx2.inf, to re-install osrusbfx2.inf and osrfx2 devices if presented.

drvInst.exe /i osrusbfx2.inf 16, to install osrusbfx2.inf in legacy mode (accepting unsigned drivers)

drvInst.exe /p osrusbfx2.inf, to obtain the path to the installed osrusbfx2.inf

drvInst.exe /u osrusbfx2.inf, to uninstall devices from the devnodes no matter if they are presented or not and uninstall driver package after that.
    
# 为何要写这个东东
作为osrusbfx2例子的一部分，驱动和设备的安装也是一个十分重要的主题。
更详细的教程请看[device-and-driver-installation.md](./device-and-driver-installation.md)

# 目录结构

# 联系方式
unicorn_wang@outlook.com

p.s. 该项目已经上传到GitHub，大家如果熟悉GitHub也可以上Github观赏并联机提出宝贵意见。地址是: https://github.com/unicornx/osrusbfx2

下载该项目的方法：

mkdir yourworkspacefolder  
cd yourworkspacefolder  
git clone https://github.com/unicornx/osrusbfx2.git

enjoy it! :)

    
      


