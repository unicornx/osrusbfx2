/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    private.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#pragma warning(disable:4200)  // nameless struct/union
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int
#include <initguid.h>
#include <ntddk.h>
#include "usbdi.h"
#include "usbdlib.h"
#include "public.h"
#include "driverspecs.h"

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)

#include <wdf.h>
#include <wdfusb.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "trace.h"

#ifndef _PRIVATE_H
#define _PRIVATE_H

#define POOL_TAG (ULONG) 'FRSO'
#define _DRIVER_NAME_ "OSRUSBFX2"

#define TEST_BOARD_TRANSFER_BUFFER_SIZE (64*1024)
#define DEVICE_DESC_LENGTH 256

extern const __declspec(selectany) LONGLONG DEFAULT_CONTROL_TRANSFER_TIMEOUT = 5 * -1 * WDF_TIMEOUT_TO_SEC;

//
// Define the vendor commands supported by our device
//
#define USBFX2LK_READ_7SEGMENT_DISPLAY      0xD4
#define USBFX2LK_READ_SWITCHES              0xD6
#define USBFX2LK_READ_BARGRAPH_DISPLAY      0xD7
#define USBFX2LK_SET_BARGRAPH_DISPLAY       0xD8
#define USBFX2LK_IS_HIGH_SPEED              0xD9
#define USBFX2LK_REENUMERATE                0xDA
#define USBFX2LK_SET_7SEGMENT_DISPLAY       0xDB

//
// Define the features that we can clear
//  and set on our device
//
#define USBFX2LK_FEATURE_EPSTALL            0x00
#define USBFX2LK_FEATURE_WAKE               0x01

//
// Order of endpoints in the interface descriptor
//
#define INTERRUPT_IN_ENDPOINT_INDEX    0
#define BULK_OUT_ENDPOINT_INDEX        1
#define BULK_IN_ENDPOINT_INDEX         2

//
// A structure representing the instance information associated with
// this particular device.
//

typedef struct _DEVICE_CONTEXT {

    WDFUSBDEVICE                    UsbDevice;

    WDFUSBINTERFACE                 UsbInterface;

    WDFUSBPIPE                      BulkReadPipe;

    WDFUSBPIPE                      BulkWritePipe;

    WDFUSBPIPE                      InterruptPipe;

    UCHAR                           CurrentSwitchState;

    WDFQUEUE                        InterruptMsgQueue;

    ULONG                           UsbDeviceTraits;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

extern ULONG DebugLevel;


DRIVER_INITIALIZE DriverEntry;

EVT_WDF_OBJECT_CONTEXT_CLEANUP OsrFxEvtDriverContextCleanup;

EVT_WDF_DRIVER_DEVICE_ADD OsrFxEvtDeviceAdd;

EVT_WDF_DEVICE_PREPARE_HARDWARE OsrFxEvtDevicePrepareHardware;

EVT_WDF_IO_QUEUE_IO_READ OsrFxEvtIoRead;

EVT_WDF_IO_QUEUE_IO_WRITE OsrFxEvtIoWrite;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL OsrFxEvtIoDeviceControl;

EVT_WDF_REQUEST_COMPLETION_ROUTINE EvtRequestReadCompletionRoutine;

EVT_WDF_REQUEST_COMPLETION_ROUTINE EvtRequestWriteCompletionRoutine;

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
ResetPipe(
    __in WDFUSBPIPE             Pipe
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
ResetDevice(
    __in WDFDEVICE Device
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
SelectInterfaces(
    __in WDFDEVICE Device
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
AbortPipes(
    __in WDFDEVICE Device
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
ReenumerateDevice(
    __in PDEVICE_CONTEXT DevContext
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
GetBarGraphState(
    __in PDEVICE_CONTEXT DevContext,
    __out PBAR_GRAPH_STATE BarGraphState
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
SetBarGraphState(
    __in PDEVICE_CONTEXT DevContext,
    __in PBAR_GRAPH_STATE BarGraphState
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
GetSevenSegmentState(
    __in PDEVICE_CONTEXT DevContext,
    __out PUCHAR SevenSegment
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
SetSevenSegmentState(
    __in PDEVICE_CONTEXT DevContext,
    __in PUCHAR SevenSegment
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
GetSwitchState(
    __in PDEVICE_CONTEXT DevContext,
    __in PSWITCH_STATE SwitchState
    );

__drv_requiresIRQL(DISPATCH_LEVEL)
VOID
OsrUsbIoctlGetInterruptMessage(
    __in WDFDEVICE Device
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
OsrFxSetPowerPolicy(
        __in WDFDEVICE Device
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
OsrFxConfigContReaderForInterruptEndPoint(
    __in PDEVICE_CONTEXT DeviceContext
    );

EVT_WDF_USB_READER_COMPLETION_ROUTINE OsrFxEvtUsbInterruptPipeReadComplete;

EVT_WDF_USB_READERS_FAILED OsrFxEvtUsbInterruptReadersFailed;

EVT_WDF_IO_QUEUE_IO_STOP OsrFxEvtIoStop;

EVT_WDF_DEVICE_D0_ENTRY OsrFxEvtDeviceD0Entry;

EVT_WDF_DEVICE_D0_EXIT OsrFxEvtDeviceD0Exit;

__drv_requiresIRQL(PASSIVE_LEVEL)
BOOLEAN
OsrFxReadFdoRegistryKeyValue(
    __in PWDFDEVICE_INIT  DeviceInit,
    __in PWCHAR           Name,
    __out PULONG          Value
    );

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
OsrFxEnumerateChildren(
    __in WDFDEVICE Device
    );

__drv_requiresIRQL(PASSIVE_LEVEL)
PCHAR
DbgDevicePowerString(
    __in WDF_POWER_DEVICE_STATE Type
    );

#endif


