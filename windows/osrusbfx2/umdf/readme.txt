This directory contains five different components.  

    FX2_driver - contains a function driver that can run on the OSR USB-FX2
                 Learning kit device.  This driver provides similar
                 functionality to the KMDF OSRUSBFX2 sample driver.

    FX2_driver\exe - contains a modified version of the OSRUSBFX2 test app
                     that demonstrates the additional UMDF only functionality
                     of the Fx2 driver.

    Echo_driver - contains a function driver that can run on the OSR USB-FX2
                  Learning kit device.  This driver provides similar 
                  functionality to the Echo driver sample.

    Filter - contains a user-mode filter driver (and an INF to install it on
             along with the Driver sample above) which demonstrates how to 
             transform the data in read and write operations.

    Exe    - a test application that works with the echo and filter drivers in 
             this directory to send I/O reqests to the OSR USB-FX2 device.

