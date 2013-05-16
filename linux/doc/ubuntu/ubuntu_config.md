#Operation cheatsheets
1) Gnome快捷键
http://wiki.ubuntu.org.cn/Gnome%E5%BF%AB%E6%8D%B7%E9%94%AE

2) Mount other partition and make link on desktop  
.sudo fdisk -l
df -lh // but this can only display Linux partitions.
Refer to http://wiki.ubuntu.org.cn/%E6%8C%82%E8%BD%BDWindows%E5%88%86%E5%8C%BA
edit /etc/fstab
add one line
/dev/hda1       /media/partitionname  ntfs    nls=utf8,umask=000 0       0

3) how to get root:   
sudo su -> switch to current user env and get root
sudo su - -> switch to root env and get root
no need to directly run "su"

4) Create shorcut on desktop
on how to create a shortcut/launcher on ubuntu, a step by step @ http://www.liberiangeek.net/2010/09/create-desktop-launchersshortcuts-ubuntu-10-0410-10-maverick-meerkat/
Actually this will create a .desktop file in ~/Desktop dir, we can modify this file by ourselves.
More details about the syntax of desktop entry, read: https://developer.gnome.org/desktop-entry-spec/

#Install Software applications:
0) apt
https://help.ubuntu.com/community/AptGet/Howto
  
1) Install chinese input:  
system->administration->language Suppoort
system->preferences->IBUS perference->launch IBUS

2) read PDF:  
install Acrobe Reader, goto official web site to download and install

3) read chm:  
KchmViewer
sudo apt-get install kchmviewer

4) RAR
fix chinese zipped file display issue for rar archivements

5) install eclipse and CDT // till now I am still not familiar with this edit tool  
If you choose to install default EClipse and CDT
From  ubuntu 10.04
sudo apt-get install eclipse
open eclipse->Help->Install New Software-> Input "CDT" in "Wor with" textbox and select C++/C Development Plugin component to install

6) install file compare/diff tools  
sudo apt-get install meld

7) Install sourceinsigt:  
sudo apt-get install wine
wine /tmp/sourceinsightinstall.exe
more details refer to http://hi.baidu.com/zongjunzhou/item/4596a90f45b077193a53eeaa

8) install usbview:  
Refer to http://packages.ubuntu.com/zh-tw/hardy/usbview
select i386 and goto http://packages.ubuntu.com/zh-tw/hardy/i386/usbview/download

read the page and do:  
*) add software source - deb http://mirrors.163.com/ubuntu/ hardy main universe  
*) sudo apt-get install usbview
note: this usbview is v1.0, to make it work on kernel 2.6.32, we have to configure the tool as below everytime we launch it.

refer http://www.kroah.com/linux-usb/
refer to http://ubuntuforums.org/showthread.php?t=1610682, modify the config in usbview
Replace:
"/proc/bus/usb/devices" with "/sys/kernel/debug/usb/devices"

To avoid upon, we have to upgrade usbview to 2.0, but due to 2.0 depends on GTK3, refer to 
http://www.2cto.com/os/201303/198703.html on how to install a new usbview
But Ubuntu 10.04 is based on GNOME 2, so we have no way to install GTK3 and cannot install latest usbview2 on ubuntu10.04 now.  
*** lsusb is also a good tool as usbview but with command line.

9) install VitrualBox on Ubuntu:  
install XP in client machine
need to use PQ create partitions first then install Ghost else will fail
to share the folder, need to install Guest Additions first, Devices -> Install Guest Additions
to access usb, add your user to the vboxusers group: sudo usrmod -a -G vboxusers youraccount, re-log and activate this

10) install ReText:  
sudo add-apt-repository ppa:mitya57
sudo apt-get update
sudo apt-get install retext

11) Install IDLE for Python:  
sudo apt-get install idle

12) install git 
To support git work with github, we should upgrade git to 1.7.10 and upper, refer to https://help.github.com/articles/https-cloning-errors
refer this: http://ubuntuforums.org/showthread.php?t=2061057 to upgrade ur git

13) when you met some issue in apt-get
*) "E: Dynamic MMap out of room!"
http://www.linuxquestions.org/questions/debian-26/dynamic-mmap-out-of-room-160966/
open /etc/apt/apt.conf.d/70debconf
added one line:
APT::Cache-Limit "55000000000"; 

*) "The following packages have unmet dependencies"
http://askubuntu.com/questions/261600/the-following-packages-have-unmet-dependencies
dpkg --purge --force-depends
git apt-get clean

14) About GTK
http://stackoverflow.com/questions/10548924/starting-with-gtk-on-ubuntu
That package(GTK3) is not available for ubuntu 10.04,It is the development libraries of GTk3 whereas 10.04 is a Gtk2 distro, So you can't install it in 10.04
