//-----------------------------------------------------------------------------
// A How-To for getting the OSR USB FX2 device to suspend and resume 
// within the Linux USB power managemnt (PM) framework.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// While the below may seem to be a hack, it appears to be recognized and
// sanctioned by the devlopers on the linux-usb-devel list.
// See the thread titled "USB PM requirements", circa the 2006-06-13 
// timeframe. This thread (appears to me to) acknowledge this state of PM 
// for USB on Linux.
//
// Many of the discussions lately (late summer 2006) on linux-usb-devel
// are centered on a new feature: autosuspend and autoresume.
// From the threads, this will allow the device driver to determine when
// the device should be suspended or resumed, rather than the current
// external method based on the power/state file of the device.
// As soon as a major distro (hopefully Ubuntu) picks up the autosuspend code,
// changes will be made in the osrfx2 driver to use these features.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// NOTE:  It is best to test these PM precedures on "real" systems, not a VM.
//         
//        Under VMware 5.0 (running Ubuntu), the following procedure does not
//        appear to work.  The dmesg log shows the osrfx2 suspend and 
//        resume functions being driven but the device does not indicate it
//        has suspended, e.g. the 7-segment display does not show an "S".
//-----------------------------------------------------------------------------

//
// In this example the OSR USB FX2 device is connected (located) on 
// bus 4-4 as 1.0 (e.g. 4-4:1.0) under sysfs.
//
robin@shinythings:~$ ls -l  /sys/bus/usb/devices/4-4:1.0/power
total 0
-rw-r--r-- 1 root root 4096 2006-08-30 11:41 state
-rw-r--r-- 1 root root 4096 2006-08-30 11:41 wakeup

//
// To enable general user-mode access to the hub and device power files.
//
robin@shinythings:~$ sudo chmod 666 /sys/bus/usb/devices/4-4/power/*
robin@shinythings:~$ sudo chmod 666 /sys/bus/usb/devices/4-4:1.0/power/*

//
// To suspend the OSR USB FX2 device:
//    suspend the osrfx2 device
//    suspend the parent hub/port connecting osrfx2
//
// The 7-segment LED should now display an "S".
//
robin@shinythings:~$ echo -n 3 > /sys/bus/usb/devices/4-4:1.0/power/state
robin@shinythings:~$ echo -n 3 > /sys/bus/usb/devices/4-4/power/state

//
// To resume the OSR USB FX2 device.
//    resume the parent hub/port connecting osrfx2
//    (no need to resume osrfx2 device explicitely)
//
// The 7-segment LED should now display an "A".
//
robin@shinythings:~$ echo -n 0 > /sys/bus/usb/devices/4-4/power/state


//-----------------------------------------------------------------------------
// There is sequence which will result in defeating switch events 
// notification. THIS IS NOT DESIRABLE, but if it happens, you can easily 
// recover from it.
//
// If you attempt to resume the device before resuming the parent, the 
// following scenario will happen. The USB-core will drive the osrfx2_resume
// function and this function will attempt to restart the interrupt-in urb.
// This restart will fail with an EHOSTUNREACH error, indicating the parent 
// hub/port is still suspended. If the parent hub/port is later resumed, the
// osrfx2_resume function is not driven as USB-core knows osrfx2 has already 
// been resumed.  In other words, a USB device can be resumed without being 
// reachable vis-a-vis its parent.
//
// Any requests for switch notification will wait but never receive any 
// notifications as the interrupt-in urb is not started.
//
// To recover, just go through the suspend and resume sequence above in the
// prescribed order and it should restart the interrupt-in usb and allow
// switch events to be delivered.
//-----------------------------------------------------------------------------
