在Ubuntu环境下操作CY001

Step1 安装VirtualBox， 同时在VirtualBox中安装windows系统作client

Step2 使能USB检测：
1） 安装VirtualBox Guest Additions的安装包
2） 在VirtualBox的USB设置中，使能USB Controller，使能USB2.0 （EHCI）Controller
3） 添加Filter，除了名字，其他全部设为缺省值

Step3 在windows系统（client）中安装Guest Additions