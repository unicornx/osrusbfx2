/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    public.h

Abstract:

Environment:

    User & Kernel mode

Revision History:
Author         Date       Tracking Number  Description of Change
--------------+----------+---------------+--------------------------------------
unicornx       June/2012  N/A             modified to work with CY001 dev board
unicornx       July/2012  N/A             added filter drv to get dev info
unicornx       Mar/2013   N/A             redefined bar graph structure
--*/

#ifndef _PUBLIC_H
#define _PUBLIC_H

#include <initguid.h>

// {78A1C341-4539-11d3-B88D-00C04FAD5171}
DEFINE_GUID(GUID_CLASS_OSRUSBFX2, 
0x78A1C341, 0x4539, 0x11d3, 0xB8, 0x8D, 0x00, 0xC0, 0x4F, 0xAD, 0x51, 0x71);

// {573E8C73-0CB4-4471-A1BF-FAB26C31D384}
DEFINE_GUID(GUID_DEVINTERFACE_OSRUSBFX2, 
            0x573e8c73, 0xcb4, 0x4471, 0xa1, 0xbf, 0xfa, 0xb2, 0x6c, 0x31, 0xd3, 0x84);

// {44E0F073-F359-4b33-908B-362B6CDCB166}
DEFINE_GUID(GUID_DEVINTERFACE_OSRUSBFX2FILTER, 
0x44e0f073, 0xf359, 0x4b33, 0x90, 0x8b, 0x36, 0x2b, 0x6c, 0xdc, 0xb1, 0x66);

//
// Define the structures that will be used by the IOCTL 
//  interface to the driver
//

//
// BAR_GRAPH_STATE
//
// BAR_GRAPH_STATE is a bit field structure with each
//  bit corresponding to one of the bar graph on the 
//  OSRFX2 Development Board
//
// Modified this structure to adpator to CY001
// Refer to icd.h of osrfx2fw "LED Bar Graph" section
// to get more details
//
#include <pshpack1.h>

#define BARGRAPH_ON  (UCHAR)0x80
#define BARGRAPH_OFF (UCHAR)0x00

#define BARGRAPH_MAXBAR (UCHAR)4 // CY001 only have 4 LED bars availabe

typedef struct _BAR_GRAPH_STATE {

    union {
 
        struct {
            //
            // Individual bars starting from the 
            //  top of the stack of bars 
            //
            // NOTE: There are actually 10 bars, 
            //  but the very top two do not light
            //  and are not counted here
            //
            UCHAR Bar1 : 1;
            UCHAR Bar2 : 1;
            UCHAR Bar3 : 1;
            UCHAR Bar4 : 1;
            UCHAR Bar5 : 1; // not used for CY001
            UCHAR Bar6 : 1; // not used for CY001
            UCHAR Bar7 : 1; // not used for CY001
            UCHAR Bar8 : 1; // used by CY001 as flag for light(1)/clear(0)
        };

        //
        // The state of all the bar graph as a single
        // UCHAR
        //
        UCHAR BarsAsUChar;

    };

}BAR_GRAPH_STATE, *PBAR_GRAPH_STATE;

//
// SWITCH_STATE
//
// SWITCH_STATE is a bit field structure with each
//  bit corresponding to one of the switches on the 
//  OSRFX2 Development Board
//
typedef struct _SWITCH_STATE {

    union {
        struct {
            //
            // Individual switches starting from the 
            //  left of the set of switches
            //
            UCHAR Switch1 : 1;
            UCHAR Switch2 : 1;
            UCHAR Switch3 : 1;
            UCHAR Switch4 : 1;
            UCHAR Switch5 : 1;
            UCHAR Switch6 : 1;
            UCHAR Switch7 : 1;
            UCHAR Switch8 : 1;
        };

        //
        // The state of all the switches as a single
        // UCHAR
        //
        UCHAR SwitchesAsUChar;

    };


}SWITCH_STATE, *PSWITCH_STATE;

//
// MOUSE_STATE
//
// Refer to icd.h of osrfx2fw
// "Mouse Position tracking simulation" section
// to get more details
//
typedef UCHAR MOUSE_STATE, *PMOUSE_STATE;

#include <poppack.h>

#define IOCTL_INDEX             0x800
#define FILE_DEVICE_OSRUSBFX2   0x65500

#define IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR CTL_CODE(FILE_DEVICE_OSRUSBFX2,     \
                                                     IOCTL_INDEX,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_READ_ACCESS)
                                                   
#define IOCTL_OSRUSBFX2_RESET_DEVICE  CTL_CODE(FILE_DEVICE_OSRUSBFX2,     \
                                                     IOCTL_INDEX + 1, \
                                                     METHOD_BUFFERED,         \
                                                     FILE_WRITE_ACCESS)

#define IOCTL_OSRUSBFX2_REENUMERATE_DEVICE  CTL_CODE(FILE_DEVICE_OSRUSBFX2, \
                                                    IOCTL_INDEX  + 3,  \
                                                    METHOD_BUFFERED, \
                                                    FILE_WRITE_ACCESS)

#define IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY CTL_CODE(FILE_DEVICE_OSRUSBFX2,\
                                                    IOCTL_INDEX  + 4, \
                                                    METHOD_BUFFERED, \
                                                    FILE_READ_ACCESS)


#define IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY CTL_CODE(FILE_DEVICE_OSRUSBFX2,\
                                                    IOCTL_INDEX + 5, \
                                                    METHOD_BUFFERED, \
                                                    FILE_WRITE_ACCESS)


#define IOCTL_OSRUSBFX2_READ_SWITCHES   CTL_CODE(FILE_DEVICE_OSRUSBFX2, \
                                                    IOCTL_INDEX + 6, \
                                                    METHOD_BUFFERED, \
                                                    FILE_READ_ACCESS)


#define IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY CTL_CODE(FILE_DEVICE_OSRUSBFX2, \
                                                    IOCTL_INDEX + 7, \
                                                    METHOD_BUFFERED, \
                                                    FILE_READ_ACCESS)


#define IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY CTL_CODE(FILE_DEVICE_OSRUSBFX2, \
                                                    IOCTL_INDEX + 8, \
                                                    METHOD_BUFFERED, \
                                                    FILE_WRITE_ACCESS)

#define IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE CTL_CODE(FILE_DEVICE_OSRUSBFX2,\
                                                    IOCTL_INDEX + 9, \
                                                    METHOD_OUT_DIRECT, \
                                                    FILE_READ_ACCESS)

#define IOCTL_OSRUSBFX2_FILTER_GETDEVINFOLEN CTL_CODE(FILE_DEVICE_OSRUSBFX2,\
                                                   IOCTL_INDEX + 10, \
                                                   METHOD_BUFFERED, \
                                                   FILE_READ_ACCESS)
                                                   
#define IOCTL_OSRUSBFX2_FILTER_GETDEVINFODATA CTL_CODE(FILE_DEVICE_OSRUSBFX2,\
                                                   IOCTL_INDEX + 11, \
                                                   METHOD_BUFFERED, \
                                                   FILE_READ_ACCESS)

#endif

